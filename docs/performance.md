---
title: Step-loop performance
---

# Single-node step-loop performance

## Reproduce the measurements

From the source root, build the portable Release simulator and its compute-only benchmark, then run the matrix.

```sh
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release -DENABLE_COVERAGE=OFF \
  -DENABLE_OPENMP=OFF -DENABLE_NATIVE_OPTIMIZATIONS=OFF
cmake --build build -j --target climate_sim step_loop_bench
python3 benchmarks/step_loop.py
```

The CSV reports median milliseconds per step and global cell updates per second. The defaults use three fresh MPI jobs per case, a Gaussian hotspot, unit grid spacing, Dirichlet faces, D = 0.05, velocity (0.5, −0.25) and dt = 0.1. The 512², 2048² and 4096² grids run 64, 12 and 8 steps respectively. Set `--repeats`, `--steps`, `--grids`, `--ranks` or `--launcher` to change the experiment. The default launcher uses Open MPI. Passing multiple directories to `--build` rotates their run order on each repetition.

- **Compute** times the production step, including halo exchange, physical boundaries and the buffer swap. The driver synchronizes ranks before starting its clock and reports maximum rank elapsed time.
- **With output** uses the simulator’s `total_max`. It includes the steps, three collective snapshots at step 0, midpoint and end, and closing the file. Both modes exclude process startup, initial conditions and file creation. Progress reporting was disabled. One update is one global interior cell advanced by one step.

## Baseline profile

The baseline was revision `a21818f`. On 2048² with three snapshots, phase timers around the original loop gave the following median percentages of summed rank time across three runs. Halo time includes waiting for other ranks. The output column includes loop timing overhead and file close.

| MPI ranks | Diffusion | Advection | Field copy | Halo | Boundaries | Output |
|---:|---:|---:|---:|---:|---:|---:|
| 1 | 44.4% | 44.7% | 1.1% | <0.1% | <0.1% | 9.8% |
| 4 | 35.8% | 39.0% | 2.4% | 2.6% | <0.1% | 20.3% |
| 12 | 29.2% | 28.5% | 2.4% | 3.8% | <0.1% | 36.1% |

The two stencils repeatedly called checked field accessors per cell and made separate sweeps over the same neighbors. The copy at the start of each step also wrote the entire destination immediately before diffusion overwrote its interior.

## Effects of individual changes

Each comparison below uses three-run medians at 2048². Rates are millions of cell updates per second. The intermediate builds isolate the changes.

| Change | Measurement | Before | After |
|---|---|---:|---:|
| Fuse diffusion and advection into one contiguous x loop, eliminate the full-grid copy on populated tiles, and swap field storage | Portable, 1 rank, compute | 14.6 | 142.7 |
| Combine diffusion and signed upwind transport into four weights on neighbor differences | Portable, 1 rank, compute | 142.7 | 198.5 |
| Describe halo-padded memory directly to collective PnetCDF with a row stride, eliminating the application packing buffer | Portable, 1 rank, three snapshots, first fused kernel | 71.3 | 97.8 |
| Same PnetCDF change | Portable, 12 ranks, three snapshots, first fused kernel | 178.6 | 212.0 |
| Enable host-specific compiler optimization | 1 rank, compute, weighted stencil | 198.5 | 535.3 |

After fusion and mapped writes, output accounted for 37% of portable rank time at one rank and 75% at 12 ranks in the same 2048² profile. This accounts for the smaller speedups with frequent snapshots.

The Cartesian decomposition already uses `MPI_Dims_create`. An experiment computing the interior during nonblocking halo exchange increased 12-rank compute time from 2.91 to 4.94 ms/step at 2048² and from 13.02 to 13.88 ms/step at 4096². The production loop completes the existing face exchange before its single stencil sweep.

## Throughput on the benchmark host

AMD Ryzen 5 8500G, one socket, six cores, 12 hardware threads (`nproc` = 12), WSL2 Linux, GCC 13.3, Open MPI 4.1.6 and PnetCDF 1.12.3. Snapshots used the worktree’s ext4 filesystem with normal filesystem caching. Baseline and portable use the same Release flags (`-O3 -DNDEBUG`). Native additionally enables `-march=native` for the core library. Each build ran at every grid and rank count with the same process placement. Run order rotated across builds to reduce sensitivity to concurrent CPU and filesystem use.

### Compute only

Each entry gives **million cell updates/s** followed by **ms/step** in parentheses.

| Grid | MPI ranks | Baseline | Portable | Native |
|---|---:|---:|---:|---:|
| 512² | 1 | 46.0 (5.696) | 1,012.2 (0.259) | 2,436.7 (0.108) |
| 512² | 2 | 84.4 (3.107) | 1,867.2 (0.140) | 3,978.9 (0.066) |
| 512² | 4 | 122.3 (2.144) | 3,055.2 (0.086) | 7,784.8 (0.034) |
| 512² | 12 | 215.9 (1.214) | 5,145.9 (0.051) | 9,665.4 (0.027) |
| 2048² | 1 | 15.0 (279.328) | 203.0 (20.665) | 550.4 (7.621) |
| 2048² | 2 | 31.1 (135.002) | 367.4 (11.416) | 877.4 (4.780) |
| 2048² | 4 | 54.6 (76.843) | 627.3 (6.687) | 1,323.9 (3.168) |
| 2048² | 12 | 123.8 (33.880) | 760.0 (5.519) | 1,509.5 (2.779) |
| 4096² | 1 | 14.4 (1168.581) | 198.0 (84.737) | 515.0 (32.579) |
| 4096² | 2 | 25.6 (654.835) | 317.1 (52.904) | 833.1 (20.138) |
| 4096² | 4 | 49.2 (341.267) | 403.6 (41.564) | 888.4 (18.884) |
| 4096² | 12 | 90.7 (184.990) | 609.0 (27.549) | 1,289.4 (13.011) |

### With three snapshots

Each entry gives **million cell updates/s** followed by **ms/step** in parentheses.

| Grid | MPI ranks | Baseline | Portable | Native |
|---|---:|---:|---:|---:|
| 512² | 1 | 32.9 (7.966) | 692.7 (0.378) | 1,288.7 (0.203) |
| 512² | 2 | 74.8 (3.506) | 743.9 (0.352) | 955.2 (0.274) |
| 512² | 4 | 122.2 (2.146) | 1,182.3 (0.222) | 1,436.6 (0.182) |
| 512² | 12 | 177.9 (1.474) | 975.7 (0.269) | 1,137.5 (0.230) |
| 2048² | 1 | 13.4 (312.898) | 122.5 (34.231) | 203.6 (20.598) |
| 2048² | 2 | 26.5 (158.459) | 179.5 (23.366) | 240.8 (17.418) |
| 2048² | 4 | 40.0 (104.756) | 153.4 (27.349) | 235.4 (17.816) |
| 2048² | 12 | 83.0 (50.530) | 198.6 (21.121) | 236.4 (17.740) |
| 4096² | 1 | 11.9 (1413.850) | 79.1 (212.100) | 133.9 (125.306) |
| 4096² | 2 | 22.2 (756.530) | 134.6 (124.660) | 161.3 (103.992) |
| 4096² | 4 | 35.3 (474.789) | 163.2 (102.770) | 194.7 (86.186) |
| 4096² | 12 | 52.3 (320.841) | 129.6 (129.409) | 177.9 (94.296) |

## Optional single-node builds

Enable native instructions on the target machine with `-DENABLE_NATIVE_OPTIMIZATIONS=ON`. Enable parallel stencil rows within a rank with `-DENABLE_OPENMP=ON`. Both CMake options default to OFF. This example gives one rank the full node and spreads OpenMP threads across its hardware threads.

```sh
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release \
  -DENABLE_NATIVE_OPTIMIZATIONS=ON -DENABLE_OPENMP=ON -DENABLE_COVERAGE=OFF
cmake --build build -j --target climate_sim step_loop_bench
OMP_NUM_THREADS=$(nproc) OMP_PLACES=threads OMP_PROC_BIND=spread \
  python3 benchmarks/step_loop.py --ranks 1 --repeats 5 --launcher 'mpirun --bind-to none'
```

Five-run medians below isolate OpenMP on one rank. Both native binaries used the same unbound MPI launcher, with their run order alternating between trials. Rates are millions of cell updates per second.

| Grid | 1 thread, compute | 12 threads, compute | 1 thread, three snapshots | 12 threads, three snapshots |
|---|---:|---:|---:|---:|
| 512² | 2,883.5 | 2,344.2 | 1,347.2 | 1,330.4 |
| 2048² | 523.9 | 930.0 | 183.4 | 232.0 |
| 4096² | 454.2 | 1,078.1 | 125.1 | 144.1 |

## Numerical checks

The MPI reference tests compare the distributed stencil after each of six steps against the original scalar kernels on a 13 × 11 global grid. They cover all 81 combinations of the four boundary faces, four velocity sign combinations, pure diffusion and pure advection at 1, 2, 3 and 4 ranks. The absolute tolerance is 1e−13. Nonzero physical ghost values exercise the existing treatment of periodic faces on the nonperiodic Cartesian communicator. A separate case checks that an empty tile relays its previous ghost value.

For a 37 × 29 run with mixed boundaries, all four NetCDF frames agreed with the baseline within 4.45e−16 across 1–4 ranks. The comparison also checked CDF-5 format, dimension order, global attributes and final progress JSON.

Preset grids smaller than the process grid can send ghost cells through empty ranks. All ranks retain the original copy, diffusion, advection and swap sequence in that configuration. On a 1 × 1 grid with 12 ranks, five identical **baseline** runs produced final values between 0.67266 and 0.68211. Cross-run agreement has no unique reference in that case.
