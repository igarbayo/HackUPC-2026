#include "Scheduler.h"

Scheduler::Scheduler(AisleContainer& container, std::vector<Robot>& robots, InputBelt& belt,
                     std::vector<Event>* log)
    : container_(container), robots_(robots), belt_(belt), eventLog_(log)
{
    if (log) {
        container_.setEventLog(log);
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
        container_.input(std::move(*box));
    }

    // 2. Each robot ticks
    for (auto& robot : robots_) {
        robot.setCurrentTick(tick_);
        robot.tick(container_);
    }

    // 3. Aisle tick (reorder instructions, advance shuttles)
    container_.tick();
}

Tick Scheduler::currentTick() const { return tick_; }
