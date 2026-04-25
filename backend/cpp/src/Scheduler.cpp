#include "Scheduler.h"

Scheduler::Scheduler(Aisle& aisle, std::vector<Robot>& robots, InputBelt& belt)
    : aisle_(aisle), robots_(robots), belt_(belt) {}

void Scheduler::activate() {
    ++tick_;

    // 1. Drain one box from InputBelt into Aisle
    if (auto boxOpt = belt_.pop()) {
        aisle_.input(*boxOpt);
    }

    // 2. Each robot ticks
    for (auto& robot : robots_) {
        robot.tick(aisle_);
    }

    // 3. Aisle tick (reorder instructions, advance shuttles)
    aisle_.tick();
}

Tick Scheduler::currentTick() const { return tick_; }
