# The Perks of Being a First Mover

**Does White's advantage in chess grow or shrink with rating, and with the opening?**
CS215 Open Project, Group 02. Mentor: Namish Bansal.

Sanidhya (25b0944), Daksh Maahor (25b0974), Sahil (25b0991), VadaPavJai (25b1030)

## The idea

Comparing raw win rates is misleading, because the two players are rarely rated exactly the
same. A 1540 who beats a 1570 with White is stronger evidence of a first-move edge than a win
between two 1570s. So for every game we use the Elo expected score with an extra bonus `h` for
White:

    E = 1 / (1 + 10^(-(whiteRating + h - blackRating) / 400))

and find the `h` that best fits the results (win = 1, draw = 0.5, loss = 0) by maximum
likelihood. `h` is White's advantage in rating points. We fit it overall, for each speed
(Bullet, Blitz, ...), for each 100-point rating bracket like [1500,1600), and for each opening.

The three questions:
1. **Rating.** Does White's advantage peak at intermediate ratings and shrink at 2200+?
2. **Openings.** Do aggressive openings carry a bigger White advantage than drawish ones?
   - Aggressive (all 1.e4 gambits): King's Gambit, Evans Gambit, Danish Gambit, Smith-Morra Gambit
   - Drawish: Petrov's Defense, Berlin Defense, Queen's Gambit Declined, French Exchange
3. **Time control.** Does White's advantage change from Bullet to Blitz, Rapid and Classical?

Each opening, each speed and each rating bracket is treated as its own set of games and gets its
own maximum-likelihood `h`. How `h` changes with rating is read from the bracket-by-bracket charts.
Two sets of games are compared with a z-test for the difference (aggressive vs drawish openings,
and each speed vs the next faster one).
All of it is explained with worked examples in `reports/final/Theory.pdf`.

## What's where

    README.md                        this file
    scripts/
        white_advantage.cpp          main(): reads the arguments and runs everything below
        src/                         the program, split by job (the list is at the top of
                                     white_advantage.cpp)
        white_advantage_colab.ipynb  runs it on Google Colab, streaming the data from Lichess
    output/
        checkpoint1/                 the Checkpoint 1 results (output.csv, summary.txt)
        final/
            counts.csv               every game counted by opening, speed, bracket and rating gap
            output.csv               results by speed and bracket
            openings.csv             results by opening (and by opening and bracket)
            summary.txt              everything the program printed
            dashboard.html           interactive Plotly dashboard: compare openings or speeds
    reports/
        proposal.pdf
        checkpoint1/                 Checkpoint 1 report (report.tex, report_numbers.tex, checkpointOne.pdf)
        final/
            report.tex               final report text
            report_numbers.tex       its numbers, tables and figures (written by the program)
            finalReport.pdf          the final report
            Theory.tex, Theory.pdf   the statistics explained, with worked examples

## Running it

The monthly Lichess files are tens of GB each, so reading them is the only slow part, and it
only has to happen once: the program saves `counts.csv` (a few MB), and everything else is
rebuilt from that file in seconds with `--from-counts`.

### 1. The slow run, on Google Colab

Open the notebook straight from GitHub:
[Open in Colab](https://colab.research.google.com/github/Sahil-M-Deo/The-Perks-of-Being-a-First-Mover/blob/main/scripts/white_advantage_colab.ipynb)
(or upload `scripts/white_advantage_colab.ipynb` to colab.research.google.com), and run steps 1 to 4.
The notebook clones this repo, so push any code changes before running it.
It streams each month from Lichess straight into the program, so nothing big is saved anywhere.
Do a test run with `MAX_GAMES = 1000000` first, then the full run. Download `results.zip` and
put everything in it into `output/final/`, then push.

### 2. Rebuilding the results, dashboard and report from `counts.csv`

Either run step 5 of the notebook, or on your own machine (needs g++, the zstd library
`sudo apt install libzstd-dev`, and LaTeX with pgfplots for the PDF), from the repo root:

    g++ -O2 -std=c++17 -pthread scripts/white_advantage.cpp -o scripts/white_advantage -lzstd
    cd output/final
    ../../scripts/white_advantage --from-counts counts.csv --report ../../reports/final/report_numbers.tex

That writes `output/final/output.csv`, `openings.csv`, `dashboard.html`,
`reports/final/report_numbers.tex` and builds `reports/final/finalReport.pdf`.
After editing `report.tex` by hand, rebuild only the PDF from `reports/final/` with
`pdflatex -jobname=finalReport report.tex` (twice, so figure numbers are right).
`Theory.pdf` is built from `Theory.tex` the same way: `pdflatex Theory.tex`.

### Reading the games on your own machine

If you have the monthly files on your machine, keep them outside the repo (they're tens of GB)
and pass their paths, from `output/final/`:

    ../../scripts/white_advantage /path/to/lichess_db_standard_rated_2024-0*.pgn.zst --report ../../reports/final/report_numbers.tex | tee summary.txt

Each file is read on its own thread and the results are added together. Memory stays small
(about 20 MB per file).

### Options

| Option | Meaning |
|---|---|
| `--max-games N` | only read the first N games of each file (for a quick test run) |
| `--from-counts FILE` | skip the games and load a saved `counts.csv` instead |
| `--report FILE` | write the numbers, tables and figures for `report.tex`, then build `finalReport.pdf` |

The analysis settings are fixed in `scripts/src/settings.h`: 100-point rating brackets (by the
average of both players' ratings), games with a rating gap over 100 skipped, and brackets with
fewer than 1,000 games left out. The openings are the `openings` list in `scripts/src/openings.h`:
each one is matched by the start of the Lichess opening name, minus some gambit side-lines.
Four spare openings (Scotch, Vienna, Four Knights, Slav) are counted too, so they can be swapped
into the report later without reading the games again.

### Output columns

- **White / Draw / Black**: share of games won by White, drawn, won by Black
- **Exp.**: White's average score predicted by the ratings alone
- **h (pts)**: White's edge in rating points, with its 95% confidence interval

Abandoned games and games without a result or rating are skipped. The counts are printed above
the tables.

## Data

[Lichess Open Database](https://database.lichess.org), standard rated games, released under the
Creative Commons CC0 licence. Opening names from
[lichess-org/chess-openings](https://github.com/lichess-org/chess-openings).