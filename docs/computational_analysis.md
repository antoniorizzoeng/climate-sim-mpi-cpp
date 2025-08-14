---
layout: default
---
{% include mathjax.html %}

# Computational Analysis

This document derives **compute**, **communication**, **memory**, and **I/O** costs for the main loop in this project and ties them to expected **strong/weak scaling** behavior and benchmark methodology.

## Assumptions

- Global grid: `Nx × Ny`, uniform spacings `Δx, Δy`.
- MPI 2D Cartesian decomposition: `Px × Py` ranks, `p = Px · Py`.
- Per-rank interior tile: `nx × ny` where `nx ≈ Nx/Px`, `ny ≈ Ny/Py` (last ranks may get remainders).
- Halo width: `h = 1` (5-point diffusion, 1st-order upwind advection).
- Data type: 8-byte double.
- One field updated per step (`u`), with a ping-pong buffer (`tmp`).

## Main Loop (reference)

```
for n in 0..steps-1:
  exchange_halos(u)
  apply_boundary(u)
  compute_diffusion(u, tmp)
  compute_advection(u, tmp)
  swap(u, tmp)
  if (n % out_every == 0): write_snapshot(u,n); report_stats(u,n)
```

## Per-step Complexity (per rank)

### 1) Halo exchange

Faces exchanged once per step (nonblocking). For `h = 1`:

- Left/right faces: `2 × (ny · h)` doubles
- Down/up faces: `2 × (nx · h)` doubles

```math
V_{\mathrm{halo}}(nx, ny) = 8 \left( 2\,ny + 2\,nx \right) \ \text{bytes} = 16 (nx+ny) \ \text{bytes}
```

Latency cost: 4 messages per step (L/R/D/U).  
Bandwidth cost: proportional to `nx+ny` bytes.

### 2) Boundary conditions

Applied only at physical boundaries. Per-rank cost is `O(nx + ny)` (copy/mirror ghost rings). Interior ranks pay ~`O(1)` if neighbors exist.

### 3) Diffusion (5-point stencil, explicit)

Each interior point touches constant neighbors:

```math
W_{\mathrm{diff}}(nx, ny) = \Theta(nx \cdot ny)
```

Arithmetic intensity (naive): ~(a few flops) per 5 loads + 1 store → memory bound unless tiled.

### 4) Advection (1st-order upwind)

Constant work per point (two 1D upwind derivatives):

```math
W_{\mathrm{adv}}(nx, ny) = \Theta(nx \cdot ny)
```

Similar memory behavior to diffusion (more loads if branching on velocity sign).

### 5) Swap

Pointer swap is O(1). If implemented as copy, it would be `O(nx·ny)` (we avoid that).

### 6) Output & stats (when triggered)

- `write_snapshot`: I/O bound; writing the full tile ⇒ `O(nx·ny)` data per rank (CSV is larger on disk).
- `report_stats`: local reduction `O(nx·ny)` plus a global `MPI_Allreduce` → `O(log p)` latency and tiny payload.

## Global (per-step) Costs

Sum over all ranks:

- **Compute:** `Θ(Nx · Ny)` for diffusion + `Θ(Nx · Ny)` for advection → `Θ(Nx · Ny)` overall.
- **Communication:** total halo volume across ranks is ~ perimeter of all tiles. For a balanced grid:
  - Each interior edge counted twice; asymptotically, global halo volume per step is
    ```math
    V_{\mathrm{halo, global}} \approx 16 \left( Px \cdot Ny + Py \cdot Nx \right)\ \text{bytes}
    ```
  - Latency: `4p` messages per step (but overlappable in practice).

## Strong vs Weak Scaling Expectations

### Strong scaling (fixed `Nx × Ny`, increase `p`)

- Compute per rank: `Θ((Nx·Ny)/p)` → ideal time `∝ 1/p`.
- Comm per rank: `Θ(nx + ny)` with `nx ≈ Nx/Px`, `ny ≈ Ny/Py`. If `Px ≈ Py ≈ √p`, then
  ```math
  nx \approx \frac{Nx}{\sqrt{p}},\quad ny \approx \frac{Ny}{\sqrt{p}} \Rightarrow V_{\mathrm{halo}} \propto \frac{Nx + Ny}{\sqrt{p}}
  ```
- Efficiency drops when comm + latency + sync dominate compute. Expect a knee where local tiles become too small.
- Benchmark scripts will generate also Karp-Flatt metric as well as speedup and efficiency

### Weak scaling (fixed `nx × ny` per rank, increase `p`)

- Compute per rank: ~constant → ideal time flat vs `p`.
- Comm per rank: `Θ(nx + ny)` → **also constant** (for fixed tile).  
  Degradation mainly due to global reductions, network congestion, and I/O.

## Memory Footprint (per rank)

For two full fields (`u`, `tmp`) including halos:

```math
M_{\mathrm{fields}} = 2 \cdot (nx + 2h) (ny + 2h) \cdot 8\ \text{bytes}
```

Plus temporary lines, MPI buffers, and datatypes (usually negligible compared to arrays).

## Putting It Together — Per-step Time Model

Let:
- `α` = latency (s/message), `β` = inverse bandwidth (s/byte) for effective links.
- `c_d, c_a` = compute costs per cell (s), for diffusion & advection respectively.

Then a simple overlapping model:

```math
T_{\mathrm{step}} \approx \max\left[ (c_d + c_a)\,nx\,ny,\ \ 4\alpha + \beta\,V_{\mathrm{halo}}(nx, ny) \right] \ +\ T_{\mathrm{reduction}}\ (if\ stats)
```

`T_reduction` for `MPI_Allreduce` is ~`O(α log p + β·payload)`, tiny payload here.

## Benchmarks to Run

- **Strong scaling:** `Nx=Ny=1024`, `steps=200`, ranks ∈ {1,2,4,8,16}.  
  Report: total runtime, per-step avg, `E_s = T1 / (p·Tp)`.
- **Weak scaling:** per-rank `(nx, ny) = (256, 256)`, steps=200, ranks ∈ {1,4,16}.  
  Report: per-step avg and `E_w = T_{p0}/T_p` with `p0=1`.
- **Sensitivity:** vary halo `h` (1→2) to see comm slope; vary output frequency to observe I/O impact.

## Measurement Methodology

- Use `MPI_Wtime()` to bracket:
  - Entire run (after warm-up barrier) → total runtime.
  - Inside loop → per-step time; accumulate average, min/max.
- Use `MPI_Reduce` to collect worst-step across ranks (max) and memory max (`VmRSS` from `/proc/self/status` on Linux).
- For shell-level timing comparisons, use `/usr/bin/time -v mpirun ...` in addition to in-code timers.

## Sanity Checks

- **Determinism:** identical outputs across runs with same seeds and decomposition.
- **CFL/diffusion limits:** automatically compute a safe `Δt` from chosen `D`, `vx`, `vy`, `Δx`, `Δy` and assert.
- **Halo correctness:** checksum per-rank borders before/after exchange.

## Appendix: Quick-reference Formulas

- Halo volume per rank (`h=1`, doubles):
  ```math
  V_{\mathrm{halo}} = 16\,(nx + ny)\ \text{bytes}
  ```
- Compute work per rank (per step):
  ```math
  W \sim \Theta(nx\,ny)
  ```
- Memory (two fields with halos):
  ```math
  M \approx 16\,(nx + 2)^2\ \text{bytes}\ \text{if}\ nx=ny\ \text{and}\ h=1
  ```
- Strong scaling efficiency:
  ```math
  E_s(p) = \frac{T_1}{p\,T_p}
  ```
- Weak scaling efficiency (baseline at `p0`):
  ```math
  E_w(p) = \frac{T_{p0}}{T_p}
  ```

---
