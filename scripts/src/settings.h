#pragma once
#include "common.h"

const int BAND_WIDTH = 100;   // rating brackets are [1500,1600), [1600,1700), ...
const int MAX_DIFF = 100;     // games with a bigger rating gap are skipped
const int MIN_GAMES = 1000;   // brackets with fewer games than this are left out
const int MAX_RATING = 4000;

// command line options
int MAX_GAMES = 0;
string REPORT_FILE = "";
string COUNTS_IN = "";

// UltraBullet has to come before Bullet, or "UltraBullet" games would match "Bullet" first
enum Speed { ULTRABULLET, BULLET, BLITZ, RAPID, CLASSICAL, CORRESPONDENCE, OTHER, NUM_SPEEDS };
string speedNames[NUM_SPEEDS] = {"UltraBullet", "Bullet", "Blitz", "Rapid",
                                 "Classical", "Correspondence", "Other"};

enum GameResult { WHITE_WINS, DRAW, BLACK_WINS, NO_RESULT };

// white's score doubled, so a draw is 1 instead of 0.5 and everything stays an int
int halfPointsFor(GameResult r) {
    if (r == WHITE_WINS) return 2;
    if (r == DRAW) return 1;
    return 0;
}

int numBands() {
    return MAX_RATING / BAND_WIDTH + 1;
}

// rating gaps go from -MAX_DIFF to +MAX_DIFF, stored at index gap + MAX_DIFF
int numDiffs() {
    return 2 * MAX_DIFF + 1;
}

int bandMiddle(int b) {
    return b * BAND_WIDTH + BAND_WIDTH / 2;
}

string bandName(int b) {
    int low = b * BAND_WIDTH;
    return "[" + to_string(low) + "," + to_string(low + BAND_WIDTH) + ")";
}
