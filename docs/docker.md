# Running with Docker

## Prerequisites

- Docker Engine 24+
- Docker Compose v2 (`docker compose` not `docker-compose`)

## Quick start

From the repo root:

```bash
docker compose up --build
```

| Service  | URL                          |
|----------|------------------------------|
| Frontend | http://localhost:3000        |
| Backend  | http://localhost:8000        |
| API docs | http://localhost:8000/docs   |

## What gets built

### Backend (`backend/Dockerfile`) — multi-stage

**Stage 1 — builder**
- Base: `python:3.12-slim`
- Installs: `cmake`, `g++`, `pybind11`
- Compiles the C++17 simulation engine and generates `scheduler_cpp.cpython-312-x86_64-linux-gnu.so` via pybind11

**Stage 2 — runtime**
- Base: `python:3.12-slim`
- Copies only the `.so` from the builder stage
- Installs Python dependencies (`requirements.txt`)
- Runs: `uvicorn main:app --host 0.0.0.0 --port 8000`

### Frontend (`frontend/Dockerfile`) — multi-stage

- Base: `node:20-slim`
- Builds Next.js in standalone mode (`output: 'standalone'` in `next.config.mjs`)
- Runtime serves via `node server.js` on port 3000

## Environment variables

| Variable              | Default                  | Where set         |
|-----------------------|--------------------------|-------------------|
| `FRONTEND_ORIGIN`     | `http://localhost:3000`  | `docker-compose.yml` (backend CORS) |
| `NEXT_PUBLIC_API_URL` | `http://localhost:8000`  | `docker-compose.yml` build arg (frontend) |

`NEXT_PUBLIC_API_URL` is embedded at **build time** — changing it requires a rebuild (`docker compose up --build`).

## Useful commands

```bash
# Build and start in background
docker compose up --build -d

# View logs
docker compose logs -f backend
docker compose logs -f frontend

# Stop and remove containers
docker compose down

# Rebuild a single service
docker compose up --build backend
```

## Running without Docker

See [architecture.md](architecture.md) for the manual setup (C++ compile + uvicorn + `npm run dev`).
