#include "simulation.h"
#include "AisleContainer.h"
#include "Heuristic.h"
#include "Robot.h"
#include "InputBelt.h"
#include "Scheduler.h"
#include <algorithm>

static std::string resolveHeuristic(const Params& p) {
    if (p.heuristic_name.empty() || p.heuristic_name == "auto")
        return (p.num_robots >= 2) ? "coop" : "stock_proximity";
    return p.heuristic_name;
}

std::vector<Event> run_simulation(const Params& p) {
    std::vector<Event> log;

    InputBelt belt;
    for (const auto& b : p.boxes)
        if (b.arrivalTick() <= p.input_phase_ticks) belt.push(b);

    Position port; port.x = -1; port.y = 1; port.z = 1; port.side = 1;
    AisleContainer     container(p.num_aisles, p.num_slots, p.num_y, p.num_sides, port);

    {
        auto sorted = p.initial_boxes;
        std::sort(sorted.begin(), sorted.end(),
            [](const PrePlacedBox& a, const PrePlacedBox& b){ return a.pos.z > b.pos.z; });
        for (const auto& pb : sorted)
            container.placeAt(pb.aisle_idx, pb.box, pb.pos);
    }

    std::vector<Robot> robots(p.num_robots);
    for (int i = 0; i < (int)robots.size(); ++i) {
        robots[i].setRobotId(i);
        robots[i].setHeuristic(makeHeuristic(resolveHeuristic(p), p.heuristic_seed));
    }

    Scheduler          scheduler(container, robots, belt, &log);

    for (int t = 0; t < p.max_ticks; ++t) {
        scheduler.activate();

        const auto meta = container.metadata();
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

SimulationSummary run_simulation_with_summary(const Params& p) {
    SimulationSummary result;

    InputBelt belt;
    for (const auto& b : p.boxes)
        if (b.arrivalTick() <= p.input_phase_ticks) belt.push(b);

    Position port; port.x = -1; port.y = 1; port.z = 1; port.side = 1;
    AisleContainer container(p.num_aisles, p.num_slots, p.num_y, p.num_sides, port);

    {
        auto sorted = p.initial_boxes;
        std::sort(sorted.begin(), sorted.end(),
            [](const PrePlacedBox& a, const PrePlacedBox& b){ return a.pos.z > b.pos.z; });
        for (const auto& pb : sorted)
            container.placeAt(pb.aisle_idx, pb.box, pb.pos);
    }

    std::vector<Robot> robots(p.num_robots);
    for (int i = 0; i < (int)robots.size(); ++i) {
        robots[i].setRobotId(i);
        robots[i].setHeuristic(makeHeuristic(resolveHeuristic(p), p.heuristic_seed));
    }

    Scheduler scheduler(container, robots, belt, &result.events);

    for (int t = 0; t < p.max_ticks; ++t) {
        scheduler.activate();

        const auto meta = container.metadata();
        const bool done = belt.empty()
                       && meta.pendingInputs    == 0
                       && meta.pendingOutputs   == 0
                       && meta.readyOutputCount == 0
                       && meta.activeShuttles   == 0;
        if (done) break;
    }

    result.events.push_back({"done", scheduler.currentTick(), {}, -1, {}});
    result.total_shuttle_moves_x = container.totalMovesX();
    result.total_shuttle_moves_z = container.totalMovesZ();
    return result;
}

SimulationResult run_simulation_with_state(const Params& p) {
    SimulationResult result;

    InputBelt belt;
    for (const auto& b : p.boxes)
        if (b.arrivalTick() <= p.input_phase_ticks) belt.push(b);

    Position port; port.x = -1; port.y = 1; port.z = 1; port.side = 1;
    AisleContainer     container(p.num_aisles, p.num_slots, p.num_y, p.num_sides, port);

    {
        auto sorted = p.initial_boxes;
        std::sort(sorted.begin(), sorted.end(),
            [](const PrePlacedBox& a, const PrePlacedBox& b){ return a.pos.z > b.pos.z; });
        for (const auto& pb : sorted)
            container.placeAt(pb.aisle_idx, pb.box, pb.pos);
    }

    std::vector<Robot> robots(p.num_robots);
    for (int i = 0; i < (int)robots.size(); ++i) {
        robots[i].setRobotId(i);
        robots[i].setHeuristic(makeHeuristic(resolveHeuristic(p), p.heuristic_seed));
    }

    Scheduler          scheduler(container, robots, belt, &result.events);

    for (int t = 0; t < p.max_ticks; ++t) {
        scheduler.activate();

        TickSnapshot snap;
        snap.tick  = scheduler.currentTick();

        for (auto& as : container.snapshot()) snap.aisles.push_back(std::move(as));

        for (int r = 0; r < (int)robots.size(); ++r) {
            const auto& pallets = robots[r].pallets();
            for (int slot = 0; slot < Robot::MAX_ACTIVE_PALLETS; ++slot) {
                if (!pallets[slot]) continue;
                const Pallet& pal = *pallets[slot];
                PalletSnap ps;
                ps.robot_id       = r;
                ps.slot           = slot;
                ps.family         = pal.family();
                ps.placed_count   = pal.placedCount();
                ps.reserved_count = pal.reservedCount();
                for (const auto& box : pal.boxes())
                    ps.boxes.push_back({box.id(), box.family(), box.arrivalTick(), {}});
                snap.pallets.push_back(std::move(ps));
            }
        }
        result.snapshots.push_back(std::move(snap));

        const auto meta = container.metadata();
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
    for (const auto& b : p.boxes)
        if (b.arrivalTick() <= p.input_phase_ticks) belt.push(b);

    Position port; port.x = -1; port.y = 1; port.z = 1; port.side = 1;
    AisleContainer     container(p.num_aisles, p.num_slots, p.num_y, p.num_sides, port);

    {
        auto sorted = p.initial_boxes;
        std::sort(sorted.begin(), sorted.end(),
            [](const PrePlacedBox& a, const PrePlacedBox& b){ return a.pos.z > b.pos.z; });
        for (const auto& pb : sorted)
            container.placeAt(pb.aisle_idx, pb.box, pb.pos);
    }

    std::vector<Robot> robots(p.num_robots);
    for (int i = 0; i < (int)robots.size(); ++i) {
        robots[i].setRobotId(i);
        robots[i].setHeuristic(makeHeuristic(resolveHeuristic(p), p.heuristic_seed));
    }

    Scheduler          scheduler(container, robots, belt, &events);

    std::size_t event_cursor = 0;

    for (int t = 0; t < p.max_ticks; ++t) {
        scheduler.activate();

        TickSnapshot snap;
        snap.tick  = scheduler.currentTick();
        for (auto& as : container.snapshot()) snap.aisles.push_back(std::move(as));

        for (std::size_t i = event_cursor; i < events.size(); ++i)
            snap.events.push_back(events[i]);
        event_cursor = events.size();

        for (int r = 0; r < (int)robots.size(); ++r) {
            const auto& pallets = robots[r].pallets();
            for (int slot = 0; slot < Robot::MAX_ACTIVE_PALLETS; ++slot) {
                if (!pallets[slot]) continue;
                const Pallet& pal = *pallets[slot];
                PalletSnap ps;
                ps.robot_id       = r;
                ps.slot           = slot;
                ps.family         = pal.family();
                ps.placed_count   = pal.placedCount();
                ps.reserved_count = pal.reservedCount();
                for (const auto& box : pal.boxes())
                    ps.boxes.push_back({box.id(), box.family(), box.arrivalTick(), {}});
                snap.pallets.push_back(std::move(ps));
            }
        }
        queue.push(std::move(snap));

        const auto meta = container.metadata();
        const bool done = belt.empty()
                       && meta.pendingInputs    == 0
                       && meta.pendingOutputs   == 0
                       && meta.readyOutputCount == 0
                       && meta.activeShuttles   == 0;
        if (done) break;
    }

    events.push_back({"done", scheduler.currentTick(), {}, -1, {}});
    TickSnapshot done_snap;
    done_snap.tick = scheduler.currentTick();
    done_snap.events.push_back(events.back());
    queue.push(std::move(done_snap));
    queue.markDone();
    return events;
}
