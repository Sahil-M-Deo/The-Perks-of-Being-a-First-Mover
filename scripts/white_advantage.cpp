/*
   white_advantage.cpp
   Measures white's advantage on Lichess, adjusted for the rating gap in each game.
   See README.md for how to compile and run it.
*/

#include <bits/stdc++.h>
#include <zstd.h>

#define int long long
#define v1d vector<int>
#define v2d vector<v1d>
#define v3d vector<v2d>

using namespace std;

// settings
const int BAND_WIDTH = 100;   // rating brackets are [1500,1600), [1600,1700), ...
const int MAX_DIFF = 100;     // skip games where the rating gap is bigger than this
const int MIN_GAMES = 1000;   // don't show brackets with fewer games than this

const string CSV_FILE = "output.csv";   // results table, saved in the folder you run from

// can be changed from the command line
int MAX_GAMES = 0;
string REPORT_FILE = "";   // numbers for the LaTeX report, see writeReport()

const int MAX_RATING = 4000;

// UltraBullet must come before Bullet, otherwise "UltraBullet" games match "Bullet" first
enum Speed { ULTRABULLET, BULLET, BLITZ, RAPID, CLASSICAL, CORRESPONDENCE, OTHER, NUM_SPEEDS };
string speedNames[NUM_SPEEDS] = {"UltraBullet", "Bullet", "Blitz", "Rapid",
                                 "Classical", "Correspondence", "Other"};

enum GameResult { WHITE_WINS, DRAW, BLACK_WINS, NO_RESULT };

// white's score doubled, so a draw is 1 instead of 0.5 and we can keep everything as ints
int halfPointsFor(GameResult r) {
    if (r == WHITE_WINS) return 2;
    if (r == DRAW) return 1;
    return 0;
}

int numBands() { return MAX_RATING / BAND_WIDTH + 1; }
int numDiffs() { return 2 * MAX_DIFF + 1; }

/*
   games[speed][band][diff + MAX_DIFF]      = number of games
   halfPoints[speed][band][diff + MAX_DIFF] = white's total score (doubled)
   diff = whiteRating - blackRating, shifted by MAX_DIFF so the index is never negative
*/
struct Stats {
    v3d games, halfPoints;
    int seen = 0, used = 0;
    int noResult = 0, abandoned = 0, noRating = 0, bigGap = 0;

    Stats() {
        games = v3d(NUM_SPEEDS, v2d(numBands(), v1d(numDiffs(), 0)));
        halfPoints = games;
    }

    void add(Stats &other) {
        for (int s = 0; s < NUM_SPEEDS; s++)
            for (int b = 0; b < numBands(); b++)
                for (int d = 0; d < numDiffs(); d++) {
                    games[s][b][d] += other.games[s][b][d];
                    halfPoints[s][b][d] += other.halfPoints[s][b][d];
                }
        seen += other.seen;
        used += other.used;
        noResult += other.noResult;
        abandoned += other.abandoned;
        noRating += other.noRating;
        bigGap += other.bigGap;
    }
};

/*
   Reads a .pgn or .pgn.zst file line by line.

   Lichess gives out its game files compressed with zstd (.zst). A month is
   tens of GB compressed and much bigger unzipped, so instead of unzipping it
   to disk first we unzip it in memory as we read it, 1 MB at a time.

   How the zstd library works here:
     - ZSTD_DCtx is the "decompressor". It remembers where it is in the file
       between calls, so we can feed it the file in pieces.
     - ZSTD_inBuffer  = the compressed bytes we read from the file.
       ZSTD_outBuffer = where it should write the unzipped text.
       Both have a .pos that zstd moves forward as it reads/writes.
     - ZSTD_decompressStream() unzips as much as it can from the input into
       the output and returns. We call it again and again until the file ends.
*/
class FileReader {
    FILE *file;
    bool compressed;
    ZSTD_DCtx *zstd = nullptr;
    vector<char> inBuffer, outBuffer;
    ZSTD_inBuffer input;
    string buffer;      // text we've read but not handed out as lines yet
    int pos = 0;        // where the next line starts in buffer
    bool finished = false;

    // gets the next 1 MB-ish block of text, returns false at end of file
    bool readChunk(string &chunk) {
        if (!compressed) {
            int n = fread(outBuffer.data(), 1, outBuffer.size(), file);
            if (n <= 0) return false;
            chunk.assign(outBuffer.data(), n);
            return true;
        }
        while (true) {
            // zstd has used up all the compressed bytes we gave it, so read more from the file
            if (input.pos == input.size) {
                int n = fread(inBuffer.data(), 1, inBuffer.size(), file);
                if (n > 0) {
                    input.src = inBuffer.data();
                    input.size = n;
                    input.pos = 0;
                }
                else {
                    /*
                       end of the file, but zstd might still be holding some unzipped
                       text inside it, so ask for that before stopping
                    */
                    ZSTD_outBuffer output = {outBuffer.data(), outBuffer.size(), 0};
                    ZSTD_inBuffer empty = {nullptr, 0, 0};
                    ZSTD_decompressStream(zstd, &output, &empty);
                    if (output.pos == 0) return false;
                    chunk.assign(outBuffer.data(), output.pos);
                    return true;
                }
            }
            // unzip into outBuffer; output.pos tells us how many bytes it wrote
            ZSTD_outBuffer output = {outBuffer.data(), outBuffer.size(), 0};
            ZSTD_decompressStream(zstd, &output, &input);
            // sometimes it needs more input before it can write anything, so loop again
            if (output.pos > 0) {
                chunk.assign(outBuffer.data(), output.pos);
                return true;
            }
        }
    }

public:
    FileReader(string path) {
        file = fopen(path.c_str(), "rb");
        compressed = path.size() > 4 && path.substr(path.size() - 4) == ".zst";
        outBuffer.resize(1 << 20);
        if (compressed) {
            zstd = ZSTD_createDCtx();
            /*
               Lichess files are compressed with a bigger "window" than zstd allows by
               default, so without this line it refuses to unzip them
            */
            ZSTD_DCtx_setParameter(zstd, ZSTD_d_windowLogMax, 31);
            inBuffer.resize(1 << 20);
            input = {inBuffer.data(), 0, 0};
        }
    }

    ~FileReader() {
        if (zstd) ZSTD_freeDCtx(zstd);
        if (file) fclose(file);
    }

    bool readLine(string &line) {
        while (true) {
            int newline = buffer.find('\n', pos);
            if (newline != string::npos) {
                line = buffer.substr(pos, newline - pos);
                pos = newline + 1;
                return true;
            }
            if (finished) {
                // last line of the file might not end with '\n'
                if (pos < (int)buffer.size()) {
                    line = buffer.substr(pos);
                    pos = buffer.size();
                    return true;
                }
                return false;
            }
            // only half a line left in buffer, so keep it and read the next chunk after it
            buffer = buffer.substr(pos);
            pos = 0;
            string chunk;
            if (readChunk(chunk)) buffer += chunk;
            else finished = true;
        }
    }
};

// header lines look like [WhiteElo "1540"]  ->  key = WhiteElo, value = 1540
string headerKey(string &line) {
    int space = line.find(' ');
    if (space == string::npos) return "";
    return line.substr(1, space - 1);
}

string headerValue(string &line) {
    int first = line.find('"');
    int last = line.rfind('"');
    if (first == string::npos || last <= first) return "";
    return line.substr(first + 1, last - first - 1);
}

// some games have "?" instead of a rating, those return -1
int toRating(string s) {
    if (s.empty()) return -1;
    for (char c : s)
        if (c < '0' || c > '9') return -1;
    return stoi(s);
}

// Lichess puts the speed in the Event header, like "Rated Blitz game"
Speed speedOf(string &event, string &timeControl) {
    for (int i = ULTRABULLET; i < OTHER; i++)
        if (event.find(speedNames[i]) != string::npos) return (Speed)i;
    if (timeControl == "-") return CORRESPONDENCE;
    return OTHER;
}

struct Game {
    GameResult result = NO_RESULT;
    int whiteRating = -1, blackRating = -1;
    bool abandoned = false;
    string event, timeControl;
    bool hasHeaders = false;
};

void addGame(Game &g, Stats &stats) {
    stats.seen++;
    if (g.result == NO_RESULT) { stats.noResult++; return; }
    if (g.abandoned) { stats.abandoned++; return; }
    if (g.whiteRating == -1 || g.blackRating == -1) { stats.noRating++; return; }

    int diff = g.whiteRating - g.blackRating;
    if (abs(diff) > MAX_DIFF) { stats.bigGap++; return; }

    // bracket by the average of both ratings (capped so a weird rating can't go out of range)
    int average = min((g.whiteRating + g.blackRating) / 2, MAX_RATING);
    int band = average / BAND_WIDTH;
    Speed speed = speedOf(g.event, g.timeControl);

    stats.games[speed][band][diff + MAX_DIFF]++;
    stats.halfPoints[speed][band][diff + MAX_DIFF] += halfPointsFor(g.result);
    stats.used++;
}

void processFile(string path, Stats &stats) {
    FileReader reader(path);
    string line;
    Game game;
    while (reader.readLine(line)) {
        // files saved on Windows end lines with \r\n
        if (!line.empty() && line.back() == '\r') line.pop_back();
        if (line.empty()) continue;

        if (line[0] == '[') {
            game.hasHeaders = true;
            string key = headerKey(line);
            string value = headerValue(line);
            if (key == "Result") {
                if (value == "1-0") game.result = WHITE_WINS;
                else if (value == "1/2-1/2") game.result = DRAW;
                else if (value == "0-1") game.result = BLACK_WINS;
            }
            else if (key == "WhiteElo") game.whiteRating = toRating(value);
            else if (key == "BlackElo") game.blackRating = toRating(value);
            else if (key == "Event") game.event = value;
            else if (key == "TimeControl") game.timeControl = value;
            else if (key == "Termination") game.abandoned = (value == "Abandoned");
        }
        else if (game.hasHeaders) {
            /*
               reached the moves, so we have all the headers for this game.
               we don't need the moves themselves, so we just skip them
            */
            addGame(game, stats);
            game = Game();
            if (MAX_GAMES > 0 && stats.seen >= MAX_GAMES) return;
        }
    }
    if (game.hasHeaders) addGame(game, stats);
}

struct Result {
    int games = 0;
    double score = 0, expected = 0, h = 0, error = 0;
};

/*
   Finds h, white's advantage in rating points.

   If white were h points stronger than their rating, the expected score for a
   game with rating gap diff would be  e = 1 / (1 + 10^(-(diff + h) / 400)).
   We want the h where the expected scores add up to the actual scores.

   We start at h = 0 and use Newton's method to nudge it:
     gradient    = how far actual scores are from expected (positive -> h is too small)
     information = how much the expected scores change when h changes
     h += gradient / information
   This settles in a few steps. "information" also tells us how precise h is:
   error = 1 / sqrt(information), and the 95% CI is h +- 1.96 * error.

   K just converts the base-10 formula above into exp(), since 10^x = exp(x * ln 10).
   games[i] and halfPoints[i] are for rating gap diff = i - MAX_DIFF.
*/
Result fitWhiteAdvantage(v1d &games, v1d &halfPoints) {
    const double K = log(10.0) / 400.0;
    Result r;
    double totalScore = 0, totalExpected = 0;

    for (int i = 0; i < numDiffs(); i++) {
        int diff = i - MAX_DIFF;
        r.games += games[i];
        totalScore += halfPoints[i] / 2.0;
        totalExpected += games[i] / (1 + exp(-K * diff));
    }
    if (r.games == 0) return r;

    double h = 0, information = 0;
    for (int iter = 0; iter < 50; iter++) {
        double gradient = 0;
        information = 0;
        for (int i = 0; i < numDiffs(); i++) {
            if (games[i] == 0) continue;
            int diff = i - MAX_DIFF;
            double e = 1 / (1 + exp(-K * (diff + h)));
            gradient += K * (halfPoints[i] / 2.0 - games[i] * e);
            information += K * K * games[i] * e * (1 - e);
        }
        double step = gradient / information;
        h += step;
        if (fabs(step) < 1e-6) break;
    }

    r.h = h;
    r.error = 1 / sqrt(information);
    r.score = totalScore / r.games;
    r.expected = totalExpected / r.games;
    return r;
}

// "[1500,1600)" for bracket b
string bandName(int b) {
    int low = b * BAND_WIDTH;
    return "[" + to_string(low) + "," + to_string(low + BAND_WIDTH) + ")";
}

// h for one speed, all brackets added together
Result fitSpeed(Stats &stats, int s) {
    v1d games(numDiffs(), 0), points(numDiffs(), 0);
    for (int b = 0; b < numBands(); b++)
        for (int d = 0; d < numDiffs(); d++) {
            games[d] += stats.games[s][b][d];
            points[d] += stats.halfPoints[s][b][d];
        }
    return fitWhiteAdvantage(games, points);
}

// h for everything: all speeds and all brackets
Result fitAll(Stats &stats) {
    v1d games(numDiffs(), 0), points(numDiffs(), 0);
    for (int s = 0; s < NUM_SPEEDS; s++)
        for (int b = 0; b < numBands(); b++)
            for (int d = 0; d < numDiffs(); d++) {
                games[d] += stats.games[s][b][d];
                points[d] += stats.halfPoints[s][b][d];
            }
    return fitWhiteAdvantage(games, points);
}

FILE *csv = nullptr;

void printRow(string speed, string band, Result r) {
    printf("%-15s%-14s%12lld%8.3f%10.3f%+10.3f%+10.1f%8.1f\n", speed.c_str(), band.c_str(),
           r.games, r.score, r.expected, r.score - r.expected, r.h, 1.96 * r.error);
    fprintf(csv, "%s,\"%s\",%lld,%.6f,%.6f,%.6f,%.4f,%.4f\n", speed.c_str(), band.c_str(),
            r.games, r.score, r.expected, r.score - r.expected, r.h, r.error);
}

void printResults(Stats &stats) {
    csv = fopen(CSV_FILE.c_str(), "w");
    fprintf(csv, "speed,band,games,score,expected,residual,h_points,h_error\n");

    cout << "\nWhite advantage, adjusted for rating gap\n"
         << "  score    = white's actual average score\n"
         << "  expected = average score predicted by ratings alone\n"
         << "  residual = score - expected (white's edge in score)\n"
         << "  h        = white's edge in rating points, with 95% confidence interval\n\n";
    printf("%-15s%-14s%12s%8s%10s%10s%10s%8s\n", "Speed", "Band", "Games", "Score",
           "Expected", "Residual", "h (pts)", "95% CI");
    cout << string(87, '-') << "\n";

    // overall row: all speeds and brackets together
    printRow("All", "all", fitAll(stats));

    // then for each speed: one row for all brackets, then one row per bracket
    for (int s = 0; s < NUM_SPEEDS; s++) {
        Result total = fitSpeed(stats, s);
        if (total.games < MIN_GAMES) continue;
        printRow(speedNames[s], "all", total);

        for (int b = 0; b < numBands(); b++) {
            Result r = fitWhiteAdvantage(stats.games[s][b], stats.halfPoints[s][b]);
            if (r.games < MIN_GAMES) continue;
            printRow(speedNames[s], bandName(b), r);
        }
    }

    fclose(csv);
    cout << "\nSaved " << CSV_FILE << "\n";
}

/*
   Writing the numbers for the LaTeX report.

   report.tex does \input{report_numbers.tex}, and this writes that file:
   one \newcommand per number (e.g. \GamesRead), the rows of the two tables,
   and the figure as a pgfplots chart. So after running this program you only
   need to run pdflatex to get the finished report.
*/

// 1234567 -> "1,234,567"
string withCommas(int n) {
    string s = to_string(n), out = "";
    int digits = 0;
    for (int i = (int)s.size() - 1; i >= 0; i--) {
        out = s[i] + out;
        digits++;
        if (digits % 3 == 0 && i > 0) out = "," + out;
    }
    return out;
}

// number with a fixed number of decimals, e.g. num(35.04, 1, true) -> "+35.0"
string num(double x, int decimals, bool plusSign = false) {
    string format = "%." + to_string(decimals) + "f";
    if (plusSign) format = "%+." + to_string(decimals) + "f";
    char buf[64];
    snprintf(buf, sizeof(buf), format.c_str(), x);
    return buf;
}

// pulls "2024-01" out of "lichess_db_standard_rated_2024-01.pgn.zst"
string monthOf(string path) {
    for (int i = 0; i + 7 <= (int)path.size(); i++) {
        string s = path.substr(i, 7);
        if (isdigit(s[0]) && isdigit(s[1]) && isdigit(s[2]) && isdigit(s[3]) && s[4] == '-' &&
            isdigit(s[5]) && isdigit(s[6]))
            return s;
    }
    // no date in the name, so just use the name ('_' has to be escaped in LaTeX)
    string out = "";
    for (char c : path) {
        if (c == '_') out += "\\_";
        else out += c;
    }
    return out;
}

string percent(int part, int whole) {
    return num(100.0 * part / whole, 1) + "\\%";
}

void macro(FILE *f, string name, string value) {
    fprintf(f, "\\newcommand{\\%s}{%s}\n", name.c_str(), value.c_str());
}

void writeReport(Stats &total, vector<string> &files, vector<Stats> &perFile) {
    FILE *f = fopen(REPORT_FILE.c_str(), "w");
    fprintf(f, "%% written by white_advantage (--report), don't edit by hand\n");

    // dataset size and cleaning
    string months = "";
    for (int i = 0; i < (int)files.size(); i++) {
        if (i > 0) months += (i == (int)files.size() - 1) ? " and " : ", ";
        months += monthOf(files[i]);
    }
    macro(f, "MonthList", months);
    macro(f, "GamesRead", withCommas(total.seen));
    macro(f, "GamesUsed", withCommas(total.used));
    macro(f, "PctUsed", percent(total.used, total.seen));
    macro(f, "SkipNoResult", withCommas(total.noResult));
    macro(f, "SkipAbandoned", withCommas(total.abandoned));
    macro(f, "SkipNoRating", withCommas(total.noRating));
    macro(f, "SkipGap", withCommas(total.bigGap));

    string fileRows = "";
    for (int i = 0; i < (int)files.size(); i++)
        fileRows += monthOf(files[i]) + " & " + withCommas(perFile[i].seen) + " & " +
                    withCommas(perFile[i].used) + " & " + percent(perFile[i].used, perFile[i].seen) +
                    " \\\\\n";
    macro(f, "FileRows", fileRows);

    // overall result
    Result all = fitAll(total);
    macro(f, "ScoreAll", num(all.score, 3));
    macro(f, "ExpectedAll", num(all.expected, 3));
    macro(f, "HAll", num(all.h, 1, true));
    macro(f, "HAllCI", num(1.96 * all.error, 1));

    // speeds with enough games, the one with the most games first
    vector<pair<int, int>> speeds;  // (games, speed)
    for (int s = 0; s < NUM_SPEEDS; s++) {
        Result r = fitSpeed(total, s);
        if (r.games >= MIN_GAMES) speeds.push_back({r.games, s});
    }
    sort(speeds.begin(), speeds.end(), greater<pair<int, int>>());

    string speedRows = "";
    for (int i = 0; i < (int)speeds.size(); i++) {
        int s = speeds[i].second;
        Result r = fitSpeed(total, s);
        speedRows += speedNames[s] + " & " + withCommas(r.games) + " & " + num(r.score, 3) + " & " +
                     num(r.expected, 3) + " & $" + num(r.h, 1, true) + " \\pm " +
                     num(1.96 * r.error, 1) + "$ \\\\\n";
    }
    macro(f, "SpeedRows", speedRows);

    /*
       smallest and largest h across the brackets of the most played speed.
       on a tiny test run (--max-games) no bracket may have enough games, then these stay "--"
    */
    int top = 0;
    for (int s = 0; s < NUM_SPEEDS; s++)
        if (fitSpeed(total, s).games > fitSpeed(total, top).games) top = s;
    int count = 0, minBand = 0, maxBand = 0;
    double minH = 1e18, maxH = -1e18;
    for (int b = 0; b < numBands(); b++) {
        Result r = fitWhiteAdvantage(total.games[top][b], total.halfPoints[top][b]);
        if (r.games < MIN_GAMES) continue;
        count++;
        if (r.h < minH) { minH = r.h; minBand = b; }
        if (r.h > maxH) { maxH = r.h; maxBand = b; }
    }
    string hMin = "--", hMax = "--", hMinBand = "--", hMaxBand = "--";
    if (count > 0) {
        hMin = num(minH, 1, true);
        hMax = num(maxH, 1, true);
        hMinBand = bandName(minBand);
        hMaxBand = bandName(maxBand);
    }
    macro(f, "MainSpeed", speedNames[top]);
    macro(f, "MainBands", to_string(count));
    macro(f, "HMin", hMin);
    macro(f, "HMinBand", hMinBand);
    macro(f, "HMax", hMax);
    macro(f, "HMaxBand", hMaxBand);

    /*
       figure: h for every bracket, one line per speed (the 3 most played speeds),
       with the 95% CI as error bars. pgfplots draws it inside LaTeX.
    */
    string colors[3] = {"blue!70!black", "orange!90!black", "green!50!black"};
    string fig = "\\begin{tikzpicture}\n"
                 "\\begin{axis}[width=\\linewidth, height=5.8cm, grid=major, grid style={gray!25},\n"
                 "  xlabel={Rating bracket (average of both players)}, ylabel={$h$ (rating points)},\n"
                 "  legend style={draw=none, font=\\small, at={(0.5,1.02)}, anchor=south}, legend columns=3,\n"
                 "  tick label style={font=\\small},\n"
                 "  label style={font=\\small}, xticklabel style={/pgf/number format/1000 sep={}},\n"
                 "  extra y ticks={0}, extra y tick labels={}, extra y tick style={grid=major, major grid style={gray}}]\n";
    for (int i = 0; i < (int)speeds.size() && i < 3; i++) {
        int s = speeds[i].second;
        fig += "\\addplot+[" + colors[i] + ", mark=*, mark size=1.3pt, mark options={fill=" + colors[i] +
               "}, error bars/.cd, y dir=both, y explicit, error bar style={" + colors[i] + "}] coordinates {\n";
        for (int b = 0; b < numBands(); b++) {
            Result r = fitWhiteAdvantage(total.games[s][b], total.halfPoints[s][b]);
            if (r.games < MIN_GAMES) continue;
            int middle = b * BAND_WIDTH + BAND_WIDTH / 2;
            fig += "(" + to_string(middle) + "," + num(r.h, 2) + ") +- (0," + num(1.96 * r.error, 2) + ")\n";
        }
        fig += "};\n\\addlegendentry{" + speedNames[s] + "}\n";
    }
    fig += "\\end{axis}\n\\end{tikzpicture}";
    macro(f, "Figure", fig);

    fclose(f);
    cout << "Saved " << REPORT_FILE << "\n";

    /*
       build the PDF too. report.tex is in the same folder as report_numbers.tex,
       so go there and run pdflatex (needs LaTeX with pgfplots installed)
    */
    string folder = ".";
    int slash = REPORT_FILE.rfind('/');
    if (slash != string::npos) folder = REPORT_FILE.substr(0, slash);
    string command = "cd " + folder + " && pdflatex -interaction=nonstopmode -jobname=checkpointOne report.tex > /dev/null";
    if (system(command.c_str()) == 0) cout << "Saved " << folder << "/checkpointOne.pdf\n";
    else cout << "Couldn't build the PDF (is pdflatex installed?), see " << folder << "/checkpointOne.log\n";
}

signed main(signed argc, char *argv[]) {
    vector<string> files;
    for (int i = 1; i < argc; i++) {
        string arg = argv[i];
        if (arg == "--max-games" && i + 1 < argc) MAX_GAMES = atoll(argv[++i]);
        else if (arg == "--report" && i + 1 < argc) REPORT_FILE = argv[++i];
        else files.push_back(arg);
    }
    if (files.empty()) {
        cout << "Usage: ./white_advantage [options] file1.pgn.zst [file2.pgn.zst ...]\n";
        return 0;
    }

    // each file gets its own thread and its own Stats, so they never touch each other's counts
    int n = files.size();
    vector<Stats> stats(n);
    vector<thread> threads;
    for (int i = 0; i < n; i++)
        threads.push_back(thread(processFile, files[i], ref(stats[i])));
    for (int i = 0; i < n; i++)
        threads[i].join();

    Stats total;
    for (int i = 0; i < n; i++) {
        cout << files[i] << ": " << stats[i].seen << " games read, " << stats[i].used << " used\n";
        total.add(stats[i]);
    }

    cout << "\nTotal games read: " << total.seen << "   used: " << total.used << "\n";
    cout << "  skipped (no result): " << total.noResult << "\n";
    cout << "  skipped (abandoned): " << total.abandoned << "\n";
    cout << "  skipped (missing rating): " << total.noRating << "\n";
    cout << "  skipped (rating gap too big): " << total.bigGap << "\n";

    printResults(total);
    if (REPORT_FILE != "") writeReport(total, files, stats);
    return 0;
}
