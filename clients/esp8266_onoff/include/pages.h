#pragma once

#include <Arduino.h>

static const char PAGE_DASHBOARD[] PROGMEM = R"=====(
<!DOCTYPE html>
<html lang="pt-BR">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title id="pageTitle">OnOff</title>
<style>
:root{--bg:#f4f4f4;--surface:#fff;--surface-2:#f9fafb;--text:#1f2937;--muted:#6b7280;--muted-subtle:#9ca3af;--primary:#5e6ad2;--primary-strong:#828fff;--primary-focus:#eef0ff;--border:#e5e7eb;--border-strong:#d1d5db;--success:#16a34a;--danger:#dc2626;--info:#2563eb}
*{margin:0;padding:0;box-sizing:border-box}
body{font-family:-apple-system,system-ui,BlinkMacSystemFont,sans-serif;background:var(--bg);color:var(--text);min-height:100vh;padding:20px;display:flex;flex-direction:column;align-items:center}
.card{background:var(--surface);border:1px solid var(--border);border-radius:12px;padding:20px;width:100%;max-width:400px}
h1{text-align:center;font-size:1.2rem;color:var(--primary);margin-bottom:4px}
.hero{text-align:center;padding:16px 0 8px}
.relay-btn{width:88px;height:88px;border-radius:50%;border:2px solid var(--border);font-size:2.5rem;cursor:pointer;transition:all .25s;background:var(--surface);color:var(--muted-subtle);display:inline-flex;align-items:center;justify-content:center}
.relay-btn:hover{border-color:var(--primary)}
.relay-btn.on{background:var(--primary);border-color:var(--primary-strong);color:#fff;box-shadow:0 0 0 4px var(--primary-focus)}
.badge{display:inline-block;padding:3px 14px;border-radius:9999px;font-size:.75rem;font-weight:600}
.badge.on{background:#dcfce7;color:var(--success)}
.badge.off{background:#fef2f2;color:var(--danger)}
.summary{display:grid;grid-template-columns:repeat(3,1fr);gap:8px;margin:12px 0 4px}
.metric{background:var(--surface-2);border:1px solid var(--border);border-radius:8px;padding:10px;text-align:center}
.metric-label{font-size:.65rem;text-transform:uppercase;letter-spacing:.4px;color:var(--muted-subtle);font-weight:500}
.metric-value{font-size:.9rem;font-weight:600;color:var(--text);margin-top:2px}
.metric-value.green{color:var(--success)}
.metric-value.red{color:var(--danger)}
.expand-header{display:flex;justify-content:space-between;align-items:center;padding:10px 0;cursor:pointer;color:var(--muted);font-size:.82rem;border-bottom:1px solid var(--border);user-select:none;margin-top:8px}
.expand-header:hover{color:var(--primary)}
.expand-icon{font-size:.8rem;transition:transform .2s}
.expand-icon.open{transform:rotate(90deg)}
.details{overflow:hidden;max-height:0;transition:max-height .25s ease}
.details.open{max-height:400px}
.row{display:flex;justify-content:space-between;align-items:center;padding:9px 0;border-bottom:1px solid var(--border)}
.row:last-child{border-bottom:none}
.label{color:var(--muted-subtle);font-size:.82rem}
.value{font-weight:600;font-size:.82rem;color:var(--text)}
.value.green{color:var(--success)}
input[type=text]{padding:6px 10px;border-radius:8px;border:1px solid var(--border);background:var(--surface);color:var(--text);font-size:.82rem;width:100%;outline:none}
input[type=text]:focus{border-color:var(--primary)}
select{padding:6px 8px;border-radius:8px;border:1px solid var(--border);background:var(--surface);color:var(--text);font-size:.82rem;outline:none}
.btn{padding:6px 14px;border-radius:8px;border:1px solid var(--primary);background:var(--surface);color:var(--primary);font-size:.82rem;font-weight:500;cursor:pointer}
.btn:active{transform:scale(.97)}
.btn-primary{background:var(--primary);color:#fff;border-color:var(--primary)}
.btn-danger{border-color:var(--danger);color:var(--danger)}
.btn-sm{padding:4px 10px;font-size:.75rem}
.footer{text-align:center;margin-top:12px;font-size:.72rem;color:var(--muted-subtle)}
.loading{opacity:.5;pointer-events:none}
.timer-add{display:flex;gap:4px;align-items:center;margin-top:8px;flex-wrap:wrap}
</style>
</head>
<body>
<div class="card" id="app">
<h1 id="deviceName">OnOff</h1>
<div class="hero">
<button class="relay-btn" id="relayBtn" onclick="toggleRelay()">&#9889;</button>
<div style="margin-top:8px"><span class="badge off" id="relayBadge">DESLIGADO</span></div>
</div>
<div class="summary">
<div class="metric"><div class="metric-label">Gateway</div><div class="metric-value" id="gwMetric">-</div></div>
<div class="metric"><div class="metric-label">RSSI</div><div class="metric-value" id="rssiMetric">-</div></div>
<div class="metric"><div class="metric-label">Uptime</div><div class="metric-value" id="uptimeMetric">-</div></div>
</div>
<div class="expand-header" onclick="toggleDetails()"><span>Detalhes</span><span class="expand-icon" id="expandIcon">&#9654;</span></div>
<div class="details" id="details">
<div class="row"><span class="label">Alexa</span><span class="value" id="alexaStatus">-</span></div>
<div class="row"><span class="label">IP</span><span class="value" id="ipStatus">-</span></div>
<div class="row"><span class="label">Bateria</span><span class="value" id="batteryStatus">-</span></div>
<div class="row"><span class="label">Versão</span><span class="value" id="fwVersion">-</span></div>
<div class="row"><span class="label">Slot</span><span class="value" id="slotStatus">-</span></div>
</div>
<div class="expand-header" onclick="toggleSettings()"><span>Configuração</span><span class="expand-icon" id="expandIcon2">&#9654;</span></div>
<div class="details" id="settingsDetails">
<div class="row"><span class="label">Nome</span><input type="text" id="deviceNameInput" maxlength="47" style="width:160px"></div>
<div class="row"><span class="label">GPIO relé</span><select id="relayPinSelect"></select></div>
<div class="row"><span class="label">GPIO botao</span><select id="buttonPinSelect"></select></div>
<div class="row"><span class="label">LED</span><label style="font-size:.82rem;color:var(--muted-subtle)"><input type="checkbox" id="ledEnabledCheck" onchange="savePins()"> habilitado</label></div>
<div class="row"><span class="label">Iniciar relé</span><select id="startupModeSelect" onchange="savePins()">
<option value="0">OFF</option>
<option value="1">ON</option>
<option value="2">Último</option>
</select></div>
<div class="row"><span class="label">Pulse</span><label style="font-size:.82rem;color:var(--muted-subtle)"><input type="checkbox" id="pulseEnabledCheck" onchange="savePulse()"> auto-OFF</label></div>
<div class="row"><span class="label">Duração</span>
<span style="display:flex;gap:4px;align-items:center">
<input type="number" id="pulseDurationInput" min="1" max="1440" style="width:60px;padding:4px 6px;border-radius:8px;border:1px solid var(--border);font-size:.82rem;outline:none;background:var(--surface);color:var(--text)">
<span style="color:var(--muted-subtle);font-size:.75rem">min</span>
</span>
</div>
<div style="display:flex;gap:8px;justify-content:center;margin-top:10px">
<button class="btn btn-primary" onclick="savePins()">Salvar</button>
<button class="btn btn-danger" onclick="restartDevice()">Reiniciar</button>
</div>
</div>
<div class="expand-header" onclick="toggleTimers()"><span>Timer</span><span class="expand-icon" id="expandIcon3">&#9654;</span></div>
<div class="details" id="timerDetails">
<div class="row"><span class="label">Próximo</span><span class="value" id="nextTimer">-</span></div>
<div id="timerList"></div>
<div class="timer-add">
<select id="timerHour" style="width:56px"></select><span style="color:var(--muted-subtle)">:</span>
<select id="timerMin" style="width:56px"></select>
<select id="timerAction" style="width:62px"><option value="0">OFF</option><option value="1">ON</option></select>
<select id="timerDays" style="width:74px"><option value="0">Todos</option><option value="127">Semana</option><option value="64">Fim de sem</option></select>
<button class="btn btn-primary btn-sm" onclick="addTimer()">+</button>
</div>
</div>
<div class="footer" id="footer">Carregando...</div>
</div>
<script>
const btn=document.getElementById('relayBtn');
const badge=document.getElementById('relayBadge');
const gwMetric=document.getElementById('gwMetric');
const rssiMetric=document.getElementById('rssiMetric');
const uptimeMetric=document.getElementById('uptimeMetric');
const alexaEl=document.getElementById('alexaStatus');
const ipEl=document.getElementById('ipStatus');
const batteryEl=document.getElementById('batteryStatus');
const fwEl=document.getElementById('fwVersion');
const slotEl=document.getElementById('slotStatus');
const footerEl=document.getElementById('footer');
const timerList=document.getElementById('timerList');
const nextTimerEl=document.getElementById('nextTimer');
let loading=false,expanded=false,expanded2=false,expanded3=false;
function toggleDetails(){expanded=!expanded;document.getElementById('details').classList.toggle('open',expanded);document.getElementById('expandIcon').classList.toggle('open',expanded)}
function toggleSettings(){expanded2=!expanded2;document.getElementById('settingsDetails').classList.toggle('open',expanded2);document.getElementById('expandIcon2').classList.toggle('open',expanded2)}
function toggleTimers(){expanded3=!expanded3;document.getElementById('timerDetails').classList.toggle('open',expanded3);document.getElementById('expandIcon3').classList.toggle('open',expanded3);if(expanded3)fetchTimers()}
for(let i=0;i<24;i++){let o=document.createElement('option');o.value=i;o.text=('0'+i).slice(-2);document.getElementById('timerHour').appendChild(o)}
for(let i=0;i<60;i++){let o=document.createElement('option');o.value=i;o.text=('0'+i).slice(-2);document.getElementById('timerMin').appendChild(o)}
async function restartDevice(){if(!confirm('Reiniciar?'))return;try{await fetch('/api/restart',{method:'POST'});footerEl.textContent='Reiniciando...'}catch(e){}}
async function savePins(){let nm=document.getElementById('deviceNameInput').value.trim();let rp=document.getElementById('relayPinSelect').value;let bp=document.getElementById('buttonPinSelect').value;
let body={relay_pin:parseInt(rp),button_pin:parseInt(bp),led_enabled:document.getElementById('ledEnabledCheck').checked,startup_mode:parseInt(document.getElementById('startupModeSelect').value)};if(nm)body.device_name=nm;
try{await fetch('/api/settings',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify(body)});fetchSettings()}catch(e){footerEl.textContent='Erro: '+e.message}}
async function savePulse(){let en=document.getElementById('pulseEnabledCheck').checked;let dur=parseInt(document.getElementById('pulseDurationInput').value)||60;
try{await fetch('/api/pulse',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify({enabled:en,duration_minutes:dur})})}catch(e){}}
async function fetchPulse(){try{let r=await fetch('/api/pulse');let d=await r.json();
document.getElementById('pulseEnabledCheck').checked=d.enabled;
document.getElementById('pulseDurationInput').value=d.duration_minutes;
let pe=document.getElementById('pulseEnabledCheck');
pe.checked=d.enabled;
}catch(e){}}
async function fetchState(){try{let r=await fetch('/api/state');let d=await r.json();
const on=d.state;btn.classList.toggle('on',on);badge.textContent=on?'LIGADO':'DESLIGADO';badge.className='badge '+(on?'on':'off');
gwMetric.textContent=d.gateway_connected?'Conectado':'Offline';gwMetric.className='metric-value'+(d.gateway_connected?' green':' red');
rssiMetric.textContent=(d.rssi||'-')+' dBm';
let u=d.uptime_s||0;let dd=Math.floor(u/86400);let hh=Math.floor((u%86400)/3600);let mm=Math.floor((u%3600)/60);
uptimeMetric.textContent=(dd?dd+'d ':'')+(hh?hh+'h ':'')+mm+'m';
alexaEl.textContent=d.alexa_connected?'Conectado':'-';alexaEl.className='value'+(d.alexa_connected?' green':'');
ipEl.textContent=d.ip||'-';batteryEl.textContent=(d.battery||0)+'%';fwEl.textContent=d.fw_version||'-';
slotEl.textContent=d.slot!==undefined&&d.slot!==null?d.slot:'-';
document.getElementById('deviceName').textContent=d.device_name;document.getElementById('pageTitle').textContent=d.device_name;
footerEl.textContent=d.device_id+(d.last_send_s?' Último envio: '+d.last_send_s+'s ago':'');
}catch(e){footerEl.textContent='Erro: '+e.message}}
async function fetchSettings(){try{let r=await fetch('/api/settings');let d=await r.json();
document.getElementById('deviceNameInput').value=d.device_name;
document.getElementById('ledEnabledCheck').checked=d.led_enabled;
document.getElementById('startupModeSelect').value=d.startup_mode;
let rps=document.getElementById('relayPinSelect');let bps=document.getElementById('buttonPinSelect');rps.innerHTML='';bps.innerHTML='';
d.available_pins.forEach(function(p){
let ro=document.createElement('option');ro.value=p;ro.text='GPIO '+p;if(p===d.relay_pin)ro.selected=true;rps.appendChild(ro);
let bo=document.createElement('option');bo.value=p;bo.text='GPIO '+p;if(p===d.button_pin)bo.selected=true;bps.appendChild(bo)})}catch(e){}}
async function toggleRelay(){if(loading)return;loading=true;btn.classList.add('loading');
try{let r=await fetch('/api/relay',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify({state:!btn.classList.contains('on')})});
let d=await r.json();const on=d.state;btn.classList.toggle('on',on);badge.textContent=on?'LIGADO':'DESLIGADO';badge.className='badge '+(on?'on':'off');
}catch(e){footerEl.textContent='Erro: '+e.message}finally{loading=false;btn.classList.remove('loading')}}
async function fetchTimers(){try{let r=await fetch('/api/timers');let d=await r.json();timerList.innerHTML='';
if(d.timers&&d.timers.length){d.timers.forEach(function(t,i){
let div=document.createElement('div');div.className='row';
let lbl=document.createElement('span');lbl.className='label';lbl.textContent=('0'+t.hour).slice(-2)+':'+('0'+t.minute).slice(-2)+' '+(t.action?'ON':'OFF');
let cb=document.createElement('input');cb.type='checkbox';cb.checked=t.enabled;cb.onchange=function(){t.enabled=cb.checked;fetch('/api/timers',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify({index:i,...t})})};
div.appendChild(lbl);div.appendChild(cb);timerList.appendChild(div)})}else{timerList.innerHTML='<div class="row"><span class="label">Nenhum timer</span></div>'}
let nr=await fetch('/api/timer/next');let nd=await nr.json();
nextTimerEl.textContent=nd.has_next?new Date(nd.next_epoch*1000).toLocaleString('pt-BR',{hour:'2-digit',minute:'2-digit'}):'-';
}catch(e){}}
async function addTimer(){let h=document.getElementById('timerHour').value;let m=document.getElementById('timerMin').value;let a=document.getElementById('timerAction').value;let d=document.getElementById('timerDays').value;
try{await fetch('/api/timers',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify({hour:parseInt(h),minute:parseInt(m),action:parseInt(a),days_mask:parseInt(d),enabled:true})});fetchTimers()}catch(e){}}
setInterval(fetchState,3000);fetchState();fetchSettings();fetchTimers();fetchPulse();
</script>
</body>
</html>
)=====";

static const char PAGE_DOCS[] PROGMEM = R"=====(
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
table{width:100%;font-size:.75rem;border-collapse:collapse;margin-top:4px}
th{text-align:left;color:var(--muted-subtle);padding:2px 4px;border-bottom:1px solid var(--border)}
td{padding:2px 4px;border-bottom:1px solid var(--border);color:var(--text);font-family:monospace}
</style>
</head>
<body>
<h1>API OnOff</h1>
<div class="endpoint">
  <div class="head"><span class="method get">GET</span><span class="path">/</span></div>
  <div class="desc">Dashboard</div>
</div>
<div class="endpoint">
  <div class="head"><span class="method get">GET</span><span class="path">/api/state</span></div>
  <div class="desc">Estado completo do dispositivo</div>
  <table><tr><th>Chave</th><th>Tipo</th></tr>
  <tr><td>state</td><td>bool</td></tr>
  <tr><td>button</td><td>bool</td></tr>
  <tr><td>battery</td><td>int</td></tr>
  <tr><td>device_id</td><td>string</td></tr>
  <tr><td>device_name</td><td>string</td></tr>
  <tr><td>gateway_connected</td><td>bool</td></tr>
  <tr><td>paired</td><td>bool</td></tr>
  <tr><td>ip</td><td>string</td></tr>
  <tr><td>rssi</td><td>int</td></tr>
  <tr><td>uptime_s</td><td>int</td></tr>
  <tr><td>slot</td><td>int</td></tr>
  <tr><td>alexa_connected</td><td>bool</td></tr>
  <tr><td>fw_version</td><td>string</td></tr>
  <tr><td>last_send_s</td><td>int</td></tr>
</table>
</div>
<div class="endpoint">
  <div class="head"><span class="method get">GET</span><span class="path">/api/relay</span></div>
  <div class="desc">Estado do relé <code>{"state":bool}</code></div>
</div>
<div class="endpoint">
  <div class="head"><span class="method post">POST</span><span class="path">/api/relay</span></div>
  <div class="desc">Controla relé <code>{"state":bool}</code></div>
</div>
<div class="endpoint">
  <div class="head"><span class="method get">GET</span><span class="path">/api/pin?gpio=N</span></div>
  <div class="desc">Leitura digital de um GPIO</div>
</div>
<div class="endpoint">
  <div class="head"><span class="method post">POST</span><span class="path">/api/pin</span></div>
  <div class="desc">Escrita digital <code>{"gpio":N,"state":0|1}</code></div>
</div>
<div class="endpoint">
  <div class="head"><span class="method get">GET</span><span class="path">/api/settings</span></div>
  <div class="desc">Configurações do dispositivo</div>
</div>
<div class="endpoint">
  <div class="head"><span class="method post">POST</span><span class="path">/api/settings</span></div>
  <div class="desc">Altera nome/pinos</div>
</div>
<div class="endpoint">
  <div class="head"><span class="method get">GET</span><span class="path">/api/timers</span></div>
  <div class="desc">Lista timers configurados</div>
</div>
<div class="endpoint">
  <div class="head"><span class="method post">POST</span><span class="path">/api/timers</span></div>
  <div class="desc">Atualiza timer</div>
</div>
<div class="endpoint">
  <div class="head"><span class="method get">GET</span><span class="path">/api/timer/next</span></div>
  <div class="desc">Próximo timer</div>
</div>
<div class="endpoint">
  <div class="head"><span class="method post">POST</span><span class="path">/api/restart</span></div>
  <div class="desc">Reinicia o dispositivo</div>
</div>
<div class="endpoint">
  <div class="head"><span class="method post">POST</span><span class="path">/api/ota</span></div>
  <div class="desc">OTA update (multipart)</div>
</div>
</body>
</html>
)=====";

static const char PAGE_WIFI_CONFIG[] PROGMEM = R"=====(
<!DOCTYPE html>
<html lang="pt-BR">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>Configurar WiFi</title>
<style>
:root{--bg:#f4f4f4;--surface:#fff;--surface-2:#f9fafb;--text:#1f2937;--muted:#6b7280;--muted-subtle:#9ca3af;--primary:#5e6ad2;--border:#e5e7eb;--success:#16a34a;--danger:#dc2626}
*{margin:0;padding:0;box-sizing:border-box}
body{font-family:-apple-system,system-ui,sans-serif;background:var(--bg);color:var(--text);min-height:100vh;display:flex;flex-direction:column;align-items:center;justify-content:center;padding:20px}
.card{background:var(--surface);border:1px solid var(--border);border-radius:12px;padding:24px;width:100%;max-width:380px}
h1{text-align:center;font-size:1.2rem;color:var(--primary);margin-bottom:16px}
label{display:block;color:var(--muted);font-size:.82rem;margin-top:12px;margin-bottom:4px}
input{width:100%;padding:8px 12px;border:1px solid var(--border);border-radius:8px;background:var(--surface);color:var(--text);font-size:.85rem;outline:none}
input:focus{border-color:var(--primary)}
.btn{width:100%;padding:10px;border:none;border-radius:8px;background:var(--primary);color:#fff;font-size:.9rem;font-weight:500;cursor:pointer;margin-top:20px}
.btn:active{transform:scale(.98)}
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
<input type="password" id="password" placeholder="Senha da rede WiFi">
<label for="devName">Nome do Dispositivo</label>
<input type="text" id="devName" placeholder="Ex: Sala">
<label for="repeaterMac">Repeater MAC (opcional)</label>
<input type="text" id="repeaterMac" placeholder="AA:BB:CC:DD:EE:FF">
<div class="hint">Deixe em branco para uso normal</div>
<button type="submit" class="btn" id="submitBtn">Conectar</button>
</form>
<div id="msg" class="msg" style="display:none"></div>
</div>
<script>
async function loadStatus(){try{let r=await fetch('/api/wifi');let d=await r.json();document.getElementById('devName').value=d.device_name||''}catch(e){}}
function showMsg(t,c){let m=document.getElementById('msg');m.textContent=t;m.className='msg '+c;m.style.display='block'}
function loading(v){document.getElementById('submitBtn').style.opacity=v?0.5:1;document.getElementById('submitBtn').disabled=v}
async function submitForm(){let ssid=document.getElementById('ssid').value.trim();if(!ssid){showMsg('Informe o SSID','err');return false}
let pass=document.getElementById('password').value;let name=document.getElementById('devName').value.trim();let rep=document.getElementById('repeaterMac').value.trim();loading(true);
let body={ssid:ssid,password:pass};if(name)body.device_name=name;if(rep)body.repeater_mac=rep;
try{let r=await fetch('/api/wifi',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify(body)});let d=await r.json();
if(d.status==='ok'){showMsg('Conectando a '+ssid+'...','ok');setTimeout(function(){showMsg('AP reativado se falhar.','ok')},2000)}else{showMsg('Erro: '+d.error,'err');loading(false)}}
catch(e){showMsg('Erro: '+e.message,'err');loading(false)}return false}
loadStatus();
</script>
</body>
</html>
)=====";
