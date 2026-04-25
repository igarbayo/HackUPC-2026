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
    for (int i = 0; i < (int)robots.size(); ++i) robots[i].setRobotId(i);
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

SimulationResult run_simulation_with_state(const Params& p) {
    SimulationResult result;

    InputBelt belt;
    for (const auto& b : p.boxes) belt.push(b);

    Position port; port.x = -1; port.y = 1; port.z = 1; port.side = 1;
    Aisle              aisle(p.num_slots, p.num_y, p.num_sides, port);
    std::vector<Robot> robots(1);
    for (int i = 0; i < (int)robots.size(); ++i) robots[i].setRobotId(i);
    Scheduler          scheduler(aisle, robots, belt, &result.events);

    for (int t = 0; t < p.max_ticks; ++t) {
        scheduler.activate();

        TickSnapshot snap;
        snap.tick  = scheduler.currentTick();
        snap.aisle = aisle.snapshot();

        const auto& pallets = robots[0].pallets();
        for (int slot = 0; slot < Robot::MAX_ACTIVE_PALLETS; ++slot) {
            if (!pallets[slot]) continue;
            const Pallet& pal = *pallets[slot];
            PalletSnap ps;
            ps.slot           = slot;
            ps.family         = pal.family();
            ps.placed_count   = pal.placedCount();
            ps.reserved_count = pal.reservedCount();
            for (const auto& box : pal.boxes())
                ps.boxes.push_back({box.id(), box.family(), box.arrivalTick(), {}});
            snap.pallets.push_back(std::move(ps));
        }
        result.snapshots.push_back(std::move(snap));

        const auto meta = aisle.metadata();
        const bool done = belt.empty()
                       && meta.pendingInputs    == 0
                       && meta.pendingOutputs   == 0
                       && meta.readyOutputCount == 0
                       && meta.activeShuttles   == 0;
        if (done) break;
    }

    result.events.push_back({"done", scheduler.currentTick(), {}, -1, {}});
    return result;
}

std::vector<Event> run_simulation_streaming(const Params& p, SnapshotQueue& queue) {
    std::vector<Event> events;

    InputBelt belt;
    for (const auto& b : p.boxes) belt.push(b);

    Position port; port.x = -1; port.y = 1; port.z = 1; port.side = 1;
    Aisle              aisle(p.num_slots, p.num_y, p.num_sides, port);
    std::vector<Robot> robots(1);
    for (int i = 0; i < (int)robots.size(); ++i) robots[i].setRobotId(i);
    Scheduler          scheduler(aisle, robots, belt, &events);

    for (int t = 0; t < p.max_ticks; ++t) {
        scheduler.activate();

        TickSnapshot snap;
        snap.tick  = scheduler.currentTick();
        snap.aisle = aisle.snapshot();

        const auto& pallets = robots[0].pallets();
        for (int slot = 0; slot < Robot::MAX_ACTIVE_PALLETS; ++slot) {
            if (!pallets[slot]) continue;
            const Pallet& pal = *pallets[slot];
            PalletSnap ps;
            ps.slot           = slot;
            ps.family         = pal.family();
            ps.placed_count   = pal.placedCount();
            ps.reserved_count = pal.reservedCount();
            for (const auto& box : pal.boxes())
                ps.boxes.push_back({box.id(), box.family(), box.arrivalTick(), {}});
            snap.pallets.push_back(std::move(ps));
        }
        queue.push(std::move(snap));

        const auto meta = aisle.metadata();
        const bool done = belt.empty()
                       && meta.pendingInputs    == 0
                       && meta.pendingOutputs   == 0
                       && meta.readyOutputCount == 0
                       && meta.activeShuttles   == 0;
        if (done) break;
    }

    events.push_back({"done", scheduler.currentTick(), {}, -1, {}});
    queue.markDone();
    return events;
}
