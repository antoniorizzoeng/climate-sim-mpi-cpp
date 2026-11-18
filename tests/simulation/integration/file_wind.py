"""CLI-level wind checks against analytical shifts and an independent periodic stencil."""

import json
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest

from netCDF4 import Dataset
import numpy as np


class FileWind(unittest.TestCase):
    nx, ny = 9, 7

    def setUp(self):
        tmp = tempfile.TemporaryDirectory()
        self.addCleanup(tmp.cleanup)
        self.work = Path(tmp.name)
        self.start = np.arange(self.nx * self.ny, dtype=float).reshape(self.ny, self.nx) ** 2
        with Dataset(self.work / "ic.nc", "w", format="NETCDF3_CLASSIC") as nc:
            nc.createDimension("y", self.ny)
            nc.createDimension("x", self.nx)
            nc.createVariable("u", "f8", ("y", "x"))[:] = self.start

    def write_wind(self, vx, vy, records=None, vx_dims=None, vy_dims=None):
        with Dataset(self.work / "winds.nc", "w", format="NETCDF3_64BIT_DATA") as nc:
            nc.createDimension("x", self.nx)
            nc.createDimension("y", self.ny)
            if records is not None:
                nc.createDimension("time", records)
            dims = ("y", "x") if records is None else ("time", "y", "x")
            nc.createVariable("east", "f8", vx_dims or dims)[:] = vx
            nc.createVariable("north", "f4", vy_dims or dims)[:] = vy

    def simulate(self, ranks=4, args=(), error=None):
        (self.work / "run.yaml").write_text(
            "ic: {mode: file, path: ic.nc}\nbc: periodic\n"
            "wind: {mode: file, path: winds.nc, vx_var: east, vy_var: north, interval: 2}\n"
        )
        cmd = [*launcher, nflag, str(ranks), simulator, "--config=run.yaml", "--dt=1",
               "--steps=1", "--out_every=1", "--output=out.nc", "--progress=progress.json", *args]
        (self.work / "out.nc").unlink(missing_ok=True)
        result = subprocess.run(cmd, cwd=self.work, capture_output=True, text=True, timeout=20)
        state = json.loads((self.work / "progress.json").read_text())
        if error:
            self.assertNotEqual(result.returncode, 0)
            self.assertEqual(state["status"], "error")
            self.assertIn(error, state["message"])
            self.assertFalse((self.work / "out.nc").exists())
            return
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertEqual(state["status"], "done")
        with Dataset(self.work / "out.nc") as nc:
            return np.asarray(nc["u"][:]), {key: nc.getncattr(key) for key in nc.ncattrs()}

    def test_uniform(self):
        self.write_wind(0.5, -0.25)
        args = ["--steps=5", "--D=0.125", "--dt=0.5"]
        uniform, _ = self.simulate(1, [*args, "--wind.mode=uniform", "--vx=0.5", "--vy=-0.25"])
        for ranks in range(1, 5):
            frames, attrs = self.simulate(ranks, [*args, "--vx=1000", "--vy=-1000"])
            np.testing.assert_array_equal(frames, uniform)
            self.assertEqual([attrs["wind_mode"], attrs["wind_path"], attrs["wind_vx_var"],
                              attrs["wind_vy_var"]], ["file", "winds.nc", "east", "north"])
            self.assertEqual(float(attrs["wind_interval"]), 2)

        self.write_wind(0.7, -0.25, records=3)
        args = ["--steps=9", "--dx=1.7", "--dy=0.8", "--D=0.03", "--dt=0.1"]
        uniform, _ = self.simulate(1, [*args, "--wind.mode=uniform", "--vx=0.7", "--vy=-0.25"])
        frames, _ = self.simulate(4, [*args, "--wind.interval=0.3"])
        np.testing.assert_array_equal(frames, uniform)

    def test_shear(self):
        speeds = np.arange(self.ny) % 3 - 1
        self.write_wind(np.broadcast_to(speeds[:, None], self.start.shape), 0)
        frames, _ = self.simulate(args=["--steps=3"])
        for step, frame in enumerate(frames):
            expected = [np.roll(row, speed * step) for row, speed in zip(self.start, speeds)]
            np.testing.assert_array_equal(frame, expected)

    def test_interpolation(self):
        # Every component varies across tile boundaries and changes sign between records.
        y, x = np.indices(self.start.shape)
        vx = np.array([(x % 3 - 1) * s / 8 for s in (1, -1, 2, -2)])
        vy = np.array([(y % 3 - 1) * s / 8 for s in (-2, 2, 1, -1)])
        self.write_wind(vx, vy, records=4)
        for interval in (2.0, 0.375):  # both sub-record steps and jumps across records
            expected = [self.start]
            for step in range(9):
                pos = min(step / interval, 3)
                lo, hi = int(pos), min(3, int(pos) + 1)
                a = pos - lo
                u = expected[-1]
                out = u.copy()
                for wind, axis in ((vx, 1), (vy, 0)):
                    vel = (1 - a) * wind[lo] + a * wind[hi]
                    back = u - np.roll(u, 1, axis)
                    forward = np.roll(u, -1, axis) - u
                    out -= vel * np.where(vel >= 0, back, forward)
                expected.append(out)
            for ranks in range(1, 5):
                frames, _ = self.simulate(ranks, ["--steps=9", "--wind.interval", str(interval)])
                np.testing.assert_allclose(frames, expected, rtol=1e-15, atol=1e-12)

    def test_stability(self):
        # Maxima on different ranks and records, including a record past the run.
        vx = np.zeros((3, self.ny, self.nx))
        vy = np.zeros_like(vx)
        vx[0, 0, 0], vy[-1, -1, -1] = -1, 2
        self.write_wind(vx, vy, records=3)
        params = ["--dx=2", "--dy=4", "--D=0.8"]  # rate = 1/2 + 2/4 + 0.5
        self.simulate(args=[*params, "--dt=0.7"], error="combined stability limit")
        self.simulate(args=[*params, "--dt=0.66"])
        vy[-1, -1, -1] = np.nan
        self.write_wind(vx, vy, records=3)
        self.simulate(error="non-finite velocities")

        self.write_wind(1, 0)
        self.simulate(args=["--dx=1e-200"], error="combined stability limit")
        frames, _ = self.simulate(args=["--dx=1e-200", "--dt=1e-200"])
        np.testing.assert_array_equal(frames[-1], np.roll(self.start, 1, axis=1))

    def test_bad_dimensions(self):
        self.write_wind(0, 0, vx_dims=("x", "y"))
        self.simulate(error="expected dimension 'y'")
        self.write_wind(0, 0, records=1, vy_dims=("y", "x"))
        self.simulate(error="matching dimensions")
        self.write_wind(0, 0, vx_dims=("x",))
        self.simulate(error="must have dimensions")
        self.write_wind(0, 0)
        self.simulate(args=["--wind.vy_var=missing"], error="Wind variable 'missing'")
        self.simulate(args=["--ic.mode=preset", "--nx=11", "--ny=7"], error="simulation grid")
        self.simulate(args=["--wind.path", "missing.nc"], error="Cannot open wind file")

    def test_empty_tiles(self):
        self.nx = self.ny = 1
        self.write_wind(0.25, -0.5, records=1)
        frames, _ = self.simulate(args=["--ic.mode=preset", "--nx=1", "--ny=1", "--steps=2"])
        np.testing.assert_array_equal(frames, 1.0)

    def test_input_protection(self):
        self.write_wind(0, 0)
        before = (self.work / "winds.nc").read_bytes()
        self.simulate(args=["--output=winds.nc"], error="Wind and snapshot paths")
        self.assertEqual((self.work / "winds.nc").read_bytes(), before)


if __name__ == "__main__":
    simulator, nflag, launcher = sys.argv[2], sys.argv[3], sys.argv[4:]
    unittest.main(argv=sys.argv[:2])
