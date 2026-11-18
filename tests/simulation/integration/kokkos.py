"""Compare every published Kokkos frame with the native MPI simulator."""

from pathlib import Path
import subprocess
import sys
import tempfile

from netCDF4 import Dataset
import numpy as np


def inputs(work, shape):
    axes = ("z", "y", "x")[-len(shape):]
    rng = np.random.default_rng(62)
    with Dataset(work / "ic.nc", "w", format="NETCDF3_CLASSIC") as nc:
        for axis, extent in zip(axes, shape):
            nc.createDimension(axis, extent)
        nc.createVariable("u", "f8", axes)[:] = rng.uniform(-1, 1, shape)
    with Dataset(work / "wind.nc", "w", format="NETCDF3_CLASSIC") as nc:
        nc.createDimension("time", 3)
        for axis, extent in zip(axes, shape):
            nc.createDimension(axis, extent)
        for axis in axes:
            nc.createVariable("v" + axis, "f8", ("time", *axes))[:] = rng.uniform(
                -0.5, 0.5, (3, *shape))


def compare(work, launcher, flags):
    frames = []
    for backend in ("cpu", "kokkos"):
        subprocess.run([*launcher, "--backend=" + backend, "--steps=7", "--out_every=2",
                        "--dt=0.125", "--dx=1.25", "--dy=0.75", "--dz=1.5", "--D=0.06",
                        "--Dz=0.09", "--vx=0.21", "--vy=-0.33", "--vz=-0.17",
                        "--output=out.nc", *flags], cwd=work, check=True, timeout=60,
                       stdout=subprocess.DEVNULL)
        with Dataset(work / "out.nc") as nc:
            frames.append(np.asarray(nc["u"][:]))
    assert frames[0].shape[0] == 5  # initial, 2, 4, 6, final
    np.testing.assert_allclose(frames[1], frames[0], rtol=2e-15, atol=2e-15,
                               equal_nan=False, err_msg=str(flags))
    return np.max(np.abs(frames[1] - frames[0]))


def main():
    exe, ranks, nflag, *mpiexec = sys.argv[1:]
    launcher = [*mpiexec, nflag, ranks, exe]
    errors = []
    with tempfile.TemporaryDirectory() as tmp:
        work = Path(tmp)
        for shape in ((9, 13), (7, 9, 13), (17, 1, 1)):
            inputs(work, shape)
            uniform = []
            file = ["--wind.mode=file", "--wind.path=wind.nc", "--wind.interval=0.3125"]
            if len(shape) == 3:
                file.append("--wind.vz_var=vz")
            for wind in (uniform, file):
                for boundary in (["--bc=dirichlet"], ["--bc=neumann"], ["--bc=periodic"],
                                 ["--bc=periodic", "--bc.bottom=dirichlet", "--bc.top=neumann",
                                  "--bc.front=neumann", "--bc.back=dirichlet"]):
                    errors.append(compare(work, launcher,
                                          ["--ic.mode=file", "--ic.path=ic.nc", *wind, *boundary]))
            if len(shape) == 3:  # missing vertical file component
                errors.append(compare(work, launcher, ["--ic.mode=file", "--ic.path=ic.nc",
                                                       "--bc=periodic", *file[:-1]]))
        inputs(work, (5, 1))
        for bc in ("neumann", "periodic"):
            for wind in ([], ["--wind.mode=file", "--wind.path=wind.nc", "--wind.interval=0.05"]):
                errors.append(compare(work, launcher, ["--nx=1", "--ny=5", "--ic.sigma_frac=0.8",
                                                       "--bc=" + bc, *wind]))
        if int(ranks) == 3:  # two populated z slabs and one rank with MPI_COMM_NULL
            errors.append(compare(work, launcher, ["--nx=1", "--ny=1", "--nz=2", "--bc=periodic",
                                                   "--ic.sigma_frac=0.8", "--ic.zc_frac=0.2"]))
    print(f"{len(errors)} comparisons on {ranks} ranks, max abs difference {max(errors):.4g}")


if __name__ == "__main__":
    main()
