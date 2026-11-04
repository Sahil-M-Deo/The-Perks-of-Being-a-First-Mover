# Aggressive vs drawish openings by rating bracket: each group's share of its games, both h's,
# and h_A - h_D with its 95% CI. Nothing is reweighted.
# Usage: python3 rating_mix.py [folder] [speed]   (default: output/final, all speeds)
import sys

import numpy as np
import pandas as pd

from check_fit import MIN_GAMES, fit, groups_of

folder = sys.argv[1] if len(sys.argv) > 1 else "output/final"
speed = sys.argv[2] if len(sys.argv) > 2 else None

counts = pd.read_csv(folder + "/counts.csv", comment="#")
counts["group"] = counts["opening"].map(groups_of(folder))
if speed:
    counts = counts[counts["speed"] == speed]
agg = counts[counts["group"] == "Aggressive"]
dra = counts[counts["group"] == "Drawish"]

rows = []
for band in sorted(set(agg["band"]) | set(dra["band"])):
    a = agg[agg["band"] == band]
    d = dra[dra["band"] == band]
    row = {"band": f"[{band},{band + 100})",
           "games_aggr": a["games"].sum(), "games_draw": d["games"].sum(),
           "share_aggr_pct": 100 * a["games"].sum() / agg["games"].sum(),
           "share_draw_pct": 100 * d["games"].sum() / dra["games"].sum()}
    if row["games_aggr"] >= MIN_GAMES and row["games_draw"] >= MIN_GAMES:
        fa = fit(a)
        fd = fit(d)
        row["h_aggr"] = fa["h_points"]
        row["h_draw"] = fd["h_points"]
        row["gap"] = fa["h_points"] - fd["h_points"]
        row["gap_ci"] = 1.96 * np.sqrt(fa["h_error"] ** 2 + fd["h_error"] ** 2)
    rows.append(row)
table = pd.DataFrame(rows)

out = f"{folder}/rating_mix{'_' + speed if speed else ''}.csv"
table.to_csv(out, index=False, float_format="%.4f")

print("Aggressive vs drawish,", speed or "all speeds")
print(f"{'Bracket':<13}{'Aggr %':>8}{'Draw %':>8}{'h_A':>8}{'h_D':>8}   h_A - h_D")
for _, r in table.iterrows():
    line = f"{r['band']:<13}{r['share_aggr_pct']:>8.1f}{r['share_draw_pct']:>8.1f}"
    if r["games_aggr"] >= MIN_GAMES and r["games_draw"] >= MIN_GAMES:
        line += f"{r['h_aggr']:>+8.1f}{r['h_draw']:>+8.1f}   {r['gap']:+.1f} +- {r['gap_ci']:.1f}"
    print(line)
print(f"Average rating: aggressive {(agg['games'] * (agg['band'] + 50)).sum() / agg['games'].sum():.0f},"
      f" drawish {(dra['games'] * (dra['band'] + 50)).sum() / dra['games'].sum():.0f}")
print("Saved", out)
