#include "Robot.h"
#include <algorithm>
#include <stdexcept>
#include <climits>

// ── decide ────────────────────────────────────────────────────────────────────

Robot::Request Robot::decide(const Aisle::Metadata& meta) {
    Request req;

    // ── Part 1: existing pallets — bulk-request up to freeSlots ──────────────
    for (const auto& [family, count] : meta.countByFamily) {
        int slot = findPalletSlot(family);
        if (slot == -1) continue;
        if (count == 0) continue;

        int available = count - getInFlight(meta, family);
        int room      = pallets_[slot]->freeSlots();
        if (available > 0 && room > 0)
            req[family] = std::min(available, room);
    }

    // ── Part 2: new pallets — threshold filter + score ranking ───────────────
    int emptySlots = countEmptyPalletSlots();
    if (emptySlots == 0) return req;

    struct Candidate { Family family; float score; int available; };
    std::vector<Candidate> candidates;

    for (const auto& [family, count] : meta.countByFamily) {
        if (findPalletSlot(family) != -1) continue;  // already has a pallet
        if (count == 0) continue;

        int available = count - getInFlight(meta, family);
        if (available <= 0) continue;

        if (available < OPEN_THRESHOLD) continue;

        candidates.push_back({family, scoreFamilyForNewPallet(family, meta), available});
    }

    // Fallback: if no family passed the threshold but boxes exist, bypass it.
    // This handles end-of-batch / aisle-draining scenarios.
    if (candidates.empty()) {
        for (const auto& [family, count] : meta.countByFamily) {
            if (findPalletSlot(family) != -1) continue;
            if (count == 0) continue;
            int available = count - getInFlight(meta, family);
            if (available <= 0) continue;
            candidates.push_back({family, scoreFamilyForNewPallet(family, meta), available});
        }
    }

    // Best-score candidates get pallet slots first
    std::sort(candidates.begin(), candidates.end(),
              [](const Candidate& a, const Candidate& b) { return a.score > b.score; });

    int toOpen = std::min(emptySlots, static_cast<int>(candidates.size()));
    for (int i = 0; i < toOpen; ++i) {
        const auto& c = candidates[i];
        req[c.family] = std::min(c.available, Pallet::CAPACITY);
    }

    return req;
}

// ── tick ──────────────────────────────────────────────────────────────────────

void Robot::tick(Aisle& aisle) {
    // Snapshot metadata before collecting so onBoxDelivered has it for eviction
    lastMeta_ = aisle.metadata();

    // 1. Collect ready outputs
    for (auto& palletOpt : pallets_) {
        if (!palletOpt) continue;
        while (auto boxOpt = aisle.collectReadyOutput(palletOpt->family())) {
            onBoxDelivered(*boxOpt);
        }
    }

    // 2. Drain check: if aisle is truly empty, ship everything and stop
    const auto meta = aisle.metadata();
    bool aisleEmpty = meta.countByFamily.empty()
                   && meta.pendingInputs  == 0
                   && meta.activeShuttles == 0
                   && meta.readyOutputCount == 0;
    if (aisleEmpty) {
        for (int i = 0; i < MAX_ACTIVE_PALLETS; ++i)
            if (pallets_[i]) dispatchPallet(i);
        return;
    }

    // 3. Decide and request (fresh metadata post-collect)
    Request req = decide(meta);

    for (const auto& [family, count] : req) {
        for (int i = 0; i < count; ++i) {
            int slot = findPalletSlot(family);
            if (slot == -1) {
                slot = findEmptyPalletSlot();
                if (slot != -1) pallets_[slot] = Pallet(family);
            }
            if (slot != -1 && pallets_[slot]->freeSlots() > 0) {
                pallets_[slot]->reserve();
                aisle.requestOutput(family);
            }
        }
    }
}

// ── accessors ─────────────────────────────────────────────────────────────────

const std::array<std::optional<Pallet>, Robot::MAX_ACTIVE_PALLETS>& Robot::pallets() const {
    return pallets_;
}

int   Robot::totalPalletsSent()  const { return totalPallets_; }
int   Robot::filledPalletsSent() const { return filledPallets_; }
int   Robot::totalBoxesSent()    const { return totalBoxes_; }
int   Robot::boxesSentForFamily(const Family& f) const {
    auto it = boxesByFamily_.find(f);
    return it != boxesByFamily_.end() ? it->second : 0;
}
const std::unordered_map<Family, int>& Robot::boxesSentByFamily() const { return boxesByFamily_; }
float Robot::avgFillRate() const {
    if (totalPallets_ == 0) return 0.0f;
    return static_cast<float>(totalBoxes_) /
           (static_cast<float>(totalPallets_) * Pallet::CAPACITY);
}
void Robot::setEventLog(std::vector<Event>* log) { eventLog_ = log; }
void Robot::setCurrentTick(Tick t)               { currentTick_ = t; }

// ── onBoxDelivered ────────────────────────────────────────────────────────────

void Robot::onBoxDelivered(Box b) {
    int slot = findPalletSlot(b.family());

    if (slot == -1) {
        int emptySlot = findEmptyPalletSlot();
        if (emptySlot != -1) {
            pallets_[emptySlot] = Pallet(b.family());
            slot = emptySlot;
        } else {
            // Force-evict the pallet least likely to complete: lowest (placed + available_in_aisle)
            int forceSlot = findLeastCompletablePalletSlot();
            if (forceSlot != -1) {
                dispatchPallet(forceSlot);
                pallets_[forceSlot] = Pallet(b.family());
                slot = forceSlot;
            }
        }
    }

    if (slot != -1) {
        pallets_[slot]->placeBox(b);
        if (eventLog_)
            eventLog_->push_back({"box_on_pallet", currentTick_, b.id(), slot, b.family()});
        if (pallets_[slot]->isFull())
            dispatchPallet(slot);
    }
}

// ── dispatchPallet ────────────────────────────────────────────────────────────

Pallet Robot::dispatchPallet(int slotIndex) {
    if (slotIndex < 0 || slotIndex >= MAX_ACTIVE_PALLETS)
        throw std::out_of_range("Robot::dispatchPallet: invalid slot index");
    if (!pallets_[slotIndex])
        throw std::runtime_error("Robot::dispatchPallet: slot is empty");
    if (eventLog_)
        eventLog_->push_back({"pallet_dispatched", currentTick_, {}, slotIndex, pallets_[slotIndex]->family()});

    int            boxes = pallets_[slotIndex]->placedCount();
    const Family&  fam   = pallets_[slotIndex]->family();
    ++totalPallets_;
    totalBoxes_          += boxes;
    boxesByFamily_[fam]  += boxes;
    if (boxes == Pallet::CAPACITY) ++filledPallets_;

    Pallet p = std::move(*pallets_[slotIndex]);
    pallets_[slotIndex].reset();
    return p;
}

// ── private helpers ───────────────────────────────────────────────────────────

int Robot::findPalletSlot(const Family& f) const {
    for (int i = 0; i < MAX_ACTIVE_PALLETS; ++i)
        if (pallets_[i] && pallets_[i]->family() == f) return i;
    return -1;
}

int Robot::findEmptyPalletSlot() const {
    for (int i = 0; i < MAX_ACTIVE_PALLETS; ++i)
        if (!pallets_[i]) return i;
    return -1;
}

int Robot::countEmptyPalletSlots() const {
    int n = 0;
    for (const auto& p : pallets_) if (!p) ++n;
    return n;
}

// Evict the pallet with lowest (placed + available_in_aisle): least likely to fill.
int Robot::findLeastCompletablePalletSlot() const {
    int best = -1, bestScore = INT_MAX;
    for (int i = 0; i < MAX_ACTIVE_PALLETS; ++i) {
        if (!pallets_[i]) continue;
        const Family& f = pallets_[i]->family();
        int placed  = pallets_[i]->placedCount();
        int inAisle = 0;
        auto it = lastMeta_.countByFamily.find(f);
        if (it != lastMeta_.countByFamily.end()) inAisle = it->second;
        int score = placed + inAisle;
        if (score < bestScore) { bestScore = score; best = i; }
    }
    return best;
}

int Robot::getInFlight(const Aisle::Metadata& meta, const Family& f) const {
    auto it = meta.reservedByFamily.find(f);
    return it != meta.reservedByFamily.end() ? it->second : 0;
}

// score = available / (avgDist + 1): prefer families with many nearby boxes.
float Robot::scoreFamilyForNewPallet(const Family& f, const Aisle::Metadata& meta) const {
    auto cit = meta.countByFamily.find(f);
    int available = (cit != meta.countByFamily.end())
                  ? cit->second - getInFlight(meta, f)
                  : 0;

    float avgDist = 1.0f;
    auto dit = meta.avgDistanceByFamily.find(f);
    if (dit != meta.avgDistanceByFamily.end() && dit->second > 0.0f)
        avgDist = dit->second;

    return static_cast<float>(available) / (avgDist + 1.0f);
}
