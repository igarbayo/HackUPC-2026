// test_relocation.cpp
// Unit tests for Aisle::findRelocationOpportunity.
//
// Port is always at x=-1.  dist(x) = |x - (-1)| = x + 1.
// MIN_GAIN = 3: a relocation only fires when the box moves at least 3 slots
// closer to the port.
// Scoring: score = gain*2 + bonus
//   bonus = 500  (same-family z2 — groups boxes, efficient retrieval)
//   bonus = 100  (any-family z2 — stacks, keeps a free slot available)
//   bonus =   0  (empty z1 slot)

#include "Aisle.h"
#include "Box.h"
#include <cassert>
#include <iostream>

static int passed = 0;
static int failed = 0;

#define RUN(name) \
    do { \
        std::cout << "  " << #name << " ... "; \
        try { name(); std::cout << "PASS\n"; ++passed; } \
        catch (const std::exception& e) \
            { std::cout << "FAIL (" << e.what() << ")\n"; ++failed; } \
        catch (...) { std::cout << "FAIL (unknown exception)\n"; ++failed; } \
    } while (0)

// ── helpers ───────────────────────────────────────────────────────────────────

static Box makeBox(BoxId id, const std::string& fam) { return Box(id, fam, 0); }

static Aisle emptyAisle(int length) {
    Position port; port.x = -1; port.y = 1; port.z = 1; port.side = 1;
    return Aisle(length, 1, 1, port);
}

static Position slot(int x, int z = 1) {
    Position p; p.x = x; p.y = 1; p.z = z; p.side = 1;
    return p;
}

// ── tests ─────────────────────────────────────────────────────────────────────
// Relocation only moves boxes to empty z1 slots.  Placing at z2 of an occupied
// slot is intentionally not supported: z2 behind an occupied z1 would be
// inaccessible until the z1 box is retrieved by a normal output mission.

// Empty aisle — nothing to relocate.
void test_nullopt_on_empty_aisle() {
    Aisle a = emptyAisle(10);
    assert(!a.findRelocationOpportunity(1).has_value());
}

// Box at x=1 (dist=2).  Best possible target is x=0 (dist=1, gain=1 < 3).
// No valid relocation exists.
void test_nullopt_gain_below_threshold() {
    Aisle a = emptyAisle(10);
    a.slotAt(slot(1)).place(makeBox("a", "A"));
    assert(!a.findRelocationOpportunity(1).has_value());
}

// Box at x=7 (dist=8), nearest free slot at x=0 (dist=1).
// gain=7 ≥ MIN_GAIN=3; drop should use z=1 (empty slot).
void test_basic_z1_placement() {
    Aisle a = emptyAisle(10);
    a.slotAt(slot(7)).place(makeBox("a", "A"));
    auto r = a.findRelocationOpportunity(1);
    assert(r.has_value());
    assert(r->first.x  == 7 && "source must be x=7");
    assert(r->second.z == 1 && "empty slot drop must use z=1");
    assert(r->second.x <  7 && "drop must be closer to port");
}

// Family A has an output reservation → skip it.
// Family B (unreserved) at x=8 must be returned instead.
void test_skips_reserved_family() {
    Aisle a = emptyAisle(12);
    a.slotAt(slot(9)).place(makeBox("a1", "A"));  // reserved → skip
    a.slotAt(slot(8)).place(makeBox("b1", "B"));  // not reserved → pick
    a.requestOutput("A");                          // reserves A

    auto r = a.findRelocationOpportunity(1);
    assert(r.has_value());
    assert(r->first.x == 8 && "reserved family A must be skipped; B at x=8 chosen");
}

// Occupied slots must be skipped as drop targets — only empty z1 slots qualify.
// Box A at x=7, slot at x=3 occupied with B: x=3 must be ignored.
// The chosen target must be one of the empty slots closer to port.
void test_occupied_slot_skipped_as_target() {
    Aisle a = emptyAisle(10);
    a.slotAt(slot(7)).place(makeBox("src", "A"));
    a.slotAt(slot(3)).place(makeBox("blk", "B"));  // occupied — must be skipped

    auto r = a.findRelocationOpportunity(1);
    assert(r.has_value());
    assert(r->second.x != 3 && "occupied slot must not be a drop target");
    assert(r->second.z == 1  && "drop must always target z1 of an empty slot");
}

// Gain == MIN_GAIN (=3) is accepted; gain == 2 is not.
void test_gain_exactly_at_boundary() {
    // gain=3 (accepted): box at x=3 (dist=4), nearest empty slot x=0 (dist=1).
    {
        Aisle a = emptyAisle(10);
        a.slotAt(slot(3)).place(makeBox("a", "A"));
        assert(a.findRelocationOpportunity(1).has_value()
               && "gain=3 must be accepted");
    }
    // gain<3 (rejected): box at x=2 (dist=3).
    // Best empty target is x=0 (dist=1, gain=2) or x=1 (dist=2, gain=1) — both < 3.
    {
        Aisle a = emptyAisle(10);
        a.slotAt(slot(2)).place(makeBox("a", "A"));
        assert(!a.findRelocationOpportunity(1).has_value()
               && "max possible gain=2 is below MIN_GAIN=3, must return nullopt");
    }
}

// Among multiple source candidates the one with the highest score is chosen.
//
//   C at x=9 (dist=10): best drop x=0 (dist=1, gain=9, score=18)
//   D at x=6 (dist=7):  best drop x=0 (dist=1, gain=6, score=12)
//   → C wins.
void test_maximizes_score_among_candidates() {
    Aisle a = emptyAisle(12);
    a.slotAt(slot(9)).place(makeBox("c", "C"));
    a.slotAt(slot(6)).place(makeBox("d", "D"));

    auto r = a.findRelocationOpportunity(1);
    assert(r.has_value());
    assert(r->first.x == 9 && "C (higher gain) must be chosen over D");
}

// ── main ──────────────────────────────────────────────────────────────────────

int main() {
    std::cout << "Aisle / findRelocationOpportunity unit tests\n";
    RUN(test_nullopt_on_empty_aisle);
    RUN(test_nullopt_gain_below_threshold);
    RUN(test_basic_z1_placement);
    RUN(test_skips_reserved_family);
    RUN(test_occupied_slot_skipped_as_target);
    RUN(test_gain_exactly_at_boundary);
    RUN(test_maximizes_score_among_candidates);

    std::cout << "\n" << passed << " passed, " << failed << " failed\n";
    return failed == 0 ? 0 : 1;
}
