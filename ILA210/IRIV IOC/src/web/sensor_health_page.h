#pragma once
static const char SENSOR_HEALTH_PAGE[] PROGMEM = R"html(
<!doctype html><html lang="en"><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1">
<title>Sensor communication alarm</title><style>body{font:16px system-ui;background:#101827;color:#edf2ff;max-width:700px;margin:30px auto;padding:20px}a{color:#82caff}label{display:block;margin:24px 0}input,select,button{font:inherit;padding:10px;max-width:100%;box-sizing:border-box}input[type=number],select{display:block;margin-top:8px}button{cursor:pointer}#result{white-space:pre-wrap}</style>
<a href="/settings">Settings</a> / <a href="/ioc">IOC</a><h1>Sensor communication alarm</h1>
<p id="status">Loading sensor status...</p><p id="identity"></p>
<form id="form"><label>Loss timeout (milliseconds, 2000–600000)<input id="timeout" type="number" min="2000" max="600000" step="1" required></label>
<label><input id="fault" type="checkbox"> Include sensor loss in the IOC combined alarm</label>
<label>DO0 / DO1 on sensor loss<select id="mode"><option value="0">Hold last state</option><option value="1">Turn enabled DO1 alarm ON; hold DO0</option><option value="2">Turn both outputs OFF</option></select></label>
<p>On recovery, vibration thresholds control DO0 / DO1 again. DO2 / DO3 follow their configured IOC alarm sources. Test the selected behavior with loads disconnected.</p>
<button id="save" disabled>Save settings</button></form><p id="result" role="status"></p>
<script>
const el=id=>document.getElementById(id);
async function request(options){const r=await fetch('/api/sensor_health',options);if(!r.ok)throw Error(await r.text());return r.json()}
request().then(d=>{el('timeout').value=d.timeout_ms;el('fault').checked=d.fault_alarm;el('mode').value=d.output_mode;el('status').textContent=d.sensor_fault?'Sensor communication LOST':'Sensor communication available / waiting for timeout';el('identity').textContent='MQTT command gateway_id: '+d.gateway_id;el('save').disabled=false}).catch(e=>el('result').textContent='Unable to load: '+e.message);
el('form').onsubmit=async e=>{e.preventDefault();el('save').disabled=true;try{await request({method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify({timeout_ms:Number(el('timeout').value),fault_alarm:el('fault').checked,output_mode:Number(el('mode').value)})});el('result').textContent='Settings saved and applied.'}catch(e){el('result').textContent='Save failed: '+e.message}finally{el('save').disabled=false}};
</script></html>
)html";
