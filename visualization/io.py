from __future__ import annotations

import os
import re
import csv
from dataclasses import dataclass
from typing import List

import numpy as np

try:
    import netCDF4
    _HAS_NETCDF4 = True
except Exception:
    _HAS_NETCDF4 = False


@dataclass(frozen=True)
class RankTile:
    rank: int
    x_off: int
    y_off: int
    nx: int
    ny: int
    halo: int
    nxg: int
    nyg: int


def _read_rank_layout(path: str) -> List[RankTile]:
    if not os.path.exists(path):
        raise FileNotFoundError(f"rank layout file not found: {path}")

    tiles: List[RankTile] = []
    with open(path, "r", newline="") as f:
        r = csv.reader(f)
        header = next(r, None)
        for row in r:
            if not row or all(not c.strip() for c in row):
                continue
            rank, x_off, y_off, nx, ny, halo, nxg, nyg = map(int, row[:8])
            tiles.append(RankTile(rank, x_off, y_off, nx, ny, halo, nxg, nyg))

    if not tiles:
        raise RuntimeError(f"empty rank layout: {path}")
    nxg = tiles[0].nxg
    nyg = tiles[0].nyg
    for t in tiles:
        if t.nxg != nxg or t.nyg != nyg:
            raise RuntimeError("inconsistent global sizes in rank_layout.csv")
    return tiles


def _snapshots_dir(base_outputs_dir: str) -> str:
    cand = os.path.join(base_outputs_dir, "snapshots")
    if not os.path.isdir(cand):
        raise FileNotFoundError(f"snapshot directory not found: {cand}")
    return cand


def _list_snapshot_files(snap_dir: str, step: int, ext: str) -> List[str]:
    pat = re.compile(rf"^snapshot_{step:05d}_rank\d{{5}}\.{re.escape(ext)}$")
    files = sorted(f for f in os.listdir(snap_dir) if pat.match(f))
    if not files:
        raise FileNotFoundError(f"no snapshots for step={step} in {snap_dir} (*.{ext})")
    return [os.path.join(snap_dir, f) for f in files]


def _read_csv_tile(path: str) -> np.ndarray:
    try:
        arr = np.loadtxt(path, delimiter=",")
    except Exception as e:
        raise RuntimeError(f"failed to read CSV tile {path}: {e}") from e
    if arr.ndim != 2:
        raise RuntimeError(f"CSV tile not 2D: {path}")
    return np.asarray(arr, dtype=float)


def _read_nc_tile(path: str, var: str) -> np.ndarray:
    if not _HAS_NETCDF4:
        raise ImportError("netCDF4 not installed; cannot read NetCDF snapshots")
    try:
        with netCDF4.Dataset(path, "r") as ds:
            if var not in ds.variables:
                raise KeyError(
                    f"variable '{var}' not found in {path}. "
                    f"available: {list(ds.variables.keys())}"
                )
            arr = ds.variables[var][:]
    except Exception as e:
        raise RuntimeError(f"failed to read NetCDF tile {path}: {e}") from e
    arr = np.asarray(arr)
    if arr.ndim != 2:
        raise RuntimeError(f"NetCDF variable '{var}' in {path} is not 2D")
    return arr.astype(float, copy=False)


def _place_tile(U: np.ndarray, tile: np.ndarray, t: RankTile) -> None:
    ny_csv, nx_csv = tile.shape
    expect_no_halo = (t.ny, t.nx)
    expect_with_halo = (t.ny + 2 * t.halo, t.nx + 2 * t.halo)

    if (ny_csv, nx_csv) == expect_no_halo:
        off = 0
    elif (ny_csv, nx_csv) == expect_with_halo:
        off = t.halo
    else:
        raise RuntimeError(
            f"tile shape mismatch for rank {t.rank}: got {tile.shape}, "
            f"expected {expect_no_halo} or {expect_with_halo}"
        )

    U[t.y_off : t.y_off + t.ny, t.x_off : t.x_off + t.nx] = \
        tile[off : off + t.ny, off : off + t.nx]


def assemble_global_csv(base_outputs_dir: str, step: int) -> np.ndarray:
    tiles = _read_rank_layout(os.path.join(base_outputs_dir, "rank_layout.csv"))
    snap_dir = _snapshots_dir(base_outputs_dir)
    files = _list_snapshot_files(snap_dir, step, "csv")

    U = np.zeros((tiles[0].nyg, tiles[0].nxg), dtype=float)
    by_rank = {}
    for f in files:
        m = re.search(r"_rank(\d{5})\.csv$", f)
        if m:
            by_rank[int(m.group(1))] = f

    for t in tiles:
        path = by_rank.get(t.rank)
        if path is None:
            raise FileNotFoundError(f"missing CSV tile for rank {t.rank} at step {step}")
        tile = _read_csv_tile(path)
        _place_tile(U, tile, t)

    return U


def assemble_global_netcdf(base_outputs_dir: str, step: int, var: str = "u") -> np.ndarray:
    tiles = _read_rank_layout(os.path.join(base_outputs_dir, "rank_layout.csv"))
    snap_dir = _snapshots_dir(base_outputs_dir)
    files = _list_snapshot_files(snap_dir, step, "nc")

    U = np.zeros((tiles[0].nyg, tiles[0].nxg), dtype=float)
    by_rank = {}
    for f in files:
        m = re.search(r"_rank(\d{5})\.nc$", f)
        if m:
            by_rank[int(m.group(1))] = f

    for t in tiles:
        path = by_rank.get(t.rank)
        if path is None:
            raise FileNotFoundError(f"missing NetCDF tile for rank {t.rank} at step {step}")
        tile = _read_nc_tile(path, var)
        _place_tile(U, tile, t)

    return U


def list_available_steps(base_outputs_dir: str, fmt: str = "csv") -> List[int]:
    snap_dir = _snapshots_dir(base_outputs_dir)
    if fmt not in ("csv", "nc"):
        raise ValueError("fmt must be 'csv' or 'nc'")
    pat = re.compile(rf"^snapshot_(\d{{5}})_rank\d{{5}}\.(?:{fmt})$")
    steps = set()
    for name in os.listdir(snap_dir):
        m = pat.match(name)
        if m:
            steps.add(int(m.group(1)))
    return sorted(steps)


def load_global(base_outputs_dir: str, step: int, fmt: str = "csv", var: str = "u") -> np.ndarray:
    if fmt == "csv":
        return assemble_global_csv(base_outputs_dir, step)
    elif fmt == "nc":
        return assemble_global_netcdf(base_outputs_dir, step, var=var)
    else:
        raise ValueError("fmt must be 'csv' or 'nc'")
