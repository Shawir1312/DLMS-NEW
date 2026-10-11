#pragma once
#include <Arduino.h>

// ============================================================================
// EMBEDDED WEB UI HTML/CSS/JS (100% OFFLINE / ZERO EXTERNAL DEPENDENCIES)
// PRD Bagian 7: Kontrol Jarak Jauh WiFi & Web UI Responsif (Mobile & Desktop)
// Lengkap 100% fitur Display LCD: Mode 1/2, Routing, Crossover, PEQ, Limiter, Delay, Presets, Setup
// ============================================================================

const char INDEX_HTML[] PROGMEM = R"rawhtml(
<!DOCTYPE html>
<html lang="id">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width, initial-scale=1.0, maximum-scale=1.0, user-scalable=no, viewport-fit=cover">
<title>DLMS S3 Audio DSP</title>
<style>
:root {
  --bg-dark: #070b14;
  --bg-card: #0e1726;
  --bg-card-sub: #09101d;
  --border-card: #17263c;
  --border-light: #223755;
  --cyan-accent: #00d2ff;
  --cyan-glow: rgba(0, 210, 255, 0.25);
  --cyan-dim: #0284c7;
  --text-main: #f1f5f9;
  --text-muted: #8494ab;
  --green: #10b981;
  --green-glow: rgba(16, 185, 129, 0.25);
  --yellow: #f59e0b;
  --red: #ef4444;
  --red-dim: #7f1d1d;
  --radius-sm: 6px;
  --radius-md: 10px;
}
* { box-sizing: border-box; margin: 0; padding: 0; font-family: -apple-system, BlinkMacSystemFont, "Segoe UI", Roboto, Helvetica, Arial, sans-serif; -webkit-tap-highlight-color: transparent; }
body { background: var(--bg-dark); color: var(--text-main); font-size: 13px; line-height: 1.4; padding-bottom: 50px; }

/* HEADER */
header {
  background: #091222;
  border-bottom: 1px solid var(--border-card);
  padding: 10px 14px;
  display: flex;
  justify-content: space-between;
  align-items: center;
  position: sticky;
  top: 0;
  z-index: 100;
  box-shadow: 0 4px 15px rgba(0,0,0,0.5);
}
.header-left { display: flex; align-items: center; gap: 8px; flex-wrap: wrap; }
.header-title { font-size: 14px; font-weight: 800; color: var(--cyan-accent); letter-spacing: 0.5px; text-transform: uppercase; }
.badge { background: #132238; color: var(--text-muted); padding: 3px 8px; border-radius: 4px; font-size: 11px; font-weight: 600; display: inline-flex; align-items: center; gap: 4px; }
.badge.live { color: var(--green); border: 1px solid var(--green); box-shadow: 0 0 6px var(--green-glow); }
.badge.mode { color: var(--cyan-accent); border: 1px solid var(--cyan-dim); cursor: pointer; }
.btn-mute-all {
  background: #2b0d12;
  color: var(--red);
  border: 1px solid var(--red);
  padding: 7px 14px;
  border-radius: var(--radius-sm);
  font-weight: 800;
  font-size: 12px;
  cursor: pointer;
  transition: all 0.2s;
  letter-spacing: 0.5px;
}
.btn-mute-all.active { background: var(--red); color: #fff; box-shadow: 0 0 12px rgba(239,68,68,0.6); }

/* CONTAINER */
.container { max-width: 1100px; margin: 10px auto; padding: 0 10px; display: flex; flex-direction: column; gap: 12px; }

/* CARDS */
.card { background: var(--bg-card); border: 1px solid var(--border-card); border-radius: var(--radius-md); padding: 14px; }
.card-header { display: flex; justify-content: space-between; align-items: center; margin-bottom: 12px; flex-wrap: wrap; gap: 8px; }
.card-title { font-size: 13px; font-weight: 800; color: var(--cyan-accent); text-transform: uppercase; letter-spacing: 0.5px; }

/* VU METERS SECTION */
.meters-grid { display: grid; grid-template-columns: repeat(6, 1fr); gap: 6px; }
@media (max-width: 680px) { .meters-grid { grid-template-columns: repeat(3, 1fr); gap: 6px; } }
@media (max-width: 380px) { .meters-grid { grid-template-columns: repeat(2, 1fr); gap: 6px; } }

.meter-col { background: var(--bg-card-sub); border: 1px solid var(--border-card); border-radius: var(--radius-sm); padding: 8px 4px; text-align: center; }
.meter-label { font-size: 10px; font-weight: 700; color: var(--text-muted); margin-bottom: 4px; white-space: nowrap; overflow: hidden; text-overflow: ellipsis; }
.meter-label.in { color: #38bdf8; }
.meter-label.out { color: var(--cyan-accent); }
.meter-bar-box { background: #040810; height: 95px; border-radius: 3px; position: relative; overflow: hidden; margin: 0 auto 6px; width: 22px; border: 1px solid #101c30; }
.meter-bar-fill { position: absolute; bottom: 0; left: 0; right: 0; background: linear-gradient(to top, #059669 0%, #10b981 70%, #f59e0b 88%, #ef4444 100%); width: 100%; height: 0%; transition: height 0.05s ease-out; }
.meter-val { font-size: 11px; font-weight: 700; font-family: monospace; color: var(--text-main); }
.meter-badges { display: flex; justify-content: center; gap: 3px; margin-top: 4px; min-height: 14px; }
.badge-mini { font-size: 8px; padding: 1px 3px; border-radius: 2px; font-weight: 800; }
.badge-clip { background: var(--red); color: #fff; }
.badge-lim { background: var(--yellow); color: #000; }

/* NAV TABS */
.nav-tabs { display: flex; gap: 6px; overflow-x: auto; -webkit-overflow-scrolling: touch; padding: 2px 2px 8px; scrollbar-width: none; }
.nav-tabs::-webkit-scrollbar { display: none; }
.tab-btn {
  background: var(--bg-card);
  border: 1px solid var(--border-card);
  color: var(--text-muted);
  padding: 8px 14px;
  border-radius: var(--radius-sm);
  cursor: pointer;
  white-space: nowrap;
  font-size: 12px;
  font-weight: 600;
  transition: all 0.15s;
  flex-shrink: 0;
}
.tab-btn:hover { color: #fff; border-color: var(--cyan-dim); }
.tab-btn.active { background: var(--cyan-dim); color: #fff; border-color: var(--cyan-accent); box-shadow: 0 0 10px var(--cyan-glow); }

/* GRID LAYOUTS */
.grid-2col { display: grid; grid-template-columns: 1fr 1fr; gap: 10px; }
.grid-4col { display: grid; grid-template-columns: repeat(4, 1fr); gap: 10px; }
@media (max-width: 820px) { .grid-4col { grid-template-columns: 1fr 1fr; } }
@media (max-width: 768px) { .grid-2col { grid-template-columns: 1fr; } }
@media (max-width: 480px) { .grid-4col { grid-template-columns: 1fr; } }

/* CHANNEL CARD & SUB BOX */
.box-sub { background: var(--bg-card-sub); border: 1px solid var(--border-card); border-radius: var(--radius-sm); padding: 10px; margin-bottom: 8px; }
.box-title { font-size: 12px; font-weight: 700; color: var(--cyan-accent); margin-bottom: 8px; display: flex; justify-content: space-between; align-items: center; }

/* CONTROL ROWS */
.control-row { display: flex; justify-content: space-between; align-items: center; padding: 6px 0; border-bottom: 1px solid rgba(255,255,255,0.04); gap: 8px; }
.control-row:last-child { border-bottom: none; }
.control-row label { color: var(--text-muted); font-size: 12px; flex-shrink: 0; min-width: 80px; }
.control-val { font-family: monospace; font-weight: 700; color: var(--cyan-accent); min-width: 58px; text-align: right; font-size: 12px; flex-shrink: 0; }

/* INPUTS, SLIDERS & BUTTONS */
input[type=range] {
  flex: 1;
  margin: 0;
  accent-color: var(--cyan-accent);
  cursor: pointer;
  min-height: 32px;
  height: 32px;
  touch-action: pan-y;
  background: transparent;
  -webkit-appearance: none;
}
input[type=range]::-webkit-slider-runnable-track {
  height: 6px;
  border-radius: 3px;
  background: #0d1829;
  border: 1px solid #1a2d4b;
}
input[type=range]::-webkit-slider-thumb {
  -webkit-appearance: none;
  width: 20px;
  height: 20px;
  border-radius: 50%;
  background: var(--cyan-accent);
  margin-top: -7px;
  cursor: pointer;
  box-shadow: 0 0 10px var(--cyan-glow);
  border: 2px solid #ffffff;
  transition: transform 0.1s ease;
}
input[type=range]:active::-webkit-slider-thumb {
  transform: scale(1.18);
  background: #ffffff;
}
select, input[type=text], input[type=number], input[type=password] {
  background: #050a14;
  border: 1px solid var(--border-light);
  color: #fff;
  padding: 6px 10px;
  border-radius: var(--radius-sm);
  font-size: 12px;
  outline: none;
  transition: border-color 0.2s;
}
select:focus, input:focus { border-color: var(--cyan-accent); }
select { cursor: pointer; }

.btn-act {
  background: #11233d;
  border: 1px solid var(--border-light);
  color: var(--text-main);
  padding: 6px 12px;
  border-radius: var(--radius-sm);
  cursor: pointer;
  font-size: 12px;
  font-weight: 600;
  transition: all 0.15s;
  display: inline-flex;
  align-items: center;
  justify-content: center;
  gap: 4px;
}
.btn-act:hover { background: var(--cyan-dim); color: #fff; border-color: var(--cyan-accent); }
.btn-act:active { transform: scale(0.97); }
.btn-act.cyan { background: var(--cyan-dim); border-color: var(--cyan-accent); color: #fff; }
.btn-act.danger { background: #3b1117; border-color: var(--red); color: #fca5a5; }
.btn-act.danger:hover { background: var(--red); color: #fff; }
.btn-toggle {
  background: #0f1c30;
  border: 1px solid var(--border-light);
  color: var(--text-muted);
  padding: 4px 10px;
  border-radius: 4px;
  font-size: 11px;
  font-weight: 700;
  cursor: pointer;
}
.btn-toggle.active { background: var(--green); color: #000; border-color: var(--green); }
.btn-toggle.muted { background: var(--red); color: #fff; border-color: var(--red); }
.btn-toggle.invert { background: var(--yellow); color: #000; border-color: var(--yellow); }

/* SVG CURVE */
.graph-container { background: #030712; border: 1px solid var(--border-card); border-radius: var(--radius-sm); height: 190px; position: relative; margin-bottom: 12px; overflow: hidden; }
svg.curve-svg { width: 100%; height: 100%; display: block; }

/* TOAST NOTIFICATION */
#toast {
  position: fixed;
  bottom: 16px;
  right: 16px;
  background: rgba(14, 23, 38, 0.95);
  border: 1px solid var(--cyan-accent);
  color: #fff;
  padding: 10px 16px;
  border-radius: var(--radius-sm);
  font-weight: 600;
  font-size: 12px;
  box-shadow: 0 4px 20px rgba(0,0,0,0.7);
  display: none;
  z-index: 1000;
  backdrop-filter: blur(8px);
}
</style>
</head>
<body>

<header>
  <div class="header-left">
    <span class="header-title">DLMS S3 AUDIO DSP</span>
    <span class="badge live" id="ws-badge">&#9679; LIVE</span>
    <span class="badge" id="cpu-badge">CPU: 0%</span>
    <span class="badge mode" id="mode-badge" onclick="switchModeQuick()">Mode 1: Stereo Pair</span>
    <span class="badge" id="preset-badge">Slot 1</span>
    <span class="badge" style="color:var(--cyan-accent); border:1px solid #1a365d;" title="WiFi SoftAP Password">Pass: 0987654321</span>
  </div>
  <button class="btn-mute-all" id="btn-master-mute" onclick="toggleMasterMute()">MUTE ALL</button>
</header>

<div class="container">
  <!-- 1. LIVE VU METERS CARD (RESPONSIVE) -->
  <div class="card">
    <div class="card-header">
      <span class="card-title">Level Meter Telemetry (-60 s/d 0 dBFS)</span>
      <span style="color:var(--text-muted); font-size:11px;" id="audio-spec-text">48 kHz / 32-bit I2S</span>
    </div>
    <div class="meters-grid">
      <div class="meter-col">
        <div class="meter-label in">IN A</div>
        <div class="meter-bar-box"><div class="meter-bar-fill" id="vu-in0"></div></div>
        <div class="meter-val" id="val-in0">-60.0</div>
        <div class="meter-badges"><span class="badge-mini badge-clip" id="clip-in0" style="display:none;">CLIP</span></div>
      </div>
      <div class="meter-col">
        <div class="meter-label in">IN B</div>
        <div class="meter-bar-box"><div class="meter-bar-fill" id="vu-in1"></div></div>
        <div class="meter-val" id="val-in1">-60.0</div>
        <div class="meter-badges"><span class="badge-mini badge-clip" id="clip-in1" style="display:none;">CLIP</span></div>
      </div>
      <div class="meter-col">
        <div class="meter-label out" id="lbl-out0">OUT 1</div>
        <div class="meter-bar-box"><div class="meter-bar-fill" id="vu-out0"></div></div>
        <div class="meter-val" id="val-out0">-60.0</div>
        <div class="meter-badges">
          <span class="badge-mini badge-clip" id="clip-out0" style="display:none;">CLIP</span>
          <span class="badge-mini badge-lim" id="lim-out0" style="display:none;">LIM</span>
        </div>
      </div>
      <div class="meter-col">
        <div class="meter-label out" id="lbl-out1">OUT 2</div>
        <div class="meter-bar-box"><div class="meter-bar-fill" id="vu-out1"></div></div>
        <div class="meter-val" id="val-out1">-60.0</div>
        <div class="meter-badges">
          <span class="badge-mini badge-clip" id="clip-out1" style="display:none;">CLIP</span>
          <span class="badge-mini badge-lim" id="lim-out1" style="display:none;">LIM</span>
        </div>
      </div>
      <div class="meter-col">
        <div class="meter-label out" id="lbl-out2">OUT 3</div>
        <div class="meter-bar-box"><div class="meter-bar-fill" id="vu-out2"></div></div>
        <div class="meter-val" id="val-out2">-60.0</div>
        <div class="meter-badges">
          <span class="badge-mini badge-clip" id="clip-out2" style="display:none;">CLIP</span>
          <span class="badge-mini badge-lim" id="lim-out2" style="display:none;">LIM</span>
        </div>
      </div>
      <div class="meter-col">
        <div class="meter-label out" id="lbl-out3">OUT 4</div>
        <div class="meter-bar-box"><div class="meter-bar-fill" id="vu-out3"></div></div>
        <div class="meter-val" id="val-out3">-60.0</div>
        <div class="meter-badges">
          <span class="badge-mini badge-clip" id="clip-out3" style="display:none;">CLIP</span>
          <span class="badge-mini badge-lim" id="lim-out3" style="display:none;">LIM</span>
        </div>
      </div>
    </div>
  </div>

  <!-- NAVIGATION TABS -->
  <div class="nav-tabs">
    <button class="tab-btn active" onclick="switchTab('tab-channels')">Channels & Routing</button>
    <button class="tab-btn" onclick="switchTab('tab-xover')">Crossover (HPF/LPF)</button>
    <button class="tab-btn" onclick="switchTab('tab-peq')">PEQ Equalizer</button>
    <button class="tab-btn" onclick="switchTab('tab-dynamics')">Limiter & Delay</button>
    <button class="tab-btn" onclick="switchTab('tab-presets')">Preset Manager</button>
    <button class="tab-btn" onclick="switchTab('tab-setup')">Setup & WiFi</button>
  </div>

  <!-- TAB 1: CHANNELS & ROUTING (GAINS, MUTES, POLARITY, ROUTING) -->
  <div class="card" id="tab-channels">
    <div class="card-header">
      <span class="card-title">Input & Output Channel Strips</span>
      <div style="font-size:11px; color:var(--text-muted);" id="chan-mode-info">Mode 1: Out 1-2 & Out 3-4 Terhubung</div>
    </div>
    
    <!-- Input Section -->
    <div style="font-size:11px; font-weight:700; color:#38bdf8; text-transform:uppercase; margin-bottom:6px;">Inputs (Master Stereo / ADC)</div>
    <div class="grid-2col" style="margin-bottom:14px;">
      <div class="box-sub">
        <div class="box-title">
          <span>INPUT A</span>
          <button class="btn-toggle" id="btn-in-mute0" onclick="toggleChannelMute('in', 0)">MUTE</button>
        </div>
        <div class="control-row">
          <label>Gain (dB)</label>
          <input type="range" min="-60" max="12" step="0.5" id="rng-in-gain0" oninput="updateInGain(0, this.value)">
          <span class="control-val" id="val-in-gain0">0.0 dB</span>
        </div>
      </div>
      <div class="box-sub">
        <div class="box-title">
          <span>INPUT B</span>
          <button class="btn-toggle" id="btn-in-mute1" onclick="toggleChannelMute('in', 1)">MUTE</button>
        </div>
        <div class="control-row">
          <label>Gain (dB)</label>
          <input type="range" min="-60" max="12" step="0.5" id="rng-in-gain1" oninput="updateInGain(1, this.value)">
          <span class="control-val" id="val-in-gain1">0.0 dB</span>
        </div>
      </div>
    </div>

    <!-- Output Section -->
    <div style="font-size:11px; font-weight:700; color:var(--cyan-accent); text-transform:uppercase; margin-bottom:6px;">Outputs (Dual PCM5102 DAC)</div>
    <div class="grid-2col" id="outputs-channel-container"></div>
  </div>

  <!-- TAB 2: CROSSOVER (HPF & LPF LENGKAP) -->
  <div class="card" id="tab-xover" style="display:none;">
    <div class="card-header">
      <span class="card-title">Crossover Filters (HPF / LPF Cascade)</span>
      <div style="display:flex; gap:6px; flex-wrap:wrap;">
        <button class="btn-act" onclick="applyQuickXover(2)">Quick 2-Way</button>
        <button class="btn-act" onclick="applyQuickXover(3)">Quick 3-Way</button>
        <button class="btn-act" onclick="applyQuickXover(4)">Quick 4-Way</button>
      </div>
    </div>
    <div class="grid-2col" id="xover-channels-container"></div>
  </div>

  <!-- TAB 3: PEQ EQUALIZER -->
  <div class="card" id="tab-peq" style="display:none;">
    <div class="card-header">
      <span class="card-title">Parametric Equalizer</span>
      <div style="display:flex; align-items:center; gap:8px;">
        <label style="color:var(--text-muted); font-size:12px;">Pilih Kanal:</label>
        <select id="peq-target-sel" onchange="changePeqTarget(this.value)">
          <option value="0">Input A (4 Band)</option>
          <option value="1">Input B (4 Band)</option>
          <option value="2" selected>OUT 1 (8 Band)</option>
          <option value="3">OUT 2 (8 Band)</option>
          <option value="4">OUT 3 (8 Band)</option>
          <option value="5">OUT 4 (8 Band)</option>
        </select>
      </div>
    </div>
    <div class="graph-container">
      <svg class="curve-svg" id="peq-svg" viewBox="0 0 800 190"></svg>
    </div>
    <div id="peq-bands-container"></div>
  </div>

  <!-- TAB 4: LIMITER & DELAY -->
  <div class="card" id="tab-dynamics" style="display:none;">
    <div class="card-header"><span class="card-title">Output Limiter & Time Alignment Delay</span></div>
    <div class="grid-2col" id="dynamics-container"></div>
  </div>

  <!-- TAB 5: PRESET MANAGER -->
  <div class="card" id="tab-presets" style="display:none;">
    <div class="card-header">
      <span class="card-title">Flash LittleFS Presets</span>
      <button class="btn-act danger" onclick="confirmFactoryReset()">Factory Reset</button>
    </div>
    <div id="preset-list" style="display:flex; flex-direction:column; gap:8px;"></div>
  </div>

  <!-- TAB 6: SETUP & WIFI -->
  <div class="card" id="tab-setup" style="display:none;">
    <div class="card-header"><span class="card-title">Pengaturan Sistem & Jaringan WiFi</span></div>
    
    <div class="grid-2col">
      <div class="box-sub">
        <div class="box-title">Mode Kanal DSP</div>
        <div class="control-row">
          <label>Arsitektur Mode</label>
          <select id="sel-out-mode" onchange="updateOutMode(this.value)">
            <option value="0">Mode 1: Stereo Pair (Out 1-2 & Out 3-4 Terhubung)</option>
            <option value="1">Mode 2: 4-Way Individual (Tiap Out Mandiri)</option>
          </select>
        </div>
        <p style="color:var(--text-muted); font-size:11px; margin-top:6px;">
          * Mode 1: Cocok untuk Speaker Stereo 2-Way / Sub-Top stereo (parameter Out 1 & 2 disinkronkan, Out 3 & 4 disinkronkan).<br>
          * Mode 2: Cocok untuk 3-Way + Sub atau 4-Way individual di mana tiap speaker memiliki crossover dan gain terpisah.
        </p>
      </div>

      <div class="box-sub">
        <div class="box-title">Koneksi WiFi & Remote Control</div>
        <div style="background:#040914; border:1px solid #1a2c47; border-radius:6px; padding:10px; margin-bottom:10px;">
          <div style="color:var(--cyan-accent); font-weight:700; margin-bottom:4px; font-size:12px;">&#128246; Kredensial SoftAP Bawaan:</div>
          <div style="font-size:12px; color:var(--text-main); font-family:monospace; line-height:1.6;">
            SSID: <b style="color:#fff;">DLMS-DSP-SHAWIR</b><br>
            Password: <b style="color:var(--cyan-accent); letter-spacing:1px;">0987654321</b><br>
            IP Akses: <b style="color:var(--green);">http://192.168.4.1</b>
          </div>
        </div>
        <div class="control-row">
          <label>Mode WiFi</label>
          <select id="sel-wifi-mode">
            <option value="1">SoftAP (Access Point Mandiri)</option>
            <option value="2">Station (Terhubung ke WiFi Router)</option>
            <option value="0">Off (Radio WiFi Mati)</option>
          </select>
        </div>
        <div class="control-row">
          <label>SSID Station</label>
          <input type="text" id="inp-wifi-ssid" placeholder="Nama WiFi Rumah / Venue" style="flex:1;">
        </div>
        <div class="control-row">
          <label>Password</label>
          <input type="password" id="inp-wifi-pass" value="0987654321" placeholder="Password WiFi" style="flex:1;">
        </div>
        <div class="control-row" style="margin-top:8px;">
          <span></span>
          <button class="btn-act cyan" onclick="saveWifiSetup()">Simpan & Terapkan WiFi</button>
        </div>
      </div>
    </div>
  </div>
</div>

<div id="toast">Pengaturan Tersimpan</div>

<script>
let currentConfig = null;
let currentTarget = 2; // Default OUT 1

function showToast(msg) {
  const t = document.getElementById('toast');
  if (!t) return;
  t.innerText = msg;
  t.style.display = 'block';
  setTimeout(() => { t.style.display = 'none'; }, 2200);
}

let lastConfigVer = -1;
let isUserEditing = false;
let configSyncTimer = null;
let meterPollTimer = null;

// Throttling manager for smooth 60 FPS slider dragging without network congestion
const throttlers = {};
function postThrottled(key, endpoint, payloadFn, delayMs = 60) {
  if (!throttlers[key]) {
    throttlers[key] = {
      timer: null,
      inFlight: false,
      pending: false,
      payloadFn: payloadFn
    };
  }
  const th = throttlers[key];
  th.payloadFn = payloadFn;

  function sendReq() {
    if (th.inFlight) {
      th.pending = true;
      return;
    }
    th.inFlight = true;
    th.pending = false;
    const data = th.payloadFn();

    fetch(endpoint, {
      method: 'POST',
      headers: { 'Content-Type': 'application/json' },
      body: JSON.stringify(data)
    })
    .then(r => r.json())
    .catch(e => console.warn(e))
    .finally(() => {
      th.inFlight = false;
      if (th.pending) {
        th.pending = false;
        th.timer = setTimeout(sendReq, delayMs);
      }
    });
  }

  if (!th.inFlight && !th.timer) {
    sendReq();
  } else {
    th.pending = true;
    if (!th.timer) {
      th.timer = setTimeout(() => {
        th.timer = null;
        sendReq();
      }, delayMs);
    }
  }
}

function scheduleConfigSync() {
  if (configSyncTimer) return;
  configSyncTimer = setTimeout(() => {
    configSyncTimer = null;
    fetchConfig();
  }, 150);
}

function initLiveTelemetry() {
  fetchConfig();
  pollMetersLoop();

  // Deteksi interaksi pengguna agar slider yang sedang digeser tidak terinterupsi
  document.addEventListener('focusin', (e) => {
    if (e.target && (e.target.tagName === 'INPUT' || e.target.tagName === 'SELECT')) {
      isUserEditing = true;
    }
  });
  document.addEventListener('focusout', () => {
    isUserEditing = false;
    scheduleConfigSync();
  });
  document.addEventListener('pointerdown', (e) => {
    if (e.target && e.target.tagName === 'INPUT' && e.target.type === 'range') {
      isUserEditing = true;
    }
  });
  document.addEventListener('pointerup', () => {
    setTimeout(() => {
      isUserEditing = false;
      scheduleConfigSync();
    }, 200);
  });
}

// Chained polling prevents stacked requests and yields WiFi bandwidth while user is dragging
function pollMetersLoop() {
  const pollInterval = isUserEditing ? 250 : 120;
  fetch('/api/meter')
    .then(r => r.json())
    .then(m => updateMeters(m))
    .catch(() => {
      const b = document.getElementById('ws-badge');
      if (b) {
        b.innerText = '○ OFFLINE';
        b.className = 'badge';
      }
    })
    .finally(() => {
      if (meterPollTimer) clearTimeout(meterPollTimer);
      meterPollTimer = setTimeout(pollMetersLoop, pollInterval);
    });
}

function updateMeters(m) {
  const wsBadge = document.getElementById('ws-badge');
  if (wsBadge) {
    wsBadge.innerText = '● LIVE';
    wsBadge.className = 'badge live';
  }
  if (m.cpu !== undefined) {
    const cpuBadge = document.getElementById('cpu-badge');
    if (cpuBadge) cpuBadge.innerText = `CPU: ${Math.round(m.cpu)}%`;
  }

  // Sinkronisasi otomatis real-time jika ada perubahan parameter di LCD / Hardware
  if (m.ver !== undefined) {
    if (lastConfigVer === -1) {
      lastConfigVer = m.ver;
    } else if (m.ver !== lastConfigVer) {
      lastConfigVer = m.ver;
      if (!isUserEditing) {
        scheduleConfigSync();
      }
    }
  }

  if (m.in_peak) {
    for (let i = 0; i < 2; i++) {
      const db = m.in_peak[i];
      const pct = Math.max(0, Math.min(100, (db + 60) / 60 * 100));
      const vu = document.getElementById(`vu-in${i}`);
      if (vu) vu.style.height = pct + '%';
      const val = document.getElementById(`val-in${i}`);
      if (val) val.innerText = db > -58 ? db.toFixed(1) : '-inf';
      const clip = document.getElementById(`clip-in${i}`);
      if (clip) clip.style.display = db >= -0.1 ? 'inline-block' : 'none';
    }
  }

  if (m.out_peak) {
    for (let i = 0; i < 4; i++) {
      const db = m.out_peak[i];
      const pct = Math.max(0, Math.min(100, (db + 60) / 60 * 100));
      const vu = document.getElementById(`vu-out${i}`);
      if (vu) vu.style.height = pct + '%';
      const val = document.getElementById(`val-out${i}`);
      if (val) val.innerText = db > -58 ? db.toFixed(1) : '-inf';
      const clip = document.getElementById(`clip-out${i}`);
      if (clip) clip.style.display = db >= -0.1 ? 'inline-block' : 'none';
      const gr = m.limiter_gr ? m.limiter_gr[i] : 0;
      const lim = document.getElementById(`lim-out${i}`);
      if (lim) lim.style.display = gr < -0.3 ? 'inline-block' : 'none';
    }
  }
}

function fetchConfig() {
  fetch('/api/config')
    .then(r => r.json())
    .then(data => {
      currentConfig = data;
      renderAll();
    })
    .catch(e => console.error(e));
}

function sendPost(endpoint, payload, callback) {
  fetch(endpoint, {
    method: 'POST',
    headers: { 'Content-Type': 'application/json' },
    body: JSON.stringify(payload)
  }).then(r => r.json()).then(res => {
    if (callback) callback(res);
  }).catch(e => console.warn(e));
}

function switchTab(tabId) {
  const tabs = ['tab-channels', 'tab-xover', 'tab-peq', 'tab-dynamics', 'tab-presets', 'tab-setup'];
  tabs.forEach(t => {
    const el = document.getElementById(t);
    if (el) el.style.display = (t === tabId ? 'block' : 'none');
  });
  document.querySelectorAll('.tab-btn').forEach((btn, idx) => {
    btn.className = 'tab-btn' + (tabs[idx] === tabId ? ' active' : '');
  });
  if (tabId === 'tab-presets') renderPresets();
}

function switchModeQuick() {
  if (!currentConfig) return;
  const newMode = (currentConfig.setup.out_mode === 0) ? 1 : 0;
  updateOutMode(newMode);
}

function updateOutMode(m) {
  m = parseInt(m);
  currentConfig.setup.out_mode = m;
  sendPost('/api/mode', { out_mode: m }, () => {
    showToast(m === 0 ? 'Mode 1: Stereo Pair Aktif' : 'Mode 2: 4-Way Indiv Aktif');
    fetchConfig();
  });
}

function toggleMasterMute() {
  if (!currentConfig) return;
  const newMute = !currentConfig.master_mute;
  currentConfig.master_mute = newMute;
  const btn = document.getElementById('btn-master-mute');
  if (btn) btn.className = 'btn-mute-all' + (newMute ? ' active' : '');
  sendPost('/api/master_mute', { mute: newMute }, () => {
    showToast(newMute ? 'Master Audio Dimatikan (Muted)' : 'Master Audio Aktif');
  });
}

function renderAll() {
  if (!currentConfig) return;
  const isMode1 = currentConfig.setup.out_mode === 0;
  const modeBadge = document.getElementById('mode-badge');
  if (modeBadge) modeBadge.innerText = isMode1 ? 'Mode 1: Stereo Pair' : 'Mode 2: 4-Way Indiv';
  const chanInfo = document.getElementById('chan-mode-info');
  if (chanInfo) chanInfo.innerText = isMode1 ? 'Mode 1: Out 1-2 & Out 3-4 Terhubung' : 'Mode 2: Masing-masing Out Mandiri';
  const presBadge = document.getElementById('preset-badge');
  if (presBadge) presBadge.innerText = `Slot ${currentConfig.current_slot}: ${currentConfig.name}`;
  const btnMute = document.getElementById('btn-master-mute');
  if (btnMute) btnMute.className = 'btn-mute-all' + (currentConfig.master_mute ? ' active' : '');

  // Setup tab sync
  const selOutMode = document.getElementById('sel-out-mode');
  if (selOutMode) selOutMode.value = currentConfig.setup.out_mode;
  const selWifi = document.getElementById('sel-wifi-mode');
  if (selWifi) selWifi.value = currentConfig.setup.wifi_mode;
  const inpSsid = document.getElementById('inp-wifi-ssid');
  if (inpSsid) inpSsid.value = currentConfig.setup.wifi_ssid || '';

  // Render Inputs
  for (let i = 0; i < 2; i++) {
    const ch = currentConfig.in[i];
    const rng = document.getElementById(`rng-in-gain${i}`);
    if (rng) rng.value = ch.gain_db;
    const val = document.getElementById(`val-in-gain${i}`);
    if (val) val.innerText = ch.gain_db.toFixed(1) + ' dB';
    const btnM = document.getElementById(`btn-in-mute${i}`);
    if (btnM) {
      btnM.className = 'btn-toggle' + (ch.mute ? ' muted' : '');
      btnM.innerText = ch.mute ? 'MUTED' : 'MUTE';
    }
  }

  // Render Outputs
  renderOutputChannelCards();
  renderXover();
  renderPeqControls();
  drawCurve();
  renderDynamics();
}

function updateInGain(ch, val) {
  val = parseFloat(val);
  currentConfig.in[ch].gain_db = val;
  const el = document.getElementById(`val-in-gain${ch}`);
  if (el) el.innerText = val.toFixed(1) + ' dB';
  postThrottled(`in_gain_${ch}`, '/api/channel', () => ({ type: 'in', channel: ch, gain_db: val }), 50);
}

function toggleChannelMute(type, ch) {
  const isOut = (type === 'out');
  const target = isOut ? currentConfig.out[ch] : currentConfig.in[ch];
  target.mute = !target.mute;
  sendPost('/api/channel', { type: type, channel: ch, mute: target.mute }, () => {
    fetchConfig();
  });
}

function togglePolarity(ch) {
  const target = currentConfig.out[ch];
  target.polarity_invert = !target.polarity_invert;
  sendPost('/api/channel', { type: 'out', channel: ch, polarity_invert: target.polarity_invert }, () => {
    showToast(`OUT ${ch+1} Polarity: ${target.polarity_invert ? '180° Invert' : '0° Normal'}`);
    fetchConfig();
  });
}

function renderOutputChannelCards() {
  const cont = document.getElementById('outputs-channel-container');
  if (!cont || !currentConfig) return;
  const isMode1 = currentConfig.setup.out_mode === 0;

  let html = '';
  currentConfig.out.forEach((ch, idx) => {
    const isPaired = isMode1 && (idx === 1 || idx === 3);
    const pairNote = isMode1 ? `<span style="font-size:10px; color:var(--cyan-dim);">[Pair ${idx < 2 ? '1-2' : '3-4'}]</span>` : '';
    html += `
      <div class="box-sub">
        <div class="box-title">
          <span>OUT ${idx + 1} (${ch.name}) ${pairNote}</span>
          <div style="display:flex; gap:4px;">
            <button class="btn-toggle ${ch.polarity_invert ? 'invert' : ''}" onclick="togglePolarity(${idx})">${ch.polarity_invert ? '180° INV' : '0° NORM'}</button>
            <button class="btn-toggle ${ch.mute ? 'muted' : ''}" onclick="toggleChannelMute('out', ${idx})">${ch.mute ? 'MUTED' : 'MUTE'}</button>
          </div>
        </div>
        <div class="control-row">
          <label>Routing Source</label>
          <select onchange="updateRouting(${idx}, this.value)" style="flex:1;">
            <option value="0" ${ch.source===0?'selected':''}>Input A</option>
            <option value="1" ${ch.source===1?'selected':''}>Input B</option>
            <option value="2" ${ch.source===2?'selected':''}>A+B Mono Sum</option>
            <option value="3" ${ch.source===3?'selected':''}>MUTE / OFF</option>
          </select>
        </div>
        <div class="control-row">
          <label>Gain (dB)</label>
          <input type="range" min="-60" max="12" step="0.5" value="${ch.gain_db}" oninput="updateOutGain(${idx}, this.value)">
          <span class="control-val" id="val-out-gain${idx}">${ch.gain_db.toFixed(1)} dB</span>
        </div>
      </div>
    `;
  });
  cont.innerHTML = html;
}

function updateOutGain(ch, val) {
  val = parseFloat(val);
  currentConfig.out[ch].gain_db = val;
  const el = document.getElementById(`val-out-gain${ch}`);
  if (el) el.innerText = val.toFixed(1) + ' dB';
  postThrottled(`out_gain_${ch}`, '/api/channel', () => ({ type: 'out', channel: ch, gain_db: val }), 50);
}

function updateRouting(ch, src) {
  src = parseInt(src);
  currentConfig.out[ch].source = src;
  sendPost('/api/channel', { type: 'out', channel: ch, source: src });
}

/* CROSSOVER */
function renderXover() {
  const cont = document.getElementById('xover-channels-container');
  if (!cont || !currentConfig) return;
  const slopes = [6, 12, 18, 24, 36, 48];
  const types = ['Butterworth', 'Linkwitz-Riley', 'Bessel'];

  let html = '';
  currentConfig.out.forEach((ch, idx) => {
    html += `
      <div class="box-sub">
        <div class="box-title">OUT ${idx + 1} (${ch.name})</div>
        
        <!-- HPF -->
        <div style="background:#050912; border:1px solid #142034; border-radius:4px; padding:8px; margin-bottom:8px;">
          <div style="display:flex; justify-content:space-between; align-items:center; margin-bottom:6px;">
            <span style="font-weight:700; color:#38bdf8;">High-Pass (HPF)</span>
            <button class="btn-toggle ${ch.hpf.enabled ? 'active' : ''}" onclick="toggleXoverEnable(${idx}, 'hpf')">${ch.hpf.enabled ? 'AKTIF' : 'BYPASS'}</button>
          </div>
          <div class="control-row">
            <label>Frekuensi</label>
            <input type="range" id="xover-rng-hpf-${idx}" min="20" max="20000" value="${ch.hpf.freq}" oninput="updateXoverVal(${idx}, 'hpf', 'freq', this.value)">
            <input type="number" id="xover-num-hpf-${idx}" min="20" max="20000" value="${Math.round(ch.hpf.freq)}" style="width:70px;" onchange="updateXoverVal(${idx}, 'hpf', 'freq', this.value)">
          </div>
          <div class="control-row">
            <label>Tipe Filter</label>
            <select onchange="updateXoverVal(${idx}, 'hpf', 'type', this.value)" style="flex:1;">
              ${types.map((t, ti) => `<option value="${ti}" ${ch.hpf.type===ti?'selected':''}>${t}</option>`).join('')}
            </select>
          </div>
          <div class="control-row">
            <label>Slope dB/okt</label>
            <select onchange="updateXoverVal(${idx}, 'hpf', 'slope', this.value)" style="flex:1;">
              ${slopes.map(s => `<option value="${s}" ${ch.hpf.slope===s?'selected':''}>${s} dB/okt</option>`).join('')}
            </select>
          </div>
        </div>

        <!-- LPF -->
        <div style="background:#050912; border:1px solid #142034; border-radius:4px; padding:8px;">
          <div style="display:flex; justify-content:space-between; align-items:center; margin-bottom:6px;">
            <span style="font-weight:700; color:var(--cyan-accent);">Low-Pass (LPF)</span>
            <button class="btn-toggle ${ch.lpf.enabled ? 'active' : ''}" onclick="toggleXoverEnable(${idx}, 'lpf')">${ch.lpf.enabled ? 'AKTIF' : 'BYPASS'}</button>
          </div>
          <div class="control-row">
            <label>Frekuensi</label>
            <input type="range" id="xover-rng-lpf-${idx}" min="20" max="20000" value="${ch.lpf.freq}" oninput="updateXoverVal(${idx}, 'lpf', 'freq', this.value)">
            <input type="number" id="xover-num-lpf-${idx}" min="20" max="20000" value="${Math.round(ch.lpf.freq)}" style="width:70px;" onchange="updateXoverVal(${idx}, 'lpf', 'freq', this.value)">
          </div>
          <div class="control-row">
            <label>Tipe Filter</label>
            <select onchange="updateXoverVal(${idx}, 'lpf', 'type', this.value)" style="flex:1;">
              ${types.map((t, ti) => `<option value="${ti}" ${ch.lpf.type===ti?'selected':''}>${t}</option>`).join('')}
            </select>
          </div>
          <div class="control-row">
            <label>Slope dB/okt</label>
            <select onchange="updateXoverVal(${idx}, 'lpf', 'slope', this.value)" style="flex:1;">
              ${slopes.map(s => `<option value="${s}" ${ch.lpf.slope===s?'selected':''}>${s} dB/okt</option>`).join('')}
            </select>
          </div>
        </div>
      </div>
    `;
  });
  cont.innerHTML = html;
}

function toggleXoverEnable(ch, type) {
  const target = currentConfig.out[ch][type];
  target.enabled = !target.enabled;
  sendPost('/api/xover', { channel: ch, type: type, config: target }, () => {
    renderXover();
    showToast(`OUT ${ch+1} ${type.toUpperCase()}: ${target.enabled ? 'Aktif' : 'Bypass'}`);
  });
}

function updateXoverVal(ch, type, prop, val) {
  val = (prop === 'type' || prop === 'slope') ? parseInt(val) : parseFloat(val);
  currentConfig.out[ch][type][prop] = val;
  if (prop === 'freq') {
    const inp = document.getElementById(`xover-num-${type}-${ch}`);
    const rng = document.getElementById(`xover-rng-${type}-${ch}`);
    if (inp && inp.value != Math.round(val)) inp.value = Math.round(val);
    if (rng && rng.value != val) rng.value = val;
  }
  postThrottled(`xover_${ch}_${type}`, '/api/xover', () => ({
    channel: ch,
    type: type,
    config: currentConfig.out[ch][type]
  }), 60);
}

function applyQuickXover(n) {
  sendPost('/api/quick_xover', { type: n }, () => {
    showToast(`Quick ${n}-Way Diaplikasikan`);
    setTimeout(fetchConfig, 250);
  });
}

/* PEQ */
function changePeqTarget(t) {
  currentTarget = parseInt(t);
  renderPeqControls();
  drawCurve();
}

function drawCurve() {
  const svg = document.getElementById('peq-svg');
  if (!svg || !currentConfig) return;
  const bands = (currentTarget < 2) ? currentConfig.in[currentTarget].peq : currentConfig.out[currentTarget - 2].peq;

  let path = 'M 0 95 ';
  for (let px = 0; px <= 800; px += 4) {
    const f = 20 * Math.pow(1000, px / 800);
    let totalDb = 0;
    bands.forEach(b => {
      if (b.enabled && !b.bypass && Math.abs(b.gain_db) > 0.05) {
        const ratio = Math.log2(f / b.freq);
        const gain = b.gain_db * Math.exp(-Math.pow(ratio * b.q, 2));
        totalDb += gain;
      }
    });
    const py = 95 - (totalDb / 18) * 80;
    path += `L ${px} ${Math.max(10, Math.min(180, py))} `;
  }

  // Grid frekuensi logaritma
  const gridFreqs = [50, 100, 200, 500, 1000, 2000, 5000, 10000];
  let gridLines = '';
  gridFreqs.forEach(gf => {
    const gx = (Math.log10(gf / 20) / 3.0) * 800;
    gridLines += `<line x1="${gx}" y1="0" x2="${gx}" y2="190" stroke="#122036" stroke-dasharray="2"/>`;
    gridLines += `<text x="${gx + 2}" y="185" fill="#475569" font-size="9">${gf >= 1000 ? (gf/1000)+'k' : gf}</text>`;
  });

  svg.innerHTML = `
    ${gridLines}
    <line x1="0" y1="95" x2="800" y2="95" stroke="#1e3454"/>
    <text x="6" y="90" fill="#475569" font-size="9">0 dB</text>
    <text x="6" y="25" fill="#475569" font-size="9">+15 dB</text>
    <text x="6" y="165" fill="#475569" font-size="9">-15 dB</text>
    <path d="${path}" fill="none" stroke="#00d2ff" stroke-width="2.5"/>
  `;
}

function renderPeqControls() {
  const cont = document.getElementById('peq-bands-container');
  if (!cont || !currentConfig) return;
  const bands = (currentTarget < 2) ? currentConfig.in[currentTarget].peq : currentConfig.out[currentTarget - 2].peq;
  const types = ['Peaking', 'Low Shelf', 'High Shelf', 'Notch'];

  let html = '<div class="grid-2col">';
  bands.forEach((b, idx) => {
    html += `
      <div class="box-sub">
        <div class="box-title">
          <span>Band ${idx + 1}</span>
          <div style="display:flex; gap:4px;">
            <button class="btn-toggle ${b.enabled ? 'active' : ''}" onclick="togglePeqBand(${idx}, 'enabled')">${b.enabled ? 'ON' : 'OFF'}</button>
            <button class="btn-toggle ${b.bypass ? 'invert' : ''}" onclick="togglePeqBand(${idx}, 'bypass')">${b.bypass ? 'BYPASS' : 'ACTIVE'}</button>
          </div>
        </div>
        <div class="control-row">
          <label>Tipe Filter</label>
          <select onchange="updatePeqVal(${idx}, 'type', this.value)" style="flex:1;">
            ${types.map((t, ti) => `<option value="${ti}" ${b.type===ti?'selected':''}>${t}</option>`).join('')}
          </select>
        </div>
        <div class="control-row">
          <label>Frekuensi</label>
          <input type="range" id="peq-rng-freq-${idx}" min="20" max="20000" value="${b.freq}" oninput="updatePeqVal(${idx}, 'freq', this.value)">
          <input type="number" id="peq-num-freq-${idx}" min="20" max="20000" value="${Math.round(b.freq)}" style="width:70px;" onchange="updatePeqVal(${idx}, 'freq', this.value)">
        </div>
        <div class="control-row">
          <label>Gain (dB)</label>
          <input type="range" min="-15" max="15" step="0.5" value="${b.gain_db}" oninput="updatePeqVal(${idx}, 'gain_db', this.value)">
          <span class="control-val" id="peq-lbl-gain-${idx}">${b.gain_db > 0 ? '+' : ''}${b.gain_db.toFixed(1)} dB</span>
        </div>
        <div class="control-row">
          <label>Q-Factor</label>
          <input type="range" min="0.1" max="15" step="0.1" value="${b.q}" oninput="updatePeqVal(${idx}, 'q', this.value)">
          <span class="control-val" id="peq-lbl-q-${idx}">${b.q.toFixed(1)}</span>
        </div>
      </div>
    `;
  });
  html += '</div>';
  cont.innerHTML = html;
}

function togglePeqBand(bIdx, prop) {
  const bands = (currentTarget < 2) ? currentConfig.in[currentTarget].peq : currentConfig.out[currentTarget - 2].peq;
  bands[bIdx][prop] = !bands[bIdx][prop];
  sendPost('/api/peq', { target: currentTarget, band: bIdx, config: bands[bIdx] }, () => {
    renderPeqControls();
    drawCurve();
  });
}

function updatePeqVal(bIdx, prop, val) {
  const bands = (currentTarget < 2) ? currentConfig.in[currentTarget].peq : currentConfig.out[currentTarget - 2].peq;
  val = (prop === 'type') ? parseInt(val) : parseFloat(val);
  bands[bIdx][prop] = val;

  // Instant 60 FPS visual feedback on UI controls
  if (prop === 'gain_db') {
    const lbl = document.getElementById(`peq-lbl-gain-${bIdx}`);
    if (lbl) lbl.innerText = (val > 0 ? '+' : '') + val.toFixed(1) + ' dB';
  } else if (prop === 'freq') {
    const inp = document.getElementById(`peq-num-freq-${bIdx}`);
    const rng = document.getElementById(`peq-rng-freq-${bIdx}`);
    if (inp && inp.value != Math.round(val)) inp.value = Math.round(val);
    if (rng && rng.value != val) rng.value = val;
  } else if (prop === 'q') {
    const lbl = document.getElementById(`peq-lbl-q-${bIdx}`);
    if (lbl) lbl.innerText = val.toFixed(1);
  }

  drawCurve();
  postThrottled(`peq_${currentTarget}_${bIdx}`, '/api/peq', () => ({
    target: currentTarget,
    band: bIdx,
    config: bands[bIdx]
  }), 60);
}

/* LIMITER & DELAY */
function renderDynamics() {
  const cont = document.getElementById('dynamics-container');
  if (!cont || !currentConfig) return;
  const units = ['ms', 'meter (m)', 'feet (ft)'];

  let html = '';
  currentConfig.out.forEach((ch, idx) => {
    const speedOfSound = 331.3 + (0.606 * ch.delay.temp_c);
    const meters = (ch.delay.delay_ms * 0.001) * speedOfSound;
    const feet = meters * 3.28084;

    html += `
      <div class="box-sub">
        <div class="box-title">OUT ${idx + 1} (${ch.name})</div>
        
        <!-- Limiter -->
        <div style="background:#050912; border:1px solid #142034; border-radius:4px; padding:8px; margin-bottom:8px;">
          <div style="display:flex; justify-content:space-between; align-items:center; margin-bottom:6px;">
            <span style="font-weight:700; color:var(--yellow);">Peak Limiter</span>
            <button class="btn-toggle ${ch.limiter.enabled ? 'active' : ''}" onclick="toggleLimiterEnable(${idx})">${ch.limiter.enabled ? 'AKTIF' : 'BYPASS'}</button>
          </div>
          <div class="control-row">
            <label>Threshold</label>
            <input type="range" min="-40" max="0" step="0.5" value="${ch.limiter.threshold_db}" oninput="updateLimiterVal(${idx}, 'threshold_db', this.value)">
            <span class="control-val" id="lim-lbl-th-${idx}">${ch.limiter.threshold_db.toFixed(1)} dBFS</span>
          </div>
          <div class="control-row">
            <label>Attack (ms)</label>
            <input type="range" min="0.1" max="50" step="0.5" value="${ch.limiter.attack_ms}" oninput="updateLimiterVal(${idx}, 'attack_ms', this.value)">
            <span class="control-val" id="lim-lbl-att-${idx}">${ch.limiter.attack_ms.toFixed(1)} ms</span>
          </div>
          <div class="control-row">
            <label>Release (ms)</label>
            <input type="range" min="10" max="1000" step="10" value="${ch.limiter.release_ms}" oninput="updateLimiterVal(${idx}, 'release_ms', this.value)">
            <span class="control-val" id="lim-lbl-rel-${idx}">${Math.round(ch.limiter.release_ms)} ms</span>
          </div>
        </div>

        <!-- Delay -->
        <div style="background:#050912; border:1px solid #142034; border-radius:4px; padding:8px;">
          <div style="font-weight:700; color:var(--cyan-accent); margin-bottom:6px;">Time Alignment Delay</div>
          <div class="control-row">
            <label>Delay (ms)</label>
            <input type="range" id="del-rng-ms-${idx}" min="0" max="100" step="0.1" value="${ch.delay.delay_ms}" oninput="updateDelayVal(${idx}, 'delay_ms', this.value)">
            <input type="number" id="del-num-ms-${idx}" min="0" max="100" step="0.1" value="${ch.delay.delay_ms.toFixed(1)}" style="width:70px;" onchange="updateDelayVal(${idx}, 'delay_ms', this.value)">
          </div>
          <div class="control-row">
            <label>Satuan</label>
            <select onchange="updateDelayVal(${idx}, 'unit', this.value)" style="flex:1;">
              ${units.map((u, ui) => `<option value="${ui}" ${ch.delay.unit===ui?'selected':''}>${u}</option>`).join('')}
            </select>
          </div>
          <div class="control-row">
            <label>Suhu Suara (°C)</label>
            <input type="number" id="del-num-temp-${idx}" min="0" max="45" value="${ch.delay.temp_c}" style="width:70px;" onchange="updateDelayVal(${idx}, 'temp_c', this.value)">
            <span style="font-size:11px; color:var(--text-muted); font-family:monospace;" id="del-lbl-dist-${idx}">${meters.toFixed(2)} m / ${feet.toFixed(2)} ft</span>
          </div>
        </div>
      </div>
    `;
  });
  cont.innerHTML = html;
}

function toggleLimiterEnable(ch) {
  const target = currentConfig.out[ch].limiter;
  target.enabled = !target.enabled;
  sendPost('/api/limiter', { channel: ch, enabled: target.enabled, threshold_db: target.threshold_db, attack_ms: target.attack_ms, release_ms: target.release_ms }, () => {
    renderDynamics();
  });
}

function updateLimiterVal(ch, prop, val) {
  val = parseFloat(val);
  currentConfig.out[ch].limiter[prop] = val;
  if (prop === 'threshold_db') {
    const el = document.getElementById(`lim-lbl-th-${ch}`);
    if (el) el.innerText = val.toFixed(1) + ' dBFS';
  } else if (prop === 'attack_ms') {
    const el = document.getElementById(`lim-lbl-att-${ch}`);
    if (el) el.innerText = val.toFixed(1) + ' ms';
  } else if (prop === 'release_ms') {
    const el = document.getElementById(`lim-lbl-rel-${ch}`);
    if (el) el.innerText = Math.round(val) + ' ms';
  }
  postThrottled(`limiter_${ch}`, '/api/limiter', () => {
    const target = currentConfig.out[ch].limiter;
    return { channel: ch, enabled: target.enabled, threshold_db: target.threshold_db, attack_ms: target.attack_ms, release_ms: target.release_ms };
  }, 60);
}

function updateDelayVal(ch, prop, val) {
  val = (prop === 'unit') ? parseInt(val) : parseFloat(val);
  currentConfig.out[ch].delay[prop] = val;
  const target = currentConfig.out[ch].delay;
  if (prop === 'delay_ms') {
    const inp = document.getElementById(`del-num-ms-${ch}`);
    const rng = document.getElementById(`del-rng-ms-${ch}`);
    if (inp && inp.value != val) inp.value = val.toFixed(1);
    if (rng && rng.value != val) rng.value = val;
  }
  const speedOfSound = 331.3 + (0.606 * target.temp_c);
  const meters = (target.delay_ms * 0.001) * speedOfSound;
  const feet = meters * 3.28084;
  const distEl = document.getElementById(`del-lbl-dist-${ch}`);
  if (distEl) distEl.innerText = `${meters.toFixed(2)} m / ${feet.toFixed(2)} ft`;

  postThrottled(`delay_${ch}`, '/api/delay', () => ({
    channel: ch,
    delay_ms: target.delay_ms,
    unit: target.unit,
    temp_c: target.temp_c
  }), 60);
}

/* PRESETS */
function renderPresets() {
  const cont = document.getElementById('preset-list');
  if (!cont) return;
  fetch('/api/presets')
    .then(r => r.json())
    .then(data => {
      let html = '';
      data.presets.forEach(p => {
        const isCur = p.is_current;
        html += `
          <div class="box-sub" style="display:flex; justify-content:space-between; align-items:center; flex-wrap:wrap; gap:8px; border-color:${isCur ? 'var(--cyan-accent)' : 'var(--border-card)'};">
            <div style="display:flex; align-items:center; gap:8px;">
              <span style="font-weight:800; color:${isCur ? 'var(--cyan-accent)' : 'var(--text-main)'}; font-size:13px;">Slot ${p.slot}</span>
              <input type="text" id="p-name-${p.slot}" value="${p.name}" ${p.is_factory ? 'disabled' : ''} style="width:140px; font-weight:600;">
              ${isCur ? '<span class="badge" style="color:var(--cyan-accent); border:1px solid var(--cyan-accent);">ACTIVE</span>' : ''}
              ${p.is_factory ? '<span class="badge">READ ONLY</span>' : ''}
            </div>
            <div style="display:flex; gap:6px;">
              <button class="btn-act cyan" onclick="loadPresetSlot(${p.slot})">LOAD</button>
              ${!p.is_factory ? `
                <button class="btn-act" onclick="savePresetSlot(${p.slot})">SIMPAN</button>
                <button class="btn-act" onclick="renamePresetSlot(${p.slot})">GANTI NAMA</button>
              ` : ''}
            </div>
          </div>
        `;
      });
      cont.innerHTML = html;
    });
}

function loadPresetSlot(s) {
  sendPost('/api/preset/load', { slot: s }, res => {
    if (res.status === 'ok') {
      showToast(`Preset Slot ${s} Berhasil Dimuat!`);
      setTimeout(fetchConfig, 300);
    } else {
      showToast('Gagal memuat preset!');
    }
  });
}

function savePresetSlot(s) {
  sendPost('/api/preset/save', { slot: s }, res => {
    if (res.status === 'ok') {
      showToast(`Tersimpan ke Slot ${s}!`);
      setTimeout(renderPresets, 300);
    } else {
      showToast('Gagal menyimpan preset!');
    }
  });
}

function renamePresetSlot(s) {
  const el = document.getElementById(`p-name-${s}`);
  if (!el) return;
  const newName = el.value.trim();
  sendPost('/api/preset/rename', { slot: s, name: newName }, res => {
    if (res.status === 'ok') {
      showToast(`Slot ${s} diubah ke "${newName}"`);
      setTimeout(fetchConfig, 300);
    }
  });
}

function confirmFactoryReset() {
  if (confirm("Reset SEMUA pengaturan DSP dan preset pengguna ke pengaturan pabrik?")) {
    sendPost('/api/factory_reset', {}, () => {
      showToast("Factory Reset Selesai!");
      setTimeout(fetchConfig, 500);
    });
  }
}

/* WIFI SETUP */
function saveWifiSetup() {
  const mode = parseInt(document.getElementById('sel-wifi-mode').value);
  const ssid = document.getElementById('inp-wifi-ssid').value.trim();
  const pass = document.getElementById('inp-wifi-pass').value.trim();

  sendPost('/api/setup', { wifi_mode: mode, wifi_ssid: ssid, wifi_pass: pass }, () => {
    showToast("Pengaturan WiFi Diterapkan!");
  });
}

window.onload = initLiveTelemetry;
</script>
</body>
</html>
)rawhtml";
