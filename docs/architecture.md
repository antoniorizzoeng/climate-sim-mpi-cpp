---
title: Architecture
nav_order: 2
---

# Architecture

## From a configuration to a tile

`src/io.cpp` reads YAML and applies CLI overrides. `src/main.cpp` resolves file-backed grid dimensions, constructs the decomposition, initialises the field and drives the time loop. The default execution backend is native C++ and MPI. A Kokkos-enabled build can select device kernels at runtime.

The decomposition uses `MPI_Dims_create` and `MPI_Cart_create`. A plane has one process partition in z. Volumes pin singleton axes and try alternative factorizations for thin grids. Each tile owns a contiguous block, and the last partition on each axis takes the remainder. `MPI_Cart_shift` locates adjacent ranks, including across paired periodic faces.

## From a tile to the next timestep

1. Read the local wind tile if needed. A time-varying wind keeps two records and interpolates velocity at the start of the step.
2. Exchange one-cell halos. Native MPI commits face datatypes once. Physical faces allow disjoint exchanges to proceed together. With periodicity, x arrives before full y rows, then z planes, so corners and edges wrap.
3. Fill physical ghosts. Dirichlet faces take the specified value and Neumann faces copy the adjacent interior.
4. Sweep contiguous x rows, advancing upwind advection and centred diffusion in one pass, then swap buffers. A legacy 2D empty-tile layout retains the separate scalar updates. Kokkos uses packed halo buffers and defaults to staging MPI traffic on the host.

## From tiles to frames

PnetCDF writes each interior into one global CDF-5 variable. Its dimensions are `(time, y, x)` for a plane and `(time, z, y, x)` for a volume. The initial and final states always appear. If configured, rank zero atomically replaces a progress JSON file after each frame and on exit. MPI reductions report the maximum total elapsed time and worst average step time.

[Run and output settings](getting_started.md#run-outputs) · [Numerical method](numerics.md) · [Step-loop measurements](performance.md)
