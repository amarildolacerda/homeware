#ifndef PAGES_H
#define PAGES_H

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
.mqtt-footer{position:fixed;bottom:0;right:0;left:200px;background:var(--surface);border-top:1px solid var(--border);padding:8px 24px;display:flex;align-items:center;gap:8px;font-size:0.78rem;z-index:10}
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

function fmtUptime(ms) {
  var s = Math.floor(ms/1000);
  if (s < 60) return s+'s';
  if (s < 3600) return Math.floor(s/60)+'m '+s%60+'s';
  if (s < 86400) return Math.floor(s/3600)+'h '+Math.floor((s%3600)/60)+'m';
  return Math.floor(s/86400)+'d '+Math.floor((s%86400)/3600)+'h';
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
.stats{display:grid;grid-template-columns:repeat(4,1fr);gap:12px;margin-bottom:20px}
.stat{background:var(--surface);border:1px solid var(--border);border-radius:10px;padding:16px;text-align:center}
.stat-value{font-size:1.4rem;font-weight:700;color:var(--primary)}
.stat-label{font-size:0.7rem;color:var(--muted-subtle);text-transform:uppercase;letter-spacing:.05em;margin-top:4px}
.filters{display:flex;gap:6px;flex-wrap:wrap;margin-bottom:16px}
.filter-btn{padding:6px 14px;border:1px solid var(--border);border-radius:9999px;background:var(--surface);color:var(--muted);font-size:0.78rem;cursor:pointer;transition:all .15s;user-select:none}
.filter-btn:hover{border-color:var(--primary-strong);color:var(--primary)}
.filter-btn.active{background:var(--primary);color:#fff;border-color:var(--primary)}
.grid{display:grid;gap:12px}
.grid-2{grid-template-columns:repeat(auto-fill,minmax(280px,1fr))}
.device{background:var(--surface);border:1px solid var(--border);border-radius:10px;padding:14px;transition:border-color .15s}
.device:hover{border-color:var(--primary-strong)}
.device.offline{border-color:var(--danger);opacity:.7}
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
.details{overflow:hidden;max-height:0;transition:max-height .3s}
.details.open{max-height:500px}
.details-inner{display:grid;grid-template-columns:1fr 1fr;gap:6px;font-size:0.78rem;padding-top:8px;border-top:1px solid var(--border);margin-top:8px}
.details-inner .label{color:var(--muted-subtle)}
.details-inner .value{font-weight:500}
.device-actions{display:flex;gap:6px;margin-top:8px;flex-wrap:wrap}
.btn-sm{padding:6px 12px;font-size:0.75rem;border-radius:6px;border:1px solid var(--border);background:var(--surface);color:var(--text);cursor:pointer}
.btn-sm:hover{background:var(--surface-2)}
.btn-sm.danger{color:var(--danger);border-color:var(--danger)}
.btn-sm.danger:hover{background:#fef2f2}
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
.btn-onoff.on{background:var(--success);color:#fff}
.btn-onoff.off{background:var(--border);color:var(--text)}
.btn-onoff.off:hover{background:var(--border-strong)}
.loading{padding:40px;text-align:center;color:var(--muted)}
@keyframes slideIn{from{opacity:0;transform:translateY(10px)}to{opacity:1;transform:translateY(0)}}
@media(max-width:600px){.stats{grid-template-columns:1fr 1fr}}
</style>
<div class="stats">
<div class="stat"><div class="stat-value" id="stat-paired">--</div><div class="stat-label">Pareados</div></div>
<div class="stat"><div class="stat-value" id="stat-online">--</div><div class="stat-label">Online</div></div>
<div class="stat"><div class="stat-value" id="stat-rx">--</div><div class="stat-label">RX Total</div></div>
<div class="stat"><div class="stat-value" id="stat-uptime">--</div><div class="stat-label">Uptime</div></div>
</div>
<div class="filters">
<button class="filter-btn active" data-type="all" onclick="filterSensors('all')">Todos</button>
<button class="filter-btn" data-type="1" onclick="filterSensors('1')">Temp</button>
<button class="filter-btn" data-type="2" onclick="filterSensors('2')">Contato</button>
<button class="filter-btn" data-type="3" onclick="filterSensors('3')">Movimento</button>
<button class="filter-btn" data-type="4" onclick="filterSensors('4')">Gás</button>
<button class="filter-btn" data-type="5" onclick="filterSensors('5')">Chuva</button>
<button class="filter-btn" data-type="6" onclick="filterSensors('6')">Tanque</button>
<button class="filter-btn" data-type="7" onclick="filterSensors('7')">DHT+Gas</button>
<button class="filter-btn" data-type="8" onclick="filterSensors('8')">Interruptor</button>
</div>
<div id="sensors-grid" class="grid grid-2"><div class="loading">carregando...</div></div>
<script>
var s_overviewLoading = false;

function typeName(type) {
  var names = {1:'Temp+Hum',2:'Contato',3:'Movimento',4:'Gas',5:'Chuva',6:'Tanque',7:'DHT+Gas',8:'Interruptor'};
  return names[type] || 'Desconhecido';
}

function typeIcon(type) {
  var icons = {1:'&#x1F321;',2:'&#x1F514;',3:'&#x1F3C3;',4:'&#x1F4A8;',5:'&#x2614;',6:'&#x1F4A7;',7:'&#x1F321;+&#x1F4A8;',8:'&#x1F50C;'};
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
  } else if (s.type === 8) {
    var on = st.state ? true : false;
    html += '<button class="btn-onoff '+(on?'on':'off')+'" onclick="toggleSensor('+s.slot+','+(on?0:1)+')">'+(on?'DESLIGAR':'LIGAR')+'</button>';
  }
  return html || '<span class="state-item" style="color:var(--muted-subtle)">Aguardando dados...</span>';
}

function renderSensors(sensors) {
  var grid = document.getElementById('sensors-grid');
  if (!sensors || !sensors.length) {
    grid.innerHTML = '<div style="text-align:center;padding:40px;color:var(--muted-subtle)">Nenhum sensor pareado</div>';
    return;
  }
  var expanded = {};
  sensors.forEach(function(s) {
    var d = document.getElementById('details-'+s.slot);
    if (d && d.classList.contains('open')) expanded[s.slot] = true;
  });
  grid.innerHTML = sensors.map(function(s) {
    var off = !s.online;
    var offClass = off ? ' offline' : '';
    var isType8 = s.type === 8;
    var onState = s.state && s.state.state;
    return '<div class="device'+offClass+'" data-slot="'+s.slot+'" data-type="'+s.type+'">'+
      '<div class="device-head">'+
        '<div>'+
          '<div class="device-icon">'+typeIcon(s.type)+'</div>'+
          '<div class="device-name">'+escHtml(s.name||'Sem nome')+'</div>'+
          '<div class="device-type">'+typeName(s.type)+' &bull; Slot '+s.slot+'</div>'+
        '</div>'+
        '<div><span class="badge '+(s.online?'online':'offline')+'">'+(s.online?'Online':'Offline')+'</span></div>'+
      '</div>'+
      '<div class="metrics">'+
        batteryBar(s.battery_pct)+
        rssiBar(s.last_rssi)+
      '</div>'+
      '<div class="state-group">'+
        (isType8
          ? '<button class="btn-onoff '+(onState?'on':'off')+'" onclick="toggleSensor('+s.slot+','+(onState?0:1)+')">'+(onState?'DESLIGAR':'LIGAR')+'</button>'
          : renderState(s))+
      '</div>'+
      '<div class="details" id="details-'+s.slot+'">'+
        '<div class="details-inner">'+
          '<div><span class="label">MAC</span><span class="value">'+escHtml(s.mac||'--')+'</span></div>'+
          '<div><span class="label">Bateria</span><span class="value">'+(s.battery_pct!==undefined?s.battery_pct+'%':'--')+'</span></div>'+
          '<div><span class="label">RSSI</span><span class="value">'+(s.last_rssi!==undefined?s.last_rssi+' dBm':'--')+'</span></div>'+
          '<div><span class="label">Último</span><span class="value">'+(s.last_seen>=0?fmtUptime(s.last_seen):'&mdash;')+'</span></div>'+
          '<div><span class="label">Seq</span><span class="value">'+s.sequence+'</span></div>'+
          '<div><span class="label">Bridge ID</span><span class="value" style="font-size:0.65rem">'+escHtml(s.bridge_device_id||'&mdash;')+'</span></div>'+
          (s.ip?'<div><span class="label">IP</span><span class="value"><a href="http://'+escHtml(s.ip)+'">'+escHtml(s.ip)+'</a></span></div>':'')+
        '</div>'+
      '</div>'+
      '<div class="device-actions">'+
        '<button class="btn-sm" onclick="renameSensor('+s.slot+')">Renomear</button>'+
        '<button class="btn-sm danger" onclick="removeSensor('+s.slot+')">Remover</button>'+
      '</div>'+
      '<div style="text-align:center;margin-top:6px">'+
        '<button class="collapse-btn" onclick="toggleDetails('+s.slot+')" id="expand-'+s.slot+'">&#9654; detalhes</button>'+
      '</div>'+
    '</div>';
  }).join('');
  Object.keys(expanded).forEach(function(slot) {
    var d = document.getElementById('details-'+slot);
    var e = document.getElementById('expand-'+slot);
    if (d && e) { d.classList.add('open'); e.innerHTML = '&#9660; detalhes'; }
  });
}

function filterSensors(type) {
  document.querySelectorAll('.filter-btn').forEach(function(b) { b.classList.remove('active'); });
  var btn = !type || type==='all' ? document.querySelector('.filter-btn[data-type="all"]') : document.querySelector('.filter-btn[data-type="'+type+'"]');
  if (btn) btn.classList.add('active');
  document.querySelectorAll('.device').forEach(function(d) {
    d.style.display = (!type || type==='all' || d.dataset.type===type) ? '' : 'none';
  });
}

function toggleDetails(slot) {
  var d = document.getElementById('details-'+slot);
  var e = document.getElementById('expand-'+slot);
  if (!d || !e) return;
  var open = d.classList.toggle('open');
  e.innerHTML = open ? '&#9660; detalhes' : '&#9654; detalhes';
}

async function renameSensor(slot) {
  var name = prompt('Novo nome para o sensor slot '+slot+':');
  if (!name || !name.trim()) return;
  try {
    await api('/api/sensor/'+slot+'/name', {method:'POST', body:JSON.stringify({name: name.trim()})});
    showToast('Nome atualizado');
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
    document.getElementById('stat-paired').textContent = info.paired_count;
    document.getElementById('stat-online').textContent = info.online_count;
    document.getElementById('stat-rx').textContent = info.rx_total;
    document.getElementById('stat-uptime').textContent = fmtUptime(info.uptime_ms);
    if (info.fw_version) document.getElementById('fw-sidebar').textContent = info.fw_version;
    updateMqttFooter(info.mqtt_connected, info.mqtt_host, info.mqtt_port);
    sensors.forEach(function(s) { delete s.mac_bytes; });
    renderSensors(sensors);
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
.row{display:flex;justify-content:space-between;align-items:center;padding:8px 0;border-bottom:1px solid var(--border);font-size:0.85rem}
.row:last-child{border-bottom:none}
.label{color:var(--muted-subtle)}
.value{font-weight:600}
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
h3{font-size:0.95rem;font-weight:600;margin-bottom:16px}
</style>
<div style="max-width:500px">
<div class="card">
<h2 style="font-size:0.95rem;font-weight:600;margin-bottom:16px;color:var(--primary)">MQTT</h2>
<div class="row"><span class="label">Host</span><span class="value" id="s-host">--</span></div>
<div class="row"><span class="label">Porta</span><span class="value" id="s-port">--</span></div>
<div class="row"><span class="label">Usuário</span><span class="value" id="s-user">--</span></div>
<div class="row"><span class="label">Status</span><span class="value" id="s-status">--</span></div>
<button class="btn btn-primary" onclick="showMqttForm()" style="margin-top:12px">Configurar MQTT</button>
</div>
<div class="card" style="margin-top:12px">
<h2 style="font-size:0.95rem;font-weight:600;margin-bottom:16px;color:var(--primary)">Bridge</h2>
<div class="row"><span class="label">Device ID</span><span class="value" id="s-device">--</span></div>
<div class="row"><span class="label">Firmware</span><span class="value" id="s-fw">--</span></div>
</div>
<div style="display:flex;gap:8px;margin-top:16px;flex-wrap:wrap">
<button class="btn btn-primary" onclick="doRestart()">Reiniciar</button>
<button class="btn btn-secondary" onclick="doReregister()">Re-registrar</button>
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
<script>
async function loadSettings() {
  try {
    var info = await api('/api/info');
    document.getElementById('s-host').textContent = info.mqtt_host || '--';
    document.getElementById('s-port').textContent = info.mqtt_port || '--';
    document.getElementById('s-user').textContent = info.mqtt_user || '--';
    document.getElementById('s-status').textContent = info.mqtt_connected ? 'Conectado' : 'Desconectado';
    document.getElementById('s-device').textContent = info.gateway_id || '--';
    document.getElementById('s-fw').textContent = info.fw_version || '--';
    if (info.fw_version) document.getElementById('fw-sidebar').textContent = info.fw_version;
    updateMqttFooter(info.mqtt_connected, info.mqtt_host, info.mqtt_port);
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
<h2 style="font-size:0.95rem;font-weight:600;margin-bottom:16px;color:var(--primary)">Logs</h2>
<div id="log-table"><div class="log-loading">carregando...</div></div>
</div>
<script>
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
      return '<div class="log-row '+cls+'"><span class="log-time">'+ts+'</span><span class="log-msg">'+l.msg+'</span></div>';
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
