"use strict";
// Transporter menu. Transporter_SF.dll registers "targets" and "beam"; the script
// (Transporter_SF.AnswerTargets) answers the "targets" request with
// strings of the form
//   meta|<units per meter>|<blocked reason or empty>|<scripts' build>|<area name>
//   (older scripts leave out the build)
//   ship|<id>|<distance in units>|<1 available, 0 not>|<note>|<name>
//   poi|<id>|<distance in units>|<1 available, 0 not>|<note>|<name>
// The name comes last so a "|" inside it survives. "beam" (id) closes the view and runs Transporter_SF.Teleport;
// "close" is OSF UI's own endpoint and closes this view.

const $ = (id) => document.getElementById(id);
const shipList = $("ship");
const poiList = $("pois");
const filter = $("filter");
const energize = $("energize");

let meta = { unitsPerMeter: 70, blocked: "", area: "" };
let ship = [];
let pois = [];
let visible = [];   // the rows on screen, ship first, in keyboard order
let selected = -1;  // index into visible

// This page's build; shown in the footer and compared with the scripts' build, so a stale
// copy of the page (not copied, or cached) shows itself.
const PAGE_VERSION = "0.7.0";

// Energize is disabled only when it can't work: nothing selectable, a target that isn't
// available, a reason from the game (meta.blocked), or the second right after a beam (so
// a double click doesn't beam twice). The last is a timestamp, not a flag, so it can't get
// stuck while the page sits hidden between openings.
const BEAM_LOCK_MS = 1500;
let lastBeamAt = 0;
const beaming = () => Date.now() - lastBeamAt < BEAM_LOCK_MS;

function parse(lines) {
  ship = [];
  pois = [];
  for (const line of Array.isArray(lines) ? lines : []) {
    const parts = String(line).split("|");
    if (parts[0] === "meta") {
      const withBuild = parts.length >= 5;
      meta = {
        unitsPerMeter: Number(parts[1]) > 0 ? Number(parts[1]) : 70,
        blocked: parts[2] || "",
        build: withBuild ? parts[3] : "",
        area: parts.slice(withBuild ? 4 : 3).join("|"),
      };
      continue;
    }
    if (parts.length < 6) continue;
    const row = {
      kind: parts[0],
      id: parts[1],
      units: Number(parts[2]),
      available: parts[3] === "1",
      note: parts[4],
      name: parts.slice(5).join("|") || parts[1],
    };
    (row.kind === "ship" ? ship : pois).push(row);
  }
}

function distance(units) {
  if (!Number.isFinite(units) || units < 0) return "";
  const meters = units / meta.unitsPerMeter;
  return meters >= 1000 ? (meters / 1000).toFixed(1) + " km" : Math.round(meters) + " m";
}

function item(row) {
  const li = document.createElement("li");
  li.setAttribute("role", "option");
  li.setAttribute("aria-selected", "false");
  if (!row.available) li.setAttribute("aria-disabled", "true");
  const name = document.createElement("span");
  name.className = "name";
  name.textContent = row.name;
  const dist = document.createElement("span");
  dist.className = "dist";
  dist.textContent = distance(row.units);
  li.append(name, dist);
  if (row.note) {
    const note = document.createElement("span");
    note.className = "note";
    note.textContent = row.note;
    li.append(note);
  }
  li.addEventListener("click", () => select(visible.indexOf(row)));
  li.addEventListener("dblclick", () => { select(visible.indexOf(row)); beam(); });
  row.element = li;
  return li;
}

function empty(text) {
  const li = document.createElement("li");
  li.className = "empty";
  li.textContent = text;
  return li;
}

function render() {
  const keep = selected >= 0 ? visible[selected] : null;
  const words = filter.value.trim().toLowerCase();
  const shown = words ? pois.filter((p) => p.name.toLowerCase().includes(words)) : pois;

  shipList.replaceChildren(...(ship.length ? ship.map(item) : [empty("No home ship.")]));
  poiList.replaceChildren(...(shown.length ? shown.map(item)
    : [empty(pois.length ? "Nothing matches the filter." : "No points of interest in range.")]));
  $("poiCount").textContent = pois.length ? "(" + (shown.length === pois.length ? pois.length : shown.length + " of " + pois.length) + ")" : "";
  $("area").textContent = meta.area || "Unknown location";

  visible = ship.concat(shown);
  let next = keep ? visible.indexOf(keep) : -1;
  if (next < 0) next = visible.findIndex((r) => r.available);
  select(next);
}

function select(index) {
  if (selected >= 0 && visible[selected] && visible[selected].element) {
    visible[selected].element.setAttribute("aria-selected", "false");
  }
  selected = index >= 0 && index < visible.length ? index : -1;
  const row = selected >= 0 ? visible[selected] : null;
  if (row && row.element) {
    row.element.setAttribute("aria-selected", "true");
    row.element.scrollIntoView({ block: "nearest" });
  }
  energize.disabled = !row || !row.available || meta.blocked !== "" || beaming();
}

function move(step) {
  if (!visible.length) return;
  let index = selected;
  for (let tries = 0; tries < visible.length; tries++) {
    index = (index + step + visible.length) % visible.length;
    if (visible[index].available) break;
  }
  select(index);
}

function status(text, kind) {
  const el = $("status");
  el.textContent = text;
  el.className = kind || "";
}

function beam() {
  const row = selected >= 0 ? visible[selected] : null;
  if (beaming() || !row || !row.available || meta.blocked) return;
  lastBeamAt = Date.now();
  energize.disabled = true;
  status("Energizing — " + row.name, "energizing");
  osfui.send("beam", row.id);
  // The DLL closes the menu right away. Whenever the page runs next, put it back to normal.
  setTimeout(() => {
    if ($("status").className === "energizing") status("");
    select(selected);
  }, BEAM_LOCK_MS + 50);
}

const SCAN_TIMEOUT_MS = 30000;
let scanStartedAt = 0;  // a scan waiting for its answer, or 0
let scannedAt = 0;

async function scan() {
  if (scanStartedAt && Date.now() - scanStartedAt < SCAN_TIMEOUT_MS + 1000) return;
  scanStartedAt = Date.now();
  status("Scanning for transporter targets…");
  energize.disabled = true;
  let lines;
  try {
    lines = await osfui.request("targets", undefined, { timeoutMs: SCAN_TIMEOUT_MS });
  } catch (err) {
    scanStartedAt = 0;
    select(selected);
    if (err.code === "unknown-endpoint") {
      status("Transporter offline: Transporter_SF.dll isn't connected to OSF UI. See Transporter_SF.log.", "error");
    } else if (err.code === "no-bridge") {
      status("Preview mode: no game connection.", "error");
    } else {
      status("Scan failed: " + (err.message || err.code), "error");
    }
    return;
  }
  scanStartedAt = 0;
  scannedAt = Date.now();
  parse(lines);
  render();
  if (meta.blocked) status(meta.blocked, "error");
  else if (meta.build && meta.build !== PAGE_VERSION) {
    status("This menu page is build " + PAGE_VERSION + " but the scripts are " + meta.build +
      ". Copy the matching main.js; if this stays, delete OSF UI's views cache.", "error");
  } else status(pois.length || ship.length ? "" : "No targets.");
}

$("close").addEventListener("click", () => osfui.send("close"));
$("rescan").addEventListener("click", scan);
energize.addEventListener("click", beam);
filter.addEventListener("input", render);

document.addEventListener("keydown", (event) => {
  switch (event.key) {
    case "Escape":
      event.preventDefault();
      if (filter.value) { filter.value = ""; render(); } else osfui.send("close");
      break;
    case "ArrowDown":
      event.preventDefault();
      move(1);
      break;
    case "ArrowUp":
      event.preventDefault();
      move(-1);
      break;
    case "Enter":
      event.preventDefault();
      beam();
      break;
    default:
      if (event.key.length === 1 && document.activeElement !== filter && !event.ctrlKey && !event.altKey) {
        filter.focus();
      }
  }
});

// The page stays loaded between openings, so each opening scans again. Transporter_SF.dll emits
// "opened" when OSF UI shows the view; visibility and focus are fallbacks (at most one
// extra scan every few seconds).
function reopen() {
  if (Date.now() - scannedAt > 3000) scan();
  else select(selected);
}
osfui.on("opened", () => scan());
document.addEventListener("visibilitychange", () => {
  if (document.visibilityState === "visible") reopen();
});
window.addEventListener("focus", reopen);

$("build").textContent = "v" + PAGE_VERSION;
scan();
