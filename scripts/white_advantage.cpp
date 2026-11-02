/*
   white_advantage.cpp
   Measures White's advantage on Lichess in rating points, adjusted for the rating gap in
   every game: overall, by speed, by rating bracket and by opening.
   See README.md for how to compile and run it, and reports/final/Theory.pdf for the maths.

   The code is split by job into the files in src/:
     settings.h      constants, speeds, rating brackets
     openings.h      which Lichess opening names count as which opening
     file_reader.h   reading .pgn / .pgn.zst files line by line
     stats.h         the counts (by opening, speed, bracket and rating gap)
     games.h         reading the games' headers into the counts
     fit.h           the maximum likelihood fit of h
     tests.h         the z-tests
     counts_file.h   saving and loading counts.csv
     tables.h        the tables printed and saved as CSV
     format.h        number formatting
     dashboard.h     dashboard.html (dashboard_page.h has its HTML)
     report.h        the numbers and charts for the LaTeX report
*/

#include "src/common.h"
#include "src/settings.h"
#include "src/openings.h"
#include "src/file_reader.h"
#include "src/stats.h"
#include "src/games.h"
#include "src/fit.h"
#include "src/tests.h"
#include "src/counts_file.h"
#include "src/tables.h"
#include "src/format.h"
#include "src/dashboard.h"
#include "src/report.h"

vector<string> readArguments(signed argc, char *argv[]) {
    vector<string> files;
    for (int i=1; i<argc; i++) {
        string arg = argv[i];
        bool hasValue = i+1<argc;
        if (arg=="--max-games" && hasValue) {
            MAX_GAMES = atoll(argv[i+1]);
            i++;
        }
        else if (arg=="--report" && hasValue) {
            REPORT_FILE = argv[i+1];
            i++;
        }
        else if (arg=="--from-counts" && hasValue) {
            COUNTS_IN = argv[i+1];
            i++;
        }
        else files.push_back(arg);
    }
    return files;
}

// each file gets its own thread and its own Stats, so the threads never share counts
void readGames(vector<string> &files, Stats &total, vector<string> &months, vector<Stats> &perFile) {
    int n = files.size();
    perFile = vector<Stats>(n);
    vector<thread> threads;
    // ref() passes perFile[i] by reference; a thread would otherwise get a copy
    for (int i=0; i<n; i++) threads.push_back(thread(processFile, files[i], ref(perFile[i])));
    for (int i=0; i<n; i++) threads[i].join();   // wait for every thread to finish

    for (int i=0; i<n; i++) {
        months.push_back(monthOf(files[i]));
        total.add(perFile[i]);
    }
}

void printSummary(Stats &total, vector<string> &months, vector<Stats> &perFile) {
    for (int i=0; i<(int)months.size(); i++)
        cout << months[i] << ": " << perFile[i].seen << " games read, " << perFile[i].used << " used\n";
    cout << "\nTotal games read: " << total.seen << "   used: " << total.used << "\n";
    cout << "  skipped (no result): " << total.noResult << "\n";
    cout << "  skipped (abandoned): " << total.abandoned << "\n";
    cout << "  skipped (missing rating): " << total.noRating << "\n";
    cout << "  skipped (rating gap too big): " << total.bigGap << "\n";
}

// main has to return a real int, so it's "signed" here because of the #define
signed main(signed argc, char *argv[]) {
    vector<string> files = readArguments(argc, argv);
    if (files.empty() && COUNTS_IN=="") {
        cout << "Usage: ./white_advantage [options] file1.pgn.zst [file2.pgn.zst ...]\n"
                << "   or: ./white_advantage [options] --from-counts counts.csv\n";
        return 0;
    }

    Stats total;
    vector<string> months;
    vector<Stats> perFile;
    if (COUNTS_IN!="") readCounts(COUNTS_IN, total, months, perFile);
    else readGames(files, total, months, perFile);

    printSummary(total, months, perFile);
    printResults(total);
    printOpenings(total);
    Analysis analysis = analyse(total);
    printAnalysis(analysis);
    writeDashboard("dashboard.html", total, months);
    if (REPORT_FILE!="") writeReport(total, analysis, months, perFile);
    if (COUNTS_IN=="") writeCounts("counts.csv", total, months, perFile);
    return 0;
}
