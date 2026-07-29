#ifndef DASHBOARD_HTML_H
#define DASHBOARD_HTML_H

#include <Arduino.h>

/**
 * ============================================================
 * dashboard_html.h
 * ------------------------------------------------------------
 * Embedded HTML/CSS/JS for the Diesel Delivery Monitor
 * web dashboard. Stored in PROGMEM (flash) to save RAM.
 *
 * Features:
 *  - Dark glassmorphism UI with smooth animations
 *  - WebSocket real-time updates (1 second interval)
 *  - Auto-reconnect on disconnect
 *  - Reset buttons for total liters and delivery counter
 *  - Responsive layout (mobile + desktop)
 * ============================================================
 */

static const char DASHBOARD_HTML[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width, initial-scale=1.0">
<title>Diesel Delivery Monitor</title>
<style>
  @import url('https://fonts.googleapis.com/css2?family=Inter:wght@300;400;500;600;700&display=swap');

  :root {
    --bg-primary: #0a0e1a;
    --bg-card: rgba(255, 255, 255, 0.04);
    --bg-card-hover: rgba(255, 255, 255, 0.07);
    --border-card: rgba(255, 255, 255, 0.08);
    --text-primary: #e8eaf0;
    --text-secondary: #8b92a8;
    --text-dim: #545b73;
    --accent-blue: #3b82f6;
    --accent-cyan: #06b6d4;
    --accent-green: #10b981;
    --accent-amber: #f59e0b;
    --accent-red: #ef4444;
    --accent-purple: #8b5cf6;
    --glow-blue: rgba(59, 130, 246, 0.15);
    --glow-cyan: rgba(6, 182, 212, 0.15);
    --glow-green: rgba(16, 185, 129, 0.15);
    --glow-amber: rgba(245, 158, 11, 0.15);
  }

  * { margin: 0; padding: 0; box-sizing: border-box; }

  body {
    font-family: 'Inter', -apple-system, sans-serif;
    background: var(--bg-primary);
    color: var(--text-primary);
    min-height: 100vh;
    overflow-x: hidden;
  }

  /* Animated background gradient */
  body::before {
    content: '';
    position: fixed;
    top: -50%; left: -50%;
    width: 200%; height: 200%;
    background: radial-gradient(ellipse at 30% 20%, rgba(59,130,246,0.06) 0%, transparent 50%),
                radial-gradient(ellipse at 70% 80%, rgba(6,182,212,0.04) 0%, transparent 50%);
    animation: bgShift 20s ease-in-out infinite alternate;
    z-index: 0;
  }
  @keyframes bgShift {
    0% { transform: translate(0, 0); }
    100% { transform: translate(-5%, 3%); }
  }

  .container {
    position: relative;
    z-index: 1;
    max-width: 1000px;
    margin: 0 auto;
    padding: 24px 20px;
  }

  /* Header */
  .header {
    display: flex;
    align-items: center;
    justify-content: space-between;
    margin-bottom: 32px;
    padding-bottom: 20px;
    border-bottom: 1px solid var(--border-card);
  }
  .header-left {
    display: flex;
    align-items: center;
    gap: 14px;
  }
  .header-icon {
    width: 44px; height: 44px;
    background: linear-gradient(135deg, var(--accent-blue), var(--accent-cyan));
    border-radius: 12px;
    display: flex; align-items: center; justify-content: center;
    font-size: 22px;
    box-shadow: 0 4px 20px var(--glow-blue);
  }
  .header h1 {
    font-size: 20px;
    font-weight: 600;
    letter-spacing: -0.3px;
  }
  .header h1 span {
    color: var(--text-secondary);
    font-weight: 400;
    font-size: 14px;
    display: block;
    margin-top: 2px;
  }

  /* Connection badge */
  .conn-badge {
    display: flex; align-items: center; gap: 8px;
    padding: 6px 14px;
    border-radius: 20px;
    font-size: 12px;
    font-weight: 500;
    letter-spacing: 0.3px;
    transition: all 0.4s ease;
  }
  .conn-badge.connected {
    background: rgba(16, 185, 129, 0.1);
    color: var(--accent-green);
    border: 1px solid rgba(16, 185, 129, 0.2);
  }
  .conn-badge.disconnected {
    background: rgba(239, 68, 68, 0.1);
    color: var(--accent-red);
    border: 1px solid rgba(239, 68, 68, 0.2);
  }
  .conn-dot {
    width: 8px; height: 8px;
    border-radius: 50%;
    transition: background 0.4s ease;
  }
  .connected .conn-dot {
    background: var(--accent-green);
    box-shadow: 0 0 8px var(--accent-green);
    animation: pulse 2s ease-in-out infinite;
  }
  .disconnected .conn-dot {
    background: var(--accent-red);
    box-shadow: 0 0 8px var(--accent-red);
  }
  @keyframes pulse {
    0%, 100% { opacity: 1; }
    50% { opacity: 0.4; }
  }

  /* Card grid */
  .grid {
    display: grid;
    grid-template-columns: repeat(3, 1fr);
    gap: 16px;
    margin-bottom: 20px;
  }
  @media (max-width: 640px) {
    .grid { grid-template-columns: repeat(2, 1fr); }
    .grid .card:last-child { grid-column: span 2; }
  }

  /* Cards */
  .card {
    background: var(--bg-card);
    border: 1px solid var(--border-card);
    border-radius: 16px;
    padding: 20px;
    backdrop-filter: blur(12px);
    -webkit-backdrop-filter: blur(12px);
    transition: all 0.3s ease;
    position: relative;
    overflow: hidden;
  }
  .card::before {
    content: '';
    position: absolute;
    top: 0; left: 0;
    width: 100%; height: 3px;
    opacity: 0;
    transition: opacity 0.3s ease;
  }
  .card:hover {
    background: var(--bg-card-hover);
    transform: translateY(-2px);
    box-shadow: 0 8px 32px rgba(0,0,0,0.3);
  }
  .card:hover::before { opacity: 1; }

  .card-label {
    font-size: 11px;
    font-weight: 500;
    text-transform: uppercase;
    letter-spacing: 1px;
    color: var(--text-dim);
    margin-bottom: 10px;
    display: flex; align-items: center; gap: 6px;
  }
  .card-label .icon { font-size: 14px; }

  .card-value {
    font-size: 32px;
    font-weight: 700;
    letter-spacing: -1px;
    line-height: 1;
    transition: color 0.3s ease;
  }
  .card-unit {
    font-size: 13px;
    font-weight: 400;
    color: var(--text-secondary);
    margin-top: 6px;
  }

  /* Card accent colors */
  .card.freq { --card-color: var(--accent-cyan); }
  .card.freq::before { background: linear-gradient(90deg, var(--accent-cyan), transparent); }
  .card.freq .card-value { color: var(--accent-cyan); }

  .card.flow { --card-color: var(--accent-blue); }
  .card.flow::before { background: linear-gradient(90deg, var(--accent-blue), transparent); }
  .card.flow .card-value { color: var(--accent-blue); }

  .card.total { --card-color: var(--accent-green); }
  .card.total::before { background: linear-gradient(90deg, var(--accent-green), transparent); }
  .card.total .card-value { color: var(--accent-green); }

  .card.state { --card-color: var(--accent-amber); }
  .card.state::before { background: linear-gradient(90deg, var(--accent-amber), transparent); }

  .card.delivery { --card-color: var(--accent-purple); }
  .card.delivery::before { background: linear-gradient(90deg, var(--accent-purple), transparent); }
  .card.delivery .card-value { color: var(--accent-purple); }

  .card.duration { --card-color: var(--text-secondary); }
  .card.duration::before { background: linear-gradient(90deg, var(--text-secondary), transparent); }

  /* State badge */
  .state-badge {
    display: inline-flex;
    align-items: center;
    gap: 8px;
    padding: 6px 16px;
    border-radius: 10px;
    font-size: 14px;
    font-weight: 600;
    letter-spacing: 0.5px;
    margin-top: 4px;
  }
  .state-badge.idle {
    background: rgba(139, 146, 168, 0.12);
    color: var(--text-secondary);
    border: 1px solid rgba(139, 146, 168, 0.15);
  }
  .state-badge.running {
    background: rgba(245, 158, 11, 0.12);
    color: var(--accent-amber);
    border: 1px solid rgba(245, 158, 11, 0.2);
    animation: glowPulse 1.5s ease-in-out infinite;
  }
  .state-badge.completed {
    background: rgba(16, 185, 129, 0.12);
    color: var(--accent-green);
    border: 1px solid rgba(16, 185, 129, 0.2);
  }
  @keyframes glowPulse {
    0%, 100% { box-shadow: 0 0 8px rgba(245,158,11,0.1); }
    50% { box-shadow: 0 0 20px rgba(245,158,11,0.25); }
  }

  /* Delivery liters (sub-card during delivery) */
  .delivery-info {
    margin-top: 24px;
    padding: 16px 20px;
    background: rgba(245, 158, 11, 0.05);
    border: 1px solid rgba(245, 158, 11, 0.12);
    border-radius: 12px;
    display: none;
  }
  .delivery-info.visible { display: block; }
  .delivery-info .label {
    font-size: 11px;
    text-transform: uppercase;
    letter-spacing: 1px;
    color: var(--accent-amber);
    margin-bottom: 6px;
    font-weight: 500;
  }
  .delivery-info .value {
    font-size: 24px;
    font-weight: 700;
    color: var(--accent-amber);
  }

  /* Action buttons */
  .actions {
    display: flex;
    gap: 12px;
    margin-top: 24px;
    flex-wrap: wrap;
  }
  .btn {
    flex: 1;
    min-width: 180px;
    padding: 14px 20px;
    border: 1px solid var(--border-card);
    border-radius: 12px;
    background: var(--bg-card);
    color: var(--text-primary);
    font-family: 'Inter', sans-serif;
    font-size: 13px;
    font-weight: 500;
    cursor: pointer;
    transition: all 0.25s ease;
    backdrop-filter: blur(12px);
    display: flex; align-items: center; justify-content: center; gap: 8px;
  }
  .btn:hover {
    background: var(--bg-card-hover);
    transform: translateY(-1px);
    box-shadow: 0 4px 16px rgba(0,0,0,0.3);
  }
  .btn:active { transform: translateY(0); }
  .btn.danger {
    border-color: rgba(239, 68, 68, 0.2);
    color: var(--accent-red);
  }
  .btn.danger:hover {
    background: rgba(239, 68, 68, 0.08);
    border-color: rgba(239, 68, 68, 0.35);
  }
  .btn.warn {
    border-color: rgba(245, 158, 11, 0.2);
    color: var(--accent-amber);
  }
  .btn.warn:hover {
    background: rgba(245, 158, 11, 0.08);
    border-color: rgba(245, 158, 11, 0.35);
  }

  /* Toast notification */
  .toast {
    position: fixed;
    bottom: 24px;
    right: 24px;
    padding: 12px 20px;
    border-radius: 10px;
    font-size: 13px;
    font-weight: 500;
    background: rgba(16, 185, 129, 0.15);
    border: 1px solid rgba(16, 185, 129, 0.3);
    color: var(--accent-green);
    opacity: 0;
    transform: translateY(12px);
    transition: all 0.3s ease;
    z-index: 100;
    backdrop-filter: blur(12px);
  }
  .toast.show {
    opacity: 1;
    transform: translateY(0);
  }

  /* Footer */
  .footer {
    margin-top: 32px;
    text-align: center;
    font-size: 11px;
    color: var(--text-dim);
    letter-spacing: 0.5px;
  }
  .footer span { color: var(--text-secondary); }
</style>
</head>
<body>
<div class="container">

  <!-- Header -->
  <div class="header">
    <div class="header-left">
      <div class="header-icon">⛽</div>
      <h1>Diesel Delivery Monitor<span>Real-time Dashboard</span></h1>
    </div>
    <div class="conn-badge disconnected" id="connBadge">
      <div class="conn-dot"></div>
      <span id="connText">Disconnected</span>
    </div>
  </div>

  <!-- Primary metrics -->
  <div class="grid">
    <div class="card freq">
      <div class="card-label"><span class="icon">〜</span> Frequency</div>
      <div class="card-value" id="freq">--</div>
      <div class="card-unit">Hz</div>
    </div>
    <div class="card flow">
      <div class="card-label"><span class="icon">▸</span> Flow Rate</div>
      <div class="card-value" id="flow">--</div>
      <div class="card-unit">L/min</div>
    </div>
    <div class="card total">
      <div class="card-label"><span class="icon">∑</span> Total Volume</div>
      <div class="card-value" id="total">--</div>
      <div class="card-unit">Liters</div>
    </div>
  </div>

  <!-- Secondary metrics -->
  <div class="grid">
    <div class="card state">
      <div class="card-label"><span class="icon">◉</span> Delivery State</div>
      <div class="state-badge idle" id="stateBadge">
        <span id="stateText">IDLE</span>
      </div>
    </div>
    <div class="card delivery">
      <div class="card-label"><span class="icon">#</span> Delivery Count</div>
      <div class="card-value" id="delCount">--</div>
      <div class="card-unit">deliveries</div>
    </div>
    <div class="card duration">
      <div class="card-label"><span class="icon">⏱</span> Current Duration</div>
      <div class="card-value" id="duration">00:00</div>
      <div class="card-unit">mm:ss</div>
    </div>
  </div>

  <!-- Active delivery info -->
  <div class="delivery-info" id="deliveryInfo">
    <div class="label">⛽ Active Delivery Volume</div>
    <div class="value"><span id="delLiters">0.00</span> L</div>
  </div>

  <!-- Action buttons -->
  <div class="actions">
    <button class="btn warn" onclick="sendCmd('reset_total')">
      🔄 Clear Total Liters
    </button>
    <button class="btn danger" onclick="sendCmd('reset_deliveries')">
      ⚠ Reset Delivery Counter
    </button>
  </div>

  <div class="footer">
    ESP32 Diesel Delivery Verification System &bull;
    <span id="devId">--</span> &bull;
    <span id="timestamp">--</span>
  </div>
</div>

<!-- Toast -->
<div class="toast" id="toast"></div>

<script>
  let ws;
  let reconnectTimer;

  function connect() {
    const host = window.location.hostname;
    const port = window.location.port || '80';
    ws = new WebSocket(`ws://${host}:${port}/ws`);

    ws.onopen = () => {
      setConnected(true);
      clearTimeout(reconnectTimer);
    };

    ws.onclose = () => {
      setConnected(false);
      reconnectTimer = setTimeout(connect, 2000);
    };

    ws.onerror = () => {
      ws.close();
    };

    ws.onmessage = (e) => {
      try {
        const d = JSON.parse(e.data);

        if (d.type === 'ack') {
          showToast(d.message || 'Command executed');
          return;
        }

        // Update values with animation
        animateValue('freq', d.frequency_hz, 1);
        animateValue('flow', d.flow_rate, 2);
        animateValue('total', d.total_liters, 2);

        // Delivery count
        document.getElementById('delCount').textContent = d.delivery_count ?? '--';

        // Duration
        if (d.delivery_duration !== undefined) {
          const m = Math.floor(d.delivery_duration / 60);
          const s = d.delivery_duration % 60;
          document.getElementById('duration').textContent =
            String(m).padStart(2,'0') + ':' + String(s).padStart(2,'0');
        }

        // Active delivery liters
        const info = document.getElementById('deliveryInfo');
        if (d.delivery_state === 'RUNNING') {
          info.classList.add('visible');
          document.getElementById('delLiters').textContent =
            (d.delivery_liters ?? 0).toFixed(2);
        } else {
          info.classList.remove('visible');
        }

        // State badge
        updateState(d.delivery_state || 'IDLE');

        // Footer info
        document.getElementById('devId').textContent = d.device_id || '--';
        document.getElementById('timestamp').textContent = d.timestamp || '--';

      } catch (err) { /* ignore parse errors */ }
    };
  }

  function animateValue(id, newVal, decimals) {
    const el = document.getElementById(id);
    const v = parseFloat(newVal);
    if (isNaN(v)) { el.textContent = '--'; return; }
    el.textContent = v.toFixed(decimals);
    el.style.transition = 'none';
    el.style.textShadow = `0 0 16px currentColor`;
    requestAnimationFrame(() => {
      el.style.transition = 'text-shadow 0.6s ease';
      el.style.textShadow = 'none';
    });
  }

  function updateState(state) {
    const badge = document.getElementById('stateBadge');
    const text = document.getElementById('stateText');
    badge.className = 'state-badge ' + state.toLowerCase();
    text.textContent = state;
  }

  function setConnected(ok) {
    const badge = document.getElementById('connBadge');
    const text = document.getElementById('connText');
    badge.className = 'conn-badge ' + (ok ? 'connected' : 'disconnected');
    text.textContent = ok ? 'Connected' : 'Disconnected';
  }

  function sendCmd(cmd) {
    if (ws && ws.readyState === WebSocket.OPEN) {
      ws.send(JSON.stringify({ cmd: cmd }));
    } else {
      showToast('Not connected!');
    }
  }

  function showToast(msg) {
    const t = document.getElementById('toast');
    t.textContent = msg;
    t.classList.add('show');
    setTimeout(() => t.classList.remove('show'), 2500);
  }

  // Start connection
  connect();
</script>
</body>
</html>
)rawliteral";

#endif // DASHBOARD_HTML_H
