// test_aisle_container.cpp
// Tests for AisleContainer routing:
//   - findBestInputSlot cost return value (unit)
//   - selectAisleForInput routes to the aisle that has a real free slot (integration)

#include "AisleContainer.h"
#include "Aisle.h"
#include "Box.h"
#include <cassert>
#include <climits>
#include <iostream>
#include <string>

static int passed = 0;
static int failed = 0;

#define RUN(name) \
    do { \
        std::cout << "  " << #name << " ... "; \
        try { name(); std::cout << "PASS\n"; ++passed; } \
        catch (const std::exception& e) { std::cout << "FAIL (" << e.what() << ")\n"; ++failed; } \
        catch (...) { std::cout << "FAIL (unknown exception)\n"; ++failed; } \
    } while (0)

static Box makeBox(BoxId id, const std::string& family) { return Box(id, family, 0); }

// ── Unit: findBestInputSlot cost return ──────────────────────────────────────

// Empty aisle: must return a valid slot with finite cost.
void test_findBestInputSlot_empty_aisle_has_slot() {
    Position port; port.x = -1; port.y = 1; port.z = 1; port.side = 1;
    Aisle aisle(5, 1, 1, port);

    auto [cost, pos] = aisle.findBestInputSlot(makeBox("B1", "A"), 1);

    assert(pos.has_value() && "empty aisle must return a slot");
    assert(cost < INT_MAX  && "cost must be finite for a viable slot");
}

// Full aisle (all z1 occupied): must return {INT_MAX, nullopt}.
void test_findBestInputSlot_full_aisle_returns_nullopt() {
    Position port; port.x = -1; port.y = 1; port.z = 1; port.side = 1;
    Aisle aisle(2, 1, 1, port);  // 2 slots: x=0, x=1

    Box f0 = makeBox("F0", "X"); aisle.slotAt({0, 1, 1, 1}).place(f0); aisle.notifyBoxPlaced(f0, {0, 1, 1, 1});
    Box f1 = makeBox("F1", "X"); aisle.slotAt({1, 1, 1, 1}).place(f1); aisle.notifyBoxPlaced(f1, {1, 1, 1, 1});

    auto [cost, pos] = aisle.findBestInputSlot(makeBox("B1", "A"), 1);

    assert(!pos.has_value() && "full aisle must return nullopt");
    assert(cost == INT_MAX  && "cost must be INT_MAX when no slot is available");
}

// ── Integration: AisleContainer input routing ─────────────────────────────────

// 2 aisles with 1 slot each (length=1). After the first box fills one aisle,
// the second box must be routed to the other aisle.
void test_input_routes_to_aisle_with_free_slot() {
    Position port; port.x = -1; port.y = 1; port.z = 1; port.side = 1;
    AisleContainer container(2, 1, 1, 1, port);

    container.input(makeBox("box0", "A"));
    for (int t = 0; t < 100; ++t) container.tick();

    // Identify which aisle received box0 (shuttles are ordered: [0]=aisle0, [1]=aisle1).
    auto snap0 = container.snapshot();
    assert(snap0.shuttles.size() == 2 && "expected 2 shuttles (one per aisle)");

    int aisleWithBox0 = -1;
    for (int i = 0; i < 2; ++i)
        for (const auto& fb : snap0.shuttles[i].floor_boxes)
            if (fb.id == "box0") aisleWithBox0 = i;
    assert(aisleWithBox0 != -1 && "box0 must be stored before routing the second box");

    // That aisle's only slot is now occupied (freeZ1_ empty) — second box must go elsewhere.
    container.input(makeBox("box1", "B"));
    for (int t = 0; t < 100; ++t) container.tick();

    auto snap1 = container.snapshot();
    int aisleWithBox1 = -1;
    for (int i = 0; i < 2; ++i)
        for (const auto& fb : snap1.shuttles[i].floor_boxes)
            if (fb.id == "box1") aisleWithBox1 = i;

    assert(aisleWithBox1 != -1            && "box1 must be stored");
    assert(aisleWithBox1 != aisleWithBox0 && "box1 must be routed to the other aisle (first is full)");
}

// ── main ─────────────────────────────────────────────────────────────────────

int main() {
    std::cout << "AisleContainer routing tests\n";
    RUN(test_findBestInputSlot_empty_aisle_has_slot);
    RUN(test_findBestInputSlot_full_aisle_returns_nullopt);
    RUN(test_input_routes_to_aisle_with_free_slot);

    std::cout << "\n" << passed << " passed, " << failed << " failed\n";
    return failed == 0 ? 0 : 1;
}
