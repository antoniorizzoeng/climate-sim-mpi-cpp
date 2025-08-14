---
layout: default
---
{% include mathjax.html %}

# Numerics

This document formalizes the mathematical model, discretization, stability limits, and boundary conditions used in the **climate-sim-mpi-cpp** project.

## 1. Governing Equations

We model a passive scalar (temperature) on a rectangular domain $\Omega \subset \mathbb{R}^2$:

```math
\frac{\partial u}{\partial t} + \mathbf{v}\cdot\nabla u = D\,\nabla^2 u,\quad \mathbf{v}=(v_x, v_y),\ D \ge 0
```

Special cases:
- **Pure advection:** $D=0$.
- **Pure diffusion:** $\mathbf{v}=\mathbf{0}$.

## 2. Spatial Discretization

Let the global grid be uniform with spacing $\Delta x, \Delta y$. We store the field at cell centers:

```math
u_{i,j}^n \approx u(x_i,y_j,t^n),\quad x_i = (i+\frac12)\Delta x,\quad y_j = (j+\frac12)\Delta y
```

### 2.1 Diffusion (5-point Laplacian, second-order)

```math
(\nabla^2 u)_{i,j} \approx \frac{u_{i+1,j}-2u_{i,j}+u_{i-1,j}}{\Delta x^2}
+ \frac{u_{i,j+1}-2u_{i,j}+u_{i,j-1}}{\Delta y^2}
```

### 2.2 Advection (1st-order upwind, donor-cell)

Define one-sided differences based on the sign of velocity components:

```math
\delta_x^- u_{i,j} = \frac{u_{i,j}-u_{i-1,j}}{\Delta x},\quad
\delta_x^+ u_{i,j} = \frac{u_{i+1,j}-u_{i,j}}{\Delta x}
```

```math
\delta_y^- u_{i,j} = \frac{u_{i,j}-u_{i,j-1}}{\Delta y},\quad
\delta_y^+ u_{i,j} = \frac{u_{i,j+1}-u_{i,j}}{\Delta y}
```

Upwinded gradient:

```math
(\partial_x u)^{\mathrm{up}}_{i,j} =
\begin{cases}
\delta_x^- u_{i,j}, & v_x \ge 0 \\
\delta_x^+ u_{i,j}, & v_x < 0
\end{cases}
\quad
(\partial_y u)^{\mathrm{up}}_{i,j} =
\begin{cases}
\delta_y^- u_{i,j}, & v_y \ge 0 \\
\delta_y^+ u_{i,j}, & v_y < 0
\end{cases}
```

Advection term:

```math
(\mathbf{v}\cdot\nabla u)^{\mathrm{up}}_{i,j} = v_x\,(\partial_x u)^{\mathrm{up}}_{i,j} + v_y\,(\partial_y u)^{\mathrm{up}}_{i,j}
```

## 3. Time Integration

We start with **explicit forward Euler**:

```math
u_{i,j}^{n+1} = u_{i,j}^{n} - \Delta t\,(\mathbf{v}\cdot\nabla u)^{\mathrm{up}}_{i,j}
+ \Delta t\, D\,(\nabla^2 u)_{i,j}
```

(With $\mathbf{v}=0$ this reduces to FTCS diffusion; with $D=0$ it is upwind advection.)

## 4. Stability Constraints (explicit)

Time step $\Delta t$ must satisfy both advection CFL and diffusion limits.

### 4.1 Advection (CFL)

Define Courant numbers:

```math
C_x = \frac{|v_x|\,\Delta t}{\Delta x},\quad C_y = \frac{|v_y|\,\Delta t}{\Delta y}
```

For donor-cell upwind in 2D, a sufficient condition is:

```math
C_x + C_y \le 1
```

### 4.2 Diffusion (FTCS)

Define diffusion numbers:

```math
\mu_x = \frac{D\,\Delta t}{\Delta x^2},\quad
\mu_y = \frac{D\,\Delta t}{\Delta y^2}
```

Stability requires:

```math
\mu_x + \mu_y \le \frac{1}{2}
```

For $\Delta x=\Delta y$, this is $\mu \le \tfrac{1}{4}$.

### 4.3 Combined

Choose $\Delta t$ to satisfy:

```math
C_x + C_y \le 1,\quad \mu_x + \mu_y \le \frac{1}{2}
```

## 5. Boundary Conditions (BCs)

We support the following BCs (applied at physical domain boundaries; interior subdomain boundaries are handled by halo exchange):

- **Dirichlet:** $u = u_b$. Implement by filling ghost cells with the boundary value.
- **Neumann (zero-flux):** $\partial_n u = 0$. Implement by mirroring the adjacent interior value into the ghost cell.
- **Periodic:** Opposite boundaries connected (requires periodic Cartesian communicator or manual wrap copy).

BCs are applied **after** halo exchange each step.

## 6. Initial Conditions (ICs)

Default IC: single Gaussian hotspot centered at $(x_c, y_c)$:

```math
u_0(x,y) = A \exp\left(-\frac{(x-x_c)^2 + (y-y_c)^2}{2\sigma^2}\right)
```

Parameters $(A,\sigma,x_c,y_c)$ are configurable; computed locally from global coordinates so all ranks agree.

## 7. Conservation & Diagnostics

- **Diffusion:** Does **not** conserve the domain integral exactly under Dirichlet/Neumann with FTCS; with periodic BCs and symmetric Laplacian, the sum is preserved to round-off for pure diffusion.
- **Advection (upwind):** Introduces numerical diffusion; domain integral is preserved with periodic BCs (constant velocity, no sources/sinks).
- **Diagnostics:** compute min/max/mean and $L^2$ norm:

```math
\|u\|_2 = \sqrt{ \sum_{i,j} u_{i,j}^2\,\Delta x\,\Delta y }
```

## 8. MPI Halo Width

- With the above stencils, **halo width $h=1$** is sufficient.
- If higher-order advection is used (e.g., 3rd-order upwind), increase $h$ accordingly.

## 9. Parameter Cheat Sheet

- Safe $\Delta t$ (uniform grid, $\Delta x=\Delta y=\Delta$):

```math
\Delta t \le \min\left( \frac{\Delta}{|v_x|+|v_y|},\ \frac{\Delta^2}{4D} \right)
```

- Start values:
  - $\Delta = 1.0$, $D = 0.1$, $|\mathbf{v}| \le 1.0$, $\Delta t = 0.1$ (check CFL in code).

---

*This document is versioned with the codebase; changes to numerics should update this file.*
