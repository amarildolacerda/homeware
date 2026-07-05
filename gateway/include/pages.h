#ifndef PAGES_H
#define PAGES_H

const char PAGE_DASHBOARD[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="pt-BR">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width,initial-scale=1,maximum-scale=1">
<title>ESP-NOW Gateway</title>
<style>
:root { --bg:#0b0f1a; --card:#111827; --border:#1f2937; --text:#e5e7eb; --muted:#9ca3af; --primary:#22d3ee; --success:#22c55e; --danger:#ef4444; --warn:#f59e0b; }
* { box-sizing:border-box; margin:0; padding:0; }
body { font-family:-apple-system,BlinkMacSystemFont,Segoe UI,Roboto,sans-serif; background:var(--bg); color:var(--text); min-height:100vh; }
.container { max-width:900px; margin:0 auto; padding:20px; }
.header { display:flex; justify-content:space-between; align-items:center; margin-bottom:24px; flex-wrap:wrap; gap:16px; }
h1 { font-size:1.5rem; font-weight:600; }
.badge { display:inline-block; padding:4px 10px; border-radius:9999px; font-size:0.75rem; font-weight:600; }
.badge-online { background:#064e3b; color:#6ee7b7; }
.badge-offline { background:#450a0a; color:#fca5a5; }
.badge-pairing { background:#451a03; color:#fcd34d; }
.card { background:var(--card); border:1px solid var(--border); border-radius:12px; padding:20px; margin-bottom:16px; }
.card h2 { font-size:1rem; font-weight:600; margin-bottom:16px; display:flex; align-items:center; gap:8px; }
.grid { display:grid; grid-template-columns:repeat(auto-fill,minmax(280px,1fr)); gap:16px; }
.sensor { background:#0c1222; border:1px solid var(--border); border-radius:8px; padding:16px; transition:border-color .2s; }
.sensor:hover { border-color:var(--primary); }
.sensor-header { display:flex; justify-content:space-between; align-items:flex-start; margin-bottom:12px; }
.sensor-name { font-weight:600; font-size:0.95rem; }
.sensor-type { font-size:0.7rem; color:var(--muted); text-transform:uppercase; letter-spacing:.05em; }
.sensor-meta { display:grid; grid-template-columns:repeat(2,1fr); gap:8px; font-size:0.8rem; }
.sensor-meta div { display:flex; flex-direction:column; }
.sensor-meta .label { color:var(--muted); font-size:0.65rem; }
.sensor-meta .value { font-weight:500; }
.sensor-state { margin-top:12px; padding-top:12px; border-top:1px solid var(--border); display:flex; flex-wrap:wrap; gap:8px; }
.state-item { background:#0b0f1a; padding:4px 10px; border-radius:6px; font-size:0.75rem; }
.state-temp { color:#f87171; }
.state-hum { color:#60a5fa; }
.state-contact { color:#a78bfa; }
.state-motion { color:#fbbf24; }
.state-gas { color:#f87171; }
.state-rain { color:#60a5fa; }
.state-tank { color:#4ade80; }
.state-battery { color:#fcd34d; }
.btn { padding:10px 18px; border:none; border-radius:8px; font-weight:600; cursor:pointer; transition:all .2s; font-size:0.85rem; min-height:44px; }
.btn-primary { background:var(--primary); color:#0b0f1a; }
.btn-primary:hover { filter:brightness(1.1); }
.btn-danger { background:var(--danger); color:white; }
.btn-danger:hover { filter:brightness(1.1); }
.btn-secondary { background:#374151; color:var(--text); }
.btn-secondary:hover { background:#4b5563; }
.btn:disabled { opacity:0.5; cursor:not-allowed; }
.btn-group { display:flex; gap:8px; flex-wrap:wrap; margin-top:16px; }
.stats { display:grid; grid-template-columns:repeat(4,1fr); gap:12px; margin-bottom:24px; }
.stat { background:var(--card); border:1px solid var(--border); border-radius:10px; padding:16px; text-align:center; }
.stat-value { font-size:1.5rem; font-weight:700; color:var(--primary); }
.stat-label { font-size:0.7rem; color:var(--muted); text-transform:uppercase; letter-spacing:.05em; margin-top:4px; }
.toast { position:fixed; bottom:20px; right:20px; padding:12px 20px; border-radius:8px; background:#1f2937; color:white; box-shadow:0 10px 25px rgba(0,0,0,.3); z-index:1000; display:none; }
.toast.show { display:block; animation:slideIn .3s; }
@keyframes slideIn { from { opacity:0; transform:translateY(20px); } to { opacity:1; transform:translateY(0); } }
.loading { text-align:center; color:var(--muted); padding:40px 20px; font-size:0.9rem; }
.loading:after { content:"..."; animation:dots 1.5s infinite; }
@keyframes dots { 0%{content:"."} 33%{content:".."} 66%{content:"..."} }
.empty { text-align:center; color:var(--muted); padding:40px 20px; }
.btn-pairing { background:var(--warn); color:#000; animation:pulse 1.5s infinite; }
@keyframes pulse { 0%,100%{opacity:1} 50%{opacity:.6} }
.modal { position:fixed; inset:0; background:rgba(0,0,0,.7); display:none; align-items:center; justify-content:center; z-index:100; padding:16px; }
.modal.show { display:flex; }
.modal-content { background:var(--card); border:1px solid var(--border); border-radius:12px; padding:24px; width:100%; max-width:400px; max-height:90vh; overflow-y:auto; }
.modal h3 { margin-bottom:16px; }
.form-group { margin-bottom:16px; }
.form-group label { display:block; margin-bottom:6px; font-size:0.85rem; }
.form-group input { width:100%; padding:10px 12px; border:1px solid var(--border); border-radius:8px; background:#0b0f1a; color:var(--text); font-size:16px; }
.row { display:flex; justify-content:space-between; align-items:center; padding:8px 0; border-bottom:1px solid var(--border); font-size:0.85rem; }
.row:last-child { border-bottom:none; }
@media (max-width:700px) {
    .header .btn-group { width:100%; }
    .header .btn-group .btn { flex:1; text-align:center; }
    .toast { left:10px; right:10px; bottom:10px; text-align:center; }
}
@media (max-width:600px) {
    .container { padding:12px; }
    h1 { font-size:1.2rem; }
    .stats { grid-template-columns:repeat(2,1fr); gap:8px; }
    .stat { padding:12px; }
    .stat-value { font-size:1.2rem; }
    .sensor-meta { grid-template-columns:1fr; }
    .sensor { padding:12px; }
    .card { padding:14px; }
    .modal-content { padding:16px; }
}
@media (max-width:400px) {
    .stats { grid-template-columns:1fr; }
    .header { flex-direction:column; align-items:stretch; }
    .header .btn-group .btn { font-size:0.8rem; padding:10px 8px; }
}
</style>
</head>
<body>
<div class="container">
    <div class="header">
        <h1>ESP-NOW Gateway</h1>
        <div class="btn-group">
            <button class="btn btn-primary" id="btn-pair" onclick="enterPairingMode()">+ Adicionar Sensor</button>
            <button class="btn btn-secondary" onclick="broadcastReregister()">Re-registrar</button>
            <button class="btn btn-secondary" onclick="loadData()" title="Atualizar">&#x21bb;</button>
        </div>
    </div>

    <div class="stats">
        <div class="stat"><div class="stat-value" id="stat-paired">--</div><div class="stat-label">Pareados</div></div>
        <div class="stat"><div class="stat-value" id="stat-online">--</div><div class="stat-label">Online</div></div>
        <div class="stat"><div class="stat-value" id="stat-rx">--</div><div class="stat-label">RX Total</div></div>
        <div class="stat"><div class="stat-value" id="stat-uptime">--</div><div class="stat-label">Uptime</div></div>
    </div>

    <div class="card">
        <h2>Sensores Virtuais</h2>
        <div id="sensors-grid" class="grid"><div class="loading">carregando</div></div>
    </div>

    <div class="card">
        <h2>Bridge</h2>
        <div id="bridge-status">
            <div class="row"><span class="label">Host</span><span class="value" id="bridge-host">--</span></div>
            <div class="row"><span class="label">Porta</span><span class="value" id="bridge-port">--</span></div>
            <div class="row"><span class="label">Status</span><span class="value" id="bridge-status-tag">--</span></div>
        </div>
        <div class="btn-group">
            <button class="btn btn-primary" onclick="showBridgeModal()">Configurar Bridge</button>
        </div>
    </div>
</div>

<div class="modal" id="bridge-modal">
    <div class="modal-content">
        <h3>Configurar Bridge</h3>
        <div class="form-group">
            <label>IP do Bridge</label>
            <input type="text" id="bridge-host-input" placeholder="Ex: 192.168.1.73" maxlength="64">
        </div>
        <div class="form-group">
            <label>Porta</label>
            <input type="number" id="bridge-port-input" placeholder="80" min="1" max="65535">
        </div>
        <div class="btn-group">
            <button class="btn btn-primary" onclick="saveBridgeConfig()">Salvar</button>
            <button class="btn btn-secondary" onclick="closeBridgeModal()">Cancelar</button>
        </div>
    </div>
</div>

<div class="modal" id="name-modal">
    <div class="modal-content">
        <h3>Nomear Sensor</h3>
        <div class="form-group">
            <label>Nome do sensor (slot <span id="name-slot">0</span>)</label>
            <input type="text" id="name-input" placeholder="Ex: Sala, Cozinha, Portao..." maxlength="32">
        </div>
        <div class="form-group">
            <label>Tipo</label>
            <input type="text" id="name-type" readonly>
        </div>
        <div class="btn-group">
            <button class="btn btn-primary" onclick="confirmName()">Salvar</button>
            <button class="btn btn-secondary" onclick="closeNameModal()">Cancelar</button>
        </div></div>
</div>

<div class="toast" id="toast"></div>

<script>
let pairingTimer = null;
let namingSlot = -1;
let s_pairingWindowSec = 180;
let s_loadFailCount = 0;
let s_loading = false;
let s_pollTimer = null;

async function api(path, opts={}) {
    const controller = new AbortController();
    const t = setTimeout(() => controller.abort(), 15000);
    try {
        const res = await fetch(path, {headers:{'Content-Type':'application/json'}, signal:controller.signal, ...opts});
        if (!res.ok) {
            let msg = 'HTTP ' + res.status;
            try { const j = await res.json(); if (j.error) msg = j.error; } catch(_) {}
            throw new Error(msg);
        }
        return res.json();
    } finally {
        clearTimeout(t);
    }
}

function showToast(msg, err=false) {
    const t = document.getElementById('toast');
    t.textContent = msg;
    t.style.background = err ? '#dc2626' : '#16a34a';
    t.classList.add('show');
    setTimeout(() => t.classList.remove('show'), 4000);
}

function fmtUptime(ms) {
    const s = Math.floor(ms/1000);
    if (s < 60) return s+'s';
    if (s < 3600) return Math.floor(s/60)+'m '+s%60+'s';
    if (s < 86400) return Math.floor(s/3600)+'h '+Math.floor((s%3600)/60)+'m';
    return Math.floor(s/86400)+'d '+Math.floor((s%86400)/3600)+'h';
}

function typeName(type) {
    const names = {1:'Temp+Hum', 2:'Contato', 3:'Movimento', 4:'Gas', 5:'Chuva', 6:'Tanque', 7:'DHT+Gas'};
    return names[type] || 'Desconhecido';
}

function renderSensors(sensors) {
    const grid = document.getElementById('sensors-grid');
    if (!sensors.length) {
        grid.innerHTML = '<div class="empty">Nenhum sensor pareado. Clique em "Adicionar Sensor".</div>';
        return;
    }
    grid.innerHTML = sensors.map(s => `
        <div class="sensor">
            <div class="sensor-header">
                <div>
                    <div class="sensor-name">${s.name || 'Sem nome'}</div>
                    <span class="sensor-type">${typeName(s.type)} &bull; Slot ${s.slot}</span>
                </div>
                <span class="badge ${s.online ? 'badge-online' : 'badge-offline'}">${s.online ? 'Online' : 'Offline'}</span>
            </div>
            <div class="sensor-meta">
                <div><span class="label">MAC</span><span class="value">${s.mac_str}</span></div>
                <div><span class="label">Bateria</span><span class="value state-battery">${s.battery_pct}%</span></div>
                <div><span class="label">RSSI</span><span class="value">${s.last_rssi} dBm</span></div>
                <div><span class="label">Ultimo</span><span class="value">${s.last_seen >= 0 ? fmtUptime(s.last_seen) : '&mdash;'}</span></div>
                <div><span class="label">Seq</span><span class="value">${s.sequence}</span></div>
                <div><span class="label">Bridge ID</span><span class="value" style="font-size:0.65rem">${s.bridge_device_id || '&mdash;'}</span></div>
                ${s.ip ? `<div><span class="label">IP</span><span class="value"><a href="http://${s.ip}">${s.ip}</a></span></div>` : ''}
            </div>
            <div class="sensor-state" id="state-${s.slot}">${renderState(s)}</div>
            <div class="btn-group">
                <button class="btn btn-secondary" onclick="renameSensor(${s.slot})">Renomear</button>
                <button class="btn btn-danger" onclick="removeSensor(${s.slot})">Remover</button>
            </div>
        </div>
    `).join('');
}

function renderState(s) {
    const st = s.state || {};
    let html = '';
    if (s.type === 1) {
        html += `<span class="state-item state-temp">${(st.temperature||0).toFixed(1)}&deg;C</span>`;
        html += `<span class="state-item state-hum">${(st.humidity||0).toFixed(0)}%</span>`;
    } else if (s.type === 2) {
        html += `<span class="state-item state-contact">${st.contact ? 'ABERTO' : 'FECHADO'}</span>`;
        if (st.tamper) html += `<span class="state-item state-contact">VIOLACAO</span>`;
    } else if (s.type === 3) {
        html += `<span class="state-item state-motion">${st.occupancy ? 'MOVIMENTO' : 'LIVRE'}</span>`;
    } else if (s.type === 4) {
        html += `<span class="state-item state-gas">${st.gas_level||0} ppm</span>`;
        if (st.alarm) html += `<span class="state-item state-gas">ALARME</span>`;
    } else if (s.type === 5) {
        html += `<span class="state-item state-rain">${st.rain_level||0}%</span>`;
        html += `<span class="state-item state-rain">${st.rain_digital ? 'Chuva' : 'Seco'}</span>`;
    } else if (s.type === 6) {
        html += `<span class="state-item state-tank">${st.level_pct||0}%</span>`;
        html += `<span class="state-item state-tank">${st.distance_cm||0} cm</span>`;
    } else if (s.type === 7) {
        html += `<span class="state-item state-temp">${(st.temperature||0).toFixed(1)}&deg;C</span>`;
        html += `<span class="state-item state-hum">${(st.humidity||0).toFixed(0)}%</span>`;
        html += `<span class="state-item state-gas">${st.gas_level||0}%</span>`;
        if (st.alarm) html += `<span class="state-item state-gas">ALARME</span>`;
    }
    return html || '<span class="state-item" style="color:var(--muted)">Aguardando dados...</span>';
}

async function loadData() {
    if (s_loading) return;
    s_loading = true;
    const grid = document.getElementById('sensors-grid');
    const wasEmpty = grid.querySelector('.loading, .empty');
    try {
        const [info, sensors] = await Promise.all([api('/api/info'), api('/api/sensors')]);
        s_loadFailCount = 0;
        document.getElementById('stat-paired').textContent = info.paired_count;
        document.getElementById('stat-online').textContent = info.online_count;
        document.getElementById('stat-rx').textContent = info.rx_total;
        document.getElementById('stat-uptime').textContent = fmtUptime(info.uptime_ms);
        if (info.pairing_window_sec) s_pairingWindowSec = info.pairing_window_sec;
        renderSensors(sensors.map(s => (delete s.mac_bytes, s.mac_str = s.mac, s)));
        document.getElementById('bridge-host').textContent = info.bridge_host || '--';
        document.getElementById('bridge-port').textContent = info.bridge_port || '--';
        document.getElementById('bridge-status-tag').innerHTML = info.bridge_discovered
            ? '<span class="badge badge-online">ok</span>'
            : '<span class="badge badge-offline">desconectado</span>';
    } catch (e) {
        s_loadFailCount++;
        if (wasEmpty) grid.innerHTML = '<div class="empty" style="color:var(--danger)">Falha ao carregar dados</div>';
        if (s_loadFailCount % 3 === 0) showToast('Erro ao carregar: '+e.message, true);
    } finally {
        s_loading = false;
    }
}

function schedulePoll() {
    if (s_pollTimer) clearTimeout(s_pollTimer);
    s_pollTimer = setTimeout(async () => {
        await loadData();
        schedulePoll();
    }, 10000);
}

function showBridgeModal() {
    document.getElementById('bridge-host-input').value = document.getElementById('bridge-host').textContent === '--' ? '' : document.getElementById('bridge-host').textContent;
    document.getElementById('bridge-port-input').value = document.getElementById('bridge-port').textContent === '--' ? '80' : document.getElementById('bridge-port').textContent;
    document.getElementById('bridge-modal').classList.add('show');
    document.getElementById('bridge-host-input').focus();
}

function closeBridgeModal() {
    document.getElementById('bridge-modal').classList.remove('show');
}

async function saveBridgeConfig() {
    const host = document.getElementById('bridge-host-input').value.trim();
    const port = parseInt(document.getElementById('bridge-port-input').value) || 80;
    if (!host) { showToast('IP do bridge obrigatorio', true); return; }
    try {
        await api('/api/config/bridge', {method:'POST', body:JSON.stringify({host, port})});
        showToast('Bridge configurado: ' + host + ':' + port);
        closeBridgeModal();
        loadData();
    } catch (e) { showToast('Erro: '+e.message, true); }
}

function startPairingCountdown() {
    const btn = document.getElementById('btn-pair');
    btn.classList.add('btn-pairing');
    let remaining = s_pairingWindowSec;
    btn.textContent = 'Cancelar (' + remaining + 's)';
    pairingTimer = setInterval(() => {
        remaining--;
        if (remaining > 0) {
            btn.textContent = 'Cancelar (' + remaining + 's)';
        } else {
            exitPairingMode();
        }
    }, 1000);
}

async function enterPairingMode() {
    if (pairingTimer) { exitPairingMode(); return; }
    try {
        await api('/api/pair/start', {method:'POST'});
        startPairingCountdown();
    } catch (e) {
        if (e.message.includes('already pairing')) {
            startPairingCountdown();
        } else {
            showToast('Erro: '+e.message, true);
        }
    }
}

async function exitPairingMode() {
    if (pairingTimer) clearInterval(pairingTimer);
    pairingTimer = null;
    try { await api('/api/pair/stop', {method:'POST'}); } catch(e) {}
    const btn = document.getElementById('btn-pair');
    btn.classList.remove('btn-pairing');
    btn.textContent = '+ Adicionar Sensor';
}

async function broadcastReregister() {
    try {
        await api('/api/broadcast', {method:'POST'});
        showToast('Re-registro enviado para Bridge');
    } catch (e) { showToast('Erro: '+e.message, true); }
}

async function renameSensor(slot) {
    try {
        const sensors = await api('/api/sensors');
        const s = sensors.find(x => x.slot === slot);
        if (!s) return;
        namingSlot = slot;
        document.getElementById('name-slot').textContent = slot;
        document.getElementById('name-input').value = s.name || '';
        document.getElementById('name-type').value = typeName(s.type);
        document.getElementById('name-modal').classList.add('show');
        document.getElementById('name-input').focus();
    } catch (e) { showToast('Erro: '+e.message, true); }
}

async function confirmName() {
    const name = document.getElementById('name-input').value.trim();
    if (!name) { showToast('Nome obrigatorio', true); return; }
    try {
        await api('/api/sensor/'+namingSlot+'/name', {method:'POST', body:JSON.stringify({name})});
        showToast('Nome atualizado');
        closeNameModal();
        loadData();
    } catch (e) { showToast('Erro: '+e.message, true); }
}

function closeNameModal() {
    document.getElementById('name-modal').classList.remove('show');
    namingSlot = -1;
}

async function removeSensor(slot) {
    if (!confirm('Remover sensor do slot ' + slot + '?')) return;
    try {
        await api('/api/sensor/'+slot+'/remove', {method:'POST'});
        showToast('Sensor removido');
        loadData();
    } catch (e) { showToast('Erro: '+e.message, true); }
}

document.addEventListener('keydown', e => {
    if (e.key === 'Escape') { closeNameModal(); closeBridgeModal(); }
});

document.getElementById('name-modal').addEventListener('click', e => { if(e.target.id==='name-modal') closeNameModal(); });
document.getElementById('bridge-modal').addEventListener('click', e => { if(e.target.id==='bridge-modal') closeBridgeModal(); });

loadData();
schedulePoll();
</script>
</body>
</html>
)rawliteral";

const char PAGE_DOCS[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="pt-BR">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>API Docs - ESP-NOW Gateway</title>
<style>
:root{--bg:#0b0f1a;--card:#111827;--border:#1f2937;--text:#e5e7eb;--muted:#9ca3af;--primary:#22d3ee;--get:#22c55e;--post:#60a5fa;--del:#ef4444}
*{box-sizing:border-box;margin:0;padding:0}
body{font-family:-apple-system,BlinkMacSystemFont,Segoe UI,Roboto,sans-serif;background:var(--bg);color:var(--text);padding:16px}
h1{font-size:1.3rem;margin-bottom:8px}
.sub{color:var(--muted);font-size:0.85rem;margin-bottom:24px}
h2{font-size:1rem;margin:20px 0 12px;color:var(--primary)}
.endpoint{background:var(--card);border:1px solid var(--border);border-radius:10px;padding:14px;margin-bottom:10px}
.endpoint .head{display:flex;align-items:center;gap:10px;margin-bottom:8px;flex-wrap:wrap}
.method{display:inline-block;padding:2px 8px;border-radius:6px;font-size:0.7rem;font-weight:700;text-transform:uppercase;letter-spacing:.05em;color:#fff;min-width:44px;text-align:center}
.method.get{background:var(--get)}
.method.post{background:var(--post)}
.method.del{background:var(--del)}
.path{font-family:Menlo,monospace;font-size:0.85rem;font-weight:600;word-break:break-all}
.path span{color:var(--muted);font-weight:400}
.desc{color:var(--muted);font-size:0.82rem;line-height:1.4}
.desc code{background:#0b0f1a;padding:1px 5px;border-radius:4px;font-size:0.78rem}
.params{margin-top:8px;padding-top:8px;border-top:1px solid var(--border);font-size:0.78rem;color:var(--muted)}
.params table{width:100%;border-collapse:collapse;margin-top:4px}
.params th,.params td{text-align:left;padding:4px 6px;font-size:0.75rem;border-bottom:1px solid var(--border)}
.params th{color:var(--muted);font-weight:500}
.footer{text-align:center;color:var(--muted);font-size:0.75rem;margin-top:32px;padding-top:16px;border-top:1px solid var(--border)}
@media(max-width:500px){
  h1{font-size:1.1rem}
  .endpoint{padding:10px}
  .path{font-size:0.78rem}
}
</style>
</head>
<body>
<h1>API Reference</h1>
<p class="sub">ESP-NOW Gateway &mdash; endpoints REST</p>

<h2>Gateway</h2>
<div class="endpoint">
  <div class="head"><span class="method get">GET</span><span class="path">/api/info</span></div>
  <div class="desc">Status do gateway: sensores pareados/online, RX total, uptime, modo de pareamento, versao FW, configuracao do bridge</div>
</div>
<div class="endpoint">
  <div class="head"><span class="method get">GET</span><span class="path">/api/sensors</span></div>
  <div class="desc">Lista todos os sensores pareados com estado atual, MAC, bateria, RSSI, IP</div>
</div>
<div class="endpoint">
  <div class="head"><span class="method get">GET</span><span class="path">/</span></div>
  <div class="desc">Dashboard web</div>
</div>
<div class="endpoint">
  <div class="head"><span class="method get">GET</span><span class="path">/docs</span></div>
  <div class="desc">Esta pagina de documentacao da API</div>
</div>

<h2>Pareamento</h2>
<div class="endpoint">
  <div class="head"><span class="method post">POST</span><span class="path">/api/pair/start</span></div>
  <div class="desc">Inicia modo de pareamento por <code>PAIRING_WINDOW_MS</code>. Retorna <code>200</code> ok, <code>409</code> ja pareando, <code>400</code> max sensores atingido</div>
</div>
<div class="endpoint">
  <div class="head"><span class="method post">POST</span><span class="path">/api/pair/stop</span></div>
  <div class="desc">Interrompe modo de pareamento ativo</div>
</div>

<h2>Sensores</h2>
<div class="endpoint">
  <div class="head"><span class="method post">POST</span><span class="path">/api/sensor/<span>{slot}</span>/name</span></div>
  <div class="desc">Renomeia um sensor</div>
  <div class="params">
    <table><tr><th>Param</th><th>Tipo</th><th>Descricao</th></tr>
    <tr><td><code>name</code></td><td>string</td><td>Novo nome (max 32 char)</td></tr></table>
    <div><em>Path:</em> <code>{slot}</code> = numero do slot do sensor</div>
  </div>
</div>
<div class="endpoint">
  <div class="head"><span class="method post">POST</span><span class="path">/api/sensor/<span>{slot}</span>/remove</span></div>
  <div class="desc">Remove um sensor pareado</div>
  <div class="params"><div><em>Path:</em> <code>{slot}</code> = numero do slot do sensor</div></div>
</div>
<div class="endpoint">
  <div class="head"><span class="method post">POST</span><span class="path">/api/clear</span></div>
  <div class="desc">Remove <strong>todos</strong> os sensores pareados</div>
</div>

<h2>Bridge</h2>
<div class="endpoint">
  <div class="head"><span class="method post">POST</span><span class="path">/api/broadcast</span></div>
  <div class="desc">Re-registra todos os sensores no Bridge via HTTP</div>
</div>
<div class="endpoint">
  <div class="head"><span class="method post">POST</span><span class="path">/api/config/bridge</span></div>
  <div class="desc">Configura host/porta do Bridge manualmente</div>
  <div class="params">
    <table><tr><th>Param</th><th>Tipo</th><th>Descricao</th></tr>
    <tr><td><code>host</code></td><td>string</td><td>IP ou hostname do Bridge</td></tr>
    <tr><td><code>port</code></td><td>number</td><td>Porta HTTP do Bridge</td></tr></table>
  </div>
</div>

<h2>Manutencao</h2>
<div class="endpoint">
  <div class="head"><span class="method post">POST</span><span class="path">/api/restart</span></div>
  <div class="desc">Reinicia o gateway</div>
</div>
<div class="endpoint">
  <div class="head"><span class="method post">POST</span><span class="path">/api/ota</span></div>
  <div class="desc">Upload de firmware via OTA (multipart/form-data)</div>
</div>

<p class="footer">ESP-NOW Gateway &mdash; <span id="v">v0.0.15</span></p>
</body>
</html>
)rawliteral";

#endif