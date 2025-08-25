---
title: Architecture
nav_order: 2
---

# Architecture

## Overview
Deterministic 2D climate toy model (temperature) using finite differences for diffusion and advection, parallelized with MPI via 2D domain decomposition and halo exchanges.

## Modules
- `src/main.cpp` — entrypoint; parse config, initialize MPI, wire components, run loop.
- `include/config.hpp` — CLI/config parsing (nx, ny, dt, steps, coeffs, output freq).
- `include/decomp.hpp` — Cartesian 2D process grid, neighbors, local sizes/offsets.
- `include/field.hpp` — 2D scalar field with halos; contiguous storage + indexing.
- `include/diffusion.hpp` — 5-point stencil (explicit) diffusion.
- `include/advection.hpp` — 1st-order upwind advection (constant vx, vy).
- `include/boundary.hpp` — physical boundary conditions.
- `include/io.hpp` — snapshots (CSV initially), reductions for stats, timing/logs.

> **Single Source of Truth**: Public interfaces live in `include/*.hpp`. This document is descriptive only. See headers for authoritative signatures.

## Domain Decomposition
- Use `MPI_Dims_create(world_size, 2, dims)` → near-square Px×Py.
- `MPI_Cart_create` with `periods={0,0}` (can switch to periodic later).
- Block distribution in x,y. Last ranks in each dimension take remainders.
- Neighbors via `MPI_Cart_shift`.

## Halo Exchange
- Halo width `h = 1` initially.
- Nonblocking pattern per step: post four `MPI_Irecv`, post four `MPI_Isend`, then `MPI_Waitall`.
- Derived datatypes for columns via `MPI_Type_vector`; rows are contiguous.
- Physical boundaries: if neighbor is `MPI_PROC_NULL`, apply BC locally (Dirichlet/Neumann).

## Numerical Kernels
See `include/diffusion.hpp` and `include/advection.hpp` for function signatures.
- **Diffusion (explicit 5-point)**: stable if `alpha = D*dt/dx^2` (with `dx==dy`) satisfies `alpha ≤ 1/4`.
- **Advection (upwind)**: CFL with `C_x + C_y ≤ 1`.

## Configuration (CLI)
Example flags:
```
--nx 512 --ny 512 --steps 1000 --dt 1e-3 --dx 1.0 --dy 1.0 --diff 0.1 --vx 0.0 --vy 0.0 --out-every 100 --bc dirichlet
```

## Initialization
- Each rank allocates `Field` with local sizes + halos.
- Initial condition (global-consistent), e.g., Gaussian hotspot centered at domain middle.
- Prebuild MPI datatypes for halos (if not handled internally).

## Output
- Per-rank CSV tiles: `snap_<step>_r<rank>.csv` (simple and parallel).
- Global stats: min/max/mean via reductions every `out_every` steps.
- Later: move to parallel NetCDF if needed.

## Main Loop Pseudocode
```
for n in 0..steps-1:
  exchange_halos(u)
  apply_boundary(u)
  compute_diffusion(u, tmp)
  compute_advection(u, tmp)
  swap(u, tmp)
  if (n % out_every == 0): write_snapshot(u,n); report_stats(u,n)
```

## Testing Targets
- Indexing/layout: `Field` bounds and halo handling.
- Decomposition: local sizes/offsets sum to global; neighbor ranks correct.
- Halo MPI datatypes: shapes/strides correct (toy sizes).
- Kernel sanity: diffusion single step vs expected stencil.
- Determinism: repeatable outputs with fixed inputs, ranks, and build.
