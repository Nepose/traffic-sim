"use strict";

// ---------------------------------------------------------------------------
// Configuration
// ---------------------------------------------------------------------------

const API_BASE = window.location.origin;

const ROAD = { NORTH: 0, SOUTH: 1, EAST: 2, WEST: 3 };
const ROAD_NAMES = ["North", "South", "East", "West"];
const LANE_NAMES = ["left", "straight", "right"];
const PHASE_NAMES = ["NS", "EW", "NS Left-only", "EW Left-only"];

// Canvas colours
const COLORS = {
  road:        "#2c2f3a",
  asphalt:     "#1e2130",
  marking:     "#444",
  divider:     "#f0c040",
  RED:         "#e03131",
  YELLOW:      "#f59f00",
  GREEN:       "#2f9e44",
  GREEN_ARROW: "#000011",
  arrow:       "#2f9e44",
  lightOff:    "#333",
  lightRing:   "#555",
  queueDot:    "#74c0fc",
  label:       "#888",
};

// ---------------------------------------------------------------------------
// State
// ---------------------------------------------------------------------------

let state = null;          // last IntersectionState from API
let autoTimer = null;

// ---------------------------------------------------------------------------
// Canvas setup
// ---------------------------------------------------------------------------

const canvas = document.getElementById("intersection");
const ctx = canvas.getContext("2d");
const W = canvas.width;
const H = canvas.height;
const CX = W / 2;
const CY = H / 2;

const BOX   = 80;   // half-width of the junction box
const LANE  = 22;   // lane width in pixels
const LPAD  = 10;   // padding before first lane

// ---------------------------------------------------------------------------
// Draw helpers
// ---------------------------------------------------------------------------

function clearCanvas() {
  ctx.fillStyle = COLORS.asphalt;
  ctx.fillRect(0, 0, W, H);
}

function drawRoads() {
  ctx.fillStyle = COLORS.road;
  // Horizontal road (East-West)
  ctx.fillRect(0, CY - BOX, W, BOX * 2);
  // Vertical road (North-South)
  ctx.fillRect(CX - BOX, 0, BOX * 2, H);

  // Junction box (overlap area)
  ctx.fillStyle = COLORS.road;
  ctx.fillRect(CX - BOX, CY - BOX, BOX * 2, BOX * 2);

  // Centre divider lines (dashed)
  ctx.setLineDash([12, 8]);
  ctx.lineWidth = 1.5;
  ctx.strokeStyle = COLORS.divider;

  ctx.beginPath();
  ctx.moveTo(CX, 0); ctx.lineTo(CX, CY - BOX);
  ctx.moveTo(CX, CY + BOX); ctx.lineTo(CX, H);
  ctx.stroke();

  ctx.beginPath();
  ctx.moveTo(0, CY); ctx.lineTo(CX - BOX, CY);
  ctx.moveTo(CX + BOX, CY); ctx.lineTo(W, CY);
  ctx.stroke();

  ctx.setLineDash([]);
}

// Draw a single traffic light circle at (x, y)
function drawLight(x, y, lightState) {
  const R = 10;
  ctx.beginPath();
  ctx.arc(x, y, R + 2, 0, Math.PI * 2);
  ctx.fillStyle = COLORS.lightRing;
  ctx.fill();

  ctx.beginPath();
  ctx.arc(x, y, R, 0, Math.PI * 2);
  const stateKey = lightStateName(lightState);
  ctx.fillStyle = COLORS[stateKey] ?? COLORS.lightOff;
  ctx.fill();

  // Arrow indicator for GREEN_ARROW
  if (lightState === 3) {
    ctx.fillStyle = COLORS.arrow;
    ctx.font = "bold 14px sans-serif";
    ctx.textAlign = "center";
    ctx.color = COLORS.text_arrow;
    ctx.textBaseline = "middle";
    ctx.fillText("←", x, y);
  }
}

function lightStateName(s) {
  return ["RED", "YELLOW", "GREEN", "GREEN_ARROW"][s] ?? "lightOff";
}

// Draw queue count badge at (x, y)
function drawQueue(x, y, count) {
  if (count === 0) return;
  const label = count > 99 ? "99+" : String(count);
  ctx.font = "bold 11px monospace";
  ctx.textAlign = "center";
  ctx.textBaseline = "middle";
  const tw = ctx.measureText(label).width;
  const bw = Math.max(tw + 8, 20);
  const bh = 16;
  ctx.fillStyle = "#1a1d27cc";
  ctx.beginPath();
  ctx.roundRect(x - bw / 2, y - bh / 2, bw, bh, 4);
  ctx.fill();
  ctx.fillStyle = COLORS.queueDot;
  ctx.fillText(label, x, y);
}

// ---------------------------------------------------------------------------
// Per-road drawing
// ---------------------------------------------------------------------------

/*
 * Road layout (looking at North road):
 *   Right lane   (LANE_RIGHT=2) - closest to the East
 *   Straight     (LANE_STRAIGHT=1)
 *   Left lane    (LANE_LEFT=0) - closest to the West
 *
 * For drawing, each road rotates around the junction centre.
 */

function drawNorthRoad(road) {
  const light = road.light;
  // Light box position: above the junction, within the northbound lanes
  const lx = CX - BOX / 2;
  const ly = CY - BOX - 28;
  drawLight(lx, ly, light.state);

  // Lane queue badges (top of road, going up)
  const lanes = road.lanes;
  const qy = CY - BOX - 30;
  drawQueue(CX - LANE - 40,     qy, lanes.right?.queue_length     ?? 0);
  drawQueue(CX - 40,            qy, lanes.straight?.queue_length ?? 0);
  drawQueue(CX + LANE - 40,     qy, lanes.left?.queue_length    ?? 0);

  // Lane labels
  ctx.font = "9px sans-serif";
  ctx.textAlign = "center";
  ctx.textBaseline = "top";
  ctx.fillStyle = COLORS.label;
  ctx.fillText("R", CX - LANE - 40, CY - BOX - 18);
  ctx.fillText("S", CX - 40,        CY - BOX - 18);
  ctx.fillText("L", CX + LANE - 40, CY - BOX - 18);
}

function drawSouthRoad(road) {
  const light = road.light;
  // Light box position: below the junction, within the southbound lanes
  const lx = CX + BOX / 2;
  const ly = CY + BOX + 28;
  drawLight(lx, ly, light.state);

  const lanes = road.lanes;
  const qy = CY + BOX + 22;
  // Mirror: left is to East side of road
  drawQueue(CX + LANE + 40, qy, lanes.right?.queue_length     ?? 0);
  drawQueue(CX + 40,        qy, lanes.straight?.queue_length ?? 0);
  drawQueue(CX - LANE + 40, qy, lanes.left?.queue_length    ?? 0);

  ctx.font = "9px sans-serif";
  ctx.textAlign = "center";
  ctx.textBaseline = "bottom";
  ctx.fillStyle = COLORS.label;
  ctx.fillText("R", CX + LANE + 40, CY + BOX + 10);
  ctx.fillText("S", CX + 40,        CY + BOX + 10);
  ctx.fillText("L", CX - LANE + 40, CY + BOX + 10);
}

function drawEastRoad(road) {
  const light = road.light;
  // Light box position: right of the junction, within the eastbound lanes
  const lx = CX + BOX + 28;
  const ly = CY - BOX / 2;
  drawLight(lx, ly, light.state);

  const lanes = road.lanes;
  const qx = CX + BOX + 30;
  drawQueue(qx, CY - LANE - 40, lanes.right?.queue_length     ?? 0);
  drawQueue(qx, CY - 40,        lanes.straight?.queue_length ?? 0);
  drawQueue(qx, CY + LANE - 40, lanes.left?.queue_length    ?? 0);

  ctx.font = "9px sans-serif";
  ctx.textAlign = "left";
  ctx.textBaseline = "middle";
  ctx.fillStyle = COLORS.label;
  ctx.fillText("R", CX + BOX + 18, CY - LANE - 40);
  ctx.fillText("S", CX + BOX + 18, CY - 40);
  ctx.fillText("L", CX + BOX + 18, CY + LANE - 40);
}

function drawWestRoad(road) {
  const light = road.light;
  // Light box position: left of the junction, within the westbound lanes
  const lx = CX - BOX - 28;
  const ly = CY + BOX / 2;
  drawLight(lx, ly, light.state);

  const lanes = road.lanes;
  const qx = CX - BOX - 30;
  drawQueue(qx, CY + LANE + 40, lanes.right?.queue_length     ?? 0);
  drawQueue(qx, CY + 40,        lanes.straight?.queue_length ?? 0);
  drawQueue(qx, CY - LANE + 40, lanes.left?.queue_length    ?? 0);

  ctx.font = "9px sans-serif";
  ctx.textAlign = "right";
  ctx.textBaseline = "middle";
  ctx.fillStyle = COLORS.label;
  ctx.fillText("R", CX - BOX - 18, CY + LANE + 40);
  ctx.fillText("S", CX - BOX - 18, CY + 40);
  ctx.fillText("L", CX - BOX - 18, CY - LANE + 40);
}

// Road direction labels
function drawDirectionLabels() {
  ctx.font = "bold 13px sans-serif";
  ctx.fillStyle = "#aaa";
  const pairs = [
    ["al. Jana Pawła II", CX, 16, "center", "top"],
    ["al. Jana Pawła II", CX, H - 16, "center", "bottom"],
    ["Okopowa", W - 16, CY, "right", "middle"],
    ["Okopowa", 16, CY, "left", "middle"],
  ];
  for (const [label, x, y, ta, tb] of pairs) {
    ctx.textAlign = ta;
    ctx.textBaseline = tb;
    ctx.fillText(label, x, y);
  }
}

const DRAW_FNS = [drawNorthRoad, drawSouthRoad, drawEastRoad, drawWestRoad];

function render(s) {
  clearCanvas();
  drawRoads();
  drawDirectionLabels();
  if (!s) return;
  for (let i = 0; i < 4; i++) {
    DRAW_FNS[i](s.roads[i]);
  }
}

// ---------------------------------------------------------------------------
// UI state updates
// ---------------------------------------------------------------------------

function updateUI(s) {
  document.getElementById("step-count").textContent = s.step_count;
  document.getElementById("phase-name").textContent = PHASE_NAMES[s.current_phase] ?? "?";
  const badge = document.getElementById("yellow-badge");
  badge.classList.toggle("hidden", !s.in_yellow_transition);

  // Queue table
  const tbody = document.getElementById("queue-body");
  tbody.innerHTML = "";
  for (const road of s.roads) {
    const tr = document.createElement("tr");
    const l = road.lanes.left?.queue_length ?? 0;
    const st = road.lanes.straight?.queue_length ?? 0;
    const r = road.lanes.right?.queue_length ?? 0;
    tr.innerHTML = `
      <td>${ROAD_NAMES[road.direction]}</td>
      <td class="${l ? "nonzero" : ""}">${l}</td>
      <td class="${st ? "nonzero" : ""}">${st}</td>
      <td class="${r ? "nonzero" : ""}">${r}</td>
    `;
    tbody.appendChild(tr);
  }
}

function showDepartures(vehicles) {
  const ul = document.getElementById("departed-list");
  ul.innerHTML = "";
  if (!vehicles.length) {
    ul.innerHTML = '<li class="muted">none</li>';
    return;
  }
  for (const v of vehicles) {
    const li = document.createElement("li");
    li.textContent = v;
    ul.appendChild(li);
  }
}

function setFeedback(msg, isError = false) {
  const el = document.getElementById("add-feedback");
  el.textContent = msg;
  el.className = "feedback" + (isError ? " error" : "");
}

// ---------------------------------------------------------------------------
// API helpers
// ---------------------------------------------------------------------------

async function fetchState() {
  const resp = await fetch(`${API_BASE}/api/state`);
  if (!resp.ok) throw new Error(`State fetch failed: ${resp.status}`);
  return resp.json();
}

async function doStep() {
  const resp = await fetch(`${API_BASE}/api/step`, { method: "POST" });
  if (!resp.ok) throw new Error(`Step failed: ${resp.status}`);
  return resp.json();
}

async function doReset() {
  await fetch(`${API_BASE}/api/reset`, { method: "POST" });
}

async function doAddVehicle(id, from, to) {
  const resp = await fetch(`${API_BASE}/api/vehicles`, {
    method: "POST",
    headers: { "Content-Type": "application/json" },
    body: JSON.stringify({ vehicle_id: id, start_road: from, end_road: to }),
  });
  const data = await resp.json();
  if (!resp.ok) {
    const detail = data?.detail ?? "Unknown error";
    throw new Error(typeof detail === "string" ? detail : JSON.stringify(detail));
  }
  return data;
}

// ---------------------------------------------------------------------------
// Refresh loop
// ---------------------------------------------------------------------------

async function refresh() {
  try {
    state = await fetchState();
    render(state);
    updateUI(state);
  } catch (e) {
    console.error(e);
  }
}

// ---------------------------------------------------------------------------
// Event wiring
// ---------------------------------------------------------------------------

document.getElementById("btn-step").addEventListener("click", async () => {
  try {
    const result = await doStep();
    showDepartures(result.left_vehicles);
    state = await fetchState();
    render(state);
    updateUI(state);
  } catch (e) {
    console.error(e);
  }
});

document.getElementById("btn-reset").addEventListener("click", async () => {
  stopAuto();
  await doReset();
  showDepartures([]);
  await refresh();
});

document.getElementById("btn-add").addEventListener("click", async () => {
  const id   = document.getElementById("veh-id").value.trim();
  const from = parseInt(document.getElementById("veh-from").value);
  const to   = parseInt(document.getElementById("veh-to").value);

  if (!id) { setFeedback("Enter a vehicle ID", true); return; }
  if (from === to) { setFeedback("Start and end must differ", true); return; }

  try {
    await doAddVehicle(id, from, to);
    setFeedback(`Added ${id}`);
    document.getElementById("veh-id").value = "";
    state = await fetchState();
    render(state);
    updateUI(state);
  } catch (e) {
    setFeedback(e.message, true);
  }
});

function startAuto() {
  if (autoTimer) return;
  const ms = parseInt(document.getElementById("auto-interval").value) || 1000;
  autoTimer = setInterval(async () => {
    try {
      const result = await doStep();
      showDepartures(result.left_vehicles);
      state = await fetchState();
      render(state);
      updateUI(state);
    } catch (e) {
      stopAuto();
    }
  }, ms);
}

function stopAuto() {
  clearInterval(autoTimer);
  autoTimer = null;
  document.getElementById("auto-step").checked = false;
}

document.getElementById("auto-step").addEventListener("change", (e) => {
  e.target.checked ? startAuto() : stopAuto();
});

// ---------------------------------------------------------------------------
// Boot
// ---------------------------------------------------------------------------

refresh();
