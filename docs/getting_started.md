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
mpirun --oversubscribe -np 4 ./build/src/climate_sim       --config=configs/dev.yaml
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

## Run outputs

Snapshots are stored in one NetCDF file per run, at `outputs/snapshots.nc` by
default. Set a separate path for each concurrent run. The simulator creates parent
directories automatically.

```bash
mpirun --oversubscribe -np 4 ./build/src/climate_sim --config=configs/dev.yaml \
  --output=runs/demo/snapshots.nc --progress=runs/demo/progress.json
```

The equivalent YAML keys are `output.path` and `output.progress`. CLI values
override the YAML settings. Relative paths resolve from the working directory.
The existing `output.prefix` setting is stored in the `output_prefix` metadata.

The `u` variable stores frames in `(time, y, x)` order. Frame 0 is the initial
field. The remaining frames are the states after every `out_every` steps and
after the last step, which appears exactly once. To recover the simulation step
for zero-based frame index `i`, use `min(i * out_every, steps)`. Multiply it by
`dt` for the simulation time. The global attributes `out_every`, `steps`, and
`dt` store those values, including the effective timestep after stability clamping.

With a progress path configured, rank 0 atomically replaces the JSON file at
startup, after each frame, and at exit. The fields are `step` (completed steps),
`steps` (planned steps), `time` (simulation time), `frames` (snapshots written),
`status` (`running`, `done`, or `error`), and `message` (the error text or an
empty string). A failed run exits with a non-zero status.

## File initial conditions

Generate a CDF-5 input with the Python dependencies installed.

```bash
python scripts/generate_ic.py --nx 64 --ny 48 --outfile inputs/start.nc
```

Save this configuration as `file-ic.yaml`.

```yaml
ic:
  mode: file
  path: inputs/start.nc
  var: u
```

```bash
mpirun --oversubscribe -np 4 ./build/src/climate_sim --config=file-ic.yaml
```

You can also set these options with `--ic.mode=file`, `--ic.path`, and `--ic.var`.
The variable defaults to `u` and must be a two-dimensional float or double field
in `(y, x)` order. Omitted `nx` and `ny` values come from the file. Explicit
values must match it. Snapshot 0 in `outputs/snapshots.nc` contains the input field.

PnetCDF supports CDF-1, CDF-2, and CDF-5. Convert NetCDF-4/HDF5 inputs with
`nccopy -k cdf5 input.nc output.nc`.

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