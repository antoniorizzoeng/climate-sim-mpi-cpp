import os
import numpy as np
import pytest
import netCDF4

from visualization.io import (
    RankTile,
    assemble_global_netcdf,
    _read_rank_layout,
    _snapshots_dir,
    _list_snapshot_files,
    _read_nc_tile,
    _place_tile
)

from tests.visualization.sample_data import (
    make_tiles_nx_ny,
    write_rank_layout,
)

def test_assemble_global_netcdf_roundtrip(tmp_outputs_dir):
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

def test_assemble_global_netcdf_missing_tile(tmp_outputs_dir):
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
        
def test_read_nc_tile_var_missing_and_not2d(tmp_path):
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

def test_read_rank_layout_inconsistent(tmp_path):
    path = tmp_path / "rank_layout.csv"
    path.write_text(
        "rank,x_offset,y_offset,nx,ny,halo,nx_global,ny_global\n"
        "0,0,0,2,2,0,4,4\n"
        "1,2,0,2,2,0,8,4\n"
    )
    with pytest.raises(RuntimeError):
        _read_rank_layout(str(path))


def test_list_snapshot_files_no_matches(tmp_path):
    snapdir = tmp_path / "snapshots"
    snapdir.mkdir()
    with pytest.raises(FileNotFoundError):
        _list_snapshot_files(str(snapdir), step=0, ext="nc")


def test_read_nc_tile_not2d(tmp_path):
    path = tmp_path / "bad.nc"
    with netCDF4.Dataset(path, "w") as ds:
        ds.createDimension("y", 2)
        ds.createDimension("x", 2)
        v = ds.createVariable("u", "f8", ("y", "x"))
        v[:, :] = np.zeros((2, 2))
    arr = _read_nc_tile(str(path), "u")
    assert arr.shape == (2, 2)

    # now 3D var
    path2 = tmp_path / "bad2.nc"
    with netCDF4.Dataset(path2, "w") as ds:
        ds.createDimension("z", 2)
        ds.createDimension("y", 2)
        ds.createDimension("x", 2)
        ds.createVariable("u", "f8", ("z", "y", "x"))
    with pytest.raises(RuntimeError):
        _read_nc_tile(str(path2), "u")


def test_place_tile_shape_mismatch():
    U = np.zeros((4, 4))
    t = RankTile(rank=0, x_off=0, y_off=0, nx=2, ny=2, halo=1, nxg=4, nyg=4)
    bad_tile = np.zeros((5, 5))
    with pytest.raises(RuntimeError):
        _place_tile(U, bad_tile, t)

