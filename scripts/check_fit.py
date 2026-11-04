# Fits h again from counts.csv, separately from fit.h, and compares with output.csv and openings.csv.
# Usage: python3 check_fit.py [folder]   (default folder: output/final)
import sys

import numpy as np
import pandas as pd

K = np.log(10) / 400
MIN_GAMES = 1000


def fit(cells):
    games = cells["games"].sum()
    avg_rating = (cells["games"] * (cells["band"] + 50)).sum() / games
    by_gap = cells.groupby("diff")[["games", "half_points", "draws"]].sum()
    d = by_gap.index.to_numpy(dtype=float)
    n = by_gap["games"].to_numpy(dtype=float)
    s = by_gap["half_points"].to_numpy() / 2
    draws = by_gap["draws"].sum() / games

    # Newton's method, Theory.pdf section 3
    h = 0.0
    for i in range(50):
        e = 1 / (1 + np.exp(-K * (d + h)))
        step = (s.sum() - (n * e).sum()) / (K * (n * e * (1 - e)).sum())
        h += step
        if abs(step) < 1e-10:
            break
    e = 1 / (1 + np.exp(-K * (d + h)))
    info = K * K * (n * e * (1 - e)).sum()

    score = s.sum() / games
    expected = (n / (1 + np.exp(-K * d))).sum() / games
    white = score - draws / 2
    return {"games": games, "score": score, "white_wins": white, "draws": draws,
            "black_wins": 1 - white - draws, "expected": expected, "residual": score - expected,
            "h_points": h, "h_error": 1 / np.sqrt(info), "avg_rating": avg_rating}


def groups_of(folder):
    o = pd.read_csv(folder + "/openings.csv")
    o = o[~o["what"].str.endswith("(all 4)")]
    return dict(zip(o["what"], o["group"]))


def rows_for(counts, groups, what, band):
    if what.endswith(" (all 4)"):
        counts = counts[counts["opening"].map(groups) == what.replace(" (all 4)", "")]
    elif what in groups:
        counts = counts[counts["opening"] == what]
    elif what != "All":
        counts = counts[counts["speed"] == what]
    if band != "all":
        # "[1500,1600)" -> 1500
        counts = counts[counts["band"] == int(band[1:].split(",")[0])]
    return counts


if __name__ == "__main__":
    folder = sys.argv[1] if len(sys.argv) > 1 else "output/final"
    counts = pd.read_csv(folder + "/counts.csv", comment="#")
    groups = groups_of(folder)

    # the C++ prints 6 decimals for shares, 4 for h and SE, 1 for the rating
    allowed = {"games": 0, "score": 1e-6, "white_wins": 1e-6, "draws": 1e-6, "black_wins": 1e-6,
               "expected": 1e-6, "residual": 1e-6, "h_points": 1e-4, "h_error": 1e-4,
               "avg_rating": 0.1}

    for name in ["output.csv", "openings.csv"]:
        table = pd.read_csv(folder + "/" + name)
        worst = dict.fromkeys(allowed, 0.0)
        for _, row in table.iterrows():
            mine = fit(rows_for(counts, groups, row["what"], row["band"]))
            for col in allowed:
                worst[col] = max(worst[col], abs(mine[col] - row[col]))
        print(name + ":", len(table), "rows")
        for col in allowed:
            ok = "ok" if worst[col] <= allowed[col] else "DIFFERENT"
            print(f"  {col:<11} biggest difference {worst[col]:.1e}  {ok}")
