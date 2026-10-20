/* speeduino-serial-sim web dashboard.
 *
 * Data: GET api/{status,track}, the api/events SSE stream ("live" 10 Hz,
 * "packet" 2 Hz) and POST api/{control,settings,wifi}. The realtime packet is
 * decoded here by byte offset with the firmware 202501 units, exactly as a
 * gateway would. Dynamic text uses textContent only.
 */
"use strict";
(() => {
  const $ = (id) => document.getElementById(id);
  const store = {
    get(k, d) { try { return localStorage.getItem(k) ?? d; } catch { return d; } },
    set(k, v) { try { localStorage.setItem(k, v); } catch { /* blocked */ } },
  };
  const C = { font: '"Chakra Petch", ui-sans-serif, system-ui, sans-serif', num: '"Doto", "JetBrains Mono", ui-monospace, monospace' };
  function themeColours() {
    const css = getComputedStyle(document.documentElement);
    for (const k of ["display", "cyan", "amber", "green", "red", "text", "muted", "faint"]) C[k] = css.getPropertyValue(`--${k}`).trim();
    C.grid = css.getPropertyValue("--line").trim();
    C.lineHi = css.getPropertyValue("--line-hi").trim();
  }
  const MODES = ["dash", "track", "engine", "control", "system"];
  const STALE_MS = 2000;

  const S = {
    mode: MODES.includes(location.hash.slice(1)) ? location.hash.slice(1) : store.get("mode", "dash"),
    theme: store.get("theme") || (matchMedia("(prefers-color-scheme: light)").matches ? "light" : "dark"),
    live: null, liveAt: 0, packet: null, status: null, track: null, streamUp: false,
    laps: [], lastLapN: null, trace: [], bestTrace: null, trail: [], prevTrail: [],
    settingsLoaded: false, events: 0, rate: 0,
  };

  const isNum = (v) => typeof v === "number" && Number.isFinite(v);
  const fmt = (v, d = 1) => (isNum(v) ? v.toFixed(d) : "—");
  const lapTime = (s) => (isNum(s) && s > 0 ? `${Math.floor(s / 60)}:${(s % 60).toFixed(2).padStart(5, "0")}` : "—");
  const fresh = () => performance.now() - S.liveAt <= STALE_MS;

  function el(tag, cls, text) {
    const e = document.createElement(tag);
    if (cls) e.className = cls;
    if (text != null) e.textContent = text;
    return e;
  }
  function setText(id, v) { const e = $(id); if (e && e.textContent !== v) e.textContent = v; }
  function kv(id, rows) {
    const frag = document.createDocumentFragment();
    for (const [k, v, small] of rows) {
      frag.append(el("dt", null, k));
      const dd = el("dd", null, v);
      if (small) dd.append(el("small", null, small));
      frag.append(dd);
    }
    $(id).replaceChildren(frag);
  }
  function meterInto(box, frac, n, zone) {
    if (box.children.length !== n) box.replaceChildren(...Array.from({ length: n }, () => el("i")));
    const lit = Math.round(Math.max(0, Math.min(1, frac)) * n);
    for (let i = 0; i < n; i++) box.children[i].className = i < lit ? `on ${zone(i / n)}` : "";
  }
  async function post(path, data) {
    const res = await fetch(path, { method: "POST", body: new URLSearchParams(data) });
    return res.json().catch(() => ({}));
  }

  // ── data ──────────────────────────────────────────────────────────────
  function onLive(d) {
    const prev = S.live;
    S.live = d; S.liveAt = performance.now(); S.events++;
    const lap = d.lap;
    if (S.lastLapN !== null && lap.n > S.lastLapN) {
      S.laps.unshift({ n: lap.n, time: lap.last, kind: prev ? prev.phase : "" });
      if (S.laps.length > 30) S.laps.pop();
      if (lap.best > 0 && Math.abs(lap.last - lap.best) < 0.005) S.bestTrace = S.trace;
      S.trace = []; S.prevTrail = S.trail; S.trail = [];
    }
    S.lastLapN = lap.n;
    const moving = d.car.kmh > 0.5 || ["launch", "out-lap", "hot lap", "cool-down"].includes(d.phase);
    if (moving) {
      S.trace.push([lap.dist, d.car.kmh]);
      if (isNum(d.car.x)) S.trail.push([d.car.x, d.car.y, d.car.kmh]);
    }
    schedule();
  }

  function stream() {
    const es = new EventSource("api/events");
    es.addEventListener("open", () => { S.streamUp = true; schedule(); });
    es.addEventListener("error", () => { S.streamUp = false; schedule(); });
    es.addEventListener("live", (e) => { try { onLive(JSON.parse(e.data)); } catch { /* partial */ } });
    es.addEventListener("packet", (e) => { try { S.packet = JSON.parse(e.data).bytes; schedule(); } catch { /* partial */ } });
  }
  async function pollStatus() {
    try {
      S.status = await (await fetch("api/status", { cache: "no-store" })).json();
      if (!S.settingsLoaded && S.status.settings) { fillSettings(S.status.settings); S.settingsLoaded = true; }
    } catch { S.status = null; }
    schedule();
  }
  async function loadTrack() {
    try { S.track = await (await fetch("api/track")).json(); } catch { setTimeout(loadTrack, 3000); }
    schedule();
  }
  setInterval(() => { S.rate = S.events; S.events = 0; }, 1000);

  // ── packet decoding (firmware 202501 units) ───────────────────────────
  const u8 = (b, o) => b[o];
  const s8 = (b, o) => (b[o] << 24) >> 24;
  const u16 = (b, o) => b[o] | (b[o + 1] << 8);
  const s16 = (b, o) => (u16(b, o) << 16) >> 16;
  const temp = (b, o) => b[o] - 40;
  const F = (o, read, label, unit = "", scale = 1, dec = 0) => ({ o, read, label, unit, scale, dec });
  const PACKET = [
    ["Engine", [F(14, u16, "Engine speed", "rpm"), F(33, s16, "RPM rate", "rpm/s"), F(4, u16, "MAP", "kPa"), F(41, u8, "Barometer", "kPa"),
      F(25, u8, "Throttle", "%", 0.5, 1), F(74, u8, "TPS ADC"), F(22, s16, "Throttle rate", "%/s"), F(93, s16, "MAP rate", "kPa/s"),
      F(86, s16, "Fuel load", "kPa"), F(104, u16, "Vehicle speed", "km/h"), F(106, u8, "Gear")]],
    ["Fuel", [F(76, u16, "Pulse width 1", "ms", 0.001, 2), F(78, u16, "Pulse width 2", "ms", 0.001, 2), F(19, u8, "VE 1", "%"),
      F(102, u8, "VE current", "%"), F(21, u8, "AFR target", "", 0.1, 1), F(10, u8, "AFR (O2)", "", 0.1, 1), F(17, u16, "Total correction", "%"),
      F(11, u8, "EGO correction", "%"), F(13, u8, "Warm-up enrich", "%"), F(103, u8, "After-start enrich", "%"), F(16, u8, "Accel enrich", "%", 2),
      F(12, u8, "IAT correction", "%"), F(101, u8, "Baro correction", "%"), F(8, u8, "Battery correction", "%"), F(107, u8, "Fuel pressure", "PSI")]],
    ["Ignition", [F(24, s8, "Advance", "°"), F(118, s8, "Advance table 1", "°"), F(90, u16, "Dwell", "ms", 0.001, 2),
      F(125, u16, "Measured dwell", "ms", 0.001, 2), F(128, u8, "Knock events"), F(129, u8, "Knock retard", "°")]],
    ["Temperatures, electrics", [F(7, temp, "Coolant", "°C"), F(6, temp, "Intake air", "°C"), F(116, temp, "Fuel (flex)", "°C"),
      F(9, u8, "Battery", "V", 0.1, 1), F(108, u8, "Oil pressure", "PSI")]],
    ["Idle, fan", [F(92, u8, "Idle target", "rpm", 10), F(38, u8, "Idle valve", "%"), F(123, u8, "Fan duty", "%", 0.5, 1)]],
    ["Controller", [F(0, u8, "Seconds counter", "s"), F(26, u16, "Main loops", "/s"), F(28, u16, "Free RAM", "B"), F(3, u8, "Sync losses"),
      F(75, u8, "Errors")]],
  ];
  const BITS = {
    engine: [2, ["running", "cranking", "afterStartEnrich", "warmupEnrich", "tpsAccel", "tpsDecel", "mapAccel", "mapDecel"]],
    status1: [1, ["inj1", "inj2", "inj3", "inj4", "dfco", "boostCutFuel", "toothLog1Ready", "toothLog2Ready"]],
    status2: [32, ["hardLaunch", "softLaunch", "hardRevLimit", "softRevLimit", "boostCutSpark", "sparkError", "idleControl", "sync"]],
    status3: [84, ["resetPrevent", "nitrous", "fuel2Active", "vssRefresh", "halfSync", "nSquirts0", "nSquirts1", "nSquirts2"]],
    status4: [110, ["wmiEmpty", "vvt1Error", "vvt2Error", "fan", "burnPending", "stagingActive", "commsCompat", "allowLegacyComms"]],
    status5: [127, ["flatShiftHard", "flatShiftSoft", "spark2Active", "knockActive", "knockPulse", "tsOnSecondary"]],
  };
  const bit = (group, name) => {
    if (!S.packet) return false;
    const [o, names] = BITS[group];
    const i = names.indexOf(name);
    return i >= 0 && ((S.packet[o] >> i) & 1) === 1;
  };

  // ── rendering ─────────────────────────────────────────────────────────
  let queued = false;
  function schedule() { if (!queued) { queued = true; requestAnimationFrame(render); } }
  function render() {
    queued = false;
    renderBar(); renderReadout();
    ({ dash: renderDash, track: renderTrack, engine: renderEngine, control: renderControl, system: renderSystem })[S.mode]();
  }
  function lamp(id, state) { $(id).className = `lamp ${state}`; }

  function renderBar() {
    const d = S.live, st = S.status, ok = fresh() && d;
    setText("version", st ? `v${st.firmware}` : "");
    lamp("lamp-engine", ok ? ((d.ecu.engine & 1) ? "ok" : (d.ecu.engine & 2) ? "warn" : "") : "");
    lamp("lamp-sync", ok ? ((d.ecu.status2 & 0x80) ? "ok" : (d.ecu.engine & 1) ? "bad" : "") : "");
    lamp("lamp-client", st?.serial?.client ? "ok" : "");
    lamp("lamp-link", S.streamUp && fresh() ? "ok" : "bad");
    setText("phase", d ? d.phase : "—");
    setText("foot-left", st ? `${st.product} · ${st.platform} · protocol on ${st.serial.transport}` : "connecting…");
    if (st) setText("foot-right", `up ${Math.floor(st.uptime / 3600)}h ${String(Math.floor(st.uptime / 60) % 60).padStart(2, "0")}m · ${st.track}`);
  }

  function renderReadout() {
    document.querySelector(".readout").classList.toggle("stale", !fresh());
    const d = S.live;
    if (!d) return;
    const e = d.ecu, c = d.car, l = d.lap;
    setText("r-rpm", fmt(e.rpm, 0));
    setText("r-rpm-sub", `${fmt(e.adv, 0)}° advance · PW ${fmt(e.pw, 2)} ms`);
    setText("r-kmh", fmt(c.kmh, 0));
    setText("r-gear", c.gear ? `gear ${c.gear}` : "neutral");
    setText("r-lap", lapTime(l.time));
    setText("r-lap-sub", `lap ${l.n + 1} · last ${lapTime(l.last)} · best ${lapTime(l.best)}`);
    setText("r-tps", fmt(e.tps, 1));
    setText("r-map", `MAP ${fmt(e.map, 0)} kPa`);
    setText("r-afr", e.afr > 0 ? fmt(e.afr, 1) : "—");
    setText("r-afr-sub", e.afrTarget > 0 ? `target ${fmt(e.afrTarget, 1)} · EGO ${e.ego}%` : " ");
    setText("r-clt", fmt(e.clt, 0));
    setText("r-clt-sub", `intake ${fmt(e.iat, 0)} °C · oil ${fmt(e.oilC, 0)} °C`);
  }

  // Dash ------------------------------------------------------------------
  const LAMPS = [
    ["engine", "running", "Running", ""], ["engine", "cranking", "Cranking", "a"], ["status2", "sync", "Sync", ""],
    ["engine", "warmupEnrich", "Warm-up", "a"], ["engine", "afterStartEnrich", "ASE", "a"], ["engine", "tpsAccel", "Accel enrich", "a"],
    ["status1", "dfco", "DFCO", ""], ["status2", "idleControl", "Idle ctrl", ""], ["status2", "softRevLimit", "Soft limit", "a"],
    ["status2", "hardRevLimit", "Hard limit", "r"], ["status2", "softLaunch", "Launch", "a"], ["status4", "fan", "Fan", ""],
  ];
  const GAUGES = [
    ["rpm", "Engine speed", "rpm", 0, 7500, 0, (v) => (f) => (f > 0.9 ? "r" : f > 0.8 ? "a" : ""), true],
    ["kmh", "Vehicle speed", "km/h", 0, 220, 0],
    ["tps", "Throttle", "%", 0, 100, 1],
    ["map", "Manifold pressure", "kPa", 0, 110, 0],
    ["afr", "Air/fuel ratio", "", 10, 20, 1],
    ["clt", "Coolant", "°C", 0, 120, 0, (v) => () => (v > 105 ? "r" : v > 99 ? "a" : "")],
    ["iat", "Intake air", "°C", -10, 80, 0],
    ["oil", "Oil pressure", "PSI", 0, 80, 0, (v, d) => () => (v < 10 && d.ecu.rpm > 1500 ? "r" : "")],
    ["bat", "Battery", "V", 10, 15, 1, (v) => () => (v < 12 ? "a" : "")],
    ["adv", "Ignition advance", "°", 0, 45, 0],
    ["pw", "Injector pulse width", "ms", 0, 12, 2],
    ["latG", "Lateral g", "g", -1.5, 1.5, 2],
  ];
  const gaugeValue = (d, k) => (k === "kmh" || k === "latG" ? d.car[k] : d.ecu[k]);
  function renderDash() {
    $("dash-lamps").replaceChildren(...LAMPS.map(([g, n, label, sev]) => {
      const on = fresh() && bit(g, n);
      const e = el("span", `ind${on ? ` on ${sev}` : ""}`);
      e.append(el("i"), document.createTextNode(label));
      return e;
    }));
    const box = $("dash-gauges");
    if (!box.children.length) {
      box.replaceChildren(...GAUGES.map(([k, label, unit, , , , , hero]) => {
        const a = el("article", `panel gauge${hero ? " hero" : ""}`);
        a.dataset.key = k;
        const v = el("span", "val"); v.append(el("b", null, "—"), el("small", null, unit));
        const scale = el("div", "scale"); scale.append(el("span"), el("span"));
        a.append(el("span", "lbl", label), v, el("div", "meter"), scale, el("span", "sub", " "));
        return a;
      }));
    }
    const d = S.live;
    if (!d) return;
    GAUGES.forEach(([k, , , lo, hi, dec, zone, hero], i) => {
      const card = box.children[i], v = gaugeValue(d, k);
      card.classList.toggle("stale", !fresh());
      card.querySelector("b").textContent = k === "afr" && !(v > 0) ? "—" : fmt(v, dec);
      meterInto(card.querySelector(".meter"), isNum(v) ? (v - lo) / (hi - lo) : 0, hero ? 40 : 24, zone ? zone(v, d) : () => "");
      const [a, b] = card.querySelectorAll(".scale span");
      a.textContent = String(lo); b.textContent = String(hi);
      card.querySelector(".sub").textContent =
        k === "rpm" ? `gear ${d.car.gear || "N"} · idle target ${d.ecu.idleTarget} rpm`
          : k === "afr" ? (d.ecu.afrTarget > 0 ? `target ${fmt(d.ecu.afrTarget, 1)}` : " ")
            : k === "map" ? `VE ${d.ecu.ve}% · total fuel ${d.ecu.gammaE}%`
              : k === "tps" ? `pedal ${fmt(d.car.pedal, 0)}% · brake ${fmt(d.car.brake, 0)}%`
                : k === "latG" ? `longitudinal ${fmt(d.car.longG, 2)} g` : " ";
    });
  }

  // Track -----------------------------------------------------------------
  function canvas(id) {
    const cv = $(id), dpr = window.devicePixelRatio || 1, w = cv.clientWidth, h = cv.clientHeight;
    if (!w || !h) return null;
    if (cv.width !== Math.round(w * dpr)) { cv.width = Math.round(w * dpr); cv.height = Math.round(h * dpr); }
    const ctx = cv.getContext("2d");
    ctx.setTransform(dpr, 0, 0, dpr, 0, 0);
    ctx.clearRect(0, 0, w, h);
    return { ctx, w, h };
  }
  const speedColour = (kmh) => {
    const f = Math.max(0, Math.min(1, (kmh - 50) / 110));
    return f < 0.5 ? C.amber : f < 0.8 ? C.green : C.cyan;
  };
  function renderTrack() {
    const t = S.track, d = S.live;
    if (t && t.name) {
      setText("track-name", t.name);
      $("track-name").append(el("span", "note", ` ${fmt(t.length / 1000, 3)} km · start line ${t.originLat.toFixed(5)}, ${t.originLon.toFixed(5)}`));
      setText("track-attrib", `Track ${t.attribution}`);
    }
    const c = canvas("track-map");
    if (c && t && t.points) {
      const { ctx, w, h } = c, p = t.points;
      let minX = Infinity, maxX = -Infinity, minY = Infinity, maxY = -Infinity;
      for (let i = 0; i < p.length; i += 2) {
        minX = Math.min(minX, p[i]); maxX = Math.max(maxX, p[i]); minY = Math.min(minY, p[i + 1]); maxY = Math.max(maxY, p[i + 1]);
      }
      const pad = 18, k = Math.min((w - 2 * pad) / (maxX - minX), (h - 2 * pad) / (maxY - minY));
      const ox = (w - (maxX - minX) * k) / 2, oy = (h - (maxY - minY) * k) / 2;
      const X = (x) => ox + (x - minX) * k, Y = (y) => h - (oy + (y - minY) * k);
      ctx.lineJoin = ctx.lineCap = "round";
      ctx.strokeStyle = C.lineHi; ctx.lineWidth = 9;
      ctx.beginPath();
      for (let i = 0; i < p.length; i += 2) (i ? ctx.lineTo(X(p[i]), Y(p[i + 1])) : ctx.moveTo(X(p[i]), Y(p[i + 1])));
      ctx.closePath(); ctx.stroke();
      // start/finish
      ctx.strokeStyle = C.text; ctx.lineWidth = 3; ctx.beginPath();
      ctx.moveTo(X(p[0]) - 7, Y(p[1])); ctx.lineTo(X(p[0]) + 7, Y(p[1])); ctx.stroke();
      // previous and current lap, coloured by speed
      const trail = (pts, alpha) => {
        ctx.globalAlpha = alpha; ctx.lineWidth = 3;
        for (let i = 1; i < pts.length; i++) {
          ctx.strokeStyle = speedColour(pts[i][2]);
          ctx.beginPath(); ctx.moveTo(X(pts[i - 1][0]), Y(pts[i - 1][1])); ctx.lineTo(X(pts[i][0]), Y(pts[i][1])); ctx.stroke();
        }
        ctx.globalAlpha = 1;
      };
      trail(S.prevTrail, 0.35); trail(S.trail, 1);
      if (d && isNum(d.car.x)) {
        ctx.fillStyle = C.display; ctx.beginPath(); ctx.arc(X(d.car.x), Y(d.car.y), 6, 0, Math.PI * 2); ctx.fill();
        ctx.strokeStyle = C.text; ctx.lineWidth = 1.5; ctx.stroke();
      }
      ctx.fillStyle = C.muted; ctx.font = `10px ${C.num}`;
      ctx.fillText("N ↑", w - 30, 16);
    }
    const tb = $("lap-table").tBodies[0];
    const best = d ? d.lap.best : 0;
    tb.replaceChildren(...S.laps.map((l) => {
      const tr = el("tr");
      const delta = best > 0 && l.time > 0 ? l.time - best : null;
      tr.append(el("td", null, String(l.n)), el("td", "n v", lapTime(l.time)),
        el("td", "n", delta === null ? "—" : delta < 0.005 ? "best" : `+${delta.toFixed(2)}`), el("td", null, l.kind));
      return tr;
    }));
    if (!S.laps.length) { const tr = el("tr"); const td = el("td", "note", "no completed laps yet"); td.colSpan = 4; tr.append(td); tb.append(tr); }
    if (d) {
      kv("car-kv", [["Phase", d.phase], ["Distance", fmt(d.lap.dist, 0), " m"], ["Speed", fmt(d.car.kmh, 1), " km/h"], ["Gear", d.car.gear ? String(d.car.gear) : "N"],
        ["Lateral", fmt(d.car.latG, 2), " g"], ["Longitudinal", fmt(d.car.longG, 2), " g"], ["Pedal", fmt(d.car.pedal, 0), " %"], ["Brake", fmt(d.car.brake, 0), " %"]]);
    }
    const s = canvas("speed-trace");
    if (s && S.track) {
      const { ctx, w, h } = s, L = S.track.length || 3650, top = 200;
      const X = (m) => 30 + (m / L) * (w - 40), Y = (v) => h - 18 - (v / top) * (h - 30);
      ctx.strokeStyle = C.grid; ctx.lineWidth = 1; ctx.fillStyle = C.muted; ctx.font = `10px ${C.num}`;
      for (let v = 0; v <= top; v += 50) { ctx.beginPath(); ctx.moveTo(30, Y(v)); ctx.lineTo(w - 10, Y(v)); ctx.stroke(); ctx.fillText(String(v), 2, Y(v) + 3); }
      for (let m = 0; m <= L; m += 500) ctx.fillText(`${m}`, X(m) - 8, h - 4);
      const line = (pts, colour, width) => {
        if (!pts || pts.length < 2) return;
        ctx.strokeStyle = colour; ctx.lineWidth = width; ctx.beginPath();
        pts.forEach(([m, v], i) => (i ? ctx.lineTo(X(m), Y(v)) : ctx.moveTo(X(m), Y(v))));
        ctx.stroke();
      };
      line(S.bestTrace, C.faint, 2);
      line(S.trace, C.cyan, 1.8);
    }
  }

  // Engine ----------------------------------------------------------------
  function renderEngine() {
    const b = S.packet;
    if (!b) { $("packet-groups").replaceChildren(el("p", "note", "waiting for the first packet…")); return; }
    $("packet-groups").replaceChildren(...PACKET.map(([title, fields]) => {
      const sec = el("section");
      sec.append(el("h3", null, title));
      const dl = el("dl", "kv");
      for (const f of fields) {
        const v = f.read(b, f.o) * f.scale;
        const dd = el("dd", null, v.toFixed(f.dec));
        if (f.unit) dd.append(el("small", null, ` ${f.unit}`));
        dl.append(el("dt", null, `${f.label} · ${f.o}`), dd);
      }
      sec.append(dl);
      return sec;
    }));
    const raw = $("packet-raw");
    if (raw.children.length !== 130) raw.replaceChildren(...b.map((_, i) => { const c = el("span"); c.title = `byte ${i}`; return c; }));
    b.forEach((v, i) => { const c = raw.children[i]; const t = String(v); if (c.textContent !== t) c.textContent = t; c.className = v ? "nz" : ""; });
    $("packet-flags").replaceChildren(...Object.entries(BITS).map(([group, [o, names]]) => {
      const sec = el("section");
      sec.append(el("h3", null, `${group} · byte ${o}`));
      const ul = el("ul");
      names.forEach((n, i) => { const li = el("li", ((b[o] >> i) & 1) ? "on" : null); li.append(el("i"), document.createTextNode(n)); ul.append(li); });
      sec.append(ul);
      return sec;
    }));
  }

  // Control ---------------------------------------------------------------
  function fillSettings(s) {
    const f = $("settings");
    for (const [k, v] of Object.entries(s)) {
      const input = f.elements[k];
      if (!input) continue;
      if (input.type === "checkbox") input.checked = !!v; else input.value = v;
    }
    setText("pace-out", Number(s.pace).toFixed(2));
  }
  function renderControl() {
    const d = S.live;
    const running = d && (d.ecu.engine & 1), onTrack = d && ["launch", "out-lap", "hot lap", "cool-down"].includes(d.phase);
    const can = { start: d && d.phase === "off", stop: d && d.phase !== "off", session: running && !onTrack, pit: d && ["out-lap", "hot lap"].includes(d.phase), blip: running && !onTrack };
    document.querySelectorAll("[data-action]").forEach((b) => { b.disabled = !can[b.dataset.action]; b.querySelector(".led").className = `led${can[b.dataset.action] ? " on" : ""}`; });
  }
  document.querySelectorAll("[data-action]").forEach((b) => b.addEventListener("click", async () => {
    const r = await post("api/control", { action: b.dataset.action });
    setText("control-msg", r.ok ? `${b.textContent.trim()}: sent` : r.error || "failed");
  }));
  $("settings").elements.pace.addEventListener("input", (e) => setText("pace-out", Number(e.target.value).toFixed(2)));
  $("settings").addEventListener("submit", async (e) => {
    e.preventDefault();
    const f = e.target, data = {};
    for (const input of f.elements) if (input.name) data[input.name] = input.type === "checkbox" ? (input.checked ? "1" : "0") : input.value;
    const r = await post("api/settings", data);
    setText("control-msg", r.ok ? "settings applied" : "settings rejected");
  });

  // System ----------------------------------------------------------------
  function renderSystem() {
    const st = S.status;
    if (!st) return;
    const p = st.serial;
    kv("sys-protocol", [["Transport", p.transport], ["Client", p.client ? "connected" : "none"], ["Commands", String(p.commands)],
      ["Realtime reads", String(p.realtime)], ["Framed", String(p.framed)], ["CRC errors", String(p.crcErrors)], ["Unknown", String(p.unknown)],
      ["Timeouts", String(p.timeouts)], ["Last command", p.last ? `'${p.last}'` : "—", p.lastAgoMs >= 0 ? ` ${(p.lastAgoMs / 1000).toFixed(1)} s ago` : ""],
      ["Identity", `${st.signature} / ${st.product}`]]);
    kv("sys-device", [["Firmware", st.firmware], ["Platform", st.platform], ["Free heap", String(st.freeHeap), " B"],
      ["Uptime", `${Math.floor(st.uptime / 60)} min`], ["Dashboard streams", String(st.streamClients)], ["Stream rate", String(S.rate), " /s"],
      ["Track", `${st.track}, ${fmt(st.trackLength / 1000, 3)} km`]]);
    kv("sys-wifi", [["Mode", st.wifi.mode], ["Network", st.wifi.ssid || "—"], ["Address", st.wifi.ip], ["Signal", st.wifi.rssi ? `${st.wifi.rssi} dBm` : "—"]]);
  }
  async function scan() {
    setText("wifi-msg", "scanning…");
    for (let i = 0; i < 20; i++) {
      const r = await (await fetch("api/wifi/scan", { cache: "no-store" })).json();
      if (!r.scanning) {
        $("wifi-list").replaceChildren(...r.networks.map((n) => {
          const li = el("li"), a = el("button", "linkish", n.ssid || "(hidden)");
          a.type = "button";
          a.addEventListener("click", () => { $("wifi-form").elements.ssid.value = n.ssid; $("wifi-form").elements.password.focus(); });
          li.append(a, el("span", "note", ` ${n.rssi} dBm${n.secure ? " · secured" : " · open"}`));
          return li;
        }));
        setText("wifi-msg", `${r.networks.length} networks`);
        return;
      }
      await new Promise((res) => setTimeout(res, 700));
    }
    setText("wifi-msg", "scan timed out");
  }
  $("wifi-scan").addEventListener("click", scan);
  $("wifi-form").addEventListener("submit", async (e) => {
    e.preventDefault();
    const f = e.target;
    const r = await post("api/wifi", { ssid: f.elements.ssid.value, password: f.elements.password.value });
    setText("wifi-msg", r.ok ? "saved; restarting into station mode (falls back to the access point if it cannot join)" : r.error || "failed");
  });
  $("wifi-reset").addEventListener("click", async () => {
    const r = await post("api/wifi/reset", {});
    setText("wifi-msg", r.ok ? "forgotten; restarting as access point SpeeduinoSim at 192.168.4.1" : "failed");
  });

  // ── modes, theme ──────────────────────────────────────────────────────
  function setMode(m) {
    S.mode = m; store.set("mode", m);
    if (location.hash !== `#${m}`) history.replaceState(null, "", `#${m}`);
    for (const id of MODES) $(`mode-${id}`).hidden = id !== m;
    document.querySelectorAll(".key[data-mode]").forEach((k) => k.classList.toggle("active", k.dataset.mode === m));
    schedule();
  }
  document.querySelectorAll(".key[data-mode]").forEach((k) => k.addEventListener("click", () => setMode(k.dataset.mode)));
  window.addEventListener("hashchange", () => { const m = location.hash.slice(1); if (MODES.includes(m)) setMode(m); });
  document.addEventListener("keydown", (e) => {
    if (e.target.closest("input, select, textarea")) return;
    const i = "12345".indexOf(e.key);
    if (i >= 0) setMode(MODES[i]);
  });
  function applyTheme() {
    document.documentElement.dataset.theme = S.theme;
    document.querySelector('meta[name="theme-color"]').content = S.theme === "light" ? "#e9ecee" : "#08090a";
    setText("k-theme-t", S.theme === "light" ? "Light" : "Dark");
    themeColours(); schedule();
  }
  $("k-theme").addEventListener("click", () => { S.theme = S.theme === "light" ? "dark" : "light"; store.set("theme", S.theme); applyTheme(); });

  new ResizeObserver(schedule).observe(document.body);
  document.addEventListener("visibilitychange", () => { if (!document.hidden) { pollStatus(); schedule(); } });
  applyTheme(); setMode(S.mode);
  loadTrack(); pollStatus(); stream();
  setInterval(pollStatus, 2000);
  setInterval(schedule, 1000);
})();
