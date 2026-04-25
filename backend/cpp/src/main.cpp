#include "simulation.h"
#include "InputBelt.h"
#include <iostream>

int main() {
    BoxGeneratorParams gen;
    gen.num_boxes        = 30;
    gen.num_destinations = 5;
    gen.seed             = 42;

    InputBelt belt = InputBelt::generate(gen);

    Params p;
    p.num_slots    = 20;
    p.num_shuttles = 2;
    p.max_ticks    = 10000;

    while (auto b = belt.pop()) p.boxes.push_back(*b);

    const auto events = run_simulation(p);

    std::cout << "=== SILOS Simulation ===\n";
    std::cout << events.size() << " events\n\n";

    for (const auto& e : events) {
        std::cout << "[t=" << e.logical_time << "] " << e.type;
        if (!e.box_id.empty()) std::cout << "  box=" << e.box_id;
        if (!e.family.empty()) std::cout << "  family=" << e.family;
        if (e.pallet_id >= 0)  std::cout << "  pallet=" << e.pallet_id;
        std::cout << '\n';
    }
    return 0;
}
