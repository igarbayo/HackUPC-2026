# Aisle changes needed to support Robot heuristics

Two fields were added to `Aisle::Metadata` (already in `Aisle.h`).
`Aisle.cpp` must populate them in `updateMeta()` and keep them consistent
via `notifyBoxPlaced` / `notifyBoxTaken`.

---

## 1. `oldestArrivalByFamily`

**Type:** `std::unordered_map<Family, Tick>`

**Meaning:** for each family currently stored in the aisle, the minimum
`arrivalTick` across all its boxes. Used by the Robot to detect starvation
(a family waiting more than `MAX_STARVATION_TICKS` without getting a pallet).

### How to populate

Add a parallel map as a private member:

```cpp
std::unordered_map<Family, Tick> oldestArrivalByFamily_;
```

In `notifyBoxPlaced(const Box& b, Position)`:

```cpp
auto it = oldestArrivalByFamily_.find(b.family());
if (it == oldestArrivalByFamily_.end() || b.arrivalTick() < it->second)
    oldestArrivalByFamily_[b.family()] = b.arrivalTick();
```

In `notifyBoxTaken(const Box& b, Position)`:
The minimum may have belonged to the removed box, so recompute:

```cpp
oldestArrivalByFamily_.erase(b.family());
for (const auto& slot : slots_) {
    if (!slot.isEmpty() && slot.peek()->family() == b.family()) {
        Tick t = slot.peek()->arrivalTick();
        auto it = oldestArrivalByFamily_.find(b.family());
        if (it == oldestArrivalByFamily_.end() || t < it->second)
            oldestArrivalByFamily_[b.family()] = t;
    }
}
```

In `updateMeta()`:

```cpp
meta_.oldestArrivalByFamily = oldestArrivalByFamily_;
```

---

## 2. `avgDistanceByFamily`

**Type:** `std::unordered_map<Family, float>`

**Meaning:** mean `|slot.x - port_.x|` across all stored boxes of each family.
Used by the Robot's scoring function to prefer families whose boxes are
clustered near the port (faster to retrieve, fills pallet sooner).

### How to populate

Compute it on the fly inside `updateMeta()` during the existing slot scan.
No extra private state needed — it's a pure aggregation of what is already
iterated.

```cpp
// Inside updateMeta(), in the slot loop alongside countByFamily:
std::unordered_map<Family, int>   distSum;
std::unordered_map<Family, int>   distCount;

for (const auto& slot : slots_) {
    if (slot.isEmpty()) continue;
    const Family& f = slot.peek()->family();
    int dist = std::abs(slot.position().x - port_.x);
    distSum[f]   += dist;
    distCount[f] += 1;
}

meta_.avgDistanceByFamily.clear();
for (const auto& [f, sum] : distSum)
    meta_.avgDistanceByFamily[f] = static_cast<float>(sum) / distCount[f];
```

---

---

## 3. Bug fix — slot double-assignment in `assignNextTo`

**Problem:** `assignNextTo` calls `findFreeSlot()` / `findNearestWithFamily()` for each idle
shuttle, but the free-slot set (`freeSlotXs_`) is only updated when a box is physically
placed or taken (via `notifyBoxPlaced` / `notifyBoxTaken`). When two shuttles are assigned
in the same call to `assignInstructions()`, both see the same "free" slot and are both
directed there. The second shuttle to arrive silently overwrites the first box
(`Slot::place` has no guard), losing one box permanently.

**Fix:** track "claimed" slots separately and exclude them from `findFreeSlot` /
`findNearestWithFamily` until the shuttle completes its mission.

```cpp
// Private member
std::set<int> claimedSlotXs_;  // slots reserved by a shuttle in-flight

// In assignNextTo, after choosing freePos / nearestPos:
claimedSlotXs_.insert(chosenPos.x);

// In notifyBoxPlaced / notifyBoxTaken, release the claim:
claimedSlotXs_.erase(where.x);

// In findFreeSlot: skip claimed slots
//   preferNear=false: iterate rbegin→rend, return first x not in claimedSlotXs_
//   preferNear=true:  iterate begin→end,   return first x not in claimedSlotXs_

// In findNearestWithFamily: skip slots whose x is in claimedSlotXs_
//   (another shuttle is already heading there)
```

---

## Summary of changes to `Aisle.cpp`

| Location | Change |
|---|---|
| Private members | Add `oldestArrivalByFamily_` map; add `claimedSlotXs_` set |
| `notifyBoxPlaced` | Update `oldestArrivalByFamily_`; release claim from `claimedSlotXs_` |
| `notifyBoxTaken` | Recompute `oldestArrivalByFamily_`; release claim from `claimedSlotXs_` |
| `updateMeta()` | Copy `oldestArrivalByFamily_`; compute `avgDistanceByFamily` |
| `assignNextTo` | After selecting a drop/pickup slot, insert into `claimedSlotXs_` |
| `findFreeSlot` | Skip slots in `claimedSlotXs_` |
| `findNearestWithFamily` | Skip slots whose x is in `claimedSlotXs_` |
