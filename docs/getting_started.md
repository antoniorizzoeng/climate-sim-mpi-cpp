---
layout: default
---

# Getting Started

This page shows how to build, run, and visualize a small demo locally.

## Prerequisites

- C++17 compiler and CMake ≥ 3.21
- MPI (OpenMPI or MPICH)
- (Optional) NetCDF C library (`libnetcdf-dev`) if you want NetCDF inputs/outputs
- Python 3.11+ for visualization: `pip install -r requirements.txt`

## Build

```bash
cmake -S . -B build -G Ninja -DMPI_TEST_PROCS=4
cmake --build build --parallel
```

## Run a pretty demo

```bash
mpirun --oversubscribe -np 4 ./build/src/climate_sim \
  --config=configs/dev.yaml \
  --output.format=netcdf
```

Suggested **dev.yaml** parameters for a compelling animation (already in repo):

```yaml
grid:    { nx: 512, ny: 512, dx: 1.0, dy: 1.0 }
physics: { D: 0.5, vx: 0.5, vy: 0.0 }
time:    { dt: 0.1, steps: 800, out_every: 10 }
bc:      periodic
output:  { prefix: "dev", format: "netcdf" }
ic:
  preset: gaussian_hotspot
  params: { A: 1.0, sigma_frac: 0.06, xc_frac: 0.5, yc_frac: 0.5 }
```

## Visualize

Single frame:

```bash
python -m visualization.cli frame outputs --fmt nc --step 400 --save demo_frame.png
```

Animation (MP4 with ffmpeg available, else GIF with Pillow):

```bash
# MP4
python -m visualization.cli animate outputs --fmt nc --fps 24 --save demo.mp4
# GIF
python -m visualization.cli animate outputs --fmt nc --fps 12 --save demo.gif --writer pillow
```

Live watch while the simulation is running:

```bash
python -m visualization.cli watch outputs --fmt nc --interval 0.5
```

---

[← Back to Home](index.md)
