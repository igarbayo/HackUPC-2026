#pragma once
#include "types.h"
#include <cstdint>
#include <string>

struct BenchmarkConfig {
    double      rate_boxes_per_hour = 1000.0;   // input cadence
    double      cv                  = 0.3;      // coefficient of variation; std = cv * mean
    Tick        input_phase_ticks   = 3600;     // belt feeds for this many ticks, then drains
    int         num_destinations    = 20;
    uint64_t    seed                = 42;
    std::string heuristic           = "current";
    int         num_robots          = 1;
    int         num_aisles          = 1;
    // aisle geometry
    int         num_slots           = 20;
    int         num_y               = 2;
    int         num_sides           = 1;
    // optional: pre-populate silo from CSV before the simulation starts
    std::string silo_csv_path       = "";
};

struct BenchmarkResult {
    // --- config (echoed for self-contained records) ---
    std::string heuristic;
    int         num_robots;
    int         num_aisles;
    bool        pre_populated  = false;
    double      rate_boxes_per_hour;
    double      mean_inter_arrival_ticks;
    double      std_inter_arrival_ticks;
    int         num_destinations;
    Tick        input_phase_ticks;
    uint64_t    seed;

    // --- raw counts ---
    int    total_pallets_sent = 0;
    int    filled_pallets     = 0;
    int    total_boxes_sent   = 0;
    Tick   finish_tick        = 0;

    // --- derived metrics ---
    double pct_pallets_filled      = 0.0;  // filled_pallets / total_pallets_sent
    double ticks_per_filled_pallet = 0.0;  // finish_tick / filled_pallets; 0 if none filled
    double avg_pallet_capacity     = 0.0;  // total_boxes_sent / (total_pallets_sent * CAPACITY)
    int    boxes_in_warehouse      = 0;    // boxes remaining in the silo at end
    double occupation_pct          = 0.0;  // boxes_in_warehouse / total_capacity * 100

    // --- shuttle movement totals (summed across all aisles) ---
    long long total_shuttle_moves_x = 0;   // cumulative x-steps across all shuttles
    long long total_shuttle_moves_z = 0;   // cumulative z-arm extensions across all shuttles
};

BenchmarkResult run_benchmark(const BenchmarkConfig& cfg);
