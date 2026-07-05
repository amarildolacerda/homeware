#pragma once

#include <Arduino.h>

static const char PAGE_DASHBOARD[] PROGMEM = R"=====(
<!DOCTYPE html>
<html lang="pt-BR">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>Lampada</title>
<style>
*{margin:0;padding:0;box-sizing:border-box}
body{font-family:-apple-system,system-ui,BlinkMacSystemFont,sans-serif;background:#1a1a2e;color:#eee;min-height:100vh;display:flex;flex-direction:column;align-items:center;justify-content:center;padding:20px}
.card{background:#16213e;border-radius:16px;padding:32px;width:100%;max-width:400px;box-shadow:0 8px 32px rgba(0,0,0,.3)}
h1{text-align:center;font-size:1.5em;margin-bottom:24px;color:#e94560}
.relay-btn{display:flex;width:120px;height:120px;border-radius:50%;border:none;margin:16px auto;font-size:4em;cursor:pointer;transition:all .3s;align-items:center;justify-content:center;background:#2d2d44;color:#666}
.relay-btn.on{background:#e94560;color:#fff;box-shadow:0 0 40px rgba(233,69,96,.5)}
.relay-btn:active{transform:scale(.95)}
.status-row{display:flex;justify-content:space-between;padding:8px 0;border-bottom:1px solid #2a2a4a}
.status-row:last-child{border-bottom:none}
.label{color:#888}
.value{font-weight:600;color:#e94560}
.value.connected{color:#4ade80}
.footer{text-align:center;margin-top:12px;font-size:.8em;color:#555}
.ota{border-top:1px solid #2a2a4a;margin-top:16px;padding-top:16px}
.ota form{display:flex;gap:8px}
.ota input[type="file"]{flex:1;background:#2d2d44;color:#eee;border:1px solid #444;border-radius:8px;padding:8px;font-size:.8em}
.ota button{background:#4361ee;border:none;border-radius:8px;padding:8px 16px;color:#fff;cursor:pointer;font-size:.8em}
.ota button:hover{background:#3a56d4}
.loading{opacity:.5;pointer-events:none}
</style>
</head>
<body>
<div class="card" id="app">
<h1><span id="deviceName">Lampada</span></h1>
<button class="relay-btn" id="relayBtn" onclick="toggleRelay()">&#9889;</button>
<div class="status-row"><span class="label">Estado</span><span class="value" id="relayStatus">Desconhecido</span></div>
<div class="status-row"><span class="label">Gateway</span><span class="value" id="gatewayStatus">-</span></div>
<div class="status-row"><span class="label">RSSI</span><span class="value" id="rssiStatus">-</span></div>
<div class="status-row"><span class="label">Bateria</span><span class="value" id="batteryStatus">-</span></div>
<div class="status-row"><span class="label">IP</span><span class="value" id="ipStatus">-</span></div>
<div class="status-row"><span class="label">Uptime</span><span class="value" id="uptimeStatus">-</span></div>
<div class="status-row"><span class="label">Slot</span><span class="value" id="slotStatus">-</span></div>
<div class="status-row"><span class="label">Versão</span><span class="value" id="fwVersion">-</span></div>
<div class="ota">
<form id="otaForm" enctype="multipart/form-data" method="POST" action="/api/ota">
<input type="file" name="firmware" accept=".bin" id="otaFile">
<button type="submit">OTA</button>
</form>
</div>
<div class="footer" id="footer">Carregando...</div>
</div>
<script>
const btn=document.getElementById('relayBtn');
const gatewayEl=document.getElementById('gatewayStatus');
const relayStatus=document.getElementById('relayStatus');
const rssiEl=document.getElementById('rssiStatus');
const batteryEl=document.getElementById('batteryStatus');
const ipEl=document.getElementById('ipStatus');
const uptimeEl=document.getElementById('uptimeStatus');
const slotEl=document.getElementById('slotStatus');
const fwEl=document.getElementById('fwVersion');
const footerEl=document.getElementById('footer');
let loading=false;
async function fetchState(){try{let r=await fetch('/api/state');let d=await r.json();
btn.classList.toggle('on',d.state);relayStatus.textContent=d.state?'LIGADA':'DESLIGADA';
gatewayEl.textContent=d.gateway_connected?'Conectado':'Desconectado';
gatewayEl.className='value'+(d.gateway_connected?' connected':'');
rssiEl.textContent=d.rssi+' dBm';batteryEl.textContent=d.battery+'%';
ipEl.textContent=d.ip;let m=Math.floor(d.uptime_s/60);let s=d.uptime_s%60;
uptimeEl.textContent=m+'m '+s+'s';slotEl.textContent='Slot '+(d.slot||'?');fwEl.textContent=d.fw_version||'-';
document.getElementById('deviceName').textContent=d.device_name;
footerEl.textContent='ID: '+d.device_id+(d.last_send_s?' Ultimo envio: '+d.last_send_s+'s ago':'');
}catch(e){footerEl.textContent='Erro: '+e.message}}
async function toggleRelay(){if(loading)return;loading=true;btn.classList.add('loading');
try{let r=await fetch('/api/relay',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify({state:!btn.classList.contains('on')})});
let d=await r.json();btn.classList.toggle('on',d.state);relayStatus.textContent=d.state?'LIGADA':'DESLIGADA';
}catch(e){footerEl.textContent='Erro: '+e.message}finally{loading=false;btn.classList.remove('loading')}}
document.getElementById('otaForm').addEventListener('submit',function(e){if(!document.getElementById('otaFile').value){e.preventDefault();alert('Selecione um arquivo .bin')}});
setInterval(fetchState,3000);fetchState();
</script>
</body>
</html>
)=====";
