#include "Shuttle.h"
#include "Aisle.h"

Shuttle::Shuttle(Position startPos) : pos_(startPos) {}

Position    Shuttle::position()    const { return pos_; }
bool        Shuttle::isFree()      const { return phase_ == Phase::Idle; }
bool        Shuttle::isOnMission() const { return phase_ != Phase::Idle; }
Shuttle::Phase Shuttle::phase()    const { return phase_; }
const Box*  Shuttle::carriedBox()  const { return carried_ ? &*carried_ : nullptr; }

void Shuttle::assignOutputMission(Position pickupFrom, Position dropAt) {
    pickup_ = pickupFrom;
    drop_   = dropAt;
    isInputMission_ = false;
    phase_ = Phase::MovingToPickup;
}

void Shuttle::assignInputMission(Position pickupFrom, Position dropAt) {
    pickup_ = pickupFrom;
    drop_   = dropAt;
    isInputMission_ = true;
    phase_ = Phase::MovingToPickup;
}

void Shuttle::moveToward(Position target) {
    if (pos_.x < target.x)      ++pos_.x;
    else if (pos_.x > target.x) --pos_.x;
    // y movement reserved for future 2D extension
}

void Shuttle::tick(Aisle& aisle) {
    switch (phase_) {
    case Phase::Idle:
        break;

    case Phase::MovingToPickup:
        if (pos_ == pickup_) {
            phase_ = Phase::Loading;
            // Fall through: process Loading in same tick
        } else {
            moveToward(pickup_);
            break;
        }
        [[fallthrough]];

    case Phase::Loading:
        if (isInputMission_) {
            auto boxOpt = aisle.takeFromInputBuffer();
            if (boxOpt) {
                carried_ = std::move(*boxOpt);
                phase_ = Phase::MovingToDrop;
            }
            // If no box yet (shouldn't happen in normal flow), stay in Loading
        } else {
            // Output mission: take box from storage slot
            Slot& s = aisle.slotAt(pickup_);
            if (!s.isEmpty()) {
                Box b = s.take();
                aisle.notifyBoxTaken(b, pickup_);
                carried_ = std::move(b);
                phase_ = Phase::MovingToDrop;
            }
            // If slot empty (race condition), stay in Loading and retry next tick
        }
        break;

    case Phase::MovingToDrop:
        if (pos_ == drop_) {
            phase_ = Phase::Unloading;
        } else {
            moveToward(drop_);
        }
        break;

    case Phase::Unloading:
        if (carried_) {
            if (isInputMission_) {
                // Place box in storage slot
                aisle.slotAt(drop_).place(*carried_);
                aisle.notifyBoxPlaced(*carried_, drop_);
            } else {
                // Deliver to output port - make available for Robot
                aisle.notifyOutputReady(*carried_);
            }
            carried_.reset();
        }
        phase_ = Phase::Idle;
        aisle.assignNextTo(*this);
        break;
    }
}
