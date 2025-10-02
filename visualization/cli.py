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
    steps = list_available_steps(args.dir)
    if not steps:
        raise SystemExit(f"No snapshots found in {args.dir}/snapshots")
    step = args.step if args.step is not None else steps[-1]
    U = load_global(args.dir, step, var=args.var)
    imshow_field(
        U,
        title=args.title or f"{args.dir} :: step {step}",
        cmap=args.cmap,
        vmin=args.vmin,
        vmax=args.vmax,
        show=args.show,
        save=args.save,
        overlay_minmax=args.overlay_minmax,
    )


def cmd_compare(args: argparse.Namespace) -> None:
    Ua = load_global(args.dir_a, args.step, var=args.var_a)
    Ub = load_global(args.dir_b, args.step, var=args.var_b)
    compare_fields(
        Ua,
        Ub,
        titles=(args.title_a or "A", args.title_b or "B"),
        cmap=args.cmap,
        vmin=args.vmin,
        vmax=args.vmax,
        show_diff=not args.no_diff,
        diff_cmap=args.diff_cmap,
        diff_vlim=args.diff_vlim,
        show=args.show,
        save=args.save,
        overlay_minmax=args.overlay_minmax,
    )


def cmd_animate(args: argparse.Namespace) -> None:
    avail = list_available_steps(args.dir)
    if not avail:
        raise SystemExit(f"No snapshots found in {args.dir}/snapshots")
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
        overlay_minmax=args.overlay_minmax,
    )


def build_parser() -> argparse.ArgumentParser:
    p = argparse.ArgumentParser(
        prog="climate-vis",
        description="Quick CLI for visualizing climate_sim outputs.",
    )
    sub = p.add_subparsers(dest="cmd", required=True)

    ps = sub.add_parser("show", help="Render a single snapshot")
    ps.add_argument("--dir", required=True)
    ps.add_argument("--var", default="u")
    ps.add_argument("--step", type=int)
    ps.add_argument("--title")
    ps.add_argument("--cmap", default="viridis")
    ps.add_argument("--vmin", type=float)
    ps.add_argument("--vmax", type=float)
    ps.add_argument("--show", action="store_true")
    ps.add_argument("--save")
    ps.add_argument("--overlay-minmax", action="store_true")
    ps.set_defaults(func=cmd_show)

    pc = sub.add_parser("compare", help="Side-by-side comparison")
    pc.add_argument("--dir-a", required=True)
    pc.add_argument("--dir-b", required=True)
    pc.add_argument("--var-a", default="u")
    pc.add_argument("--var-b", default="u")
    pc.add_argument("--step", type=int, required=True)
    pc.add_argument("--title-a")
    pc.add_argument("--title-b")
    pc.add_argument("--cmap", default="viridis")
    pc.add_argument("--vmin", type=float)
    pc.add_argument("--vmax", type=float)
    pc.add_argument("--no-diff", action="store_true")
    pc.add_argument("--diff-cmap", default="coolwarm")
    pc.add_argument("--diff-vlim", type=float)
    pc.add_argument("--show", action="store_true")
    pc.add_argument("--save")
    pc.add_argument("--overlay-minmax", action="store_true")
    pc.set_defaults(func=cmd_compare)

    pa = sub.add_parser("animate", help="Create animation")
    pa.add_argument("--dir", required=True)
    pa.add_argument("--var", default="u")
    group_sel = pa.add_mutually_exclusive_group()
    group_sel.add_argument("--steps")
    group_sel2 = pa.add_argument_group("range selection")
    group_sel2.add_argument("--start", type=int)
    group_sel2.add_argument("--end", type=int)
    group_sel2.add_argument("--stride", type=int)
    pa.add_argument("--interval", type=int, default=150)
    pa.add_argument("--fps", type=int, default=12)
    pa.add_argument("--no-repeat", action="store_true")
    pa.add_argument("--cmap", default="viridis")
    pa.add_argument("--vmin", type=float)
    pa.add_argument("--vmax", type=float)
    pa.add_argument("--save", required=True)
    pa.add_argument("--writer", choices=["ffmpeg", "pillow"])
    pa.add_argument("--title-prefix", default="timestep")
    pa.add_argument("--overlay-minmax", action="store_true")
    pa.set_defaults(func=cmd_animate)

    return p


def main(argv: Optional[Sequence[str]] = None) -> None:
    parser = build_parser()
    args = parser.parse_args(argv)
    args.func(args)


if __name__ == "__main__":
    main()
