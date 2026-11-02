#pragma once
#include "common.h"
#include "settings.h"
#include "openings.h"
#include "stats.h"
#include "fit.h"
#include "tests.h"
#include "format.h"

/*
   Writes report_numbers.tex, which reports/final/report.tex reads with \input.
   Every number becomes a LaTeX command, e.g. \newcommand{\GamesRead}{190,562,735},
   so report.tex can just write \GamesRead. Tables are whole rows, and figures are
   pgfplots charts (pgfplots draws plots inside LaTeX). Then pdflatex builds the PDF.
*/

void macro(FILE *f, string name, string value) {
    fprintf(f, "\\newcommand{\\%s}{%s}\n", name.c_str(), value.c_str());
}

// a table row: "name & games & avg rating & white & draw & black & h \\" (& separates columns)
string resultRow(string name, Result r) {
    return name + " & " + withCommas(r.games) + " & " + num(r.avgRating, 0) + " & " +
           percent(r.whiteWins) + " & " + percent(r.draws) + " & " + percent(r.blackWins) + " & " +
           withCI(r.h, r.error) + " \\\\\n";
}

string compareRow(string name, Comparison c) {
    double z = c.diff / c.error;
    return name + " & " + withCI(c.a.h, c.a.error) + " & " + withCI(c.b.h, c.b.error) + " & " +
           withCI(c.diff, c.error) + " & " + num(z, 1, true) + " & " + pText(pValue(z)) + " \\\\\n";
}

// ----- charts

enum Measure { MEASURE_H, MEASURE_DRAWS, MEASURE_WHITE, MEASURE_BLACK };

struct Line {
    string label;
    int opening, group, speed;
    Measure measure;
};

double measureOf(Result r, Measure m) {
    if (m == MEASURE_DRAWS) return 100 * r.draws;
    if (m == MEASURE_WHITE) return 100 * r.whiteWins;
    if (m == MEASURE_BLACK) return 100 * r.blackWins;
    return r.h;
}

/*
   A pgfplots line chart, one line per Line, x = rating bracket.
   "\addplot+[...] coordinates {(x,y) +- (0,e) ...}" draws one line; "+- (0,e)" is its
   error bar. Colours like "blue!70!black" mean 70% blue mixed with black.
*/
string lineChart(Stats &st, vector<Line> lines, string ylabel) {
    string colors[5] = {"blue!70!black", "red!70!black", "green!50!black", "orange!90!black",
                        "violet!80!black"};
    string marks[5] = {"*", "square*", "triangle*", "diamond*", "pentagon*"};
    string fig = "\\begin{tikzpicture}\n"
                 "\\begin{axis}[width=\\linewidth, height=6cm, grid=major, grid style={gray!25},\n"
                 "  xlabel={Rating bracket (average of both players)}, ylabel={" + ylabel + "},\n"
                 "  legend style={draw=none, font=\\small, at={(0.5,1.02)}, anchor=south},\n"
                 "  legend columns=5,\n"
                 "  tick label style={font=\\small}, label style={font=\\small},\n"
                 "  xticklabel style={/pgf/number format/1000 sep={}}]\n";
    for (int i = 0; i < (int)lines.size(); i++) {
        Line l = lines[i];
        string c = colors[i % 5];
        fig += "\\addplot+[" + c + ", mark=" + marks[i % 5] + ", mark size=1.3pt";
        fig += ", mark options={fill=" + c + "}";
        if (l.measure == MEASURE_H)
            fig += ", error bars/.cd, y dir=both, y explicit, error bar style={" + c + "}";
        fig += "] coordinates {\n";
        for (int b = 0; b < numBands(); b++) {
            Result r = fit(st, l.opening, l.group, l.speed, b);
            if (r.games < MIN_GAMES) continue;
            fig += "(" + to_string(bandMiddle(b)) + "," + num(measureOf(r, l.measure), 2) + ")";
            if (l.measure == MEASURE_H) fig += " +- (0," + num(1.96 * r.error, 2) + ")";
            fig += "\n";
        }
        fig += "};\n\\addlegendentry{" + l.label + "}\n";
    }
    fig += "\\end{axis}\n\\end{tikzpicture}";
    return fig;
}

// one bar per opening or speed; bars with the same colour number are drawn together
struct Bar {
    string label;
    Result r;
    int color;
};

string barChart(vector<Bar> bars, vector<string> legend, string xlabel) {
    string colors[3] = {"red!70!black", "blue!70!black", "teal!70!black"};
    int n = bars.size();

    // bars start at 0, so the axis starts at 0 (or lower if an error bar goes below 0)
    double xmin = 0;
    for (Bar bar : bars) xmin = min(xmin, bar.r.h - 1.96 * bar.r.error);

    // pgfplots puts bar 1 at the bottom, so the labels go in reverse order
    string labels = "";
    for (int i = n - 1; i >= 0; i--) {
        if (i < n - 1) labels += ",";
        labels += "{" + bars[i].label + "}";
    }

    string fig = "\\begin{tikzpicture}\n"
                 "\\begin{axis}[xbar, bar width=7pt, width=0.8\\linewidth, height=" + num(1.2 + 0.62 * n, 1) +
                 "cm, xmin=" + num(xmin, 1) + ",\n"
                 "  ytick={1,...," + to_string(n) + "}, yticklabels={" + labels + "},\n"
                 "  xlabel={" + xlabel + "}, grid=major, grid style={gray!25},\n"
                 "  tick label style={font=\\small}, label style={font=\\small},\n"
                 "  legend style={draw=none, font=\\small, at={(0.5,1.02)}, anchor=south},\n"
                 "  legend columns=2]\n";
    for (int c = 0; c < 3; c++) {
        bool used = false;
        for (Bar bar : bars)
            if (bar.color == c) used = true;
        if (!used) continue;
        fig += "\\addplot+[bar shift=0pt, draw=" + colors[c] + ", fill=" + colors[c] + "!40, "
               "error bars/.cd, x dir=both, x explicit, error bar style={black}] coordinates {\n";
        for (int i = 0; i < n; i++) {
            if (bars[i].color != c) continue;
            fig += "(" + num(bars[i].r.h, 2) + "," + to_string(n - i) + ") +- (" +
                   num(1.96 * bars[i].r.error, 2) + ",0)\n";
        }
        fig += "};\n";
        if (c < (int)legend.size()) fig += "\\addlegendentry{" + legend[c] + "}\n";
    }
    fig += "\\end{axis}\n\\end{tikzpicture}";
    return fig;
}

// ----- the sections of the report

void writeDataNumbers(FILE *f, Stats &st, vector<string> &months, vector<Stats> &perFile) {
    macro(f, "MonthList", monthList(months));
    macro(f, "GamesRead", withCommas(st.seen));
    macro(f, "GamesUsed", withCommas(st.used));
    macro(f, "PctUsed", percent((double)st.used / st.seen));
    macro(f, "SkipNoResult", withCommas(st.noResult));
    macro(f, "SkipAbandoned", withCommas(st.abandoned));
    macro(f, "SkipNoRating", withCommas(st.noRating));
    macro(f, "SkipGap", withCommas(st.bigGap));

    string rows = "";
    for (int i = 0; i < (int)months.size(); i++)
        rows += months[i] + " & " + withCommas(perFile[i].seen) + " & " + withCommas(perFile[i].used) +
                " & " + percent((double)perFile[i].used / perFile[i].seen) + " \\\\\n";
    macro(f, "FileRows", rows);
}

void writeOverallNumbers(FILE *f, Stats &st) {
    Result all = fit(st, -1, -1, -1, -1);
    macro(f, "HAll", num(all.h, 1, true));
    macro(f, "HAllCI", num(1.96 * all.error, 1));
    macro(f, "ScoreAll", num(all.score, 3));
    macro(f, "ExpectedAll", num(all.expected, 3));
    macro(f, "WhiteAll", percent(all.whiteWins));
    macro(f, "DrawAll", percent(all.draws));
    macro(f, "BlackAll", percent(all.blackWins));

    macro(f, "FigResults", lineChart(st, {{"White wins", -1, -1, -1, MEASURE_WHITE},
                                          {"Draws", -1, -1, -1, MEASURE_DRAWS},
                                          {"Black wins", -1, -1, -1, MEASURE_BLACK}},
                                     "share of games (\\%)"));
}

string openingDefinitionRow(int o) {
    string names = "", excluded = "";
    for (int i = 0; i < (int)openings[o].prefixes.size(); i++) {
        if (i > 0) names += ", ";
        names += "\\emph{" + openings[o].prefixes[i] + "}";
    }
    for (int i = 0; i < (int)openings[o].excludes.size(); i++) {
        if (i > 0) excluded += ", ";
        excluded += openings[o].excludes[i];
    }
    if (excluded == "") excluded = "--";
    return openingName(o) + " & " + groupNames[groupOf(o)] + " & " + names + " & " + excluded + " \\\\\n";
}

void writeOpeningNumbers(FILE *f, Stats &st, Analysis &a) {
    string rows = "", definitions = "";
    vector<Bar> bars;
    int inOpenings = 0;
    for (int o = 0; o < (int)openings.size(); o++) {
        if (groupOf(o) == EXTRA) continue;
        Result r = fit(st, o, -1, -1, -1);
        inOpenings += r.games;
        rows += resultRow(openingName(o), r);
        if (o == 3) rows += "\\midrule\n";   // a line between the aggressive and the drawish ones
        definitions += openingDefinitionRow(o);
        bars.push_back({openingName(o), r, groupOf(o)});
    }
    macro(f, "OpeningRows", rows);
    macro(f, "OpeningDefRows", definitions);
    macro(f, "OpeningGames", withCommas(inOpenings));
    macro(f, "OpeningShare", percent((double)inOpenings / st.used));

    string groupRows = "";
    for (int g = AGGRESSIVE; g <= DRAWISH; g++)
        groupRows += resultRow(groupNames[g] + " (all 4)", fit(st, -1, g, -1, -1));
    groupRows += resultRow("Other openings", fit(st, otherOpening(), -1, -1, -1));
    macro(f, "GroupRows", groupRows);

    string compareRows = compareRow("All speeds", a.groups);
    for (int s = BULLET; s <= RAPID; s++) compareRows += compareRow(speedNames[s], a.groupsSpeed[s]);
    macro(f, "CompareRows", compareRows);
    macro(f, "CompareDiff", withCI(a.groups.diff, a.groups.error));
    macro(f, "CompareZ", num(a.groups.diff / a.groups.error, 1));
    macro(f, "CompareP", pText(pValue(a.groups.diff / a.groups.error)));

    macro(f, "FigOpenings", barChart(bars, {groupNames[AGGRESSIVE], groupNames[DRAWISH]},
                                     "$h$ (rating points, all speeds)"));
    macro(f, "FigGroupH", lineChart(st, {{"All games", -1, -1, -1, MEASURE_H},
                                         {"Aggressive", -1, AGGRESSIVE, -1, MEASURE_H},
                                         {"Drawish", -1, DRAWISH, -1, MEASURE_H}},
                                    "$h$ (rating points)"));
    macro(f, "FigGroupDraws", lineChart(st, {{"All games", -1, -1, -1, MEASURE_DRAWS},
                                             {"Aggressive", -1, AGGRESSIVE, -1, MEASURE_DRAWS},
                                             {"Drawish", -1, DRAWISH, -1, MEASURE_DRAWS}},
                                        "draws (\\%)"));
}

void writeSpeedNumbers(FILE *f, Stats &st, Analysis &a) {
    string rows = resultRow("All speeds", fit(st, -1, -1, -1, -1));
    vector<Bar> bars;
    vector<Line> hLines, drawLines;
    for (int s = 0; s < NUM_SPEEDS; s++) {
        if (!enoughGames(st, s)) continue;
        Result r = fit(st, -1, -1, s, -1);
        rows += resultRow(speedNames[s], r);
        if (s == OTHER) continue;   // games Lichess didn't label with a speed: table only
        bars.push_back({speedNames[s], r, 2});
        if (a.hasLine[s]) {
            hLines.push_back({speedNames[s], -1, -1, s, MEASURE_H});
            drawLines.push_back({speedNames[s], -1, -1, s, MEASURE_DRAWS});
        }
    }
    macro(f, "SpeedRows", rows);

    string stepRows = "";
    for (Comparison c : a.speedSteps) stepRows += compareRow(c.first + " vs " + c.second, c);
    macro(f, "SpeedStepRows", stepRows);

    macro(f, "FigSpeed", lineChart(st, hLines, "$h$ (rating points)"));
    macro(f, "FigSpeedBars", barChart(bars, {}, "$h$ (rating points, all openings)"));
    macro(f, "FigSpeedDraws", lineChart(st, drawLines, "draws (\\%)"));
}

// report.tex is in the same folder as report_numbers.tex, so run pdflatex there
void buildPdf() {
    string folder = ".";
    int slash = REPORT_FILE.rfind('/');
    if (slash != string::npos) folder = REPORT_FILE.substr(0, slash);

    // system() runs a terminal command; pdflatex runs twice so figure numbers come out right
    string latex = "pdflatex -interaction=nonstopmode -jobname=finalReport report.tex > /dev/null";
    string command = "cd " + folder + " && " + latex + " && " + latex;
    if (system(command.c_str()) == 0) cout << "Saved " << folder << "/finalReport.pdf\n";
    else cout << "Couldn't build the PDF (is pdflatex installed?), see " << folder << "/finalReport.log\n";
}

void writeReport(Stats &st, Analysis &a, vector<string> &months, vector<Stats> &perFile) {
    FILE *f = fopen(REPORT_FILE.c_str(), "w");
    fprintf(f, "%% written by white_advantage (--report), don't edit by hand\n");
    writeDataNumbers(f, st, months, perFile);
    writeOverallNumbers(f, st);
    writeOpeningNumbers(f, st, a);
    writeSpeedNumbers(f, st, a);
    fclose(f);
    cout << "Saved " << REPORT_FILE << "\n";
    buildPdf();
}
