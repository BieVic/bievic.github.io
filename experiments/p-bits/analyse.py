#!/usr/bin/env python3
"""Merges results/factor/*.json and fits the growth of sweeps with problem size."""
import glob, json, math

rows = sorted((json.loads(open(f).read()) for f in glob.glob("results/factor/*.json")),
              key=lambda r: (r["factor_bits"], r["mode"]))
json.dump(rows, open("results/factor.json", "w"), indent=1)
names = {0: "Gibbs", 1: "annealing", 2: "tempering"}

def fit(xs, ys):
    """Least-squares slope and intercept of log2(y) against x."""
    ls = [math.log2(y) for y in ys]
    mx, my = sum(xs) / len(xs), sum(ls) / len(ls)
    slope = sum((x - mx) * (l - my) for x, l in zip(xs, ls)) / sum((x - mx) ** 2 for x in xs)
    return slope, my - slope * mx

print(f"{'n':>2} {'free':>5} {'sampler':>10} {'beta':>5} {'restart':>8} {'t/o':>4} {'median':>10} {'mean':>11} {'blind':>10} {'mean/blind':>10} {'trial div':>9}")
for r in rows:
    print(f"{r['factor_bits']:>2} {r['free_pbits']:>5} {names[r['mode']]:>10} {r['beta']:>5} {r['restart']:>8} {r['timeouts']:>4} "
          f"{r['median_sweeps']:>10} {r['mean_sweeps']:>11.0f} {r['blind_mean_guesses']:>10.0f} "
          f"{r['mean_sweeps'] / r['blind_mean_guesses']:>10.2f} {r['median_trial_div']:>9}")

print("\nGrowth per extra factor bit, n >= 6 (equivalently, per two product bits):")
for m, name in names.items():
    sel = [r for r in rows if r["mode"] == m and r["factor_bits"] >= 6 and r["timeouts"] == 0]
    if len(sel) < 3:
        continue
    xs = [r["factor_bits"] for r in sel]
    for key in ("median_sweeps", "mean_sweeps"):
        s, _ = fit(xs, [r[key] for r in sel])
        print(f"  {name:>10} {key:>13}: x{2 ** s:.2f} per factor bit")
print("  blind guessing: x4.00 per factor bit; trial division: about x2")
