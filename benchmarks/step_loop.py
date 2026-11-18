"""Report median time per step and global cell updates/s. Run from the source root."""

import argparse
import csv
import os
from pathlib import Path
import re
import shlex
import statistics
import subprocess
import sys
import tempfile


def main():
    available = len(os.sched_getaffinity(0)) if hasattr(os, "sched_getaffinity") else os.cpu_count()
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--build", type=Path, nargs="+", default=[Path("build")],
                        help="one or more builds to measure in rotation")
    parser.add_argument("--ranks", nargs="+", type=int, default=sorted({1, 2, 4, available or 1}))
    parser.add_argument("--grids", nargs="+", type=int, default=[512, 2048, 4096])
    parser.add_argument("--steps", type=int, help="override grid-specific step count")
    parser.add_argument("--repeats", type=int, default=3)
    parser.add_argument("--modes", nargs="+", choices=["compute", "output"],
                        default=["compute", "output"])
    parser.add_argument("--launcher", default="mpirun --use-hwthread-cpus --bind-to hwthread --map-by core")
    args = parser.parse_args()
    if min(args.grids + args.ranks + [args.repeats]) <= 0 or (args.steps is not None and args.steps < 1):
        parser.error("grid sizes, ranks, steps and repeats must be positive")

    writer = csv.writer(sys.stdout)
    writer.writerow(["build", "grid", "ranks", "mode", "steps", "repeats", "ms_per_step", "cell_updates_s"])
    with tempfile.TemporaryDirectory(dir=args.build[0]) as work:
        for n in args.grids:
            steps = args.steps or {512: 64, 2048: 12, 4096: 8}.get(n, 16)
            common = [
                f"--nx={n}", f"--ny={n}", f"--steps={steps}", "--D=0.05",
                "--vx=0.5", "--vy=-0.25", "--dt=0.1", f"--out_every={max(1, steps // 2)}",
                f"--output={work}/frames.nc",
            ]
            for ranks in args.ranks:
                launcher = [*shlex.split(args.launcher), "-np", str(ranks)]
                for mode in args.modes:
                    binary = "src/climate_sim" if mode == "output" else "step_loop_bench"
                    times = {build: [] for build in args.build}
                    for trial in range(args.repeats):
                        start = trial % len(args.build)
                        for build in args.build[start:] + args.build[:start]:
                            command = [*launcher, build / binary, *common]
                            log = subprocess.check_output(command, text=True)
                            elapsed = float(re.search(r"total_max=([\d.eE+-]+)", log)[1])
                            times[build].append(elapsed)
                    for build, samples in times.items():
                        elapsed = statistics.median(samples)
                        writer.writerow([build, n, ranks, mode, steps, len(samples),
                                         f"{1000 * elapsed / steps:.6f}", round(n * n * steps / elapsed)])
                    sys.stdout.flush()


if __name__ == "__main__":
    main()
