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
*{margin:0;padding:0;box-sizing:border-box}
body{font-family:-apple-system,system-ui,BlinkMacSystemFont,sans-serif;background:#1a1a2e;color:#eee;min-height:100vh;display:flex;flex-direction:column;align-items:center;justify-content:center;padding:20px}
.card{background:#16213e;border-radius:16px;padding:24px;width:100%;max-width:380px;box-shadow:0 8px 32px rgba(0,0,0,.3)}
h1{text-align:center;font-size:1.3em;color:#e94560;margin-bottom:12px}
.relay-btn{display:flex;width:100px;height:100px;border-radius:50%;border:none;margin:8px auto;font-size:3em;cursor:pointer;transition:all .3s;align-items:center;justify-content:center;background:#2d2d44;color:#666}
.relay-btn.on{background:#e94560;color:#fff;box-shadow:0 0 40px rgba(233,69,96,.5)}
.relay-btn:active{transform:scale(.95)}
.badge{display:inline-block;padding:4px 16px;border-radius:999px;font-size:.85em;font-weight:600;margin:8px auto;text-align:center}
.badge.on{background:rgba(39,166,68,.15);color:#4ade80}
.badge.off{background:rgba(229,72,77,.15);color:#e5484d}
.expand-header{display:flex;justify-content:space-between;align-items:center;padding:10px 0;cursor:pointer;color:#888;font-size:.85em;border-bottom:1px solid #2a2a4a;user-select:none}
.expand-header:hover{color:#e94560}
#expandIcon,#expandIcon2{font-size:1em;transition:transform .2s}
#expandIcon.open,#expandIcon2.open{transform:rotate(90deg)}
.details{overflow:hidden;max-height:0;transition:max-height .3s ease}
.details.open{max-height:400px}
select{padding:6px 10px;border-radius:8px;border:1px solid #2a2a4a;background:#1a1a2e;color:#eee;font-size:.85em;width:100px}
input[type=text]{padding:6px 10px;border-radius:8px;border:1px solid #2a2a4a;background:#1a1a2e;color:#eee;font-size:.85em;width:100%;box-sizing:border-box}
.save-btn{padding:6px 16px;border-radius:8px;border:none;background:#e94560;color:#fff;font-size:.85em;cursor:pointer;margin-left:8px}
.save-btn:active{transform:scale(.95)}
.row{display:flex;justify-content:space-between;padding:8px 0;border-bottom:1px solid #2a2a4a}
.row:last-child{border-bottom:none}
.label{color:#888;font-size:.85em}
.value{font-weight:600;font-size:.85em;color:#e94560}
.value.connected{color:#4ade80}
.footer{text-align:center;margin-top:12px;font-size:.75em;color:#555}
.loading{opacity:.5;pointer-events:none}
</style>
</head>
<body>
<div class="card" id="app">
<h1 id="deviceName">OnOff</h1>
<button class="relay-btn" id="relayBtn" onclick="toggleRelay()">&#9889;</button>
<div style="text-align:center"><span class="badge off" id="relayBadge">DESLIGADA</span></div>
<div class="expand-header" onclick="toggleDetails()"><span>Detalhes</span><span id="expandIcon">&#9654;</span></div>
<div class="details" id="details">
<div class="row"><span class="label">Alexa</span><span class="value" id="alexaStatus">-</span></div>
<div class="row"><span class="label">Gateway</span><span class="value" id="gatewayStatus">-</span></div>
<div class="row"><span class="label">RSSI</span><span class="value" id="rssiStatus">-</span></div>
<div class="row"><span class="label">Bateria</span><span class="value" id="batteryStatus">-</span></div>
<div class="row"><span class="label">IP</span><span class="value" id="ipStatus">-</span></div>
<div class="row"><span class="label">Uptime</span><span class="value" id="uptimeStatus">-</span></div>
<div class="row"><span class="label">Versão</span><span class="value" id="fwVersion">-</span></div>
</div>
<div class="expand-header" onclick="toggleSettings()"><span>Configuracao</span><span id="expandIcon2">&#9654;</span></div>
<div class="details" id="settingsDetails">
<div class="row"><span class="label">Nome</span>
<span><input type="text" id="deviceNameInput" maxlength="47"></span>
</div>
<div class="row"><span class="label">GPIO rele</span>
<span><select id="relayPinSelect"></select></span>
</div>
<div class="row"><span class="label">GPIO botao</span>
<span><select id="buttonPinSelect"></select></span>
</div>
<div class="row"><span class="label">LED habilitado</span>
<span><input type="checkbox" id="ledEnabledCheck" onchange="savePins()"></span>
</div>
<div class="row"><span class="label">Iniciar rele</span>
<span><select id="startupModeSelect" onchange="savePins()">
<option value="0">OFF</option>
<option value="1">ON</option>
<option value="2">Ultimo</option>
</select></span>
</div>
<div style="text-align:center;margin-top:8px"><button class="save-btn" onclick="savePins()">Salvar</button></div>
<div class="row"><span class="label"> </span><span><button class="save-btn" style="background:#ef4444" onclick="restartDevice()">Reiniciar</button></span></div>
</div>
<div class="expand-header" onclick="toggleTimers()"><span>Timer</span><span id="expandIcon3">&#9654;</span></div>
<div class="details" id="timerDetails">
<div class="row"><span class="label">Proximo</span><span class="value" id="nextTimer">-</span></div>
<div id="timerList"></div>
<select id="timerHour" style="width:60px"></select>:<select id="timerMin" style="width:60px"></select>
<select id="timerAction" style="width:70px"><option value="0">OFF</option><option value="1">ON</option></select>
<select id="timerDays" style="width:80px"><option value="0">Todos</option><option value="127">Dias semana</option><option value="64">Sab/Dom</option></select>
<button class="save-btn" onclick="addTimer()">+Timer</button>
</div>
<div class="footer" id="footer">Carregando...</div>
</div>
<script>
const btn=document.getElementById('relayBtn');
const badge=document.getElementById('relayBadge');
const alexaEl=document.getElementById('alexaStatus');
const gatewayEl=document.getElementById('gatewayStatus');
const rssiEl=document.getElementById('rssiStatus');
const batteryEl=document.getElementById('batteryStatus');
const ipEl=document.getElementById('ipStatus');
const uptimeEl=document.getElementById('uptimeStatus');
const fwEl=document.getElementById('fwVersion');
const footerEl=document.getElementById('footer');
const details=document.getElementById('details');
const settingsDetails=document.getElementById('settingsDetails');
const expandIcon=document.getElementById('expandIcon');
const expandIcon2=document.getElementById('expandIcon2');
const relayPinSelect=document.getElementById('relayPinSelect');
const buttonPinSelect=document.getElementById('buttonPinSelect');
const ledEnabledCheck=document.getElementById('ledEnabledCheck');
const startupModeSelect=document.getElementById('startupModeSelect');
const timerDetails=document.getElementById('timerDetails');
const timerList=document.getElementById('timerList');
const nextTimerEl=document.getElementById('nextTimer');
const expandIcon3=document.getElementById('expandIcon3');
let loading=false;
let expanded=false;
let expanded2=false;
let expanded3=false;
function toggleDetails(){expanded=!expanded;details.classList.toggle('open',expanded);expandIcon.classList.toggle('open',expanded)}
function toggleSettings(){expanded2=!expanded2;settingsDetails.classList.toggle('open',expanded2);expandIcon2.classList.toggle('open',expanded2)}
function toggleTimers(){expanded3=!expanded3;timerDetails.classList.toggle('open',expanded3);expandIcon3.classList.toggle('open',expanded3);if(expanded3){fetchTimers()}}
for(let i=0;i<24;i++){let o=document.createElement('option');o.value=i;o.text=('0'+i).slice(-2);document.getElementById('timerHour').appendChild(o)}
for(let i=0;i<60;i++){let o=document.createElement('option');o.value=i;o.text=('0'+i).slice(-2);document.getElementById('timerMin').appendChild(o)}
async function restartDevice(){if(!confirm('Reiniciar dispositivo?'))return;
try{await fetch('/api/restart',{method:'POST'});footerEl.textContent='Reiniciando...'}catch(e){}}
async function savePins(){let nm=deviceNameInput.value.trim();let rp=relayPinSelect.value;let bp=buttonPinSelect.value;if(!rp||!bp)return;
let body={relay_pin:parseInt(rp),button_pin:parseInt(bp),led_enabled:ledEnabledCheck.checked,startup_mode:parseInt(startupModeSelect.value)};if(nm)body.device_name=nm;
try{let r=await fetch('/api/settings',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify(body)});
fetchSettings()}catch(e){footerEl.textContent='Erro: '+e.message}}
async function fetchState(){try{let r=await fetch('/api/state');let d=await r.json();
const on=d.state;btn.classList.toggle('on',on);badge.textContent=on?'LIGADA':'DESLIGADA';
badge.className='badge '+(on?'on':'off');
alexaEl.textContent=d.alexa_connected?'Conectado':'-';
alexaEl.className='value'+(d.alexa_connected?' connected':'');
gatewayEl.textContent=d.gateway_connected?'Conectado':'Desconectado';
gatewayEl.className='value'+(d.gateway_connected?' connected':'');
rssiEl.textContent=d.rssi+' dBm';batteryEl.textContent=d.battery+'%';
ipEl.textContent=d.ip;let m=Math.floor(d.uptime_s/60);let s=d.uptime_s%60;
let d_d=Math.floor(d.uptime_s/86400);let h=Math.floor((d.uptime_s%86400)/3600);let m_m=Math.floor((d.uptime_s%3600)/60);
uptimeEl.textContent=(d_d?d_d+'d ':'')+(h?h+'h ':'')+m_m+'m';fwEl.textContent=d.fw_version||'-';
document.getElementById('deviceName').textContent=d.device_name;
document.getElementById('pageTitle').textContent=d.device_name;
footerEl.textContent=d.device_id+(d.last_send_s?' Ultimo envio: '+d.last_send_s+'s ago':'');
}catch(e){footerEl.textContent='Erro: '+e.message}}
async function fetchSettings(){try{let r=await fetch('/api/settings');let d=await r.json();
deviceNameInput.value=d.device_name;
ledEnabledCheck.checked=d.led_enabled;
startupModeSelect.value=d.startup_mode;
relayPinSelect.innerHTML='';buttonPinSelect.innerHTML='';
d.available_pins.forEach(function(p){
let ro=document.createElement('option');ro.value=p;ro.text='GPIO '+p;if(p===d.relay_pin)ro.selected=true;relayPinSelect.appendChild(ro);
let bo=document.createElement('option');bo.value=p;bo.text='GPIO '+p;if(p===d.button_pin)bo.selected=true;buttonPinSelect.appendChild(bo)})}catch(e){}}
async function toggleRelay(){if(loading)return;loading=true;btn.classList.add('loading');
try{let r=await fetch('/api/relay',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify({state:!btn.classList.contains('on')})});
let d=await r.json();const on=d.state;btn.classList.toggle('on',on);badge.textContent=on?'LIGADA':'DESLIGADA';
badge.className='badge '+(on?'on':'off');
}catch(e){footerEl.textContent='Erro: '+e.message}finally{loading=false;btn.classList.remove('loading')}}
async function fetchTimers(){try{let r=await fetch('/api/timers');let d=await r.json();timerList.innerHTML='';
if(d.timers&&d.timers.length){d.timers.forEach(function(t,i){
let div=document.createElement('div');div.className='row';
let lbl=document.createElement('span');lbl.className='label';lbl.textContent=('0'+t.hour).slice(-2)+':'+('0'+t.minute).slice(-2)+' '+(t.action?'ON':'OFF');
let tog=document.createElement('input');tog.type='checkbox';tog.checked=t.enabled;tog.onchange=function(){t.enabled=tog.checked;updateTimer(i,t)};
div.appendChild(lbl);div.appendChild(tog);timerList.appendChild(div)})}else{timerList.innerHTML='<div class="row"><span class="label">Nenhum timer configurado</span></div>'}
let nr=await fetch('/api/timer/next');let nd=await nr.json();
nextTimerEl.textContent=nd.has_next?new Date(nd.next_epoch*1000).toLocaleString('pt-BR',{hour:'2-digit',minute:'2-digit'}):'-';
}catch(e){}}
async function updateTimer(i,t){try{let r=await fetch('/api/timers',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify({index:i,...t})});
if(r.ok)console.log('Timer updated')}catch(e){}}
async function addTimer(){let h=document.getElementById('timerHour').value;let m=document.getElementById('timerMin').value;
let a=document.getElementById('timerAction').value;let d=document.getElementById('timerDays').value;
let t={hour:parseInt(h),minute:parseInt(m),action:parseInt(a),days_mask:parseInt(d),enabled:true};
try{let r=await fetch('/api/timers',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify(t)});
if(r.ok){fetchTimers()}}catch(e){}}
setInterval(fetchState,3000);fetchState();fetchSettings();fetchTimers();
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
*{margin:0;padding:0;box-sizing:border-box}
body{font-family:-apple-system,system-ui,sans-serif;background:#1a1a2e;color:#eee;padding:16px}
h1{color:#e94560;font-size:1.2em;margin-bottom:16px}
.endpoint{background:#16213e;border-radius:12px;padding:12px;margin-bottom:8px}
.head{display:flex;align-items:center;gap:8px;margin-bottom:4px}
.method{font-weight:700;font-size:.8em;padding:2px 8px;border-radius:4px}
.get{color:#4ade80}.post{color:#e94560}
.path{font-family:monospace;font-size:.9em;color:#eee}
.desc{color:#888;font-size:.82em;margin-top:4px}
table{width:100%;font-size:.8em;border-collapse:collapse;margin-top:4px}
th{text-align:left;color:#888;padding:2px 4px;border-bottom:1px solid #2a2a4a}
td{padding:2px 4px;border-bottom:1px solid #2a2a4a;color:#aaa;font-family:monospace}
</style>
</head>
<body>
<h1>API</h1>
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
  <div class="desc">Altera nome/pinos <code>{"device_name":"...","relay_pin":N,"button_pin":N}</code></div>
</div>
<div class="endpoint">
  <div class="head"><span class="method post">POST</span><span class="path">/api/restart</span></div>
  <div class="desc">Reinicia o dispositivo</div>
</div>
<div class="endpoint">
  <div class="head"><span class="method post">POST</span><span class="path">/api/ota</span></div>
  <div class="desc">OTA update (upload multipart)</div>
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
*{margin:0;padding:0;box-sizing:border-box}
body{font-family:-apple-system,system-ui,sans-serif;background:#1a1a2e;color:#eee;min-height:100vh;display:flex;flex-direction:column;align-items:center;justify-content:center;padding:20px}
.card{background:#16213e;border-radius:16px;padding:24px;width:100%;max-width:380px;box-shadow:0 8px 32px rgba(0,0,0,.3)}
h1{text-align:center;font-size:1.2em;color:#e94560;margin-bottom:16px}
label{display:block;color:#888;font-size:.85em;margin-top:12px;margin-bottom:4px}
input{width:100%;padding:8px 12px;border-radius:8px;border:1px solid #2a2a4a;background:#1a1a2e;color:#eee;font-size:.9em}
input:focus{outline:none;border-color:#e94560}
.btn{width:100%;padding:10px;border-radius:8px;border:none;background:#e94560;color:#fff;font-size:1em;cursor:pointer;margin-top:20px}
.btn:active{transform:scale(.98)}
.btn.loading{opacity:.6;pointer-events:none}
.hint{color:#555;font-size:.75em;margin-top:4px}
.msg{margin-top:12px;padding:8px;border-radius:8px;font-size:.85em;text-align:center}
.msg.ok{background:rgba(39,166,68,.15);color:#4ade80}
.msg.err{background:rgba(229,72,77,.15);color:#e5484d}
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
<div class="hint">Deixe em branco para uso normal (nao repeater)</div>
<button type="submit" class="btn" id="submitBtn">Conectar</button>
</form>
<div id="msg" class="msg" style="display:none"></div>
</div>
<script>
var currentName='';
async function loadStatus(){try{var r=await fetch('/api/wifi');var d=await r.json();
document.getElementById('devName').value=d.device_name||'';currentName=d.device_name||''}catch(e){}}
function showMsg(t,c){var m=document.getElementById('msg');m.textContent=t;m.className='msg '+c;m.style.display='block'}
function loading(v){document.getElementById('submitBtn').className='btn'+(v?' loading':'');document.getElementById('submitBtn').disabled=v}
async function submitForm(){var ssid=document.getElementById('ssid').value.trim();if(!ssid){showMsg('Informe o SSID','err');return false}
var pass=document.getElementById('password').value;var name=document.getElementById('devName').value.trim();
var rep=document.getElementById('repeaterMac').value.trim();loading(true);
var body={ssid:ssid,password:pass};if(name)body.device_name=name;if(rep)body.repeater_mac=rep;
try{var r=await fetch('/api/wifi',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify(body)});
var d=await r.json();if(d.status==='ok'){showMsg('Conectando a '+ssid+'...','ok');
setTimeout(function(){showMsg('Se a conexao falhar, o AP sera reativado.','ok')},2000)}else{showMsg('Erro: '+d.error,'err');loading(false)}}
catch(e){showMsg('Erro: '+e.message,'err');loading(false)}return false}
loadStatus();
</script>
</body>
</html>
)=====";
