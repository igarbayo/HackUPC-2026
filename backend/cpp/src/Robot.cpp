#include "Robot.h"
#include <algorithm>
#include <stdexcept>
#include <climits>

Robot::Request Robot::decide(const Aisle::Metadata& meta) {
    Request req;
    for (const auto& [family, count] : meta.countByFamily) {
        if (count == 0) continue;

        int palletSlot = findPalletSlot(family);
        if (palletSlot != -1) {
            // We have an open pallet for this family - check capacity
            if (pallets_[palletSlot]->freeSlots() <= 0) continue;
        } else {
            // Need to open a new pallet - check if we have room
            if (findEmptyPalletSlot() == -1) continue;
        }

        // How many are already reserved/in-flight?
        int inFlight = 0;
        auto rit = meta.reservedByFamily.find(family);
        if (rit != meta.reservedByFamily.end()) inFlight = rit->second;

        if (count - inFlight > 0) {
            req[family] = 1;
        }
    }
    return req;
}

void Robot::tick(Aisle& aisle) {
    // 1. Collect ready outputs
    // Collect for all open pallets
    for (auto& palletOpt : pallets_) {
        if (!palletOpt) continue;
        while (auto boxOpt = aisle.collectReadyOutput(palletOpt->family())) {
            onBoxDelivered(*boxOpt);
        }
    }

    // 2. Decide and request
    const auto meta = aisle.metadata();
    Request req = decide(meta);

    for (const auto& [family, count] : req) {
        for (int i = 0; i < count; ++i) {
            // Ensure pallet is open (or open one)
            int slot = findPalletSlot(family);
            if (slot == -1) {
                slot = findEmptyPalletSlot();
                if (slot != -1) {
                    pallets_[slot] = Pallet(family);
                }
            }
            if (slot != -1 && pallets_[slot]->freeSlots() > 0) {
                pallets_[slot]->reserve();
                aisle.requestOutput(family);
            }
        }
    }
}

const std::array<std::optional<Pallet>, Robot::MAX_ACTIVE_PALLETS>& Robot::pallets() const {
    return pallets_;
}

int  Robot::dispatchedPallets()                 const { return dispatchedFullPallets_; }
int  Robot::dispatchedBoxes()                   const { return dispatchedBoxes_; }
void Robot::setEventLog(std::vector<Event>* log) { eventLog_ = log; }
void Robot::setCurrentTick(Tick t)              { currentTick_ = t; }

void Robot::onBoxDelivered(Box b) {
    int slot = findPalletSlot(b.family());

    if (slot == -1) {
        int emptySlot = findEmptyPalletSlot();
        if (emptySlot != -1) {
            pallets_[emptySlot] = Pallet(b.family());
            slot = emptySlot;
        } else {
            int forceSlot = findLeastFullPalletSlot();
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

Pallet Robot::dispatchPallet(int slotIndex) {
    if (slotIndex < 0 || slotIndex >= MAX_ACTIVE_PALLETS)
        throw std::out_of_range("Robot::dispatchPallet: invalid slot index");
    if (!pallets_[slotIndex])
        throw std::runtime_error("Robot::dispatchPallet: slot is empty");
    if (eventLog_)
        eventLog_->push_back({"pallet_dispatched", currentTick_, 0, slotIndex, pallets_[slotIndex]->family()});

    int dispatchedBoxes = pallets_[slotIndex]->placedCount();
    dispatchedBoxes_ += dispatchedBoxes;
    if (dispatchedBoxes == Pallet::CAPACITY)
        ++dispatchedFullPallets_;


    Pallet p = std::move(*pallets_[slotIndex]);
    pallets_[slotIndex].reset();
    return p;
}

int Robot::findPalletSlot(const Family& f) const {
    for (int i = 0; i < MAX_ACTIVE_PALLETS; ++i) {
        if (pallets_[i] && pallets_[i]->family() == f) return i;
    }
    return -1;
}

int Robot::findEmptyPalletSlot() const {
    for (int i = 0; i < MAX_ACTIVE_PALLETS; ++i) {
        if (!pallets_[i]) return i;
    }
    return -1;
}

int Robot::findLeastFullPalletSlot() const {
    int best = -1, bestCount = INT_MAX;
    for (int i = 0; i < MAX_ACTIVE_PALLETS; ++i) {
        if (pallets_[i] && pallets_[i]->placedCount() < bestCount) {
            bestCount = pallets_[i]->placedCount();
            best = i;
        }
    }
    return best;
}
