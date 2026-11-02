#pragma once
#include "common.h"

// 1234567 -> "1,234,567"
string withCommas(int n) {
    string s = to_string(n), out = "";
    int digits = 0;
    for (int i=(int)s.size()-1; i>=0; i--) {
        out = s[i] + out;
        digits++;
        if (digits%3==0 && i>0) out = "," + out;
    }
    return out;
}

// num(35.04, 1, true) -> "+35.0"
string num(double x, int decimals, bool plusSign = false) {
    // printf-style format: "%.1f" = 1 decimal, "%+.1f" = 1 decimal with a + or - sign
    string format = "%." + to_string(decimals) + "f";
    if (plusSign) format = "%+." + to_string(decimals) + "f";
    char buf[64];
    snprintf(buf, sizeof(buf), format.c_str(), x);
    return buf;
}

// {"2024-01", "2024-02"} -> "2024-01 and 2024-02"
string monthList(vector<string> &months) {
    string s = "";
    for (int i=0; i<(int)months.size(); i++) {
        if (i>0 && i==(int)months.size()-1) s += " and ";
        else if (i>0) s += ", ";
        s += months[i];
    }
    return s;
}

// the rest are for LaTeX, where % and \ have to be escaped

string percent(double fraction) {
    return num(100 * fraction, 1) + "\\%";
}

// "$+35.0 \pm 1.4$": a value with its 95% CI (1.96 standard errors)
string withCI(double value, double error, int decimals = 1) {
    return "$" + num(value, decimals, true) + " \\pm " + num(1.96 * error, decimals) + "$";
}

// tiny p-values are shown as an upper bound, the rest in scientific notation
string pText(double p) {
    if (p<1e-300) return "$< 10^{-300}$";
    if (p>=0.001) return "$" + num(p, 3) + "$";
    int exponent = floor(log10(p));
    return "$" + num(p / pow(10, exponent), 1) + " \\times 10^{" + to_string(exponent) + "}$";
}
