#include "simulation.h"
#include <iostream>

int main() {
    Params p;
    p.num_slots    = 20;
    p.num_shuttles = 2;
    p.max_ticks    = 10000;

    static BoxId nextId = 1;
    for (const std::string f : {"zara", "bershka", "stradivarius", "zara", "bershka",
                                 "zara", "zara", "stradivarius", "bershka", "zara"}) {
        p.boxes.emplace_back(nextId++, f, static_cast<Tick>(p.boxes.size()));
    }

    const auto events = run_simulation(p);

    std::cout << "=== SILOS Simulation ===\n";
    std::cout << events.size() << " events\n\n";

    for (const auto& e : events) {
        std::cout << "[t=" << e.logical_time << "] " << e.type;
        if (e.box_id > 0)       std::cout << "  box="    << e.box_id;
        if (!e.family.empty())  std::cout << "  family=" << e.family;
        if (e.pallet_id >= 0)   std::cout << "  pallet=" << e.pallet_id;
        std::cout << '\n';
    }
    return 0;
}
