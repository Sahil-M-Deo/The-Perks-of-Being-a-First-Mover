#pragma once
#include "common.h"
#include "settings.h"
#include "openings.h"
#include "stats.h"
#include "fit.h"

// two-sided p-value of z from the normal distribution; erfc is the complementary error function
double pValue(double z) {
    return erfc(fabs(z) / sqrt(2.0));
}

/*
   z-test for the difference between the h of two different sets of games, like
   aggressive vs drawish or Blitz vs Bullet. Theory.pdf, section 5.
*/
struct Comparison {
    string first, second;
    Result a, b;
    double diff = 0, error = 0;
};

Comparison compare(string first, Result a, string second, Result b) {
    Comparison c;
    c.first = first;
    c.second = second;
    c.a = a;
    c.b = b;
    c.diff = a.h - b.h;
    c.error = sqrt(a.error * a.error + b.error * b.error);
    return c;
}

Comparison compareGroups(Stats &st, int speed) {
    Result aggressive = fit(st, -1, AGGRESSIVE, speed, -1);
    Result drawish = fit(st, -1, DRAWISH, speed, -1);
    return compare("Aggressive", aggressive, "Drawish", drawish);
}

bool enoughGames(Stats &st, int speed) {
    return fit(st, -1, -1, speed, -1).games >= MIN_GAMES;
}

// a speed gets a line in the by-rating charts if at least 3 brackets have enough games
bool enoughBrackets(Stats &st, int speed) {
    int count = 0;
    for (int b = 0; b < numBands(); b++)
        if (fit(st, -1, -1, speed, b).games >= MIN_GAMES) count++;
    return count >= 3;
}

struct Analysis {
    bool hasLine[NUM_SPEEDS];
    Comparison groups, groupsSpeed[NUM_SPEEDS];
    vector<Comparison> speedSteps;   // each speed minus the next faster one
};

Analysis analyse(Stats &st) {
    Analysis a;
    for (int s = 0; s < NUM_SPEEDS; s++) a.hasLine[s] = enoughBrackets(st, s);
    a.groups = compareGroups(st, -1);
    for (int s = BULLET; s <= RAPID; s++) a.groupsSpeed[s] = compareGroups(st, s);

    // the Speed enum is already in order from fastest to slowest
    int previous = -1;
    for (int s = ULTRABULLET; s <= CORRESPONDENCE; s++) {
        if (!enoughGames(st, s)) continue;
        if (previous != -1) {
            Result slower = fit(st, -1, -1, s, -1);
            Result faster = fit(st, -1, -1, previous, -1);
            a.speedSteps.push_back(compare(speedNames[s], slower, speedNames[previous], faster));
        }
        previous = s;
    }
    return a;
}

void printComparison(string what, Comparison c) {
    printf("  %-30s %s %+6.1f   %s %+6.1f   difference %+6.1f +- %4.1f   (z = %+.1f, p = %.2g)\n",
           what.c_str(), c.first.c_str(), c.a.h, c.second.c_str(), c.b.h, c.diff, 1.96 * c.error,
           c.diff / c.error, pValue(c.diff / c.error));
}

void printAnalysis(Analysis &a) {
    cout << "\nAggressive vs drawish openings, h in rating points (95% CI)\n";
    printComparison("All speeds", a.groups);
    for (int s = BULLET; s <= RAPID; s++) printComparison(speedNames[s], a.groupsSpeed[s]);

    cout << "\nEach speed against the next faster one, h in rating points (95% CI)\n";
    for (Comparison c : a.speedSteps) printComparison(c.first + " vs " + c.second, c);
}
