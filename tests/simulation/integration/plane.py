"""Regression checks on one-step uniform 2D simulations through the MPI CLI."""

from pathlib import Path
import subprocess
import sys
import tempfile
import unittest

from netCDF4 import Dataset
import numpy as np


class Plane(unittest.TestCase):
    def setUp(self):
        tmp = tempfile.TemporaryDirectory()
        self.addCleanup(tmp.cleanup)
        self.work = Path(tmp.name)
        self.pulse = np.zeros((5, 5))
        self.pulse[2, 2] = 1

    def simulate(self, *args, initial=None, ranks=1, error=None):
        ic = ["--ic.mode=preset"]
        if initial is not None:
            with Dataset(self.work / "input.nc", "w", format="NETCDF3_64BIT_DATA") as nc:
                for dim, length in zip(("y", "x"), initial.shape):
                    nc.createDimension(dim, length)
                nc.createVariable("u", "f8", ("y", "x"))[:] = initial
            ic = ["--ic.mode=file", "--ic.path=input.nc"]
        command = [*launcher, nflag, str(ranks), exe, f"--backend={backend}", *ic, "--bc=neumann", "--dt=1",
                   "--steps=1", "--out_every=1", "--output=out.nc", *args]
        (self.work / "out.nc").unlink(missing_ok=True)
        result = subprocess.run(command, cwd=self.work, capture_output=True, text=True, timeout=30)
        if error:
            self.assertNotEqual(result.returncode, 0)
            self.assertIn(error, result.stderr)
            self.assertFalse((self.work / "out.nc").exists())
            return
        self.assertEqual(result.returncode, 0, result.stderr)
        with Dataset(self.work / "out.nc") as nc:
            nc.set_auto_maskandscale(False)
            return nc["u"][:], {key: nc.getncattr(key) for key in nc.ncattrs()}

    def test_combined_stability(self):
        frames, attrs = self.simulate("--D=0.25", "--vx=1", initial=self.pulse)
        self.assertEqual(float(attrs["dt"]), 0.5)
        expected = np.zeros_like(self.pulse)
        expected[1, 2] = expected[3, 2] = expected[2, 1] = 0.125
        expected[2, 3] = 0.625
        np.testing.assert_array_equal(frames[-1], expected)

    def test_thin_neumann(self):
        for ranks in (1, 2, 6):
            frames, _ = self.simulate("--nx=1", "--ny=5", "--ic.sigma_frac=1", "--vx=1",
                                      ranks=ranks)
            self.assertEqual(frames[0, 2, 0], 1)
            np.testing.assert_array_equal(frames[-1], frames[0])

    def test_transport_metadata(self):
        _, attrs = self.simulate("--D=1e-7", "--vx=2e-7", "--vy=-3e-8", "--dx=2", "--dy=3",
                                 initial=self.pulse)
        self.assertEqual(float(attrs["D"]), 1e-7)
        vx, vy = attrs["velocity"].strip("()").split(",")
        self.assertEqual((float(vx), float(vy)), (2e-7, -3e-8))
        self.assertEqual((float(attrs["dx"]), float(attrs["dy"])), (2, 3))

        dx, dy, vz = 1.2345678901234567, 2.345678901234567, -1.234567890123e-8
        _, attrs = self.simulate("--nx=5", "--ny=5", "--nz=2", f"--dx={dx}", f"--dy={dy}",
                                 f"--vz={vz}")
        self.assertEqual((float(attrs["dx"]), float(attrs["dy"])), (dx, dy))
        self.assertEqual(tuple(map(float, attrs["velocity"].strip("()").split(","))), (0, 0, vz))

    def test_tiny_spacing(self):
        frames, _ = self.simulate("--dx=1e-180", initial=self.pulse)
        np.testing.assert_array_equal(frames[-1], self.pulse)
        # The normalized x diffusion coefficient is 0.25 although dx squared underflows.
        frames, _ = self.simulate("--dx=1e-180", "--D=2.5e-181", "--dt=1e-180",
                                  initial=self.pulse)
        np.testing.assert_allclose(frames[-1, 2], [0, 0.25, 0.5, 0.25, 0], atol=1e-15)

        # A preset must also start finite when its Gaussian width squared underflows.
        frames, _ = self.simulate("--dx=1e-180", "--nx=5", "--ny=5")
        self.assertTrue(np.isfinite(frames).all())
        self.assertEqual(frames[0, 2, 2], 1)
        np.testing.assert_array_equal(frames[1], frames[0])

    def test_extreme_amplitude(self):
        error = "Initial condition magnitude exceeds the stencil limit"
        field = np.zeros((5, 5))
        field[:, -2:] = [1e308, -1e308]  # Only the second rank sees the extreme values.
        for speed in (0, 1):
            self.simulate(f"--vx={speed}", initial=field, ranks=2, error=error)
        self.simulate("--nx=5", "--ny=5", "--ic.A=-1e308", error=error)

        # The inclusive limit still permits opposite signs and an active stencil.
        limit = np.finfo(float).max / 4
        field[:, -2:] = [limit, -limit]
        frames, _ = self.simulate("--vx=1", initial=field, ranks=2)
        np.testing.assert_array_equal(frames[-1, :, 1:], field[:, :-1])


if __name__ == "__main__":
    backend, exe, nflag, *launcher = sys.argv[2:]
    unittest.main(argv=sys.argv[:2])
