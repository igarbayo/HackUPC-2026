#include <iostream>
#include <vector>
#include "Aisle.h"
#include "Robot.h"
#include "InputBelt.h"
#include "Scheduler.h"

static BoxId nextId = 1;

Box makeBox(const Family& f, Tick t) {
    return Box(nextId++, f, t);
}

int main() {
    // Aisle with 20 storage slots, 2 shuttles
    // Input port at x=-1 (virtual), output port at x=20 (virtual)
    Aisle aisle(20, 2, Position{-1, 0}, Position{20, 0});
    std::vector<Robot> robots(1);
    InputBelt belt;

    Scheduler scheduler(aisle, robots, belt);

    // Load some boxes onto the belt
    std::vector<std::string> families = {"zara", "bershka", "stradivarius", "zara", "bershka",
                                          "zara", "zara", "stradivarius", "bershka", "zara"};
    for (std::size_t i = 0; i < families.size(); ++i) {
        belt.push(makeBox(families[i], static_cast<Tick>(i)));
    }

    std::cout << "=== SILOS Simulation ===\n";
    std::cout << "Belt size: " << belt.size() << " boxes\n\n";

    for (int t = 0; t < 60; ++t) {
        scheduler.activate();

        auto meta = aisle.metadata();
        std::cout << "Tick " << scheduler.currentTick()
                  << " | freeSlots=" << meta.freeSlots
                  << " | pendingIn=" << meta.pendingInputs
                  << " | pendingOut=" << meta.pendingOutputs
                  << " | belt=" << belt.size();

        for (const auto& [f, cnt] : meta.countByFamily) {
            std::cout << " | " << f << "=" << cnt;
        }

        // Show robot pallet state
        const auto& pallets = robots[0].pallets();
        int openPallets = 0;
        for (const auto& p : pallets) {
            if (p) ++openPallets;
        }
        std::cout << " | openPallets=" << openPallets;
        std::cout << "\n";
    }

    std::cout << "\n=== Final pallet state ===\n";
    for (int i = 0; i < Robot::MAX_ACTIVE_PALLETS; ++i) {
        const auto& p = robots[0].pallets()[i];
        if (p) {
            std::cout << "Pallet[" << i << "]: family=" << p->family()
                      << " placed=" << p->placedCount()
                      << " reserved=" << p->reservedCount() << "\n";
        }
    }

    return 0;
}
