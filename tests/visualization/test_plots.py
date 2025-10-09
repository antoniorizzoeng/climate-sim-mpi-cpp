import numpy as np
import pytest
import matplotlib.pyplot as plt
from matplotlib.animation import FuncAnimation
from visualization import plots

def test__imshow_sets_aspect_and_labels():
    fig, ax = plt.subplots()
    U = np.zeros((2, 2))
    im = plots._imshow(ax, U, cmap="viridis", vmin=0, vmax=1)
    assert im.get_array().shape == (2, 2)
    assert ax.get_xlabel() == "x"
    assert ax.get_ylabel() == "y"
    plt.close(fig)

def test__overlay_minmax_adds_text():
    fig, ax = plt.subplots()
    U = np.array([[1, 2], [3, 4]])
    plots._overlay_minmax(ax, U)
    texts = [child for child in ax.get_children() if isinstance(child, plt.Text)]
    assert any("min=1.00" in t.get_text() for t in texts)
    assert any("max=4.00" in t.get_text() for t in texts)
    plt.close(fig)

def test_imshow_field_creates_fig_and_ax():
    U = np.arange(4).reshape(2, 2)
    fig, ax = plots.imshow_field(U)
    assert fig is not None
    assert ax is not None
    plt.close(fig)

def test_imshow_field_with_title_and_overlay(tmp_path):
    U = np.arange(4).reshape(2, 2)
    out = tmp_path / "field.png"
    fig, ax = plots.imshow_field(U, title="Test", save=out, overlay_minmax=True)
    assert out.exists()
    plt.close(fig)

def test_compare_fields_basic_and_diff():
    A = np.zeros((2, 2))
    B = np.ones((2, 2))
    fig, axes = plots.compare_fields(A, B, show_diff=True)
    assert len(axes) == 3
    plt.close(fig)

def test_compare_fields_no_diff():
    A = np.zeros((2, 2))
    B = np.ones((2, 2))
    fig, axes = plots.compare_fields(A, B, show_diff=False)
    assert len(axes) == 2
    plt.close(fig)

def test_compare_fields_overlay_minmax():
    A = np.zeros((2, 2))
    B = np.ones((2, 2))
    fig, axes = plots.compare_fields(A, B, show_diff=True, overlay_minmax=True)
    plt.close(fig)

def test_compare_fields_shape_mismatch():
    A = np.zeros((2, 2))
    B = np.zeros((3, 3))
    with pytest.raises(AssertionError):
        plots.compare_fields(A, B)

def test_animate_from_outputs_runtime_error(tmp_path):
    with pytest.raises(RuntimeError):
        plots.animate_from_outputs(str(tmp_path), steps=[])

def test_animate_from_outputs_basic(monkeypatch):
    steps = [0, 1]
    frames = [np.zeros((2, 2)), np.ones((2, 2))]

    monkeypatch.setattr(plots, "list_available_steps", lambda d: steps)
    monkeypatch.setattr(plots, "load_global", lambda d, s, var="u": frames[s])

    anim, fig, ax = plots.animate_from_outputs("dummy_dir", steps=[0, 1])
    assert isinstance(anim, FuncAnimation)
    plt.close(fig)

def test_animate_from_outputs_overlay_minmax(monkeypatch):
    steps = [0]
    frame = np.array([[1, 2], [3, 4]])

    monkeypatch.setattr(plots, "list_available_steps", lambda d: steps)
    monkeypatch.setattr(plots, "load_global", lambda d, s, var="u": frame)

    anim, fig, ax = plots.animate_from_outputs("dummy_dir", steps=[0], overlay_minmax=True)
    texts = [c for c in ax.get_children() if isinstance(c, plt.Text)]
    assert any("min=1.00" in t.get_text() for t in texts)
    assert any("max=4.00" in t.get_text() for t in texts)
    plt.close(fig)

def test_animate_from_outputs_save_gif(monkeypatch, tmp_path):
    steps = [0]
    monkeypatch.setattr(plots, "list_available_steps", lambda d: steps)
    monkeypatch.setattr(plots, "load_global", lambda d, s, var="u": np.zeros((2, 2)))

    out = tmp_path / "anim.gif"
    anim, fig, ax = plots.animate_from_outputs("dummy_dir", steps=[0], save=out, writer="pillow")
    assert isinstance(anim, FuncAnimation)
    plt.close(fig)

def test_animate_from_outputs_update_overlay_text(monkeypatch):
    steps = [0, 1]
    frames = [np.zeros((2, 2)), np.ones((2, 2)) * 5]

    monkeypatch.setattr(plots, "list_available_steps", lambda d: steps)
    monkeypatch.setattr(plots, "load_global", lambda d, s, var="u": frames[s])

    anim, fig, ax = plots.animate_from_outputs("dummy_dir", steps=steps, overlay_minmax=True)
    anim._func(1)  # manually call update
    texts = [c for c in ax.get_children() if isinstance(c, plt.Text)]
    assert any("min=5.00" in t.get_text() for t in texts)
    assert any("max=5.00" in t.get_text() for t in texts)
    plt.close(fig)

def test__overlay_metadata_basic():
    fig, ax = plt.subplots()
    meta = {
        "description": "desc",
        "grid": "10x10",
        "dt": "0.1",
        "D": "2",
        "velocity": "1",
        "boundary_conditions": "left=Dirichlet right=Neumann top=Top bottom=Bottom"
    }
    plots._overlay_metadata(ax, meta)
    texts = [c.get_text() for c in ax.get_children() if isinstance(c, plt.Text)]
    assert any("desc" in t for t in texts)
    assert any("grid=10x10" in t for t in texts)
    assert any("Top" in t for t in texts)
    assert any("Bottom" in t for t in texts)
    assert any("Dirichlet" in t for t in texts)
    assert any("Neumann" in t for t in texts)
    plt.close(fig)

def test_compare_fields_diff_vlim_and_overlay_none(tmp_path):
    A = np.zeros((2, 2))
    B = np.zeros((2, 2))
    fig, axes = plots.compare_fields(A, B, show_diff=True, diff_vlim=None)
    plt.close(fig)

def test_animate_from_outputs_vmin_vmax_autocalc(monkeypatch):
    steps = [0, 1]
    frames = [np.zeros((2, 2)), np.ones((2, 2)) * 5]
    monkeypatch.setattr(plots, "list_available_steps", lambda d: steps)
    monkeypatch.setattr(plots, "load_global", lambda d, s, var="u": frames[s])
    anim, fig, ax = plots.animate_from_outputs("dummy_dir", steps=steps, vmin=None, vmax=None)
    assert isinstance(anim, FuncAnimation)
    plt.close(fig)

def test_animate_from_outputs_repeat_false(monkeypatch):
    steps = [0]
    frame = np.zeros((2, 2))
    monkeypatch.setattr(plots, "list_available_steps", lambda d: steps)
    monkeypatch.setattr(plots, "load_global", lambda d, s, var="u": frame)
    anim, fig, ax = plots.animate_from_outputs("dummy_dir", steps=steps, repeat=False)
    assert isinstance(anim, FuncAnimation)
    plt.close(fig)

def test_animate_from_outputs_save_mp4(monkeypatch, tmp_path):
    steps = [0]
    frame = np.zeros((2, 2))
    monkeypatch.setattr(plots, "list_available_steps", lambda d: steps)
    monkeypatch.setattr(plots, "load_global", lambda d, s, var="u": frame)
    out = tmp_path / "anim.mp4"
    anim, fig, ax = plots.animate_from_outputs("dummy_dir", steps=steps, save=str(out))
    assert isinstance(anim, FuncAnimation)
    plt.close(fig)

def test__overlay_metadata_empty_and_none():
    fig, ax = plt.subplots()
    plots._overlay_metadata(ax, None)
    plots._overlay_metadata(ax, {})
    plt.close(fig)

def test__overlay_metadata_bc_parsing_fallback():
    fig, ax = plt.subplots()
    meta = {"boundary_conditions": "malformed string"}
    plots._overlay_metadata(ax, meta)
    plt.close(fig)

def test_compare_fields_diff_vlim_zero_and_overlay_none():
    A = np.zeros((2, 2))
    B = np.zeros((2, 2))
    fig, axes = plots.compare_fields(A, B, show_diff=True, diff_vlim=None)
    plt.close(fig)

def test_animate_from_outputs_vmin_vmax_auto_and_text_removal(monkeypatch):
    steps = [0, 1]
    frames = [np.zeros((2, 2)), np.ones((2, 2)) * 7]
    monkeypatch.setattr(plots, "list_available_steps", lambda d: steps)
    monkeypatch.setattr(plots, "load_global", lambda d, s, var="u": frames[s])
    anim, fig, ax = plots.animate_from_outputs("dummy_dir", steps=steps, overlay_minmax=True, vmin=None, vmax=None)
    anim._func(1)
    texts = [t.get_text() for t in ax.get_children() if isinstance(t, plt.Text)]
    assert any("min=7.00" in t for t in texts)
    assert any("max=7.00" in t for t in texts)
    plt.close(fig)

def test_animate_from_outputs_save_pillow_and_ffmpeg(monkeypatch, tmp_path):
    steps = [0]
    frame = np.zeros((2, 2))
    monkeypatch.setattr(plots, "list_available_steps", lambda d: steps)
    monkeypatch.setattr(plots, "load_global", lambda d, s, var="u": frame)

    gif_out = tmp_path / "anim.gif"
    mp4_out = tmp_path / "anim.mp4"

    anim, fig, ax = plots.animate_from_outputs("dummy_dir", steps=steps, save=gif_out, writer="pillow")
    assert isinstance(anim, FuncAnimation)
    anim, fig, ax = plots.animate_from_outputs("dummy_dir", steps=steps, save=str(mp4_out), writer="ffmpeg")
    assert isinstance(anim, FuncAnimation)
    plt.close(fig)

def test_imshow_field_with_metadata_only():
    fig, ax = plt.subplots()
    meta = {"description": "test description", "grid": "5x5"}
    U = np.zeros((2, 2))
    fig, ax = plots.imshow_field(U, metadata=meta)
    texts = [t.get_text() for t in ax.get_children() if isinstance(t, plt.Text)]
    assert any("test description" in t for t in texts)
    plt.close(fig)
