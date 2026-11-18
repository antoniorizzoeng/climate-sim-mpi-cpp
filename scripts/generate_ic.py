#!/usr/bin/env python3
import numpy as np
import os
import argparse
from netCDF4 import Dataset

def make_gaussian_ic(Nx=256, Ny=512, dx=1.0, dy=1.0,
                     A=1.0, sigma_frac=0.05, xc_frac=0.5, yc_frac=0.5, nz=1, dz=1.0, zc_frac=0.5):
    x = (np.arange(Nx) + 0.5) * dx
    y = (np.arange(Ny) + 0.5) * dy
    Lx, Ly = Nx * dx, Ny * dy
    r2 = (x - xc_frac * Lx)**2 + (y[:, None] - yc_frac * Ly)**2
    width = min(Lx, Ly)
    if nz > 1:
        Lz = nz * dz
        width = min(width, Lz)
        z = (np.arange(nz) + 0.5) * dz - zc_frac * Lz
        r2 = r2 + z[:, None, None]**2
    sigma = sigma_frac * width
    U = A * np.exp(-r2 / (2 * sigma * sigma))
    return U

def write_netcdf(U, out_path, dx=1.0, dy=1.0, var="u", dz=1.0):
    Ny, Nx = U.shape[-2:]
    os.makedirs(os.path.dirname(out_path) or ".", exist_ok=True)
    with Dataset(out_path, "w", format="NETCDF3_64BIT_DATA") as nc:
        nc.createDimension("x", Nx)
        nc.createDimension("y", Ny)

        x_var = nc.createVariable("x", "f8", ("x",))
        y_var = nc.createVariable("y", "f8", ("y",))
        axes = ("y", "x")
        if U.ndim == 3:
            nc.createDimension("z", U.shape[0])
            nc.createVariable("z", "f8", ("z",))[:] = (np.arange(U.shape[0]) + 0.5) * dz
            axes = ("z", *axes)
        u_var = nc.createVariable(var, "f8", axes)

        x_var[:] = (np.arange(Nx) + 0.5) * dx
        y_var[:] = (np.arange(Ny) + 0.5) * dy
        u_var[:] = U

        u_var.units = "arbitrary"
        u_var.long_name = "Gaussian hotspot"

    print(f"[netcdf] Initial condition written to {out_path} (var='{var}')")

if __name__ == "__main__":
    p = argparse.ArgumentParser()
    p.add_argument("--nx", type=int, default=16)
    p.add_argument("--ny", type=int, default=16)
    p.add_argument("--nz", type=int, default=1)
    p.add_argument("--dz", type=float, default=1.0)
    p.add_argument("--zc-frac", type=float, default=0.5)
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

    if args.nz < 1 or not np.isfinite(args.dz) or args.dz <= 0:
        p.error("nz and dz must be positive and finite")
    U = make_gaussian_ic(args.nx, args.ny, args.dx, args.dy,
                         args.amp, args.sigma_frac, args.xc_frac, args.yc_frac,
                         args.nz, args.dz, args.zc_frac)

    out_path = args.outfile or os.path.join(args.outdir, "ic_global.nc")
    if not os.path.splitext(out_path)[1]:
        out_path += ".nc"

    write_netcdf(U, out_path, args.dx, args.dy, var=args.var, dz=args.dz)
