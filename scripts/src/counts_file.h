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
/*
   Files are saved safely: everything is written to "<path>.tmp" first, flushed to the disk,
   and only then renamed to <path>. A crash in the middle of saving leaves the previous
   version of <path> untouched instead of a half-written file.
   The "#status" line says whether the counts cover the whole file ("complete"), only the
   games read so far ("partial", a checkpoint), or a file that was cut off ("incomplete").
*/
bool writeCountsFile(string path, Stats &total, vector<string> &months, vector<int> &seen,
        vector<int> &used, string status) {
    for (char &c : status)
        if (c==',' || c=='\n') c = ';';
    string tmp = path + ".tmp";
    FILE *f = fopen(tmp.c_str(), "w");
    if (!f) {
        cout << "ERROR: could not create " << tmp << "\n";
        return false;
    }
    bool ok = true;
    ok &= fprintf(f, "#status,%s\n", status.c_str()) > 0;
    for (int i=0; i<(int)months.size(); i++)
        ok &= fprintf(f, "#month,%s,%lld,%lld\n", months[i].c_str(), seen[i], used[i]) > 0;
    ok &= fprintf(f, "#skipped,%lld,%lld,%lld,%lld\n",
            total.noResult, total.abandoned, total.noRating, total.bigGap) > 0;
    for (pair<string, int> fam : total.families)
        ok &= fprintf(f, "#family,%s,%lld\n", fam.first.c_str(), fam.second) > 0;

    ok &= fprintf(f, "opening,speed,band,diff,games,half_points,draws\n") > 0;
    for (int o=0; o<numOpenings(); o++)
        for (int s=0; s<NUM_SPEEDS; s++)
            for (int b=0; b<numBands(); b++)
                for (int d=0; d<numDiffs(); d++) {
                    if (total.games[o][s][b][d]==0) continue;
                    ok &= fprintf(f, "%s,%s,%lld,%lld,%lld,%lld,%lld\n",
                            openingName(o).c_str(), speedNames[s].c_str(), b * BAND_WIDTH, d - MAX_DIFF,
                            total.games[o][s][b][d], total.halfPoints[o][s][b][d], total.draws[o][s][b][d]) > 0;
                }
    ok &= fprintf(f, "#end\n") > 0;   // the last line, so a file that was cut short is easy to spot
    ok &= fflush(f)==0;
    ok &= fsync(fileno(f))==0;
    ok &= fclose(f)==0;
    if (!ok || rename(tmp.c_str(), path.c_str())!=0) {
        cout << "ERROR: could not save " << path << " (disk full?)\n";
        return false;
    }
    return true;
}

bool writeCounts(string path, Stats &total, vector<string> &months, vector<Stats> &perFile, string status) {
    vector<int> seen, used;
    for (Stats &p : perFile) {
        seen.push_back(p.seen);
        used.push_back(p.used);
    }
    bool ok = writeCountsFile(path, total, months, seen, used, status);
    if (ok) cout << "Saved " << path << "\n";
    return ok;
}

// the counts of one file on its own (a checkpoint, or one month that has just finished)
bool writeOneMonth(string path, string month, Stats &st, string status) {
    vector<string> months = {month};
    vector<int> seen = {st.seen}, used = {st.used};
    return writeCountsFile(path, st, months, seen, used, status);
}

vector<string> splitCommas(string line) {
    vector<string> parts;
    string cur = "";
    for (char c : line) {
        if (c==',') {
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
    while (o<otherOpening() && openingName(o)!=p[0]) o++;
    while (s<OTHER && speedNames[s]!=p[1]) s++;
    int b = stoll(p[2]) / BAND_WIDTH;
    int d = stoll(p[3]) + MAX_DIFF;
    total.games[o][s][b][d] += stoll(p[4]);
    total.halfPoints[o][s][b][d] += stoll(p[5]);
    total.draws[o][s][b][d] += stoll(p[6]);
}

// adds the counts in path to total; returns the file's "#status" ("" for files saved before
// the status line existed), or "unreadable" if the file is missing or was cut short
string readCounts(string path, Stats &total, vector<string> &months, vector<Stats> &perFile) {
    ifstream in(path);
    if (!in) {
        cout << "ERROR: could not open " << path << "\n";
        return "unreadable";
    }
    string line, status = "";
    bool hasHeader = false, hasEnd = false;
    while (getline(in, line)) {
        if (!line.empty() && line.back()=='\r') line.pop_back();
        if (line.empty()) continue;
        vector<string> p = splitCommas(line);
        if (p[0]=="#status") status = p.size()>1 ? p[1] : "";
        else if (p[0]=="#end") hasEnd = true;
        else if (p[0]=="#month") {
            months.push_back(p[1]);
            perFile.push_back(Stats());
            perFile.back().seen = stoll(p[2]);
            perFile.back().used = stoll(p[3]);
            total.seen += stoll(p[2]);
            total.used += stoll(p[3]);
        }
        else if (p[0]=="#skipped") {
            // += so that several counts files can be added together
            total.noResult += stoll(p[1]);
            total.abandoned += stoll(p[2]);
            total.noRating += stoll(p[3]);
            total.bigGap += stoll(p[4]);
        }
        else if (p[0]=="#family" && p.size()>=3) {
            // some Lichess names have commas ("London System, with Bd3"): the count is the
            // last field and the name is everything between the first and the last comma
            string name = p[1];
            for (int i=2; i+1<(int)p.size(); i++) name += "," + p[i];
            total.families[name] += stoll(p.back());
        }
        else if (p[0]=="opening") hasHeader = true;
        else if (p[0][0]=='#') continue;   // a line from a newer version; skip it
        else if (p.size()>=7) readCountsLine(p, total);
    }
    // files from this version end with "#end"; older ones don't have "#status" either
    if (!hasHeader || (status!="" && !hasEnd)) {
        cout << "ERROR: " << path << " is cut short or not a counts file\n";
        return "unreadable";
    }
    cout << "Loaded " << path << (status!="" ? " (" + status + ")" : "") << "\n";
    return status;
}
