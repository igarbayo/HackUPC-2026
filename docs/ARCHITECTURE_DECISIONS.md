# Algorithms and Technical Decisions

Four decision points drive throughput: instruction ordering inside an aisle, input slot selection, box retrieval selection, and robot pallet management.

---

## How the simulation tick works

The entire simulation advances in discrete ticks driven by `Scheduler::activate()`. Each tick executes three stages in order:

```
Scheduler::activate()
 ├── 1. Belt feed      — pop boxes whose arrivalTick ≤ currentTick, enqueue into the aisle
 ├── 2. Robot tick     — collect ready outputs, decide which families to request, fire requestOutput()
 └── 3. Aisle tick     — reorder instructions, advance shuttles, assign new missions
```

## Instruction ordering — SSTF with aging

Inspired by HDD disk scheduling, where the read head minimises seek time by always serving the closest pending sector, each aisle sorts its instruction queue every tick using an analogous cost function.

**Priority formula:**

```
priority = wait_time
         + stock_bonus
         - (min_seek × SEEK_WEIGHT) / aisle_length
```

| Term          | Meaning                                                                                    |
| ------------- | ------------------------------------------------------------------------------------------ |
| `wait_time`   | `currentTick - issuedAt` — aging, prevents starvation                                      |
| `stock_bonus` | +2 if output instruction and family count > 5 (drain deep stock faster)                    |
| `min_seek`    | minimum estimated travel cost to execute this instruction across all free shuttles         |
| `SEEK_WEIGHT` | 3 — seek penalty weight; normalised by aisle length so short aisles are not over-penalised |

**Seek cost per instruction type:**

- _Input_: `|shuttle.x − port.x|` (pickup is always at the port)
- _Output_: `|shuttle.x − nearest_box.x| + |nearest_box.x − port.x|`

If no shuttle is free, the seek term is skipped and the instruction waits purely on aging. Instructions are stable-sorted descending by priority so earlier arrivals win ties.

---

## Pull scheduling model

The classic push model calls `assignInstructions()` at the start of each tick, meaning a shuttle that finishes in tick N sits idle until tick N+1. The pull model eliminates this latency: when a shuttle completes a mission it immediately calls `aisle.assignNextTo(shuttle)`, fetching its next instruction in the same tick. `assignInstructions()` still runs at the end of `Aisle::tick()` to cover any shuttle that went idle without a completion callback.

---

## Input heuristic — `findBestInputSlot`

Scores every free z1 slot at the shuttle's row y and returns the minimum-cost position.

**Cost formula:**

```
cost = W1 × |x − xIdeal|
     + W2 × penZ
     + W3 × |x − nextPickX|
     + W4 × shuttleLoad × 10
```

| Term                   | Value                                                              | Meaning                                                                                                                          |
| ---------------------- | ------------------------------------------------------------------ | -------------------------------------------------------------------------------------------------------------------------------- |
| `W1`, `W2`, `W3`, `W4` | 1, 1, 1, 10                                                        | Weights (tuned by genetic algorithm — see below)                                                                                 |
| `xIdeal`               | `(1 − completeness) × (length − 1)`                                | Target X shifts toward the far end when the family is sparse; shifts toward x=0 as stock grows, reducing future retrieval travel |
| `penZ`                 | +1000 empty slot · −1000 same-family z2 · +500 different-family z2 | Strongly prefer stacking on a same-family z2 box (colocation bonus); penalise wasting a clean slot or mixing families            |
| `nextPickX`            | x of nearest pending output pick                                   | Place close to where the next retrieval will happen                                                                              |
| `shuttleLoad`          | 0 idle · 1 busy                                                    | Heavy penalty to avoid contention on an already-occupied shuttle                                                                 |

If no free z1 slot exists at row y for the front-of-queue input instruction, that instruction is skipped and the next one is tried. This prevents a single blocked level from stalling the entire input queue.

**Weight selection:** W1–W4 were chosen by running a genetic algorithm over a fixed benchmark suite, maximising pallet throughput.

---

## Output (retrieval) heuristic — `findBestBoxForShuttle`

For a given family f and shuttle position, selects the box that minimises total shuttle travel.

**Cost:**

```
cost = |shuttle.x − box.x| + |box.x − port.x|
```

Tiebreak order:

1. Lower cost wins.
2. If costs equal, prefer a slot where z1 is occupied and z2 also exists — picking z1 frees the blocked z2 box.
3. If still tied, prefer larger x (deeper in the aisle, reserves closer slots for future inserts).

Only boxes at the shuttle's own y level are considered; each shuttle operates on a single row.

---

## Robot heuristic — `StockProximityHeuristic`

When the robot needs to open a new pallet slot it scores every eligible family and picks the highest scorer.

**Score:**

```
score(f) = available(f) / (avgDist(f) + 1)
```

| Term           | Meaning                                                                                      |
| -------------- | -------------------------------------------------------------------------------------------- |
| `available(f)` | `countByFamily[f] − reservedByFamily[f]` — boxes in the aisle that are not already in-flight |
| `avgDist(f)`   | mean <code>&#124;x − port.x&#124;</code> over all boxes of family f — proxy for shuttle retrieval cost |

A family with many nearby boxes scores high; a family with few or distant boxes scores low. A new pallet is only opened if `available ≥ OPEN_THRESHOLD` (= `CAPACITY / 2` = 6). If no family clears the threshold (end-of-batch drain), the threshold is bypassed and the best-scoring family wins anyway.

**Pallet eviction:** if all four pallet slots are occupied and an unrouted box arrives, the slot with the lowest `placed + inAisle` score is evicted — the pallet least likely to ever fill up.

---

## Language choices

The simulation engine is written in C++17. The choice was deliberate: we wanted to step outside our comfort zone while keeping the hot path fast. The benchmark confirms sub-millisecond tick times even for large aisle configurations.

The REST API is Python with FastAPI. It acts as intermediary between the C++ engine (loaded as a pybind11 `.so`), the KPI computation layer, and the frontend. The C++ runs in a `ThreadPoolExecutor` with the GIL released (`py::gil_scoped_release`) so multiple concurrent simulations do not block each other.

The frontend is React + Vite. It consumes a WebSocket stream of simulation events and renders the live state of the aisle, shuttles, and pallets.

---

## Design patterns

**Composition** — `Aisle` holds `Shuttle` objects rather than inheriting from them. `Robot` holds `Pallet` slots. Capabilities are assembled, not inherited.

**Producer-consumer pipeline** — `InputBelt` → `Aisle` → `Robot` form a three-stage pipeline. Each stage produces outputs consumed by the next, decoupled through well-defined interfaces (`AisleContainer`, `collectReadyOutput`). The belt drives arrival timing; the robot drives retrieval demand.

---

## Digital twin — live aisle visualisation

The frontend maintains a digital twin of every aisle: a pixel-accurate mirror of the C++ state updated in real time via the WebSocket event stream. Each box insertion, retrieval, and pallet placement is reflected immediately in the UI. This lets operators observe shuttle movements, slot occupancy, and pallet filling as they happen, without polling or replaying logs.

---

## Robot heuristic experiments

Several robot dispatch heuristics were implemented and benchmarked:

- **FIFO** — serve requests in arrival order; simple baseline.
- **StockDepth** — prefer the family with the most boxes in the aisle.
- **StockProximityHeuristic** (selected) — `score = available / (avgDist + 1)`; balances stock depth against shuttle travel cost. Consistently outperformed the others on pallet throughput across the benchmark suite.

The experiment showed that proximity weight is critical: ignoring travel distance causes the robot to open pallets for deep-aisle families whose retrieval time stalls pallet completion.

---

## Theoretical lower bound on pallet fill time

Shuttle retrieval time for a single box follows:

```
t = 10 + D
```

where D is the Manhattan distance from the shuttle to the box plus the box to the port. The minimum possible D is 0 (box already at the port), giving `t_min = 10 s` per box.

A pallet holds 12 boxes (CAPACITY = 12). As shuttles move independently and we can have 4 aisles (8 shuttles per aisle, 8×4=32), we can obtain up to 32 boxes at the same time. So the theoretical lower bound to fill one pallet is given by the time to retrieve 1 box and place it on the shuttle port:

```
t_lower = 10 + 10 = 20 s
```

Real fill times exceed this because D > 0 for most boxes and the robot serialises retrievals. The benchmark uses this bound as a sanity check: any run completing a pallet in under 20 s indicates a logic error.

---

## Box arrival simulator — normal distribution

The input belt simulator generates box arrivals whose inter-arrival times follow a normal distribution N(μ, σ²). Parameters μ and σ are configurable at simulation launch. This models realistic warehouse intake bursts (high variance during shift changes, low variance during steady-state operation) and stress-tests the scheduler under both sparse and dense arrival patterns.

---

## Testing

The entire backend is covered by automated test scripts: C++ unit and integration suites run via `run_tests.sh`, and the Python API layer is exercised end-to-end with pytest, covering simulation lifecycle, WebSocket streaming, and KPI computation against known inputs. Both suites are designed to run headlessly in CI with a single command.

---

## Future work — sustainability and energy metrics

One area we did not have time to tackle is sustainability observability: modelling the energy consumed per simulation run (shuttle motor cycles, robot arm movements, lighting and HVAC estimates) and surfacing kilowatt-hour figures alongside throughput KPIs. This would let operators compare scheduling strategies not only by speed but by energy cost, which is increasingly relevant for large-scale warehouse automation.