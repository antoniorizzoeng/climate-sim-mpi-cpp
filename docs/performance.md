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

## 3D scaling and 2D compatibility (issue #59)

The portable Release build ran on the Ryzen 5 8500G host described below,
with OpenMP, native optimization and coverage off. The volume cases used
eight steps, unit spacing, `Dz = D = 0.05`, velocity (0.5, −0.25, 0.125)
and `dt = 0.1`. Both volumes contain 16,777,216 cells. Each entry is a
median of three fresh jobs. Output timings include three CDF-5 frames
(384 MiB of field data per run) and file close.

| Grid | Ranks | Compute ms/step | Compute M updates/s | With output ms/step | With output M updates/s |
|---|---:|---:|---:|---:|---:|
| 256³ | 1 | 129.70 | 129.4 | 219.92 | 76.3 |
| 256³ | 2 | 99.88 | 168.0 | 213.14 | 78.7 |
| 256³ | 4 | 50.77 | 330.4 | 138.07 | 121.5 |
| 256³ | 12 | 27.09 | 619.4 | 162.98 | 102.9 |
| 512 × 512 × 64 | 1 | 132.85 | 126.3 | 220.65 | 76.0 |
| 512 × 512 × 64 | 2 | 69.64 | 240.9 | 175.11 | 95.8 |
| 512 × 512 × 64 | 4 | 39.54 | 424.3 | 128.52 | 130.5 |
| 512 × 512 × 64 | 12 | 34.84 | 481.6 | 127.26 | 131.8 |

The 2D comparison used the parent revision `412efad` and the 3D-capable
engine. Each entry is the median of five runs, in ms/step. The harness rotated
the order of the two builds on every repetition. Step counts and output
cadence match the original 2D benchmark above.

| Grid | Ranks | Compute before | Compute after | With output before | With output after |
|---|---:|---:|---:|---:|---:|
| 512² | 1 | 0.2456 | 0.2505 | 0.3497 | 0.3505 |
| 512² | 2 | 0.1272 | 0.1262 | 0.2935 | 0.2832 |
| 512² | 4 | 0.0798 | 0.0801 | 0.2039 | 0.2099 |
| 512² | 12 | 0.0504 | 0.0506 | 0.2727 | 0.2719 |
| 2048² | 1 | 20.7462 | 20.6566 | 34.2536 | 33.8920 |
| 2048² | 2 | 11.3872 | 11.4124 | 23.6049 | 23.6957 |
| 2048² | 4 | 6.8095 | 6.7635 | 20.5270 | 18.3649 |
| 2048² | 12 | 4.7497 | 4.8116 | 20.7855 | 19.5261 |
| 4096² | 1 | 85.9385 | 85.0837 | 164.0188 | 163.0725 |
| 4096² | 2 | 45.9989 | 45.8055 | 114.7634 | 122.7186 |
| 4096² | 4 | 27.1115 | 27.2997 | 138.8638 | 132.1362 |
| 4096² | 12 | 19.8867 | 20.0114 | 139.4050 | 95.0649 |

Across the twelve grid/rank combinations, the geometric mean of the after/before
time ratios was 1.002 for compute and 0.954 with output. Compute medians
ranged from 1.0% faster to 2.0% slower. The largest compute increase, at 512²
on one rank, changed direction in a nine-run repeat with reversed starting order
(0.2518 ms before, 0.2497 ms after). The largest output increase was 6.9% at
4096² on two ranks. A nine-run repeat measured 130.56 ms before and 132.41 ms
after (+1.4%). Filesystem caching and shared-host timing limit the precision
of the output comparison.

```sh
python3 benchmarks/step_loop.py --build build/release --grids 256x256x256 512x512x64 --repeats 3
python3 benchmarks/step_loop.py --build build/baseline build/release --grids 512 2048 4096 --repeats 5
```

The 3D kernel sweeps contiguous x rows and parallelizes the z/y row pairs
when OpenMP is enabled. The compiler generates the five-point single-layer
loop from the same stencil, omitting z weights and z loads. Each rank commits
its face datatypes once. With physical boundaries, all six disjoint receive
faces can be in flight together. With periodic boundaries, x arrives before
full y rows and then full z planes so edges and corners wrap correctly.

## Uniform-wind regression check for periodic boundaries and file wind

The comparison used the portable Release binaries from `f14724b` and the
updated engine on the Ryzen 5 8500G host. Each entry is the median of nine
compute-only runs of 48 steps with Dirichlet boundaries, default process
placement, OpenMP off, and coverage off. The driver alternated build order.
Small timing differences on this shared host can vary between runs.

| Grid | MPI ranks | Before (ms/step) | After (ms/step) |
|---|---:|---:|---:|
| 512² | 1 | 0.2479 | 0.2452 |
| 512² | 4 | 0.0801 | 0.0781 |
| 2048² | 1 | 20.683 | 20.759 |
| 2048² | 4 | 6.770 | 6.732 |

At 2048² on one rank, the final median increased by 0.37%. An earlier
nine-run comparison of the same uniform kernel measured 20.651 ms before
and 20.623 ms after. The timing difference changed sign between runs.

```sh
python3 benchmarks/step_loop.py --build build/baseline build --grids 2048 512 \
  --ranks 4 1 --steps 48 --repeats 9 --modes compute
```

Uniform wind retains its precomputed stencil weights. The non-periodic exchange
posts the four faces together, and the kernel walks the two field buffers by
row stride. Periodic exchange completes the x faces before sending full y rows
so the corner ghosts wrap as well. File wind interpolates the two local velocity
components and applies upwind advection and diffusion in one stencil pass.

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

For issue #59, four updates on an 11 × 9 × 7 volume agreed with an independent
NumPy implementation of the directional finite differences at 1, 2, 3, 4 and
8 ranks (absolute and relative tolerances of 2e−15). A z-only diffusion case
on a 1 × 1 × 17 grid checks the exact discrete Fourier-mode decay. Separate
tests check wrapped edge and corner ghosts, surplus ranks, 3D file-wind
interpolation, and the steady-wind result against uniform wind. The reference
and file-wind cases also passed with two OpenMP threads per rank.

On the portable build, the complete 2D snapshot and progress files matched
`412efad` byte for byte in 13 comparisons. These covered mixed, periodic and
Neumann faces on 37 × 29 at 1–4 ranks, plus a 1 × 5 Neumann run on two ranks
that exercises the legacy empty-tile ghost exchange.

The MPI reference tests compare the distributed stencil after each of six steps against the original scalar kernels on a 13 × 11 global grid. They cover all 25 valid combinations of the four boundary faces, four velocity sign combinations, pure diffusion and pure advection at 1, 2, 3 and 4 ranks. The absolute tolerance is 1e−13. The scalar reference fills periodic ghosts from the opposite global edges. Additional checks cover diagonal ghosts, exact one-cell translations across both seams and periodic mass conservation. A separate case checks that an empty tile relays its previous ghost value.

For a 37 × 29 run with mixed boundaries, all four NetCDF frames agreed with the baseline within 4.45e−16 across 1–4 ranks. The comparison also checked CDF-5 format, dimension order, global attributes and final progress JSON.

Preset grids smaller than the process grid can send ghost cells through empty ranks. All ranks retain the original copy, diffusion, advection and swap sequence in that configuration. On a 1 × 1 grid with 12 ranks, five identical **baseline** runs produced final values between 0.67266 and 0.68211. Cross-run agreement has no unique reference in that case.
