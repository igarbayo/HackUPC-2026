#include "benchmark.h"
#include "simulation.h"
#include "SiloLoader.h"
#include "InputBelt.h"
#include "Pallet.h"
#include <cmath>

BenchmarkResult run_benchmark(const BenchmarkConfig& cfg) {
    const double mean = 3600.0 / cfg.rate_boxes_per_hour;
    const double std  = cfg.cv * mean;

    // Generous box ceiling: 3x the expected count so max_arrival_tick exits first.
    const double expected_boxes = cfg.rate_boxes_per_hour * (cfg.input_phase_ticks / 3600.0);
    const int    box_ceiling    = static_cast<int>(std::ceil(expected_boxes * 3.0));

    BoxGeneratorParams gen;
    gen.mean_inter_arrival_ticks = mean;
    gen.std_inter_arrival_ticks  = std;
    gen.num_destinations         = cfg.num_destinations;
    gen.seed                     = cfg.seed;
    gen.num_boxes                = box_ceiling;
    gen.max_arrival_tick         = cfg.input_phase_ticks;

    InputBelt belt = InputBelt::generate(gen);

    Params p;
    p.num_slots         = cfg.num_slots;
    p.num_y             = cfg.num_y;
    p.num_sides         = cfg.num_sides;
    p.num_robots        = cfg.num_robots;
    p.input_phase_ticks = cfg.input_phase_ticks;
    p.heuristic_name    = cfg.heuristic;
    p.heuristic_seed    = cfg.seed;
    p.max_ticks         = static_cast<int>(cfg.input_phase_ticks);

    while (auto b = belt.pop()) p.boxes.push_back(std::move(*b));

    if (!cfg.silo_csv_path.empty())
        p.initial_boxes = loadSiloCSV(cfg.silo_csv_path);

    const auto sim = run_simulation_with_summary(p);
    const auto& events = sim.events;

    BenchmarkResult r;
    r.heuristic                = cfg.heuristic;
    r.num_robots               = cfg.num_robots;
    r.num_aisles               = cfg.num_aisles;
    r.pre_populated            = !cfg.silo_csv_path.empty();
    r.rate_boxes_per_hour      = cfg.rate_boxes_per_hour;
    r.mean_inter_arrival_ticks = mean;
    r.std_inter_arrival_ticks  = std;
    r.num_destinations         = cfg.num_destinations;
    r.input_phase_ticks        = cfg.input_phase_ticks;
    r.seed                     = cfg.seed;

    for (const auto& e : events) {
        if (e.type == "done") {
            r.finish_tick = e.logical_time;
            continue;
        }
        if (e.type == "pallet_dispatched" && e.robot_id >= 0) {
            ++r.total_pallets_sent;
            r.total_boxes_sent += e.box_count;
            if (e.box_count == Pallet::CAPACITY) ++r.filled_pallets;
        }
    }

    int stored = 0, retrieved = 0;
    for (const auto& e : events) {
        if (e.type == "box_stored")    ++stored;
        if (e.type == "box_retrieved") ++retrieved;
    }
    const int initial  = static_cast<int>(p.initial_boxes.size());
    const int capacity = cfg.num_aisles * cfg.num_slots * cfg.num_y * cfg.num_sides * 2;
    r.boxes_in_warehouse = initial + stored - retrieved;
    r.occupation_pct     = capacity > 0
        ? 100.0 * r.boxes_in_warehouse / capacity : 0.0;

    if (r.total_pallets_sent > 0) {
        r.pct_pallets_filled  = static_cast<double>(r.filled_pallets) / r.total_pallets_sent;
        r.avg_pallet_capacity = static_cast<double>(r.total_boxes_sent) /
                                (r.total_pallets_sent * Pallet::CAPACITY);
    }
    if (r.filled_pallets > 0) {
        r.ticks_per_filled_pallet = static_cast<double>(r.finish_tick) / r.filled_pallets;
    }

    r.total_shuttle_moves_x = sim.total_shuttle_moves_x;
    r.total_shuttle_moves_z = sim.total_shuttle_moves_z;

    return r;
}
