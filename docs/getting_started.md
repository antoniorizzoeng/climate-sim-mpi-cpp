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

For a demo that wraps in both directions, use these settings in your YAML file.

```yaml
grid:    { nx: 512, ny: 512, dx: 1.0, dy: 1.0 }
physics: { D: 0.5, vx: 0.5, vy: 0.0 }
time:    { dt: 0.1, steps: 800, out_every: 10 }
bc:      periodic
output:  { prefix: "dev" }
ic:
  preset: gaussian_hotspot
  A: 1.0
  sigma_frac: 0.06
  xc_frac: 0.5
  yc_frac: 0.5
```

## Periodic boundaries

`bc: periodic` (CLI `--bc=periodic`) joins the left edge to the right and the
bottom edge to the top. A signal crossing an edge re-enters at the opposite
edge, including when one MPI rank owns the whole axis. To wrap x only, use

```yaml
bc: {left: periodic, right: periodic, bottom: neumann, top: neumann}
```

Each periodic face needs a periodic partner on the same axis. You can set
individual faces with `--bc.left`, `--bc.right`, `--bc.bottom`, and `--bc.top`.

## Wind

Uniform wind is the default. Set its components with `physics.vx` and
`physics.vy` or `--vx` and `--vy`. For a wind field that varies in space and
time, generate a CDF-5 example and run it with the same grid and record interval.

```bash
python scripts/generate_wind.py --nx 64 --ny 48 --records 3 --interval 2 --outfile inputs/winds.nc
mpirun --oversubscribe -np 4 ./build/src/climate_sim --nx=64 --ny=48 --bc=periodic \
  --wind.mode=file --wind.path=inputs/winds.nc --wind.interval=2 --dt=0.2 --steps=40
```

The equivalent wind configuration is

```yaml
wind:
  mode: file
  path: inputs/winds.nc
  vx_var: vx
  vy_var: vy
  interval: 2
```

On a 2D grid both component variables use the same dimensions, either `(y, x)` for
steady wind or `(time, y, x)` for a series. The named spatial dimensions must
match the simulation grid, including a grid inferred from the initial condition.
Supply finite numeric, cell-centred velocities in grid-length units per second
in a CDF-1, CDF-2, or CDF-5 file. Custom names use `--wind.vx_var` and
`--wind.vy_var`. Convert packed data to physical velocities before use.

Record zero applies at time zero. The engine linearly interpolates at the start
of each step using `wind.interval` seconds between records (default 3600).
This setting supplies the spacing even when the file has a time coordinate.
After the last record, its velocities continue to apply. Each rank reads only
its own tile and keeps at most two wind records in memory.

File wind requires a timestep at or below
`1 / (max|vx|/dx + max|vy|/dy + 2 D (1/dx² + 1/dy²))`.
At startup the engine scans every wind record and reduces both component maxima
across MPI ranks. An excessive timestep stops the run before its first snapshot.
A zero denominator gives an unlimited timestep. Uniform wind retains its
existing timestep clamping. File-wind snapshots record `wind_mode`, `wind_path`,
`wind_vx_var`, `wind_vy_var`, and `wind_interval` as global attributes.

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

The `u` variable stores 2D frames in `(time, y, x)` order and volumes in `(time, z, y, x)` order. Frame 0 is the initial
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
For a 2D run the variable defaults to `u` and contains a float or double field in `(y, x)` order. Omitted `nx` and `ny` values come from the file. Explicit
values must match it. Snapshot 0 in `outputs/snapshots.nc` contains the input field.

PnetCDF supports CDF-1, CDF-2, and CDF-5. Convert NetCDF-4/HDF5 inputs with
`nccopy -k cdf5 input.nc output.nc`.

## 3D simulation

Save the following configuration as `volume.yaml`. It transports a Gaussian
hotspot horizontally and vertically, wraps at the horizontal edges, and applies
zero normal gradient at the front (`z = 0`) and back (`z = nz - 1`) faces.

```yaml
grid: {nx: 64, ny: 48, nz: 24, dx: 1.0, dy: 1.0, dz: 0.5}
physics: {D: 0.05, Dz: 0.01, vx: 0.5, vy: -0.25, vz: 0.125}
time: {dt: 0.1, steps: 40, out_every: 10}
bc: {left: periodic, right: periodic, bottom: periodic, top: periodic,
     front: neumann, back: neumann}
ic: {preset: gaussian_hotspot, sigma_frac: 0.12, zc_frac: 0.4}
output: {path: runs/volume/snapshots.nc, progress: runs/volume/progress.json}
```

```bash
mpirun --oversubscribe -np 4 ./build/src/climate_sim --config=volume.yaml
```

`nz` and `dz` default to 1, `vz` defaults to zero, and `Dz` defaults to the
effective horizontal diffusivity `D`, including CLI overrides. Use `--nz`,
`--dz`, `--vz`, `--Dz`, `--bc.front`, and `--bc.back` to override the
corresponding values. Periodic front and back faces must be paired.
Uniform 3D wind clamps the timestep to

```text
1 / (|vx|/dx + |vy|/dy + |vz|/dz + 2*(D/dx² + D/dy² + Dz/dz²))
```

File wind uses the maximum absolute value of each component across all cells
and records and rejects a timestep above the combined limit. A zero denominator
allows any finite timestep. Single-layer runs use the original 2D stability
rules and snapshot metadata. Volumes pin each singleton axis to one MPI
partition. Single-layer runs retain the legacy horizontal rank layout, including
empty tiles when a horizontal extent is smaller than the process grid.

To run the same grid with generated initial conditions and time-varying winds:

```bash
python scripts/generate_ic.py --nx 64 --ny 48 --nz 24 --dz 0.5 --zc-frac 0.4 --sigma-frac 0.12 --outfile inputs/start3d.nc
python scripts/generate_wind.py --nx 64 --ny 48 --nz 24 --records 3 --interval 2 --outfile inputs/wind3d.nc
mpirun --oversubscribe -np 4 ./build/src/climate_sim --config=volume.yaml \
  --ic.mode=file --ic.path=inputs/start3d.nc --wind.mode=file \
  --wind.path=inputs/wind3d.nc --wind.interval=2 --wind.vz_var=vz
```

A 3D initial condition uses a float or double variable with dimensions
`(z, y, x)`. The engine infers omitted grid extents from the variable and
checks explicit extents. To start every layer from the same 2D field, specify
the desired `nz` and set `ic.replicate_z: true` or `--ic.replicate_z=true`.

File-wind components use `(z, y, x)` for a steady wind and `(time, z, y, x)`
for time records. Set `wind.vz_var` to read vertical velocities. Omitting it
gives zero vertical wind. The generator writes all three components for
`--nz` greater than one.

Snapshots include the layer count (`nz`), spacing (`dz`), vertical diffusivity
(`Dz`), and the two z boundary conditions. To read a horizontal slice using
the existing Python dependencies:

```python
from netCDF4 import Dataset

with Dataset("runs/volume/snapshots.nc") as nc:
    middle = len(nc.dimensions["z"]) // 2
    final_slice = nc["u"][-1, middle, :, :]
```

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