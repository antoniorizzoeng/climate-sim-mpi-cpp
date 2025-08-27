from __future__ import annotations

import argparse
from typing import Optional, Sequence

from .io import load_global, list_available_steps
from .plots import imshow_field, compare_fields, animate_from_outputs


def _parse_steps_arg(steps_arg: Optional[str], avail: Sequence[int]) -> Sequence[int]:
    if steps_arg is None:
        return avail
    s = steps_arg.strip()
    if "-" in s:
        a, b = s.split("-", 1)
        a = int(a) if a else (avail[0] if avail else 0)
        b = int(b) if b else (avail[-1] if avail else a)
        return [k for k in avail if a <= k <= b]
    return [int(x) for x in s.split(",") if x.strip()]


def cmd_show(args: argparse.Namespace) -> None:
    steps = list_available_steps(args.dir, fmt=args.fmt)
    if not steps:
        raise SystemExit(f"No snapshots found in {args.dir}/snapshots for fmt={args.fmt!r}")
    step = args.step if args.step is not None else steps[-1]
    U = load_global(args.dir, step, fmt=args.fmt, var=args.var)
    imshow_field(
        U,
        title=args.title or f"{args.dir} :: step {step}",
        cmap=args.cmap,
        vmin=args.vmin,
        vmax=args.vmax,
        colorbar=not args.no_colorbar,
        show=args.show,
        save=args.save,
    )


def cmd_compare(args: argparse.Namespace) -> None:
    Ua = load_global(args.dir_a, args.step, fmt=args.fmt_a, var=args.var_a)
    Ub = load_global(args.dir_b, args.step, fmt=args.fmt_b, var=args.var_b)
    compare_fields(
        Ua,
        Ub,
        titles=(args.title_a or "A", args.title_b or "B"),
        cmap=args.cmap,
        share_colorbar=not args.split_colorbar,
        vmin=args.vmin,
        vmax=args.vmax,
        show_diff=not args.no_diff,
        diff_cmap=args.diff_cmap,
        diff_vlim=args.diff_vlim,
        show=args.show,
        save=args.save,
    )


def cmd_animate(args: argparse.Namespace) -> None:
    avail = list_available_steps(args.dir, fmt=args.fmt)
    if not avail:
        raise SystemExit(f"No snapshots found in {args.dir}/snapshots for fmt={args.fmt!r}")
    if args.steps:
        sel = _parse_steps_arg(args.steps, avail)
    else:
        sel = avail
        if args.start is not None or args.end is not None or args.stride is not None:
            a = args.start if args.start is not None else avail[0]
            b = args.end if args.end is not None else avail[-1]
            stride = args.stride if args.stride is not None else 1
            sel = [k for k in avail if a <= k <= b][::stride]
    animate_from_outputs(
        args.dir,
        fmt=args.fmt,
        var=args.var,
        steps=sel,
        interval_ms=args.interval,
        fps=args.fps,
        repeat=not args.no_repeat,
        cmap=args.cmap,
        vmin=args.vmin,
        vmax=args.vmax,
        save=args.save,
        writer=args.writer,
        title_prefix=args.title_prefix,
    )

def cmd_watch(args):
    import time
    import matplotlib.pyplot as plt
    from . import io

    base = args.dir
    fmt = args.fmt
    var = args.var
    interval = args.interval

    plt.ion()
    fig, ax = plt.subplots()
    im = None
    cbar = None
    last_step = None

    vmin = args.vmin
    vmax = args.vmax

    try:
        while True:
            try:
                steps = io.list_available_steps(base, fmt=fmt)
            except FileNotFoundError:
                time.sleep(interval)
                continue

            if not steps:
                time.sleep(interval)
                continue

            step = steps[-1]
            if step == last_step and not args.redraw_same:
                time.sleep(interval)
                continue

            U = io.load_global(base, step, fmt=fmt, var=var)

            if im is None:
                im = ax.imshow(U, origin="lower", vmin=vmin, vmax=vmax, cmap=args.cmap)
                ax.set_title(args.title or f"Step {step}")
                if not args.no_colorbar:
                    cbar = fig.colorbar(im, ax=ax)
            else:
                im.set_data(U)
                if vmin is None or vmax is None:
                    im.set_clim(vmin=vmin, vmax=vmax)
                ax.set_title(args.title or f"Step {step}")

            if args.tight:
                fig.tight_layout()

            fig.canvas.draw()
            fig.canvas.flush_events()

            last_step = step
            time.sleep(interval)
    except KeyboardInterrupt:
        pass

def cmd_interactive(args):
    import matplotlib.pyplot as plt
    from . import io

    steps = io.list_available_steps(args.dir, fmt=args.fmt)
    if not steps:
        raise SystemExit("No snapshots found")

    idx = 0
    U = io.load_global(args.dir, steps[idx], fmt=args.fmt, var=args.var)

    fig, ax = plt.subplots()
    im = ax.imshow(U, origin="lower", cmap=args.cmap)
    ttl = ax.set_title(f"Step {steps[idx]}")

    def on_key(event):
        nonlocal idx
        if event.key == "right":
            idx = min(idx + 1, len(steps) - 1)
        elif event.key == "left":
            idx = max(idx - 1, 0)
        U = io.load_global(args.dir, steps[idx], fmt=args.fmt, var=args.var)
        im.set_data(U)
        ttl.set_text(f"Step {steps[idx]}")
        fig.canvas.draw_idle()

    fig.canvas.mpl_connect("key_press_event", on_key)
    plt.show()

def build_parser() -> argparse.ArgumentParser:
    p = argparse.ArgumentParser(
        prog="climate-vis",
        description="Quick CLI for visualizing climate_sim outputs (CSV/NetCDF).",
    )
    sub = p.add_subparsers(dest="cmd", required=True)

    # show
    ps = sub.add_parser("show", help="Render a single snapshot")
    ps.add_argument("--dir", required=True, help="Base outputs directory (contains snapshots/)")
    ps.add_argument("--fmt", choices=["csv", "nc"], default="csv")
    ps.add_argument("--var", default="u", help="NetCDF variable name (ignored for CSV)")
    ps.add_argument("--step", type=int, help="Snapshot index (default: latest)")
    ps.add_argument("--title", help="Plot title")
    ps.add_argument("--cmap", default="viridis")
    ps.add_argument("--vmin", type=float)
    ps.add_argument("--vmax", type=float)
    ps.add_argument("--no-colorbar", action="store_true")
    ps.add_argument("--show", action="store_true", help="Display the figure window")
    ps.add_argument("--save", help="Path to save PNG")
    ps.set_defaults(func=cmd_show)

    # compare
    pc = sub.add_parser("compare", help="Side-by-side comparison of two outputs at same step")
    pc.add_argument("--dir-a", required=True)
    pc.add_argument("--dir-b", required=True)
    pc.add_argument("--fmt-a", choices=["csv", "nc"], default="csv")
    pc.add_argument("--fmt-b", choices=["csv", "nc"], default="nc")
    pc.add_argument("--var-a", default="u")
    pc.add_argument("--var-b", default="u")
    pc.add_argument("--step", type=int, required=True)
    pc.add_argument("--title-a")
    pc.add_argument("--title-b")
    pc.add_argument("--cmap", default="viridis")
    pc.add_argument("--vmin", type=float)
    pc.add_argument("--vmax", type=float)
    pc.add_argument("--split-colorbar", action="store_true")
    pc.add_argument("--no-diff", action="store_true")
    pc.add_argument("--diff-cmap", default="coolwarm")
    pc.add_argument("--diff-vlim", type=float)
    pc.add_argument("--show", action="store_true")
    pc.add_argument("--save", help="Path to save PNG")
    pc.set_defaults(func=cmd_compare)

    # animate
    pa = sub.add_parser("animate", help="Create an animation over steps")
    pa.add_argument("--dir", required=True)
    pa.add_argument("--fmt", choices=["csv", "nc"], default="csv")
    pa.add_argument("--var", default="u")
    group_sel = pa.add_mutually_exclusive_group()
    group_sel.add_argument("--steps", help="Explicit selection: 'a-b' or 'i,j,k'")
    group_sel2 = pa.add_argument_group("range selection")
    group_sel2.add_argument("--start", type=int)
    group_sel2.add_argument("--end", type=int)
    group_sel2.add_argument("--stride", type=int)
    pa.add_argument("--interval", type=int, default=150, help="ms between frames")
    pa.add_argument("--fps", type=int, default=12)
    pa.add_argument("--no-repeat", action="store_true")
    pa.add_argument("--cmap", default="viridis")
    pa.add_argument("--vmin", type=float)
    pa.add_argument("--vmax", type=float)
    pa.add_argument("--save", required=True, help="Output file (.mp4 or .gif)")
    pa.add_argument("--writer", choices=["ffmpeg", "pillow"])
    pa.add_argument("--title-prefix", default="timestep")
    pa.set_defaults(func=cmd_animate)

    # watch
    p_w =sub.add_parser("watch", help="Live view: poll outputs/snapshots and redraw on new steps")
    p_w.add_argument("--dir", default="outputs", help="Base outputs dir (contains rank_layout.csv and snapshots/)")
    p_w.add_argument("--fmt", choices=["csv", "nc"], default="csv", help="Snapshot format to read")
    p_w.add_argument("--var", default="u", help="NetCDF variable name (when --fmt=nc)")
    p_w.add_argument("--interval", type=float, default=0.5, help="Polling interval in seconds")
    p_w.add_argument("--cmap", default="viridis", help="Matplotlib colormap")
    p_w.add_argument("--vmin", type=float, default=None, help="Fixed lower color limit (default: autoscale)")
    p_w.add_argument("--vmax", type=float, default=None, help="Fixed upper color limit (default: autoscale)")
    p_w.add_argument("--title", default=None, help="Figure title (default: Step <n>)")
    p_w.add_argument("--no-colorbar", action="store_true", help="Disable colorbar")
    p_w.add_argument("--tight", action="store_true", help="Apply tight_layout on each draw")
    p_w.add_argument("--redraw-same", action="store_true", help="Redraw even if the latest step didn’t change")
    p_w.set_defaults(func=cmd_watch)

    #interactive
    pi = sub.add_parser("interactive", help="Interactive navigation of outputs")
    pi.add_argument("--dir", required=True)
    pi.add_argument("--fmt", choices=["csv", "nc"], default="csv")
    pi.add_argument("--var", default="u")
    pi.add_argument("--cmap", default="viridis")
    pi.set_defaults(func=cmd_interactive)

    return p


def main(argv: Optional[Sequence[str]] = None) -> None:
    parser = build_parser()
    args = parser.parse_args(argv)
    args.func(args)


if __name__ == "__main__":
    main()
