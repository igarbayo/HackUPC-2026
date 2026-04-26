#include "Scheduler.h"
#include <unordered_set>

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

    // 2. Each robot ticks — build per-robot otherClaims from sibling pallets
    for (int i = 0; i < (int)robots_.size(); ++i) {
        std::unordered_set<Family> otherClaims;
        for (int j = 0; j < (int)robots_.size(); ++j) {
            if (j == i) continue;
            for (const auto& p : robots_[j].pallets())
                if (p) otherClaims.insert(p->family());
        }
        robots_[i].setCurrentTick(tick_);
        robots_[i].tick(container_, otherClaims);
    }

    // 3. Aisle tick (reorder instructions, advance shuttles)
    container_.tick();
}

Tick Scheduler::currentTick() const { return tick_; }
