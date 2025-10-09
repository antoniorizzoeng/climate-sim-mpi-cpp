import os
from typing import List, Dict
import numpy as np
import netCDF4


def _snapshots_dir(base_outputs_dir: str) -> str:
    if not os.path.isdir(base_outputs_dir):
        raise FileNotFoundError(f"directory not found: {base_outputs_dir}")
    return base_outputs_dir


def list_available_steps(base_outputs_dir: str) -> List[int]:
    snap_dir = _snapshots_dir(base_outputs_dir)
    nc_files = [f for f in os.listdir(snap_dir) if f.endswith(".nc")]
    if not nc_files:
        return []
    nc_path = os.path.join(snap_dir, nc_files[0])
    with netCDF4.Dataset(nc_path, "r") as ds:
        if "time" not in ds.dimensions:
            raise RuntimeError(f"No time dimension 'time' in {nc_path}")
        nt = len(ds.dimensions["time"])
    return list(range(nt))


def load_global(base_outputs_dir: str, step: int, var: str = "u") -> np.ndarray:
    snap_dir = _snapshots_dir(base_outputs_dir)
    nc_files = [f for f in os.listdir(snap_dir) if f.endswith(".nc")]
    if not nc_files:
        raise FileNotFoundError(f"No NetCDF file found in {base_outputs_dir}")
    nc_path = os.path.join(snap_dir, nc_files[0])

    with netCDF4.Dataset(nc_path, "r") as ds:
        if var not in ds.variables:
            raise KeyError(f"Variable '{var}' not found in {nc_path}")
        if "time" not in ds.dimensions:
            raise RuntimeError(f"No time dimension 'time' in {nc_path}")
        nt = len(ds.dimensions["time"])
        if step < 0 or step >= nt:
            raise IndexError(f"Step {step} out of range [0, {nt-1}]")
        data = ds.variables[var][step, :, :]

    return np.asarray(data, dtype=float)


def load_metadata(base_outputs_dir: str) -> Dict[str, str]:
    snap_dir = _snapshots_dir(base_outputs_dir)
    nc_files = [f for f in os.listdir(snap_dir) if f.endswith(".nc")]
    if not nc_files:
        raise FileNotFoundError(f"No NetCDF file found in {base_outputs_dir}")
    nc_path = os.path.join(snap_dir, nc_files[0])

    with netCDF4.Dataset(nc_path, "r") as ds:
        meta = {attr: getattr(ds, attr) for attr in ds.ncattrs()}

    return meta
