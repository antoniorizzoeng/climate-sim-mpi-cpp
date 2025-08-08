# climate-sim-mpi-cpp

C++ + MPI toy climate model (2D diffusion/advection).  
Goal: deterministic, scalable domain-decomposed simulation with benchmarks.

## Build
```bash
mkdir -p build && cd build
cmake ..
cmake --build .
mpirun -np 4 ./mpi_hello
