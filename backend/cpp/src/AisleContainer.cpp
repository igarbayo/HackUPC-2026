#include "AisleContainer.h"
#include <climits>

AisleContainer::AisleContainer(int numAisles, int length, int numY, int numSides, Position port) {
    aisles_.reserve(numAisles);
    for (int i = 0; i < numAisles; ++i)
        aisles_.emplace_back(length, numY, numSides, port);
}

// ── routing ───────────────────────────────────────────────────────────────────

int AisleContainer::selectAisleForInput(const Box& box) const {
    int bestIdx  = -1;
    int bestCost = INT_MAX;

    for (int i = 0; i < (int)aisles_.size(); ++i) {
        int numY = static_cast<int>(aisles_[i].shuttles().size());
        for (int y = 1; y <= numY; ++y) {
            auto [cost, pos] = aisles_[i].findBestInputSlot(box, y);
            if (!pos) continue;

            // -- NOT USED FOR NOW -- Locality bonus: lower effective cost when the family is already in this aisle
            int effectiveCost = cost;
            /*auto it = aisles_[i].metadata().countByFamily.find(box.family());
            if (it != aisles_[i].metadata().countByFamily.end() && it->second > 0)
                effectiveCost -= 1000;*/

            if (effectiveCost < bestCost) {
                bestCost = effectiveCost;
                bestIdx  = i;
            }
        }
    }

    // Fallback: all slots are claimed — pick the aisle with most physical free slots
    if (bestIdx == -1) {
        int mostFree = -1;
        for (int i = 0; i < (int)aisles_.size(); ++i) {
            int fs = aisles_[i].metadata().freeSlots;
            if (fs > mostFree) { mostFree = fs; bestIdx = i; }
        }
    }
    if (bestIdx == -1) bestIdx = 0;
    return bestIdx;
}

int AisleContainer::selectAisleForOutput(const Family& f) const {
    int bestIdx  = -1;
    int bestCost = INT_MAX;

    for (int i = 0; i < (int)aisles_.size(); ++i) {
        const Metadata& m = aisles_[i].metadata();

        // Skip aisles with no unreserved boxes of this family
        int count = 0;
        auto cit = m.countByFamily.find(f);
        if (cit != m.countByFamily.end()) count = cit->second;
        int reserved = 0;
        auto rit = m.reservedByFamily.find(f);
        if (rit != m.reservedByFamily.end()) reserved = rit->second;
        if (count - reserved <= 0) continue;

        // Use findBestBoxForShuttle for each shuttle to find the true retrieval cost
        for (const auto& shuttle : aisles_[i].shuttles()) {
            auto boxPos = aisles_[i].findBestBoxForShuttle(f, shuttle.position());
            if (!boxPos) continue;
            int cost = std::abs(shuttle.position().x - boxPos->x)
                     + std::abs(boxPos->x - aisles_[i].port().x);
            if (cost < bestCost) {
                bestCost = cost;
                bestIdx  = i;
            }
        }
    }

    // Fallback: all boxes of this family are already reserved — pick by raw count
    if (bestIdx == -1) {
        int maxCount = -1;
        for (int i = 0; i < (int)aisles_.size(); ++i) {
            int cnt = 0;
            auto it = aisles_[i].metadata().countByFamily.find(f);
            if (it != aisles_[i].metadata().countByFamily.end()) cnt = it->second;
            if (cnt > maxCount) { maxCount = cnt; bestIdx = i; }
        }
    }

    if (bestIdx == -1) bestIdx = 0;
    return bestIdx;
}

// ── public interface ──────────────────────────────────────────────────────────

void AisleContainer::input(Box box) {
    aisles_[selectAisleForInput(box)].input(std::move(box));
}

void AisleContainer::requestOutput(const Family& f) {
    aisles_[selectAisleForOutput(f)].requestOutput(f);
}

std::optional<Box> AisleContainer::collectReadyOutput(const Family& f) {
    for (auto& aisle : aisles_) {
        auto box = aisle.collectReadyOutput(f);
        if (box) return box;
    }
    return std::nullopt;
}

void AisleContainer::tick() {
    for (auto& aisle : aisles_) aisle.tick();
}

void AisleContainer::setEventLog(std::vector<Event>* log) {
    for (auto& aisle : aisles_) aisle.setEventLog(log);
}

std::vector<AisleSnap> AisleContainer::snapshot() const {
    std::vector<AisleSnap> snaps;
    snaps.reserve(aisles_.size());
    for (int i = 0; i < (int)aisles_.size(); ++i) {
        AisleSnap s = aisles_[i].snapshot();
        s.aisle_id  = i;
        snaps.push_back(std::move(s));
    }
    return snaps;
}

// ── metadata aggregation ──────────────────────────────────────────────────────

AisleContainer::Metadata AisleContainer::aggregateMeta() const {
    Metadata agg;

    for (const auto& aisle : aisles_) {
        const Metadata& m = aisle.metadata();

        for (const auto& [fam, cnt] : m.countByFamily)
            agg.countByFamily[fam] += cnt;

        for (const auto& [fam, cnt] : m.reservedByFamily)
            agg.reservedByFamily[fam] += cnt;

        for (const auto& [fam, t] : m.oldestArrivalByFamily) {
            auto it = agg.oldestArrivalByFamily.find(fam);
            if (it == agg.oldestArrivalByFamily.end() || t < it->second)
                agg.oldestArrivalByFamily[fam] = t;
        }

        // Accumulate weighted sum; resolved after the loop
        for (const auto& [fam, avg] : m.avgDistanceByFamily) {
            int cnt = 0;
            auto cit = m.countByFamily.find(fam);
            if (cit != m.countByFamily.end()) cnt = cit->second;
            agg.avgDistanceByFamily[fam] += avg * static_cast<float>(cnt);
        }

        agg.freeSlots        += m.freeSlots;
        agg.pendingInputs    += m.pendingInputs;
        agg.pendingOutputs   += m.pendingOutputs;
        agg.readyOutputCount += m.readyOutputCount;
        agg.activeShuttles   += m.activeShuttles;
    }

    // Finalise avgDistance: divide weighted sum by total count
    for (auto& [fam, wsum] : agg.avgDistanceByFamily) {
        int total = 0;
        auto it = agg.countByFamily.find(fam);
        if (it != agg.countByFamily.end()) total = it->second;
        wsum = (total > 0) ? wsum / static_cast<float>(total) : 0.0f;
    }

    return agg;
}

AisleContainer::Metadata AisleContainer::metadata() const {
    return aggregateMeta();
}
