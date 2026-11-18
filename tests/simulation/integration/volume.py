"""Exercise the production MPI CLI, classic NetCDF inputs and independent 3D solutions."""

import json
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest

from netCDF4 import Dataset
import numpy as np


class Volume(unittest.TestCase):
    def setUp(self):
        tmp = tempfile.TemporaryDirectory()
        self.addCleanup(tmp.cleanup)
        self.work = Path(tmp.name)

    def initial(self, field, dims=("z", "y", "x")):
        with Dataset(self.work / "ic.nc", "w", format="NETCDF3_64BIT_DATA") as nc:
            for dim, length in zip(dims, field.shape):
                nc.createDimension(dim, length)
            nc.createVariable("u", "f8", dims)[:] = field

    def simulate(self, *args, ranks=4, error=None):
        command = [*launcher, nflag, str(ranks), exe, "--ic.mode=file", "--ic.path=ic.nc",
                   "--steps=4", "--out_every=1", "--output=out.nc", "--progress=progress.json", *args]
        result = subprocess.run(command, cwd=self.work, text=True, capture_output=True, timeout=240)
        state = json.loads((self.work / "progress.json").read_text())
        if error:
            self.assertNotEqual(result.returncode, 0)
            self.assertEqual(state["status"], "error")
            self.assertIn(error, state["message"])
            return
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertEqual(state["status"], "done")
        with Dataset(self.work / "out.nc") as nc:
            self.assertEqual(nc.data_model, "NETCDF3_64BIT_DATA")
            return nc["u"][:], nc["u"].dimensions, {a: nc.getncattr(a) for a in nc.ncattrs()}

    def test_reference(self):
        # Scalar finite differences with each external neighbor set independently.
        u0 = np.random.default_rng(59).normal(size=(7, 9, 11))
        self.initial(u0)
        for front, back in (("dirichlet", "neumann"), ("neumann", "dirichlet"),
                            ("periodic", "periodic")):
            u = u0
            expected = [u]
            for _ in range(4):
                update = np.zeros_like(u)
                for axis, (spacing, diffusivity, speed, low, high) in enumerate([
                    (1.5, 0.125, -0.4, front, back),
                    (0.75, 0.05, 0.2, "periodic", "periodic"),
                    (1.25, 0.05, -0.1, "neumann", "dirichlet"),
                ]):
                    lower, upper = np.roll(u, 1, axis), np.roll(u, -1, axis)
                    for ghost, side, bc in [(lower, 0, low), (upper, -1, high)]:
                        if bc != "periodic":
                            face = [slice(None)] * 3
                            face[axis] = side
                            ghost[tuple(face)] = 0 if bc == "dirichlet" else u[tuple(face)]
                    update += diffusivity * (lower - 2*u + upper) / spacing**2
                    update -= speed * ((u - lower) if speed > 0 else (upper - u)) / spacing
                u = u + 0.25 * update
                expected.append(u)
            for ranks in (1, 2, 3, 4, 8):
                frames, dims, attrs = self.simulate(
                    "--dx=1.25", "--dy=0.75", "--dz=1.5", "--dt=0.25", "--D=0.05",
                    "--Dz=0.125", "--vx=-0.1", "--vy=0.2", "--vz=-0.4",
                    "--bc.left=neumann", "--bc.bottom=periodic", "--bc.top=periodic",
                    f"--bc.front={front}", f"--bc.back={back}", ranks=ranks)
                self.assertEqual(dims, ("time", "z", "y", "x"))
                self.assertEqual((attrs["grid"], int(attrs["nz"]), float(attrs["dz"]),
                                  float(attrs["Dz"])), ("11 x 9 x 7", 7, 1.5, 0.125))
                np.testing.assert_allclose(frames, expected, atol=2e-15, rtol=2e-15)

    def test_vertical_diffusion(self):
        # The periodic Fourier mode is an exact eigenvector of the discrete Laplacian.
        nz = 17
        u = np.cos(2*np.pi*np.arange(nz)/nz)[:, None, None]
        self.initial(u)
        frames, _, _ = self.simulate("--bc=periodic", "--D=0", "--Dz=0.25", "--dz=0.5",
                                         "--dt=0.1", ranks=8)
        factor = 1 - 4*0.25*0.1/0.5**2*np.sin(np.pi/nz)**2
        np.testing.assert_allclose(frames, [u*factor**n for n in range(5)], atol=4e-15)
        # Grid inference must happen before clamping.
        _, _, attrs = self.simulate("--D=0.5", "--vz=-1", "--dz=2", "--dt=1", ranks=2)
        self.assertAlmostEqual(float(attrs["dt"]), 1/(0.5 + 2*(1+0.5/4)))
        self.assertEqual(float(attrs["Dz"]), 0.5)

    def test_surplus_ranks(self):
        # A prime-size job cannot tile this 2x2x1 volume with five populated ranks.
        u = np.arange(4, dtype=float).reshape(2, 2, 1)
        self.initial(u)
        frames, _, _ = self.simulate("--dt=1", "--vz=-1", "--bc=periodic", ranks=5)
        np.testing.assert_array_equal(frames, [np.roll(u, -n, axis=0) for n in range(5)])

    def test_file_wind(self):
        shape = (5, 7, 9)
        u = np.arange(np.prod(shape), dtype=float).reshape(shape)**2
        self.initial(u)
        z, y, x = np.indices(shape)
        args = ["--dt=0.5", "--bc=periodic", "--wind.mode=file", "--wind.path=wind.nc",
                "--wind.interval=1"]
        winds = [(coord % 3 - 1)/8 for coord in (z, y, x)]
        with Dataset(self.work / "wind.nc", "w", format="NETCDF3_CLASSIC") as nc:
            for dim, length in zip(("time", "z", "y", "x"), (2, *shape)):
                nc.createDimension(dim, length)
            for name, speed in zip(("up", "vy", "vx"), winds):
                nc.createVariable(name, "f8", ("time", "z", "y", "x"))[:] = [speed, -speed]
        for vertical in (False, True):
            expected = [u]
            for scale in (1, 0, -1, -1):
                field = expected[-1]
                out = field.copy()
                for axis, speed in enumerate(winds):
                    if axis == 0 and not vertical:
                        continue
                    v = speed * scale
                    out -= 0.5 * v * np.where(v >= 0, field - np.roll(field, 1, axis),
                                              np.roll(field, -1, axis) - field)
                expected.append(out)
            frames, _, attrs = self.simulate(*args, *(["--wind.vz_var=up"] if vertical else []),
                                             ranks=8)
            np.testing.assert_allclose(frames, expected, atol=1e-12)
            if vertical:
                self.assertEqual(attrs["wind_vz_var"], "up")
        self.simulate(*args, "--wind.vz_var=up", "--dt=3", error="combined stability limit")
        self.simulate(*args, "--wind.vz_var=missing", error="Wind variable 'missing'")

    def test_steady_wind(self):
        shape = (5, 7, 9)
        self.initial(np.random.default_rng(3).normal(size=shape))
        with Dataset(self.work / "wind.nc", "w", format="NETCDF3_CLASSIC") as nc:
            for dim, length in zip(("z", "y", "x"), shape):
                nc.createDimension(dim, length)
            for name, speed in (("vx", -0.25), ("vy", 0.5), ("vz", 0.125)):
                nc.createVariable(name, "f4", ("z", "y", "x"))[:] = speed
        args = ["--bc=periodic", "--dx=1.5", "--dy=0.5", "--dz=2",
                "--D=0.05", "--Dz=0.1", "--dt=0.2"]
        uniform, _, _ = self.simulate(*args, "--vx=-0.25", "--vy=0.5", "--vz=0.125", ranks=1)
        steady, _, _ = self.simulate(*args, "--wind.mode=file", "--wind.path=wind.nc",
                                     "--wind.vz_var=vz", ranks=8)
        np.testing.assert_array_equal(steady, uniform)
        self.initial(np.zeros(shape).transpose(2, 1, 0), ("x", "y", "z"))
        self.simulate(error="expected dimension 'z'")

    def test_generators(self):
        scripts = Path(__file__).resolve().parents[3] / "scripts"
        for script, filename in (("generate_wind.py", "wind.nc"), ("generate_ic.py", "ic.nc")):
            subprocess.run([sys.executable, scripts / script, "--nx=9", "--ny=7", "--nz=5",
                            "--outfile", self.work / filename], check=True)
        (self.work / "volume.yaml").write_text(
            "grid: {nz: 5, dz: 1}\nphysics: {Dz: 0.2, vz: 0.15}\n"
            "ic: {zc_frac: 0.5}\nbc: {front: periodic, back: periodic}\n"
            "wind: {mode: file, path: wind.nc, vz_var: vz, interval: 2}\n")
        args = ["--config=volume.yaml", "--nx=9", "--ny=7"]
        file_frames, _, _ = self.simulate(*args)
        preset, _, _ = self.simulate(*args, "--ic.mode=preset")
        np.testing.assert_allclose(file_frames, preset, atol=1e-15)
        self.simulate("--nz=6", error="z extent")
        self.simulate("--dz=0", error="nz and dz")
        self.simulate("--Dz=-1", error="nonnegative")
        self.simulate("--bc.front=periodic", error="front and back")

    def test_replication(self):
        u = np.arange(35, dtype=float).reshape(5, 7)
        self.initial(u, ("y", "x"))
        self.simulate("--nz=3", error="replicate_z")
        (self.work / "rep.yaml").write_text("grid: {nz: 3}\nic: {replicate_z: true}\n")
        frames, _, _ = self.simulate("--config=rep.yaml", "--bc=periodic", "--vz=1")
        np.testing.assert_array_equal(frames, np.broadcast_to(u, frames.shape))
        self.simulate("--config=rep.yaml", "--ic.replicate_z=false", error="replicate_z")
        old, dims, attrs = self.simulate()
        explicit, dims2, attrs2 = self.simulate("--nz=1", "--vz=9", "--Dz=8", "--dz=0.125")
        self.assertEqual(dims, ("time", "y", "x"))
        self.assertEqual((dims, attrs), (dims2, attrs2))
        np.testing.assert_array_equal(old, explicit)


if __name__ == "__main__":
    exe, nflag, launcher = sys.argv[2], sys.argv[3], sys.argv[4:]
    unittest.main(argv=sys.argv[:2])
