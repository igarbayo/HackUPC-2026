#include "simulation.h"
#include "Aisle.h"
#include "Robot.h"
#include "InputBelt.h"
#include "Scheduler.h"

std::vector<Event> run_simulation(const Params& p) {
    std::vector<Event> log;

    // Inicializamos la cinta
    InputBelt belt;
    for (const auto& b : p.boxes) belt.push(b);

    // Inicializamos el aisle y el scheduler
    Position port; port.x = -1; port.y = 1; port.z = 1; port.side = 1;
    Aisle              aisle(p.num_slots, p.num_y, p.num_sides, port);
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

    log.push_back({"done", scheduler.currentTick(), {}, -1, {}});
    return log;
}
