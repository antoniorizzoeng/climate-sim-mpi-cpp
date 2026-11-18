"""Report median time per step and global cell updates/s. Run from the source root."""

import argparse
import csv
import math
import os
from pathlib import Path
import re
import shlex
import statistics
import subprocess
import sys
import tempfile


def grid_shape(value):
    try:
        shape = tuple(int(n) for n in value.lower().split("x"))
    except ValueError:
        raise argparse.ArgumentTypeError("use N or NXxNYxNZ") from None
    if min(shape) < 1 or len(shape) not in (1, 3):
        raise argparse.ArgumentTypeError("use positive grid dimensions N or NXxNYxNZ")
    return (shape[0], shape[0], 1) if len(shape) == 1 else shape


def main():
    available = len(os.sched_getaffinity(0)) if hasattr(os, "sched_getaffinity") else os.cpu_count()
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--build", type=Path, nargs="+", default=[Path("build")],
                        help="one or more builds to measure in rotation")
    parser.add_argument("--backends", nargs="+", choices=["cpu", "kokkos"],
                        help="one execution backend per --build directory, in the same order")
    parser.add_argument("--ranks", nargs="+", type=int, default=sorted({1, 2, 4, available or 1}))
    parser.add_argument("--grids", nargs="+", type=grid_shape, default=[(n,n,1) for n in (512,2048,4096)],
                        help="square width or NXxNYxNZ")
    parser.add_argument("--steps", type=int, help="override grid-specific step count")
    parser.add_argument("--repeats", type=int, default=3)
    parser.add_argument("--modes", nargs="+", choices=["compute", "output"],
                        default=["compute", "output"])
    parser.add_argument("--launcher", default="mpirun --use-hwthread-cpus --bind-to hwthread --map-by core")
    args = parser.parse_args()
    if min(args.ranks + [args.repeats]) <= 0 or (args.steps is not None and args.steps < 1):
        parser.error("grid sizes, ranks, steps and repeats must be positive")

    if args.backends and len(args.backends) != len(args.build):
        parser.error("supply one --backends entry per --build directory")
    builds = list(zip(args.build, args.backends or ["cpu"] * len(args.build)))
    writer = csv.writer(sys.stdout)
    writer.writerow(["build", *(["backend"] if args.backends else []), "grid", "ranks", "mode", "steps", "repeats", "ms_per_step", "cell_updates_s"])
    with tempfile.TemporaryDirectory(dir=args.build[0]) as work:
        for nx, ny, nz in args.grids:
            grid = str(nx) if nx == ny and nz == 1 else f"{nx}x{ny}x{nz}"
            default_steps = {512: 64, 2048: 12, 4096: 8}.get(nx, 16) if nz == 1 else 8
            steps = args.steps or default_steps
            common = [
                f"--nx={nx}", f"--ny={ny}", f"--steps={steps}", "--D=0.05",
                "--vx=0.5", "--vy=-0.25", "--dt=0.1", f"--out_every={max(1, steps // 2)}",
                f"--output={work}/frames.nc",
            ]
            if nz > 1:
                common += [f"--nz={nz}", "--vz=0.125"]
            for ranks in args.ranks:
                launcher = [*shlex.split(args.launcher), "-np", str(ranks)]
                for mode in args.modes:
                    binary = "src/climate_sim" if mode == "output" else "step_loop_bench"
                    times = {case: [] for case in builds}
                    for trial in range(args.repeats):
                        start = trial % len(builds)
                        for build, backend in builds[start:] + builds[:start]:
                            command = [*launcher, build / binary, *common]
                            if args.backends:
                                command.append(f"--backend={backend}")
                            log = subprocess.check_output(command, text=True)
                            elapsed = float(re.search(r"total_max=([\d.eE+-]+)", log)[1])
                            times[build, backend].append(elapsed)
                    for (build, backend), samples in times.items():
                        elapsed = statistics.median(samples)
                        writer.writerow([build, *([backend] if args.backends else []), grid, ranks, mode, steps, len(samples),
                                         f"{1000 * elapsed / steps:.6f}", round(math.prod((nx, ny, nz)) * steps / elapsed)])
                    sys.stdout.flush()


if __name__ == "__main__":
    main()
