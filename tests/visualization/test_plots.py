import os
import numpy as np
import pytest

import matplotlib.pyplot as plt

from visualization.plots import (
    add_overlays,
    _imshow_with_colorbar,
    imshow_field,
    compare_fields,
    animate_from_outputs,
)
from visualization.io import assemble_global_csv
from tests.visualization.sample_data import (
    make_tiles_nx_ny,
    write_rank_layout,
    write_csv_snapshots,
)

def _seed_csv_case(base_outputs_dir, step=0, nxg=8, nyg=6):
    tiles = make_tiles_nx_ny(nxg, nyg, halo=1)
    write_rank_layout(base_outputs_dir, tiles)

    def field_fn(t):
        ny, nx = t["ny"], t["nx"]
        y0, x0 = t["y_off"], t["x_off"]
        yy = np.linspace(0, 1, ny)[:, None]
        xx = np.linspace(0, 1, nx)[None, :]
        return np.sin(2 * np.pi * (xx + yy))

    write_csv_snapshots(base_outputs_dir, step=step, tiles=tiles, field_fn=field_fn)
    return nxg, nyg

def test_add_overlays_minmax_and_rankgrid(tmp_path):
    fig, ax = plt.subplots()
    U = np.arange(9).reshape(3, 3)
    rank_layout = [(0, 0, 0, 3, 3)]
    add_overlays(ax, U, show_minmax=True,
                 show_rankgrid=True, show_rankboxes=True,
                 rank_layout=rank_layout)
    # Should have added both text and rectangle
    assert any(isinstance(child, plt.Text) for child in ax.get_children())
    assert any(isinstance(child, plt.Rectangle) for child in ax.patches)

def test_imshow_field_with_overlays_and_show(monkeypatch, tmp_outputs_dir):
    _seed_csv_case(tmp_outputs_dir, step=0)
    U = assemble_global_csv(tmp_outputs_dir, 0)

    called = {}
    monkeypatch.setattr(plt, "show", lambda: called.setdefault("shown", True))

    fig, ax = imshow_field(
        U,
        title="demo",
        overlay_minmax=True,
        overlay_rankgrid=True,
        overlay_rankboxes=True,
        rank_layout=[(0, 0, 0, U.shape[1], U.shape[0])],
        show=True,
    )
    assert "shown" in called
    plt.close(fig)

def test__imshow_with_colorbar_returns_im_and_cb():
    fig, ax = plt.subplots()
    U = np.ones((4, 4))
    im, cb = _imshow_with_colorbar(ax, U, "viridis", None, None)
    assert im.get_array().shape == (4, 4)
    assert cb.ax in fig.axes
    plt.close(fig)

def test_compare_fields_with_diff_and_overlays():
    A = np.zeros((4, 4))
    B = np.ones((4, 4))
    fig, axes = compare_fields(A, B,
                               titles=("A", "B"),
                               show_diff=True,
                               overlay_minmax=True,
                               overlay_rankboxes=True)
    assert len(axes) == 3
    plt.close(fig)

def test_compare_fields_no_diff_split_colorbar():
    A = np.zeros((2, 2))
    B = np.ones((2, 2))
    fig, axes = compare_fields(A, B,
                               show_diff=False,
                               share_colorbar=False)
    assert len(axes) == 2
    plt.close(fig)

def test_animate_from_outputs_runtime_error(tmp_path):
    with pytest.raises(RuntimeError):
        animate_from_outputs(str(tmp_path), fmt="csv", steps=[])



