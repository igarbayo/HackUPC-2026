#include "Robot.h"
#include <algorithm>
#include <stdexcept>
#include <climits>

// ── decide ────────────────────────────────────────────────────────────────────

Robot::Request Robot::decide(const Aisle::Metadata& meta, const RobotContext& ctx) {
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

        candidates.push_back({family, heuristic_->score(family, meta, ctx), available});
    }

    // Fallback: if no family passed the threshold but boxes exist, bypass it.
    // This handles end-of-batch / aisle-draining scenarios.
    if (candidates.empty()) {
        for (const auto& [family, count] : meta.countByFamily) {
            if (findPalletSlot(family) != -1) continue;
            if (count == 0) continue;
            int available = count - getInFlight(meta, family);
            if (available <= 0) continue;
            candidates.push_back({family, heuristic_->score(family, meta, ctx), available});
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

void Robot::tick(AisleContainer& container, const std::unordered_set<Family>& otherClaims) {
    // Snapshot metadata before collecting so onBoxDelivered has it for eviction
    lastMeta_ = container.metadata();

    // 1. Collect ready outputs
    for (auto& palletOpt : pallets_) {
        if (!palletOpt) continue;
        Family fam = palletOpt->family();
        while (auto boxOpt = container.collectReadyOutput(fam)) {
            onBoxDelivered(*boxOpt);
        }
    }

    // 1b. Dispatch pallets whose family is exhausted (nothing left in aisle or in-flight).
    //     Without this, a pallet can block a slot forever if the aisle filled up with
    //     other families and all boxes of this family were already collected.
    {
        const auto metaPost = container.metadata();
        for (int i = 0; i < MAX_ACTIVE_PALLETS; ++i) {
            if (!pallets_[i]) continue;
            Family f = pallets_[i]->family();
            auto cit = metaPost.countByFamily.find(f);
            int inAisle   = (cit != metaPost.countByFamily.end()) ? cit->second : 0;
            int inFlight  = getInFlight(metaPost, f);
            if (inAisle == 0 && inFlight == 0) {
                if (pallets_[i]->placedCount() > 0)
                    dispatchPallet(i);
                else
                    pallets_[i].reset();
            }
        }
    }

    // 2. Drain check: if aisle is truly empty, ship everything and stop
    const auto meta = container.metadata();
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
    RobotContext ctx;
    ctx.otherClaims = otherClaims;
    for (const auto& p : pallets_)
        if (p) ctx.ownClaims.insert(p->family());
    Request req = decide(meta, ctx);

    for (const auto& [family, count] : req) {
        for (int i = 0; i < count; ++i) {
            int slot = findPalletSlot(family);
            if (slot == -1) {
                slot = findEmptyPalletSlot();
                if (slot != -1) pallets_[slot] = Pallet(family);
            }
            if (slot != -1 && pallets_[slot]->freeSlots() > 0) {
                pallets_[slot]->reserve();
                container.requestOutput(family);
            }
        }
    }
}

// ── accessors ─────────────────────────────────────────────────────────────────

const std::array<std::optional<Pallet>, Robot::MAX_ACTIVE_PALLETS>& Robot::pallets() const {
    return pallets_;
}

void Robot::setEventLog(std::vector<Event>* log)            { eventLog_ = log; }
void Robot::setCurrentTick(Tick t)                          { currentTick_ = t; }
void Robot::setRobotId(int id)                              { robotId_ = id; }
void Robot::setHeuristic(std::shared_ptr<RobotHeuristic> h) { heuristic_ = std::move(h); }

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
            eventLog_->push_back({"box_on_pallet", currentTick_, b.id(), slot, b.family(), robotId_});
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
    int    boxes = pallets_[slotIndex]->placedCount();
    if (eventLog_)
        eventLog_->push_back({"pallet_dispatched", currentTick_, {}, slotIndex, pallets_[slotIndex]->family(), robotId_, boxes});

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
        Family f    = pallets_[i]->family();
        int    placed  = pallets_[i]->placedCount();
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

