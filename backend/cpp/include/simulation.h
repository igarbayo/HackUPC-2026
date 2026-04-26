#pragma once
#include "types.h"
#include "Box.h"
#include "SnapshotQueue.h"
#include <cstdint>
#include <string>
#include <vector>

struct PrePlacedBox {
    Box      box;
    int      aisle_idx = 0;  // 0-based
    Position pos;

    PrePlacedBox() : box("", "", 0) {}
    PrePlacedBox(Box b, int idx, Position p)
        : box(std::move(b)), aisle_idx(idx), pos(p) {}
};

struct Params {
    std::vector<Box>          boxes;
    std::vector<PrePlacedBox> initial_boxes;
    int num_aisles   = 4;
    int  num_slots         = 20;
    int  num_y             = 2;          // height levels; one shuttle per level
    int  num_sides         = 1;          // 1=Left only, 2=Left+Right
    int  num_robots        = 2;          // number of independent robot arms
    int  max_ticks         = 10000;
    Tick        input_phase_ticks = UINT64_MAX;       // belt stops after this tick; drain continues
    std::string heuristic_name   = "auto"; // "auto" picks stock_proximity(1 robot) or coop(≥2)
    uint64_t    heuristic_seed   = 42;                // seed forwarded to RandomHeuristic
};

// Run the full simulation and return the complete event log.
// The last event is always {"done", final_tick, ...}.
// This is the only function Python (via pybind11) needs to call.
std::vector<Event> run_simulation(const Params& p);

struct SimulationResult {
    std::vector<Event>        events;
    std::vector<TickSnapshot> snapshots;
};

// Like run_simulation but also collects a full TickSnapshot after every tick.
SimulationResult run_simulation_with_state(const Params& p);

// Streaming variant: pushes each TickSnapshot into queue as it is produced.
// Calls queue.markDone() when finished. Returns the full event log.
std::vector<Event> run_simulation_streaming(const Params& p, SnapshotQueue& queue);
