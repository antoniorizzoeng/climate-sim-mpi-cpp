import os
import numpy as np
import pytest

from visualization.io import (
    assemble_global_csv,
    assemble_global_netcdf,
    list_available_steps,
    load_global,
    _HAS_NETCDF4,
)

from tests.visualization.sample_data import (
    make_tiles_nx_ny,
    write_rank_layout,
    write_csv_snapshots,
)

@pytest.fixture()
def toy_csv_case(tmp_outputs_dir):
    nxg, nyg = 12, 8
    tiles = make_tiles_nx_ny(nxg, nyg, halo=1)
    write_rank_layout(tmp_outputs_dir, tiles)

    def field_fn(t):
        ny, nx = t["ny"], t["nx"]
        y0, x0 = t["y_off"], t["x_off"]
        yy = np.arange(y0, y0 + ny)[:, None]
        xx = np.arange(x0, x0 + nx)[None, :]
        return (yy * 1000 + xx).astype(float)

    write_csv_snapshots(tmp_outputs_dir, step=0, tiles=tiles, field_fn=field_fn)
    write_csv_snapshots(tmp_outputs_dir, step=5, tiles=tiles, field_fn=field_fn)
    return tmp_outputs_dir, nxg, nyg

def test_list_available_steps_csv(toy_csv_case):
    base, _, _ = toy_csv_case
    steps = list_available_steps(base, fmt="csv")
    assert steps == [0, 5]

def test_assemble_global_csv_matches_pattern(toy_csv_case):
    base, nxg, nyg = toy_csv_case
    U = assemble_global_csv(base, step=0)
    assert U.shape == (nyg, nxg)
    assert U[0, 0] == 0
    assert U[0, 10] == 10
    assert U[7, 0] == 7000
    assert U[7, 11] == 7011

def test_load_global_dispatch_csv(toy_csv_case):
    base, nxg, nyg = toy_csv_case
    U = load_global(base, 5, fmt="csv")
    assert U.shape == (nyg, nxg)
    assert np.isfinite(U).all()

@pytest.mark.skipif(not _HAS_NETCDF4, reason="netCDF4 not installed")
def test_assemble_global_netcdf_roundtrip(tmp_outputs_dir):
    import netCDF4

    nxg, nyg = 8, 6
    tiles = make_tiles_nx_ny(nxg, nyg, halo=1)
    write_rank_layout(tmp_outputs_dir, tiles)

    snapdir = os.path.join(tmp_outputs_dir, "snapshots")
    os.makedirs(snapdir, exist_ok=True)
    step = 0

    def core_fn(t):
        ny, nx = t["ny"], t["nx"]
        y0, x0 = t["y_off"], t["x_off"]
        yy = np.arange(y0, y0 + ny)[:, None]
        xx = np.arange(x0, x0 + nx)[None, :]
        return (yy + xx / 100.0).astype(float)

    for t in tiles:
        ny = t["ny"] + 2 * t["halo"]
        nx = t["nx"] + 2 * t["halo"]
        tile = np.zeros((ny, nx), dtype=float)
        core = core_fn(t)
        tile[t["halo"]:t["halo"] + t["ny"], t["halo"]:t["halo"] + t["nx"]] = core

        path = os.path.join(snapdir, f"snapshot_{step:05d}_rank{t['rank']:05d}.nc")
        with netCDF4.Dataset(path, "w") as ds:
            ds.createDimension("y", ny)
            ds.createDimension("x", nx)
            v = ds.createVariable("u", "f8", ("y", "x"))
            v[:, :] = tile

    U = assemble_global_netcdf(tmp_outputs_dir, step=0, var="u")
    assert U.shape == (nyg, nxg)
    # Check a couple of locations in global space (must match core_fn)
    assert np.isclose(U[0, 0], 0.0)
    assert np.isclose(U[-1, -1], (nyg - 1) + (nxg - 1) / 100.0)
