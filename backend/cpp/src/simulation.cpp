#include "simulation.h"
#include "Aisle.h"
#include "Robot.h"
#include "InputBelt.h"
#include "Scheduler.h"

std::vector<Event> run_simulation(const Params& p) {
    std::vector<Event> log;

    InputBelt belt;
    for (const auto& b : p.boxes) belt.push(b);

    Aisle              aisle(p.num_slots, p.num_shuttles, Position{-1, 0});
    std::vector<Robot> robots(1);
    Scheduler          scheduler(aisle, robots, belt, &log);

    for (int t = 0; t < p.max_ticks; ++t) {
        scheduler.activate();

        const auto meta = aisle.metadata();
        const bool done = belt.empty()
                       && meta.pendingInputs    == 0
                       && meta.pendingOutputs   == 0
                       && meta.readyOutputCount == 0
                       && meta.activeShuttles   == 0;
        if (done) break;
    }

    log.push_back({"done", scheduler.currentTick(), 0, -1, {}});
    return log;
}
