#pragma once
#include "types.h"
#include "Aisle.h"
#include "Robot.h"
#include "InputBelt.h"
#include <vector>

class Scheduler {
public:
    Scheduler(Aisle& aisle, std::vector<Robot>& robots, InputBelt& belt);

    // Execute one tick:
    // 1) drain InputBelt -> aisle.input()
    // 2) each robot.tick(aisle)
    // 3) aisle.tick()
    void activate();
    Tick currentTick() const;

private:
    Aisle&              aisle_;
    std::vector<Robot>& robots_;
    InputBelt&          belt_;
    Tick                tick_ = 0;
};
