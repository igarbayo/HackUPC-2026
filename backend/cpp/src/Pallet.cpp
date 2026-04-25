#include "Pallet.h"

Pallet::Pallet(Family family) : family_(std::move(family)) {}

Family Pallet::family()        const { return family_; }
int    Pallet::placedCount()   const { return static_cast<int>(boxes_.size()); }
int    Pallet::reservedCount() const { return reserved_; }
int    Pallet::freeSlots()     const { return CAPACITY - placedCount() - reserved_; }
bool   Pallet::isFull()        const { return placedCount() >= CAPACITY; }

void Pallet::reserve() {
    if (freeSlots() <= 0) throw std::runtime_error("Pallet::reserve() no free slots");
    ++reserved_;
}

void Pallet::releaseReservation() {
    if (reserved_ <= 0) throw std::runtime_error("Pallet::releaseReservation() no reservations");
    --reserved_;
}

void Pallet::placeBox(Box b) {
    if (reserved_ > 0) --reserved_;  // consume reservation if any
    boxes_.push_back(std::move(b));
}

const std::vector<Box>& Pallet::boxes() const { return boxes_; }
