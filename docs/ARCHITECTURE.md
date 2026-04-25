# System Architecture

## Overview

The system simulates a box scheduler: boxes arrive as a stream, get grouped internally, and the output is throughput metrics and group assignments. It supports two modes:

- **Batch mode**: run the simulation as fast as possible, return the full result.
- **Visualization mode**: stream simulation events to a React frontend with a controllable virtual clock (speed up / slow down / pause).

Multiple simulations can run concurrently.

---

## Components

```
┌──────────────────────────────────────────────────────────────────┐
│  React Frontend                                                  │
│  - Launch simulations (with parameters)                          │
│  - List / monitor active simulations                             │
│  - Visualize events with a controllable virtual clock            │
└────────────────────┬─────────────────────────────────────────────┘
                     │  REST (launch, status, results)
                     │  WebSocket (event stream, speed control)
┌────────────────────▼─────────────────────────────────────────────┐
│  Python — FastAPI backend                                        │
│  - Manages simulation lifecycle (create, track, cancel)          │
│  - Runs C++ in a thread pool (non-blocking)                      │
│  - Streams event log to frontend at requested playback speed     │
│  - Handles multiple concurrent simulations                       │
└────────────────────┬─────────────────────────────────────────────┘
                     │  pybind11 (called from worker thread)
┌────────────────────▼─────────────────────────────────────────────┐
│  C++ Scheduler                                                   │
│  - Pure logic, no wall-clock dependency                          │
│  - Input: list of boxes with parameters                          │
│  - Output: event log with logical timestamps                     │
│  - Stateless between calls (each simulation is independent)      │
└──────────────────────────────────────────────────────────────────┘
```

---

## How Python runs C++ without blocking

This is the key concurrency question.

FastAPI runs on an `asyncio` event loop. If Python called C++ directly in a coroutine, it would block the entire event loop — no other requests or WebSocket messages would be processed until C++ finished.

**Solution: run C++ in a thread pool.**

```python
import asyncio
from concurrent.futures import ThreadPoolExecutor

thread_pool = ThreadPoolExecutor(max_workers=8)

async def run_simulation(params):
    loop = asyncio.get_event_loop()
    # This awaits in the event loop but C++ runs in a separate thread
    events = await loop.run_in_executor(thread_pool, cpp_scheduler.run, params)
    return events
```

While C++ runs in its thread, the event loop is free to:
- Accept new simulation requests
- Serve WebSocket messages (speed changes, pause/resume)
- Return status queries from the frontend

pybind11 supports releasing the GIL (`py::gil_scoped_release`) inside the C++ call, so multiple C++ simulations can truly run in parallel across threads.

```cpp
// In the pybind11 binding:
m.def("run", [](Params p) {
    py::gil_scoped_release release;  // release GIL, allow parallelism
    return Scheduler(p).run();       // pure C++, runs in parallel
});
```

**Maximum concurrency** is limited by `max_workers` in the thread pool. New simulations beyond that limit queue and wait for a free thread.

---

## Simulation Lifecycle

```
        React                  Python                    C++
          |                      |                        |
          |-- POST /simulations->|                        |
          |   { params }         | creates sim_id         |
          |<- { sim_id } --------|                        |
          |                      |                        |
          |                      |--run_in_executor()---->|
          |                      |  (non-blocking)        | running...
          |                      |                        |
          |-- GET /sim/{id} ---->|                        |
          |<- { status: running}-|                        |
          |                      |                        |
          |-- WS /sim/{id} ----->| (WebSocket connected)  |
          |                      |                        | done
          |                      |<----- event_log[] -----|
          |                      | stores events          |
          |                      | begins streaming       |
          |<--- event stream -----|                        |
          |   at playback speed  |                        |
          |                      |                        |
          |--{ speed: 2.0 }----->| adjusts drip rate      |
          |<--- faster events ---|                        |
```

---

## API Design

### REST — Simulation Management

| Method | Path | Description |
|--------|------|-------------|
| `POST` | `/simulations` | Launch a new simulation |
| `GET` | `/simulations` | List all simulations and their status |
| `GET` | `/simulations/{id}` | Get status and metadata for one simulation |
| `GET` | `/simulations/{id}/result` | Get the full event log (once finished) |
| `DELETE` | `/simulations/{id}` | Cancel a running simulation |

**POST /simulations — request body:**
```json
{
  "num_boxes": 500,
  "arrival_rate": 10.0,
  "group_size": 5,
  "mode": "batch" | "stream"
}
```

**POST /simulations — response:**
```json
{
  "sim_id": "a3f2c1",
  "status": "pending",
  "created_at": "2026-04-25T10:00:00Z"
}
```

**GET /simulations/{id} — response:**
```json
{
  "sim_id": "a3f2c1",
  "status": "running" | "done" | "error" | "pending",
  "progress": 0.73,
  "created_at": "...",
  "finished_at": "..." | null
}
```

---

### WebSocket — Event Streaming

**Endpoint:** `WS /simulations/{id}/stream`

Once connected, Python streams simulation events to the frontend.
The frontend can send control messages at any time.

**Server → Client (events):**
```json
{ "type": "box_arrived",   "logical_time": 0.10, "box_id": 1 }
{ "type": "box_grouped",   "logical_time": 0.35, "box_id": 1, "group_id": 7 }
{ "type": "group_emitted", "logical_time": 0.35, "group_id": 7, "size": 5 }
{ "type": "throughput",    "logical_time": 0.35, "boxes_per_minute": 120 }
{ "type": "done",          "logical_time": 9.80 }
```

**Client → Server (playback control):**
```json
{ "action": "set_speed", "value": 2.0 }   // 2x faster
{ "action": "set_speed", "value": 0.5 }   // half speed
{ "action": "pause" }
{ "action": "resume" }
```

---

## Playback Speed Control

C++ always runs at full speed and produces an event log with **logical timestamps**.
Python is responsible for mapping logical time to real time.

```python
async def stream_events(websocket, events, speed=1.0):
    for i, event in enumerate(events):
        if i == 0:
            await websocket.send_json(event)
            continue

        logical_gap = events[i]["logical_time"] - events[i-1]["logical_time"]
        real_wait   = logical_gap / speed  # divide by speed factor

        await asyncio.sleep(real_wait)     # non-blocking wait
        await websocket.send_json(event)
```

When the frontend sends `{ action: "set_speed", value: 2.0 }`, Python updates `speed` and the next `asyncio.sleep` uses the new value. Since `asyncio.sleep` is non-blocking, speed changes take effect immediately on the next event.

---

## State Management in Python

Python keeps a simulation registry in memory (or a database for persistence):

```python
simulations: dict[str, SimulationRecord] = {}

@dataclass
class SimulationRecord:
    sim_id:     str
    status:     Literal["pending", "running", "done", "error"]
    params:     dict
    events:     list[dict]   # filled when C++ finishes
    created_at: datetime
    finished_at: datetime | None = None
    error:      str | None = None
```

---

## Multiple Simultaneous Simulations

Each simulation is independent:
- Its own thread in the pool
- Its own `SimulationRecord` in the registry
- Its own WebSocket connection(s) — multiple clients can watch the same simulation

The React frontend can list all simulations and open a viewer for any of them simultaneously. Simulations that exceed `max_workers` are queued with status `"pending"` until a thread is free.

---

## C++ Interface Contract

The C++ module exposes a single function. Everything else is internal.

```cpp
struct Box {
    int    id;
    double arrival_time;  // logical time
};

struct Event {
    double      logical_time;
    std::string type;        // "box_arrived", "box_grouped", ...
    int         box_id;
    int         group_id;
    double      throughput;
};

struct Params {
    std::vector<Box> boxes;
    int              group_size;
};

// The only entry point Python needs:
std::vector<Event> run_simulation(Params params);
```

This keeps C++ fully decoupled from networking, threading, and timing concerns.

---

## Technology Summary

| Layer | Technology | Why |
|-------|-----------|-----|
| Frontend | React + Vite | Component model fits simulation state well |
| WebSocket client | native `WebSocket` API | No extra dependency |
| Backend | Python FastAPI | Async, WebSocket support built-in, minimal boilerplate |
| C++ bridge | pybind11 | Clean C++ ↔ Python types, GIL release for parallelism |
| Concurrency | `asyncio` + `ThreadPoolExecutor` | Event loop stays free while C++ runs in threads |
| C++ scheduler | C++17 | Pure logic, fast, no networking code |
