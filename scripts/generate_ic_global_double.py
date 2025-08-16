import numpy as np
import os
import argparse

try:
    from netCDF4 import Dataset
    HAS_NETCDF = True
except ImportError:
    HAS_NETCDF = False

def make_gaussian_ic(Nx=256, Ny=512, dx=1.0, dy=1.0, A=1.0):
    x = (np.arange(Nx) + 0.5) * dx
    y = (np.arange(Ny) + 0.5) * dy
    X, Y = np.meshgrid(x, y, indexing='xy')

    Lx, Ly = Nx * dx, Ny * dy
    xc, yc = 0.5 * Lx, 0.5 * Ly
    sigma = 0.05 * min(Lx, Ly)

    U = A * np.exp(-((X - xc)**2 + (Y - yc)**2) / (2 * sigma * sigma))
    return U

def write_bin(U, out_path):
    U.astype(np.float64).tofile(out_path)
    print(f"[bin] Initial condition written to {out_path}")

def write_netcdf(U, out_path, dx=1.0, dy=1.0):
    if not HAS_NETCDF:
        raise RuntimeError("netCDF4 not available, install with `pip install netCDF4`")
    Ny, Nx = U.shape
    with Dataset(out_path, "w", format="NETCDF4") as nc:
        nc.createDimension("x", Nx)
        nc.createDimension("y", Ny)

        x_var = nc.createVariable("x", "f8", ("x",))
        y_var = nc.createVariable("y", "f8", ("y",))
        u_var = nc.createVariable("T", "f8", ("y","x"))

        x_var[:] = (np.arange(Nx) + 0.5) * dx
        y_var[:] = (np.arange(Ny) + 0.5) * dy
        u_var[:, :] = U

        # Add simple metadata
        u_var.units = "arbitrary"
        u_var.long_name = "Gaussian hotspot"

    print(f"[netcdf] Initial condition written to {out_path}")

if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("--format", choices=["bin","netcdf"], default="bin",
                        help="Output format: raw binary (.bin) or NetCDF (.nc)")
    parser.add_argument("--nx", type=int, default=256)
    parser.add_argument("--ny", type=int, default=512)
    parser.add_argument("--dx", type=float, default=1.0)
    parser.add_argument("--dy", type=float, default=1.0)
    parser.add_argument("--amp", type=float, default=1.0)
    parser.add_argument("--outdir", type=str, default="inputs")
    args = parser.parse_args()

    U = make_gaussian_ic(args.nx, args.ny, args.dx, args.dy, args.amp)

    os.makedirs(args.outdir, exist_ok=True)
    if args.format == "bin":
        out_path = os.path.join(args.outdir, "ic_global_double.bin")
        write_bin(U, out_path)
    else:
        out_path = os.path.join(args.outdir, "ic_global.nc")
        write_netcdf(U, out_path, args.dx, args.dy)
