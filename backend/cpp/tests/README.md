# Robot test coverage

## Tests

| Test | What it checks |
|---|---|
| `test_basic_placement` | Delivering one box opens a pallet for that family and places the box on it. |
| `test_pallet_auto_dispatch_on_full` | Delivering exactly `CAPACITY` boxes to the same family auto-dispatches the pallet and frees the slot. Verifies the `pallet_dispatched` and `box_on_pallet` event log entries. |
| `test_multi_family_pallets` | Boxes from four different families open four separate pallet slots simultaneously. |
| `test_force_dispatch_least_full` | When all 4 slots are occupied and a new family arrives, the pallet with the fewest placed boxes is force-evicted to make room. |
| `test_same_family_same_pallet` | Boxes of the same family always accumulate on a single pallet rather than being scattered across slots. |
| `test_tick_integration_with_aisle` | Full pipeline: `OPEN_THRESHOLD` boxes enter the aisle, shuttles store them, the robot calls `tick()` each step and eventually places a box on a pallet. Verifies the event ordering (`box_stored` precedes `box_on_pallet`). |
| `test_threshold_bypass_when_aisle_low` | When fewer than `OPEN_THRESHOLD` boxes are available for a family, the robot still opens a pallet (threshold bypass for low-stock scenarios). |
| `test_drain_dispatch_on_empty_aisle` | When the aisle reaches a fully empty state (no stored boxes, no pending inputs, no active shuttles, no ready outputs), all open pallets are dispatched immediately. |
| `test_bulk_request_pipelining` | Two cases: (1) 5 boxes available with plenty of free slots — `decide()` requests all 5 in one call; (2) pallet nearly full with 1 free slot and 5 available — request is capped at 1. Verifies bulk-requesting rather than drip-feeding one box per tick. |
| `test_threshold_blocks_high_score_low_count_family` | With one free slot, family X has fewer boxes than `OPEN_THRESHOLD` but a better score (near port); family Y has more boxes than `OPEN_THRESHOLD` but a worse score (far away). Verifies the robot opens a pallet only for Y — the threshold filter runs before scoring. |
| `test_stats_mixed_fill_rate` | Dispatches one full pallet (A, `CAPACITY` boxes) and one half-full pallet (B, `CAPACITY/2` boxes dispatched manually). Verifies `totalPalletsSent=2`, `filledPalletsSent=1`, `totalBoxesSent=CAPACITY+half`, and `avgFillRate` equals `(CAPACITY+half)/(2×CAPACITY)` within float tolerance. |
| `test_stats_full_pallet` | After a full pallet is dispatched: `totalPalletsSent=1`, `filledPalletsSent=1`, `totalBoxesSent=CAPACITY`, `boxesSentForFamily` correct, unknown family returns 0, `avgFillRate=1.0`. |
| `test_stats_partial_pallet_fill_rate` | Force-evicting a partial pallet (3 boxes): `filledPalletsSent=0`, `avgFillRate` is strictly between 0 and 1. |
| `test_stats_boxes_by_family` | Two full pallets (families A and B): per-family counters in both `boxesSentForFamily()` and `boxesSentByFamily()` match `CAPACITY`, aggregate counters are consistent, `avgFillRate=1.0`. |
| `test_score_prefers_nearby_family` | With one free slot, two families both pass `OPEN_THRESHOLD`: X has more boxes but is far (`avgDist=10`), Y has fewer boxes but is near (`avgDist=1`). Verifies Y wins the slot because `score = available / (avgDist + 1)` is higher for Y. |
| `test_eviction_uses_aisle_metadata` | All 4 slots occupied (A=1 box, B=2, C=3, D=3). 5 A-boxes are stored in a real aisle; `robot.tick()` populates `lastMeta_`. When E arrives and forces eviction, A's combined score (1+5=6) saves it and B (2+0=2) is evicted instead — proving the `available_in_aisle` component is used. |
