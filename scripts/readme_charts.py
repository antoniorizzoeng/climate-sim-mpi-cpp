"""Regenerate the README figures from the published performance tables."""

from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
ASSETS = ROOT / "docs/assets"
ACCENT = "#ffad86"
LILAC = "#c2b1e5"


def rows(section):
    return [
        [cell.strip() for cell in line.strip("|").split("|")]
        for line in section.splitlines()
        if line.startswith("| ") and line[2].isdigit()
    ]


def number(cell):
    return float(cell.split()[0].replace(",", ""))


def label(x, y, value, size=17, color="#eeedf1", anchor="start"):
    return (
        f'<text x="{x}" y="{y}" fill="{color}" font-size="{size}" '
        f'text-anchor="{anchor}">{value}</text>'
    )


def save(name, title, subtitle, marks, height):
    svg = [
        f'<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 840 {height}" '
        'font-family="DejaVu Sans, sans-serif" role="img">',
        f"<title>{title}</title><desc>{subtitle}</desc>",
        f'<rect width="840" height="{height}" rx="12" fill="#18191f"/>',
        label(28, 39, title, 25),
        label(28, 66, subtitle, 16, "#c4c0ca"),
        *marks,
        "</svg>",
    ]
    (ASSETS / name).write_text("\n".join(svg) + "\n")


def speedup(compute):
    # Use matching portable Release flags and the same rank count in every pair.
    marks = []
    for i, row in enumerate(r for r in compute if r[1] == "1"):
        grid, _, baseline, portable, _ = row
        ratio = number(portable) / number(baseline)
        y = 104 + i * 38
        marks.extend([
            label(28, y + 6, grid),
            f'<rect x="120" y="{y-8}" width="{ratio*23:.1f}" height="16" rx="4" fill="{ACCENT}"/>',
            label(760, y + 6, f"{ratio:.1f}×", 19, ACCENT, "end"),
        ])
    save("speedup.svg", "Speed-up on one MPI rank",
         "Portable fused kernel ÷ baseline throughput  ·  compute only", marks, 215)


def rank_plot(name, title, subtitle, ranks, series, ceiling):
    # Horizontal distance follows the numerical rank count.
    left, right, top, bottom = 68, 775, 126, 249
    strip = []
    for i, (legend, color, values) in enumerate(series):
        strip.append(label(68 + 320*i, 100, "●  " + legend, 17, color))
    for rate in (0, ceiling // 2, ceiling):
        y = bottom - rate / ceiling * (bottom - top)
        strip.append(f'<path d="M{left} {y}H{right}" stroke="#404048"/>')
        strip.append(label(left-10, y+5, rate, 14, "#c4c0ca", "end"))
    xs = [left + (r - ranks[0]) / (ranks[-1] - ranks[0]) * (right-left) for r in ranks]
    for x, rank in zip(xs, ranks):
        strip.append(label(x, bottom+26, rank, 16, anchor="middle"))
    strip.append(label(420, 299, "MPI ranks", 15, "#c4c0ca", "middle"))
    for _, color, values in series:
        coords = [(x, bottom - value / ceiling * (bottom-top)) for x, value in zip(xs, values)]
        points = " ".join(f"{x:.2f},{y:.2f}" for x, y in coords)
        strip.append(f'<polyline points="{points}" fill="none" stroke="{color}" stroke-width="3"/>')
        for x, y in coords:
            strip.append(f'<circle cx="{x:.2f}" cy="{y:.2f}" r="4.5" fill="{color}"/>')
    save(name, title, subtitle, strip, 317)


def main():
    report = (ROOT / "docs/performance.md").read_text()
    compute = rows(report.split("### Compute only\n", 1)[1].split("### With three snapshots", 1)[0])
    speedup(compute)
    plane = [r for r in compute if r[0] == "2048²"]
    rank_plot("rank-scaling.svg", "Strong scaling on 2048² cells",
              "Million cell updates per second  ·  fixed global grid  ·  CPU compute only",
              [int(r[1]) for r in plane],
              [("Portable", ACCENT, [number(r[3]) for r in plane]),
               ("Baseline", LILAC, [number(r[2]) for r in plane])], 800)
    volume_section = report.split("## 3D scaling", 1)[1].split("The 2D comparison", 1)[0]
    cube = [r for r in rows(volume_section) if r[0] == "256³"]
    rank_plot("volume-throughput.svg", "What three snapshots cost on 256³ cells",
              "Million cell updates per second  ·  portable CPU  ·  eight steps",
              [int(r[1]) for r in cube],
              [("Compute only", ACCENT, [number(r[3]) for r in cube]),
               ("With CDF-5 output", LILAC, [number(r[5]) for r in cube])], 640)


if __name__ == "__main__":
    main()
