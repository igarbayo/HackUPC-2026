// test_aisle_heuristic.cpp
// Desk-test from hackupc.pdf: verifies the numeric cost-function heuristic
// defined in hackupc2.pdf produces the correct slot scores and placement.
//
// Scenario (Y=1, port at X=0):
//   X=1 (x=1): z1=A z2=A   – occupied, next scheduled pick here
//   X=2 (x=2): z1=empty z2=empty
//   X=3 (x=3): z1=empty z2=A  – perfect grouping opportunity
//   X=4 (x=4): z1=empty z2=B  – blocking slot
//
// Incoming box: family A (hot pallet reserved, Xideal=0, X_pick=1)
// Weights: W1=10, W2=1, W3=10
//
// Expected scores:
//   Option 1  x=2 z=1 : 20 + 1000 + 10 =  1030   (burns z2)
//   Option 2  x=2 z=2 :  20 +    0 + 10 =    30   (hypothetical; z=2 of empty slot)
//   Option 3  x=4 z=1 :  40 +  500 + 30 =   570   (blocks family B at z2)
//   Option 4  x=3 z=1 :  30 – 1000 + 20 =  -950   (perfect grouping → WINNER)

#include "Aisle.h"
#include "helpers.h"
#include <cmath>
#include <stdexcept>

// ── cost-function formula (hackupc2.pdf) ─────────────────────────────────────

// PenalizacionZ component: depends on what sits at z=2 of the target slot.
static int penalizacionZ(const Slot& slot, int zs, const Family& incoming) {
    if (zs == 2) return 0;                    // safe insertion at back, no side-effects
    const Box* z2 = slot.peekZ2();
    if (!z2)                        return  1000;   // z2 empty → burned slot
    if (z2->family() == incoming)   return -1000;   // same family → perfect grouping
    return 500;                                      // different family → forced future relocation
}

// Full slot cost: Cost = W1·|Xs−Xideal| + W2·PenalizacionZ + W3·|Xs−X_pick|
static int slotCost(int xs, int zs, const Slot& slot,
                    int x_ideal, int x_pick, const Family& incoming,
                    int W1, int W2, int W3) {
    return W1 * std::abs(xs - x_ideal)
         + W2 * penalizacionZ(slot, zs, incoming)
         + W3 * std::abs(xs - x_pick);
}

// ── initial state builder ────────────────────────────────────────────────────
// port.x=0, length=5 (storage x=0..4).  PDF X=k → code x=k.
// x=0 is occupied by a blocker so it is never selected for new input.
// x=1: z1=A, z2=A   (PDF X=1 – occupied, próximo pick)
// x=2: z1=empty, z2=empty  (PDF X=2 – totally clean)
// x=3: z1=empty, z2=A      (PDF X=3 – grouping opportunity)
// x=4: z1=empty, z2=B      (PDF X=4 – blocking slot)
static void setupHackupcState(Aisle& aisle) {
    // Block x=0 (port slot) so the shuttle never places input boxes there.
    {
        Box blk = makeBox("_port_", "_block_");
        aisle.slotAt({0,1,1,1}).place(blk);
        aisle.notifyBoxPlaced(blk, {0,1,1,1});
    }

    // x=1: z1=A, z2=A
    {
        Box a1 = makeBox("A1", "A");
        Box a2 = makeBox("A2", "A");
        aisle.slotAt({1,1,1,1}).placeZ2(a2);
        aisle.notifyBoxPlaced(a2, {1,1,2,1});
        aisle.slotAt({1,1,1,1}).place(a1);
        aisle.notifyBoxPlaced(a1, {1,1,1,1});
    }

    // x=2: both z=1 and z=2 empty — already clean from constructor, no action.

    // x=3: z1=empty, z2=A
    {
        Box a3    = makeBox("A3", "A");
        aisle.slotAt({3,1,1,1}).placeZ2(a3);
        aisle.notifyBoxPlaced(a3, {3,1,2,1});
    }

    // x=4: z1=empty, z2=B
    {
        Box b1    = makeBox("B1", "B");
        aisle.slotAt({4,1,1,1}).placeZ2(b1);
        aisle.notifyBoxPlaced(b1, {4,1,2,1});
    }
}

// ── tests ─────────────────────────────────────────────────────────────────────

// Verify the four slot-cost values from hackupc.pdf match exactly.
void test_slot_score_values() {
    Position port; port.x = 0; port.y = 1; port.z = 1; port.side = 1;
    Aisle aisle(5, 1, 1, port);
    setupHackupcState(aisle);

    const int W1 = 10, W2 = 1, W3 = 10;
    const int X_ideal = 0;  // hot family: ideal slot is at port head (x=0)
    const int X_pick  = 1;  // next scheduled pick for family A is at x=1
    const Family F    = "A";

    // Option 1: x=2, z=1 — empty slot, z2 also empty → burned slot (+1000)
    int s1 = slotCost(2, 1, aisle.slotAt({2,1,1,1}), X_ideal, X_pick, F, W1, W2, W3);
    assert(s1 == 1030 && "Option 1 (x=2,z=1) score must be 1030");

    // Option 2: x=2, z=2 — hypothetical back insertion → PenZ=0
    // (Current code cannot insert directly into z=2 of an empty slot, but the
    //  cost function still evaluates it for completeness.)
    int s2 = slotCost(2, 2, aisle.slotAt({2,1,1,1}), X_ideal, X_pick, F, W1, W2, W3);
    assert(s2 == 30 && "Option 2 (x=2,z=2) score must be 30");

    // Option 3: x=4, z=1 — z2 has different family B → blocking (+500)
    int s3 = slotCost(4, 1, aisle.slotAt({4,1,1,1}), X_ideal, X_pick, F, W1, W2, W3);
    assert(s3 == 570 && "Option 3 (x=4,z=1) score must be 570");

    // Option 4: x=3, z=1 — z2 has same family A → perfect grouping (-1000) → WINNER
    int s4 = slotCost(3, 1, aisle.slotAt({3,1,1,1}), X_ideal, X_pick, F, W1, W2, W3);
    assert(s4 == -950 && "Option 4 (x=3,z=1) score must be -950");

    assert(s4 < s1 && s4 < s2 && s4 < s3 && "x=3,z=1 must be the global minimum-cost slot");
}

// Verify the cost function correctly identifies the grouping bonus and
// distinguishes it from the burning and blocking cases.
void test_penalizacion_z_cases() {
    Position port; port.x = 0; port.y = 1; port.z = 1; port.side = 1;
    Aisle aisle(5, 1, 1, port);
    setupHackupcState(aisle);

    // z2 empty → burned slot
    assert(penalizacionZ(aisle.slotAt({2,1,1,1}), 1, "A") == 1000);

    // z2 has same family → perfect grouping
    assert(penalizacionZ(aisle.slotAt({3,1,1,1}), 1, "A") == -1000);

    // z2 has different family → blocking
    assert(penalizacionZ(aisle.slotAt({4,1,1,1}), 1, "A") == 500);

    // z=2 insertion is always safe regardless of z2 content
    assert(penalizacionZ(aisle.slotAt({2,1,1,1}), 2, "A") == 0);
    assert(penalizacionZ(aisle.slotAt({3,1,1,1}), 2, "A") == 0);
}

// Integration test: incoming family-A box must be placed at x=3, z=1 (cost=-950).
//
// NOTE: This test verifies the numeric cost-function heuristic from hackupc2.pdf.
//       It will FAIL with the current binary hot/cold heuristic (which places hot
//       boxes at the nearest free slot, i.e. x=2 instead of x=3).
//       The test passes once Aisle::assignNextTo is updated to use slotCost.
void test_integration_best_slot_selected() {
    Position port; port.x = 0; port.y = 1; port.z = 1; port.side = 1;
    Aisle aisle(5, 1, 1, port);
    setupHackupcState(aisle);

    std::vector<Event> log;
    aisle.setEventLog(&log);

    aisle.input(makeBox("NEW_A", "A"));

    // Run until the new box is stored (shuttle starts at port x=0, needs a few ticks)
    for (int t = 1; t <= 50; ++t) {
        aisle.tick();
        const Slot& target = aisle.slotAt({3,1,1,1});
        if (const Box* b = target.peek(); b && b->id() == "NEW_A") return;  // PASS
    }

    // Report where it actually landed for easier debugging
    std::string actual = "(not stored)";
    for (int x = 1; x <= 4; ++x) {
        const Slot& s = aisle.slotAt({x,1,1,1});
        if (const Box* b = s.peek(); b && b->id() == "NEW_A")
            actual = "x=" + std::to_string(x) + " z=1";
    }
    throw std::runtime_error(
        "NEW_A not placed at x=3,z=1 (cost=-950). Actual: " + actual +
        ". Implement numeric cost-function in Aisle::assignNextTo.");
}

// ── main ──────────────────────────────────────────────────────────────────────

int main() {
    SUITE("Aisle: placement heuristic (hackupc.pdf scenario)");
    RUN(test_slot_score_values);
    RUN(test_penalizacion_z_cases);
    RUN(test_integration_best_slot_selected);

    std::cout << "\n" << passed << " passed, " << failed << " failed\n";
    return failed == 0 ? 0 : 1;
}
