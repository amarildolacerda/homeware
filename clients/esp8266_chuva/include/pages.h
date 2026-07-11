#pragma once

#include <Arduino.h>

static const char PAGE_DASHBOARD[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="pt-BR">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title id="pageTitle">Sensor de Chuva</title>
<style>
:root{--bg:#f4f4f4;--surface:#fff;--surface-2:#f9fafb;--text:#1f2937;--muted:#6b7280;--muted-subtle:#9ca3af;--primary:#5e6ad2;--primary-strong:#828fff;--primary-focus:#eef0ff;--border:#e5e7eb;--success:#16a34a;--danger:#dc2626;--warn:#f59e0b;--info:#2563eb;--sidebar-w:180px}
*{margin:0;padding:0;box-sizing:border-box}
body{font-family:-apple-system,system-ui,BlinkMacSystemFont,sans-serif;background:var(--bg);color:var(--text);min-height:100vh;display:flex}
.sidebar{position:fixed;left:0;top:0;width:var(--sidebar-w);height:100%;background:var(--surface);border-right:1px solid var(--border);display:flex;flex-direction:column;z-index:10}
.sidebar-top{padding:16px 16px 8px;border-bottom:1px solid var(--border)}
.sidebar-top .dev-name{font-size:.85rem;font-weight:600;color:var(--primary);white-space:nowrap;overflow:hidden;text-overflow:ellipsis}
.sidebar-top .dev-id{font-size:.65rem;color:var(--muted-subtle);margin-top:2px}
.nav{flex:1;padding:8px 0;overflow-y:auto}
.nav-item{display:flex;align-items:center;gap:8px;padding:10px 16px;cursor:pointer;font-size:.82rem;color:var(--muted);border-left:3px solid transparent;user-select:none}
.nav-item:hover{color:var(--text);background:var(--surface-2)}
.nav-item.active{color:var(--primary);border-left-color:var(--primary);font-weight:600;background:var(--primary-focus)}
.sidebar-bottom{padding:12px 16px;border-top:1px solid var(--border);font-size:.7rem;color:var(--muted-subtle)}
.main{flex:1;margin-left:var(--sidebar-w);display:flex;flex-direction:column;min-height:100vh}
.stats-header{background:var(--surface);border-bottom:1px solid var(--border);padding:5px 20px;display:flex;align-items:center;gap:6px}
.stats-header .back-link{color:var(--primary);text-decoration:none;font-size:.95rem;margin-right:2px}
.stats-header .stat{flex:1;background:var(--surface-2);border:1px solid var(--border);border-radius:8px;padding:5px 2px;text-align:center}
.stats-header .stat-value{font-size:.82rem;font-weight:700;color:var(--primary)}
.stats-header .stat-label{font-size:.52rem;color:var(--muted-subtle);text-transform:uppercase;letter-spacing:.03em;margin-top:1px}
.content{flex:1;padding:20px;max-width:480px;width:100%;margin:0 auto}
.footer-bar{display:flex;align-items:center;gap:16px;padding:8px 20px;border-top:1px solid var(--border);background:var(--surface);font-size:.75rem;color:var(--muted-subtle);flex-wrap:wrap;justify-content:flex-end}
.fb-dot{width:7px;height:7px;border-radius:50%;flex-shrink:0}
.fb-dot.online{background:var(--success)}
.fb-dot.offline{background:var(--danger)}
.fb-sep{color:var(--border);user-select:none}
h1{font-size:1.1rem;color:var(--primary);margin-bottom:12px;text-align:center}
.hero{text-align:center;padding:16px 0 8px}
.rain-value{font-size:4rem;font-weight:700;line-height:1}
.rain-value.dry{color:var(--success)}
.rain-value.light{color:var(--warn)}
.rain-value.wet{color:var(--primary)}
.rain-value.heavy{color:var(--danger)}
.badge{display:inline-block;padding:4px 14px;border-radius:9999px;font-size:.78rem;font-weight:600;margin-top:8px}
.badge.dry{background:rgba(22,163,74,0.12);color:var(--success)}
.badge.light{background:rgba(245,158,11,0.12);color:var(--warn)}
.badge.wet{background:var(--primary-focus);color:var(--primary)}
.badge.heavy{background:rgba(220,38,38,0.12);color:var(--danger)}
.dig-pill{display:inline-flex;align-items:center;gap:5px;margin-top:10px;padding:4px 14px;border-radius:9999px;font-size:.78rem;font-weight:600}
.dig-pill.wet{background:var(--primary-focus);color:var(--primary)}
.dig-pill.dry{background:rgba(22,163,74,0.12);color:var(--success)}
.card{background:var(--surface);border:1px solid var(--border);border-radius:12px;padding:16px;margin-top:16px}
.row{display:flex;justify-content:space-between;align-items:center;padding:9px 0;border-bottom:1px solid var(--border)}
.row:last-child{border-bottom:none}
.label{color:var(--muted-subtle);font-size:.82rem}
.value{font-weight:600;font-size:.82rem;color:var(--text)}
.value.green{color:var(--success)}
input[type=text]{padding:6px 10px;border-radius:8px;border:1px solid var(--border);background:var(--surface);color:var(--text);font-size:.82rem;outline:none;width:100%}
input[type=text]:focus{border-color:var(--primary)}
.btn{padding:6px 14px;border-radius:8px;border:1px solid var(--primary);background:var(--surface);color:var(--primary);font-size:.82rem;font-weight:500;cursor:pointer}
.btn:active{transform:scale(.97)}
.btn-primary{background:var(--primary);color:#fff;border-color:var(--primary)}
.btn-danger{border-color:var(--danger);color:var(--danger)}
.footer{text-align:center;margin-top:12px;font-size:.72rem;color:var(--muted-subtle)}
.loading{opacity:.5;pointer-events:none}
.section{display:none}
.section.active{display:block}
.collapsible{cursor:pointer;user-select:none;display:flex;align-items:center;justify-content:space-between}
.collapsible .arrow{font-size:.65rem;transition:transform .2s}
.collapsible.open .arrow{transform:rotate(90deg)}
.collapse-body{display:none}
.collapse-body.open{display:block}
@media(max-width:600px){.sidebar{width:48px}.sidebar-top .dev-name,.sidebar-top .dev-id,.sidebar-bottom{display:none}.nav-item{justify-content:center;padding:10px 4px}.nav-item span{display:none}.nav-item span:first-child{display:inline;font-size:1.1rem}.main{margin-left:48px}.stats-header{padding:5px 10px}.content{padding:12px}.footer-bar{padding:6px 12px;font-size:.7rem;gap:8px}}
</style>
</head>
<body>
<div class="sidebar">
<div class="sidebar-top">
<div class="dev-name" id="sbName">Sensor de Chuva</div>
<div class="dev-id" id="sbId">-</div>
</div>
<div class="nav">
<div class="nav-item active" data-section="home" onclick="showSection('home')"><span>🏠</span><span>Home</span></div>
<div class="nav-item" data-section="propriedades" onclick="showSection('propriedades')"><span>📋</span><span>Propriedades</span></div>
<div class="nav-item" data-section="config" onclick="showSection('config')"><span>⚙</span><span>Configurações</span></div>
</div>
<div class="sidebar-bottom"><span id="sbVersion">...</span></div>
</div>
<div class="main">
<div class="stats-header">
<a id="backBtn" href="#" class="back-link" style="display:none">&larr;</a>
<div class="stat"><div class="stat-value" id="rxVal">0</div><div class="stat-label">RX</div></div>
<div class="stat"><div class="stat-value" id="txVal">0</div><div class="stat-label">TX</div></div>
<div class="stat"><div class="stat-value" id="memVal">-</div><div class="stat-label">Mem</div></div>
<div class="stat"><div class="stat-value" id="nivelVal">0%</div><div class="stat-label">Nível</div></div>
</div>
<div class="content">
<div class="section active" id="secHome">
<div class="hero">
<div class="rain-value dry" id="rainValue">—</div>
<div><span class="badge dry" id="rainBadge">—</span></div>
<div class="dig-pill dry" id="digPill">—</div>
</div>
<div class="card">
<div class="collapsible open" onclick="toggleDetails()">
<span style="font-size:.85rem;font-weight:600;color:var(--primary)">Detalhes</span>
<span class="arrow" id="detArrow">&#9654;</span>
</div>
<div class="collapse-body open" id="detBody">
<div class="row"><span class="label">Digital</span><span class="value" id="detDigital">—</span></div>
<div class="row"><span class="label">Bateria</span><span class="value" id="detBattery">—</span></div>
<div class="row"><span class="label">RSSI</span><span class="value" id="detRssi">—</span></div>
<div class="row"><span class="label">IP</span><span class="value" id="detIp">—</span></div>
<div class="row"><span class="label">Versão</span><span class="value" id="detVersion">—</span></div>
<div class="row"><span class="label">Slot</span><span class="value" id="detSlot">—</span></div>
<div class="row"><span class="label">Gateway</span><span class="value" id="detGateway">—</span></div>
</div>
</div>
<div class="footer" id="footer">Carregando...</div>
</div>
<div class="section" id="secPropriedades">
<h1>Propriedades</h1>
<div class="card">
<div class="row"><span class="label">Device</span><span class="value" id="propId">—</span></div>
<div class="row"><span class="label">IP</span><span class="value" id="propIp">—</span></div>
<div class="row"><span class="label">RSSI</span><span class="value" id="propRssi">—</span></div>
<div class="row"><span class="label">Versão</span><span class="value" id="propVersion">—</span></div>
<div class="row"><span class="label">Slot</span><span class="value" id="propSlot">—</span></div>
</div>
</div>
<div class="section" id="secConfig">
<h1>Configuração</h1>
<div class="card">
<div class="row"><span class="label">Nome</span><input type="text" id="deviceNameInput" maxlength="47" style="width:160px"></div>
<div style="display:flex;gap:8px;justify-content:center;margin-top:10px">
<button class="btn btn-primary" onclick="saveName()">Salvar</button>
<button class="btn btn-danger" onclick="restartDevice()">Reiniciar</button>
</div>
</div>
</div>
</div>
<div class="footer-bar">
<span style="display:flex;align-items:center;gap:4px"><span class="fb-dot" id="fbDot"></span><span id="fbGateway">-</span></span>
<span class="fb-sep">|</span>
<span id="fbTime">--:--</span>
<span class="fb-sep">|</span>
<span id="fbUptime">0m</span>
</div>
</div>
<script>
const p=new URLSearchParams(window.location.search);
const gw=p.get('from');
if(gw){document.getElementById('backBtn').href='http://'+gw;document.getElementById('backBtn').style.display='inline'}
const rxVal=document.getElementById('rxVal');
const txVal=document.getElementById('txVal');
const memVal=document.getElementById('memVal');
const nivelVal=document.getElementById('nivelVal');
const rainValue=document.getElementById('rainValue');
const rainBadge=document.getElementById('rainBadge');
const digPill=document.getElementById('digPill');
const footerEl=document.getElementById('footer');
const fbDot=document.getElementById('fbDot');
const fbGateway=document.getElementById('fbGateway');
const fbTime=document.getElementById('fbTime');
const fbUptime=document.getElementById('fbUptime');
let currentSection='home';
function showSection(s){document.querySelectorAll('.section').forEach(function(el){el.classList.remove('active')});document.getElementById('sec'+(s.charAt(0).toUpperCase()+s.slice(1))).classList.add('active');document.querySelectorAll('.nav-item[data-section]').forEach(function(el){el.classList.remove('active')});document.querySelector('.nav-item[data-section="'+s+'"]').classList.add('active');currentSection=s}
function toggleDetails(){var c=document.getElementById('detBody');var a=document.getElementById('detArrow');c.classList.toggle('open');a.parentElement.classList.toggle('open')}
async function saveName(){let nm=document.getElementById('deviceNameInput').value.trim();if(!nm)return;try{await fetch('/api/settings',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify({device_name:nm})});fetchSettings()}catch(e){footerEl.textContent='Erro: '+e.message}}
async function restartDevice(){if(!confirm('Reiniciar?'))return;try{await fetch('/api/restart',{method:'POST'});footerEl.textContent='Reiniciando...'}catch(e){}}
function rainClass(lvl){if(lvl<=10)return'dry';if(lvl<=40)return'light';if(lvl<=70)return'wet';return'heavy'}
function rainLabel(lvl){if(lvl<=10)return'SECO';if(lvl<=40)return'CHUVISCO';if(lvl<=70)return'CHUVENDO';return'CHUVA FORTE'}
async function fetchState(){try{let r=await fetch('/api/state');let d=await r.json();
let lvl=d.rain_level||0;let cls=rainClass(lvl);
rainValue.textContent=lvl+'%';rainValue.className='rain-value '+cls;
rainBadge.textContent=rainLabel(lvl);rainBadge.className='badge '+cls;
digPill.textContent=d.rain_digital?'\u2614 CHUVA':'\u2600\ufe0f SECO';
digPill.className='dig-pill '+(d.rain_digital?'wet':'dry');
nivelVal.textContent=lvl+'%';
rxVal.textContent=d.rx_count||0;txVal.textContent=d.tx_count||0;
memVal.textContent=d.free_heap||0;
document.getElementById('detDigital').textContent=d.rain_digital?'Chuva':'Seco';
document.getElementById('detBattery').textContent=(d.battery||0)+'%';
document.getElementById('detRssi').textContent=(d.rssi||0)+' dBm';
document.getElementById('detIp').textContent=d.ip||'-';
document.getElementById('detVersion').textContent=d.fw_version||'-';
document.getElementById('detSlot').textContent=d.slot!==undefined&&d.slot!==null?d.slot:'-';
document.getElementById('detGateway').textContent=d.gateway_connected?'Online':'Offline';
document.getElementById('detGateway').className='value'+(d.gateway_connected?' green':'');
document.getElementById('propId').textContent=d.device_id||'-';
document.getElementById('propIp').textContent=d.ip||'-';
document.getElementById('propRssi').textContent=(d.rssi||0)+' dBm';
document.getElementById('propVersion').textContent=d.fw_version||'-';
document.getElementById('propSlot').textContent=d.slot!==undefined&&d.slot!==null?d.slot:'-';
let name=d.device_name||'Sensor de Chuva';
document.getElementById('pageTitle').textContent=name;
document.getElementById('sbName').textContent=name;
document.getElementById('sbId').textContent=d.device_id||'';
const ve=document.getElementById('sbVersion');if(ve&&d.fw_version)ve.textContent=d.fw_version;
let u=d.uptime_s||0;let dd=Math.floor(u/86400);let hh=Math.floor((u%86400)/3600);let mm=Math.floor((u%3600)/60);
fbDot.className='fb-dot'+(d.gateway_connected?' online':' offline');
fbGateway.textContent=d.gateway_connected?'Online':'Offline';
fbUptime.textContent=(dd?dd+'d ':'')+(hh?hh+'h ':'')+mm+'m';
fbTime.textContent=new Date().toLocaleTimeString('pt-BR',{hour:'2-digit',minute:'2-digit'});
let ls=d.last_send_s;let lastSend=ls==null?'':ls<60?ls+'s':ls<3600?Math.floor(ls/60)+'m':Math.floor(ls/3600)+'h';
footerEl.textContent=d.device_id+(lastSend?' \u00B7 \u00FAltimo envio: '+lastSend:'');
}catch(e){footerEl.textContent='Erro: '+e.message}}
async function fetchSettings(){try{let r=await fetch('/api/settings');let d=await r.json();document.getElementById('deviceNameInput').value=d.device_name||''}catch(e){}}
setInterval(function(){fetchState()},3000);
fetchState();fetchSettings();
</script>
</body>
</html>
)rawliteral";

static const char PAGE_DOCS[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="pt-BR">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>API Docs</title>
<style>
:root{--bg:#f4f4f4;--surface:#fff;--text:#1f2937;--muted:#6b7280;--muted-subtle:#9ca3af;--primary:#5e6ad2;--border:#e5e7eb;--success:#16a34a;--danger:#dc2626}
*{margin:0;padding:0;box-sizing:border-box}
body{font-family:-apple-system,system-ui,sans-serif;background:var(--bg);color:var(--text);padding:16px;max-width:640px;margin:0 auto}
h1{color:var(--primary);font-size:1.2rem;margin-bottom:16px}
.endpoint{background:var(--surface);border:1px solid var(--border);border-radius:10px;padding:12px;margin-bottom:8px}
.head{display:flex;align-items:center;gap:8px;margin-bottom:4px}
.method{font-weight:700;font-size:.75rem;padding:2px 8px;border-radius:4px}
.get{background:#dcfce7;color:var(--success)}
.post{background:#fef2f2;color:var(--danger)}
.path{font-family:monospace;font-size:.85rem;color:var(--text)}
.desc{color:var(--muted);font-size:.78rem;margin-top:4px}
</style>
</head>
<body>
<h1>Sensor de Chuva - API</h1>
<div class="endpoint"><div class="head"><span class="method get">GET</span><span class="path">/</span></div><div class="desc">Dashboard</div></div>
<div class="endpoint"><div class="head"><span class="method get">GET</span><span class="path">/docs</span></div><div class="desc">Esta página</div></div>
<div class="endpoint"><div class="head"><span class="method get">GET</span><span class="path">/api/state</span></div><div class="desc">Estado do dispositivo</div></div>
<div class="endpoint"><div class="head"><span class="method get">GET</span><span class="path">/api/pin?gpio=N</span></div><div class="desc">Ler GPIO</div></div>
<div class="endpoint"><div class="head"><span class="method post">POST</span><span class="path">/api/pin</span></div><div class="desc">Setar GPIO {"gpio":N,"state":0|1}</div></div>
<div class="endpoint"><div class="head"><span class="method get">GET</span><span class="path">/api/settings</span></div><div class="desc">Configurações</div></div>
<div class="endpoint"><div class="head"><span class="method post">POST</span><span class="path">/api/settings</span></div><div class="desc">Atualizar {"device_name":"..."}</div></div>
<div class="endpoint"><div class="head"><span class="method post">POST</span><span class="path">/api/ota</span></div><div class="desc">Upload firmware</div></div>
<p style="margin-top:16px"><a href="/" style="color:var(--primary)">&larr; Dashboard</a></p>
</body>
</html>
)rawliteral";

static const char PAGE_WIFI_CONFIG[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="pt-BR">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>Configurar WiFi</title>
<style>
:root{--bg:#f4f4f4;--surface:#fff;--text:#1f2937;--muted:#6b7280;--muted-subtle:#9ca3af;--primary:#5e6ad2;--border:#e5e7eb;--success:#16a34a;--danger:#dc2626}
*{margin:0;padding:0;box-sizing:border-box}
body{font-family:-apple-system,system-ui,sans-serif;background:var(--bg);color:var(--text);min-height:100vh;display:flex;flex-direction:column;align-items:center;justify-content:center;padding:20px}
.card{background:var(--surface);border:1px solid var(--border);border-radius:12px;padding:24px;width:100%;max-width:380px}
h1{text-align:center;font-size:1.2rem;color:var(--primary);margin-bottom:16px}
label{display:block;color:var(--muted);font-size:.82rem;margin-top:12px;margin-bottom:4px}
input{width:100%;padding:8px 12px;border:1px solid var(--border);border-radius:8px;background:var(--surface);color:var(--text);font-size:.85rem;outline:none}
input:focus{border-color:var(--primary)}
.btn{width:100%;padding:10px;border:none;border-radius:8px;background:var(--primary);color:#fff;font-size:.9rem;font-weight:500;cursor:pointer;margin-top:20px}
.hint{color:var(--muted-subtle);font-size:.72rem;margin-top:4px}
.msg{margin-top:12px;padding:8px;border-radius:8px;font-size:.82rem;text-align:center}
.msg.ok{background:#dcfce7;color:var(--success)}
.msg.err{background:#fef2f2;color:var(--danger)}
</style>
</head>
<body>
<div class="card">
<h1>Configurar Rede WiFi</h1>
<form id="wifiForm" onsubmit="return submitForm()">
<label for="ssid">SSID</label>
<input type="text" id="ssid" placeholder="Nome da rede WiFi" required>
<label for="password">Senha</label>
<input type="password" id="password" placeholder="Senha">
<label for="devName">Nome do Dispositivo</label>
<input type="text" id="devName" placeholder="Ex: Chuva">
<button type="submit" class="btn" id="submitBtn">Conectar</button>
</form>
<div id="msg" class="msg" style="display:none"></div>
</div>
<script>
async function loadStatus(){try{let r=await fetch('/api/wifi');let d=await r.json();document.getElementById('devName').value=d.device_name||''}catch(e){}}
function showMsg(t,c){let m=document.getElementById('msg');m.textContent=t;m.className='msg '+c;m.style.display='block'}
function loading(v){document.getElementById('submitBtn').style.opacity=v?0.5:1;document.getElementById('submitBtn').disabled=v}
async function submitForm(){let ssid=document.getElementById('ssid').value.trim();if(!ssid){showMsg('Informe o SSID','err');return false}
let pass=document.getElementById('password').value;let name=document.getElementById('devName').value.trim();loading(true);
let body={ssid:ssid,password:pass};if(name)body.device_name=name;
try{let r=await fetch('/api/wifi',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify(body)});let d=await r.json();
if(d.status==='ok'){showMsg('Conectando...','ok');setTimeout(function(){showMsg('AP reativado se falhar','ok')},2000)}else{showMsg('Erro: '+d.error,'err');loading(false)}}
catch(e){showMsg('Erro: '+e.message,'err');loading(false)}return false}
loadStatus();
</script>
</body>
</html>
)rawliteral";
