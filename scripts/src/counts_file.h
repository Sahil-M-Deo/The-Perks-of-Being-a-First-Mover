#pragma once
#include "common.h"
#include "settings.h"
#include "openings.h"
#include "stats.h"

/*
   counts.csv: every non-empty cell of Stats, plus the per-month and skipped counts on
   lines starting with '#'. It's a few MB instead of 60+ GB of games, and
   --from-counts counts.csv rebuilds everything from it in seconds.
*/
void writeCounts(string path, Stats &total, vector<string> &months, vector<Stats> &perFile) {
    FILE *f = fopen(path.c_str(), "w");
    for (int i = 0; i < (int)months.size(); i++)
        fprintf(f, "#month,%s,%lld,%lld\n", months[i].c_str(), perFile[i].seen, perFile[i].used);
    fprintf(f, "#skipped,%lld,%lld,%lld,%lld\n",
            total.noResult, total.abandoned, total.noRating, total.bigGap);
    for (pair<string, int> fam : total.families)
        fprintf(f, "#family,%s,%lld\n", fam.first.c_str(), fam.second);

    fprintf(f, "opening,speed,band,diff,games,half_points,draws\n");
    for (int o = 0; o < numOpenings(); o++)
        for (int s = 0; s < NUM_SPEEDS; s++)
            for (int b = 0; b < numBands(); b++)
                for (int d = 0; d < numDiffs(); d++) {
                    if (total.games[o][s][b][d] == 0) continue;
                    fprintf(f, "%s,%s,%lld,%lld,%lld,%lld,%lld\n",
                            openingName(o).c_str(), speedNames[s].c_str(), b * BAND_WIDTH, d - MAX_DIFF,
                            total.games[o][s][b][d], total.halfPoints[o][s][b][d], total.draws[o][s][b][d]);
                }
    fclose(f);
    cout << "Saved " << path << "\n";
}

vector<string> splitCommas(string line) {
    vector<string> parts;
    string cur = "";
    for (char c : line) {
        if (c == ',') {
            parts.push_back(cur);
            cur = "";
        }
        else cur += c;
    }
    parts.push_back(cur);
    return parts;
}

void readCountsLine(vector<string> &p, Stats &total) {
    int o = 0, s = 0;
    while (o < otherOpening() && openingName(o) != p[0]) o++;
    while (s < OTHER && speedNames[s] != p[1]) s++;
    int b = stoll(p[2]) / BAND_WIDTH;
    int d = stoll(p[3]) + MAX_DIFF;
    total.games[o][s][b][d] += stoll(p[4]);
    total.halfPoints[o][s][b][d] += stoll(p[5]);
    total.draws[o][s][b][d] += stoll(p[6]);
}

void readCounts(string path, Stats &total, vector<string> &months, vector<Stats> &perFile) {
    ifstream in(path);
    string line;
    while (getline(in, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        vector<string> p = splitCommas(line);
        if (p[0] == "#month") {
            months.push_back(p[1]);
            perFile.push_back(Stats());
            perFile.back().seen = stoll(p[2]);
            perFile.back().used = stoll(p[3]);
            total.seen += stoll(p[2]);
            total.used += stoll(p[3]);
        }
        else if (p[0] == "#skipped") {
            total.noResult = stoll(p[1]);
            total.abandoned = stoll(p[2]);
            total.noRating = stoll(p[3]);
            total.bigGap = stoll(p[4]);
        }
        else if (p[0] == "#family") total.families[p[1]] = stoll(p[2]);
        else if (p[0] != "opening") readCountsLine(p, total);
    }
    cout << "Loaded " << path << "\n";
}
