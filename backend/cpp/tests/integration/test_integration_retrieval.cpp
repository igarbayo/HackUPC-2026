// test_integration_retrieval.cpp
// Integration test: silo with A-in-front-of-B scenario.
//
// Initial state (Y=1, port at X=0, length=5):
//   X=0: _block_
//   X=1: z1=A1  z2=A2
//   X=2: z1=B2  z2=empty   <- second B, directly accessible
//   X=3: z1=empty  z2=A3   <- only free z1 slot / grouping opportunity
//   X=4: z1=A4  z2=B1      <- A in front of B
//
// Sequence tested: insert NEW_A -> request B -> request A -> request A
// Assertions: correct slot placement, correct families out, correct pallet contents.

#include "Aisle.h"
#include "Pallet.h"
#include "helpers.h"
#include <optional>
#include <stdexcept>

// ── helpers ───────────────────────────────────────────────────────────────────

static Position makePort() {
    Position p;
    p.x = 0; p.y = 1; p.z = 1; p.side = 1;
    return p;
}

static void setupState(Aisle& aisle) {
    // x=0: block port slot so shuttle never uses it for new boxes
    {
        Box blk = makeBox("_port_", "_block_");
        aisle.slotAt({0,1,1,1}).place(blk);
        aisle.notifyBoxPlaced(blk, {0,1,1,1});
    }
    // x=1: z1=A1, z2=A2  (full)
    {
        Box a1 = makeBox("A1", "A");
        Box a2 = makeBox("A2", "A");
        aisle.slotAt({1,1,1,1}).place(a1);
        aisle.notifyBoxPlaced(a1, {1,1,1,1});
        aisle.slotAt({1,1,1,1}).placeZ2(a2);
    }
    // x=2: z1=B2, z2=empty
    {
        Box b2 = makeBox("B2", "B");
        aisle.slotAt({2,1,1,1}).place(b2);
        aisle.notifyBoxPlaced(b2, {2,1,1,1});
    }
    // x=3: z1=empty, z2=A3  (dummy trick: place dummy, add A3 to z2, remove dummy)
    {
        Box dummy = makeBox("_dx3_", "_block_");
        Box a3    = makeBox("A3", "A");
        aisle.slotAt({3,1,1,1}).place(dummy);
        aisle.notifyBoxPlaced(dummy, {3,1,1,1});
        aisle.slotAt({3,1,1,1}).placeZ2(a3);
        Box taken = aisle.slotAt({3,1,1,1}).take();
        aisle.notifyBoxTaken(taken, {3,1,1,1});
    }
    // x=4: z1=A4, z2=B1  (A in front of B; placeZ2 valid because z1 occupied)
    {
        Box a4 = makeBox("A4", "A");
        Box b1 = makeBox("B1", "B");
        aisle.slotAt({4,1,1,1}).place(a4);
        aisle.notifyBoxPlaced(a4, {4,1,1,1});
        aisle.slotAt({4,1,1,1}).placeZ2(b1);
    }
}

static Box waitForOutput(Aisle& aisle, const Family& f, int max_ticks = 150) {
    for (int t = 0; t < max_ticks; ++t) {
        aisle.tick();
        auto box = aisle.collectReadyOutput(f);
        if (box) return *box;
    }
    throw std::runtime_error("Timeout waiting for output family=" + f);
}

// ── tests ─────────────────────────────────────────────────────────────────────

// NEW_A must land at x=3, z=1 — the only free z1 slot in the silo.
void test_insertion_placement() {
    Aisle aisle(5, 1, 1, makePort());
    setupState(aisle);

    aisle.input(makeBox("NEW_A", "A"));

    for (int t = 0; t < 50; ++t) {
        aisle.tick();
        const Box* b = aisle.slotAt({3,1,1,1}).peek();
        if (b && b->id() == "NEW_A") return;
    }
    throw std::runtime_error("NEW_A not placed at x=3,z=1 (only free z1 slot)");
}

// Requesting family B must yield B2 (at x=2 z1, nearest accessible B).
// B1 at x=4 z2 is blocked by A4, so B2 comes first.
void test_retrieval_B() {
    Aisle aisle(5, 1, 1, makePort());
    setupState(aisle);

    aisle.requestOutput("B");
    Box box = waitForOutput(aisle, "B");

    if (box.family() != "B")
        throw std::runtime_error("Expected family B, got " + box.family());
    if (box.id() != "B2")
        throw std::runtime_error("Expected B2 (nearest accessible B at x=2 z1), got " + box.id());
}

// Two consecutive A requests: A1 comes first (nearest at x=1 z1),
// second A is any family-A box.
void test_retrieval_A_A() {
    Aisle aisle(5, 1, 1, makePort());
    setupState(aisle);

    aisle.requestOutput("A");
    Box first = waitForOutput(aisle, "A");
    if (first.family() != "A")
        throw std::runtime_error("First A: wrong family " + first.family());
    if (first.id() != "A1")
        throw std::runtime_error("First A: expected A1 (x=1 z1), got " + first.id());

    aisle.requestOutput("A");
    Box second = waitForOutput(aisle, "A");
    if (second.family() != "A")
        throw std::runtime_error("Second A: wrong family " + second.family());
}

// Full sequence: insert NEW_A, then retrieve B, A, A.
// Verifies intermediate slot state and final pallet contents.
void test_full_sequence() {
    Aisle aisle(5, 1, 1, makePort());
    setupState(aisle);

    // Insert NEW_A -> must land at x=3 z1 (only free z1 slot)
    aisle.input(makeBox("NEW_A", "A"));
    bool placed = false;
    for (int t = 0; t < 50; ++t) {
        aisle.tick();
        const Box* b = aisle.slotAt({3,1,1,1}).peek();
        if (b && b->id() == "NEW_A") { placed = true; break; }
    }
    if (!placed)
        throw std::runtime_error("NEW_A not placed at x=3,z=1");

    // Request B -> B2 (nearest accessible B; B1 is blocked by A4)
    aisle.requestOutput("B");
    Box b = waitForOutput(aisle, "B");
    if (b.family() != "B")
        throw std::runtime_error("Request B: wrong family " + b.family());
    if (b.id() != "B2")
        throw std::runtime_error("Request B: expected B2, got " + b.id());

    Pallet palletB("B");
    palletB.placeBox(b);

    // Request A (first) -> A1 (nearest accessible A at x=1 z1)
    aisle.requestOutput("A");
    Box a1 = waitForOutput(aisle, "A");
    if (a1.family() != "A")
        throw std::runtime_error("First A: wrong family " + a1.family());
    if (a1.id() != "A1")
        throw std::runtime_error("First A: expected A1, got " + a1.id());

    Pallet palletA("A");
    palletA.placeBox(a1);

    // Request A (second)
    aisle.requestOutput("A");
    Box a2 = waitForOutput(aisle, "A");
    if (a2.family() != "A")
        throw std::runtime_error("Second A: wrong family " + a2.family());

    palletA.placeBox(a2);

    // Pallet assertions
    if (static_cast<int>(palletB.boxes().size()) != 1)
        throw std::runtime_error("palletB: expected 1 box, got "
            + std::to_string(palletB.boxes().size()));
    if (palletB.boxes()[0].family() != "B")
        throw std::runtime_error("palletB: box is not family B");

    if (static_cast<int>(palletA.boxes().size()) != 2)
        throw std::runtime_error("palletA: expected 2 boxes, got "
            + std::to_string(palletA.boxes().size()));
    for (const auto& bx : palletA.boxes())
        if (bx.family() != "A")
            throw std::runtime_error("palletA: contains non-A box " + bx.id());
}

// ── main ──────────────────────────────────────────────────────────────────────

int main() {
    SUITE("Integration: retrieval sequence (B, A, A)");
    RUN(test_insertion_placement);
    RUN(test_retrieval_B);
    RUN(test_retrieval_A_A);
    RUN(test_full_sequence);
    std::cout << "\n" << passed << " passed, " << failed << " failed\n";
    return failed == 0 ? 0 : 1;
}
