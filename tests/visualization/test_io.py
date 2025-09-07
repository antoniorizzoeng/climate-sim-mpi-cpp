import os
import numpy as np
import pytest

from visualization.io import (
    assemble_global_csv,
    assemble_global_netcdf,
    list_available_steps,
    load_global,
    _HAS_NETCDF4,
    _read_rank_layout,
    _snapshots_dir,
    _list_snapshot_files,
    _read_csv_tile,
    _place_tile,
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
    assert np.isclose(U[0, 0], 0.0)
    assert np.isclose(U[-1, -1], (nyg - 1) + (nxg - 1) / 100.0)

def test_read_rank_layout_errors(tmp_path):
    path = tmp_path / "rank_layout.csv"
    with pytest.raises(FileNotFoundError):
        _read_rank_layout(str(path))

    path.write_text("")
    with pytest.raises(RuntimeError):
        _read_rank_layout(str(path))

    path.write_text("rank,x_offset,y_offset,nx,ny,halo,nx_global,ny_global\n")
    with pytest.raises(Exception):
        _read_rank_layout(str(path))


def test_snapshots_dir_and_list_snapshot_files(tmp_path):
    with pytest.raises(FileNotFoundError):
        _snapshots_dir(str(tmp_path))

    snapdir = tmp_path / "snapshots"
    snapdir.mkdir()
    with pytest.raises(FileNotFoundError):
        _list_snapshot_files(str(snapdir), 0, "csv")
    fname = snapdir / "snapshot_00000_rank00000.csv"
    fname.write_text("1,2\n3,4\n")
    out = _list_snapshot_files(str(snapdir), 0, "csv")
    assert str(fname) in out


def test_read_csv_tile_and_place_tile(tmp_path):
    goodfile = tmp_path / "good.csv"
    np.savetxt(goodfile, np.ones((2, 2)), delimiter=",")
    arr = _read_csv_tile(str(goodfile))
    assert arr.shape == (2, 2)

    badfile = tmp_path / "bad.csv"
    np.savetxt(badfile, [1, 2, 3], delimiter=",")
    with pytest.raises(RuntimeError):
        _read_csv_tile(str(badfile))

    U = np.zeros((4, 4))
    tile = np.ones((3, 3))
    from visualization.io import RankTile
    t = RankTile(rank=0, x_off=0, y_off=0, nx=2, ny=2, halo=1, nxg=4, nyg=4)
    with pytest.raises(RuntimeError):
        _place_tile(U, tile, t)


def test_assemble_global_csv_missing_tile(toy_csv_case):
    base, _, _ = toy_csv_case
    snapdir = os.path.join(base, "snapshots")
    fname = os.path.join(snapdir, "snapshot_00000_rank00000.csv")
    os.remove(fname)
    with pytest.raises(FileNotFoundError):
        assemble_global_csv(base, step=0)


@pytest.mark.skipif(not _HAS_NETCDF4, reason="netCDF4 not installed")
def test_assemble_global_netcdf_missing_tile(tmp_outputs_dir):
    import netCDF4
    tiles = make_tiles_nx_ny(4, 4, halo=0)
    write_rank_layout(tmp_outputs_dir, tiles)
    snapdir = os.path.join(tmp_outputs_dir, "snapshots")
    os.makedirs(snapdir, exist_ok=True)
    t = tiles[0]
    path = os.path.join(snapdir, f"snapshot_00000_rank{t['rank']:05d}.nc")
    with netCDF4.Dataset(path, "w") as ds:
        ds.createDimension("y", t["ny"])
        ds.createDimension("x", t["nx"])
        v = ds.createVariable("u", "f8", ("y", "x"))
        v[:, :] = np.zeros((t["ny"], t["nx"]))
    with pytest.raises(FileNotFoundError):
        assemble_global_netcdf(tmp_outputs_dir, step=0, var="u")


def test_list_available_steps_invalid_fmt(toy_csv_case):
    base, _, _ = toy_csv_case
    with pytest.raises(ValueError):
        list_available_steps(base, fmt="txt")


def test_load_global_invalid_fmt(toy_csv_case):
    base, _, _ = toy_csv_case
    with pytest.raises(ValueError):
        load_global(base, 0, fmt="txt")

def test_read_nc_tile_no_netcdf(monkeypatch):
    from visualization import io
    monkeypatch.setattr(io, "_HAS_NETCDF4", False)
    with pytest.raises(ImportError):
        io._read_nc_tile("fake.nc", "u")

@pytest.mark.skipif(not _HAS_NETCDF4, reason="netCDF4 not installed")
def test_read_nc_tile_var_missing_and_not2d(tmp_path):
    import netCDF4
    path = tmp_path / "bad.nc"
    with netCDF4.Dataset(path, "w") as ds:
        ds.createDimension("y", 4)
        ds.createDimension("x", 4)
        ds.createVariable("v", "f8", ("y", "x"))
    from visualization import io
    with pytest.raises(RuntimeError):
        io._read_nc_tile(str(path), "u")

    path2 = tmp_path / "bad2.nc"
    with netCDF4.Dataset(path2, "w") as ds:
        ds.createDimension("z", 2)
        ds.createVariable("u", "f8", ("z",))
    with pytest.raises(RuntimeError):
        io._read_nc_tile(str(path2), "u")
