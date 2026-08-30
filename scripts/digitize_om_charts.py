#!/usr/bin/env python3
"""Digitise the Model 525 Operating Manual thrust-setting charts (Figures 7-6..7-10).

The charts are graphs, not tables: a family of straight rising lines, one per
pressure altitude, each clipped from above by a configuration cap. So

    N1(RAT, PA) = min(rise_PA(RAT), cap_config(RAT))

This reads the scanned pages, calibrates pixels to chart units off the printed
axis ticks and the major-gridline lattice, and fits that model. It exists to
*check* `data/`, not to generate it: reading a photocopied graph is good to
roughly a third of a percent N1, where the shipped tables are printed to a
tenth. See `data/PROVENANCE.md`.

The source PDF is not redistributable and is not in this repository; point
--pdf at your own copy.

Usage:
  scripts/digitize_om_charts.py --pdf "CJ1 N1 setting from Ops Man.pdf" [--check data/]
"""

import argparse
import json
import subprocess
import sys
import tempfile
from dataclasses import dataclass, field
from pathlib import Path

import numpy as np
from PIL import Image
from scipy import ndimage

DPI = 400
# N1 axis range and the altitude curves drawn on each page, in order.
HIGH = [0, 5000, 10000, 15000, 25000, 35000, 41000]
PAGES = {
    1: dict(
        fig="7-6",
        title="takeoff / go-around",
        yr=(85, 105),
        alts=[0, 2000, 4000, 6000, 8000, 10000],
    ),
    2: dict(
        fig="7-7", title="max continuous climb, anti-ice off", yr=(85, 102), alts=[0, 5000, 10000]
    ),
    3: dict(fig="7-8", title="max continuous climb, anti-ice on (all)", yr=(85, 102), alts=HIGH),
    4: dict(
        fig="7-9",
        title="max continuous cruise, anti-ice off",
        yr=(85, 102),
        alts=[0, 5000, 10000, 35000],
    ),
    5: dict(fig="7-10", title="max continuous cruise, anti-ice on (all)", yr=(85, 102), alts=HIGH),
}
XR = (-60, 60)


@dataclass
class Calibration:
    """Maps chart units to pixels for one page."""

    x_at0: float
    x_per_deg: float
    y_top: float
    y_per_unit: float
    yr: tuple[int, int]

    def x_of(self, t: float) -> float:
        return self.x_at0 + self.x_per_deg * t

    def y_of(self, n1: float) -> float:
        return self.y_top + (self.yr[1] - n1) * self.y_per_unit

    def n1_of(self, py: float) -> float:
        return self.yr[1] - (py - self.y_top) / self.y_per_unit

    def t_of(self, px: float) -> float:
        return (px - self.x_at0) / self.x_per_deg


def render(pdf: Path, out: Path) -> list[Path]:
    subprocess.run(
        ["pdftoppm", "-r", str(DPI), "-png", "-gray", str(pdf), str(out / "p")], check=True
    )
    return sorted(out.glob("p-*.png"))


def _runs(mask: np.ndarray) -> list[tuple[int, int]]:
    m = np.r_[False, mask, False]
    d = np.diff(m.astype(np.int8))
    s = np.where(d == 1)[0]
    e = np.where(d == -1)[0]
    return list(zip(s, e - s, strict=True))


def _comb(prof: np.ndarray, teeth: int, lo: float, hi: float) -> tuple[float, float]:
    """Best (first tooth, spacing) for `teeth` evenly spaced maxima in `prof`."""
    best = None
    n = len(prof)
    grid = np.arange(n)
    for spacing in np.arange(lo, hi, 0.05):
        width = spacing * (teeth - 1)
        if width >= n:
            continue
        for start in np.arange(0, n - width, 0.5):
            score = np.interp(start + spacing * np.arange(teeth), grid, prof).sum()
            if best is None or score > best[0]:
                best = (score, start, spacing)
    return best[1], best[2]


def _snap(prof: np.ndarray, start: float, spacing: float, teeth: int, win: int = 6):
    centres = []
    for i in range(teeth):
        p = start + spacing * i
        lo, hi = int(max(0, p - win)), int(min(len(prof), p + win + 1))
        seg = prof[lo:hi]
        centres.append(lo + (seg * np.arange(len(seg))).sum() / seg.sum() if seg.sum() > 0 else p)
    slope, intercept = np.polyfit(np.arange(teeth), centres, 1)
    return intercept, slope


def _x_tick_centres(a: np.ndarray, lo: int, hi: int) -> list[float]:
    """Centroids of the printed x-axis tick labels, left to right."""
    lab, _ = ndimage.label(a[lo:hi, :] < 150, structure=np.ones((3, 3)))
    blobs = sorted(
        (sl[1].start + sl[1].stop) / 2.0
        for sl in ndimage.find_objects(lab)
        if (sl[0].stop - sl[0].start) >= 20 and (sl[1].stop - sl[1].start) >= 8
    )
    groups, cur = [], [blobs[0]]
    for b in blobs[1:]:
        if b - cur[-1] < 60:
            cur.append(b)
        else:
            groups.append(cur)
            cur = [b]
    groups.append(cur)
    return [sum(g) / len(g) for g in groups]


def calibrate(a: np.ndarray, yr: tuple[int, int]) -> Calibration:
    """Vertical scale from the major-gridline lattice, horizontal from the tick labels.

    Only the 0..+60 labels anchor x: a leading minus sign drags a negative
    label's centroid left of its tick. The two axes are fitted independently,
    so their agreeing on scale (the grid squares are square) checks both.
    """
    h, w = a.shape
    ink = a < 225
    rows = np.where(ink.sum(1) > 0.28 * w)[0]
    cols = np.where(ink.sum(0) > 0.28 * h)[0]
    y0, y1 = int(rows.min()), int(rows.max())
    x0, x1 = int(cols.min()), int(cols.max())

    ny = yr[1] - yr[0] + 1
    prof_y = (255.0 - a[y0:y1, x0 + 200 : x1 - 200]).mean(1)
    sy, dy = _comb(prof_y, ny, (y1 - y0) / (ny + 1), (y1 - y0) / (ny - 1.6))
    sy, dy = _snap(prof_y, sy, dy, ny)

    ybot = y0 + sy + (ny - 1) * dy
    centres = _x_tick_centres(a, int(ybot + 20), int(ybot + 130))
    if len(centres) != 13:
        raise SystemExit(f"expected 13 x tick labels, found {len(centres)}")
    temps = np.arange(XR[0], XR[1] + 1, 10)
    keep = temps >= 0
    bx, ax = np.polyfit(temps[keep], np.array(centres)[keep], 1)
    return Calibration(x_at0=ax, x_per_deg=bx, y_top=y0 + sy, y_per_unit=dy, yr=yr)


def curve_mask(a: np.ndarray, cal: Calibration) -> np.ndarray:
    """Thick plotted strokes only: opening removes the fine grid, the curves survive."""
    op = ndimage.binary_opening(a < 135, structure=np.ones((5, 5)))
    keep = np.zeros_like(op)
    left, right = int(cal.x_of(XR[0])), int(cal.x_of(XR[1]))
    top, bottom = int(cal.y_of(cal.yr[1])), int(cal.y_of(cal.yr[0]))
    keep[top:bottom, left:right] = True
    return op & keep


def strokes_at(mask: np.ndarray, cal: Calibration, t: float) -> list[float]:
    """N1 of every plotted stroke crossing the RAT column `t`, highest first."""
    x = round(cal.x_of(t))
    if not 0 <= x < mask.shape[1]:
        return []
    idx = np.where(mask[:, x])[0]
    if len(idx) == 0:
        return []
    runs, cur = [], [idx[0]]
    for i in idx[1:]:
        if i - cur[-1] <= 3:
            cur.append(i)
        else:
            runs.append(cur)
            cur = [i]
    runs.append(cur)
    vals = sorted((cal.n1_of(sum(r) / len(r)) for r in runs), reverse=True)
    out = [vals[0]]
    for v in vals[1:]:
        if out[-1] - v > 0.30:
            out.append(v)
    return out


def ransac_lines(pts: np.ndarray, tol=6.0, min_inliers=4000, min_span=200, iters=3000):
    """Repeatedly pull the longest straight stroke out of the point cloud."""
    rng = np.random.default_rng(7)
    alive = np.ones(len(pts), bool)
    found = []
    while alive.sum() > min_inliers:
        idx = np.where(alive)[0]
        p = pts[idx]
        best = None
        for _ in range(iters):
            i, j = rng.integers(0, len(p), 2)
            d = p[j] - p[i]
            n = float(np.hypot(*d))
            if n < min_span:
                continue
            normal = np.array([-d[1] / n, d[0] / n])
            inl = np.abs((p - p[i]) @ normal) < tol
            if best is None or inl.sum() > best[0]:
                best = (int(inl.sum()), inl.copy())
        if best is None or best[0] < min_inliers:
            break
        q = p[best[1]]
        mu = q.mean(0)
        _, _, v = np.linalg.svd(q - mu, full_matrices=False)
        t = (q - mu) @ v[0]
        alive[idx[best[1]]] = False
        if float(t.max() - t.min()) >= min_span:
            found.append((mu, v[0]))
    return found


def line_to_chart(mu, direction, cal: Calibration):
    if abs(direction[0]) < 1e-9:
        return None
    p0, p1 = mu - direction * 400, mu + direction * 400
    t0, t1 = cal.t_of(p0[0]), cal.t_of(p1[0])
    n0, n1 = cal.n1_of(p0[1]), cal.n1_of(p1[1])
    slope = (n1 - n0) / (t1 - t0)
    return slope, n0 - slope * t0


def trace(mask, cal, slope, at0, t_lo, t_hi, tol=0.40, step=1.0):
    """Follow the stroke nearest a moving prediction, refitting as it goes."""
    ts, ns = [], []
    for t in np.arange(t_lo, t_hi + 1e-9, step):
        pred = at0 + slope * t
        cand = [v for v in strokes_at(mask, cal, t) if abs(v - pred) < tol]
        if not cand:
            continue
        ts.append(t)
        ns.append(min(cand, key=lambda z: abs(z - pred)))
        if len(ts) >= 12:
            slope, at0 = np.polyfit(ts[-12:], ns[-12:], 1)
    return np.array(ts), np.array(ns)


def fit_polyline(ts, ns):
    """Two straight segments with a free breakpoint; the caps are kinked, not curved."""
    best = None
    for i in range(8, len(ts) - 8):
        lo, hi = ts <= ts[i], ts >= ts[i]
        if lo.sum() < 6 or hi.sum() < 6:
            continue
        p1, p2 = np.polyfit(ts[lo], ns[lo], 1), np.polyfit(ts[hi], ns[hi], 1)
        r = np.r_[np.polyval(p1, ts[lo]) - ns[lo], np.polyval(p2, ts[hi]) - ns[hi]]
        s = float((r**2).sum())
        if best is None or s < best[0]:
            best = (s, p1, p2)
    s, p1, p2 = best
    return dict(
        brk=float((p2[1] - p1[1]) / (p1[0] - p2[0])),
        cold=list(p1),
        hot=list(p2),
        rms=float(np.sqrt(s / len(ts))),
    )


def cap_eval(cap, t):
    t = np.asarray(t, float)
    return np.where(t <= cap["brk"], np.polyval(cap["cold"], t), np.polyval(cap["hot"], t))


@dataclass
class Figure:
    fig: str
    title: str
    rise: dict = field(default_factory=dict)
    caps: dict = field(default_factory=dict)


def digitise_takeoff(png: Path) -> Figure:
    """Figure 7-6, the one chart this model fits cleanly end to end."""
    a = np.array(Image.open(png).convert("L")).astype(np.float32)
    cal = calibrate(a, PAGES[1]["yr"])
    mask = curve_mask(a, cal)
    ys, xs = np.nonzero(mask)
    fits = [line_to_chart(mu, d, cal) for mu, d in ransac_lines(np.column_stack([xs, ys]))]
    rising = sorted((f for f in fits if f and f[0] > 0), key=lambda f: f[1])
    out = Figure(fig="7-6", title=PAGES[1]["title"])
    for alt, (slope, at0) in zip(PAGES[1]["alts"], rising, strict=True):
        ts, ns = trace(mask, cal, slope, at0, -50, 30)
        p = np.polyfit(ts, ns, 1)
        out.rise[alt] = dict(
            slope=float(p[0]),
            at0=float(p[1]),
            rms=float(np.sqrt(((np.polyval(p, ts) - ns) ** 2).mean())),
        )
    seeded_caps = (
        ("anti_ice_off", ((-0.1522, 101.60, -20, 24), (-0.2414, 103.86, 27, 56))),
        ("anti_ice_on_all", ((-0.2110, 94.11, -30, -14), (-0.1550, 94.76, -10, 27))),
    )
    for name, seeds in seeded_caps:
        ts, ns = [], []
        for slope, at0, lo, hi in seeds:
            a_, b_ = trace(mask, cal, slope, at0, lo, hi)
            ts.append(a_)
            ns.append(b_)
        ts, ns = np.concatenate(ts), np.concatenate(ns)
        order = np.argsort(ts)
        out.caps[name] = fit_polyline(ts[order], ns[order])
    return out


def chart_n1(fig: Figure, cap: str, t: float, pa: float) -> float:
    alts = np.array(sorted(fig.rise), float)
    vals = np.array([fig.rise[a]["at0"] + fig.rise[a]["slope"] * t for a in sorted(fig.rise)])
    return min(float(np.interp(pa, alts, vals)), float(cap_eval(fig.caps[cap], t)))


def read_table(path: Path):
    rows = [ln.split(",") for ln in path.read_text().strip().split("\n")]
    pas = [float(x) for x in rows[0][1:]]
    return {
        (float(r[0]), pa): float(c)
        for r in rows[1:]
        for pa, c in zip(pas, r[1:], strict=True)
        if c.strip()
    }


def compare(fig: Figure, data_dir: Path) -> None:
    print(f"\nFigure {fig.fig} vs shipped tables (chart minus table, %N1)")
    pairs = (
        ("n1_takeoff.csv", "anti_ice_off"),
        ("n1_takeoff_ai.csv", "anti_ice_on_all"),
        ("n1_goaround.csv", "anti_ice_off"),
        ("n1_goaround_ai.csv", "anti_ice_on_all"),
    )
    for name, cap in pairs:
        path = data_dir / name
        if not path.exists():
            continue
        tbl = read_table(path)
        d = np.array([chart_n1(fig, cap, t, pa) - v for (t, pa), v in tbl.items()])
        print(
            f"  {name:20s} n={len(d):4d}  mean {d.mean():+.2f}  sd {d.std():.2f}  "
            f"max|d| {np.abs(d).max():.2f}"
        )


def main() -> int:
    ap = argparse.ArgumentParser(
        description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter
    )
    ap.add_argument("--pdf", required=True, type=Path, help="the Ops Manual chart pages")
    ap.add_argument("--check", type=Path, help="data/ directory to compare against")
    ap.add_argument("--json", type=Path, help="write the fitted model here")
    args = ap.parse_args()

    with tempfile.TemporaryDirectory() as tmp:
        pages = render(args.pdf, Path(tmp))
        if len(pages) < 1:
            raise SystemExit("no pages rendered")
        fig = digitise_takeoff(pages[0])

    print(f"Figure {fig.fig} — {fig.title}")
    print("  rising lines, N1 = at0 + slope * RAT")
    for alt in sorted(fig.rise):
        r = fig.rise[alt]
        print(f"    {alt:>6} ft  N1 = {r['at0']:7.3f} {r['slope']:+.4f}*T   (rms {r['rms']:.3f})")
    print("  caps, N1 = min(rise, cap)")
    for name, c in fig.caps.items():
        print(
            f"    {name:16s} break {c['brk']:+6.1f} C"
            f" | cold {c['cold'][1]:7.3f}{c['cold'][0]:+.4f}T"
            f" | hot {c['hot'][1]:7.3f}{c['hot'][0]:+.4f}T  (rms {c['rms']:.3f})"
        )

    if args.check:
        compare(fig, args.check)
    if args.json:
        args.json.write_text(json.dumps(dict(fig=fig.fig, rise=fig.rise, caps=fig.caps), indent=2))
    return 0


if __name__ == "__main__":
    sys.exit(main())
