#!/usr/bin/env python3
import numpy as np
import os
import argparse
from netCDF4 import Dataset

def make_gaussian_ic(Nx=256, Ny=512, dx=1.0, dy=1.0,
                     A=1.0, sigma_frac=0.05, xc_frac=0.5, yc_frac=0.5):
    x = (np.arange(Nx) + 0.5) * dx
    y = (np.arange(Ny) + 0.5) * dy
    X, Y = np.meshgrid(x, y, indexing='xy')

    Lx, Ly = Nx * dx, Ny * dy
    xc, yc = xc_frac * Lx, yc_frac * Ly
    sigma = sigma_frac * min(Lx, Ly)

    U = A * np.exp(-((X - xc)**2 + (Y - yc)**2) / (2 * sigma * sigma))
    return U

def write_netcdf(U, out_path, dx=1.0, dy=1.0, var="u"):
    Ny, Nx = U.shape
    os.makedirs(os.path.dirname(out_path) or ".", exist_ok=True)
    with Dataset(out_path, "w", format="NETCDF4") as nc:
        nc.createDimension("x", Nx)
        nc.createDimension("y", Ny)

        x_var = nc.createVariable("x", "f8", ("x",))
        y_var = nc.createVariable("y", "f8", ("y",))
        u_var = nc.createVariable(var, "f8", ("y","x"))

        x_var[:] = (np.arange(Nx) + 0.5) * dx
        y_var[:] = (np.arange(Ny) + 0.5) * dy
        u_var[:, :] = U

        u_var.units = "arbitrary"
        u_var.long_name = "Gaussian hotspot"

    print(f"[netcdf] Initial condition written to {out_path} (var='{var}')")

if __name__ == "__main__":
    p = argparse.ArgumentParser()
    p.add_argument("--nx", type=int, default=256)
    p.add_argument("--ny", type=int, default=512)
    p.add_argument("--dx", type=float, default=1.0)
    p.add_argument("--dy", type=float, default=1.0)
    p.add_argument("--amp", type=float, default=1.0)
    p.add_argument("--sigma-frac", type=float, default=0.05)
    p.add_argument("--xc-frac", type=float, default=0.5)
    p.add_argument("--yc-frac", type=float, default=0.5)
    p.add_argument("--outdir", type=str, default="inputs",
                   help="Directory to place output if --outfile not given")
    p.add_argument("--outfile", "--out", type=str, default="",
                   help="Full path to output file; extension inferred if missing")
    p.add_argument("--var", type=str, default="u",
                   help="NetCDF variable name to write (default: 'u')")

    args = p.parse_args()

    U = make_gaussian_ic(args.nx, args.ny, args.dx, args.dy,
                         args.amp, args.sigma_frac, args.xc_frac, args.yc_frac)

    out_path = os.path.join(args.outdir, "ic_global.nc")

    write_netcdf(U, out_path, args.dx, args.dy, var=args.var)
