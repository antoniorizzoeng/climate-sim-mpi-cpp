from __future__ import annotations

from typing import Optional, Sequence, Tuple

import numpy as np
import matplotlib.pyplot as plt
from matplotlib.animation import FuncAnimation, FFMpegWriter, PillowWriter

from .io import load_global, list_available_steps

__all__ = ["save_frame", "save_animation"]

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
):
    if ax is None:
        fig, ax = plt.subplots(figsize=(6, 4), constrained_layout=True)
    else:
        fig = ax.figure

    im = ax.imshow(U, origin="lower", cmap=cmap, vmin=vmin, vmax=vmax, extent=extent)
    ax.set_aspect("equal")
    ax.set_xlabel("x")
    ax.set_ylabel("y")
    if title:
        ax.set_title(title)
    if colorbar:
        fig.colorbar(im, ax=ax, shrink=0.85)

    if save:
        fig.savefig(save, dpi=150)
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
):
    assert A.shape == B.shape, "Fields must have the same shape"

    if vmin is None or vmax is None:
        vmin = np.nanmin([A.min(), B.min()]) if vmin is None else vmin
        vmax = np.nanmax([A.max(), B.max()]) if vmax is None else vmax

    ncols = 3 if show_diff else 2
    fig, axes = plt.subplots(1, ncols, figsize=(4 * ncols + 1, 4), constrained_layout=True)

    if ncols == 2:
        axA, axB = axes
    else:
        axA, axB, axD = axes

    imA = axA.imshow(A, origin="lower", cmap=cmap, vmin=vmin, vmax=vmax)
    axA.set_aspect("equal")
    axA.set_title(titles[0])
    axA.set_xlabel("x")
    axA.set_ylabel("y")

    imB = axB.imshow(B, origin="lower", cmap=cmap, vmin=vmin, vmax=vmax)
    axB.set_aspect("equal")
    axB.set_title(titles[1])
    axB.set_xlabel("x")
    axB.set_ylabel("y")

    if share_colorbar:
        cbar = fig.colorbar(imB, ax=[axA, axB], shrink=0.85)
        cbar.set_label("value")
    else:
        fig.colorbar(imA, ax=axA, shrink=0.85)
        fig.colorbar(imB, ax=axB, shrink=0.85)

    if show_diff:
        D = B - A
        if diff_vlim is None:
            m = np.nanmax(np.abs(D))
            diff_vlim = 1e-16 if m == 0 else m
        imD = axD.imshow(D, origin="lower", cmap=diff_cmap, vmin=-diff_vlim, vmax=diff_vlim)
        axD.set_aspect("equal")
        axD.set_title("B - A")
        axD.set_xlabel("x")
        axD.set_ylabel("y")
        fig.colorbar(imD, ax=axD, shrink=0.85)

    if save:
        fig.savefig(save, dpi=150)
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
):
    if steps is None:
        steps = list_available_steps(base_outputs_dir, fmt=fmt)
    if not steps:
        raise RuntimeError(f"No steps found in {base_outputs_dir}/snapshots for fmt='{fmt}'")

    U0 = load_global(base_outputs_dir, steps[0], fmt=fmt, var=var)
    if vmin is None or vmax is None:
        vals = [U0.min(), U0.max()]
        U_last = load_global(base_outputs_dir, steps[-1], fmt=fmt, var=var)
        vals += [U_last.min(), U_last.max()]
        vmin = min(vals) if vmin is None else vmin
        vmax = max(vals) if vmax is None else vmax

    fig, ax = plt.subplots(figsize=(6, 4), constrained_layout=True)
    im = ax.imshow(U0, origin="lower", cmap=cmap, vmin=vmin, vmax=vmax)
    ax.set_aspect("equal")
    ax.set_xlabel("x")
    ax.set_ylabel("y")
    ttl = ax.set_title(f"{title_prefix}: {steps[0]}")

    fig.colorbar(im, ax=ax, shrink=0.85)

    def _update(frame_idx: int):
        step = steps[frame_idx]
        U = load_global(base_outputs_dir, step, fmt=fmt, var=var)
        im.set_data(U)
        ttl.set_text(f"{title_prefix}: {step}")
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

    return anim, fig, ax
