#pragma once
#include "types.h"
#include "AisleContainer.h"
#include "Robot.h"
#include "InputBelt.h"
#include <vector>

class Scheduler {
public:
    // log is optional: pass nullptr (or omit) for standalone/demo use.
    Scheduler(AisleContainer& container, std::vector<Robot>& robots, InputBelt& belt,
              std::vector<Event>* log = nullptr);

    // Execute one tick:
    // 1) each robot.tick(container)   (emits box_on_pallet, pallet_dispatched)
    // 2) container.tick()             (emits box_stored, box_retrieved)
    void activate();
    Tick currentTick() const;

private:
    AisleContainer&     container_;
    std::vector<Robot>& robots_;
    InputBelt&          belt_;
    std::vector<Event>* eventLog_ = nullptr;
    Tick                tick_ = 0;
};
