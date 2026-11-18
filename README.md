# Climate-Sim-MPI-CPP
[![CI](https://github.com/antoniorizzoeng/climate-sim-mpi-cpp/actions/workflows/ci.yml/badge.svg)](../../actions/workflows/ci.yml)
[![CodeQL](https://github.com/antoniorizzoeng/climate-sim-mpi-cpp/actions/workflows/codeql.yml/badge.svg)](../../actions/workflows/codeql.yml)
[![clang-format](https://github.com/antoniorizzoeng/climate-sim-mpi-cpp/actions/workflows/format.yml/badge.svg)](https://github.com/antoniorizzoeng/climate-sim-mpi-cpp/actions/workflows/format.yml)
[![Deploy Docs](https://github.com/antoniorizzoeng/climate-sim-mpi-cpp/actions/workflows/docs.yml/badge.svg)](https://github.com/antoniorizzoeng/climate-sim-mpi-cpp/actions/workflows/docs.yml)

MPI advection–diffusion simulation on 2D and 3D grids, with Python tools for plotting 2D snapshots.

<p align="center">
  <img src="docs/demo.gif" alt="Simulation demo (GIF)" width="45%"/>
</p>

## Features

### Simulation (C++/MPI)
- Cartesian MPI decomposition with one-cell halos and contiguous x rows.
- Fused advection and diffusion with uniform or file-based wind and an optional vertical diffusivity.
- Boundary conditions:
  - Periodic on any axis, with opposite faces paired
  - Dirichlet
  - Neumann
- Initial conditions:
  - Constant zero
  - Gaussian hotspot
  - NetCDF fields read in parallel with PnetCDF (CDF-1, CDF-2, CDF-5)
- Configurable via YAML and CLI overrides.
- Timing statistics logged at end of run.

### Visualization (Python)
- Supported format: NetCDF.
- Plot utilities:
  - Single field plots with colorbars.
  - Field comparison with optional difference panel.
  - Animation of time steps (MP4 / GIF).
- CLI entrypoint for manual visualization tasks.

---

## Requirements

### C++ (simulation)
- C++17 compiler
- CMake ≥ 3.16
- MPI implementation (OpenMPI, MPICH, etc.)
- NetCDF-C library
- PnetCDF-C library

## Build & Run

```bash
cmake -S . -B build -G Ninja
cmake --build build -j
```

Run simulation (example, 4 MPI ranks):

```bash
mpirun --oversubscribe -np 4 ./build/src/climate_sim --config=configs/dev.yaml
```

For a volume, set `--nz` to the layer count and `--dz` to the layer spacing.
The CDF-5 snapshot variable is `u(time, z, y, x)` for a volume and
`u(time, y, x)` for a single layer (the default). [3D example configuration](docs/getting_started.md#3d-simulation)

Start from a NetCDF field with `--ic.mode=file --ic.path=inputs/ic_global.nc`.
The variable defaults to `u` and can be set with `--ic.var`. Omit `nx` or `ny`
to use the corresponding file dimension. See the [file initial conditions guide](docs/getting_started.md#file-initial-conditions).

Use `--output=runs/demo/snapshots.nc` to give a run its own snapshot file and
`--progress=runs/demo/progress.json` to track its status. The default snapshot
file is `outputs/snapshots.nc`. Each file includes the initial and final states.
See [run outputs](docs/getting_started.md#run-outputs) for the YAML settings and
frame timing.

### Wind

Uniform wind uses `physics.vx`, `physics.vy`, and, in 3D, `physics.vz` (CLI `--vx`, `--vy`, `--vz`).
Use `--wind.mode=file --wind.path=inputs/winds.nc` for cell-centred velocities.
The engine reads each rank’s tile from a classic NetCDF file, interpolates between
wind records, and holds the final record. Set `bc: periodic` to carry the field
across the periodic faces. The x, y and z face pairs are left/right, bottom/top and front/back. [Wind file example and timestep limit](docs/getting_started.md#wind)

### Python (visualization)
Install dependencies with:

```bash
pip install -r requirements.txt
```

---

## Visualization Usage

### CLI

```bash
# Rebuild and save single frame
python -m visualization.cli show --dir outputs --step 10 --save frame.png

# Animate all steps into a GIF
python -m visualization.cli animate --dir outputs --save anim.gif --writer pillow
```
---

## Tests

### C++ tests (gtest)
Run via CTest:

```bash
cd build
ctest --output-on-failure
```

### Python tests (pytest)
Run visualization tests:

```bash
pytest tests/visualization -v
```

---

## Installation

After building with CMake, you can install `climate_sim` system-wide:

```bash
cmake --install build --prefix /usr/local
```

This installs:

- `climate_sim` into `/usr/local/bin`
- CMake package config files into `/usr/local/lib/cmake/climate-sim-mpi-cpp`
- Headers and libraries for bundled dependencies (e.g. GoogleTest, if enabled)

> **Note:** `sudo` may be required if you do not have write permission for the prefix.

### Local Installation (Non-Root)

To install into a custom location (e.g. `$HOME/.local`):

```bash
cmake --install build --prefix $HOME/.local
```

Make sure the binary directory is on your `PATH`:

```bash
export PATH="$HOME/.local/bin:$PATH"
```

Now you can run the simulator from anywhere:

```bash
climate_sim --config configs/dev.yaml
```

---

## Uninstallation

CMake does not generate an uninstall target by default. You can remove the installed files manually.

If you installed to `/usr/local`:

```bash
sudo rm -f /usr/local/bin/climate_sim
sudo rm -rf /usr/local/lib/cmake/climate-sim-mpi-cpp
```

If you installed locally to `$HOME/.local`:

```bash
rm -f $HOME/.local/bin/climate_sim
rm -rf $HOME/.local/lib/cmake/climate-sim-mpi-cpp
```

Alternatively, consider using package managers (e.g. building a `.deb` or `.rpm`) for more automated installation/uninstallation.

---

## Verification

After installation, verify the binary is available:

```bash
which climate_sim
```

## License

This project is licensed under the **GNU Affero General Public License v3.0 (AGPLv3)**.
- Strong copyleft: any derivative or network service use must publish source under AGPL.
- Compatible with most GPL code.
- Discourages proprietary/commercial adoption.

See [LICENSE](LICENSE) for full text.
Visit https://antoniorizzoeng.github.io/climate-sim-mpi-cpp/ for more info.
