---
title: Learn the source
nav_order: 5
---

# Learn the Source
This document has the objective to guide you through the entire codebase.
We will follow the journey of the data throughout the code: Input -> Computation -> Output.

During developement, this document will act as Antonio's project diary and you might see some raw/non-polished lines, you might see TODOs that are still not created as github issues.

As soon the first beta is released this document will only contain the source complete explanation :)

## Input
Let's start with **i/o** structs
We have 3 of them:

**ICConfig**: Defines the Single Gaussian Initial Conditions (IC) of the simulation (TODO: To be generalized) you can learn the math [here](numerics.md)

**SimConfig**: Defines the Simulation parameters
- nx, ny: define the size of our simulation world (TODO: allow 3D)
- dx, dy: define the smallest space unit
- D: defines the [Diffusion](numerics.md) strenght
- vx, vy: define the [Advection](numerics.md) strenght
- dt: the simulation time sampling rate, the smallest unit of time in our world.
- steps: when the simulation stops, how many ticks of dt the simulation will last.
- out_every: after how many dt the simulation will report its status on disk.
- bc: the [Boundary](numerics.md) condition, what will happen at the edge of our world.
- output_prefix: self explanatory
- output_format: currently supported CSV and NetCDF (TODO: make an enum)
- Ic: ICConfig variable
- void validate(): validates that all parameters make sense (TODO: extend validation)

**CLIOverrides**:  Climate sim uses config files, defaults to struct values Command Line Interfaces (CLI) parameters are used to override config/defaults. Attributes of this struct are the optional version of SimConfig and ICConfig ones (TODO: generalize IC)

We now know our inputs:
- The definition of our world
- What happens in our world
- When we want to peek into our world and when it will end.

WIP Next up start from main.cpp