# Changelog

All notable changes to this project will be documented in this file.
The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/).

Entries are written in plain prose and explain **why** a change was made,
not just which files were touched. Every significant change deserves a sentence
of motivation.

---

## [Unreleased]

### Added

- **C++ simulation engine** (`backend/cpp/`): a pure-logic, wall-clock-free
  scheduler that models the full warehouse pipeline — boxes arrive on an input
  belt, are placed in numbered aisles by autonomous shuttles, and are later
  retrieved and packed onto pallets by a robotic arm. The engine produces a
  timestamped event log that can be replayed at any speed by the frontend.
  Built with C++17 and compiled as a shared library via pybind11 so that the
  Python backend can call it without spawning a subprocess.

- **FastAPI backend** (`backend/python/`): a thin async layer that owns the
  simulation lifecycle (create, track, cancel, stream). It runs the C++ engine
  in a thread pool so the event loop stays free for WebSocket messages and
  concurrent requests. Exposes a REST API for launching simulations and a
  WebSocket endpoint for real-time event streaming. Chosen because its async
  model maps naturally onto the two-phase workflow: run fast in C++, then
  stream slowly to the browser.

- **React frontend** (`frontend/`): a Next.js application that lets users
  configure and launch simulations, monitor their status, and watch the
  warehouse operate in a 3D view (Three.js). The controllable virtual clock
  (speed up / pause / rewind) was the primary reason for separating simulation
  time from wall time in the engine.

- **Docker Compose** (`docker-compose.yml`): a single-command deployment that
  wires the backend and frontend containers together. Added so that evaluators
  at HackUPC 2026 could run the full stack without installing a C++ toolchain
  or Node.js locally.

- **Test suites** (`backend/cpp/tests/`, `backend/python/tests/`): unit tests
  for the C++ aisle logic, shuttle heuristic, metadata updates, robot dispatch,
  and two integration scenarios; and pytest suites for the Python API layer.
  Tests were added incrementally as each component stabilised, not as an
  afterthought.

- **Open-source governance layer**: LICENSE (MIT), CONTRIBUTING guide,
  SECURITY policy, CODE\_OF\_CONDUCT, GOVERNANCE rules, DCO, and GitHub issue
  and PR templates. Added at the end of the hackathon to make the repository
  usable and trustworthy beyond the competition context.

---

*Versions will be tagged on `main` following semantic versioning once the
project moves beyond the hackathon prototype stage.*
