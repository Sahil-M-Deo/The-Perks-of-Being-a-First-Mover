#pragma once
#include "common.h"
#include "settings.h"
#include "openings.h"
#include "stats.h"
#include "file_reader.h"
#include "counts_file.h"

// header lines look like [WhiteElo "1540"]: key = WhiteElo, value = 1540
string headerKey(string &line) {
    int space = line.find(' ');
    if (space==string::npos) return "";
    return line.substr(1, space - 1);
}

string headerValue(string &line) {
    int first = line.find('"');
    int last = line.rfind('"');
    if (first==string::npos || last<=first) return "";
    return line.substr(first + 1, last - first - 1);
}

// some games have "?" instead of a rating
int toRating(string s) {
    if (s.empty()) return -1;
    for (char c : s)
        if (c<'0' || c>'9') return -1;
    return stoi(s);
}

// Lichess puts the speed in the Event header, like "Rated Blitz game"
Speed speedOf(string &event, string &timeControl) {
    for (int i=ULTRABULLET; i<OTHER; i++)
        if (event.find(speedNames[i])!=string::npos) return (Speed)i;
    if (timeControl=="-") return CORRESPONDENCE;
    return OTHER;
}

struct Game {
    GameResult result = NO_RESULT;
    int whiteRating = -1, blackRating = -1;
    bool abandoned = false;
    string event, timeControl, opening;
    bool hasHeaders = false;
};

void addGame(Game &g, Stats &st) {
    st.seen++;
    if (g.result==NO_RESULT) { st.noResult++; return; }
    if (g.abandoned) { st.abandoned++; return; }
    if (g.whiteRating==-1 || g.blackRating==-1) { st.noRating++; return; }

    int diff = g.whiteRating - g.blackRating;
    if (abs(diff)>MAX_DIFF) { st.bigGap++; return; }

    int average = min((g.whiteRating + g.blackRating) / 2, MAX_RATING);
    int b = average / BAND_WIDTH;
    int s = speedOf(g.event, g.timeControl);
    int o = openingOf(g.opening);
    int d = diff + MAX_DIFF;

    st.games[o][s][b][d]++;
    st.halfPoints[o][s][b][d] += halfPointsFor(g.result);
    if (g.result==DRAW) st.draws[o][s][b][d]++;
    st.used++;

    // the family is the part before the ':', "Sicilian Defense: Najdorf" -> "Sicilian Defense"
    st.families[g.opening.substr(0, g.opening.find(':'))]++;
}

void readHeader(string &line, Game &game) {
    string key = headerKey(line);
    string value = headerValue(line);
    if (key=="Result") {
        if (value=="1-0") game.result = WHITE_WINS;
        else if (value=="1/2-1/2") game.result = DRAW;
        else if (value=="0-1") game.result = BLACK_WINS;
    }
    else if (key=="WhiteElo") game.whiteRating = toRating(value);
    else if (key=="BlackElo") game.blackRating = toRating(value);
    else if (key=="Event") game.event = value;
    else if (key=="TimeControl") game.timeControl = value;
    else if (key=="Termination") game.abandoned = (value=="Abandoned");
    else if (key=="Opening") game.opening = value;
}

// "lichess_db_standard_rated_2024-01.pgn.zst" -> "2024-01"
// only the file name is searched, so a folder like ".../run_2024-01_2024-02/" can't be mistaken for it
string monthOf(string path) {
    int slash = path.rfind('/');
    if (slash!=(int)string::npos) path = path.substr(slash + 1);
    for (int i=0; i+7<=(int)path.size(); i++) {
        string s = path.substr(i, 7);
        bool isMonth = isdigit(s[0]) && isdigit(s[1]) && isdigit(s[2]) && isdigit(s[3]) &&
                s[4]=='-' && isdigit(s[5]) && isdigit(s[6]);
        if (isMonth) return s;
    }
    return path;
}

// the threads print at the same time, so one line at a time
mutex printLock;

string clockTime() {
    time_t now = time(nullptr);
    char text[20];
    strftime(text, sizeof text, "%H:%M:%S", localtime(&now));
    return text;
}

// every CHECKPOINT_EVERY games: print how far it got and save the counts so far
void checkpoint(string month, Stats &st, chrono::steady_clock::time_point start) {
    double seconds = chrono::duration<double>(chrono::steady_clock::now() - start).count();
    bool saved = writeOneMonth("partial_counts_" + month + ".csv", month, st, "partial");
    lock_guard<mutex> lock(printLock);
    cout << "[" << clockTime() << "] " << month << ": " << st.seen << " games read, " << st.used
            << " used, " << (int)(st.seen / max(seconds, 1.0)) << " games/s"
            << (saved ? ", checkpoint saved" : ", CHECKPOINT NOT SAVED") << endl;
}

// with --checkpoint-every, each month is saved as counts_<month>.csv as soon as it's done
void finishFile(string month, Stats &st) {
    string status = "complete";
    if (st.problem!="") status = "incomplete: " + st.problem;
    else if (MAX_GAMES>0) status = "test run (max-games " + to_string(MAX_GAMES) + ")";
    bool saved = writeOneMonth("counts_" + month + ".csv", month, st, status);
    if (saved) remove(("partial_counts_" + month + ".csv").c_str());
    lock_guard<mutex> lock(printLock);
    cout << "[" << clockTime() << "] " << month << " finished (" << status << "): " << st.seen
            << " games read" << (saved ? ", saved counts_" + month + ".csv" : ", COULD NOT SAVE") << endl;
}

// reads every game in one file; only the header lines matter, the moves are skipped
void processFile(string path, Stats &st) {
    string month = monthOf(path);
    auto start = chrono::steady_clock::now();
    FileReader reader(path);
    string line;
    Game game;
    bool stoppedEarly = false;
    while (reader.readLine(line)) {
        if (!line.empty() && line.back()=='\r') line.pop_back();
        if (line.empty()) continue;

        if (line[0]=='[') {
            game.hasHeaders = true;
            readHeader(line, game);
        }
        else if (game.hasHeaders) {
            // reached the moves, so this game's headers are complete
            addGame(game, st);
            game = Game();
            if (CHECKPOINT_EVERY>0 && st.seen % CHECKPOINT_EVERY==0) checkpoint(month, st, start);
            if (MAX_GAMES>0 && st.seen>=MAX_GAMES) {
                stoppedEarly = true;
                break;
            }
        }
    }
    if (!stoppedEarly) {
        if (game.hasHeaders) addGame(game, st);
        st.problem = reader.problem;
    }
    if (st.problem!="") {
        lock_guard<mutex> lock(printLock);
        cout << "WARNING: " << month << ": " << st.problem << " after " << st.seen << " games" << endl;
    }
    if (CHECKPOINT_EVERY>0) finishFile(month, st);
}

