# Climate-Sim-MPI-CPP
[![CI](https://github.com/antoniorizzoeng/climate-sim-mpi-cpp/actions/workflows/ci.yml/badge.svg)](../../actions/workflows/ci.yml)
[![CodeQL](https://github.com/antoniorizzoeng/climate-sim-mpi-cpp/actions/workflows/codeql.yml/badge.svg)](../../actions/workflows/codeql.yml)
[![clang-format](https://github.com/antoniorizzoeng/climate-sim-mpi-cpp/actions/workflows/format.yml/badge.svg)](https://github.com/antoniorizzoeng/climate-sim-mpi-cpp/actions/workflows/format.yml)
[![pages-build-deployment](https://github.com/antoniorizzoeng/climate-sim-mpi-cpp/actions/workflows/pages/pages-build-deployment/badge.svg)](https://github.com/antoniorizzoeng/climate-sim-mpi-cpp/actions/workflows/pages/pages-build-deployment)
A parallel climate simulation framework in C++ using MPI for domain decomposition and distributed computation.

## Features

- **Domain Decomposition**: 2D Cartesian decomposition for distributing simulation grids across MPI processes.
- **Configurable Simulation**: YAML-based configuration for grid size, timestep, physics parameters, and boundary conditions.
- **Physics Modules**:
  - Diffusion
  - Advection
  - Boundary condition handling (periodic, fixed, etc.)
- **Performance Analysis**:
  - Strong and weak scaling benchmarks
  - Speedup, efficiency, and Karp–Flatt metric calculation
- **I/O Support**:
  - Rank layout export (`CSV`)
  - Snapshot output for field reconstruction
- **Testing**: Unit and MPI integration tests with GoogleTest.

## Build Instructions

```bash
cmake -S . -B build -DBUILD_TESTING=ON
cmake --build build --parallel
```

## Running the Simulation

```bash
mpirun --oversubscribe -np 4 ./build/src/climate_sim --config=configs/dev.yaml
```

## Running Tests

```bash
ctest --test-dir build --output-on-failure
```

## Benchmarks

Run strong/weak scaling benchmarks:

```bash
./scripts/run_benchmark.sh
```

## License

This project is licensed under the MIT License.
