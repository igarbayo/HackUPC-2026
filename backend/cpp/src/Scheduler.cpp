#include "Scheduler.h"

Scheduler::Scheduler(Aisle& aisle, std::vector<Robot>& robots, InputBelt& belt,
                     std::vector<Event>* log)
    : aisle_(aisle), robots_(robots), belt_(belt), eventLog_(log)
{
    aisle_.connectBelt(belt_);
    if (log) {
        aisle_.setEventLog(log);
        for (auto& r : robots_) r.setEventLog(log);
    }
}

void Scheduler::activate() {
    ++tick_;

    // 1. Feed boxes whose arrival_tick has come
    while (belt_.peek() && belt_.peek()->arrivalTick() <= tick_) {
        auto box = belt_.pop();
        if (eventLog_)
            eventLog_->push_back({"box_arrived", tick_, box->id(), -1, box->family()});
        aisle_.input(std::move(*box));
    }

    // 2. Each robot ticks
    for (auto& robot : robots_) {
        robot.setCurrentTick(tick_);
        robot.tick(aisle_);
    }

    // 3. Aisle tick (reorder instructions, advance shuttles)
    aisle_.tick();
}

Tick Scheduler::currentTick() const { return tick_; }
