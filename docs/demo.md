---
layout: default
---

# Demo & Gallery

Below is a short animation from a 2D Gaussian hotspot diffusing and advecting to the right.
The simulation was run on 4 MPI ranks with periodic BCs and saved every 10 steps.

![Gaussian hotspot demo](demo.gif)

**Reproduce**: see [Getting Started](getting_started.md) for the exact `dev.yaml` and commands.

---

## Tips for photogenic runs

- Use `nx=ny=512` for smooth visuals.
- Keep `D` around `0.3–0.7` so the hotspot fades but doesn’t vanish too fast.
- Small rightward flow like `vx=0.3–0.6` keeps motion interesting without smearing.
- Set `out_every=10–20` and `fps=20–30` for a pleasing pace.

---

[← Back to Home](index.md)
