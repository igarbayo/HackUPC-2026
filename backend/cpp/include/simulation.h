#pragma once
#include "types.h"
#include "Box.h"
#include <vector>

struct Params {
    std::vector<Box> boxes;
    int num_slots    = 20;
    int num_shuttles = 2;
    int max_ticks    = 10000;
};

// Run the full simulation and return the complete event log.
// The last event is always {"done", final_tick, ...}.
// This is the only function Python (via pybind11) needs to call.
std::vector<Event> run_simulation(const Params& p);
