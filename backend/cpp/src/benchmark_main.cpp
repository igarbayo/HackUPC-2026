#include "benchmark.h"
#include "json.hpp"
#include <iostream>
#include <vector>

int main() {
    // Benchmark grid
    const std::vector<double>      rates      = {1000.0, 400.0};
    const std::vector<uint64_t>    seeds      = {42, 123, 777};
    const std::vector<std::string> heuristics = {"stock_proximity", "largest_stock",
                                                  "nearest", "random"};
    const Tick phase = 1 * 3600;  // 8-hour working shift; belt feeds throughout

    nlohmann::json output = nlohmann::json::array();

    for (double rate : rates) {
        for (uint64_t seed : seeds) {
            for (const auto& heuristic : heuristics) {
            BenchmarkConfig cfg;
            cfg.rate_boxes_per_hour = rate;
            cfg.cv                  = 0.3;
            cfg.input_phase_ticks   = phase;
            cfg.num_destinations    = 20;
            cfg.seed                = seed;
            cfg.heuristic           = heuristic;
            cfg.num_robots          = 1;
            cfg.num_aisles          = 1;

            const BenchmarkResult r = run_benchmark(cfg);

            output.push_back({
                {"heuristic",                r.heuristic},
                {"num_robots",               r.num_robots},
                {"num_aisles",               r.num_aisles},
                {"rate_boxes_per_hour",      r.rate_boxes_per_hour},
                {"mean_inter_arrival_ticks", r.mean_inter_arrival_ticks},
                {"std_inter_arrival_ticks",  r.std_inter_arrival_ticks},
                {"num_destinations",         r.num_destinations},
                {"input_phase_ticks",        r.input_phase_ticks},
                {"seed",                     r.seed},
                {"total_pallets_sent",       r.total_pallets_sent},
                {"filled_pallets",           r.filled_pallets},
                {"total_boxes_sent",         r.total_boxes_sent},
                {"finish_tick",              r.finish_tick},
                {"pct_pallets_filled",       r.pct_pallets_filled},
                {"ticks_per_filled_pallet",  r.ticks_per_filled_pallet},
                {"avg_pallet_capacity",      r.avg_pallet_capacity},
            });
            } // heuristic
        }     // seed
    }         // rate

    std::cout << output.dump(2) << '\n';
    return 0;
}
