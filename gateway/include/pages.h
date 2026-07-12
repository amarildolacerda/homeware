#ifndef PAGES_H
#define PAGES_H

const char PAGE_PORTAL[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="pt-BR">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width,initial-scale=1,maximum-scale=1">
<title>Configurar Gateway</title>
<style>
:root{--bg:#f4f4f4;--surface:#fff;--surface-2:#f9fafb;--text:#1f2937;--muted:#6b7280;--muted-subtle:#9ca3af;--primary:#5e6ad2;--border:#e5e7eb;--border-strong:#d1d5db;--danger:#dc2626;--success:#16a34a}
*{box-sizing:border-box;margin:0;padding:0}
body{font-family:-apple-system,BlinkMacSystemFont,Segoe UI,Roboto,sans-serif;background:var(--bg);color:var(--text);min-height:100vh;display:flex;align-items:center;justify-content:center;padding:16px}
.card{background:var(--surface);border:1px solid var(--border);border-radius:12px;padding:24px;width:100%;max-width:420px}
h1{font-size:1.1rem;font-weight:700;color:var(--primary);margin-bottom:4px}
.sub{color:var(--muted);font-size:0.8rem;margin-bottom:20px}
.form-group{margin-bottom:14px}
.form-group label{display:block;margin-bottom:4px;font-size:0.82rem;color:var(--muted)}
input,select{width:100%;padding:10px 12px;border:1px solid var(--border);border-radius:8px;font-size:16px;background:var(--surface-2);color:var(--text)}
.toggle-row{display:flex;gap:8px;margin:10px 0}
.seg{flex:1;padding:10px;border:1px solid var(--border);background:var(--surface-2);color:var(--muted);border-radius:8px;font-weight:600;cursor:pointer;font-size:0.85rem}
.seg.active{background:var(--primary);color:#fff;border-color:var(--primary)}
.hidden{display:none!important}
.btn{padding:12px;width:100%;border:none;border-radius:8px;font-weight:600;cursor:pointer;font-size:0.9rem;background:var(--primary);color:#fff}
.section{margin-top:18px;padding-top:14px;border-top:1px solid var(--border)}
.section h2{font-size:0.9rem;color:var(--primary);margin-bottom:12px}
</style>
</head>
<body>
<div class="card">
<h1>Configurar Gateway</h1>
<div class="sub">Conecte o gateway à sua rede Wi-Fi e ao broker MQTT</div>
<div class="form-group">
<label>SSID</label>
<input list="ssid-list" id="ssid" placeholder="Nome da rede" maxlength="32">
<datalist id="ssid-list"></datalist>
</div>
<div class="form-group">
<label>Senha</label>
<input type="password" id="pass" placeholder="Senha do Wi-Fi" maxlength="64">
</div>
<div class="toggle-row">
<button type="button" class="seg active" id="seg-dhcp" onclick="setMode(0)">DHCP</button>
<button type="button" class="seg" id="seg-static" onclick="setMode(1)">IP Fixo</button>
</div>
<div id="static-fields" class="hidden">
<div class="form-group"><label>IP Fixo</label><input id="ip" placeholder="192.168.1.50"></div>
<div class="form-group"><label>Gateway</label><input id="gw" placeholder="192.168.1.1"></div>
<div class="form-group"><label>Máscara</label><input id="mask" placeholder="255.255.255.0"></div>
<div class="form-group"><label>DNS</label><input id="dns" placeholder="192.168.1.1"></div>
</div>
<div class="section">
<h2>MQTT</h2>
<div class="form-group"><label>Broker IP</label><input id="mqtt_host" placeholder="192.168.1.12"></div>
<div class="form-group"><label>Porta</label><input id="mqtt_port" placeholder="1883"></div>
<div class="form-group"><label>Usuário</label><input id="mqtt_user" placeholder="(opcional)"></div>
<div class="form-group"><label>Senha</label><input type="password" id="mqtt_pass" placeholder="(opcional)"></div>
</div>
<button class="btn" id="save" onclick="save()">Salvar e Reiniciar</button>
</div>
<script>
var mode = 0;
function setMode(m){mode=m;document.getElementById('seg-dhcp').classList.toggle('active',m===0);document.getElementById('seg-static').classList.toggle('active',m===1);document.getElementById('static-fields').classList.toggle('hidden',m!==1);}
function save(){
  var ssid=document.getElementById('ssid').value.trim();
  if(!ssid){alert('SSID obrigatório');return;}
  if(mode===1 && (!document.getElementById('ip').value.trim() || !document.getElementById('gw').value.trim())){alert('IP Fixo e Gateway obrigatórios');return;}
  var body={ssid:ssid,pass:document.getElementById('pass').value,mode:mode,
    ip:document.getElementById('ip').value.trim(),gateway:document.getElementById('gw').value.trim(),
    subnet:document.getElementById('mask').value.trim(),dns:document.getElementById('dns').value.trim(),
    mqtt_host:document.getElementById('mqtt_host').value.trim(),
    mqtt_port:parseInt(document.getElementById('mqtt_port').value)||1883,
    mqtt_user:document.getElementById('mqtt_user').value.trim(),
    mqtt_pass:document.getElementById('mqtt_pass').value};
  fetch('/api/portal/setup',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify(body)})
    .then(function(r){if(!r.ok)throw new Error('erro');return r.json();})
    .then(function(){document.getElementById('save').textContent='Salvo! Reiniciando...';})
    .catch(function(){alert('Falha ao salvar');});
}
</script>
</body>
</html>
)rawliteral";

const char PAGE_SHELL[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="pt-BR">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width,initial-scale=1,maximum-scale=1">
<title>ESP-NOW Gateway</title>
<style>
:root{--bg:#f4f4f4;--surface:#fff;--surface-2:#f9fafb;--text:#1f2937;--muted:#6b7280;--muted-subtle:#9ca3af;--primary:#5e6ad2;--primary-strong:#828fff;--primary-focus:#eef0ff;--border:#e5e7eb;--border-strong:#d1d5db;--success:#16a34a;--danger:#dc2626;--warn:#f59e0b;--info:#2563eb}
*{box-sizing:border-box;margin:0;padding:0}
body{font-family:-apple-system,BlinkMacSystemFont,Segoe UI,Roboto,sans-serif;background:var(--bg);color:var(--text);min-height:100vh;display:flex}
.sidebar{width:200px;background:var(--surface);border-right:1px solid var(--border);padding:24px 0;display:flex;flex-direction:column;position:fixed;top:0;left:0;height:100vh;z-index:10}
.sidebar .logo{padding:0 20px 20px;border-bottom:1px solid var(--border);margin-bottom:8px}
.sidebar .logo h1{font-size:1rem;font-weight:700;color:var(--primary)}
.sidebar .logo span{font-size:0.75rem;color:var(--muted)}
.sidebar nav{flex:1}
.sidebar nav a{display:flex;align-items:center;gap:10px;padding:12px 20px;color:var(--muted);text-decoration:none;font-size:0.85rem;font-weight:500;transition:all .15s;border-left:3px solid transparent}
.sidebar nav a:hover{color:var(--text);background:var(--primary-focus)}
.sidebar nav a.active{color:var(--primary);background:var(--primary-focus);border-left-color:var(--primary);font-weight:600}
.sidebar nav a .icon{width:20px;text-align:center;font-size:1.1rem}
.sidebar .footer-nav{padding:12px 20px;border-top:1px solid var(--border);font-size:0.7rem;color:var(--muted-subtle)}
.main{margin-left:200px;flex:1;padding:24px;max-width:960px}
#page{min-height:calc(100vh - 80px)}
.mqtt-footer{position:fixed;bottom:0;right:0;left:200px;background:var(--surface);border-top:1px solid var(--border);padding:8px 24px;display:flex;align-items:center;justify-content:flex-end;gap:8px;font-size:0.78rem;z-index:10}
.mqtt-footer .dot{width:8px;height:8px;border-radius:50%;display:inline-block}
.mqtt-footer .dot.on{background:var(--success)}
.mqtt-footer .dot.off{background:var(--danger)}
.toast{position:fixed;bottom:60px;right:20px;padding:10px 16px;border-radius:8px;background:#1f2937;color:#fff;z-index:100;display:none;font-size:0.85rem}
.toast.show{display:block;animation:slideIn .3s}
@keyframes slideIn{from{opacity:0;transform:translateY(10px)}to{opacity:1;transform:translateY(0)}}
@media(max-width:700px){
.sidebar{width:60px}
.sidebar .logo h1,.sidebar .logo span,.sidebar nav a span:last-child,.sidebar .footer-nav{display:none}
.sidebar nav a{justify-content:center;padding:14px;border-left:none}
.sidebar nav a.active{border-left:none;background:var(--primary-focus)}
.main{margin-left:60px;padding:16px}
.mqtt-footer{left:60px}
}
</style>
</head>
<body>
<div class="sidebar">
<div class="logo"><h1>ESP-NOW</h1><span>Gateway</span></div>
<nav>
<a href="#" onclick="navigate('overview');return false" class="active" id="nav-overview"><span class="icon">&#x1F3E0;</span><span>Visão Geral</span></a>
<a href="#" onclick="navigate('settings');return false" id="nav-settings"><span class="icon">&#x2699;</span><span>Configurações</span></a>
<a href="#" onclick="navigate('logs');return false" id="nav-logs"><span class="icon">&#x1F4CB;</span><span>Logs</span></a>
</nav>
<div class="footer-nav" id="fw-sidebar">v--</div>
</div>
<div class="main"><div id="page"><div class="loading" style="text-align:center;padding:60px 20px;color:var(--muted)">carregando...</div></div></div>
<div class="mqtt-footer" id="mqtt-footer"><span class="dot off" id="mqtt-dot"></span><span id="mqtt-footer-text">MQTT: desconectado</span></div>
<div class="toast" id="toast"></div>
<script>
let s_pollTimer = null;
let s_mqtt_connected = false;

function navigate(page) {
  if (s_pollTimer) { clearInterval(s_pollTimer); s_pollTimer = null; }
  document.querySelectorAll('.sidebar nav a').forEach(function(a) { a.classList.remove('active'); });
  var link = document.getElementById('nav-'+page);
  if (link) link.classList.add('active');
  var el = document.getElementById('page');
  el.innerHTML = '<div class="loading" style="text-align:center;padding:60px 20px;color:var(--muted)">carregando...</div>';
  fetch('/'+page).then(function(r) { return r.text(); }).then(function(html) {
    el.innerHTML = html;
    el.querySelectorAll('script').forEach(function(s) {
      var ns = document.createElement('script');
      ns.textContent = s.textContent;
      s.parentNode.replaceChild(ns, s);
    });
  }).catch(function() {
    el.innerHTML = '<div style="text-align:center;padding:60px 20px;color:var(--danger)">Erro ao carregar</div>';
  });
}

function updateMqttFooter(connected, host, port) {
  var dot = document.getElementById('mqtt-dot');
  var txt = document.getElementById('mqtt-footer-text');
  dot.className = 'dot ' + (connected ? 'on' : 'off');
  txt.textContent = connected ? 'MQTT: Conectado ao ' + (host||'broker') : 'MQTT: desconectado';
  s_mqtt_connected = connected;
}

function showToast(msg, err) {
  var t = document.getElementById('toast');
  if (!t) return;
  t.textContent = msg;
  t.style.background = err ? '#dc2626' : '#16a34a';
  t.classList.add('show');
  setTimeout(function() { t.classList.remove('show'); }, 4000);
}

async function api(path, opts) {
  opts = opts || {};
  var controller = new AbortController();
  var t = setTimeout(function() { controller.abort(); }, 15000);
  try {
    var res = await fetch(path, {headers:{'Content-Type':'application/json'}, signal:controller.signal, method:opts.method, body:opts.body});
    if (!res.ok) {
      var msg = 'HTTP ' + res.status;
      try { var j = await res.json(); if (j.error) msg = j.error; } catch(_) {}
      throw new Error(msg);
    }
    return res.json();
  } finally {
    clearTimeout(t);
  }
}

function fmtBytes(b) {
  if (b === undefined || b === null) return '--';
  if (b < 1024) return b+' B';
  if (b < 1048576) return (b/1024).toFixed(0)+' KB';
  return (b/1048576).toFixed(1)+' MB';
}

function fmtUptime(ms) {
  var s = Math.floor(ms/1000);
  if (s < 60) return s+'s';
  if (s < 3600) return Math.floor(s/60)+'m '+s%60+'s';
  if (s < 86400) return Math.floor(s/3600)+'h '+Math.floor((s%3600)/60)+'m';
  return Math.floor(s/86400)+'d '+Math.floor((s%86400)/3600)+'h';
}

function fmtHeap(b) {
  if (!b) return '-';
  return b > 1024 ? (b/1024).toFixed(1)+'KB' : b+'B';
}

function escHtml(s) {
  return String(s).replace(/&/g,'&amp;').replace(/</g,'&lt;').replace(/>/g,'&gt;').replace(/"/g,'&quot;');
}

document.addEventListener('keydown', function(e) {
  if (e.key === 'Escape') {
    document.querySelectorAll('.modal.show').forEach(function(m) { m.classList.remove('show'); });
  }
});

navigate('overview');
</script>
</body>
</html>
)rawliteral";

const char PAGE_OVERVIEW[] PROGMEM = R"rawliteral(
<style>
.stats{display:grid;grid-template-columns:repeat(5,1fr);gap:8px;margin-bottom:16px}
.stat{background:var(--surface);border:1px solid var(--border);border-radius:8px;padding:10px 6px;text-align:center}
.stat-value{font-size:1.2rem;font-weight:700;color:var(--primary)}
.stat-label{font-size:0.6rem;color:var(--muted-subtle);text-transform:uppercase;letter-spacing:.03em;margin-top:2px}
.filters{display:flex;gap:6px;flex-wrap:wrap;margin-bottom:16px}
.filter-btn{padding:6px 14px;border:1px solid var(--border);border-radius:9999px;background:var(--surface);color:var(--muted);font-size:0.78rem;cursor:pointer;transition:all .15s;user-select:none}
.filter-btn:hover{border-color:var(--primary-strong);color:var(--primary)}
.filter-btn.active{background:var(--primary);color:#fff;border-color:var(--primary)}
.grid{display:grid;gap:12px}
.grid-2{grid-template-columns:repeat(auto-fill,minmax(280px,1fr))}
.device{background:var(--surface);border:1px solid var(--border);border-radius:10px;padding:14px;transition:border-color .15s}
.device:hover{border-color:var(--primary-strong)}
.device.offline{border-color:var(--danger);opacity:.7}
.stat.active .stat-value{color:var(--primary)}
.device-head{display:flex;justify-content:space-between;align-items:flex-start;margin-bottom:8px}
.device-icon{font-size:1.5rem;width:36px;text-align:center}
.device-name{font-weight:600;font-size:0.9rem}
.device-type{font-size:0.7rem;color:var(--muted-subtle)}
.badge{display:inline-block;padding:2px 8px;border-radius:9999px;font-size:0.7rem;font-weight:600}
.badge.online{background:#dcfce7;color:#16a34a}
.badge.offline{background:#fef2f2;color:#dc2626}
.badge.warn{background:#fef3c7;color:#d97706}
.badge.danger{background:#fef2f2;color:#dc2626}
.metrics{display:grid;grid-template-columns:1fr 1fr;gap:8px;margin:10px 0}
.metric-bar{display:flex;align-items:center;gap:6px;font-size:0.78rem}
.metric-bar .bar{flex:1;height:6px;background:var(--border);border-radius:3px;overflow:hidden}
.metric-bar .bar .fill{height:100%;border-radius:3px;transition:width .3s}
.metric-bar .bar .fill.green{background:var(--success)}
.metric-bar .bar .fill.yellow{background:var(--warn)}
.metric-bar .bar .fill.red{background:var(--danger)}
.device-actions{position:relative;display:inline-flex}
.device-actions .menu-trigger{background:none;border:none;color:var(--muted);cursor:pointer;font-size:1.2rem;padding:2px 6px;border-radius:6px;line-height:1}
.device-actions .menu-trigger:hover{background:var(--border);color:var(--text)}
.device-actions .menu-dropdown{display:none;position:absolute;bottom:100%;right:0;background:var(--surface);border:1px solid var(--border);border-radius:8px;min-width:140px;overflow:hidden;z-index:20;box-shadow:0 4px 12px rgba(0,0,0,.1)}
.device-actions .menu-dropdown.show{display:block}
.device-actions .menu-dropdown button{display:block;width:100%;padding:10px 16px;border:none;background:transparent;color:var(--text);font-size:0.82rem;text-align:left;cursor:pointer}
.device-actions .menu-dropdown button:hover{background:var(--primary-focus)}
.device-actions .menu-dropdown button.danger{color:var(--danger)}
.device-actions .menu-dropdown button.danger:hover{background:#fef2f2}
.collapse-btn{background:none;border:none;color:var(--muted);cursor:pointer;font-size:0.75rem;padding:4px 0}
.collapse-btn:hover{color:var(--primary)}
.state-group{display:flex;flex-wrap:wrap;gap:4px;margin:8px 0}
.state-item{background:var(--surface-2);padding:4px 10px;border-radius:6px;font-size:0.75rem}
.state-temp{color:var(--danger)}
.state-hum{color:var(--info)}
.state-contact{color:#7c3aed}
.state-motion{color:var(--warn)}
.state-gas{color:var(--danger)}
.state-rain{color:var(--info)}
.state-tank{color:var(--success)}
.btn-onoff{width:100%;padding:14px;font-size:1rem;font-weight:700;border:none;border-radius:8px;cursor:pointer;min-height:48px;margin:10px 0}
.device-toggle{display:inline-flex;align-items:center;gap:4px;padding:4px 10px;border:none;border-radius:6px;font-size:0.78rem;font-weight:600;cursor:pointer;vertical-align:middle;margin-left:6px;transition:all .2s}
.device-toggle.on{background:rgba(22,163,74,0.12);color:var(--success)}
.device-toggle.off{background:var(--border);color:var(--muted)}
@media(max-width:700px){.device-toggle{font-size:0.65rem;padding:2px 6px}}
.btn-onoff.on{background:var(--success);color:#fff}
.btn-onoff.off{background:var(--border);color:var(--text)}
.btn-onoff.off:hover{background:var(--border-strong)}
.loading{padding:40px;text-align:center;color:var(--muted)}
@keyframes slideIn{from{opacity:0;transform:translateY(10px)}to{opacity:1;transform:translateY(0)}}
@media(max-width:600px){.stats{grid-template-columns:repeat(3,1fr);gap:6px}}
@media(max-width:700px){
  .stats{grid-template-columns:repeat(5,1fr);gap:4px;margin-bottom:10px}
  .stat{padding:6px 2px}
  .stat-value{font-size:0.9rem}
  .stat-label{font-size:0.5rem}
  .filters{gap:4px;margin-bottom:10px}
  .filter-btn{padding:4px 10px;font-size:0.7rem}
  .device .metrics,.device .device-type{display:none}
  .device{padding:8px 10px}
  .device-head{margin-bottom:2px}
  .device-icon{font-size:1.1rem;width:24px}
  .device-name{font-size:0.8rem;display:flex;align-items:center;gap:4px;min-width:0}
  .device-name span{overflow:hidden;text-overflow:ellipsis;white-space:nowrap}
  .device-actions .menu-trigger{font-size:1.1rem;padding:2px 4px}
  .device-actions .menu-dropdown{min-width:120px}
  .state-group{margin:4px 0 0}
  .btn-onoff{padding:8px;font-size:0.85rem;margin:2px 0}
  .grid-2{grid-template-columns:repeat(auto-fill,minmax(160px,1fr));gap:8px}
  .badge{font-size:0.6rem;padding:1px 5px;min-width:auto}
}
.modal{position:fixed;inset:0;background:rgba(0,0,0,.5);display:none;align-items:center;justify-content:center;z-index:100}
.modal.show{display:flex}
.modal-content{background:var(--surface);border:1px solid var(--border);border-radius:12px;padding:24px;width:100%;max-width:400px}
.props-section{margin-bottom:14px}
.props-section-title{font-size:0.75rem;font-weight:600;color:var(--muted-subtle);text-transform:uppercase;letter-spacing:.05em;margin-bottom:6px}
.props-section .row{display:flex;justify-content:space-between;align-items:center;padding:5px 0;border-bottom:1px solid var(--border);font-size:0.82rem}
.props-section .row:last-child{border-bottom:none}
.props-section .row .label{color:var(--muted-subtle)}
.props-section .row .value{font-weight:600}
.btn{padding:10px 18px;border:none;border-radius:8px;font-weight:600;cursor:pointer;font-size:0.85rem;min-height:44px}
.btn-primary{background:var(--primary);color:#fff}
.btn-primary:hover{filter:brightness(1.1)}
.btn-secondary{background:var(--border);color:var(--text)}
.btn-secondary:hover{background:var(--border-strong)}
.btn-pairing{background:var(--warn);color:#000;animation:pulse 1.5s infinite}
@keyframes pulse{0%,100%{opacity:1}50%{opacity:.6}}
</style>
<div class="stats">
<div class="stat" data-status="all" onclick="filterStatus('all')" style="cursor:pointer"><div class="stat-value" id="stat-paired">--</div><div class="stat-label">Pareados</div></div>
<div class="stat" data-status="online" onclick="filterStatus('online')" style="cursor:pointer"><div class="stat-value" id="stat-online">--</div><div class="stat-label">Online</div></div>
<div class="stat" data-status="offline" onclick="filterStatus('offline')" style="cursor:pointer"><div class="stat-value" id="stat-offline">--</div><div class="stat-label">Offline</div></div>
<div class="stat"><div class="stat-value" id="stat-rx">--</div><div class="stat-label">RX Total</div></div>
<div class="stat"><div class="stat-value" id="stat-uptime">--</div><div class="stat-label">Uptime</div></div>
</div>
<div class="filters" id="filter-bar"></div>
<div id="sensors-grid" class="grid grid-2"><div class="loading">carregando...</div></div>
<div id="repeaterSection" style="display:none">
<h2 style="font-size:0.95rem;font-weight:600;color:var(--primary);margin:20px 0 12px">Repeaters</h2>
<div id="repeaterList"></div>
</div>

<div class="modal" id="rename-modal">
  <div class="modal-content" style="max-width:360px">
    <h3 style="margin-bottom:12px;font-size:0.95rem">Renomear Sensor</h3>
    <p style="font-size:0.8rem;color:var(--muted-subtle);margin-bottom:8px">Slot <span id="rename-slot"></span> &bull; <span id="rename-type"></span></p>
    <input type="text" id="rename-input" maxlength="32" style="width:100%;padding:10px;border:1px solid var(--border);border-radius:8px;font-size:16px;background:var(--surface-2);color:var(--text)">
    <div style="display:flex;gap:8px;margin-top:12px">
      <button class="btn btn-primary" onclick="confirmRename()" style="flex:1">Salvar</button>
      <button class="btn btn-secondary" onclick="closeRename()" style="flex:1">Cancelar</button>
    </div>
  </div>
</div>

<div class="modal" id="props-modal">
  <div class="modal-content" style="max-width:400px">
    <div style="display:flex;justify-content:space-between;align-items:center;margin-bottom:16px">
      <h3 style="font-size:0.95rem;font-weight:600" id="props-title">Propriedades</h3>
      <button onclick="closePropsModal()" style="background:none;border:none;font-size:1.2rem;cursor:pointer;color:var(--muted);padding:4px 8px">&times;</button>
    </div>
    <div id="props-body"></div>
  </div>
</div>

<script>
var s_overviewLoading = false;
var s_renameSlot = null;
var s_sensors = [];
var s_activeFilter = 'all';
var s_statusFilter = 'all';
var s_prevSensorMap = {};
var s_sensorCount = 0;
var s_info = {};

function typeName(type) {
  var names = {1:'Temp+Hum',2:'Contato',3:'Movimento',4:'Gas',5:'Chuva',6:'Tanque',7:'DHT+Gas',8:'Interruptor',9:'Lâmpada'};
  return names[type] || 'Desconhecido';
}

function typeIcon(type) {
  var icons = {1:'&#x1F321;',2:'&#x1F514;',3:'&#x1F3C3;',4:'&#x1F4A8;',5:'&#x2614;',6:'&#x1F4A7;',7:'&#x1F321;+&#x1F4A8;',8:'&#x1F50C;',9:'&#x1F4A1;'};
  return icons[type] || '&#x2753;';
}

function batteryBar(pct) {
  if (pct === undefined || pct === null) return '<div class="metric-bar"><span style="min-width:32px;font-size:0.7rem;color:var(--muted-subtle)">BAT</span><div class="bar"><div class="fill" style="width:0%"></div></div><span style="min-width:32px;text-align:right;font-size:0.7rem">--</span></div>';
  var color = pct > 50 ? 'green' : pct > 20 ? 'yellow' : 'red';
  var badge = pct < 20 ? ' <span class="badge danger" style="font-size:0.65rem">baixa</span>' : '';
  return '<div class="metric-bar"><span style="min-width:32px;font-size:0.7rem;color:var(--muted-subtle)">BAT</span><div class="bar"><div class="fill '+color+'" style="width:'+Math.min(100,pct)+'%"></div></div><span style="min-width:32px;text-align:right;font-size:0.7rem">'+pct+'%'+badge+'</span></div>';
}

function rssiBar(rssi) {
  if (rssi === undefined || rssi === null) return '<div class="metric-bar"><span style="min-width:32px;font-size:0.7rem;color:var(--muted-subtle)">RSSI</span><div class="bar"><div class="fill" style="width:0%"></div></div><span style="min-width:42px;text-align:right;font-size:0.7rem">--</span></div>';
  var pct = Math.min(100, Math.max(0, (rssi + 100) * 2));
  var color = rssi > -70 ? 'green' : rssi > -85 ? 'yellow' : 'red';
  return '<div class="metric-bar"><span style="min-width:32px;font-size:0.7rem;color:var(--muted-subtle)">RSSI</span><div class="bar"><div class="fill '+color+'" style="width:'+pct+'%"></div></div><span style="min-width:42px;text-align:right;font-size:0.7rem">'+rssi+' dBm</span></div>';
}

function renderState(s) {
  var st = s.state || {};
  var html = '';
  if (s.type === 1) {
    html += '<span class="state-item state-temp">'+(st.temperature||0).toFixed(1)+'&deg;C</span>';
    html += '<span class="state-item state-hum">'+(st.humidity||0).toFixed(0)+'%</span>';
  } else if (s.type === 2) {
    html += '<span class="state-item state-contact">'+(st.contact ? 'ABERTO' : 'FECHADO')+'</span>';
    if (st.tamper) html += '<span class="state-item state-contact">VIOLAÇÃO</span>';
  } else if (s.type === 3) {
    html += '<span class="state-item state-motion">'+(st.occupancy ? 'MOVIMENTO' : 'LIVRE')+'</span>';
  } else if (s.type === 4) {
    html += '<span class="state-item state-gas">'+(st.gas_level||0)+' ppm</span>';
    if (st.alarm) html += '<span class="state-item state-gas">ALARME</span>';
  } else if (s.type === 5) {
    html += '<span class="state-item state-rain">'+(st.rain_level||0)+'%</span>';
    html += '<span class="state-item state-rain">'+(st.rain_digital ? 'Chuva' : 'Seco')+'</span>';
  } else if (s.type === 6) {
    html += '<span class="state-item state-tank">'+(st.level_pct||0)+'%</span>';
    html += '<span class="state-item state-tank">'+(st.distance_cm||0)+' cm</span>';
  } else if (s.type === 7) {
    html += '<span class="state-item state-temp">'+(st.temperature||0).toFixed(1)+'&deg;C</span>';
    html += '<span class="state-item state-hum">'+(st.humidity||0).toFixed(0)+'%</span>';
    html += '<span class="state-item state-gas">'+(st.gas_level||0)+'%</span>';
    if (st.alarm) html += '<span class="state-item state-gas">ALARME</span>';
  } else if (s.type === 8 || s.type === 9) {
    var on = st.state ? true : false;
    html += '<button class="btn-onoff '+(on?'on':'off')+'" onclick="toggleSensor('+s.slot+','+(on?0:1)+')">'+(on?'DESLIGAR':'LIGAR')+'</button>';
  }
  return html || '<span class="state-item" style="color:var(--muted-subtle)">Aguardando dados...</span>';
}

function renderSensors(sensors) {
  var grid = document.getElementById('sensors-grid');
  var nonRepeater = sensors.filter(function(s) { return s.type_name !== 'repeater'; });
  if (!nonRepeater || !nonRepeater.length) {
    grid.innerHTML = '<div style="text-align:center;padding:40px;color:var(--muted-subtle)">Nenhum sensor pareado</div>';
    s_prevSensorMap = {};
    return;
  }
  grid.innerHTML = nonRepeater.map(function(s) { return buildSensorCard(s); }).join('');
  s_prevSensorMap = {};
  nonRepeater.forEach(function(s) { s_prevSensorMap[s.slot] = JSON.stringify(s); });
}

function buildSensorCard(s) {
  var off = !s.online;
  var offClass = off ? ' offline' : '';
  var isType9 = s.type === 9;
  var isType8 = s.type === 8 || isType9;
  var onState = s.state && s.state.state;
  return '<div class="device'+offClass+'" data-slot="'+s.slot+'" data-type="'+s.type+'">'+
    '<div class="device-head">'+
      '<div>'+
        '<div class="device-icon">'+typeIcon(s.type)+'</div>'+
        '<div class="device-name"><span>'+escHtml(s.name||'Sem nome')+'</span>'+
          (isType8 ? '<button class="device-toggle '+(onState?'on':'off')+'" onclick="event.stopPropagation();toggleSensor('+s.slot+','+(onState?0:1)+')">'+(onState?'ON':'OFF')+'</button>' : '')+
        '</div>'+
        '<div class="device-type">'+typeName(s.type)+' &bull; Slot '+s.slot+'</div>'+
      '</div>'+
      '<div style="display:flex;align-items:center;gap:4px">'+
        '<span class="badge '+(s.online?'online':'offline')+'">'+(s.online?'Online':'Offline')+'</span>'+
        '<div class="device-actions">'+
          '<button class="menu-trigger" onclick="event.stopPropagation();toggleDeviceMenu('+s.slot+')">&#x22ee;</button>'+
          '<div class="menu-dropdown" id="dmenu-'+s.slot+'">'+
            '<button onclick="showPropsModal('+s.slot+')">Propriedades</button>'+
            '<button onclick="renameSensor('+s.slot+')">Renomear</button>'+
            '<button class="danger" onclick="removeSensor('+s.slot+')">Remover</button>'+
          '</div>'+
        '</div>'+
      '</div>'+
    '</div>'+
    '<div class="metrics">'+
      batteryBar(s.battery_pct)+
      rssiBar(s.last_rssi)+
    '</div>'+
    '<div class="state-group">'+
      (isType8 ? '' : renderState(s))+
    '</div>'+
  '</div>';
}

function renderRepeaters(sensors) {
  var list = document.getElementById('repeaterList');
  var section = document.getElementById('repeaterSection');
  var repeaters = sensors.filter(function(s) { return s.type_name === 'repeater'; });
  if (!repeaters.length) { section.style.display = 'none'; return; }
  section.style.display = '';
  list.innerHTML = repeaters.map(function(r) {
    var st = r.state || {};
    var ago = r.last_seen >= 0 ? fmtUptime(r.last_seen) : 'nunca';
    return '<div class="device" style="margin-bottom:12px">'+
      '<div class="device-head">'+
        '<div>'+
          '<div class="device-icon">&#x1F504;</div>'+
          '<div class="device-name"><span>'+escHtml(r.name||'Repeater')+'</span></div>'+
          '<div class="device-type">Repeater &bull; Slot '+r.slot+'</div>'+
        '</div>'+
        '<span class="badge '+(r.online?'online':'offline')+'">'+(r.online?'Online':'Offline')+'</span>'+
      '</div>'+
      '<div class="metrics" style="display:grid;grid-template-columns:repeat(auto-fit,minmax(120px,1fr));gap:8px;margin-top:12px">'+
        '<div class="metric"><div class="metric-label">Recebidos</div><div class="metric-value">'+(st.received||0)+'</div></div>'+
        '<div class="metric"><div class="metric-label">Encaminhados</div><div class="metric-value">'+(st.forwarded||0)+'</div></div>'+
        '<div class="metric"><div class="metric-label">Clients</div><div class="metric-value">'+(st.client_count||0)+'</div></div>'+
        '<div class="metric"><div class="metric-label">Falhas ACK</div><div class="metric-value" style="color:'+(st.ack_failures>0?'var(--danger)':'var(--text)')+'">'+(st.ack_failures||0)+'</div></div>'+
        '<div class="metric"><div class="metric-label">Canal</div><div class="metric-value">'+(st.channel||'-')+'</div></div>'+
        '<div class="metric"><div class="metric-label">RSSI</div><div class="metric-value">'+r.last_rssi+' dBm</div></div>'+
        '<div class="metric"><div class="metric-label">Uptime</div><div class="metric-value">'+fmtUptime(st.uptime_s*1000)+'</div></div>'+
        '<div class="metric"><div class="metric-label">Memória</div><div class="metric-value">'+fmtHeap(st.free_heap)+'</div></div>'+
      '</div>'+
      '<div style="font-size:0.75rem;color:var(--muted-subtle);margin-top:8px">Último envio: '+ago+'</div>'+
    '</div>';
  }).join('');
}

function updateFilters(sensors) {
  var bar = document.getElementById('filter-bar');
  if (!bar) return;
  var types = {};
  sensors.forEach(function(s) { types[s.type] = true; });
  var names = {1:'Temp+Hum',2:'Contato',3:'Movimento',4:'Gas',5:'Chuva',6:'Tanque',7:'DHT+Gas',8:'Interruptor',9:'Lâmpada',10:'Repeater'};
  var html = '<button class="filter-btn active" data-type="all" onclick="filterSensors(\'all\')">Todos</button>';
  Object.keys(types).sort().forEach(function(t) {
    html += '<button class="filter-btn" data-type="'+t+'" onclick="filterSensors(\''+t+'\')">'+(names[t]||'Tipo '+t)+'</button>';
  });
  bar.innerHTML = html;
}

function filterSensors(type) {
  s_activeFilter = !type ? 'all' : type;
  document.querySelectorAll('.filter-btn').forEach(function(b) { b.classList.remove('active'); });
  var btn = s_activeFilter==='all' ? document.querySelector('.filter-btn[data-type="all"]') : document.querySelector('.filter-btn[data-type="'+s_activeFilter+'"]');
  if (btn) btn.classList.add('active');
  applyFilters();
}

function filterStatus(st) {
  s_statusFilter = st;
  document.querySelectorAll('.stat[data-status]').forEach(function(s) { s.classList.remove('active'); });
  var el = document.querySelector('.stat[data-status="'+st+'"]');
  if (el) el.classList.add('active');
  applyFilters();
}

function applyFilters() {
  document.querySelectorAll('.device').forEach(function(d) {
    var show = true;
    if (s_activeFilter !== 'all' && d.dataset.type !== s_activeFilter) show = false;
    if (s_statusFilter === 'online' && d.classList.contains('offline')) show = false;
    if (s_statusFilter === 'offline' && !d.classList.contains('offline')) show = false;
    d.style.display = show ? '' : 'none';
  });
}

var s_pairingTimer = null;
var s_pairingWindowSec = 60;

function startPairingUI(remainingSec) {
  var btn = document.getElementById('btn-pair');
  if (!btn) return;
  if (s_pairingTimer) { clearInterval(s_pairingTimer); s_pairingTimer = null; }
  btn.classList.add('btn-pairing');
  btn.textContent = 'Cancelar ('+remainingSec+'s)';
  s_pairingTimer = setInterval(function() {
    remainingSec--;
    if (remainingSec > 0) {
      btn.textContent = 'Cancelar ('+remainingSec+'s)';
    } else {
      exitPairingMode();
    }
  }, 1000);
}

function enterPairingMode() {
  if (s_pairingTimer) { exitPairingMode(); return; }
  api('/api/pair/start', {method:'POST'}).then(function() {
    startPairingUI(s_pairingWindowSec);
  }).catch(function(e) {
    showToast('Erro: '+e.message, true);
  });
}

function exitPairingMode() {
  if (s_pairingTimer) { clearInterval(s_pairingTimer); s_pairingTimer = null; }
  api('/api/pair/stop', {method:'POST'}).catch(function(){});
  var btn = document.getElementById('btn-pair');
  if (btn) { btn.classList.remove('btn-pairing'); btn.textContent = '+ Adicionar Sensor'; }
}

function toggleDeviceMenu(slot) {
  if (window.innerWidth <= 700) { showPropsModal(slot); return; }
  var m = document.getElementById('dmenu-'+slot);
  if (!m) return;
  var open = m.classList.contains('show');
  document.querySelectorAll('.menu-dropdown').forEach(function(d) { d.classList.remove('show'); });
  if (!open) m.classList.add('show');
}

function closeDeviceMenu(slot) {
  var m = document.getElementById('dmenu-'+slot);
  if (m) m.classList.remove('show');
}

document.addEventListener('click', function(e) {
  if (e.target.closest('.menu-dropdown') || e.target.closest('.menu-trigger')) return;
  document.querySelectorAll('.menu-dropdown').forEach(function(m) { m.classList.remove('show'); });
});

function showPropsModal(slot) {
  closeDeviceMenu(slot);
  var s = null;
  for (var i = 0; i < s_sensors.length; i++) {
    if (s_sensors[i].slot === slot) { s = s_sensors[i]; break; }
  }
  var body = document.getElementById('props-body');
  var title = document.getElementById('props-title');
  if (!s) { body.innerHTML = '<p style="color:var(--danger)">Sensor não encontrado</p>'; document.getElementById('props-modal').classList.add('show'); return; }
  title.textContent = escHtml(s.name||'Sem nome')+' — '+typeName(s.type);
  var st = s.state || {};
  var stateHtml = '';
  if (s.type === 1) stateHtml = '<div class="row"><span class="label">Temperatura</span><span class="value">'+(st.temperature||0).toFixed(1)+'&deg;C</span></div><div class="row"><span class="label">Umidade</span><span class="value">'+(st.humidity||0).toFixed(0)+'%</span></div>';
  else if (s.type === 2) stateHtml = '<div class="row"><span class="label">Contato</span><span class="value">'+(st.contact?'Aberto':'Fechado')+'</span></div>'+(st.tamper!==undefined?'<div class="row"><span class="label">Tamper</span><span class="value">'+(st.tamper?'Sim':'Não')+'</span></div>':'');
  else if (s.type === 3) stateHtml = '<div class="row"><span class="label">Movimento</span><span class="value">'+(st.occupancy?'Detectado':'Nenhum')+'</span></div>'+(st.duration!==undefined?'<div class="row"><span class="label">Duração</span><span class="value">'+st.duration+'s</span></div>':'');
  else if (s.type === 4) stateHtml = '<div class="row"><span class="label">Gás</span><span class="value">'+(st.gas_level||0)+'%</span></div>'+(st.alarm?'<div class="row"><span class="label">Alarme</span><span class="value" style="color:var(--danger)">ATIVO</span></div>':'');
  else if (s.type === 5) stateHtml = '<div class="row"><span class="label">Nível</span><span class="value">'+(st.rain_level||0)+'%</span></div><div class="row"><span class="label">Digital</span><span class="value">'+(st.rain_digital?'Chuva':'Seco')+'</span></div>';
  else if (s.type === 6) stateHtml = '<div class="row"><span class="label">Nível</span><span class="value">'+(st.level_pct||0)+'%</span></div><div class="row"><span class="label">Distância</span><span class="value">'+(st.distance_cm||0)+' cm</span></div>';
  else if (s.type === 7) stateHtml = '<div class="row"><span class="label">Temperatura</span><span class="value">'+(st.temperature||0).toFixed(1)+'&deg;C</span></div><div class="row"><span class="label">Umidade</span><span class="value">'+(st.humidity||0).toFixed(0)+'%</span></div><div class="row"><span class="label">Gás</span><span class="value">'+(st.gas_level||0)+'%</span></div>'+(st.alarm?'<div class="row"><span class="label">Alarme</span><span class="value" style="color:var(--danger)">ATIVO</span></div>':'');
  else if (s.type === 8 || s.type === 9) stateHtml = '<div class="row"><span class="label">Estado</span><span class="value">'+(st.state?'Ligado':'Desligado')+'</span></div>';
  body.innerHTML =
    '<div class="props-section"><div class="props-section-title">Estado</div>'+
    (stateHtml || '<div class="row"><span class="label">Dados</span><span class="value" style="color:var(--muted-subtle)">Aguardando...</span></div>')+
    '</div>'+
    '<div class="props-section"><div class="props-section-title">Dispositivo</div>'+
    '<div class="row"><span class="label">Sensores</span><span class="value">'+s_sensors.length+'/'+(s_info.max_sensors||'--')+'</span></div>'+
    (s.ip?'<div class="row"><span class="label">IP</span><span class="value"><a href="http://'+escHtml(s.ip)+'?from='+escHtml(window.location.hostname)+'">'+escHtml(s.ip)+'</a></span></div>':'')+
    '<div class="row"><span class="label">Bateria</span><span class="value">'+(s.battery_pct!==undefined?s.battery_pct+'%':'--')+'</span></div>'+
    '<div class="row"><span class="label">RSSI</span><span class="value">'+(s.last_rssi!==undefined?s.last_rssi+' dBm':'--')+'</span></div>'+
    '<div class="row"><span class="label">Última vez</span><span class="value">'+(s.last_seen>=0?fmtUptime(s.last_seen):'&mdash;')+'</span></div>'+
    '<div class="row"><span class="label">Sequência</span><span class="value">'+s.sequence+'</span></div>'+
    (s.free_heap!==undefined && s.free_heap>0 ? '<div class="row"><span class="label">Mem. Livre</span><span class="value">'+fmtBytes(s.free_heap)+'</span></div>' : '')+
    '</div>';
  var modal = document.getElementById('props-modal');
  modal.classList.add('show');
  modal.onclick = function(e) { if (e.target === modal) closePropsModal(); };
}

function closePropsModal() {
  document.getElementById('props-modal').classList.remove('show');
}

async function renameSensor(slot) {
  var d = document.getElementById('rename-input');
  var slotLabel = document.getElementById('rename-slot');
  var typeLabel = document.getElementById('rename-type');
  if (!d || !slotLabel || !typeLabel) return;
  slotLabel.textContent = slot;
  var dev = document.querySelector('.device[data-slot="'+slot+'"]');
  typeLabel.textContent = dev ? typeName(parseInt(dev.dataset.type)) : '';
  d.value = '';
  s_renameSlot = slot;
  var modal = document.getElementById('rename-modal');
  modal.classList.add('show');
  modal.onclick = function(e) { if (e.target === modal) closeRename(); };
  setTimeout(function() { d.focus(); }, 100);
}

function closeRename() {
  document.getElementById('rename-modal').classList.remove('show');
  s_renameSlot = null;
}

async function confirmRename() {
  var name = document.getElementById('rename-input').value.trim();
  if (!name) { showToast('Nome obrigatório', true); return; }
  try {
    await api('/api/sensor/'+s_renameSlot+'/name', {method:'POST', body:JSON.stringify({name: name})});
    showToast('Nome atualizado');
    closeRename();
    loadData();
  } catch(e) { showToast('Erro: '+e.message, true); }
}

async function removeSensor(slot) {
  if (!confirm('Remover sensor do slot '+slot+'?')) return;
  try {
    await api('/api/sensor/'+slot+'/remove', {method:'POST'});
    showToast('Sensor removido');
    loadData();
  } catch(e) { showToast('Erro: '+e.message, true); }
}

async function toggleSensor(slot, state) {
  try {
    await api('/api/sensor/'+slot+'/command', {method:'POST', body:JSON.stringify({state: !!state})});
    showToast(state ? 'Ligado' : 'Desligado');
    loadData();
  } catch(e) { showToast('Erro: '+e.message, true); }
}

async function loadData() {
  if (s_overviewLoading) return;
  s_overviewLoading = true;
  try {
    var data = await Promise.all([api('/api/info'), api('/api/sensors')]);
    var info = data[0], sensors = data[1];
    s_info = info;
    document.getElementById('stat-paired').textContent = info.paired_count + '/' + info.max_sensors;
    document.getElementById('stat-online').textContent = info.online_count;
    document.getElementById('stat-offline').textContent = info.paired_count - info.online_count;
    document.getElementById('stat-rx').textContent = info.rx_total;
    document.getElementById('stat-uptime').textContent = fmtUptime(info.uptime_ms);
    if (info.fw_version) document.getElementById('fw-sidebar').textContent = info.fw_version;
    updateMqttFooter(info.mqtt_connected, info.mqtt_host, info.mqtt_port);
    s_sensors = sensors;
    updateFilters(sensors);
    var nonRepeater = sensors.filter(function(s) { return s.type_name !== 'repeater'; });
    var prevCount = Object.keys(s_prevSensorMap).length;
    if (nonRepeater.length !== prevCount || nonRepeater.length === 0) {
      renderSensors(sensors);
      renderRepeaters(sensors);
    } else {
      var changed = false;
      nonRepeater.forEach(function(s) {
        var prev = s_prevSensorMap[s.slot];
        var curr = JSON.stringify(s);
        if (prev !== curr) {
          var el = document.querySelector('.device[data-slot="'+s.slot+'"]');
          if (el) {
            var tmp = document.createElement('div');
            tmp.innerHTML = buildSensorCard(s);
            el.replaceWith(tmp.firstChild);
          }
          changed = true;
        }
      });
      renderRepeaters(sensors);
      if (changed) {
        filterSensors(s_activeFilter);
        filterStatus(s_statusFilter);
      }
      s_prevSensorMap = {};
      nonRepeater.forEach(function(s) { s_prevSensorMap[s.slot] = JSON.stringify(s); });
    }
  } catch(e) {
    showToast('Erro ao carregar: '+e.message, true);
  } finally {
    s_overviewLoading = false;
  }
}

loadData();
s_pollTimer = setInterval(loadData, 5000);
</script>
)rawliteral";

const char PAGE_SETTINGS[] PROGMEM = R"rawliteral(
<style>
.card{background:var(--surface);border:1px solid var(--border);border-radius:10px;padding:16px}
.card.collapsible{padding:0;overflow:hidden}
.card-head{display:flex;align-items:center;justify-content:space-between;padding:16px;cursor:pointer;user-select:none}
.card-head h2{font-size:0.95rem;font-weight:600;color:var(--primary);margin:0}
.card-head .summary{display:flex;align-items:center;gap:8px;flex:1;margin-left:10px;justify-content:flex-end;text-align:right}
.chev{transition:transform .2s;color:var(--muted);font-size:0.9rem}
.card.collapsed .chev{transform:rotate(-90deg)}
.card-body{padding:0 16px 16px}
.card.collapsed .card-body{display:none}
.row{display:flex;justify-content:space-between;align-items:center;padding:8px 0;border-bottom:1px solid var(--border);font-size:0.85rem}
.row:last-child{border-bottom:none}
.label{color:var(--muted-subtle)}
.value{font-weight:600}
.badge{display:inline-block;padding:2px 8px;border-radius:6px;font-size:0.72rem;font-weight:600;max-width:200px;overflow:hidden;text-overflow:ellipsis;white-space:nowrap}
.badge-ok{background:rgba(34,197,94,.15);color:#22c55e}
.badge-off{background:rgba(239,68,68,.15);color:#ef4444}
.badge-info{background:rgba(94,106,210,.15);color:var(--primary)}
.btn{padding:10px 18px;border:none;border-radius:8px;font-weight:600;cursor:pointer;font-size:0.85rem;min-height:44px}
.btn-primary{background:var(--primary);color:#fff}
.btn-primary:hover{filter:brightness(1.1)}
.btn-secondary{background:var(--border);color:var(--text)}
.btn-secondary:hover{background:var(--border-strong)}
.modal{position:fixed;inset:0;background:rgba(0,0,0,.5);display:none;align-items:center;justify-content:center;z-index:100}
.modal.show{display:flex}
.modal-content{background:var(--surface);border:1px solid var(--border);border-radius:12px;padding:24px;width:100%;max-width:400px}
.form-group{margin-bottom:14px}
.form-group label{display:block;margin-bottom:4px;font-size:0.82rem;color:var(--muted)}
.form-group input{width:100%;padding:10px 12px;border:1px solid var(--border);border-radius:8px;font-size:16px;background:var(--surface-2);color:var(--text)}
.toggle-row{display:flex;gap:8px;margin:10px 0}
.seg{flex:1;padding:10px;border:1px solid var(--border);background:var(--surface-2);color:var(--muted);border-radius:8px;font-weight:600;cursor:pointer;font-size:0.85rem}
.seg.active{background:var(--primary);color:#fff;border-color:var(--primary)}
.hidden{display:none!important}
h3{font-size:0.95rem;font-weight:600;margin-bottom:16px}
</style>
<div style="max-width:500px">

<div class="card collapsible" id="card-bridge">
<div class="card-head" onclick="toggleCard('card-bridge')">
<h2>Bridge</h2>
<div class="summary"><span id="bridge-sum" class="badge badge-info">--</span><span class="chev">&#9662;</span></div>
</div>
<div class="card-body">
<div class="row"><span class="label">Device ID</span><span class="value" id="s-device" style="font-size:0.7rem">--</span></div>
<div class="row"><span class="label">IP</span><span class="value" id="s-ip">--</span></div>
<div class="row"><span class="label">Firmware</span><span class="value" id="s-fw">--</span></div>
<div class="row"><span class="label">Uptime</span><span class="value" id="s-uptime">--</span></div>
<div class="row"><span class="label">Mem. Livre</span><span class="value" id="s-free-heap">--</span></div>
<button class="btn btn-primary" id="btn-pair" onclick="enterPairingMode()" style="margin-top:12px;width:100%">+ Adicionar Sensor</button>
<button class="btn btn-primary" onclick="doRestart()" style="margin-top:8px;width:100%">Reiniciar</button>
</div>
</div>

<div class="card collapsible" id="card-wifi" style="margin-top:12px">
<div class="card-head" onclick="toggleCard('card-wifi')">
<h2>Rede (WiFi)</h2>
<div class="summary"><span id="wifi-sum" class="badge badge-info">--</span><span class="chev">&#9662;</span></div>
</div>
<div class="card-body">
<div class="row"><span class="label">SSID</span><span class="value" id="s-wifi-ssid">--</span></div>
<div class="row"><span class="label">IP</span><span class="value" id="s-wifi-ip">--</span></div>
<div class="row"><span class="label">Modo</span><span class="value" id="s-wifi-mode">--</span></div>
<button class="btn btn-primary" onclick="showWifiForm()" style="margin-top:12px;width:100%">Configurar Rede</button>
</div>
</div>

<div class="card collapsible" id="card-mqtt" style="margin-top:12px">
<div class="card-head" onclick="toggleCard('card-mqtt')">
<h2>MQTT</h2>
<div class="summary"><span id="mqtt-sum" class="badge badge-off">--</span><span class="chev">&#9662;</span></div>
</div>
<div class="card-body">
<div class="row"><span class="label">Host</span><span class="value" id="s-host">--</span></div>
<div class="row"><span class="label">Porta</span><span class="value" id="s-port">--</span></div>
<div class="row"><span class="label">Usuário</span><span class="value" id="s-user">--</span></div>
<div class="row"><span class="label">Status</span><span class="value" id="s-status">--</span></div>
<div class="row"><span class="label">Conectado há</span><span class="value" id="s-mqtt-uptime">--</span></div>
<button class="btn btn-primary" onclick="showMqttForm()" style="margin-top:12px;width:100%">Configurar MQTT</button>
<button class="btn btn-secondary" onclick="doReregister()" style="margin-top:8px;width:100%">Forçar re-registro</button>
</div>
</div>
</div>
<div class="modal" id="mqtt-modal">
<div class="modal-content">
<h3>Configurar MQTT</h3>
<div class="form-group"><label>Broker IP</label><input type="text" id="mqtt-host-input" placeholder="Ex: 192.168.1.50" maxlength="64"></div>
<div class="form-group"><label>Porta</label><input type="number" id="mqtt-port-input" placeholder="1883" min="1" max="65535"></div>
<div class="form-group"><label>Usuário (opcional)</label><input type="text" id="mqtt-user-input" placeholder="usuario" maxlength="32"></div>
<div class="form-group"><label>Senha (opcional)</label><input type="password" id="mqtt-pass-input" placeholder="senha" maxlength="32"></div>
<div style="display:flex;gap:8px;margin-top:16px">
<button class="btn btn-primary" onclick="saveMqttConfig()">Salvar</button>
<button class="btn btn-secondary" onclick="closeMqttForm()">Cancelar</button>
</div>
</div>
</div>

<div class="modal" id="wifi-modal">
<div class="modal-content">
<h3>Configurar Rede (WiFi)</h3>
<div class="form-group"><label>SSID</label><input type="text" id="wifi-ssid-input" maxlength="32" placeholder="Nome da rede"></div>
<div class="form-group"><label>Senha</label><input type="password" id="wifi-pass-input" maxlength="64" placeholder="(em branco mantém atual)"></div>
<div class="toggle-row">
<button type="button" class="seg active" id="seg-dhcp" onclick="setWifiMode(0)">DHCP</button>
<button type="button" class="seg" id="seg-static" onclick="setWifiMode(1)">IP Fixo</button>
</div>
<div id="wifi-static-fields" class="hidden">
<div class="form-group"><label>IP Fixo</label><input type="text" id="wifi-ip-input" placeholder="192.168.1.50"></div>
<div class="form-group"><label>Gateway</label><input type="text" id="wifi-gw-input" placeholder="192.168.1.1"></div>
<div class="form-group"><label>Máscara</label><input type="text" id="wifi-mask-input" placeholder="255.255.255.0"></div>
<div class="form-group"><label>DNS</label><input type="text" id="wifi-dns-input" placeholder="192.168.1.1"></div>
</div>
<div style="display:flex;gap:8px;margin-top:16px">
<button class="btn btn-primary" onclick="saveWifiConfig()">Salvar</button>
<button class="btn btn-secondary" onclick="closeWifiForm()">Cancelar</button>
</div>
</div>
</div>

<script>
function toggleCard(id) {
  document.getElementById(id).classList.toggle('collapsed');
}

function setWifiMode(mode) {
  document.getElementById('seg-dhcp').classList.toggle('active', mode === 0);
  document.getElementById('seg-static').classList.toggle('active', mode === 1);
  document.getElementById('wifi-static-fields').classList.toggle('hidden', mode !== 1);
  window.s_wifiMode = mode;
}

async function loadSettings() {
  try {
    var info = await api('/api/info');
    document.getElementById('s-host').textContent = info.mqtt_host || '--';
    document.getElementById('s-port').textContent = info.mqtt_port || '--';
    document.getElementById('s-user').textContent = info.mqtt_user || '--';
    document.getElementById('s-status').textContent = info.mqtt_connected ? 'Conectado' : 'Desconectado';
    document.getElementById('mqtt-sum').textContent = info.mqtt_connected ? 'MQTT OK' : 'MQTT OFF';
    document.getElementById('mqtt-sum').className = 'badge ' + (info.mqtt_connected ? 'badge-ok' : 'badge-off');
    document.getElementById('s-mqtt-uptime').textContent = info.mqtt_connected && info.mqtt_connected_since ? fmtUptime(info.uptime_ms - info.mqtt_connected_since) : '--';
    document.getElementById('s-device').textContent = info.gateway_id || '--';
    document.getElementById('s-ip').textContent = info.ip || '--';
    document.getElementById('s-fw').textContent = info.fw_version || '--';
    document.getElementById('s-uptime').textContent = fmtUptime(info.uptime_ms);
    document.getElementById('s-free-heap').textContent = info.free_heap !== undefined ? fmtBytes(info.free_heap) : '--';
    document.getElementById('bridge-sum').textContent = info.ip || '--';
    if (info.fw_version) document.getElementById('fw-sidebar').textContent = info.fw_version;
    updateMqttFooter(info.mqtt_connected, info.mqtt_host, info.mqtt_port);
    if (info.pairing_mode && info.pairing_remaining_sec > 0) {
      startPairingUI(info.pairing_remaining_sec);
    }
    var wifi = await api('/api/config/wifi');
    document.getElementById('s-wifi-ssid').textContent = (wifi.ssid && wifi.ssid.length) ? wifi.ssid : (info.wifi_ssid || '--');
    document.getElementById('s-wifi-ip').textContent = info.ip || '--';
    var isStatic = wifi.mode === 1;
    document.getElementById('s-wifi-mode').textContent = isStatic ? 'IP Fixo' : 'DHCP';
    document.getElementById('wifi-sum').textContent = ((wifi.ssid && wifi.ssid.length) ? wifi.ssid : (info.wifi_ssid || 'sem SSID'));
    document.getElementById('wifi-sum').className = 'badge ' + ((wifi.ssid && wifi.ssid.length) ? 'badge-info' : 'badge-off');
  } catch(e) {
    showToast('Erro ao carregar: '+e.message, true);
  }
}

function showMqttForm() {
  var host = document.getElementById('s-host').textContent;
  var port = document.getElementById('s-port').textContent;
  var user = document.getElementById('s-user').textContent;
  document.getElementById('mqtt-host-input').value = host === '--' ? '' : host;
  document.getElementById('mqtt-port-input').value = port === '--' ? '1883' : port;
  document.getElementById('mqtt-user-input').value = user === '--' ? '' : user;
  document.getElementById('mqtt-pass-input').value = '';
  document.getElementById('mqtt-modal').classList.add('show');
  document.getElementById('mqtt-host-input').focus();
}

function closeMqttForm() {
  document.getElementById('mqtt-modal').classList.remove('show');
}

async function saveMqttConfig() {
  var host = document.getElementById('mqtt-host-input').value.trim();
  var port = parseInt(document.getElementById('mqtt-port-input').value) || 1883;
  var user = document.getElementById('mqtt-user-input').value.trim();
  var pass = document.getElementById('mqtt-pass-input').value;
  if (!host) { showToast('IP do broker obrigatório', true); return; }
  try {
    await api('/api/config/mqtt', {method:'POST', body:JSON.stringify({host:host, port:port, user:user, pass:pass})});
    showToast('MQTT configurado: '+host+':'+port);
    closeMqttForm();
    loadSettings();
  } catch(e) { showToast('Erro: '+e.message, true); }
}

async function showWifiForm() {
  try {
    var wifi = await api('/api/config/wifi');
    document.getElementById('wifi-ssid-input').value = wifi.ssid || '';
    document.getElementById('wifi-pass-input').value = '';
    setWifiMode(wifi.mode === 1 ? 1 : 0);
    document.getElementById('wifi-ip-input').value = wifi.ip || '';
    document.getElementById('wifi-gw-input').value = wifi.gateway || '';
    document.getElementById('wifi-mask-input').value = wifi.subnet || '';
    document.getElementById('wifi-dns-input').value = wifi.dns || '';
    document.getElementById('wifi-modal').classList.add('show');
    document.getElementById('wifi-ssid-input').focus();
  } catch(e) { showToast('Erro: '+e.message, true); }
}

function closeWifiForm() {
  document.getElementById('wifi-modal').classList.remove('show');
}

async function saveWifiConfig() {
  var ssid = document.getElementById('wifi-ssid-input').value.trim();
  var pass = document.getElementById('wifi-pass-input').value;
  var mode = window.s_wifiMode || 0;
  if (!ssid) { showToast('SSID obrigatório', true); return; }
  if (mode === 1) {
    var ip = document.getElementById('wifi-ip-input').value.trim();
    var gw = document.getElementById('wifi-gw-input').value.trim();
    if (!ip || !gw) { showToast('IP Fixo e Gateway obrigatórios', true); return; }
  }
  var body = { ssid: ssid, pass: pass, mode: mode,
    ip: document.getElementById('wifi-ip-input').value.trim(),
    gateway: document.getElementById('wifi-gw-input').value.trim(),
    subnet: document.getElementById('wifi-mask-input').value.trim(),
    dns: document.getElementById('wifi-dns-input').value.trim() };
  try {
    await api('/api/config/wifi', {method:'POST', body:JSON.stringify(body)});
    showToast('Rede salva, reiniciando...');
    closeWifiForm();
    setTimeout(function() { window.location.reload(); }, 2500);
  } catch(e) { showToast('Erro: '+e.message, true); }
}

async function doRestart() {
  if (!confirm('Reiniciar o gateway?')) return;
  try {
    await api('/api/restart', {method:'POST'});
    showToast('Reiniciando...');
    setTimeout(function() { window.location.reload(); }, 3000);
  } catch(e) { showToast('Erro: '+e.message, true); }
}

async function doReregister() {
  try {
    await api('/api/broadcast', {method:'POST'});
    showToast('Re-registro enviado');
  } catch(e) { showToast('Erro: '+e.message, true); }
}

loadSettings();
</script>
)rawliteral";

const char PAGE_LOGS[] PROGMEM = R"rawliteral(
<style>
.log-table{display:flex;flex-direction:column;gap:4px}
.log-row{display:flex;gap:8px;padding:6px 10px;border-radius:6px;font-size:0.8rem;border-left:3px solid var(--muted)}
.log-row.info{border-left-color:var(--muted)}
.log-row.warn{border-left-color:var(--warn)}
.log-row.error{border-left-color:var(--danger)}
.log-time{color:var(--muted-subtle);min-width:60px;white-space:nowrap}
.log-msg{flex:1;word-break:break-word}
.log-loading{padding:40px;text-align:center;color:var(--muted)}
.log-empty{text-align:center;padding:40px;color:var(--muted-subtle)}
</style>
<div>
<div style="display:flex;justify-content:space-between;align-items:center;margin-bottom:16px">
<h2 style="font-size:0.95rem;font-weight:600;color:var(--primary)">Logs</h2>
<button class="clear-btn" onclick="clearLogs()" style="font-size:0.75rem;padding:4px 12px;border-radius:6px;border:1px solid var(--danger);background:transparent;color:var(--danger);cursor:pointer">Limpar</button>
</div>
<div id="log-table"><div class="log-loading">carregando...</div></div>
</div>
<script>
async function clearLogs() {
  try {
    await fetch('/api/logs/clear', {method:'POST'});
    loadLogs();
  } catch(e) {
    document.getElementById('log-table').innerHTML = '<div class="log-empty" style="color:var(--danger)">Erro ao limpar logs</div>';
  }
}
async function loadLogs() {
  try {
    var d = await fetch('/api/logs').then(function(r) { return r.json(); });
    var table = document.getElementById('log-table');
    if (!d.logs || !d.logs.length) {
      table.innerHTML = '<div class="log-empty">Nenhum evento registrado</div>';
      return;
    }
    table.innerHTML = '<div class="log-table">' + d.logs.map(function(l) {
      var s = Math.floor(l.t/1000);
      var m = Math.floor(s/60)%60;
      var h = Math.floor(s/3600)%24;
      var sec = s%60;
      var ts = (h<10?'0':'')+h+':'+(m<10?'0':'')+m+':'+(sec<10?'0':'')+sec;
      var cls = l.level === 'error' ? 'error' : l.level === 'warn' ? 'warn' : 'info';
      return '<div class="log-row '+cls+'"><span class="log-time">'+ts+'</span><span class="log-msg">'+(parent.escHtml||escHtml)(l.msg)+'</span></div>';
    }).join('') + '</div>';
  } catch(e) {
    document.getElementById('log-table').innerHTML = '<div class="log-empty" style="color:var(--danger)">Erro ao carregar logs</div>';
  }
}
loadLogs();
s_pollTimer = setInterval(loadLogs, 10000);
</script>
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
<div class="desc">Status do gateway: sensores pareados/online, RX total, uptime, modo de pareamento, versao FW, configuracao MQTT</div>
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

<h2>MQTT</h2>
<div class="endpoint">
<div class="head"><span class="method post">POST</span><span class="path">/api/broadcast</span></div>
<div class="desc">Publica discovery + estado de todos os sensores via MQTT</div>
</div>
<div class="endpoint">
<div class="head"><span class="method get">GET</span><span class="path">/api/config/mqtt</span></div>
<div class="desc">Retorna configuracao atual do broker MQTT</div>
</div>
<div class="endpoint">
<div class="head"><span class="method post">POST</span><span class="path">/api/config/mqtt</span></div>
<div class="desc">Configura broker MQTT</div>
<div class="params">
<table><tr><th>Param</th><th>Tipo</th><th>Descricao</th></tr>
<tr><td><code>host</code></td><td>string</td><td>IP do broker MQTT</td></tr>
<tr><td><code>port</code></td><td>number</td><td>Porta do broker</td></tr>
<tr><td><code>user</code></td><td>string</td><td>Usuario (opcional)</td></tr>
<tr><td><code>pass</code></td><td>string</td><td>Senha (opcional)</td></tr></table>
</div>
</div>

<h2>Rede (WiFi)</h2>
<div class="endpoint">
<div class="head"><span class="method get">GET</span><span class="path">/api/config/wifi</span></div>
<div class="desc">Retorna configuracao de rede (SSID, modo DHCP/estatico, IP, gateway, mascara, DNS)</div>
</div>
<div class="endpoint">
<div class="head"><span class="method post">POST</span><span class="path">/api/config/wifi</span></div>
<div class="desc">Configura rede WiFi (credenciais + IP fixo ou DHCP). Reinicia o gateway para aplicar</div>
<div class="params">
<table><tr><th>Param</th><th>Tipo</th><th>Descricao</th></tr>
<tr><td><code>ssid</code></td><td>string</td><td>Nome da rede (obrigatorio)</td></tr>
<tr><td><code>pass</code></td><td>string</td><td>Senha (vazio mantem a atual)</td></tr>
<tr><td><code>mode</code></td><td>number</td><td>0 = DHCP, 1 = IP fixo</td></tr>
<tr><td><code>ip</code> / <code>gateway</code> / <code>subnet</code> / <code>dns</code></td><td>string</td><td>Usados quando mode=1</td></tr></table>
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

<p class="footer">ESP-NOW Gateway &mdash; <span id="v">v0.0.21</span></p>
</body>
</html>
)rawliteral";

#endif
