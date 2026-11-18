---
title: Computational Analysis
nav_order: 3
---

{% include mathjax.html %}

# Computational analysis

## One local step

For a populated 2D tile with $n_x n_y$ interior cells, the production step exchanges faces, fills physical ghosts, sweeps both transport terms into the destination field and swaps storage. In a volume it repeats that row sweep across the $n_z$ local layers. Output writes only at the configured cadence, with an additional final frame if needed.

| Cost per participating rank | Plane | Volume |
| --- | --- | --- |
| Fused stencil work | $\Theta(n_x n_y)$ | $\Theta(n_x n_y n_z)$ |
| Interior face payload sent to neighbours, in doubles | $2(n_x+n_y)$ | $2(n_x n_y+n_y n_z+n_z n_x)$ |
| Two native scalar buffers, in bytes | $16(n_x+2)(n_y+2)$ | $16(n_x+2)(n_y+2)(n_z+2)$ |
| Field bytes per snapshot | $8n_x n_y$ | $8n_x n_y n_z$ |

The face counts assume neighbours on both sides of every axis. A physical edge can remove a message. A periodic exchange includes extra edge and corner ghosts as later axes wrap. File wind adds local velocity buffers for interpolation. Kokkos may also stage halo and output data on the host.

## Strong and weak scaling

For a fixed square plane on $p$ balanced ranks, work per rank decreases as $1/p$ while face payload decreases only as $1/\sqrt{p}$. For a fixed cube the corresponding face term decreases as $p^{-2/3}$. Eventually communication takes a larger fraction of each step. The production kernel completes the exchange before its stencil sweep.

Weak scaling keeps the local extents fixed. Each rank has roughly constant stencil work and face payload, but collective I/O and contention can still grow with the job. The benchmark reports global cell updates per second and median milliseconds per step, using maximum rank time. Strong scaling efficiency between one and $p$ ranks is $T_1/(pT_p)$.

## Measured cost of output

The published 256³ case advanced eight steps. On twelve ranks, compute took 27.09 ms per step. Including three collective snapshots and closing the file raised this to 162.98 ms per step. Snapshot frequency, storage throughput and caching matter when comparing whole runs.

[Portable, native and Kokkos measurements](performance.md) · [Decomposition and file layout](architecture.md)
