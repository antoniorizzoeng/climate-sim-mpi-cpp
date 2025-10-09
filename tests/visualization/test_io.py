import pytest
import numpy as np
import netCDF4

from visualization.io import _snapshots_dir, list_available_steps, load_global, load_metadata

def test_snapshots_dir_missing(tmp_path):
    missing_dir = tmp_path / "nonexistent"
    with pytest.raises(FileNotFoundError):
        _snapshots_dir(str(missing_dir))

def test_snapshots_dir_exists(tmp_path):
    path = _snapshots_dir(str(tmp_path))
    assert path == str(tmp_path)


def test_list_available_steps_empty(tmp_path):
    steps = list_available_steps(str(tmp_path))
    assert steps == []

def test_list_available_steps_missing_time_dim(tmp_path):
    nc_path = tmp_path / "file.nc"
    with netCDF4.Dataset(nc_path, "w") as ds:
        ds.createDimension("y", 2)
        ds.createDimension("x", 2)
        ds.createVariable("u", "f8", ("y", "x"))
    with pytest.raises(RuntimeError):
        list_available_steps(str(tmp_path))

def test_list_available_steps_returns_steps(tmp_path):
    nc_path = tmp_path / "file.nc"
    with netCDF4.Dataset(nc_path, "w") as ds:
        ds.createDimension("time", 3)
        ds.createDimension("y", 2)
        ds.createDimension("x", 2)
        ds.createVariable("u", "f8", ("time", "y", "x"))
    steps = list_available_steps(str(tmp_path))
    assert steps == [0, 1, 2]


def test_load_global_no_nc_files(tmp_path):
    with pytest.raises(FileNotFoundError):
        load_global(str(tmp_path), 0, var="u")

def test_load_global_var_missing(tmp_path):
    nc_path = tmp_path / "file.nc"
    with netCDF4.Dataset(nc_path, "w") as ds:
        ds.createDimension("time", 2)
        ds.createDimension("y", 2)
        ds.createDimension("x", 2)
        ds.createVariable("v", "f8", ("time", "y", "x"))
    with pytest.raises(KeyError):
        load_global(str(tmp_path), 0, var="u")

def test_load_global_no_time_dim(tmp_path):
    nc_path = tmp_path / "file.nc"
    with netCDF4.Dataset(nc_path, "w") as ds:
        ds.createDimension("y", 2)
        ds.createDimension("x", 2)
        ds.createVariable("u", "f8", ("y", "x"))
    with pytest.raises(RuntimeError):
        load_global(str(tmp_path), 0, var="u")

def test_load_global_step_out_of_range(tmp_path):
    nc_path = tmp_path / "file.nc"
    with netCDF4.Dataset(nc_path, "w") as ds:
        ds.createDimension("time", 2)
        ds.createDimension("y", 2)
        ds.createDimension("x", 2)
        ds.createVariable("u", "f8", ("time", "y", "x"))
    with pytest.raises(IndexError):
        load_global(str(tmp_path), 5, var="u")

def test_load_global_returns_correct_array(tmp_path):
    nc_path = tmp_path / "file.nc"
    arr = np.arange(4).reshape((1, 2, 2))
    with netCDF4.Dataset(nc_path, "w") as ds:
        ds.createDimension("time", 1)
        ds.createDimension("y", 2)
        ds.createDimension("x", 2)
        v = ds.createVariable("u", "f8", ("time", "y", "x"))
        v[0, :, :] = arr[0]
    out = load_global(str(tmp_path), 0, var="u")
    assert isinstance(out, np.ndarray)
    assert out.shape == (2, 2)
    assert np.array_equal(out, arr[0])

def test_load_metadata_returns_dict(tmp_path):
    nc_path = tmp_path / "file.nc"
    with netCDF4.Dataset(nc_path, "w") as ds:
        ds.description = "test dataset"
    meta = load_metadata(str(tmp_path))
    assert isinstance(meta, dict)
    assert meta.get("description") == "test dataset"
