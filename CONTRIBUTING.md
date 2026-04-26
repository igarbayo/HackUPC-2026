# Contributing to XEITECH

Thank you so much for taking the time to read this. It genuinely means a lot.
XEITECH is a small project born at HackUPC, and the fact that you are here
considering contributing to it is something we do not take for granted.

Because this started as a hackathon project maintained by students, **review
and response times are not fixed**. We will do our best to acknowledge pull
requests within a week, but academic commitments can cause delays of several
weeks or more. Please be patient — we value every contribution.

---

## Table of contents

1. [Setting up the development environment](#1-setting-up-the-development-environment)
2. [Coding standards](#2-coding-standards)
3. [Running the tests](#3-running-the-tests)
4. [Commit conventions](#4-commit-conventions)
5. [Pull request process](#5-pull-request-process)
6. [Definition of done](#6-definition-of-done)
7. [Review expectations](#7-review-expectations)

---

## 1. Setting up the development environment

### Prerequisites

- Git
- C++17 compiler: GCC 8+, Clang 7+, or MSVC 2017+
- CMake 3.14+
- Python 3.10+
- Node.js 18+
- Docker 24+ and Docker Compose v2 (optional, but simplest)

### Fork and clone

```bash
# Fork the repository on GitHub, then:
git clone https://github.com/<your-username>/HackUPC-2026.git
cd HackUPC-2026
git remote add upstream https://github.com/igarbayo/HackUPC-2026.git
```

### Build the C++ engine

```bash
cd backend/cpp
cmake -S . -B build
cmake --build build
```

Run the demo to verify:

```bash
./build/silos
```

### Install Python dependencies

```bash
cd backend/python
pip install -r requirements.txt
```

### Run the backend

```bash
cd backend/python
PYTHONPATH=/absolute/path/to/HackUPC-2026/backend/cpp/build \
    uvicorn main:app --reload
```

### Install and run the frontend

```bash
cd frontend
npm install
NEXT_PUBLIC_API_URL=http://localhost:8000 npm run dev
```

### Docker Compose shortcut

If you only need to verify the integration layer and do not intend to modify C++:

```bash
docker compose up
```

This builds and starts both containers. Rebuild after code changes:

```bash
docker compose build && docker compose up
```

---

## 2. Coding standards

### C++ (`backend/cpp/`)

- **Style:** Allman (opening brace on its own line).
- **Indentation:** 4 spaces (no tabs).
- **Naming:** PascalCase for classes and structs; camelCase for methods and
  local variables; trailing underscore for private member variables
  (e.g. `capacity_`, `boxes_`).
- **Files:** one class per file; header in `include/`, implementation in `src/`.
- **Headers:** use `#pragma once`; include only what the file directly uses.
- **No raw owning pointers:** prefer value types, `std::unique_ptr`, or
  `std::shared_ptr` where heap allocation is necessary.

Example:

```cpp
class Shuttle
{
public:
    explicit Shuttle(int id);
    bool isAvailable() const;

private:
    int id_;
    bool busy_;
};
```

### Python (`backend/python/`)

- **Style:** PEP 8; 4-space indentation.
- **Async:** never block the event loop — if you call anything slow, use
  `loop.run_in_executor`.
- **Typing:** add type annotations to all public function signatures.
- Only `scheduler.py` may import the pybind11 `.so`; everything else stays
  pure Python.

### TypeScript / React (`frontend/`)

- **Style:** Prettier defaults (2-space indent, double quotes, trailing commas).
- Prefer functional components with hooks; no class components.
- Keep API calls in `lib/` and out of component files.

---

## 3. Running the tests

### C++ tests

```bash
# Build and run all suites
./backend/cpp/run_tests.sh

# Skip rebuild and run all
./backend/cpp/run_tests.sh -n

# Run a single suite
./backend/cpp/run_tests.sh test_robot
```

Available suites: `test_aisle`, `test_aisle_heuristic`, `test_metadata_update`,
`test_robot`, `test_integration`, `test_integration_retrieval`.

### Python tests

```bash
cd backend/python
PYTHONPATH=/absolute/path/to/backend/cpp/build pytest
```

---

## 4. Commit conventions

We use **Conventional Commits**. Each commit message follows this structure:

```
<type>(<optional scope>): <description>

<optional body>

<optional footer>
```

### Types

| Type | When to use |
|---|---|
| `feat` | Add or remove a feature visible in the API or UI |
| `fix` | Fix a bug in a feature |
| `refactor` | Rewrite code without changing observable behaviour |
| `perf` | Improve performance (subset of `refactor`) |
| `style` | Formatting, whitespace — no behaviour change |
| `test` | Add or correct tests |
| `docs` | Documentation only |
| `build` | Build system, dependencies, project version |
| `ops` | CI/CD, deployment, infrastructure |
| `chore` | Everything else (`.gitignore`, initial commit, etc.) |

### Rules

- **Description:** imperative present tense, lowercase first letter, no trailing
  period. Think "This commit will `<description>`."
- **Scope:** optional; use it when the change is clearly contained in one area
  (e.g. `feat(robot): add flush on timeout`).
- **Breaking changes:** add `!` before the colon (`feat(api)!: remove /status endpoint`)
  and a `BREAKING CHANGE:` footer with details.
- **Body/footer:** include motivation and context. Reference issues with
  `Closes #123` in the footer.

### Examples

```
feat(shuttle): add priority queue for retrieval order

Boxes headed to the robot now skip the FIFO queue and are served
in priority order based on distance from the arm.
```

```
fix: prevent pallet dispatch with zero boxes

The robot was occasionally flushing an empty pallet when a
simulation ended mid-fill. Added a guard in Robot::flush().
```

```
docs: add troubleshooting section to README
```

### Versioning from commits

- Breaking change (`!`) → bump **major** version.
- `feat` or `fix` → bump **minor** version.
- Anything else → bump **patch** version.

---

## 5. Pull request process

1. Create a **descriptive branch** from `develop`:
   - `feature/<short-name>` for new functionality
   - `fix/<short-name>` for bug fixes
   - `docs/<short-name>` for documentation
2. Make your changes and commit them following the conventions above.
3. Update `CHANGELOG.md` under `[Unreleased]` with a human-readable entry
   explaining *why* the change was made.
4. Push your branch and open a pull request against `develop` (not `main`).
5. Fill in the pull request template completely.
6. At least **one maintainer** must approve before merging.
7. We squash-merge to keep the `develop` history linear.

Direct commits to `main` or `develop` are not allowed.

---

## 6. Definition of done

A contribution is considered complete when all of the following are true:

- The C++ build succeeds without warnings: `cmake --build build`
- All C++ test suites pass: `./backend/cpp/run_tests.sh`
- All Python tests pass: `pytest`
- `CHANGELOG.md` has been updated under `[Unreleased]`
- New source files include an SPDX license header:
  ```cpp
  // SPDX-License-Identifier: MIT
  // SPDX-FileCopyrightText: 2026 XEITECH Team
  ```
- The pull request description explains the motivation for the change
- At least one maintainer has approved the pull request

---

## 7. Review expectations

Maintainers will:

- Acknowledge your pull request with a comment within **one week** when
  possible (academic schedules may cause delays).
- Provide actionable feedback, not just approval or rejection.
- Respect your time — if a change is almost there, we will suggest the
  minimum edits needed rather than asking for a full rewrite.

We ask contributors to:

- Be patient — this is a small project maintained in spare time.
- Respond to review comments within a reasonable time; after 30 days of
  inactivity we may close the pull request with a note that it is welcome
  to be reopened.
- Keep the scope of each pull request focused — one logical change per PR
  makes reviews faster and history cleaner.
