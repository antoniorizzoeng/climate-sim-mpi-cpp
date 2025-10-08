from typing import Optional, Sequence, Tuple, Dict
import numpy as np
import matplotlib.pyplot as plt
from matplotlib.animation import FuncAnimation, FFMpegWriter, PillowWriter

from .io import load_global, list_available_steps


def _imshow(ax: plt.Axes, U: np.ndarray, cmap: str, vmin: Optional[float], vmax: Optional[float]):
    im = ax.imshow(U, origin="lower", cmap=cmap, vmin=vmin, vmax=vmax)
    ax.set_aspect("equal")
    ax.set_xlabel("x")
    ax.set_ylabel("y")
    return im


def _overlay_minmax(ax: plt.Axes, U: np.ndarray) -> None:
    umin, umax = float(np.nanmin(U)), float(np.nanmax(U))
    ax.text(
        0.99, 0.99, f"min={umin:.2f}\nmax={umax:.2f}",
        transform=ax.transAxes, ha="right", va="top",
        fontsize=8, color="white",
        bbox=dict(facecolor="black", alpha=0.5, edgecolor="none")
    )


def _overlay_metadata(ax: plt.Axes, metadata: Dict[str, str]) -> None:
    if not metadata:
        return

    desc = metadata.get("description", "")
    grid = metadata.get("grid", "")
    dt = metadata.get("dt", "")
    D = metadata.get("D", "")
    velocity = metadata.get("velocity", "")
    subtitle_parts = []
    if grid: subtitle_parts.append(f"grid={grid}")
    if dt: subtitle_parts.append(f"dt={dt}")
    if D: subtitle_parts.append(f"D={D}")
    if velocity: subtitle_parts.append(f"v={velocity}")
    subtitle = " | ".join(subtitle_parts)

    if desc or subtitle:
        ax.text(
            0.5, 0.96,
            f"{desc}\n{subtitle}",
            transform=ax.transAxes,
            ha="center", va="top",
            fontsize=9, color="gray",
        )

    bc_text = metadata.get("boundary_conditions", "")
    if not bc_text:
        return

    try:
        bcs = {kv.split("=")[0]: kv.split("=")[1] for kv in bc_text.split()}
    except Exception:
        bcs = {}

    style = dict(fontsize=9, color="black")

    if "left" in bcs:
        label = bcs["left"]
        ax.text(-0.12, 0.5, label,
                transform=ax.transAxes, rotation=90,
                ha="right", va="center", **style)
    if "right" in bcs:
        label = bcs["right"]
        ax.text(1.12, 0.5, label,
                transform=ax.transAxes, rotation=-90,
                ha="left", va="center", **style)
    if "bottom" in bcs:
        label = bcs["bottom"]
        ax.text(0.5, -0.12, label,
                transform=ax.transAxes,
                ha="center", va="top", **style)
    if "top" in bcs:
        label = bcs["top"]
        ax.text(0.5, 1.10, label,
                transform=ax.transAxes,
                ha="center", va="bottom", **style)

def imshow_field(
    U: np.ndarray,
    title: Optional[str] = None,
    cmap: str = "viridis",
    vmin: Optional[float] = None,
    vmax: Optional[float] = None,
    ax: Optional[plt.Axes] = None,
    show: bool = False,
    save: Optional[str] = None,
    overlay_minmax: bool = False,
    metadata: Optional[Dict[str, str]] = None,
):
    if ax is None:
        fig, ax = plt.subplots(figsize=(6, 6))
    else:
        fig = ax.figure

    _imshow(ax, U, cmap, vmin, vmax)

    if title:
        ax.set_title(title)

    if overlay_minmax:
        _overlay_minmax(ax, U)

    if metadata:
        _overlay_metadata(ax, metadata)

    if save:
        fig.savefig(save, dpi=150, bbox_inches="tight")
    if show:
        plt.show()

    return fig, ax


def compare_fields(
    A: np.ndarray,
    B: np.ndarray,
    titles: Tuple[str, str] = ("A", "B"),
    cmap: str = "viridis",
    vmin: Optional[float] = None,
    vmax: Optional[float] = None,
    show_diff: bool = True,
    diff_cmap: str = "coolwarm",
    diff_vlim: Optional[float] = None,
    show: bool = False,
    save: Optional[str] = None,
    overlay_minmax: bool = False,
    metadata_a: Optional[Dict[str, str]] = None,
    metadata_b: Optional[Dict[str, str]] = None,
):
    assert A.shape == B.shape, "Fields must have the same shape"

    if vmin is None or vmax is None:
        vmin = np.nanmin([A.min(), B.min()]) if vmin is None else vmin
        vmax = np.nanmax([A.max(), B.max()]) if vmax is None else vmax

    ncols = 3 if show_diff else 2
    fig, axes = plt.subplots(1, ncols, figsize=(6 * ncols, 6))

    if ncols == 2:
        axA, axB = axes
    else:
        axA, axB, axD = axes

    _imshow(axA, A, cmap, vmin, vmax)
    axA.set_title(titles[0])
    if overlay_minmax:
        _overlay_minmax(axA, A)
    if metadata_a:
        _overlay_metadata(axA, metadata_a)

    _imshow(axB, B, cmap, vmin, vmax)
    axB.set_title(titles[1])
    if overlay_minmax:
        _overlay_minmax(axB, B)
    if metadata_b:
        _overlay_metadata(axB, metadata_b)

    if show_diff:
        D = B - A
        if diff_vlim is None:
            m = np.nanmax(np.abs(D))
            diff_vlim = 1e-16 if m == 0 else m
        _imshow(axD, D, diff_cmap, -diff_vlim, diff_vlim)
        axD.set_title("B - A")

    if save:
        fig.savefig(save, dpi=150, bbox_inches="tight")
    if show:
        plt.show()

    return fig, axes


def animate_from_outputs(
    base_outputs_dir: str,
    var: str = "u",
    steps: Optional[Sequence[int]] = None,
    interval_ms: int = 150,
    fps: int = 12,
    repeat: bool = True,
    cmap: str = "viridis",
    vmin: Optional[float] = None,
    vmax: Optional[float] = None,
    save: Optional[str] = None,
    writer: Optional[str] = None,
    title_prefix: str = "timestep",
    show: bool = False,
    overlay_minmax: bool = False,
    metadata: Optional[Dict[str, str]] = None,
):
    if steps is None:
        steps = list_available_steps(base_outputs_dir)
    if not steps:
        raise RuntimeError(f"No steps found in {base_outputs_dir}")

    first = load_global(base_outputs_dir, steps[0], var=var)
    last = load_global(base_outputs_dir, steps[-1], var=var)
    if vmin is None:
        vmin = min(first.min(), last.min())
    if vmax is None:
        vmax = max(first.max(), last.max())

    fig, ax = plt.subplots(figsize=(6, 6))
    im = _imshow(ax, first, cmap, vmin, vmax)
    ttl = ax.set_title(f"{title_prefix}: {steps[0]}")

    if overlay_minmax:
        _overlay_minmax(ax, first)
    if metadata:
        _overlay_metadata(ax, metadata)

    def _update(frame_idx: int):
        step = steps[frame_idx]
        U = load_global(base_outputs_dir, step, var=var)
        im.set_data(U)
        ttl.set_text(f"{title_prefix}: {step}")

        for txt in ax.texts[:]:
            txt.remove()

        if overlay_minmax:
            _overlay_minmax(ax, U)
        if metadata:
            _overlay_metadata(ax, metadata)

        return [im]


    anim = FuncAnimation(fig, _update, frames=len(steps),
                         interval=interval_ms, blit=False, repeat=repeat)

    if save:
        if writer is None:
            writer = "ffmpeg" if save.lower().endswith(".mp4") else "pillow"
        if writer == "ffmpeg":
            anim.save(save, writer=FFMpegWriter(fps=fps, bitrate=-1))
        else:
            anim.save(save, writer=PillowWriter(fps=fps))

    if show:
        plt.show()

    return anim, fig, ax

