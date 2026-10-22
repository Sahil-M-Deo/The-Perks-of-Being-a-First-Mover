# The Perks of Being a First Mover

**Does White's advantage in chess grow or shrink with rating?**
CS215 Open Project, Group 02. Mentor: Namish Bansal.

Sanidhya (25b0944), Daksh Maahor (25b0974), Sahil (25b0991), VadaPavJai (25b1030)

## The idea

Comparing raw win rates across rating brackets is misleading, because the two players are
rarely rated exactly the same. A 1540 who beats a 1570 with White is stronger evidence of a
first-move edge than a win between two 1570s. So for every game we use the Elo expected score
with an extra bonus `h` for White:

    E = 1 / (1 + 10^(-(whiteRating + h - blackRating) / 400))

and find the `h` that best fits the results (win = 1, draw = 0.5, loss = 0). `h` is White's
advantage in rating points. We fit it overall, for each speed (Bullet, Blitz, ...) and for each
100-point rating bracket like [1500,1600).

## Results so far

Lichess standard rated games, January and February 2024: 190.6 million games read, 162.2 million
kept after cleaning.

| Speed | Games | White score | h (rating points, 95% CI) |
|---|---:|---:|---:|
| All | 162,175,412 | 0.517 | +11.9 ± 0.1 |
| Blitz | 77,918,475 | 0.518 | +12.8 ± 0.1 |
| Bullet | 59,945,392 | 0.514 | +9.9 ± 0.1 |
| Rapid | 22,650,621 | 0.520 | +14.2 ± 0.1 |
| Classical | 857,174 | 0.519 | +13.4 ± 0.7 |
| UltraBullet | 722,976 | 0.513 | +9.1 ± 0.8 |
| Correspondence | 80,774 | 0.504 | +2.8 ± 2.4 |

The full table for every speed and bracket is in `output/summary.txt`, and the write-up is
`reports/checkpoint1/checkpointOne.pdf`.

## What's where

    README.md                        this file
    scripts/
        white_advantage.cpp          the whole analysis: reads the games, fits h, writes the results
        white_advantage_colab.ipynb  runs it on Google Colab, streaming the data from Lichess
    output/
        output.csv                   results table (one row per speed and bracket)
        summary.txt                  everything the program printed
    reports/
        proposal.pdf
        checkpoint1/
            report.tex               Checkpoint 1 report
            report_numbers.tex       numbers, tables and figure for it (written by the program)
            checkpointOne.pdf        the submitted report
    raw_data/                        put downloaded .pgn.zst files here (never committed)

Note: the current `output/output.csv` was rebuilt from the printed table, so it has the
printed precision (scores to 3 decimals). Rerun the program to get the exact values.

## Running it

### On Google Colab (recommended)

The monthly files are tens of GB each, so the easiest way is
`scripts/white_advantage_colab.ipynb`: upload it to colab.research.google.com and run the cells.
It streams each month from Lichess straight into the program, so nothing big is saved anywhere,
and at the end you download `results.zip` with the results and the report.

### On your own machine

Needs g++ and the zstd library (`sudo apt install libzstd-dev`), plus LaTeX with pgfplots if you
want the PDF built too. Download monthly files from https://database.lichess.org into `raw_data/`,
then from the repo root:

    g++ -O2 -std=c++17 -pthread scripts/white_advantage.cpp -o scripts/white_advantage -lzstd
    cd output
    ../scripts/white_advantage ../raw_data/lichess_db_standard_rated_*.pgn.zst --report ../reports/checkpoint1/report_numbers.tex | tee summary.txt

That one run writes:
- `output/output.csv` and `output/summary.txt`: the results
- `reports/checkpoint1/report_numbers.tex`: every number, table and the figure for the report
- `reports/checkpoint1/checkpointOne.pdf`: the finished report (the program runs `pdflatex`)

Each file is read on its own thread, and the results are added together. Memory stays at a few MB.
After editing `report.tex` by hand, rebuild only the PDF from `reports/checkpoint1/` with
`pdflatex -jobname=checkpointOne report.tex`.

### Options

| Option | Meaning |
|---|---|
| `--max-games N` | only read the first N games of each file (for a quick test run) |
| `--report FILE` | write the numbers, tables and figure for `report.tex`, then build `checkpointOne.pdf` |

The results table is always saved as `output.csv` in the folder you run the program from. The
analysis settings are fixed at the top of `white_advantage.cpp`: 100-point rating brackets (by the
average of both players' ratings), games with a rating gap over 100 skipped, and brackets with
fewer than 1,000 games hidden.

### Output columns

- **Score**: White's actual average score
- **Expected**: average score the ratings alone predict
- **Residual**: Score − Expected, White's edge in score
- **h (pts)**: White's edge in rating points
- **95% CI**: h is within ± this value with 95% confidence

Abandoned games and games without a result or rating are skipped. The counts are printed above
the table.

## Data

[Lichess Open Database](https://database.lichess.org), standard rated games, released under the
Creative Commons CC0 licence.
