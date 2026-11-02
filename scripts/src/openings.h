#pragma once
#include "common.h"

/*
   Lichess writes the opening in the "Opening" header, like
   "King's Gambit Accepted: Bishop's Gambit". A game belongs to an opening below if
   the name starts with one of its prefixes and doesn't contain any of its excludes.
   The EXTRA ones aren't in the report; they're counted so they can be swapped in
   later without reading the games again.
*/
enum Group { AGGRESSIVE, DRAWISH, EXTRA, NO_GROUP };
string groupNames[4] = {"Aggressive", "Drawish", "Extra", "Other"};

struct Opening {
    string name;
    Group group;
    vector<string> prefixes;
    vector<string> excludes;
};

vector<Opening> openings = {
    {"King's Gambit", AGGRESSIVE, {"King's Gambit"}, {}},
            {"Evans Gambit", AGGRESSIVE, {"Italian Game: Evans Gambit"}, {}},
            {"Danish Gambit", AGGRESSIVE, {"Danish Gambit"}, {}},
            {"Smith-Morra Gambit", AGGRESSIVE, {"Sicilian Defense: Smith-Morra Gambit"}, {}},
    // Lichess used to call the Petrov "Russian Game" (it still does in the 2024 files)
            {"Petrov's Defense", DRAWISH, {"Petrov's Defense", "Russian Game"},
            {"Stafford Gambit", "Cochrane Gambit"}},
            {"Berlin Defense", DRAWISH, {"Ruy Lopez: Berlin Defense"}, {}},
            {"Queen's Gambit Declined", DRAWISH, {"Queen's Gambit Declined"},
            {"Albin Countergambit", "Chigorin Defense", "Baltic Defense"}},
            {"French Exchange", DRAWISH, {"French Defense: Exchange Variation"}, {}},
            {"Scotch Game", EXTRA, {"Scotch Game"}, {}},
            {"Vienna Game", EXTRA, {"Vienna Game"}, {}},
            {"Four Knights Game", EXTRA, {"Four Knights Game"}, {"Halloween Gambit"}},
            {"Slav Defense", EXTRA, {"Slav Defense"}, {}},
            };

// one more slot after the list for "every other opening"
int numOpenings() {
    return openings.size() + 1;
}

int otherOpening() {
    return openings.size();
}

Group groupOf(int o) {
    if (o==otherOpening()) return NO_GROUP;
    return openings[o].group;
}

string openingName(int o) {
    if (o==otherOpening()) return "Other openings";
    return openings[o].name;
}

bool startsWith(string &s, string &prefix) {
    return s.compare(0, prefix.size(), prefix)==0;
}

int openingOf(string &name) {
    for (int o=0; o<(int)openings.size(); o++) {
        for (string p : openings[o].prefixes) {
            if (!startsWith(name, p)) continue;
            bool excluded = false;
            for (string e : openings[o].excludes)
                if (name.find(e)!=string::npos) excluded = true;
            if (!excluded) return o;
        }
    }
    return otherOpening();
}
