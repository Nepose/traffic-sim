# Multi-stage: stage 1 builds the C simulator, stage 2 runs the Python API.
# Build context must be the repo root (docker build -f web/Dockerfile .)

# ---------------------------------------------------------------------------
# Stage 1 - build traffic_sim binary
# ---------------------------------------------------------------------------
FROM ubuntu:24.04 AS builder

RUN apt-get update && apt-get install -y --no-install-recommends \
    cmake gcc make ca-certificates \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /src
COPY src/ src/
COPY include/ include/
COPY CMakeLists.txt .
RUN cmake -S . -B build -DCMAKE_BUILD_TYPE=Release \
 && cmake --build build --parallel

# ---------------------------------------------------------------------------
# Stage 2 - Python API + frontend
# ---------------------------------------------------------------------------
FROM python:3.12-slim

WORKDIR /app

# Copy binary from builder
COPY --from=builder /src/build/traffic_sim /app/traffic_sim_bin

# Install Python dependencies
COPY web/backend/requirements.txt requirements.txt
RUN pip install --no-cache-dir -r requirements.txt

# Copy backend source
COPY web/backend/app/ app/

# Copy frontend (served as static files)
COPY web/frontend/ frontend/

ENV TRAFFIC_SIM=/app/traffic_sim_bin
ENV PYTHONUNBUFFERED=1

EXPOSE 8000

CMD ["uvicorn", "app.main:app", "--host", "0.0.0.0", "--port", "8000"]
