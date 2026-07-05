#pragma once

#include <Arduino.h>

static const char PAGE_DASHBOARD[] PROGMEM = R"=====(
<!DOCTYPE html>
<html lang="pt-BR">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>Lampada</title>
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
#expandIcon{font-size:1em;transition:transform .2s}
#expandIcon.open{transform:rotate(90deg)}
.details{overflow:hidden;max-height:0;transition:max-height .3s ease}
.details.open{max-height:400px}
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
<h1 id="deviceName">Lampada</h1>
<button class="relay-btn" id="relayBtn" onclick="toggleRelay()">&#9889;</button>
<div style="text-align:center"><span class="badge off" id="relayBadge">DESLIGADA</span></div>
<div class="expand-header" onclick="toggleDetails()"><span>Detalhes</span><span id="expandIcon">&#9654;</span></div>
<div class="details" id="details">
<div class="row"><span class="label">Gateway</span><span class="value" id="gatewayStatus">-</span></div>
<div class="row"><span class="label">RSSI</span><span class="value" id="rssiStatus">-</span></div>
<div class="row"><span class="label">Bateria</span><span class="value" id="batteryStatus">-</span></div>
<div class="row"><span class="label">IP</span><span class="value" id="ipStatus">-</span></div>
<div class="row"><span class="label">Uptime</span><span class="value" id="uptimeStatus">-</span></div>
<div class="row"><span class="label">Versão</span><span class="value" id="fwVersion">-</span></div>
</div>
<div class="footer" id="footer">Carregando...</div>
</div>
<script>
const btn=document.getElementById('relayBtn');
const badge=document.getElementById('relayBadge');
const gatewayEl=document.getElementById('gatewayStatus');
const rssiEl=document.getElementById('rssiStatus');
const batteryEl=document.getElementById('batteryStatus');
const ipEl=document.getElementById('ipStatus');
const uptimeEl=document.getElementById('uptimeStatus');
const fwEl=document.getElementById('fwVersion');
const footerEl=document.getElementById('footer');
const details=document.getElementById('details');
const expandIcon=document.getElementById('expandIcon');
let loading=false;
let expanded=false;
function toggleDetails(){expanded=!expanded;details.classList.toggle('open',expanded);expandIcon.classList.toggle('open',expanded)}
async function fetchState(){try{let r=await fetch('/api/state');let d=await r.json();
const on=d.state;btn.classList.toggle('on',on);badge.textContent=on?'LIGADA':'DESLIGADA';
badge.className='badge '+(on?'on':'off');
gatewayEl.textContent=d.gateway_connected?'Conectado':'Desconectado';
gatewayEl.className='value'+(d.gateway_connected?' connected':'');
rssiEl.textContent=d.rssi+' dBm';batteryEl.textContent=d.battery+'%';
ipEl.textContent=d.ip;let m=Math.floor(d.uptime_s/60);let s=d.uptime_s%60;
uptimeEl.textContent=m+'m '+s+'s';fwEl.textContent=d.fw_version||'-';
document.getElementById('deviceName').textContent=d.device_name;
footerEl.textContent=d.device_id+(d.last_send_s?' Ultimo envio: '+d.last_send_s+'s ago':'');
}catch(e){footerEl.textContent='Erro: '+e.message}}
async function toggleRelay(){if(loading)return;loading=true;btn.classList.add('loading');
try{let r=await fetch('/api/relay',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify({state:!btn.classList.contains('on')})});
let d=await r.json();const on=d.state;btn.classList.toggle('on',on);badge.textContent=on?'LIGADA':'DESLIGADA';
badge.className='badge '+(on?'on':'off');
}catch(e){footerEl.textContent='Erro: '+e.message}finally{loading=false;btn.classList.remove('loading')}}
setInterval(fetchState,3000);fetchState();
</script>
</body>
</html>
)=====";
