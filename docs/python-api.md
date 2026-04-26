# Python Backend API

Base URL: `http://localhost:8000`

All HTTP responses are JSON. All timestamps are ISO 8601 UTC.

---

## Overview

The backend is a FastAPI app that wraps the C++ simulation engine via pybind11.
It uses a producer-consumer pipeline so the frontend can start receiving ticks
before the simulation finishes.

```
POST /simulations          →  launch simulation (async)
GET  /simulations          →  list all simulations
GET  /simulations/{id}     →  status of one simulation
GET  /simulations/{id}/result  →  full result once done
WS   /simulations/{id}/stream  →  tick-by-tick state stream (live)
```

---

## Data types

### Position
```json
{ "x": 3, "y": 1, "z": 1, "side": 1 }
```
- `x` — slot index (`-1` = port/head, `0..num_slots-1` = storage)
- `y` — height level (1-based)
- `z` — depth (`1` = front, `2` = back)
- `side` — rack side (`1` = left, `2` = right)

### Box
```json
{ "id": "B42", "family": "ZARA", "arrival_tick": 15, "position": <Position|null> }
```
`position` is set for floor boxes (aisle storage), `null` for pallet boxes.

### Shuttle
```json
{
  "y_level": 2,
  "position": <Position>,
  "phase": "MovingToDrop",
  "is_carrying": true,
  "carried_box_id": "B42",
  "carried_box_family": "ZARA",
  "floor_boxes": [<Box>, ...]
}
```
- `phase` values: `"Idle"` | `"MovingToPickup"` | `"Loading"` | `"MovingToDrop"` | `"Unloading"`
- `floor_boxes` — all boxes stored at this shuttle's y-level (both sides, both depths)
- `carried_box_id` / `carried_box_family` — empty strings when `is_carrying` is false

### Pallet
```json
{
  "slot": 0,
  "family": "ZARA",
  "placed_count": 5,
  "reserved_count": 2,
  "boxes": [<Box>, ...]
}
```
- `slot` — robot pallet slot index (0–3)
- `placed_count` — boxes physically on the pallet
- `reserved_count` — slots promised but not yet delivered
- `boxes` — only boxes already placed (length == `placed_count`)

### TickState
```json
{
  "tick": 42,
  "aisle": {
    "shuttles": [<Shuttle>, ...]
  },
  "pallets": [<Pallet>, ...]
}
```
One `TickState` per simulation tick. `pallets` only contains active pallets (up to 4).

### Event
```json
{ "type": "box_stored", "logical_time": 17, "box_id": "B42", "pallet_id": -1, "family": "ZARA" }
```
| `type`              | `box_id` | `pallet_id` |
|---------------------|----------|-------------|
| `box_arrived`       | ✓        | -1          |
| `box_stored`        | ✓        | -1          |
| `box_retrieved`     | ✓        | -1          |
| `box_on_pallet`     | ✓        | slot 0–3    |
| `pallet_dispatched` | —        | slot 0–3    |
| `done`              | —        | -1          |

---

## REST Endpoints

---

### `POST /simulations`

Launch a new simulation. Returns immediately; the C++ engine runs in a thread pool.

**Request body** (`SimulationParams`):
```json
{
  "num_slots": 20,
  "num_y": 2,
  "num_sides": 1,
  "max_ticks": 10000,
  "boxes": null,
  "generator": null
}
```

Provide boxes via **one of**:

`boxes` — explicit list:
```json
"boxes": [
  { "id": "B1", "family": "ZARA",    "arrival_tick": 0  },
  { "id": "B2", "family": "BERSHKA", "arrival_tick": 10 }
]
```

`generator` — let C++ generate them:
```json
"generator": {
  "num_boxes": 100,
  "num_destinations": 5,
  "weights": { "ZARA": 0.6, "BERSHKA": 0.4 },
  "seed": 42,
  "mean_inter_arrival_ticks": 10.0,
  "std_inter_arrival_ticks": 3.0,
  "demand_profile": [
    { "from_tick": 0, "to_tick": 500, "rate_multiplier": 1.0 }
  ]
}
```

If both are omitted, the simulation runs with no boxes (useful for testing the idle lifecycle).

**Response `202 Accepted`:**
```json
{ "sim_id": "f47ac10b-58cc-4372-a567-0e02b2c3d479", "status": "running" }
```

---

### `GET /simulations`

List all simulations. Status reflects the live state derived from `store.done`.

**Response `200 OK`:** array of `SimulationRecord`
```json
[
  {
    "sim_id": "f47ac10b-...",
    "status": "done",
    "created_at": "2026-04-25T10:00:00Z",
    "finished_at": "2026-04-25T10:00:03Z",
    "params": { "num_slots": 20, "num_y": 2, ... }
  }
]
```
`status` values: `"running"` | `"done"` | `"error"`

---

### `GET /simulations/{sim_id}`

Get the record for one simulation.

**Response `200 OK`:** `SimulationRecord` (same shape as above)

**Response `404`:**
```json
{ "detail": "Simulation not found" }
```

---

### `GET /simulations/{sim_id}/result`

Fetch the full result once the simulation is done.

**Response `200 OK`:**
```json
{
  "events": [<Event>, ...],
  "snapshots": [<TickState>, ...]
}
```
`events` — complete log of what happened (all ticks, all boxes).
`snapshots` — one `TickState` per tick, in chronological order.

**Response `202`** — simulation still running:
```json
{ "detail": "Simulation still running" }
```

**Response `404`** — sim_id not found.

**Response `500`** — simulation crashed.

---

## WebSocket

### `WS /simulations/{sim_id}/stream`

Stream per-tick state to the frontend. The server sends tick frames as soon as
they arrive in the store — it does **not** wait for the simulation to finish.
A `done` frame is sent once the last tick has been delivered.

**Server → client messages:**

Tick frame (one per tick):
```json
{
  "tick": 42,
  "aisle": { "shuttles": [...] },
  "pallets": [...]
}
```

Done frame (last message):
```json
{ "type": "done", "total_ticks": 387 }
```

Error frame (sim not found or failed):
```json
{ "type": "error", "detail": "Simulation not found" }
```

**Client → server messages (playback control):**

```json
{ "action": "set_speed", "value": 10.0 }
```
Set playback speed in ticks per second (default: `1.0`).

```json
{ "action": "pause" }
```
Pause streaming (server keeps its position).

```json
{ "action": "resume" }
```
Resume streaming from where it paused.

---

## Concurrency model

`POST /simulations` launches two threads per simulation:

```
ThreadPoolExecutor
  hilo A: run_simulation_streaming(params, queue)
            C++ pushes one TickSnapshot per tick into SnapshotQueue
            GIL released for the entire C++ run
            returns the full event log when done

  hilo B: drain loop
            while True:
                snap = queue.pop()   # blocks until item or done+empty
                if snap is None: break
                store.snapshots[sim_id].append(convert(snap))
            store.done[sim_id] = True
```

The asyncio WS handler runs independently on the event loop:

```
WS handler (asyncio)
  idx = 0
  while True:
      if idx < len(store.snapshots[sim_id]):
          send(store.snapshots[sim_id][idx]); idx++
          await asyncio.sleep(1/speed)
      elif store.done[sim_id]:
          break
      else:
          await asyncio.sleep(0.01)   # wait for next tick
```

The three actors run at their own pace. `SnapshotQueue` buffers ticks when
C++ produces faster than the drain thread consumes; `asyncio.sleep(0.01)` buffers
when the drain thread is faster than the frontend.

State is kept in four module-level dicts in `store.py`:
- `simulations` — `SimulationRecord` objects (status, params, timestamps)
- `snapshots` — `list[TickStateModel]` per sim, grows tick by tick as hilo-B drains the queue
- `events` — `list[EventModel]` per sim, populated atomically when hilo-A finishes
- `done` — `bool` per sim, set to `True` by hilo-B after the queue is fully drained

Snapshot and event conversion (C++ objects → Pydantic models) happens inside
the thread pool in `scheduler.py`, keeping the asyncio event loop free.
