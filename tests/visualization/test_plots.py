import numpy as np
import pytest

import matplotlib.pyplot as plt
from visualization import plots

def test_add_overlays_minmax_and_rankgrid(tmp_path):
    fig, ax = plt.subplots()
    U = np.arange(9).reshape(3, 3)
    rank_layout = [(0, 0, 0, 3, 3)]
    plots.add_overlays(ax, U, show_minmax=True,
                 show_rankgrid=True, show_rankboxes=True,
                 rank_layout=rank_layout)
    assert any(isinstance(child, plt.Text) for child in ax.get_children())
    assert any(isinstance(child, plt.Rectangle) for child in ax.patches)

def test__imshow_with_colorbar_returns_im_and_cb():
    fig, ax = plt.subplots()
    U = np.ones((4, 4))
    im, cb = plots._imshow_with_colorbar(ax, U, "viridis", None, None)
    assert im.get_array().shape == (4, 4)
    assert cb.ax in fig.axes
    plt.close(fig)

def test_compare_fields_with_diff_and_overlays():
    A = np.zeros((4, 4))
    B = np.ones((4, 4))
    fig, axes = plots.compare_fields(A, B,
                               titles=("A", "B"),
                               show_diff=True,
                               overlay_minmax=True,
                               overlay_rankboxes=True)
    assert len(axes) == 3
    plt.close(fig)

def test_animate_from_outputs_runtime_error(tmp_path):
    with pytest.raises(RuntimeError):
        plots.animate_from_outputs(str(tmp_path), steps=[])

def test_add_overlays_no_rank_layout():
    fig, ax = plt.subplots()
    U = np.zeros((2, 2))
    plots.add_overlays(ax, U, show_rankgrid=True, rank_layout=None)
    plt.close(fig)

def test_compare_fields_autoscale_and_save_show(monkeypatch, tmp_path):
    A = np.zeros((3, 3))
    B = np.ones((3, 3))
    B[:] = A[:]
    monkeypatch.setattr(plt, "show", lambda: None)
    out = tmp_path / "cmp.png"
    fig, axes = plots.compare_fields(A, B, save=out, show=True)
    assert out.exists()
    plt.close(fig)

def test_add_overlays_with_rankgrid_no_layout():
    fig, ax = plt.subplots()
    U = np.zeros((2, 2))
    plots.add_overlays(ax, U, show_rankgrid=True, rank_layout=None)
    plt.close(fig)

def test_compare_fields_autoscale_and_diff_vlim_zero(monkeypatch):
    A = np.zeros((4, 4))
    B = np.zeros((4, 4))
    monkeypatch.setattr(plt, "show", lambda: None)
    fig, axes = plots.compare_fields(A, B, show_diff=True)
    plt.close(fig)

def test_imshow_field_with_title_save_and_show(monkeypatch, tmp_path):
    monkeypatch.setattr(plt, "show", lambda: None)
    U = np.arange(16).reshape(4, 4)
    out = tmp_path / "field.png"
    fig, ax = plots.imshow_field(
        U,
        title="test",
        save=out,
        show=True,
        overlay_minmax=True,
        overlay_rankgrid=True,
        overlay_rankboxes=True,
        rank_layout=[(0, 0, 0, 4, 4)],
    )
    assert out.exists()
    plt.close(fig)


def test_compare_fields_assertion_error():
    A = np.zeros((2, 2))
    B = np.zeros((3, 3))
    with pytest.raises(AssertionError):
        plots.compare_fields(A, B)


def test_animate_from_outputs_value_error_writer(monkeypatch, tmp_path):
    monkeypatch.setattr(plots, "list_available_steps", lambda d: [0])
    monkeypatch.setattr(
        plots, "load_global", lambda d, s, var="u": np.zeros((2, 2))
    )
    with pytest.raises(ValueError):
        plots.animate_from_outputs(str(tmp_path), save="out.xxx", writer="bad")


def test_animate_from_outputs_ffmpeg_failure(monkeypatch, tmp_path):
    monkeypatch.setattr(plots, "list_available_steps", lambda d: [0])
    monkeypatch.setattr(
        plots, "load_global", lambda d, s, var="u": np.zeros((2, 2))
    )

    class DummyAnim:
        def save(self, *a, **k):
            raise RuntimeError("ffmpeg missing")

    monkeypatch.setattr(plots, "FuncAnimation", lambda *a, **k: DummyAnim())

    with pytest.raises(RuntimeError):
        plots.animate_from_outputs(str(tmp_path), save="out.mp4", writer="ffmpeg")


def test_animate_from_outputs_with_pillow(monkeypatch, tmp_path):
    monkeypatch.setattr(plots, "list_available_steps", lambda d: [0])
    monkeypatch.setattr(
        plots, "load_global", lambda d, s, var="u": np.zeros((2, 2))
    )

    class DummyAnim:
        def save(self, *a, **k):
            return None

    monkeypatch.setattr(plots, "FuncAnimation", lambda *a, **k: DummyAnim())
    anim, fig, ax = plots.animate_from_outputs(
        str(tmp_path), save="out.gif", writer="pillow"
    )
    assert fig is not None
    plt.close(fig)


def test_animate_from_outputs_with_show(monkeypatch, tmp_path):
    monkeypatch.setattr(plots, "list_available_steps", lambda d: [0, 1])
    monkeypatch.setattr(
        plots,
        "load_global",
        lambda d, s, var="u": np.ones((2, 2)) * s,
    )
    monkeypatch.setattr(plt, "show", lambda: None)
    anim, fig, ax = plots.animate_from_outputs(
        str(tmp_path), show=True, steps=[0, 1]
    )
    assert fig is not None
    plt.close(fig)

def test_add_overlays_rankgrid_no_boxes():
    fig, ax = plt.subplots()
    U = np.zeros((2, 2))
    rank_layout = [(0, 0, 0, 2, 2)]
    plots.add_overlays(ax, U, show_rankgrid=True, show_rankboxes=False, rank_layout=rank_layout)
    plt.close(fig)


def test__imshow_with_colorbar_with_vmin_vmax():
    fig, ax = plt.subplots()
    U = np.arange(4).reshape(2, 2)
    im, cb = plots._imshow_with_colorbar(ax, U, "viridis", vmin=0, vmax=1)
    assert cb.ax in fig.axes
    plt.close(fig)


def test_imshow_field_with_existing_ax_and_save(tmp_path):
    fig, ax = plt.subplots()
    U = np.ones((3, 3))
    out = tmp_path / "field.png"
    fig2, ax2 = plots.imshow_field(U, ax=ax, save=out)
    assert out.exists()
    assert ax2 is ax
    plt.close(fig2)


def test_compare_fields_no_diff(monkeypatch):
    A = np.zeros((3, 3))
    B = np.ones((3, 3))
    monkeypatch.setattr(plt, "show", lambda: None)
    fig, axes = plots.compare_fields(A, B, show_diff=False)
    assert len(axes) == 2
    plt.close(fig)


def test_animate_from_outputs_steps_none(monkeypatch, tmp_path):
    monkeypatch.setattr(plots, "list_available_steps", lambda d: [0, 1])
    monkeypatch.setattr(
        plots, "load_global", lambda d, s, var="u": np.ones((2, 2)) * s
    )
    anim, fig, ax = plots.animate_from_outputs(str(tmp_path), steps=None)
    assert fig is not None
    plt.close(fig)


def test_animate_from_outputs_writer_auto_ffmpeg(monkeypatch, tmp_path):
    monkeypatch.setattr(plots, "list_available_steps", lambda d: [0])
    monkeypatch.setattr(
        plots, "load_global", lambda d, s, var="u": np.zeros((2, 2))
    )

    class DummyAnim:
        def save(self, *a, **k): return None

    monkeypatch.setattr(plots, "FuncAnimation", lambda *a, **k: DummyAnim())
    anim, fig, ax = plots.animate_from_outputs(str(tmp_path), save="video.mp4", writer=None)
    assert fig is not None
    plt.close(fig)


def test_animate_from_outputs_writer_auto_pillow(monkeypatch, tmp_path):
    monkeypatch.setattr(plots, "list_available_steps", lambda d: [0])
    monkeypatch.setattr(
        plots, "load_global", lambda d, s, var="u": np.zeros((2, 2))
    )

    class DummyAnim:
        def save(self, *a, **k): return None

    monkeypatch.setattr(plots, "FuncAnimation", lambda *a, **k: DummyAnim())
    anim, fig, ax = plots.animate_from_outputs(str(tmp_path), save="anim.gif", writer=None)
    assert fig is not None
    plt.close(fig)
