#include "Aisle.h"
#include "Box.h"
#include <cassert>
#include <cmath>
#include <iostream>
#include <string>

// ── test harness ──────────────────────────────────────────────────────────────

static int passed = 0;
static int failed = 0;

#define RUN(name) \
    do { \
        std::cout << "  " << #name << " ... "; \
        try { name(); std::cout << "PASS\n"; ++passed; } \
        catch (const std::exception& e) { std::cout << "FAIL (" << e.what() << ")\n"; ++failed; } \
        catch (...) { std::cout << "FAIL (unknown exception)\n"; ++failed; } \
    } while (0)

// ── helpers ───────────────────────────────────────────────────────────────────

static Box makeBox(BoxId id, const std::string& family, Tick arrival = 0) {
    return Box(id, family, arrival);
}

static Position pos(int x) {
    Position p; p.x = x; p.y = 1; p.z = 1; p.side = 1;
    return p;
}

// Aisle with port at x=0.  Distance of slot x = |x - 0| = x.
static Aisle makeAisle(int length) {
    Position port; port.x = 0; port.y = 1; port.z = 1; port.side = 1;
    return Aisle(length, 1, 1, port);
}

// Trigger a metadata refresh: tick with no belt, empty queues, idle shuttles.
static void refreshMeta(Aisle& a) { a.tick(); }

// Make z2 at slot x accessible: place a temporary z1 box, placeZ2 the real box,
// then take the z1 box away.
static void placeAccessibleZ2(Aisle& a, int x, Box real) {
    a.slotAt(pos(x)).place(makeBox("_tmp_", "_tmp_"));
    a.slotAt(pos(x)).placeZ2(std::move(real));
    a.slotAt(pos(x)).take();  // z2 is now directly accessible
}

// ── countByFamily — placement ─────────────────────────────────────────────────

// Empty aisle: no family entries.
void test_count_empty_aisle() {
    Aisle a = makeAisle(5);
    refreshMeta(a);
    assert(a.metadata().countByFamily.empty());
}

// Single z1 box → count = 1.
void test_count_one_box_z1() {
    Aisle a = makeAisle(5);
    a.slotAt(pos(2)).place(makeBox("b1", "A"));
    refreshMeta(a);
    assert(a.metadata().countByFamily.at("A") == 1);
}

// Two z1 boxes of the same family → count = 2.
void test_count_two_boxes_same_family() {
    Aisle a = makeAisle(5);
    a.slotAt(pos(1)).place(makeBox("b1", "A"));
    a.slotAt(pos(3)).place(makeBox("b2", "A"));
    refreshMeta(a);
    assert(a.metadata().countByFamily.at("A") == 2);
}

// Different families counted independently.
void test_count_multiple_families() {
    Aisle a = makeAisle(6);
    a.slotAt(pos(1)).place(makeBox("a1", "A"));
    a.slotAt(pos(2)).place(makeBox("b1", "B"));
    a.slotAt(pos(3)).place(makeBox("b2", "B"));
    refreshMeta(a);
    assert(a.metadata().countByFamily.at("A") == 1);
    assert(a.metadata().countByFamily.at("B") == 2);
}

// An accessible z2 box (z1 empty) IS counted.
void test_count_accessible_z2_counted() {
    Aisle a = makeAisle(5);
    placeAccessibleZ2(a, 2, makeBox("b1", "A"));
    refreshMeta(a);
    assert(a.metadata().countByFamily.at("A") == 1);
}

// A blocked z2 box (z1 occupied) is NOT counted — only z1 is visible.
// This documents the current metadata semantics: count = accessible boxes only.
void test_count_blocked_z2_not_counted() {
    Aisle a = makeAisle(5);
    a.slotAt(pos(2)).place(makeBox("b1", "A"));    // z1
    a.slotAt(pos(2)).placeZ2(makeBox("b2", "A"));  // z2 blocked behind z1
    refreshMeta(a);
    assert(a.metadata().countByFamily.at("A") == 1);  // only z1 counted
}

// ── countByFamily — retrieval ─────────────────────────────────────────────────

// Removing one of two boxes decreases count to 1.
void test_count_decreases_on_retrieval() {
    Aisle a = makeAisle(5);
    a.slotAt(pos(1)).place(makeBox("b1", "A"));
    a.slotAt(pos(3)).place(makeBox("b2", "A"));
    refreshMeta(a);
    assert(a.metadata().countByFamily.at("A") == 2);

    a.slotAt(pos(1)).take();
    refreshMeta(a);
    assert(a.metadata().countByFamily.at("A") == 1);
}

// Removing the last box removes the family entry entirely.
void test_count_entry_removed_when_last_box_gone() {
    Aisle a = makeAisle(5);
    a.slotAt(pos(2)).place(makeBox("b1", "A"));
    refreshMeta(a);
    assert(a.metadata().countByFamily.count("A") == 1);

    a.slotAt(pos(2)).take();
    refreshMeta(a);
    assert(a.metadata().countByFamily.count("A") == 0);
}

// Taking z1 from a full slot makes z2 accessible; count stays at 1.
// Then taking z2 drops count to 0.
void test_count_z1_then_accessible_z2_retrieval() {
    Aisle a = makeAisle(5);
    a.slotAt(pos(2)).place(makeBox("b1", "A"));    // z1
    a.slotAt(pos(2)).placeZ2(makeBox("b2", "A"));  // z2 (blocked)
    refreshMeta(a);
    assert(a.metadata().countByFamily.at("A") == 1);  // z1 visible

    a.slotAt(pos(2)).take();  // remove z1 → z2 becomes accessible
    refreshMeta(a);
    assert(a.metadata().countByFamily.at("A") == 1);  // z2 now visible

    a.slotAt(pos(2)).takeZ2();  // remove z2
    refreshMeta(a);
    assert(a.metadata().countByFamily.count("A") == 0);
}

// ── avgDistanceByFamily — placement ──────────────────────────────────────────

// Empty aisle: no avgDistance entries.
void test_avg_dist_empty_aisle() {
    Aisle a = makeAisle(5);
    refreshMeta(a);
    assert(a.metadata().avgDistanceByFamily.empty());
}

// Single box at x=3, port at x=0 → avg distance = 3.
void test_avg_dist_single_box() {
    Aisle a = makeAisle(5);
    a.slotAt(pos(3)).place(makeBox("b1", "A"));
    refreshMeta(a);
    float avg = a.metadata().avgDistanceByFamily.at("A");
    assert(std::abs(avg - 3.0f) < 1e-5f);
}

// Two boxes at x=1 and x=3 → avg = (1+3)/2 = 2.
void test_avg_dist_two_boxes_same_family() {
    Aisle a = makeAisle(5);
    a.slotAt(pos(1)).place(makeBox("b1", "A"));
    a.slotAt(pos(3)).place(makeBox("b2", "A"));
    refreshMeta(a);
    float avg = a.metadata().avgDistanceByFamily.at("A");
    assert(std::abs(avg - 2.0f) < 1e-5f);
}

// Accessible z2 at x=2 contributes its distance: avg = 2.
void test_avg_dist_accessible_z2_contributes() {
    Aisle a = makeAisle(5);
    placeAccessibleZ2(a, 2, makeBox("b1", "A"));
    refreshMeta(a);
    float avg = a.metadata().avgDistanceByFamily.at("A");
    assert(std::abs(avg - 2.0f) < 1e-5f);
}

// A at z1 x=1 (dist=1) and A at accessible z2 x=4 (dist=4) → avg = 2.5.
void test_avg_dist_z1_and_accessible_z2_combined() {
    Aisle a = makeAisle(6);
    a.slotAt(pos(1)).place(makeBox("a1", "A"));
    placeAccessibleZ2(a, 4, makeBox("a2", "A"));
    refreshMeta(a);
    float avg = a.metadata().avgDistanceByFamily.at("A");
    assert(std::abs(avg - 2.5f) < 1e-5f);
}

// Different families have independent averages.
// A at x=2 → avg_A=2.  B at x=1 and x=3 → avg_B=2.
void test_avg_dist_independent_per_family() {
    Aisle a = makeAisle(6);
    a.slotAt(pos(2)).place(makeBox("a1", "A"));
    a.slotAt(pos(1)).place(makeBox("b1", "B"));
    a.slotAt(pos(3)).place(makeBox("b2", "B"));
    refreshMeta(a);
    assert(std::abs(a.metadata().avgDistanceByFamily.at("A") - 2.0f) < 1e-5f);
    assert(std::abs(a.metadata().avgDistanceByFamily.at("B") - 2.0f) < 1e-5f);
}

// ── avgDistanceByFamily — retrieval ──────────────────────────────────────────

// Remove the closer box: avg shifts toward the farther one.
// A at x=1 (dist=1) and x=3 (dist=3) → avg=2.  Remove x=1 → avg=3.
void test_avg_dist_updates_after_retrieval() {
    Aisle a = makeAisle(5);
    a.slotAt(pos(1)).place(makeBox("b1", "A"));
    a.slotAt(pos(3)).place(makeBox("b2", "A"));
    refreshMeta(a);
    assert(std::abs(a.metadata().avgDistanceByFamily.at("A") - 2.0f) < 1e-5f);

    a.slotAt(pos(1)).take();
    refreshMeta(a);
    assert(std::abs(a.metadata().avgDistanceByFamily.at("A") - 3.0f) < 1e-5f);
}

// When the last box of a family is retrieved, the avgDistance entry disappears.
void test_avg_dist_entry_removed_when_empty() {
    Aisle a = makeAisle(5);
    a.slotAt(pos(2)).place(makeBox("b1", "A"));
    refreshMeta(a);
    assert(a.metadata().avgDistanceByFamily.count("A") == 1);

    a.slotAt(pos(2)).take();
    refreshMeta(a);
    assert(a.metadata().avgDistanceByFamily.count("A") == 0);
}

// ── main ──────────────────────────────────────────────────────────────────────

int main() {
    std::cout << "Metadata update tests (countByFamily, avgDistanceByFamily)\n";

    std::cout << "\n-- countByFamily / placement --\n";
    RUN(test_count_empty_aisle);
    RUN(test_count_one_box_z1);
    RUN(test_count_two_boxes_same_family);
    RUN(test_count_multiple_families);
    RUN(test_count_accessible_z2_counted);
    RUN(test_count_blocked_z2_not_counted);

    std::cout << "\n-- countByFamily / retrieval --\n";
    RUN(test_count_decreases_on_retrieval);
    RUN(test_count_entry_removed_when_last_box_gone);
    RUN(test_count_z1_then_accessible_z2_retrieval);

    std::cout << "\n-- avgDistanceByFamily / placement --\n";
    RUN(test_avg_dist_empty_aisle);
    RUN(test_avg_dist_single_box);
    RUN(test_avg_dist_two_boxes_same_family);
    RUN(test_avg_dist_accessible_z2_contributes);
    RUN(test_avg_dist_z1_and_accessible_z2_combined);
    RUN(test_avg_dist_independent_per_family);

    std::cout << "\n-- avgDistanceByFamily / retrieval --\n";
    RUN(test_avg_dist_updates_after_retrieval);
    RUN(test_avg_dist_entry_removed_when_empty);

    std::cout << "\n" << passed << " passed, " << failed << " failed\n";
    return failed == 0 ? 0 : 1;
}
