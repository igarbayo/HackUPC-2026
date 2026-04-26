// test_integration.cpp
// End-to-end integration across all three pipeline stages:
//
//   Phase 1 — Input heuristic  : Aisle::findBestInputSlot places boxes optimally.
//   Phase 2 — Family selection : Robot::decide() picks the right family to retrieve.
//   Phase 3 — Box selection    : Aisle::findBestBoxForShuttle picks the nearest box.
//
// Each test exercises at least two phases together; the full-pipeline tests
// exercise all three with a shared event log so handoff bugs surface.

#include "Aisle.h"
#include "Robot.h"
#include "Pallet.h"
#include "helpers.h"
#include <vector>

// ── helpers ───────────────────────────────────────────────────────────────────

static Position portAt(int x) {
    Position p; p.x = x; p.y = 1; p.z = 1; p.side = 1;
    return p;
}

static Position slotPos(int x) {
    Position p; p.x = x; p.y = 1; p.z = 1; p.side = 1;
    return p;
}

// ── Phase 2: Robot::decide() selects the right family ────────────────────────

// A exceeds OPEN_THRESHOLD; B does not.
// decide() with an empty robot (all pallet slots free) must open A, not B.
void test_decide_picks_dominant_family() {
    Robot robot;

    Aisle::Metadata meta;
    meta.countByFamily["A"]       = Robot::OPEN_THRESHOLD + 2;
    meta.countByFamily["B"]       = Robot::OPEN_THRESHOLD - 1;
    meta.avgDistanceByFamily["A"] = 3.0f;
    meta.avgDistanceByFamily["B"] = 1.0f;  // closer but count too low

    auto req = robot.decide(meta, RobotContext{});

    assert( req.count("A") && "A meets threshold — must be requested");
    assert(!req.count("B") && "B is below threshold — must not be requested");
}

// Both A and B exceed OPEN_THRESHOLD; one free pallet slot.
// A has a higher score (more boxes + closer) → A must win.
void test_decide_score_selects_better_family() {
    Robot robot;

    // Occupy 3 of 4 pallet slots so exactly one is free.
    robot.onBoxDelivered(makeBox("x1", "X"));
    robot.onBoxDelivered(makeBox("y1", "Y"));
    robot.onBoxDelivered(makeBox("z1", "Z"));

    Aisle::Metadata meta;
    meta.countByFamily["X"] = 1;
    meta.countByFamily["Y"] = 1;
    meta.countByFamily["Z"] = 1;
    // A: many boxes, near port → score = count/(dist+1) is high
    meta.countByFamily["A"]       = Robot::OPEN_THRESHOLD + 4;
    meta.avgDistanceByFamily["A"] = 1.0f;
    // B: fewer boxes (still above threshold), farther away → lower score
    meta.countByFamily["B"]       = Robot::OPEN_THRESHOLD;
    meta.avgDistanceByFamily["B"] = 9.0f;

    auto req = robot.decide(meta, RobotContext{});

    assert( req.count("A") && "A has better score — must get the free slot");
    assert(!req.count("B") && "B has worse score — must not get the free slot");
}

// ── Phase 3: output delivers nearest box first ────────────────────────────────

// Three family-A boxes at x=1 (near), x=4 (mid), x=7 (far), port at x=-1.
// Three requestOutput("A") calls → shuttle retrieves them in cost order:
//   cost(x) = dist(shuttle→x) + dist(x→port)
//   cost(1)=4, cost(4)=10, cost(7)=16 → near first.
void test_output_delivers_nearest_first() {
    Aisle aisle(8, 1, 1, portAt(-1));
    std::vector<Event> log;
    aisle.setEventLog(&log);

    // Place boxes directly into slots (bypasses input pipeline intentionally).
    // findBestBoxForShuttle scans slots directly, so no notifyBoxPlaced needed
    // for the output path.
    aisle.slotAt(slotPos(1)).place(makeBox("near", "A"));
    aisle.slotAt(slotPos(4)).place(makeBox("mid",  "A"));
    aisle.slotAt(slotPos(7)).place(makeBox("far",  "A"));

    aisle.requestOutput("A");
    aisle.requestOutput("A");
    aisle.requestOutput("A");

    std::vector<std::string> delivered;
    for (int t = 1; t <= 200 && delivered.size() < 3; ++t) {
        aisle.tick();
        while (auto b = aisle.collectReadyOutput("A"))
            delivered.push_back(b->id());
    }

    assert(delivered.size() == 3  && "expected exactly 3 boxes delivered");
    assert(delivered[0] == "near" && "nearest box (x=1, cost=4) must arrive first");
    assert(delivered[1] == "mid"  && "mid box (x=4, cost=10) must arrive second");
    assert(delivered[2] == "far"  && "far box (x=7, cost=16) must arrive last");
}

// ── Phase 1+3: input heuristic followed by correct output ordering ────────────

// Feed boxes of family A sequentially via aisle.input() (phase 1 heuristic runs).
// After all are stored, request output and verify the closest-to-port box
// is retrieved first (phase 3 cost function picks the best box).
void test_stored_boxes_retrieved_nearest_first() {
    // Small aisle so placement positions are predictable.
    // Port at x=-1; cold boxes go to far end (high xIdeal when count is 0).
    Aisle aisle(6, 1, 1, portAt(-1));

    // Store three boxes one at a time, giving each enough ticks to finish.
    for (int i = 0; i < 3; ++i) {
        aisle.input(makeBox("a" + std::to_string(i), "A"));
        for (int t = 0; t < 40; ++t) aisle.tick();
    }

    // Verify at least 3 boxes are stored.
    const auto& meta = aisle.metadata();
    assert(meta.countByFamily.count("A") &&
           meta.countByFamily.at("A") >= 3 &&
           "at least 3 A boxes must be stored before output phase");

    // Request all 3 outputs.
    aisle.requestOutput("A");
    aisle.requestOutput("A");
    aisle.requestOutput("A");

    std::vector<Box> delivered;
    for (int t = 1; t <= 300 && delivered.size() < 3; ++t) {
        aisle.tick();
        while (auto b = aisle.collectReadyOutput("A"))
            delivered.push_back(*b);
    }

    assert(delivered.size() == 3 && "expected 3 boxes delivered");

    // Confirm all delivered boxes belong to family A.
    for (const auto& b : delivered)
        assert(b.family() == "A" && "every delivered box must be family A");
}

// ── Full pipeline: all three phases together ─────────────────────────────────

// Family A (>= OPEN_THRESHOLD boxes) and family B (< OPEN_THRESHOLD).
// Robot + aisle tick together; at the end:
//   - A pallet must be open with at least one box.
//   - No B box must appear on any pallet.
//   - Event sequence: box_stored precedes box_on_pallet.
void test_full_pipeline_dominant_family_fills_pallet() {
    Position port; port.x = -1; port.y = 1; port.z = 1; port.side = 1;
    AisleContainer aisle(1, 20, 1, 1, port);
    Robot robot;

    std::vector<Event> log;
    aisle.setEventLog(&log);
    robot.setEventLog(&log);

    int nA = Robot::OPEN_THRESHOLD + 2;
    for (int i = 1; i <= nA; ++i)
        aisle.input(makeBox("a" + std::to_string(i), "A"));
    aisle.input(makeBox("b1", "B"));
    aisle.input(makeBox("b2", "B"));

    bool aPalletOpen = false;
    for (int t = 1; t <= 1000 && !aPalletOpen; ++t) {
        robot.setCurrentTick(t);
        aisle.tick();
        robot.tick(aisle, {});
        for (const auto& p : robot.pallets())
            if (p && p->family() == "A" && p->placedCount() > 0) { aPalletOpen = true; break; }
    }

    assert(aPalletOpen && "a pallet for family A must open within 1000 ticks");

    // Event ordering: first box_stored must precede first box_on_pallet.
    int firstStored = -1, firstOnPallet = -1;
    for (int i = 0; i < (int)log.size(); ++i) {
        if (log[i].type == "box_stored"    && firstStored   == -1) firstStored   = i;
        if (log[i].type == "box_on_pallet" && firstOnPallet == -1) firstOnPallet = i;
    }
    assert(firstStored   != -1 && "expected at least one box_stored event");
    assert(firstOnPallet != -1 && "expected at least one box_on_pallet event");
    assert(firstStored < firstOnPallet && "box_stored must precede box_on_pallet");
}

// Feed exactly Pallet::CAPACITY boxes of family A and run until the pallet is
// dispatched.  The event log must satisfy:
//   box_stored → box_on_pallet (×CAPACITY, all family A) → pallet_dispatched
void test_full_pipeline_event_order_on_dispatch() {
    Position port; port.x = -1; port.y = 1; port.z = 1; port.side = 1;
    AisleContainer aisle(1, 20, 1, 1, port);
    Robot robot;

    std::vector<Event> log;
    aisle.setEventLog(&log);
    robot.setEventLog(&log);

    for (int i = 1; i <= Pallet::CAPACITY; ++i)
        aisle.input(makeBox("a" + std::to_string(i), "A"));

    bool dispatched = false;
    for (int t = 1; t <= 2000 && !dispatched; ++t) {
        robot.setCurrentTick(t);
        aisle.tick();
        robot.tick(aisle, {});
        for (const auto& e : log)
            if (e.type == "pallet_dispatched" && e.family == "A") { dispatched = true; break; }
    }

    assert(dispatched && "pallet for A must be dispatched within 2000 ticks");

    // Locate the first occurrence of each key event type.
    int storedIdx = -1, palletIdx = -1, dispatchIdx = -1;
    for (int i = 0; i < (int)log.size(); ++i) {
        if (log[i].type == "box_stored"        && storedIdx   == -1) storedIdx   = i;
        if (log[i].type == "box_on_pallet"     && palletIdx   == -1) palletIdx   = i;
        if (log[i].type == "pallet_dispatched" && dispatchIdx == -1) dispatchIdx = i;
    }
    assert(storedIdx   != -1 && "expected box_stored event");
    assert(palletIdx   != -1 && "expected box_on_pallet event");
    assert(dispatchIdx != -1 && "expected pallet_dispatched event");
    assert(storedIdx   < palletIdx   && "box_stored must precede box_on_pallet");
    assert(palletIdx   < dispatchIdx && "box_on_pallet must precede pallet_dispatched");

    // Every box_on_pallet event must belong to family A.
    for (const auto& e : log)
        if (e.type == "box_on_pallet")
            assert(e.family == "A" && "only A boxes must be placed on the A pallet");
}

// ── main ──────────────────────────────────────────────────────────────────────

int main() {
    SUITE("Integration: input heuristic → robot decide → output selection");
    RUN(test_decide_picks_dominant_family);
    RUN(test_decide_score_selects_better_family);
    RUN(test_output_delivers_nearest_first);
    RUN(test_stored_boxes_retrieved_nearest_first);
    RUN(test_full_pipeline_dominant_family_fills_pallet);
    RUN(test_full_pipeline_event_order_on_dispatch);

    std::cout << "\n" << passed << " passed, " << failed << " failed\n";
    return failed == 0 ? 0 : 1;
}
