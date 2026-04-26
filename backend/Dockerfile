# ── Stage 1: build C++ pybind11 extension ────────────────────────────────────
FROM python:3.12-slim AS builder

RUN apt-get update && apt-get install -y --no-install-recommends \
        cmake g++ make \
    && rm -rf /var/lib/apt/lists/*

RUN pip install --no-cache-dir pybind11

WORKDIR /app/cpp
COPY cpp/ .

RUN cmake -S . -B build \
        -Dpybind11_DIR="$(python -c 'import pybind11; print(pybind11.get_cmake_dir())')" \
    && cmake --build build --target scheduler_cpp

# ── Stage 2: runtime ──────────────────────────────────────────────────────────
FROM python:3.12-slim

WORKDIR /app

COPY --from=builder /app/cpp/build/scheduler_cpp*.so /app/cpp/build/

COPY python/requirements.txt .
RUN pip install --no-cache-dir -r requirements.txt

COPY python/ .

ENV PYTHONPATH=/app/cpp/build

EXPOSE 8000
CMD ["python", "main.py"]
