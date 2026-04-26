#include "Robot.h"
#include "Aisle.h"
#include "helpers.h"

// ── tests ─────────────────────────────────────────────────────────────────────

// Delivering one box opens a pallet and places the box on it.
void test_basic_placement() {
    Robot robot;
    robot.onBoxDelivered(makeBox("1", "A"));

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
        robot.onBoxDelivered(makeBox(std::to_string(i), "A"));
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
    robot.onBoxDelivered(makeBox("1", "A"));
    robot.onBoxDelivered(makeBox("2", "B"));
    robot.onBoxDelivered(makeBox("3", "C"));
    robot.onBoxDelivered(makeBox("4", "D"));

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
    robot.onBoxDelivered(makeBox("1",  "A"));
    robot.onBoxDelivered(makeBox("2",  "A"));
    robot.onBoxDelivered(makeBox("3",  "B"));
    robot.onBoxDelivered(makeBox("4",  "B"));
    robot.onBoxDelivered(makeBox("5",  "B"));
    robot.onBoxDelivered(makeBox("6",  "C"));          // C:1 — least full
    robot.onBoxDelivered(makeBox("7",  "D"));
    robot.onBoxDelivered(makeBox("8",  "D"));
    robot.onBoxDelivered(makeBox("9",  "D"));
    robot.onBoxDelivered(makeBox("10", "D"));

    // All 4 slots occupied; delivering family E triggers force dispatch
    robot.onBoxDelivered(makeBox("11", "E"));

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
    robot.onBoxDelivered(makeBox("1", "A"));
    robot.onBoxDelivered(makeBox("2", "B"));
    robot.onBoxDelivered(makeBox("3", "A"));

    const auto& pallets = robot.pallets();
    int countA = 0;
    for (const auto& p : pallets)
        if (p && p->family() == "A") countA += p->placedCount();
    assert(countA == 2 && "both A boxes must be on the same pallet");
}

// Full pipeline: OPEN_THRESHOLD boxes enter the aisle; robot opens a pallet once
// enough of the same family are stored and delivers them.
void test_tick_integration_with_aisle() {
    // Small aisle: 5 storage slots, 1 Y level, 1 side, port at x=-1
    Position port; port.x = -1; port.y = 1; port.z = 1; port.side = 1;
    AisleContainer aisle(1, 5, 1, 1, port);

    std::vector<Event> log;
    aisle.setEventLog(&log);

    Robot robot;
    robot.setEventLog(&log);

    // Feed exactly OPEN_THRESHOLD boxes of family X so the robot opens a pallet
    for (int i = 1; i <= Robot::OPEN_THRESHOLD; ++i)
        aisle.input(makeBox("x" + std::to_string(i), "X"));

    bool received = false;
    for (int t = 1; t <= 200 && !received; ++t) {
        robot.setCurrentTick(t);
        aisle.tick();
        robot.tick(aisle, {});

        for (const auto& p : robot.pallets()) {
            if (p && p->family() == "X" && p->placedCount() > 0) {
                received = true;
                break;
            }
        }
    }

    assert(received && "robot should have placed box on pallet within 200 ticks");

    // Verify event sequence: at least one box_stored precedes first box_on_pallet
    int storedIdx = -1, palletIdx = -1;
    for (int i = 0; i < static_cast<int>(log.size()); ++i) {
        if (log[i].type == "box_stored"    && storedIdx == -1) storedIdx = i;
        if (log[i].type == "box_on_pallet" && palletIdx == -1) palletIdx = i;
    }
    assert(storedIdx  != -1 && "expected box_stored event");
    assert(palletIdx  != -1 && "expected box_on_pallet event");
    assert(storedIdx < palletIdx && "box_stored must precede box_on_pallet");
}

// When no family reaches OPEN_THRESHOLD the robot still opens pallets (threshold bypass).
void test_threshold_bypass_when_aisle_low() {
    Robot robot;
    // Deliver fewer than OPEN_THRESHOLD boxes of each family directly
    for (int i = 1; i < Robot::OPEN_THRESHOLD; ++i)
        robot.onBoxDelivered(makeBox(std::to_string(i), "A"));

    // Robot should have opened a pallet for A even though count < threshold
    const auto& pallets = robot.pallets();
    int countA = 0;
    for (const auto& p : pallets)
        if (p && p->family() == "A") countA += p->placedCount();
    assert(countA == Robot::OPEN_THRESHOLD - 1 && "bypass: all low-count boxes must be placed");
}

// When the aisle drains to zero, all open pallets are dispatched immediately.
void test_drain_dispatch_on_empty_aisle() {
    std::vector<Event> log;

    // Aisle with 1 shuttle; send exactly OPEN_THRESHOLD boxes then let it drain
    Position port; port.x = -1; port.y = 1; port.z = 1; port.side = 1;
    AisleContainer aisle(1, 10, 1, 1, port);
    aisle.setEventLog(&log);

    Robot robot;
    robot.setEventLog(&log);

    for (int i = 1; i <= Robot::OPEN_THRESHOLD; ++i)
        aisle.input(makeBox(std::to_string(i), "A"));

    // Run until robot has placed at least one box (pallet is open and partially filled)
    bool palletOpen = false;
    int t = 1;
    for (; t <= 400 && !palletOpen; ++t) {
        robot.setCurrentTick(t);
        aisle.tick();
        robot.tick(aisle, {});
        for (const auto& p : robot.pallets())
            if (p && p->family() == "A" && p->placedCount() > 0) { palletOpen = true; break; }
    }
    assert(palletOpen && "pallet should have at least one box before drain check");

    // Keep ticking until the aisle is completely empty and robot dispatches
    bool dispatched = false;
    for (; t <= 1000 && !dispatched; ++t) {
        robot.setCurrentTick(t);
        aisle.tick();
        robot.tick(aisle, {});

        // All pallet slots should be empty once drain fires
        bool anyOpen = false;
        for (const auto& p : robot.pallets()) if (p) { anyOpen = true; break; }
        if (!anyOpen) dispatched = true;
    }
    assert(dispatched && "all pallets should be dispatched once aisle is empty");

    int drainDispatches = 0;
    for (const auto& e : log)
        if (e.type == "pallet_dispatched" && e.family == "A") ++drainDispatches;
    assert(drainDispatches >= 1 && "at least one pallet_dispatched event expected");
}

// decide() must bulk-request all available boxes for an existing pallet in a single call.
// Also verifies the request is capped at freeSlots when the pallet is nearly full.
void test_bulk_request_pipelining() {
    // Case 1: available (5) < freeSlots (CAPACITY-1) → request all 5 at once
    {
        Robot robot;
        robot.onBoxDelivered(makeBox("a1", "A"));  // pallet open, 1 placed, CAPACITY-1 free

        Aisle::Metadata meta;
        meta.countByFamily["A"] = 5;  // 5 available, none in-flight

        auto req = robot.decide(meta, RobotContext{});
        assert(req.count("A")    && "A must appear in request");
        assert(req.at("A") == 5  && "all 5 available boxes must be requested in one call");
    }

    // Case 2: available (5) > freeSlots (1) → request capped at freeSlots
    {
        Robot robot;
        for (int i = 1; i <= Pallet::CAPACITY - 1; ++i)
            robot.onBoxDelivered(makeBox(std::to_string(i), "A"));  // 1 free slot left

        Aisle::Metadata meta;
        meta.countByFamily["A"] = 5;  // 5 available but pallet only has 1 slot

        auto req = robot.decide(meta, RobotContext{});
        assert(req.count("A")    && "A must appear in request");
        assert(req.at("A") == 1  && "request must be capped at freeSlots (1)");
    }
}

// Family X has fewer boxes than OPEN_THRESHOLD but a better score (nearer port).
// Family Y has more boxes than OPEN_THRESHOLD but a worse score (farther away).
// With only one free pallet slot, the robot must open a pallet for Y (passes threshold)
// and must NOT open one for X (fails threshold, score is irrelevant).
void test_threshold_blocks_high_score_low_count_family() {
    Robot robot;

    // Occupy 3 of 4 slots so exactly one is free for a new family
    robot.onBoxDelivered(makeBox("a1", "A"));
    robot.onBoxDelivered(makeBox("b1", "B"));
    robot.onBoxDelivered(makeBox("c1", "C"));

    // Craft metadata:
    //   X: 1 box, avgDist=0  → score = 1/(0+1) = 1.0  (would win if threshold skipped)
    //   Y: OPEN_THRESHOLD+1 boxes, avgDist=10 → score ≈ 0.6  (lower score but passes threshold)
    Aisle::Metadata meta;
    meta.countByFamily["A"] = 1;
    meta.countByFamily["B"] = 1;
    meta.countByFamily["C"] = 1;
    meta.countByFamily["X"] = 1;
    meta.countByFamily["Y"] = Robot::OPEN_THRESHOLD + 1;
    meta.avgDistanceByFamily["X"] = 0.0f;
    meta.avgDistanceByFamily["Y"] = 10.0f;

    auto req = robot.decide(meta, RobotContext{});

    assert(!req.count("X") && "X has fewer boxes than OPEN_THRESHOLD — must not get the free slot");
    assert( req.count("Y") && "Y meets OPEN_THRESHOLD and must get the free slot");
}

// findLeastCompletablePalletSlot scores slots as placed + available_in_aisle (lastMeta_).
// Without aisle data A (placed=1) would be cheapest and evicted.
// After robot.tick() populates lastMeta_ with 5 A-boxes, A's score rises to 6 and
// B (placed=2, inAisle=0 → score=2) becomes the eviction target instead.
void test_eviction_uses_aisle_metadata() {
    Position port; port.x = -1; port.y = 1; port.z = 1; port.side = 1;
    AisleContainer aisle(1, 20, 1, 1, port);
    Robot robot;

    // Fill all 4 pallet slots: A=1 box, B=2, C=3, D=3
    robot.onBoxDelivered(makeBox("a1", "A"));
    robot.onBoxDelivered(makeBox("b1", "B"));
    robot.onBoxDelivered(makeBox("b2", "B"));
    robot.onBoxDelivered(makeBox("c1", "C"));
    robot.onBoxDelivered(makeBox("c2", "C"));
    robot.onBoxDelivered(makeBox("c3", "C"));
    robot.onBoxDelivered(makeBox("d1", "D"));
    robot.onBoxDelivered(makeBox("d2", "D"));
    robot.onBoxDelivered(makeBox("d3", "D"));

    // Store 5 A-boxes in the aisle so lastMeta_ will carry inAisle["A"]=5
    for (int i = 1; i <= 5; ++i)
        aisle.input(makeBox("aa" + std::to_string(i), "A"));
    for (int t = 0; t < 200; ++t)
        aisle.tick();
    assert(aisle.metadata().countByFamily.count("A") &&
           "setup: at least one A box must be stored before robot.tick()");

    // One tick populates lastMeta_ — A's combined score becomes placed(1)+inAisle(5)=6
    robot.setCurrentTick(1);
    robot.tick(aisle, {});

    // Trigger forced eviction: all 4 slots occupied, E needs a new slot
    robot.onBoxDelivered(makeBox("e1", "E"));

    const auto& pallets = robot.pallets();
    bool hasE = false, hasB = false, hasA = false;
    for (const auto& p : pallets) {
        if (!p) continue;
        if (p->family() == "E") hasE = true;
        if (p->family() == "B") hasB = true;
        if (p->family() == "A") hasA = true;
    }
    assert(hasE  && "E must occupy the freed slot");
    assert(!hasB && "B (placed=2, inAisle=0 → score=2) must be evicted");
    assert(hasA  && "A (placed=1, inAisle=5 → score=6) must survive despite lowest placed count");
}

// With one free slot and two threshold-passing families, the one with the higher
// score (more boxes relative to distance) wins even if it has fewer raw boxes.
// X: 9 boxes, avgDist=10 → score = 9/11 ≈ 0.82
// Y: 6 boxes, avgDist=1  → score = 6/2  = 3.0  ← wins
void test_score_prefers_nearby_family() {
    Robot robot;

    // Occupy 3 of 4 slots so exactly one is free
    robot.onBoxDelivered(makeBox("a1", "A"));
    robot.onBoxDelivered(makeBox("b1", "B"));
    robot.onBoxDelivered(makeBox("c1", "C"));

    // Both X and Y have at least OPEN_THRESHOLD boxes; Y is nearby, X is far.
    Aisle::Metadata meta;
    meta.countByFamily["A"] = 1;
    meta.countByFamily["B"] = 1;
    meta.countByFamily["C"] = 1;
    meta.countByFamily["X"] = Robot::OPEN_THRESHOLD + 3;  // 9 boxes, far
    meta.countByFamily["Y"] = Robot::OPEN_THRESHOLD;      // 6 boxes, near
    meta.avgDistanceByFamily["X"] = 10.0f;
    meta.avgDistanceByFamily["Y"] =  1.0f;

    auto req = robot.decide(meta, RobotContext{});

    assert( req.count("Y") && "Y has better score (nearby) — must get the only free slot");
    assert(!req.count("X") && "X has worse score (far away) — must not get the free slot");
}

// Robot holds pallets for A/B/C/D but the aisle contains only family X (belt
// empty, no A/B/C/D boxes anywhere). tick() must dispatch all four exhausted
// pallets and open a new pallet for X within the same tick.
void test_exhausted_families_dispatched_on_tick() {
    std::vector<Event> log;

    Position port; port.x = -1; port.y = 1; port.z = 1; port.side = 1;
    AisleContainer aisle(1, 20, 1, 1, port);
    aisle.setEventLog(&log);

    // Store enough X boxes so robot will open a pallet for X after dispatch
    for (int i = 1; i <= Robot::OPEN_THRESHOLD; ++i)
        aisle.input(makeBox("x" + std::to_string(i), "X"));
    for (int t = 0; t < 100; ++t) aisle.tick();
    assert(aisle.metadata().countByFamily.count("X") > 0 &&
           "setup: X boxes must be stored before robot tick");

    // Fill all 4 pallet slots with A/B/C/D via direct delivery (not through aisle)
    Robot robot;
    robot.setEventLog(&log);
    robot.setCurrentTick(100);
    robot.onBoxDelivered(makeBox("a1", "A"));
    robot.onBoxDelivered(makeBox("b1", "B"));
    robot.onBoxDelivered(makeBox("c1", "C"));
    robot.onBoxDelivered(makeBox("d1", "D"));
    for (int i = 0; i < Robot::MAX_ACTIVE_PALLETS; ++i)
        assert(robot.pallets()[i] && "setup: all 4 pallet slots must be filled");

    // One robot tick: A/B/C/D exhausted → dispatched, X gets a slot
    robot.setCurrentTick(101);
    robot.tick(aisle, {});

    const auto& pallets = robot.pallets();
    bool hasA = false, hasB = false, hasC = false, hasD = false, hasX = false;
    for (const auto& p : pallets) {
        if (!p) continue;
        if (p->family() == "A") hasA = true;
        if (p->family() == "B") hasB = true;
        if (p->family() == "C") hasC = true;
        if (p->family() == "D") hasD = true;
        if (p->family() == "X") hasX = true;
    }
    assert(!hasA && "A has no boxes in aisle — must be dispatched");
    assert(!hasB && "B has no boxes in aisle — must be dispatched");
    assert(!hasC && "C has no boxes in aisle — must be dispatched");
    assert(!hasD && "D has no boxes in aisle — must be dispatched");
    assert(hasX  && "X must get a pallet slot once the four exhausted slots are freed");

    int dispatches = 0;
    for (const auto& e : log)
        if (e.type == "pallet_dispatched") ++dispatches;
    assert(dispatches >= 4 && "all 4 exhausted pallets must emit pallet_dispatched events");
}

// ── main ──────────────────────────────────────────────────────────────────────

int main() {
    SUITE("Robot: pallet management and dispatch");
    RUN(test_basic_placement);
    RUN(test_pallet_auto_dispatch_on_full);
    RUN(test_multi_family_pallets);
    RUN(test_force_dispatch_least_full);
    RUN(test_same_family_same_pallet);
    RUN(test_tick_integration_with_aisle);
    RUN(test_threshold_bypass_when_aisle_low);
    RUN(test_drain_dispatch_on_empty_aisle);
    RUN(test_bulk_request_pipelining);
    RUN(test_eviction_uses_aisle_metadata);
    RUN(test_threshold_blocks_high_score_low_count_family);
    RUN(test_score_prefers_nearby_family);
    RUN(test_exhausted_families_dispatched_on_tick);

    std::cout << "\n" << passed << " passed, " << failed << " failed\n";
    return failed == 0 ? 0 : 1;
}
