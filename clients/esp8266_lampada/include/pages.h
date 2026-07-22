#pragma once

#include <Arduino.h>

static const char PAGE_DASHBOARD[] PROGMEM = R"=====(
<!DOCTYPE html>
<html lang="pt-BR">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title id="pageTitle">Lâmpada</title>
<style>
:root{--bg:#f4f4f4;--surface:#fff;--surface-2:#f9fafb;--text:#1f2937;--muted:#6b7280;--muted-subtle:#9ca3af;--primary:#5e6ad2;--primary-strong:#828fff;--primary-focus:#eef0ff;--border:#e5e7eb;--success:#16a34a;--danger:#dc2626;--sidebar-w:180px}
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
.nav-sub{padding-left:16px}
.nav-sub .nav-item{font-size:.78rem;padding:7px 16px}
.sidebar-bottom{padding:12px 16px;border-top:1px solid var(--border);font-size:.7rem;color:var(--muted-subtle)}
.main{flex:1;margin-left:var(--sidebar-w);display:flex;flex-direction:column;min-height:100vh}
.stats-header{background:var(--surface);border-bottom:1px solid var(--border);padding:5px 20px;display:flex;align-items:center;gap:6px}
.stats-header .back-link{color:var(--primary);text-decoration:none;font-size:.95rem;margin-right:2px}
.stats-header .stat{flex:1;background:var(--surface-2);border:1px solid var(--border);border-radius:8px;padding:5px 2px;text-align:center}
.stats-header .stat-value{font-size:.82rem;font-weight:700;color:var(--primary)}
.stats-header .stat-label{font-size:.52rem;color:var(--muted-subtle);text-transform:uppercase;letter-spacing:.03em;margin-top:1px}
.content{flex:1;padding:20px;max-width:480px;width:100%;margin:0 auto}
.footer-bar{display:flex;align-items:center;gap:16px;padding:8px 20px;border-top:1px solid var(--border);background:var(--surface);font-size:.75rem;color:var(--muted-subtle);flex-wrap:wrap;justify-content:flex-end;margin-top:auto}
.fb-dot{width:7px;height:7px;border-radius:50%;flex-shrink:0}
.fb-dot.online{background:var(--success)}
.fb-dot.offline{background:var(--danger)}
.fb-sep{color:var(--border);user-select:none}
h1{font-size:1.1rem;color:var(--primary);margin-bottom:12px;text-align:center}
.hero{text-align:center;padding:16px 0 8px}
.relay-btn{width:80px;height:80px;border-radius:50%;border:2px solid var(--border);font-size:2.2rem;cursor:pointer;transition:all .25s;background:var(--surface);color:var(--muted-subtle);display:inline-flex;align-items:center;justify-content:center}
.relay-btn:hover{border-color:var(--primary)}
.relay-btn.on{background:var(--primary);border-color:var(--primary-strong);color:#fff;box-shadow:0 0 0 4px var(--primary-focus)}
.badge{display:inline-block;padding:3px 14px;border-radius:9999px;font-size:.75rem;font-weight:600}
.badge.on{background:#dcfce7;color:var(--success)}
.badge.off{background:#fef2f2;color:var(--danger)}
.row{display:flex;justify-content:space-between;align-items:center;padding:9px 0;border-bottom:1px solid var(--border)}
.row:last-child{border-bottom:none}
.label{color:var(--muted-subtle);font-size:.82rem}
.value{font-weight:600;font-size:.82rem;color:var(--text)}
.value.green{color:var(--success)}
input[type=text],input[type=number]{padding:6px 10px;border-radius:8px;border:1px solid var(--border);background:var(--surface);color:var(--text);font-size:.82rem;outline:none;width:100%}
input[type=text]:focus,input[type=number]:focus{border-color:var(--primary)}
select{padding:6px 8px;border-radius:8px;border:1px solid var(--border);background:var(--surface);color:var(--text);font-size:.82rem;outline:none}
.btn{padding:6px 14px;border-radius:8px;border:1px solid var(--primary);background:var(--surface);color:var(--primary);font-size:.82rem;font-weight:500;cursor:pointer}
.btn:active{transform:scale(.97)}
.btn-primary{background:var(--primary);color:#fff;border-color:var(--primary)}
.btn-danger{border-color:var(--danger);color:var(--danger)}
.btn-sm{padding:4px 10px;font-size:.75rem}
.footer{text-align:center;margin-top:12px;font-size:.72rem;color:var(--muted-subtle)}
.loading{opacity:.5;pointer-events:none}
.timer-add{display:flex;gap:4px;align-items:center;margin-top:8px;flex-wrap:wrap}
.section{display:none}
.section.active{display:block}
@media(max-width:600px){.sidebar{width:48px}.sidebar-top .dev-name,.sidebar-top .dev-id,.sidebar-bottom{display:none}.nav-item{justify-content:center;padding:10px 4px}.nav-item span{display:none}.nav-item span:first-child{display:inline;font-size:1.1rem}.nav-sub{display:none}.main{margin-left:48px}.stats-header{padding:5px 10px}.content{padding:12px}.footer-bar{padding:6px 12px;font-size:.7rem;gap:8px}}
</style>
</head>
<body>
<div class="sidebar">
<div class="sidebar-top">
<div class="dev-name" id="sbName">Lâmpada</div>
<div class="dev-id" id="sbId">-</div>
</div>
<div class="nav">
<div class="nav-item active" data-section="home" onclick="showSection('home')"><span>🏠</span><span>Home</span></div>
<div class="nav-item" data-section="ciclo" onclick="toggleCiclo()"><span>🔄</span><span>Ciclo</span><span class="expand-icon" id="cicloIcon" style="margin-left:auto;font-size:.65rem">&#9654;</span></div>
<div id="cicloSub" class="nav-sub" style="display:none">
<div class="nav-item" data-section="timer" onclick="showSection('timer')"><span>⏰</span><span>Timer</span></div>
<div class="nav-item" data-section="cyclic" onclick="showSection('cyclic')"><span>🔁</span><span>Ciclico</span></div>
<div class="nav-item" data-section="sync" onclick="showSection('sync')"><span>🔗</span><span>Sincronizacao</span></div>
</div>
<div class="nav-item" data-section="propriedades" onclick="showSection('propriedades')"><span>📋</span><span>Propriedades</span></div>
)=====";
#ifdef HABILITA_PINOS
static const char PAGE_PINS_NAV[] PROGMEM = R"=====(
<div class="nav-item" data-section="pins" onclick="showSection('pins')"><span>🔧</span><span>Pinos</span></div>
)=====";
#else
static const char PAGE_PINS_NAV[] PROGMEM = R"=====()=====";
#endif
static const char PAGE_DASHBOARD_CONT1[] PROGMEM = R"=====(
<div class="nav-item" data-section="config" onclick="showSection('config')"><span>⚙</span><span>Configurações</span></div>
<div class="nav-item" data-section="repeater" onclick="showSection('repeater')" id="navRepeater" style="display:none"><span>📡</span><span>Repeater</span></div>
</div>
<div class="sidebar-bottom"><span id="sbVersion">...</span></div>
</div>
<div class="main">
<div class="stats-header">
<a id="backBtn" href="#" class="back-link" style="display:none">&larr;</a>
<div class="stat"><div class="stat-value" id="rxVal">0</div><div class="stat-label">RX</div></div>
<div class="stat"><div class="stat-value" id="txVal">0</div><div class="stat-label">TX</div></div>
<div class="stat"><div class="stat-value" id="memVal">-</div><div class="stat-label">Mem</div></div>
<div class="stat"><div class="stat-value" id="onVal">0</div><div class="stat-label">Liga</div></div>
<div class="stat" id="fwdStat" style="display:none"><div class="stat-value" id="fwdVal">0</div><div class="stat-label">Retransm.</div></div>
</div>
<div class="content">
<div class="section active" id="secHome">
<div class="hero">
<button class="relay-btn" id="relayBtn" onclick="toggleRelay()">&#9889;</button>
<div style="margin-top:6px"><span class="badge off" id="relayBadge">DESLIGADA</span></div>
</div>
<div class="footer" id="footer">Carregando...</div>
</div>
<div class="section" id="secTimer">
<h1>Timer</h1>
<div class="row"><span class="label">Próximo</span><span class="value" id="nextTimer">-</span></div>
<div id="timerList"></div>
<div class="timer-add">
<select id="timerHour" style="width:56px"></select><span style="color:var(--muted-subtle)">:</span><select id="timerMin" style="width:56px"></select>
<select id="timerAction" style="width:62px"><option value="0">OFF</option><option value="1">ON</option></select>
<select id="timerDays" style="width:74px"><option value="0">Todos</option><option value="127">Semana</option><option value="64">Fim de sem</option></select>
<button class="btn btn-primary btn-sm" onclick="addTimer()">+</button>
</div>
</div>
<div class="section" id="secCyclic">
<h1>Ciclico</h1>
<div class="row"><span class="label">Ativado</span><input type="checkbox" id="cyclicEnabled"></div>
<div class="row"><span class="label">Duracao (min)</span><input type="number" id="cyclicDuration" min="1" max="1440" style="width:80px"></div>
<div style="text-align:center;margin-top:10px"><button class="btn btn-primary btn-sm" onclick="saveCyclic()">Salvar</button></div>
</div>
<div class="section" id="secSync">
<h1>Sincronizacao</h1>
<div class="row"><span class="label">Ativado</span><input type="checkbox" id="syncEnabled"></div>
<div class="row"><span class="label">Target ID</span><input type="text" id="syncTargetId" maxlength="32" style="width:160px"></div>
<div style="text-align:center;margin-top:10px"><button class="btn btn-primary btn-sm" onclick="saveSync()">Salvar</button></div>
</div>
<div class="section" id="secConfig">
<h1>Configuração</h1>
<div class="row"><span class="label">Nome</span><input type="text" id="deviceNameInput" maxlength="47" style="width:160px"></div>
<div class="row"><span class="label">GPIO relé</span><select id="relayPinSelect"></select></div>
<div class="row"><span class="label">GPIO botão</span><select id="buttonPinSelect"></select></div>
<div class="row"><span class="label">LED</span><label style="font-size:.82rem;color:var(--muted-subtle)"><input type="checkbox" id="ledEnabledCheck" onchange="savePins()"> habilitado</label></div>
<div class="row"><span class="label">Estado ao Iniciar</span><select id="startupModeSelect" onchange="savePins()">
<option value="0">OFF</option><option value="1">ON</option><option value="2">Último</option></select></div>
)=====";
#ifdef HABILITA_REPEATER
static const char PAGE_DASHBOARD_REPEATER_CFG[] PROGMEM = R"=====(
<div class="row"><span class="label">Modo repetidor</span><button class="btn" id="repBtn" onclick="toggleRepeater()">-</button></div>
)=====";
#endif
static const char PAGE_DASHBOARD_CONT3[] PROGMEM = R"=====(
<div style="display:flex;gap:8px;justify-content:center;margin-top:10px">
<button class="btn btn-primary" onclick="savePins()">Salvar</button>
<button class="btn btn-danger" onclick="restartDevice()">Reiniciar</button>
</div>
<div class="row" style="margin-top:14px;flex-direction:column;align-items:stretch;gap:6px">
<span class="label">Atualizar Firmware</span>
<input type="file" id="otaFile" accept=".bin">
<button class="btn btn-primary btn-sm" onclick="doUpdate()">Enviar e Atualizar</button>
<span class="value" id="otaStatus" style="font-size:.72rem;color:var(--muted-subtle)"></span>
</div>
</div>
<div class="section" id="secPropriedades">
<h1>Propriedades</h1>
<div class="row"><span class="label">Alexa</span><span class="value" id="alexaStatus">-</span></div>
<div class="row"><span class="label">IP</span><span class="value" id="ipStatus">-</span></div>
<div class="row"><span class="label">Bateria</span><span class="value" id="batteryStatus">-</span></div>
<div class="row"><span class="label">Versão</span><span class="value" id="fwVersion">-</span></div>
<div class="row"><span class="label">Slot</span><span class="value" id="slotStatus">-</span></div>
</div>
)=====";
#ifdef HABILITA_PINOS
static const char PAGE_PINS_SEC[] PROGMEM = R"=====(
<div class="section" id="secPins">
<h1>Estado dos Pinos</h1>
<div id="pinList"></div>
</div>
)=====";
#else
static const char PAGE_PINS_SEC[] PROGMEM = R"=====()=====";
#endif
static const char PAGE_DASHBOARD_CONT2[] PROGMEM = R"=====(
<div class="section" id="secRepeater">
<h1>Repeater</h1>
<div class="row"><span class="label">Função</span><span class="value" id="repState">-</span></div>
<div class="row"><span class="label">Total Retransmitidos</span><span class="value" id="repFwd">0</span></div>
<div id="repClientList"></div>
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
const btn=document.getElementById('relayBtn');
const badge=document.getElementById('relayBadge');
const rxVal=document.getElementById('rxVal');
const txVal=document.getElementById('txVal');
const memVal=document.getElementById('memVal');
const onVal=document.getElementById('onVal');
const alexaEl=document.getElementById('alexaStatus');
const ipEl=document.getElementById('ipStatus');
const batteryEl=document.getElementById('batteryStatus');
const fwEl=document.getElementById('fwVersion');
const slotEl=document.getElementById('slotStatus');
const footerEl=document.getElementById('footer');
const timerList=document.getElementById('timerList');
const nextTimerEl=document.getElementById('nextTimer');
const fbDot=document.getElementById('fbDot');
const fbGateway=document.getElementById('fbGateway');
const fbTime=document.getElementById('fbTime');
const fbUptime=document.getElementById('fbUptime');
let loading=false,cicloOpen=false,currentSection='home';
function showSection(s){document.querySelectorAll('.section').forEach(function(el){el.classList.remove('active')});document.getElementById('sec'+(s.charAt(0).toUpperCase()+s.slice(1))).classList.add('active');
document.querySelectorAll('.nav-item[data-section]').forEach(function(el){el.classList.remove('active')});document.querySelector('.nav-item[data-section="'+s+'"]').classList.add('active');currentSection=s;if(s==='pins')fetchPins()}
function toggleCiclo(){cicloOpen=!cicloOpen;document.getElementById('cicloSub').style.display=cicloOpen?'block':'none';document.getElementById('cicloIcon').style.transform=cicloOpen?'rotate(90deg)':'none'}
for(let i=0;i<24;i++){let o=document.createElement('option');o.value=i;o.text=('0'+i).slice(-2);document.getElementById('timerHour').appendChild(o)}
for(let i=0;i<60;i++){let o=document.createElement('option');o.value=i;o.text=('0'+i).slice(-2);document.getElementById('timerMin').appendChild(o)}
async function restartDevice(){if(!confirm('Reiniciar?'))return;try{await fetch('/api/restart',{method:'POST'});footerEl.textContent='Reiniciando...'}catch(e){}}
async function savePins(){let nm=document.getElementById('deviceNameInput').value.trim();let rp=document.getElementById('relayPinSelect').value;let bp=document.getElementById('buttonPinSelect').value;
let body={relay_pin:parseInt(rp),button_pin:parseInt(bp),led_enabled:document.getElementById('ledEnabledCheck').checked,startup_mode:parseInt(document.getElementById('startupModeSelect').value)};if(nm)body.device_name=nm;
try{await fetch('/api/settings',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify(body)});fetchSettings()}catch(e){footerEl.textContent='Erro: '+e.message}}
async function fetchState(){try{let r=await fetch('/api/state');let d=await r.json();
  const on=d.state;btn.classList.toggle('on',on);badge.textContent=on?'LIGADA':'DESLIGADA';badge.className='badge '+(on?'on':'off');
  rxVal.textContent=d.rx_count||0;
  txVal.textContent=d.tx_count||0;
  memVal.textContent=d.free_heap||0;
  onVal.textContent=d.on_count||0;
  let fwdStat=document.getElementById('fwdStat');let fwdVal=document.getElementById('fwdVal');
  if(fwdStat&&fwdVal){if(d.repeater_active){fwdStat.style.display='';fwdVal.textContent=d.repeater_fwd||0;}else{fwdStat.style.display='none'}}
  let u=d.uptime_s||0;let dd=Math.floor(u/86400);let hh=Math.floor((u%86400)/3600);let mm=Math.floor((u%3600)/60);
  let uptimeStr=(dd?dd+'d ':'')+(hh?hh+'h ':'')+mm+'m';
alexaEl.textContent=d.alexa_connected?'Conectado':'-';alexaEl.className='value'+(d.alexa_connected?' green':'');
ipEl.textContent=d.ip||'-';batteryEl.textContent=(d.battery||0)+'%';fwEl.textContent=d.fw_version||'-';
slotEl.textContent=d.slot!==undefined&&d.slot!==null?d.slot:'-';
let name=d.device_name||'Lâmpada';
document.getElementById('pageTitle').textContent=name;
document.getElementById('sbName').textContent=name;
document.getElementById('sbId').textContent=d.device_id||'';
const ve=document.getElementById('sbVersion');if(ve&&d.fw_version)ve.textContent=d.fw_version;
footerEl.textContent=d.device_id+(d.last_send_s?' Último envio: '+d.last_send_s+'s ago':'');
fbDot.className='fb-dot'+(d.gateway_connected?' online':' offline');
fbGateway.textContent=d.gateway_connected?'Online':'Offline';
fbUptime.textContent=uptimeStr;
fbTime.textContent=new Date().toLocaleTimeString('pt-BR',{hour:'2-digit',minute:'2-digit'});
fetchRepeater(d);
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
let d=await r.json();const on=d.state;btn.classList.toggle('on',on);badge.textContent=on?'LIGADA':'DESLIGADA';badge.className='badge '+(on?'on':'off');
}catch(e){footerEl.textContent='Erro: '+e.message}finally{loading=false;btn.classList.remove('loading')}}
async function fetchTimers(){try{let r=await fetch('/api/timers');let d=await r.json();timerList.innerHTML='';
if(d.timers&&d.timers.length){d.timers.forEach(function(t,i){
let div=document.createElement('div');div.className='row';
let lbl=document.createElement('span');lbl.className='label';lbl.textContent=('0'+t.hour).slice(-2)+':'+('0'+t.minute).slice(-2)+' '+(t.action?'ON':'OFF');
let cb=document.createElement('input');cb.type='checkbox';cb.checked=t.enabled;cb.onchange=function(){t.enabled=cb.checked;fetch('/api/timers',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify({index:i,...t})})};
div.appendChild(lbl);div.appendChild(cb);timerList.appendChild(div)})}else{timerList.innerHTML='<div class="row"><span class="label">Nenhum timer</span></div>'}
if(d.cyclic){document.getElementById('cyclicEnabled').checked=!!d.cyclic.enabled;document.getElementById('cyclicDuration').value=d.cyclic.duration_min||30}
if(d.sync){document.getElementById('syncEnabled').checked=!!d.sync.enabled;document.getElementById('syncTargetId').value=d.sync.target_id||''}
let nr=await fetch('/api/timer/next');let nd=await nr.json();
nextTimerEl.textContent=nd.has_next?new Date(nd.next_epoch*1000).toLocaleString('pt-BR',{hour:'2-digit',minute:'2-digit'}):'-'}catch(e){}}
async function addTimer(){let h=document.getElementById('timerHour').value;let m=document.getElementById('timerMin').value;let a=document.getElementById('timerAction').value;let d=document.getElementById('timerDays').value;
try{await fetch('/api/timers',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify({hour:parseInt(h),minute:parseInt(m),action:parseInt(a),days_mask:parseInt(d),enabled:true})});fetchTimers()}catch(e){}}
async function saveCyclic(){try{await fetch('/api/timers',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify({cyclic:{enabled:document.getElementById('cyclicEnabled').checked,duration_min:parseInt(document.getElementById('cyclicDuration').value)||30}})});footerEl.textContent='Ciclico salvo'}catch(e){footerEl.textContent='Erro: '+e.message}}
async function saveSync(){try{await fetch('/api/timers',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify({sync:{enabled:document.getElementById('syncEnabled').checked,target_id:document.getElementById('syncTargetId').value.trim()}})});footerEl.textContent='Sync salvo'}catch(e){footerEl.textContent='Erro: '+e.message}}
async function fetchRepeater(d){try{if(!d){let r=await fetch('/api/state');d=await r.json();}
let nav=document.getElementById('navRepeater');
if(d.repeater_supported&&d.repeater_enabled){nav.style.display='flex';document.getElementById('repState').textContent='ATIVADO';document.getElementById('repFwd').textContent=d.repeater_fwd||0;
let list=document.getElementById('repClientList');list.innerHTML='';
if(d.repeater_clients&&d.repeater_clients.length){d.repeater_clients.forEach(function(c){
let div=document.createElement('div');div.className='row';div.innerHTML='<span class="label">'+c.mac+'</span><span class="value">'+c.packets+' pkts</span>';list.appendChild(div)})}else{list.innerHTML='<div class="row"><span class="label">Nenhum cliente visto</span></div>'}}else{nav.style.display='none';let rs=document.getElementById('repState');if(rs)rs.textContent='DESATIVADO'}}catch(e){}}
async function toggleRepeater(){try{let cur=await fetch('/api/repeater');let cd=await cur.json();let en=!cd.enabled;let r=await fetch('/api/repeater',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify({enabled:en})});let d=await r.json();let rb=document.getElementById('repBtn');if(rb){rb.textContent=d.enabled?'DESATIVAR':'ATIVAR';rb.className='btn '+(d.enabled?'btn-danger':'btn-primary')}fetchRepeater(d)}catch(e){if(rb)rb.textContent='ERRO'}}
function doUpdate(){let f=document.getElementById('otaFile').files[0];let st=document.getElementById('otaStatus');if(!f){st.textContent='Selecione um .bin';return;}st.textContent='Enviando 0%...';let fd=new FormData();fd.append('firmware',f);let xhr=new XMLHttpRequest();xhr.open('POST','/api/ota');xhr.upload.onprogress=function(e){if(e.lengthComputable){let pct=Math.round(e.loaded*100/e.total);st.textContent='Enviando '+pct+'%...';}};xhr.onload=function(){try{let d=JSON.parse(xhr.responseText);if(d.status==='ok'){st.textContent='Concluído! Reiniciando...';}else{st.textContent='Erro: '+d.status;}}catch(e){st.textContent='Concluído! Reiniciando...';}};xhr.onerror=function(){st.textContent='Concluído! Reiniciando... (dispositivo vai voltar)';};xhr.send(fd);}
)=====";
#ifdef HABILITA_PINOS
static const char PAGE_SCRIPT_PINS[] PROGMEM = R"=====(
async function fetchPins(){try{let r=await fetch('/api/pins');let d=await r.json();let list=document.getElementById('pinList');if(!list)return;
list.innerHTML='';d.pins.forEach(function(p){
let div=document.createElement('div');div.className='row';
div.innerHTML='<span class="label">GPIO '+p.gpio+'</span><span class="value">'+(p.state?'HIGH':'LOW')+'</span>';
list.appendChild(div)})}catch(e){}}
setInterval(function(){fetchState();if(currentSection==='timer'||currentSection==='cyclic'||currentSection==='sync')fetchTimers();if(currentSection==='pins')fetchPins()},3000);
fetchState();fetchSettings();fetchTimers();if(currentSection==='pins')fetchPins();
)=====";
#else
static const char PAGE_SCRIPT_PINS[] PROGMEM = R"=====(
setInterval(function(){fetchState();if(currentSection==='timer'||currentSection==='cyclic'||currentSection==='sync')fetchTimers()},3000);
fetchState();fetchSettings();fetchTimers();
)=====";
#endif
static const char PAGE_DASHBOARD_END[] PROGMEM = R"=====(
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
</style>
</head>
<body>
<h1>API Lâmpada</h1>
<div class="endpoint"><div class="head"><span class="method get">GET</span><span class="path">/</span></div><div class="desc">Dashboard</div></div>
<div class="endpoint"><div class="head"><span class="method get">GET</span><span class="path">/api/state</span></div><div class="desc">Estado completo</div></div>
<div class="endpoint"><div class="head"><span class="method get">GET</span><span class="path">/api/relay</span></div><div class="desc">Estado do relé</div></div>
<div class="endpoint"><div class="head"><span class="method post">POST</span><span class="path">/api/relay</span></div><div class="desc">Controla relé</div></div>
<div class="endpoint"><div class="head"><span class="method post">POST</span><span class="path">/api/pin</span></div><div class="desc">Escrita GPIO</div></div>
<div class="endpoint"><div class="head"><span class="method get">GET</span><span class="path">/api/settings</span></div><div class="desc">Configurações</div></div>
<div class="endpoint"><div class="head"><span class="method post">POST</span><span class="path">/api/settings</span></div><div class="desc">Altera nome/pinos</div></div>
<div class="endpoint"><div class="head"><span class="method get">GET</span><span class="path">/api/timers</span></div><div class="desc">Lista timers</div></div>
<div class="endpoint"><div class="head"><span class="method post">POST</span><span class="path">/api/timers</span></div><div class="desc">Atualiza timer</div></div>
<div class="endpoint"><div class="head"><span class="method get">GET</span><span class="path">/api/timer/next</span></div><div class="desc">Próximo timer</div></div>
<div class="endpoint"><div class="head"><span class="method post">POST</span><span class="path">/api/restart</span></div><div class="desc">Reinicia</div></div>
<div class="endpoint"><div class="head"><span class="method post">POST</span><span class="path">/api/ota</span></div><div class="desc">OTA</div></div>
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
if(d.status==='ok'){showMsg('Conectando...','ok');setTimeout(function(){showMsg('AP reativado se falhar','ok')},2000)}else{showMsg('Erro: '+d.error,'err');loading(false)}}
catch(e){showMsg('Erro: '+e.message,'err');loading(false)}return false}
loadStatus();
</script>
</body>
</html>
)=====";
