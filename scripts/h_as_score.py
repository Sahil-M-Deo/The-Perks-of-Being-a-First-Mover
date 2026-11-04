# Adds White's extra expected score to output.csv and openings.csv: 100 * (E(h) - 0.5)
# percentage points, so h = 11.9 means White scores about 51.7% between equal players.
# Usage: python3 h_as_score.py [folder]   (default folder: output/final)
import sys

import pandas as pd


def extra(h):
    return 100 * (1 / (1 + 10 ** (-h / 400)) - 0.5)


folder = sys.argv[1] if len(sys.argv) > 1 else "output/final"

for name in ["output", "openings"]:
    t = pd.read_csv(f"{folder}/{name}.csv")
    t["extra_score_pts"] = extra(t["h_points"])
    # E(h) only goes up with h, so the ends of h's CI give the ends of this one
    t["extra_score_low"] = extra(t["h_points"] - 1.96 * t["h_error"])
    t["extra_score_high"] = extra(t["h_points"] + 1.96 * t["h_error"])
    t.to_csv(f"{folder}/{name}_score.csv", index=False, float_format="%.4f")
    print(f"Saved {folder}/{name}_score.csv")

    if name == "output":
        print(f"\n{'Speed':<16}{'h':>7}{'extra score':>13}   95% CI")
        for _, r in t[t["band"] == "all"].iterrows():
            print(f"{r['what']:<16}{r['h_points']:>+7.1f}{r['extra_score_pts']:>+13.2f}"
                  f"   [{r['extra_score_low']:+.2f}, {r['extra_score_high']:+.2f}]")
        print()
