#pragma once
#include "common.h"
#include "settings.h"
#include "openings.h"
#include "stats.h"
#include "fit.h"

string CSV_HEADER =
    "what,band,games,score,white_wins,draws,black_wins,expected,residual,h_points,h_error,avg_rating\n";

void writeCsvRow(FILE *csv, string what, string band, Result r) {
    fprintf(csv, "\"%s\",\"%s\",%lld,%.6f,%.6f,%.6f,%.6f,%.6f,%.6f,%.4f,%.4f,%.1f\n",
            what.c_str(), band.c_str(), r.games, r.score, r.whiteWins, r.draws, r.blackWins,
            r.expected, r.score - r.expected, r.h, r.error, r.avgRating);
}

// printf: %-24s = text padded to 24 characters, %12lld = long long in 12 columns
void printHeader(string first) {
    printf("%-24s%-14s%12s%8s%8s%8s%8s%9s%7s\n", first.c_str(), "Band", "Games",
           "White", "Draw", "Black", "Exp.", "h (pts)", "95% CI");
    cout << string(98, '-') << "\n";
}

void printRow(string what, string band, Result r) {
    printf("%-24s%-14s%12lld%8.3f%8.3f%8.3f%8.3f%+9.1f%7.1f\n", what.c_str(), band.c_str(), r.games,
           r.whiteWins, r.draws, r.blackWins, r.expected, r.h, 1.96 * r.error);
}

void showRow(FILE *csv, string what, string band, Result r) {
    printRow(what, band, r);
    writeCsvRow(csv, what, band, r);
}

// output.csv: one row per speed, then per speed and bracket (all openings together)
void printResults(Stats &st) {
    FILE *csv = fopen("output.csv", "w");
    fprintf(csv, "%s", CSV_HEADER.c_str());

    cout << "\nWhite advantage, adjusted for rating gap (all openings)\n"
         << "  White / Draw / Black = share of games won by white, drawn, won by black\n"
         << "  Exp.  = white's average score predicted by the ratings alone\n"
         << "  h     = white's edge in rating points, with 95% confidence interval\n\n";
    printHeader("Speed");
    showRow(csv, "All", "all", fit(st, -1, -1, -1, -1));
    for (int s = 0; s < NUM_SPEEDS; s++) {
        Result total = fit(st, -1, -1, s, -1);
        if (total.games < MIN_GAMES) continue;
        showRow(csv, speedNames[s], "all", total);
        for (int b = 0; b < numBands(); b++) {
            Result r = fit(st, -1, -1, s, b);
            if (r.games >= MIN_GAMES) showRow(csv, speedNames[s], bandName(b), r);
        }
    }
    fclose(csv);
    cout << "\nSaved output.csv\n";
}

// openings.csv: one row per opening (all speeds), then per opening and bracket
void printOpenings(Stats &st) {
    FILE *csv = fopen("openings.csv", "w");
    fprintf(csv, "group,%s", CSV_HEADER.c_str());

    cout << "\nBy opening (all speeds)\n\n";
    printHeader("Opening");
    for (int o = 0; o < numOpenings(); o++) {
        Result r = fit(st, o, -1, -1, -1);
        if (r.games == 0) {
            cout << openingName(o) << ": no games (check the name in the openings list)\n";
            continue;
        }
        fprintf(csv, "%s,", groupNames[groupOf(o)].c_str());
        showRow(csv, openingName(o), "all", r);
    }
    for (int g = AGGRESSIVE; g <= DRAWISH; g++) {
        fprintf(csv, "%s,", groupNames[g].c_str());
        showRow(csv, groupNames[g] + " (all 4)", "all", fit(st, -1, g, -1, -1));
    }
    for (int o = 0; o < numOpenings(); o++)
        for (int b = 0; b < numBands(); b++) {
            Result r = fit(st, o, -1, -1, b);
            if (r.games < MIN_GAMES) continue;
            fprintf(csv, "%s,", groupNames[groupOf(o)].c_str());
            writeCsvRow(csv, openingName(o), bandName(b), r);
        }
    fclose(csv);
    cout << "\nSaved openings.csv\n";
}
