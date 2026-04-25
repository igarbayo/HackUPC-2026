#include "Robot.h"
#include "Aisle.h"
#include "Box.h"
#include <cassert>
#include <iostream>
#include <string>

// ── helpers ──────────────────────────────────────────────────────────────────

static int passed = 0;
static int failed = 0;

#define RUN(name) \
    do { \
        std::cout << "  " << #name << " ... "; \
        try { name(); std::cout << "PASS\n"; ++passed; } \
        catch (const std::exception& e) { std::cout << "FAIL (" << e.what() << ")\n"; ++failed; } \
        catch (...) { std::cout << "FAIL (unknown exception)\n"; ++failed; } \
    } while (0)

static Box makeBox(BoxId id, const std::string& family) {
    return Box(id, family, 0);
}

// ── tests ─────────────────────────────────────────────────────────────────────

// Delivering one box opens a pallet and places the box on it.
void test_basic_placement() {
    Robot robot;
    robot.onBoxDelivered(makeBox(1, "A"));

    const auto& pallets = robot.pallets();
    int slot = -1;
    for (int i = 0; i < Robot::MAX_ACTIVE_PALLETS; ++i) {
        if (pallets[i] && pallets[i]->family() == "A") { slot = i; break; }
    }
    assert(slot != -1 && "no pallet opened for family A");
    assert(pallets[slot]->placedCount() == 1);
    assert(pallets[slot]->freeSlots()   == Pallet::CAPACITY - 1);
}

// Delivering CAPACITY boxes dispatches the pallet and frees the slot.
void test_pallet_auto_dispatch_on_full() {
    std::vector<Event> log;
    Robot robot;
    robot.setEventLog(&log);
    robot.setCurrentTick(0);

    for (int i = 1; i <= Pallet::CAPACITY; ++i) {
        robot.setCurrentTick(i);
        robot.onBoxDelivered(makeBox(i, "A"));
    }

    // Slot should be empty — pallet was dispatched
    const auto& pallets = robot.pallets();
    for (int i = 0; i < Robot::MAX_ACTIVE_PALLETS; ++i) {
        assert(!pallets[i] && "pallet slot should be empty after dispatch");
    }

    // Event log must contain exactly one pallet_dispatched for family A
    int dispatched = 0;
    for (const auto& e : log)
        if (e.type == "pallet_dispatched" && e.family == "A") ++dispatched;
    assert(dispatched == 1);

    // And CAPACITY box_on_pallet events
    int placed = 0;
    for (const auto& e : log)
        if (e.type == "box_on_pallet") ++placed;
    assert(placed == Pallet::CAPACITY);
}

// Boxes of four different families open four separate pallets.
void test_multi_family_pallets() {
    Robot robot;
    robot.onBoxDelivered(makeBox(1, "A"));
    robot.onBoxDelivered(makeBox(2, "B"));
    robot.onBoxDelivered(makeBox(3, "C"));
    robot.onBoxDelivered(makeBox(4, "D"));

    const auto& pallets = robot.pallets();
    int occupied = 0;
    for (const auto& p : pallets)
        if (p) ++occupied;
    assert(occupied == 4);
}

// When all 4 slots are occupied and a new family arrives, the least-full pallet
// is force-dispatched to make room.
void test_force_dispatch_least_full() {
    std::vector<Event> log;
    Robot robot;
    robot.setEventLog(&log);
    robot.setCurrentTick(1);

    // Fill all 4 slots with different families (different box counts)
    // A:2  B:3  C:1  D:4
    robot.onBoxDelivered(makeBox(1,  "A"));
    robot.onBoxDelivered(makeBox(2,  "A"));
    robot.onBoxDelivered(makeBox(3,  "B"));
    robot.onBoxDelivered(makeBox(4,  "B"));
    robot.onBoxDelivered(makeBox(5,  "B"));
    robot.onBoxDelivered(makeBox(6,  "C"));          // C:1 — least full
    robot.onBoxDelivered(makeBox(7,  "D"));
    robot.onBoxDelivered(makeBox(8,  "D"));
    robot.onBoxDelivered(makeBox(9,  "D"));
    robot.onBoxDelivered(makeBox(10, "D"));

    // All 4 slots occupied; delivering family E triggers force dispatch
    robot.onBoxDelivered(makeBox(11, "E"));

    // C (1 box) should have been evicted; E should now occupy its slot
    const auto& pallets = robot.pallets();
    bool hasE = false;
    bool hasC = false;
    for (const auto& p : pallets) {
        if (!p) continue;
        if (p->family() == "E") hasE = true;
        if (p->family() == "C") hasC = true;
    }
    assert(hasE && "family E should have a pallet after force dispatch");
    assert(!hasC && "family C (least full) should have been evicted");

    // Exactly one pallet_dispatched event (the forced C eviction)
    int dispatched = 0;
    for (const auto& e : log)
        if (e.type == "pallet_dispatched") ++dispatched;
    assert(dispatched == 1);
}

// Boxes of the same family always land on the same pallet (not scattered).
void test_same_family_same_pallet() {
    Robot robot;
    robot.onBoxDelivered(makeBox(1, "A"));
    robot.onBoxDelivered(makeBox(2, "B"));
    robot.onBoxDelivered(makeBox(3, "A"));

    const auto& pallets = robot.pallets();
    int countA = 0;
    for (const auto& p : pallets)
        if (p && p->family() == "A") countA += p->placedCount();
    assert(countA == 2 && "both A boxes must be on the same pallet");
}

// Full pipeline: box enters the aisle, robot ticks until it collects and places it.
void test_tick_integration_with_aisle() {
    // Small aisle: 5 storage slots, 1 Y level, 1 side, port at x=-1
    Position port; port.x = -1; port.y = 1; port.z = 1; port.side = 1;
    Aisle aisle(5, 1, 1, port);

    std::vector<Event> log;
    aisle.setEventLog(&log);

    Robot robot;
    robot.setEventLog(&log);

    aisle.input(makeBox(42, "X"));

    bool received = false;
    for (int t = 1; t <= 50 && !received; ++t) {
        robot.setCurrentTick(t);
        aisle.tick();
        robot.tick(aisle);

        for (const auto& p : robot.pallets()) {
            if (p && p->family() == "X" && p->placedCount() > 0) {
                received = true;
                break;
            }
        }
    }

    assert(received && "robot should have placed box on pallet within 50 ticks");

    // Verify event sequence: box_stored before box_on_pallet
    int storedIdx = -1, palletIdx = -1;
    for (int i = 0; i < static_cast<int>(log.size()); ++i) {
        if (log[i].type == "box_stored"   && storedIdx == -1) storedIdx = i;
        if (log[i].type == "box_on_pallet" && palletIdx == -1) palletIdx = i;
    }
    assert(storedIdx  != -1 && "expected box_stored event");
    assert(palletIdx  != -1 && "expected box_on_pallet event");
    assert(storedIdx < palletIdx && "box_stored must precede box_on_pallet");
}

// ── main ──────────────────────────────────────────────────────────────────────

int main() {
    std::cout << "Robot tests\n";
    RUN(test_basic_placement);
    RUN(test_pallet_auto_dispatch_on_full);
    RUN(test_multi_family_pallets);
    RUN(test_force_dispatch_least_full);
    RUN(test_same_family_same_pallet);
    RUN(test_tick_integration_with_aisle);

    std::cout << "\n" << passed << " passed, " << failed << " failed\n";
    return failed == 0 ? 0 : 1;
}
