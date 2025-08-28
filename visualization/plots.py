from __future__ import annotations

from typing import Optional, Sequence, Tuple

import numpy as np
import matplotlib.pyplot as plt
import matplotlib.patches as patches
from matplotlib.animation import FuncAnimation, FFMpegWriter, PillowWriter
from mpl_toolkits.axes_grid1 import make_axes_locatable

from .io import load_global, list_available_steps

__all__ = ["imshow_field", "compare_fields", "animate_from_outputs"]


def add_overlays(ax, U,
                 show_minmax=False,
                 show_rankgrid=False,
                 show_rankboxes=False,
                 rank_layout=None):
    if show_minmax:
        umin, umax = float(np.nanmin(U)), float(np.nanmax(U))
        ax.text(0.99, 0.99, f"min={umin:.2f}\nmax={umax:.2f}",
                transform=ax.transAxes, ha="right", va="top",
                fontsize=8, color="white",
                bbox=dict(facecolor="black", alpha=0.5, edgecolor="none"))

    if (show_rankgrid or show_rankboxes) and rank_layout is not None:
        for (rank, x0, y0, nx, ny) in rank_layout:
            cx, cy = x0 + nx / 2, y0 + ny / 2
            if show_rankgrid:
                ax.text(cx, cy, str(rank), color="red",
                        ha="center", va="center", fontsize=8,
                        bbox=dict(facecolor="white", alpha=0.3, edgecolor="none"))
            if show_rankboxes:
                rect = patches.Rectangle(
                    (x0, y0), nx, ny,
                    linewidth=0.8,
                    edgecolor="black",
                    facecolor="none",
                    alpha=0.6,
                )
                ax.add_patch(rect)


def _imshow_with_colorbar(ax, U, cmap, vmin, vmax):
    im = ax.imshow(U, origin="lower", cmap=cmap, vmin=vmin, vmax=vmax)
    ax.set_aspect("equal")
    ax.set_xlabel("x")
    ax.set_ylabel("y")

    divider = make_axes_locatable(ax)
    cax = divider.append_axes("right", size="5%", pad=0.05)
    cb = ax.figure.colorbar(im, cax=cax)
    return im, cb


def imshow_field(
    U: np.ndarray,
    title: Optional[str] = None,
    cmap: str = "viridis",
    vmin: Optional[float] = None,
    vmax: Optional[float] = None,
    extent: Optional[Tuple[float, float, float, float]] = None,
    colorbar: bool = True,
    ax: Optional[plt.Axes] = None,
    show: bool = False,
    save: Optional[str] = None,
    overlay_minmax: bool = False,
    overlay_rankgrid: bool = False,
    overlay_rankboxes: bool = False,
    rank_layout=None,
):
    if ax is None:
        fig, ax = plt.subplots(figsize=(6, 6))
    else:
        fig = ax.figure

    im, _ = _imshow_with_colorbar(ax, U, cmap, vmin, vmax)
    if title:
        ax.set_title(title)

    add_overlays(ax, U,
                 show_minmax=overlay_minmax,
                 show_rankgrid=overlay_rankgrid,
                 show_rankboxes=overlay_rankboxes,
                 rank_layout=rank_layout)

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
    share_colorbar: bool = True,
    vmin: Optional[float] = None,
    vmax: Optional[float] = None,
    show_diff: bool = True,
    diff_cmap: str = "coolwarm",
    diff_vlim: Optional[float] = None,
    show: bool = False,
    save: Optional[str] = None,
    overlay_minmax: bool = False,
    overlay_rankboxes: bool = False,
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

    imA, _ = _imshow_with_colorbar(axA, A, cmap, vmin, vmax)
    axA.set_title(titles[0])
    add_overlays(axA, A, show_minmax=overlay_minmax,
                 show_rankboxes=overlay_rankboxes)

    imB, _ = _imshow_with_colorbar(axB, B, cmap, vmin, vmax)
    axB.set_title(titles[1])
    add_overlays(axB, B, show_minmax=overlay_minmax,
                 show_rankboxes=overlay_rankboxes)

    if show_diff:
        D = B - A
        if diff_vlim is None:
            m = np.nanmax(np.abs(D))
            diff_vlim = 1e-16 if m == 0 else m
        imD, _ = _imshow_with_colorbar(axD, D, diff_cmap,
                                       -diff_vlim, diff_vlim)
        axD.set_title("B - A")

    if save:
        fig.savefig(save, dpi=150, bbox_inches="tight")
    if show:
        plt.show()

    return fig, axes


def animate_from_outputs(
    base_outputs_dir: str,
    fmt: str = "csv",
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
    overlay_rankgrid: bool = False,
    overlay_rankboxes: bool = False,
    rank_layout=None,
):
    if steps is None:
        steps = list_available_steps(base_outputs_dir, fmt=fmt)
    if not steps:
        raise RuntimeError(
            f"No steps found in {base_outputs_dir}/snapshots for fmt='{fmt}'"
        )

    U0 = load_global(base_outputs_dir, steps[0], fmt=fmt, var=var)
    if vmin is None or vmax is None:
        vals = [U0.min(), U0.max()]
        U_last = load_global(base_outputs_dir, steps[-1], fmt=fmt, var=var)
        vals += [U_last.min(), U_last.max()]
        vmin = min(vals) if vmin is None else vmin
        vmax = max(vals) if vmax is None else vmax

    fig, ax = plt.subplots(figsize=(6, 6))
    im, _ = _imshow_with_colorbar(ax, U0, cmap, vmin, vmax)
    ttl = ax.set_title(f"{title_prefix}: {steps[0]}")

    add_overlays(ax, U0,
                 show_minmax=overlay_minmax,
                 show_rankgrid=overlay_rankgrid,
                 show_rankboxes=overlay_rankboxes,
                 rank_layout=rank_layout)

    def _update(frame_idx: int):
        step = steps[frame_idx]
        U = load_global(base_outputs_dir, step, fmt=fmt, var=var)
        im.set_data(U)
        ttl.set_text(f"{title_prefix}: {step}")
        add_overlays(ax, U,
                     show_minmax=overlay_minmax,
                     show_rankgrid=overlay_rankgrid,
                     show_rankboxes=overlay_rankboxes,
                     rank_layout=rank_layout)
        return (im, ttl)

    anim = FuncAnimation(
        fig,
        _update,
        frames=len(steps),
        interval=interval_ms,
        blit=False,
        repeat=repeat,
    )

    if save:
        if writer is None:
            writer = "ffmpeg" if save.lower().endswith(".mp4") else "pillow"
        if writer == "ffmpeg":
            try:
                anim.save(save, writer=FFMpegWriter(fps=fps, bitrate=-1))
            except Exception as e:
                raise RuntimeError(
                    "Failed to save with FFMpegWriter. Ensure ffmpeg is installed "
                    "or choose writer='pillow' and a .gif filename."
                ) from e
        elif writer == "pillow":
            anim.save(save, writer=PillowWriter(fps=fps))
        else:
            raise ValueError("writer must be 'ffmpeg' or 'pillow'")

    if show:
        plt.show()

    return anim, fig, ax
