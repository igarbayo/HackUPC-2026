#include "Shuttle.h"
#include "Aisle.h"

Shuttle::Shuttle(Position startPos) : pos_(startPos) {}

Position       Shuttle::position()    const { return pos_; }
int            Shuttle::yLevel()      const { return pos_.y; }
bool           Shuttle::isFree()      const { return phase_ == Phase::Idle; }
bool           Shuttle::isOnMission() const { return phase_ != Phase::Idle; }
Shuttle::Phase Shuttle::phase()       const { return phase_; }
const Box*     Shuttle::carriedBox()  const { return carried_ ? &*carried_ : nullptr; }

void Shuttle::assignOutputMission(Position pickupFrom, Position dropAt) {
    pickup_      = pickupFrom;
    drop_        = dropAt;
    missionKind_ = MissionKind::Output;
    phase_       = Phase::MovingToPickup;
}

void Shuttle::assignInputMission(Position pickupFrom, Position dropAt) {
    pickup_      = pickupFrom;
    drop_        = dropAt;
    missionKind_ = MissionKind::Input;
    phase_       = Phase::MovingToPickup;
}

void Shuttle::assignRelocateMission(Position pickupFrom, Position dropAt) {
    pickup_      = pickupFrom;
    drop_        = dropAt;
    missionKind_ = MissionKind::Relocate;
    phase_       = Phase::MovingToPickup;
}

void Shuttle::moveToward(Position target) {
    if (pos_.x < target.x)      ++pos_.x;
    else if (pos_.x > target.x) --pos_.x;
    // shuttle fixed at its Y level; side/z determined by mission targets
}

void Shuttle::tick(Aisle& aisle) {
    switch (phase_) {
    case Phase::Idle:
        break;

    case Phase::MovingToPickup:
        if (pos_.x == pickup_.x) {
            phase_ = Phase::Loading;
            // Fall through: process Loading in same tick
        } else {
            moveToward(pickup_);
            break;
        }
        [[fallthrough]];

    case Phase::Loading:
        if (missionKind_ == MissionKind::Input) {
            auto boxOpt = aisle.takeFromInputBuffer();
            if (boxOpt) {
                carried_ = std::move(*boxOpt);
                phase_ = Phase::MovingToDrop;
            }
            // If no box yet (shouldn't happen in normal flow), stay in Loading
        } else {
            // Output and Relocate: take from storage slot
            Slot& s = aisle.slotAt(pickup_);
            if (pickup_.z == 2 && s.hasZ2()) {
                Box b = s.takeZ2();
                aisle.notifyBoxTaken(b, pickup_);
                carried_ = std::move(b);
                phase_ = Phase::MovingToDrop;
            } else if (pickup_.z == 1 && !s.isEmpty()) {
                Box b = s.take();
                aisle.notifyBoxTaken(b, pickup_);
                carried_ = std::move(b);
                phase_ = Phase::MovingToDrop;
            }
            // else slot not accessible (race condition), retry next tick
        }
        break;

    case Phase::MovingToDrop:
        if (pos_.x == drop_.x) {
            phase_ = Phase::Unloading;
        } else {
            moveToward(drop_);
        }
        break;

    case Phase::Unloading:
        if (carried_) {
            if (missionKind_ == MissionKind::Output) {
                aisle.notifyOutputReady(*carried_);
            } else {
                // Input and Relocate: place box into a storage slot
                Slot& s = aisle.slotAt(drop_);
                if (drop_.z == 2)
                    s.placeZ2(*carried_);
                else
                    s.place(*carried_);
                aisle.notifyBoxPlaced(*carried_, drop_);
            }
            carried_.reset();
        }
        phase_ = Phase::Idle;
        aisle.assignNextTo(*this);
        break;
    }
}
