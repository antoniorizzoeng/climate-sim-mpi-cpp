#!/usr/bin/env python3
"""Generate cell-centred shear winds in a CDF-5 file."""

import argparse
from pathlib import Path

import numpy as np
from netCDF4 import Dataset


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--nx", type=int, default=64)
    parser.add_argument("--ny", type=int, default=48)
    parser.add_argument("--records", type=int, default=3)
    parser.add_argument("--interval", type=float, default=3600.0, help="seconds between records")
    parser.add_argument("--outfile", type=Path, default=Path("inputs/winds.nc"))
    args = parser.parse_args()
    if min(args.nx, args.ny, args.records) < 1:
        parser.error("nx, ny and records must be positive")
    if not np.isfinite(args.interval) or args.interval <= 0:
        parser.error("interval must be finite and positive")

    args.outfile.parent.mkdir(parents=True, exist_ok=True)
    with Dataset(args.outfile, "w", format="NETCDF3_64BIT_DATA") as nc:
        nc.createDimension("time", args.records)
        nc.createDimension("y", args.ny)
        nc.createDimension("x", args.nx)
        time = nc.createVariable("time", "f8", ("time",))
        time.units = "seconds"
        time[:] = np.arange(args.records) * args.interval
        vx = nc.createVariable("vx", "f8", ("time", "y", "x"))
        vy = nc.createVariable("vy", "f8", ("time", "y", "x"))
        x = 2 * np.pi * (np.arange(args.nx) + 0.5) / args.nx
        y = 2 * np.pi * (np.arange(args.ny) + 0.5) / args.ny
        for record in range(args.records):
            phase = 2 * np.pi * record / args.records
            vx[record] = np.broadcast_to(0.5 * np.sin(y[:, None] + phase), (args.ny, args.nx))
            vy[record] = np.broadcast_to(0.25 * np.cos(x - phase), (args.ny, args.nx))
    print(f"Wrote {args.records} wind records to {args.outfile}")


if __name__ == "__main__":
    main()
