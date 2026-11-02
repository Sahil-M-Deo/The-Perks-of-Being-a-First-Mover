#pragma once
#include "common.h"
#include "settings.h"
#include "openings.h"

/*
   All the counts, by [opening][speed][band][rating gap + MAX_DIFF]:
   number of games, white's score (doubled) and number of draws.
*/
struct Stats {
    v4d games, halfPoints, draws;
    int seen = 0, used = 0;
    int noResult = 0, abandoned = 0, noRating = 0, bigGap = 0;
    map<string, int> families;   // games per opening family, like "Sicilian Defense"

    Stats() {
        games = v4d(numOpenings(), v3d(NUM_SPEEDS, v2d(numBands(), v1d(numDiffs(), 0))));
        halfPoints = games;
        draws = games;
    }

    void add(Stats &other) {
        for (int o=0; o<numOpenings(); o++)
            for (int s=0; s<NUM_SPEEDS; s++)
                for (int b=0; b<numBands(); b++)
                    for (int d=0; d<numDiffs(); d++) {
                        games[o][s][b][d] += other.games[o][s][b][d];
                        halfPoints[o][s][b][d] += other.halfPoints[o][s][b][d];
                        draws[o][s][b][d] += other.draws[o][s][b][d];
                    }
        seen += other.seen;
        used += other.used;
        noResult += other.noResult;
        abandoned += other.abandoned;
        noRating += other.noRating;
        bigGap += other.bigGap;
        for (pair<string, int> f : other.families) families[f.first] += f.second;
    }
};

// the counts for one selection of games, still split by rating gap (that's what the fit needs)
struct Cells {
    v1d games, halfPoints, draws;
    double ratingSum = 0;
};

// -1 means "any": collect(st, -1, DRAWISH, BLITZ, -1) is every drawish Blitz game
Cells collect(Stats &st, int opening, int group, int speed, int band) {
    Cells c;
    c.games = v1d(numDiffs(), 0);
    c.halfPoints = c.games;
    c.draws = c.games;

    int oLo = 0, oHi = numOpenings() - 1;
    int sLo = 0, sHi = NUM_SPEEDS - 1;
    int bLo = 0, bHi = numBands() - 1;
    if (opening!=-1) oLo = oHi = opening;
    if (speed!=-1) sLo = sHi = speed;
    if (band!=-1) bLo = bHi = band;

    for (int o=oLo; o<=oHi; o++) {
        if (group!=-1 && groupOf(o)!=group) continue;
        for (int s=sLo; s<=sHi; s++)
            for (int b=bLo; b<=bHi; b++)
                for (int d=0; d<numDiffs(); d++) {
                    int n = st.games[o][s][b][d];
                    if (n==0) continue;
                    c.games[d] += n;
                    c.halfPoints[d] += st.halfPoints[o][s][b][d];
                    c.draws[d] += st.draws[o][s][b][d];
                    c.ratingSum += (double)n * bandMiddle(b);
                }
    }
    return c;
}
