#include "simulation.h"
#include "InputBelt.h"
#include "Pallet.h"
#include <iostream>
#include <unordered_map>

int main(int argc, char* argv[]) {
    const std::string config_path = (argc > 1)
        ? argv[1]
        : "backend/cpp/input/generator_config.json";

    BoxGeneratorParams gen = BoxGeneratorParams::fromFile(config_path);
    InputBelt belt = InputBelt::generate(gen);

    Params p;

    p.num_aisles   = 1;
    p.num_slots    = 20;
    p.num_y        = 4;
    p.num_sides    = 1;
    p.max_ticks    = 10000;

    while (auto b = belt.pop()) p.boxes.push_back(*b);

    const auto events = run_simulation(p);

    // Per-robot accumulators keyed by robot_id
    struct RobotStats {
        int total_pallets  = 0;
        int filled_pallets = 0;
        int total_boxes    = 0;
        std::unordered_map<Family, int> boxes_by_family;
    };
    std::unordered_map<int, RobotStats> stats;

    Tick finish_tick = 0;
    for (const auto& e : events) {
        if (e.type == "done") { finish_tick = e.logical_time; continue; }
        if (e.type == "pallet_dispatched" && e.robot_id >= 0) {
            auto& s = stats[e.robot_id];
            ++s.total_pallets;
            s.total_boxes += e.box_count;
            if (e.box_count == Pallet::CAPACITY) ++s.filled_pallets;
        }
        if (e.type == "box_on_pallet" && e.robot_id >= 0) {
            stats[e.robot_id].boxes_by_family[e.family]++;
        }
    }

    std::cout << "=== SILOS Simulation Summary ===\n";
    std::cout << "Total events : " << events.size() << '\n';
    std::cout << "Finish tick  : " << finish_tick   << "\n\n";

    for (auto& [robot_id, s] : stats) {
        float fill_rate = (s.total_pallets > 0)
            ? static_cast<float>(s.total_boxes) / (s.total_pallets * Pallet::CAPACITY)
            : 0.0f;

        std::cout << "--- Robot " << robot_id << " ---\n";
        std::cout << "  Total pallets sent  : " << s.total_pallets  << '\n';
        std::cout << "  Filled pallets      : " << s.filled_pallets << '\n';
        std::cout << "  Total boxes sent    : " << s.total_boxes    << '\n';
        std::cout << "  Avg fill rate       : " << fill_rate        << '\n';
        std::cout << "  Boxes by family:\n";
        for (const auto& [fam, count] : s.boxes_by_family)
            std::cout << "    " << fam << ": " << count << '\n';
    }

    return 0;
}
