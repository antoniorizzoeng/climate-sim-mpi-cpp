import os
import numpy as np

from visualization.io import assemble_global_csv
from visualization.plots import imshow_field, animate_from_outputs
from tests.visualization.sample_data import (
    make_tiles_nx_ny,
    write_rank_layout,
    write_csv_snapshots,
)

def _seed_csv_case(base_outputs_dir, step=0, nxg=20, nyg=12):
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

def test_imshow_field_saves_png(tmp_outputs_dir):
    _seed_csv_case(tmp_outputs_dir, step=0)
    U = assemble_global_csv(tmp_outputs_dir, 0)
    out = os.path.join(tmp_outputs_dir, "frame.png")
    imshow_field(U, title="test", save=out)
    assert os.path.isfile(out)
    assert os.path.getsize(out) > 0

def test_animate_from_outputs_saves_gif(tmp_outputs_dir):
    steps = [0, 1, 2]
    _seed_csv_case(tmp_outputs_dir, step=0)
    _seed_csv_case(tmp_outputs_dir, step=1)
    _seed_csv_case(tmp_outputs_dir, step=2)

    out = os.path.join(tmp_outputs_dir, "anim.gif")
    animate_from_outputs(
        tmp_outputs_dir,
        fmt="csv",
        steps=steps,
        fps=8,
        save=out,
        writer="pillow"
    )
    assert os.path.isfile(out)
    assert os.path.getsize(out) > 0
