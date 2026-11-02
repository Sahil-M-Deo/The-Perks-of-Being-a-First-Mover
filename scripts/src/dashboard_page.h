#pragma once
#include "common.h"

/*
   The dashboard page: HTML, CSS and JavaScript. writeDashboard() writes PAGE_START,
   then the data (const DATA = {...}), then PAGE_END.

   R"HTML( ... )HTML" is a C++ raw string: everything between the brackets is taken
   exactly as written, so the quotes and backslashes in the page don't need escaping.
   The charts are drawn by Plotly (plotly.js), loaded from its website.
*/
string PAGE_START = R"HTML(<!doctype html>
<html lang="en">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>First Mover Dashboard</title>
<script src="https://cdn.plot.ly/plotly-2.35.2.min.js"></script>
<style>
  :root { --bg: #fbfaf7; --card: #ffffff; --text: #1d1d1f; --muted: #6b6b70; --line: #e4e2dc;
          --accent: #2f5d8a; }
  @media (prefers-color-scheme: dark) {
    :root { --bg: #141416; --card: #1d1d20; --text: #ececee; --muted: #9a9aa2; --line: #2e2e33;
            --accent: #7fb0e0; }
  }
  * { box-sizing: border-box; }
  body { margin: 0; background: var(--bg); color: var(--text);
         font: 15px/1.5 -apple-system, "Segoe UI", Roboto, Helvetica, Arial, sans-serif; }
  main { max-width: 1100px; margin: 0 auto; padding: 24px 16px 48px; }
  h1 { font-size: 26px; margin: 0 0 4px; }
  .sub { color: var(--muted); margin: 0 0 20px; }
  .controls { display: flex; flex-wrap: wrap; gap: 16px 28px; background: var(--card);
              border: 1px solid var(--line); border-radius: 10px; padding: 14px 16px; margin-bottom: 16px; }
  .controls label.head { display: block; font-weight: 600; font-size: 13px; color: var(--muted);
                         text-transform: uppercase; letter-spacing: .04em; margin-bottom: 6px; }
  select { font: inherit; padding: 4px 8px; border-radius: 6px; border: 1px solid var(--line);
           background: var(--bg); color: var(--text); }
  .checks { display: flex; flex-wrap: wrap; gap: 4px 14px; max-width: 760px; }
  .checks label { white-space: nowrap; cursor: pointer; }
  .group-tag { font-size: 11px; color: var(--muted); }
  .card { background: var(--card); border: 1px solid var(--line); border-radius: 10px;
          padding: 8px 8px 0; margin-bottom: 16px; }
  .chart { width: 100%; height: 360px; }
  .grid { display: grid; grid-template-columns: 1fr 1fr; gap: 16px; }
  @media (max-width: 760px) { .grid { grid-template-columns: 1fr; } }
  .note { color: var(--muted); font-size: 13px; padding: 0 8px 10px; }
  .table-wrap { overflow-x: auto; }
  table { border-collapse: collapse; width: 100%; font-variant-numeric: tabular-nums; }
  th, td { padding: 6px 10px; border-bottom: 1px solid var(--line); text-align: right; white-space: nowrap; }
  th:first-child, td:first-child { text-align: left; }
  th { font-size: 13px; color: var(--muted); font-weight: 600; }
</style>
</head>
<body>
<main>
  <h1>The Perks of Being a First Mover</h1>
  <p class="sub" id="sub"></p>

  <div class="controls">
    <div>
      <label class="head" for="mode">Compare</label>
      <select id="mode">
        <option value="openings">Openings</option>
        <option value="speeds">Speeds</option>
      </select>
    </div>
    <div>
      <label class="head" for="fixed" id="fixedLabel">Speed</label>
      <select id="fixed"></select>
    </div>
    <div>
      <label class="head">Show</label>
      <div class="checks" id="checks"></div>
    </div>
  </div>

  <div class="card">
    <div id="hChart" class="chart"></div>
    <div class="note">h = White's advantage in rating points, fitted separately in each 100-point
      bracket (average of both players' ratings), with 95% confidence intervals.
      Brackets with fewer than 1,000 games are left out.</div>
  </div>
  <div class="grid">
    <div class="card"><div id="drawChart" class="chart"></div></div>
    <div class="card"><div id="gamesChart" class="chart"></div></div>
  </div>
  <div class="card table-wrap">
    <table id="summary"></table>
    <div class="note">White / Draw / Black = share of games won by White, drawn, and won by Black.</div>
  </div>
</main>
<script>
)HTML";

string PAGE_END = R"HTML(
const COLORS = ["#2f5d8a", "#c8553d", "#3a8d6e", "#d08c2f", "#7b5ea7", "#4b9bb8", "#a14d6f",
                "#6d8b3a", "#b07a52", "#5d6dbb", "#c25f9b", "#3f7f7f", "#8a7d2f", "#9c4f3f",
                "#4f6f9f", "#888888"];
/*
   Two ways to compare:
   - openings: pick one speed, tick openings (or the groups); one line per opening
   - speeds:   pick one set of games, tick speeds; one line per speed
   speed index 0 in DATA is "All speeds"
*/
const checkedSeries = new Set([0, 1, 2]);
const checkedSpeeds = new Set([2, 3, 4]);   // Bullet, Blitz, Rapid
const modeSel = document.getElementById("mode");
const fixedSel = document.getElementById("fixed");
const checks = document.getElementById("checks");

function css(name) { return getComputedStyle(document.documentElement).getPropertyValue(name).trim(); }
function pct(x) { return (100 * x).toFixed(1) + "%"; }

function layout(title, ytitle, extra) {
  return Object.assign({
    title: { text: title, font: { size: 15 } },
    paper_bgcolor: "rgba(0,0,0,0)", plot_bgcolor: "rgba(0,0,0,0)",
    font: { color: css("--text") },
    margin: { l: 56, r: 16, t: 44, b: 64 },
    xaxis: { title: { text: "Rating bracket (average of both players)" }, gridcolor: css("--line"), zeroline: false },
    yaxis: { title: { text: ytitle }, gridcolor: css("--line"), zerolinecolor: css("--muted") },
    legend: { orientation: "h", y: -0.3 },
    hovermode: "closest"
  }, extra || {});
}

// the lines to draw: [series index, speed index, name, colour]
function lines() {
  const fixed = +fixedSel.value;
  if (modeSel.value === "openings")
    return [...checkedSeries].sort((a, b) => a - b).map(k => [k, fixed, DATA.series[k], k]);
  return [...checkedSpeeds].sort((a, b) => a - b).map(s => [fixed, s, DATA.speeds[s], s]);
}

function draw() {
  const hTraces = [], drawTraces = [], gameTraces = [];
  let rows = "<tr><th>" + (modeSel.value === "openings" ? "Games" : "Speed") + "</th><th>Games</th>" +
             "<th>Avg rating</th><th>White</th><th>Draw</th><th>Black</th><th>h (pts)</th><th>95% CI</th></tr>";
  lines().forEach(([k, s, name, c]) => {
    const pts = DATA.bands[k][s];
    const color = COLORS[c % COLORS.length];
    const x = pts.map(p => p[0]);
    hTraces.push({ x, y: pts.map(p => p[1]), name, mode: "lines+markers", line: { color },
      marker: { size: 5 },
      error_y: { type: "data", array: pts.map(p => 1.96 * p[2]), color, thickness: 1, width: 2 },
      hovertemplate: name + "<br>bracket %{x}<br>h = %{y:.1f} pts<extra></extra>" });
    drawTraces.push({ x, y: pts.map(p => 100 * p[5]), name, mode: "lines+markers", line: { color },
      marker: { size: 4 }, hovertemplate: name + "<br>bracket %{x}<br>draws %{y:.1f}%<extra></extra>" });
    gameTraces.push({ x, y: pts.map(p => p[3]), name, mode: "lines", line: { color },
      hovertemplate: name + "<br>bracket %{x}<br>%{y:,} games<extra></extra>" });
    const t = DATA.totals[k][s];
    const tag = modeSel.value === "openings" && k > 2 ? " <span class='group-tag'>" + DATA.groups[k] + "</span>" : "";
    if (t[2] > 0)
      rows += "<tr><td>" + name + tag + "</td><td>" + t[2].toLocaleString() + "</td><td>" + t[6] +
              "</td><td>" + pct(t[3]) + "</td><td>" + pct(t[4]) + "</td><td>" + pct(t[5]) + "</td><td>" +
              (t[0] >= 0 ? "+" : "") + t[0].toFixed(1) + "</td><td>&plusmn; " + (1.96 * t[1]).toFixed(1) + "</td></tr>";
  });
  const cfg = { responsive: true, displaylogo: false };
  Plotly.react("hChart", hTraces, layout("White's advantage by rating", "h (rating points)"), cfg);
  Plotly.react("drawChart", drawTraces, layout("Draw rate by rating", "draws (%)"), cfg);
  Plotly.react("gamesChart", gameTraces, layout("Games per bracket", "games (log scale)"), cfg);
  Plotly.relayout("gamesChart", { "yaxis.type": "log" });
  document.getElementById("summary").innerHTML = rows;
}

function addOption(select, value, text) {
  const opt = document.createElement("option");
  opt.value = value;
  opt.textContent = text;
  select.appendChild(opt);
}

function addCheck(name, value, set) {
  const label = document.createElement("label");
  const box = document.createElement("input");
  box.type = "checkbox";
  box.checked = set.has(value);
  box.addEventListener("change", () => { box.checked ? set.add(value) : set.delete(value); draw(); });
  label.appendChild(box);
  label.append(" " + name);
  checks.appendChild(label);
}

// rebuild the controls when switching between comparing openings and comparing speeds
function setMode() {
  fixedSel.innerHTML = "";
  checks.innerHTML = "";
  if (modeSel.value === "openings") {
    document.getElementById("fixedLabel").textContent = "Speed";
    DATA.speeds.forEach((name, s) => { if (DATA.totals[0][s][2] > 0) addOption(fixedSel, s, name); });
    DATA.series.forEach((name, k) => {
      if (DATA.totals[k][0][2] > 0 && DATA.groups[k] !== "Extra") addCheck(name, k, checkedSeries);
    });
  } else {
    document.getElementById("fixedLabel").textContent = "Games";
    DATA.series.forEach((name, k) => {
      if (DATA.totals[k][0][2] > 0 && DATA.groups[k] !== "Extra") addOption(fixedSel, k, name);
    });
    DATA.speeds.forEach((name, s) => { if (s > 0 && DATA.totals[0][s][2] > 0) addCheck(name, s, checkedSpeeds); });
  }
  draw();
}

document.getElementById("sub").textContent = "White's advantage on Lichess, adjusted for the rating gap in every game. " +
  "Standard rated games, " + DATA.months + ": " + DATA.games.toLocaleString() + " games.";
modeSel.addEventListener("change", setMode);
fixedSel.addEventListener("change", draw);
setMode();
</script>
</body>
</html>
)HTML";
