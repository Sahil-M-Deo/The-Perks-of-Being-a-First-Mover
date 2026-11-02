#pragma once
#include "common.h"
#include "settings.h"
#include "stats.h"

struct Result {
    int games = 0;
    double score = 0, expected = 0, h = 0, error = 0;
    double whiteWins = 0, draws = 0, blackWins = 0;   // fractions of the games
    double avgRating = 0;
};

/*
   Maximum likelihood estimate of h with Newton's method, and its standard error
   from the Fisher information. The maths and a worked example are in Theory.pdf,
   sections 3 and 4.
*/
Result fitWhiteAdvantage(Cells &c) {
    const double K = log(10.0) / 400.0;   // 10^x = exp(x * ln 10)
    Result r;
    double totalScore = 0, totalExpected = 0, totalDraws = 0;

    for (int i=0; i<numDiffs(); i++) {
        int diff = i - MAX_DIFF;
        r.games += c.games[i];
        totalScore += c.halfPoints[i] / 2.0;
        totalDraws += c.draws[i];
        totalExpected += c.games[i] / (1 + exp(-K * diff));
    }
    if (r.games==0) return r;

    double h = 0, information = 0;
    for (int iter=0; iter<50; iter++) {
        double gradient = 0;
        information = 0;
        for (int i=0; i<numDiffs(); i++) {
            if (c.games[i]==0) continue;
            int diff = i - MAX_DIFF;
            double e = 1 / (1 + exp(-K * (diff + h)));
            gradient += K * (c.halfPoints[i] / 2.0 - c.games[i] * e);
            information += K * K * c.games[i] * e * (1 - e);
        }
        double step = gradient / information;
        h += step;
        if (fabs(step)<1e-6) break;
    }

    r.h = h;
    r.error = 1 / sqrt(information);
    r.score = totalScore / r.games;
    r.expected = totalExpected / r.games;
    r.draws = totalDraws / r.games;
    r.whiteWins = r.score - r.draws / 2;   // score = wins + draws / 2
    r.blackWins = 1 - r.whiteWins - r.draws;
    r.avgRating = c.ratingSum / r.games;
    return r;
}

Result fit(Stats &st, int opening, int group, int speed, int band) {
    Cells c = collect(st, opening, group, speed, band);
    return fitWhiteAdvantage(c);
}
