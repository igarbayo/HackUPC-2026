#pragma once
#include "types.h"
#include "Aisle.h"
#include "Robot.h"
#include "InputBelt.h"
#include <vector>

class Scheduler {
public:
    // log is optional: pass nullptr (or omit) for standalone/demo use.
    Scheduler(Aisle& aisle, std::vector<Robot>& robots, InputBelt& belt,
              std::vector<Event>* log = nullptr);

    // Execute one tick:
    // 1) each robot.tick(aisle)   (emits box_on_pallet, pallet_dispatched)
    // 2) aisle.tick()             (emits box_stored, box_retrieved)
    // Boxes are no longer pushed here — shuttles pull from InputBelt on demand.
    void activate();
    Tick currentTick() const;

private:
    Aisle&              aisle_;
    std::vector<Robot>& robots_;
    InputBelt&          belt_;
    std::vector<Event>* eventLog_ = nullptr;
    Tick                tick_ = 0;
};
