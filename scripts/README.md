# White advantage on Lichess

Measures how much of an advantage white has on Lichess, after correcting each
game for the rating gap between the players.

For every game, the expected score for white is

    E = 1 / (1 + 10^(-(whiteRating + h - blackRating) / 400))

where `h` is white's advantage in rating points. The program finds the `h` that
best fits all the results (win = 1, draw = 0.5, loss = 0), overall, for each
speed (Bullet, Blitz, ...) and for each rating bracket like [1500,1600).

## Compile

Needs the zstd library (`sudo apt install libzstd-dev`, or `brew install zstd` on Mac).

    g++ -O2 -std=c++17 -pthread white_advantage.cpp -o white_advantage -lzstd

`bits/stdc++.h` only exists in GCC. On a Mac, `g++` is actually Clang, so install
GCC (`brew install gcc`) and compile with `g++-14` instead.

## Run

Download monthly files from https://database.lichess.org and pass them in.
Each file is read on its own thread and the results are added together.

    ./white_advantage lichess_db_standard_rated_2024-01.pgn.zst lichess_db_standard_rated_2024-02.pgn.zst

Plain `.pgn` files work too. Try `--max-games 1000000` first for a quick test run.

## Options

| Option | Default | Meaning |
|---|---|---|
| `--band-width` | 100 | width of each rating bracket, by the average of both players' ratings |
| `--max-diff` | 100 | skip games where the rating gap is bigger than this |
| `--min-rating` | 0 | skip games where either player is below this rating |
| `--max-games` | 0 (all) | only read the first N games of each file |
| `--min-games` | 1000 | don't print brackets with fewer games than this |
| `--csv` | none | also save the table to a CSV file |

## Output columns

- **Score**: white's actual average score
- **Expected**: average score the ratings alone predict
- **Residual**: Score − Expected, white's edge in score
- **h (pts)**: white's edge in rating points
- **95% CI**: h is within ± this value with 95% confidence

Abandoned games and games without a result or rating are skipped. The counts are
printed above the table.
