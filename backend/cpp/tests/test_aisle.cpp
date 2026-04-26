#include "Aisle.h"
#include "helpers.h"

// ── helpers ───────────────────────────────────────────────────────────────────

// Build a small aisle, feed N boxes of given family, tick until all stored.
// Cold placement (not hot) → boxes go to the far end: x = length-1, length-2, …
static Aisle makeAisleWithBoxes(int length, const std::string& family, int count) {
    Position port; port.x = -1; port.y = 1; port.z = 1; port.side = 1;
    Aisle aisle(length, 1, 1, port);
    for (int i = 0; i < count; ++i) {
        aisle.input(makeBox(std::to_string(i), family));
        for (int t = 0; t < 100; ++t) aisle.tick();  // increased from 25 to 100
    }
    return aisle;
}

static Position shuttleAt(int x) {
    Position p; p.x = x; p.y = 1; p.z = 1; p.side = 1;
    return p;
}

static Position slotPos(int x) {
    Position p; p.x = x; p.y = 1; p.z = 1; p.side = 1;
    return p;
}

static Aisle emptyAisle(int length) {
    Position port; port.x = -1; port.y = 1; port.z = 1; port.side = 1;
    return Aisle(length, 1, 1, port);
}

// ── tests ─────────────────────────────────────────────────────────────────────

// Shuttle near a box farther from the port should prefer that box because
// total cost = dist(shuttle→box) + dist(box→port) is lower.
//
// Aisle(5): cold boxes stored at x=4 then x=3.  Shuttle at x=0 (port side):
//   cost(x=3) = |0-3| + |3-(-1)| = 3 + 4 = 7   ← lower
//   cost(x=4) = |0-4| + |4-(-1)| = 4 + 5 = 9
void test_findBestBox_prefers_lower_cost() {
    Aisle aisle = makeAisleWithBoxes(5, "A", 2);
    auto result = aisle.findBestBoxForShuttle("A", shuttleAt(0));
    assert(result.has_value() && "expected a box to be found");
    assert(result->x == 3 && "shuttle at x=0 should prefer x=3 (cost 7 < 9)");
}

// When two boxes have equal total cost the tiebreak should pick the one
// with larger x (frees a far slot, keeps near slots available).
//
// Same aisle (boxes at x=3 and x=4).  Shuttle at x=4:
//   cost(x=3) = |4-3| + 4 = 5
//   cost(x=4) = |4-4| + 5 = 5   ← tie → prefer larger x = 4
void test_findBestBox_tiebreak_farther_x() {
    Aisle aisle = makeAisleWithBoxes(5, "A", 2);
    auto result = aisle.findBestBoxForShuttle("A", shuttleAt(4));
    assert(result.has_value() && "expected a box to be found");
    assert(result->x == 4 && "tiebreak: should prefer x=4 (larger)");
}

// Requesting a family that has no box in the aisle returns nullopt.
void test_findBestBox_no_family_returns_nullopt() {
    Position port; port.x = -1; port.y = 1; port.z = 1; port.side = 1;
    Aisle aisle(5, 1, 1, port);
    auto result = aisle.findBestBoxForShuttle("A", shuttleAt(0));
    assert(!result.has_value() && "empty aisle should return nullopt");
}

// Requesting a family different from what is stored returns nullopt.
void test_findBestBox_wrong_family_returns_nullopt() {
    Aisle aisle = makeAisleWithBoxes(5, "B", 1);
    auto result = aisle.findBestBoxForShuttle("A", shuttleAt(0));
    assert(!result.has_value() && "no A boxes stored, should return nullopt");
}

// A shuttle finishing an input mission ends up at its drop position, not at
// its current mid-trip position.  The box that looks cheaper from mid-trip
// may not be optimal once the shuttle is actually free.
//
// Boxes at x=3 (A) and x=4 (B).  Shuttle mid-trip at x=2:
//   cost(x=3) = |2-3| + |3-(-1)| = 1+4 = 5  ← A is cheaper, closer to current
//   cost(x=4) = |2-4| + |4-(-1)| = 2+5 = 7
//
// Shuttle at drop position x=4 (just finished storing a box there):
//   cost(x=3) = |4-3| + 4 = 5
//   cost(x=4) = |4-4| + 5 = 5   ← tie → tiebreak: prefer x=4
//
// Using the drop position correctly selects x=4 (farther from the current
// mid-trip position), not x=3 which only looked better mid-trip.
void test_findBestBox_drop_position_vs_current() {
    Aisle aisle = makeAisleWithBoxes(5, "A", 2);  // cold storage: x=4 first, then x=3

    // From mid-trip position x=2: x=3 is strictly cheaper.
    auto fromCurrent = aisle.findBestBoxForShuttle("A", shuttleAt(2));
    assert(fromCurrent.has_value() && fromCurrent->x == 3);

    // From drop position x=4: tie broken to x=4 (the one farther from x=2).
    auto fromDrop = aisle.findBestBoxForShuttle("A", shuttleAt(4));
    assert(fromDrop.has_value() && fromDrop->x == 4);
}

// Only one box of the requested family → always returned regardless of shuttle position.
void test_findBestBox_single_box() {
    Aisle aisle = makeAisleWithBoxes(5, "A", 1);  // box lands at x=4
    auto r1 = aisle.findBestBoxForShuttle("A", shuttleAt(0));
    auto r2 = aisle.findBestBoxForShuttle("A", shuttleAt(4));
    assert(r1.has_value() && r1->x == 4);
    assert(r2.has_value() && r2->x == 4);
}

// z2 is blocked when z1 holds a different family.
// The function must skip that slot and find the accessible box elsewhere.
//
//   x=0: z1=G (blocker), z2=F (blocked — cannot reach)
//   x=4: z1=F (accessible)
//   shuttle at x=0 → must return x=4
void test_findBestBox_blocked_z2_skipped() {
    Aisle aisle = emptyAisle(5);
    aisle.slotAt(slotPos(0)).placeZ2(makeBox("f1", "F"));  // F at z2
    aisle.slotAt(slotPos(0)).place(makeBox("g1", "G"));    // G at z1 (blocks F)
    aisle.slotAt(slotPos(4)).place(makeBox("f2", "F"));    // F is accessible

    auto result = aisle.findBestBoxForShuttle("F", shuttleAt(0));
    assert(result.has_value() && "expected accessible F box at x=4");
    assert(result->x == 4 && "blocked z2 at x=0 must be skipped");
    assert(result->z == 1 && "should be z1");
}

// z2 is accessible when z1 is empty.
// Simulate: place F at z2 (directly accessible since z1 is empty).
//
//   x=0: z1=empty, z2=F  →  directly accessible
//   shuttle at x=0 → should return x=0, z=2
void test_findBestBox_accessible_z2_returned() {
    Aisle aisle = emptyAisle(5);
    aisle.slotAt(slotPos(0)).placeZ2(makeBox("f1", "F"));  // F at z2 (z1 is empty)

    auto result = aisle.findBestBoxForShuttle("F", shuttleAt(0));
    assert(result.has_value() && "F at z2 should be accessible");
    assert(result->x == 0 && result->z == 2 && "should point to z2 at x=0");
}

// Full-cell preference: picking z1 from a full slot (z1+z2 both occupied)
// makes z2 immediately accessible — this is prioritised over a lone z1 box
// when the cost is equal.
//
//   x=2: z1=F (alone)         — picking it frees nothing extra
//   x=3: z1=F, z2=G (full)   — picking z1 makes G accessible
//
// Reversed: full at x=2, lone at x=3. Shuttle at x=4:
//   same costs, but without full-cell heuristic x=3 (larger) would win.
//   With full-cell heuristic x=2 (full cell) wins over x=3 (lone).
void test_findBestBox_prefers_full_cell() {
    Aisle aisle = emptyAisle(5);

    // x=2: full cell — z1=F, z2=G (must place z2 then z1)
    aisle.slotAt(slotPos(2)).placeZ2(makeBox("g1", "G"));
    aisle.slotAt(slotPos(2)).place(makeBox("f1", "F"));

    // x=3: lone z1=F
    aisle.slotAt(slotPos(3)).place(makeBox("f2", "F"));

    // shuttle at x=4: both have cost 5 → full-cell preference selects x=2
    auto result = aisle.findBestBoxForShuttle("F", shuttleAt(4));
    assert(result.has_value());
    assert(result->x == 2 && "full-cell box at x=2 must beat lone box at x=3");
    assert(result->z == 1);
}

// ── main ──────────────────────────────────────────────────────────────────────

int main() {
    SUITE("Aisle: findBestBoxForShuttle");
    RUN(test_findBestBox_prefers_lower_cost);
    RUN(test_findBestBox_tiebreak_farther_x);
    RUN(test_findBestBox_no_family_returns_nullopt);
    RUN(test_findBestBox_wrong_family_returns_nullopt);
    RUN(test_findBestBox_drop_position_vs_current);
    RUN(test_findBestBox_single_box);
    RUN(test_findBestBox_blocked_z2_skipped);
    RUN(test_findBestBox_accessible_z2_returned);
    RUN(test_findBestBox_prefers_full_cell);

    std::cout << "\n" << passed << " passed, " << failed << " failed\n";
    return failed == 0 ? 0 : 1;
}
