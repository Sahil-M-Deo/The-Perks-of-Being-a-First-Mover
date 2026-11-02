#pragma once
#include "common.h"
#include "settings.h"
#include "openings.h"
#include "stats.h"
#include "fit.h"
#include "format.h"
#include "dashboard_page.h"

// what you can tick in the dashboard: all games, a group of openings, or one opening
struct Series {
    string name;
    int opening, group;   // as in collect(), -1 = any
};

vector<Series> allSeries() {
    vector<Series> list = {{"All games", -1, -1},
                           {"Aggressive openings", -1, AGGRESSIVE},
                           {"Drawish openings", -1, DRAWISH}};
    for (int o = 0; o < numOpenings(); o++) list.push_back({openingName(o), o, -1});
    return list;
}

// {"1", "2"} -> "[1,2]", the way lists are written in JavaScript
string jsList(vector<string> items) {
    string s = "[";
    for (int i = 0; i < (int)items.size(); i++) {
        if (i > 0) s += ",";
        s += items[i];
    }
    return s + "]";
}

string inQuotes(string s) {
    return "\"" + s + "\"";
}

// [h, error, games, white wins, draws, black wins, average rating]
vector<string> resultValues(Result r) {
    return {num(r.h, 2), num(r.error, 3), to_string(r.games), num(r.whiteWins, 4),
            num(r.draws, 4), num(r.blackWins, 4), num(r.avgRating, 0)};
}

// one result per speed (speed -1 = all speeds), left empty if there are too few games
string totalsFor(Stats &st, Series se) {
    vector<string> perSpeed;
    for (int s = -1; s < NUM_SPEEDS; s++) {
        Result r = fit(st, se.opening, se.group, s, -1);
        if (r.games < MIN_GAMES) r = Result();
        perSpeed.push_back(jsList(resultValues(r)));
    }
    return jsList(perSpeed);
}

// per speed, a list of [bracket middle, result values...] for brackets with enough games
string bracketsFor(Stats &st, Series se) {
    vector<string> perSpeed;
    for (int s = -1; s < NUM_SPEEDS; s++) {
        vector<string> rows;
        for (int b = 0; b < numBands(); b++) {
            Result r = fit(st, se.opening, se.group, s, b);
            if (r.games < MIN_GAMES) continue;
            vector<string> row = resultValues(r);
            row.insert(row.begin(), to_string(bandMiddle(b)));
            rows.push_back(jsList(row));
        }
        perSpeed.push_back(jsList(rows));
    }
    return jsList(perSpeed);
}

string groupLabel(Series se) {
    if (se.group != -1) return groupNames[se.group];
    if (se.opening != -1) return groupNames[groupOf(se.opening)];
    return "";
}

// dashboard.html: one file with all the numbers inside, open it in any browser
void writeDashboard(string path, Stats &st, vector<string> &months) {
    vector<Series> series = allSeries();
    vector<string> names, groups, totals, brackets;
    for (Series se : series) {
        names.push_back(inQuotes(se.name));
        groups.push_back(inQuotes(groupLabel(se)));
        totals.push_back(totalsFor(st, se));
        brackets.push_back(bracketsFor(st, se));
    }
    vector<string> speeds = {inQuotes("All speeds")};
    for (int s = 0; s < NUM_SPEEDS; s++) speeds.push_back(inQuotes(speedNames[s]));

    FILE *f = fopen(path.c_str(), "w");
    fprintf(f, "%s", PAGE_START.c_str());
    fprintf(f, "const DATA = {\n  months: \"%s\",\n  games: %lld,\n", monthList(months).c_str(), st.used);
    fprintf(f, "  series: %s,\n  groups: %s,\n  speeds: %s,\n",
            jsList(names).c_str(), jsList(groups).c_str(), jsList(speeds).c_str());
    fprintf(f, "  totals: %s,\n  bands: %s\n};\n", jsList(totals).c_str(), jsList(brackets).c_str());
    fprintf(f, "%s", PAGE_END.c_str());
    fclose(f);
    cout << "Saved " << path << "\n";
}
