#ifndef PAGES_H
#define PAGES_H

#include <pgmspace.h>

const char PAGE_DASHBOARD[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1">
    <title>AgriSense TCP Node</title>
    <style>
        :root {
            --bg: #1a1a2e;
            --card: #16213e;
            --accent: #0f3460;
            --text: #eaeaea;
            --success: #4caf50;
            --warning: #ff9800;
            --danger: #f44336;
        }
        * { box-sizing: border-box; margin: 0; padding: 0; }
        body { font-family: -apple-system, BlinkMacSystemFont, 'Segoe UI', Roboto, sans-serif; background: var(--bg); color: var(--text); display: flex; min-height: 100vh; }
        .sidebar { width: 180px; background: var(--card); padding: 20px; display: flex; flex-direction: column; }
        .sidebar h2 { font-size: 14px; margin-bottom: 20px; color: var(--accent); }
        .sidebar a { color: var(--text); text-decoration: none; padding: 10px; border-radius: 4px; margin-bottom: 5px; }
        .sidebar a:hover { background: var(--accent); }
        .content { flex: 1; padding: 20px; max-width: 480px; }
        .stats-header { display: flex; gap: 10px; margin-bottom: 20px; flex-wrap: wrap; }
        .stat { background: var(--card); padding: 10px 15px; border-radius: 8px; flex: 1; min-width: 80px; }
        .stat-label { font-size: 10px; color: #888; text-transform: uppercase; }
        .stat-value { font-size: 18px; font-weight: bold; }
        .card { background: var(--card); border-radius: 8px; padding: 20px; margin-bottom: 15px; }
        .card h3 { margin-bottom: 10px; font-size: 14px; color: #888; }
        .badge { display: inline-block; padding: 4px 8px; border-radius: 4px; font-size: 12px; }
        .badge-success { background: var(--success); color: white; }
        .badge-warning { background: var(--warning); color: white; }
        .badge-danger { background: var(--danger); color: white; }
        .footer-bar { position: fixed; bottom: 0; left: 180px; right: 0; background: var(--card); padding: 10px 20px; display: flex; justify-content: space-between; font-size: 12px; }
        .dot { width: 8px; height: 8px; border-radius: 50%; display: inline-block; margin-right: 5px; }
        .dot-online { background: var(--success); }
        .dot-offline { background: var(--danger); }
        details { margin-top: 10px; }
        summary { cursor: pointer; color: var(--accent); }
        .value { font-size: 24px; font-weight: bold; }
    </style>
</head>
<body>
    <div class="sidebar">
        <h2>AgriSense TCP</h2>
        <a href="/">Home</a>
        <a href="/wifi">WiFi</a>
        <a href="/settings">Propriedades</a>
        <a href="/config">Config</a>
        <a href="/docs">API Docs</a>
    </div>
    <div class="content">
        <div class="stats-header">
            <div class="stat">
                <div class="stat-label">RX</div>
                <div class="stat-value" id="rx">0</div>
            </div>
            <div class="stat">
                <div class="stat-label">TX</div>
                <div class="stat-value" id="tx">0</div>
            </div>
            <div class="stat">
                <div class="stat-label">Mem</div>
                <div class="stat-value" id="mem">0</div>
            </div>
            <div class="stat">
                <div class="stat-label">Uptime</div>
                <div class="stat-value" id="uptime">0</div>
            </div>
        </div>
        
        <div class="card">
            <h3>Status</h3>
            <p><span class="dot" id="status-dot"></span> <span id="status-text">Connecting...</span></p>
            <p>Hub: <span id="hub-ip">-</span></p>
            <p>Slot: <span id="slot">-</span></p>
        </div>
        
        <div class="card">
            <h3>Sensor</h3>
            <div class="value" id="sensor-value">--</div>
            <p id="sensor-unit"></p>
        </div>
        
        <details>
            <summary>Detalhes</summary>
            <div class="card">
                <p>Device ID: <span id="device-id">-</span></p>
                <p>Firmware: <span id="fw-version">-</span></p>
                <p>WiFi: <span id="wifi-ssid">-</span></p>
                <p>IP: <span id="ip">-</span></p>
            </div>
        </details>
    </div>
    
    <div class="footer-bar">
        <div><span class="dot dot-online"></span> Gateway</div>
        <div id="clock">--:--:--</div>
        <div id="uptime-footer">Uptime: 0s</div>
    </div>
    
    <script>
        function updateStats() {
            fetch('/api/state')
                .then(r => r.json())
                .then(data => {
                    document.getElementById('rx').textContent = data.rx_count || 0;
                    document.getElementById('tx').textContent = data.tx_count || 0;
                    document.getElementById('mem').textContent = (data.free_heap / 1024).toFixed(1) + 'K';
                    document.getElementById('uptime').textContent = formatUptime(data.uptime_s);
                    document.getElementById('device-id').textContent = data.device_id || '-';
                    document.getElementById('fw-version').textContent = data.fw_version || '-';
                    document.getElementById('wifi-ssid').textContent = data.wifi_ssid || '-';
                    document.getElementById('ip').textContent = data.ip || '-';
                    document.getElementById('hub-ip').textContent = data.hub_ip || '-';
                    document.getElementById('slot').textContent = data.slot || '-';
                    
                    if (data.online) {
                        document.getElementById('status-dot').className = 'dot dot-online';
                        document.getElementById('status-text').textContent = 'Online';
                    } else {
                        document.getElementById('status-dot').className = 'dot dot-offline';
                        document.getElementById('status-text').textContent = 'Offline';
                    }
                    
                    if (data.sensor_value !== undefined) {
                        document.getElementById('sensor-value').textContent = data.sensor_value;
                        document.getElementById('sensor-unit').textContent = data.sensor_unit || '';
                    }
                })
                .catch(err => console.error('Error:', err));
        }
        
        function formatUptime(seconds) {
            if (!seconds) return '0s';
            const h = Math.floor(seconds / 3600);
            const m = Math.floor((seconds % 3600) / 60);
            const s = seconds % 60;
            if (h > 0) return h + 'h ' + m + 'm';
            if (m > 0) return m + 'm ' + s + 's';
            return s + 's';
        }
        
        function updateClock() {
            const now = new Date();
            document.getElementById('clock').textContent = now.toLocaleTimeString();
        }
        
        setInterval(updateStats, 3000);
        setInterval(updateClock, 1000);
        updateStats();
        updateClock();
    </script>
</body>
</html>
)rawliteral";

const char PAGE_DOCS[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
    <meta charset="UTF-8">
    <title>API Docs - AgriSense TCP</title>
    <style>
        body { font-family: sans-serif; padding: 20px; background: #1a1a2e; color: #eaeaea; }
        h1 { color: #0f3460; }
        .endpoint { background: #16213e; padding: 15px; border-radius: 8px; margin: 10px 0; }
        .method { display: inline-block; padding: 2px 8px; border-radius: 4px; font-weight: bold; }
        .get { background: #4caf50; }
        .post { background: #2196f3; }
        code { background: #0f3460; padding: 2px 6px; border-radius: 4px; }
    </style>
</head>
<body>
    <h1>API Documentation</h1>
    <div class="endpoint">
        <span class="method get">GET</span> <code>/api/state</code>
        <p>Returns device state as JSON</p>
    </div>
    <div class="endpoint">
        <span class="method post">POST</span> <code>/api/settings</code>
        <p>Update device settings (device_name)</p>
    </div>
    <div class="endpoint">
        <span class="method get">GET</span> <code>/api/wifi</code>
        <p>Get WiFi configuration</p>
    </div>
    <div class="endpoint">
        <span class="method post">POST</span> <code>/api/wifi</code>
        <p>Update WiFi configuration</p>
    </div>
    <div class="endpoint">
        <span class="method post">POST</span> <code>/api/ota</code>
        <p>Upload firmware for OTA update</p>
    </div>
    <div class="endpoint">
        <span class="method post">POST</span> <code>/api/restart</code>
        <p>Restart device</p>
    </div>
    <p><a href="/">Back to Dashboard</a></p>
</body>
</html>
)rawliteral";

const char PAGE_WIFI_CONFIG[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1">
    <title>WiFi Config - AgriSense</title>
    <style>
        body { font-family: sans-serif; padding: 20px; background: #1a1a2e; color: #eaeaea; }
        h1 { color: #0f3460; }
        input { width: 100%; padding: 10px; margin: 5px 0 15px; border: none; border-radius: 4px; background: #16213e; color: #eaeaea; }
        button { background: #0f3460; color: white; padding: 10px 20px; border: none; border-radius: 4px; cursor: pointer; }
        button:hover { background: #1a4a7a; }
    </style>
</head>
<body>
    <h1>WiFi Configuration</h1>
    <form action="/api/wifi" method="POST">
        <label>SSID:</label>
        <input type="text" name="ssid" required>
        <label>Password:</label>
        <input type="password" name="password" required>
        <label>Hub IP:</label>
        <input type="text" name="hub_ip" placeholder="192.168.1.100">
        <button type="submit">Save</button>
    </form>
    <p><a href="/">Back to Dashboard</a></p>
</body>
</html>
)rawliteral";

#endif
