#pragma once
#include "types.h"
#include "Box.h"
#include "SnapshotQueue.h"
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
    int num_slots    = 20;
    int num_y        = 2;   // height levels; one shuttle per level
    int num_sides    = 1;   // 1=Left only, 2=Left+Right
    int max_ticks    = 10000;
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
