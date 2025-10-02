---
title: Getting Started
nav_order: 5
---

# Getting Started

This page shows how to build, run, and visualize a small demo locally.

## Prerequisites

- C++17 compiler and CMake ≥ 3.21
- MPI (OpenMPI or MPICH)
- NetCDF C library (`libnetcdf-dev`)
- Python 3.11+ for visualization: `pip install -r requirements.txt`

## Build

```bash
cmake -S . -B build -G Ninja -DMPI_TEST_PROCS=4
cmake --build build --parallel
```

## Run a demo

```bash
mpirun --oversubscribe -np 4 ./build/src/climate_sim       --config=configs/dev.yaml       --output.format=netcdf
```

Suggested **dev.yaml** parameters for a compelling animation (already in repo):

```yaml
grid:    { nx: 512, ny: 512, dx: 1.0, dy: 1.0 }
physics: { D: 0.5, vx: 0.5, vy: 0.0 }
time:    { dt: 0.1, steps: 800, out_every: 10 }
bc:      periodic
output:  { prefix: "dev" }
ic:
  preset: gaussian_hotspot
  params: { A: 1.0, sigma_frac: 0.06, xc_frac: 0.5, yc_frac: 0.5 }
```

### CSV vs NetCDF outputs

- **CSV**: human-readable, one file per rank, useful for debugging.  
- **NetCDF**: structured, one file per snapshot (recommended for production & visualization).  

## Visualize

### Single frame (snapshot)
```bash
python -m visualization.cli show --dir outputs --step 400 --save demo_frame.png --overlay-minmax
```

### Animation
```bash
# MP4 (requires ffmpeg)
python -m visualization.cli animate --dir outputs --fps 24 --save demo.mp4 --overlay-minmax

# GIF (using Pillow)
python -m visualization.cli animate --dir outputs --fps 12 --save demo.gif --writer pillow
```

## Tips

- Use `--cmap plasma` or another Matplotlib colormap for different visuals.  
- Combine overlays (`--overlay-minmax --overlay-rankgrid`) for richer diagnostics.  
- For large runs, prefer NetCDF output for efficiency.  

---

## Install

You can install climate-sim using these commands
```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel
sudo cmake --install build --prefix /usr/local # Or specify a different folder
which climate_sim # Verify installation
```

[← Back to Home](index.md)