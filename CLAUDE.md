# CLAUDE.md

## Project

Automated warehouse simulation. Boxes arrive on an input belt, get stored in aisles (silos) by shuttles, and are later retrieved and packed onto pallets by a robotic arm.

Full stack:
- **C++17** — pure simulation logic (scheduler, aisles, shuttles, robots, pallets)
- **Python / FastAPI** — REST + WebSocket backend; runs C++ via pybind11 in a thread pool
- **React + Vite** — frontend for launching simulations and visualizing events

## Repository layout

```
HackUPC-2026/
├── backend/
│   ├── cpp/                  # C++17 simulation engine
│   │   ├── CMakeLists.txt
│   │   ├── include/          # one header per class
│   │   ├── src/              # implementations + main.cpp
│   │   ├── data/             # sample CSV inputs
│   │   └── bindings/         # pybind11 bindings (bindings.cpp)
│   │
│   └── python/               # FastAPI backend
│       ├── main.py           # app entrypoint
│       ├── routers/          # simulations.py, ws.py
│       ├── models.py         # SimulationRecord, Params, etc.
│       ├── scheduler.py      # thin wrapper around the compiled C++ .so
│       └── requirements.txt
│
├── frontend/                 # React + Vite
│   └── src/
│
├── docs/                     # reference docs — do not modify
│   ├── challenge-description.md
│   ├── domain-definition.md
│   ├── system-definition.md
│   └── architecture.md       # design documentation — you should edit this as you implement    
```

## Build — C++ engine

```bash
cd backend/cpp
/usr/bin/cmake -S . -B build
/usr/bin/cmake --build build
```

This produces two binaries:
- `./backend/cpp/build/silos` — interactive demo
- `./backend/cpp/build/bench` — benchmark runner

Run the demo:
```bash
./backend/cpp/build/silos
```

Run the benchmark and print a comparison table:
```bash
cd backend/cpp
./build/bench | python3 benchmark/compare.py
```

Save results to a file:
```bash
./build/bench > results.json
python3 benchmark/compare.py results.json
```

Requires C++17: GCC 8+, Clang 7+, or MSVC 2017+.

## Tests — C++ engine

Run all suites (build + run):
```bash
./backend/cpp/run_tests.sh
```

Run a single suite:
```bash
./backend/cpp/run_tests.sh test_robot
```

Skip rebuild (just run):
```bash
./backend/cpp/run_tests.sh -n
./backend/cpp/run_tests.sh -n test_robot
```

Test suites (unit first, then integration):
- `test_aisle` — shuttle output selection (`findBestBoxForShuttle`)
- `test_aisle_heuristic` — input placement heuristic
- `test_metadata_update` — `countByFamily` / `avgDistanceByFamily`
- `test_robot` — pallet management and dispatch
- `test_integration` — end-to-end across all three pipeline phases
- `test_integration_retrieval` — A-in-front-of-B retrieval scenario

Shared test harness lives in `backend/cpp/tests/helpers.h` (`SUITE`, `SECTION`, `RUN` macros, `makeBox`).

## Build — Python backend

```bash
cd backend/python
pip install -r requirements.txt
```

Run (the `.so` must be on `PYTHONPATH`):
```bash
cd backend/python
PYTHONPATH=/home/ignacio/Escritorio/SILOS/HackUPC-2026/backend/cpp/build uvicorn main:app --reload
```

The pybind11 extension must be compiled first (see `backend/cpp/bindings/`). Only `scheduler.py` imports the `.so`; everything else in Python is pure Python.

## C++ conventions

- PascalCase for classes; camelCase for methods; private members with trailing `_`
- One class per file: header in `include/`, implementation in `src/`
- `Pallet::CAPACITY = 12` — adjust if the problem statement specifies it
- `Pallet` is a dumb data structure; all dispatch/reservation logic lives in `Robot`

## Python conventions

- Async FastAPI; never block the event loop — C++ runs in a `ThreadPoolExecutor`
- `scheduler.py` is the only file that touches the pybind11 `.so`
- pybind11 bindings must release the GIL (`py::gil_scoped_release`) so multiple simulations run truly in parallel

## Agent instructions

- Read existing files before writing code.
- If a file's content is needed and you haven't read it, ask — do not assume.
- For ambiguous tasks, ask before implementing.
- Prefer editing over full rewrites.
- Do not re-read a file unless it may have changed since you last read it.
- Do not run builds after every code change — the user runs them manually.
- Do not create commits, PRs, or any GitHub actions. When the user needs a git command, provide the exact command.
- Keep output concise but reasoning thorough.
- No flattering openers or filler closings.
- Keep solutions simple and direct.
- User instructions always override this file.
- All generated code and documentation must be in English. Also every part of the code, frontend or backend must be in English.

## Documentation

When you implement something, document how it works in `docs/architecture.md` (system-wide design), or in `docs/backend.md` / `docs/frontend.md` if the scope is layer-specific. Technical details belong there, not in this file.

## Protected files — do not modify

- `docs/challenge-description.md`
- `docs/domain-definition.md`
- `docs/system-definition.md`
