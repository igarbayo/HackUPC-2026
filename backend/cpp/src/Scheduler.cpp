#include "Scheduler.h"

Scheduler::Scheduler(Aisle& aisle, std::vector<Robot>& robots, InputBelt& belt,
                     std::vector<Event>* log)
    : aisle_(aisle), robots_(robots), belt_(belt), eventLog_(log)
{
    if (log) {
        aisle_.setEventLog(log);
        for (auto& r : robots_) r.setEventLog(log);
    }
}

void Scheduler::activate() {
    ++tick_;

    // 1. Each robot ticks
    for (auto& robot : robots_) {
        robot.setCurrentTick(tick_);
        robot.tick(aisle_);
    }

    // 2. Aisle tick (reorder instructions, advance shuttles)
    aisle_.tick();
}

Tick Scheduler::currentTick() const { return tick_; }
