#pragma once

#include <Arduino.h>

static const char PAGE_DASHBOARD[] PROGMEM = R"=====(
<!DOCTYPE html>
<html lang="pt-BR">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>Repeater</title>
<style>
*{margin:0;padding:0;box-sizing:border-box}
body{font-family:-apple-system,system-ui,sans-serif;background:#1a1a2e;color:#eee;min-height:100vh;display:flex;align-items:center;justify-content:center;padding:20px}
.card{background:#16213e;border-radius:16px;padding:24px;width:100%;max-width:500px;box-shadow:0 8px 32px rgba(0,0,0,.3)}
h1{text-align:center;font-size:1.3em;margin-bottom:16px;color:#e94560}
.row{display:flex;justify-content:space-between;padding:6px 0;border-bottom:1px solid #2a2a4a;font-size:.9em}.row:last-child{border:none}
.lbl{color:#888}.val{font-weight:600;color:#4ade80}
table{width:100%;border-collapse:collapse;margin-top:8px;font-size:.8em}
th{text-align:left;color:#888;padding:4px 0}td{padding:4px 0;font-family:monospace}
.footer{text-align:center;margin-top:12px;font-size:.7em;color:#555}
</style>
</head>
<body>
<div class="card">
<h1>ESP-NOW Repeater</h1>
<div class="row"><span class="lbl">Gateway</span><span class="val" id="gw">-</span></div>
<div class="row"><span class="lbl">Uptime</span><span class="val" id="up">-</span></div>
<div class="row"><span class="lbl">Channel</span><span class="val" id="ch">-</span></div>
<div class="row"><span class="lbl">RSSI</span><span class="val" id="rssi">-</span></div>
<div class="row"><span class="lbl">RX</span><span class="val" id="rx">0</span></div>
<div class="row"><span class="lbl">TX</span><span class="val" id="tx">0</span></div>
<div class="row"><span class="lbl">Clients</span><span class="val" id="clients">0</span></div>
<div class="row"><span class="lbl">Last Activity</span><span class="val" id="last">-</span></div>
<table><thead><tr><th>#</th><th>MAC</th></tr></thead><tbody id="tbl"></tbody></table>
<div class="footer" id="footer"></div>
</div>
<script>
async function u(){try{let r=await fetch('/api/status');let d=await r.json();
document.getElementById('gw').textContent=d.gateway;
let u=d.uptime_s,h=Math.floor(u/3600),m=Math.floor((u%3600)/60),s=u%60;document.getElementById('up').textContent=h?h+'h '+m+'m':m+'m '+s+'s';
document.getElementById('ch').textContent=d.channel;document.getElementById('rssi').textContent=d.rssi+' dBm';
document.getElementById('rx').textContent=d.received;document.getElementById('tx').textContent=d.forwarded;
document.getElementById('clients').textContent=d.clients;
let a=d.last_activity_s;document.getElementById('last').textContent=a<0?'-':a<60?a+'s':a<3600?Math.floor(a/60)+'m':Math.floor(a/3600)+'h '+Math.floor((a%3600)/60)+'m';
let t='';d.client_list.forEach((c,i)=>{t+='<tr><td>'+(i+1)+'</td><td>'+c+'</td></tr>';});document.getElementById('tbl').innerHTML=t;
}catch(e){document.getElementById('footer').textContent='Erro: '+e.message}}
setInterval(u,3000);u();
</script>
</body>
</html>
)=====";
