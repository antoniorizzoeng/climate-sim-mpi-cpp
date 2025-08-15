import numpy as np
import os

Nx, Ny = 256, 512
dx = dy = 1.0
x = (np.arange(Nx) + 0.5) * dx
y = (np.arange(Ny) + 0.5) * dy
X, Y = np.meshgrid(x, y, indexing='xy')

A = 1.0
Lx, Ly = Nx * dx, Ny * dy
xc, yc = 0.5 * Lx, 0.5 * Ly
sigma = 0.05 * min(Lx, Ly)

U = A * np.exp(-((X - xc)**2 + (Y - yc)**2) / (2 * sigma * sigma))

out_dir = "inputs"
os.makedirs(out_dir, exist_ok=True) 
out_path = os.path.join(out_dir, "ic_global_double.bin")

U.astype(np.float64).tofile(out_path)

print(f"Initial condition written to {out_path}")
