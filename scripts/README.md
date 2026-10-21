# White advantage on Lichess

Measures how much of an advantage white has on Lichess, after correcting each
game for the rating gap between the players.

For every game, the expected score for white is

    E = 1 / (1 + 10^(-(whiteRating + h - blackRating) / 400))

where `h` is white's advantage in rating points. The program finds the `h` that
best fits all the results (win = 1, draw = 0.5, loss = 0), overall, for each
speed (Bullet, Blitz, ...) and for each rating bracket like [1500,1600).

## Compile

Needs the zstd library (`sudo apt install libzstd-dev`).

    g++ -O2 -std=c++17 -pthread white_advantage.cpp -o white_advantage -lzstd

## Run

Download monthly files from https://database.lichess.org and pass them in.
Each file is read on its own thread and the results are added together.

    ./white_advantage lichess_db_standard_rated_2024-01.pgn.zst lichess_db_standard_rated_2024-02.pgn.zst

Plain `.pgn` files work too. Try `--max-games 1000000` first for a quick test run.

## Options

| Option | Meaning |
|---|---|
| `--max-games N` | only read the first N games of each file (for a quick test run) |
| `--report FILE` | write the numbers, tables and figure for `report.tex`, then build `checkpointOne.pdf` |

The results table is always saved as `output.csv` in the folder you run the program from.

The analysis settings are fixed at the top of `white_advantage.cpp`: 100-point rating
brackets (by the average of both players' ratings), games with a rating gap over 100
skipped, and brackets with fewer than 1,000 games hidden.

## Output columns

- **Score**: white's actual average score
- **Expected**: average score the ratings alone predict
- **Residual**: Score − Expected, white's edge in score
- **h (pts)**: white's edge in rating points
- **95% CI**: h is within ± this value with 95% confidence

Abandoned games and games without a result or rating are skipped. The counts are
printed above the table.

## Making the Checkpoint 1 report

From the repo root, with the `.pgn.zst` files in `raw_data/` (they're too big for GitHub, so don't commit them):

    g++ -O2 -std=c++17 -pthread scripts/white_advantage.cpp -o scripts/white_advantage -lzstd
    cd output
    ../scripts/white_advantage ../raw_data/lichess_db_standard_rated_*.pgn.zst --report ../report_numbers.tex | tee summary.txt

That one run does everything:
- `output/output.csv`: the results table
- `output/summary.txt`: everything the program printed
- `report_numbers.tex`: every number in the report, both tables and the figure
- `checkpointOne.pdf`: the finished report (the program runs `pdflatex` on `report.tex`,
  so LaTeX with pgfplots has to be installed)

Until it's run, the committed `report_numbers.tex` just has red placeholders. The mentor,
repo link and roles are typed into `report.tex` by hand. After editing `report.tex`, rebuild
the PDF without re-reading the games with `pdflatex -jobname=checkpointOne report.tex` from the repo root.
