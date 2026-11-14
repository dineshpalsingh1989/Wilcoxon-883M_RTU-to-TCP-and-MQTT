#pragma once
#include <Arduino.h>
static const char IOC_PAGE[] PROGMEM = R"ioc(
<!doctype html><html lang="en"><head><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1">
<title>IOC I/O &amp; Alarms</title><style>
*{box-sizing:border-box}body{margin:0;background:#f3f6fa;color:#203044;font:15px system-ui,sans-serif}header{background:#203448;color:white;padding:22px max(20px,calc((100% - 1100px)/2))}header a{color:#d6e7fa}h1{margin:12px 0 6px;font-size:27px}p{line-height:1.6}main{max-width:1140px;margin:auto;padding:20px}nav{display:flex;flex-wrap:wrap;gap:8px;margin:0 0 20px}nav a{color:#214267;background:white;border:1px solid #cad5e2;padding:11px 16px;border-radius:8px;text-decoration:none}nav a[aria-current=page]{background:#1765ad;color:white;border-color:#1765ad}.grid{display:grid;grid-template-columns:repeat(auto-fit,minmax(min(100%,310px),1fr));gap:16px}.card{background:white;border:1px solid #d7e0eb;border-radius:12px;padding:20px;min-width:0}h2{margin-top:0;font-size:20px}h3{margin-top:0}label{display:block;font-weight:600;margin-top:14px}input,select{font:inherit;width:100%;padding:10px;border:1px solid #aebed0;border-radius:6px;margin-top:6px;background:white;color:#203044}input[type=checkbox]{width:auto;margin-right:8px}.pair{display:grid;grid-template-columns:1fr 1fr;gap:12px}.status{font-weight:600;margin:12px 0;padding:10px;border-radius:6px;background:#eaf1f8;overflow-wrap:anywhere}.alarm{background:#fff0eb;color:#a32b11}.muted{color:#596d82}button{font:inherit;font-weight:600;padding:11px 16px;border:0;border-radius:7px;background:#1765ad;color:white;cursor:pointer}button.secondary{background:#dfe9f5;color:#214267}button:disabled{opacity:.5;cursor:default}.actions{display:flex;flex-wrap:wrap;gap:10px;align-items:center;margin:20px 0}a:focus-visible,button:focus-visible,input:focus-visible,select:focus-visible{outline:3px solid #f0ad21;outline-offset:3px}#message{position:sticky;top:0;background:#fff8df;border:1px solid #e1ce83;padding:12px;margin-bottom:16px;z-index:2}#message:empty{display:none}small{display:block;line-height:1.5;margin-top:8px}table{width:100%;border-collapse:collapse}td,th{text-align:left;padding:10px;border-bottom:1px solid #d7e0eb}.table-wrap{overflow:auto}@media(max-width:420px){main{padding:12px}.card{padding:15px}.pair{grid-template-columns:1fr}}
</style></head><body>
<header><a href="/settings">← Settings</a><h1>IOC I/O &amp; Alarms</h1><p>Connect machine signals, configure alarm limits, and choose what your outputs do.</p></header>
<main><nav aria-label="IOC pages"><a href="/ioc">Live status</a><a href="/ioc/inputs">Inputs &amp; push-buttons</a><a href="/ioc/analog">Analog &amp; 4–20 mA</a><a href="/ioc/outputs">Alarm outputs</a></nav>
<div id="message" role="status" aria-live="polite"></div><p id="connection" class="muted">Connecting to IOC…</p>
<section id="overview" hidden><div class="grid"><article class="card"><h2>Alarm summary</h2><div id="alarm-summary" class="status">Waiting…</div><button type="button" id="acknowledge">Acknowledge alarms</button><p>Acknowledgement silences the sounder. An active condition remains an alarm. The latched alarm clears once acknowledged and all conditions return to normal.</p></article><article class="card"><h2>Push-button actions</h2><p id="capture-counts">Waiting…</p><p>Capture requests are accepted only when the sensor capture is idle. A held button triggers once; it must be released before another request.</p><a href="/ioc/inputs">Configure push-buttons →</a></article></div><div class="card" style="margin-top:16px"><h2>Channel status</h2><div class="table-wrap"><table><thead><tr><th>Channel</th><th>Name</th><th>State / reading</th></tr></thead><tbody id="live-channels"></tbody></table></div></div></section>
<form id="config-form" hidden><p id="intro"></p><div id="presets" class="actions"></div><div id="editor" class="grid"></div><div class="actions"><button id="save" type="submit" disabled>Save and apply</button><button type="button" id="reload" class="secondary">Reload saved settings</button></div><p class="muted">All new channels and spare outputs start disabled. Saving applies only the choices shown on this page.</p></form>
<p><a href="/sensor_fault_settings">Sensor communication alarm settings</a></p>
<p class="muted">DO0 and DO1 remain assigned to vibration alert/alarm settings. <a href="/relay_settings">Configure vibration outputs →</a></p>
</main><script>
'use strict';
const path = location.pathname.replace(/\/$/,'') || '/ioc';
const mode = path.endsWith('/inputs') ? 'inputs' : path.endsWith('/analog') ? 'analog' : path.endsWith('/outputs') ? 'outputs' : 'status';
let config = null, state = null, timer, polling = false, saving = false, connected = false;
const $ = id => document.getElementById(id);
const message = text => { $('message').textContent = text; };
document.querySelectorAll('nav a').forEach(a => { if (a.getAttribute('href') === path) a.setAttribute('aria-current','page'); });
async function request(url, options = {}) {
    const controller = new AbortController(), timeout = setTimeout(() => controller.abort(), 8000);
    try {
        const response = await fetch(url, {...options, signal:controller.signal, cache:'no-store'});
        if (!response.ok) throw new Error(await response.text() || `Request failed (${response.status})`);
        return response;
    } finally { clearTimeout(timeout); }
}
function element(tag, text, className) { const el = document.createElement(tag); if (text !== undefined) el.textContent=text; if (className) el.className=className; return el; }
function field(parent, key, label, value, type = 'text', choices = null) {
    const wrap = element('label',label), input = element(choices ? 'select' : 'input');
    input.id = 'field-' + key; input.name = key; wrap.htmlFor=input.id;
    if (choices) choices.forEach((text,index) => { const option=element('option',text); option.value=index; input.append(option); });
    else { input.type=type; if(type==='number'){input.step='any';input.required=true;} if(type==='text')input.maxLength=key.endsWith('.unit')?12:24; }
    if(type==='checkbox') input.checked=!!value; else input.value=value;
    wrap.append(input); parent.append(wrap); return input;
}
function statusLine(card,index,kind) { const line=element('div','Waiting…','status'); line.dataset[kind]=index; card.append(line); }
function renderEditor() {
    $('editor').replaceChildren(); $('presets').replaceChildren();
    $('overview').hidden = mode !== 'status'; $('config-form').hidden = mode === 'status';
    const root=$('editor');
    if (mode==='inputs') {
        $('intro').textContent='Use an input as a machine-status signal, a delayed external alarm, a capture push-button, or an acknowledge push-button. Choose the polarity that matches your wiring.';
        const preset=element('button','Use DI0 for capture and DI1 for acknowledge','secondary'); preset.type='button';
        preset.onclick=()=>{document.getElementById('field-digital.0.action').value=3;document.getElementById('field-digital.1.action').value=4;message('Preset selected. Check polarity and save to apply.');};$('presets').append(preset);
        config.digital.forEach((d,i)=>{
            const card=element('article',undefined,'card');card.append(element('h2',`DI${i}`));statusLine(card,i,'di');
            field(card,`digital.${i}.name`,'Name',d.name);
            field(card,`digital.${i}.action`,'Function',d.action,'select',['Disabled','Monitor machine signal','External alarm','Capture push-button','Acknowledge push-button']);
            field(card,`digital.${i}.active_low`,'Active when input is LOW',d.active_low,'checkbox');
            const delay=field(card,`digital.${i}.delay_ms`,'Alarm delay (ms)',d.delay_ms,'number');delay.min=0;delay.max=60000;
            card.append(element('small','Inputs are debounced for 50 ms. Alarm delay applies to External alarm only. An input active at boot does not fire a push-button action.','muted'));root.append(card);
        });
    } else if(mode==='analog') {
        $('intro').textContent='AN0 and AN1 read a 4–20 mA transmitter or a 0–10 V signal. Set the physical AN-IN switch to Current or Voltage before connecting the signal. This page cannot switch the hardware mode.';
        config.analog.forEach((a,i)=>{
            const card=element('article',undefined,'card');card.append(element('h2',`AN${i}`));statusLine(card,i,'an');
            field(card,`analog.${i}.name`,'Measurement name',a.name);
            field(card,`analog.${i}.mode`,'Input mode',a.mode,'select',['Disabled','4–20 mA (switch: Current)','0–10 V (switch: Voltage)']);
            field(card,`analog.${i}.unit`,'Engineering unit, e.g. bar or °C',a.unit);
            const scaling=element('div',undefined,'pair');field(scaling,`analog.${i}.minimum`,'Value at 4 mA / 0 V',a.minimum,'number');field(scaling,`analog.${i}.maximum`,'Value at 20 mA / 10 V',a.maximum,'number');card.append(scaling);
            field(card,`analog.${i}.low_enabled`,'Enable low alarm',a.low_enabled,'checkbox');field(card,`analog.${i}.low`,'Low threshold',a.low,'number');
            field(card,`analog.${i}.high_enabled`,'Enable high alarm',a.high_enabled,'checkbox');field(card,`analog.${i}.high`,'High threshold',a.high,'number');
            const h=field(card,`analog.${i}.hysteresis`,'Alarm reset hysteresis',a.hysteresis,'number');h.min=0;
            const d=field(card,`analog.${i}.delay_ms`,'Alarm delay (ms)',a.delay_ms,'number');d.min=0;d.max=60000;
            field(card,`analog.${i}.fault_alarm`,'Alarm on current-loop fault / ADC saturation',a.fault_alarm,'checkbox');
            card.append(element('small','Current below 3.6 mA or above 21 mA is flagged as a loop fault. Verify the transmitter and calibrate before using engineering thresholds.','muted'));root.append(card);
        });
        const calibration=element('article',undefined,'card');calibration.append(element('h2','Analog calibration'));
        const ref=field(calibration,'reference_mv','Measured ADC reference (mV)',config.reference_mv,'number');ref.min=3000;ref.max=3600;
        calibration.append(element('p','Default 3320 mV follows Cytron’s example. Use a measured reference or compare with a calibrated current source.'));root.append(calibration);
    } else if(mode==='outputs') {
        $('intro').textContent='Use DO2 and DO3 for an alarm beacon, a sounder, or to follow a monitored digital input. DO0 and DO1 keep their existing vibration alarm assignments.';
        const preset=element('button','Use DO2 as latched beacon and DO3 as sounder','secondary');preset.type='button';preset.onclick=()=>{document.getElementById('field-outputs.0.source').value=2;document.getElementById('field-outputs.1.source').value=3;message('Beacon/sounder preset selected. Save to apply.');};$('presets').append(preset);
        config.outputs.forEach((o,i)=>{
            const card=element('article',undefined,'card');card.append(element('h2',`DO${i+2}`));statusLine(card,i,'out');
            field(card,`outputs.${i}.source`,'Output function',o.source,'select',['Disabled / OFF','Any active alarm','Latched alarm beacon','Unacknowledged alarm sounder','Follow monitored digital input']);
            field(card,`outputs.${i}.input`,'Input to follow',o.input,'select',Array.from({length:11},(_,n)=>`DI${n}`));
            card.append(element('p','Any alarm includes the enabled I/O alarms and active vibration outputs. Acknowledge silences the sounder; it does not bypass active conditions.'));
            card.append(element('small','Follow-input mode requires that DI to be enabled. Outputs start OFF during boot and apply their saved function when sampling starts.','muted'));root.append(card);
        });
    }
}
function updateStatus() {
    if(!state || !config)return;
    const valid=state.ready && state.age_ms<1000;
    $('connection').textContent=valid?'IOC connected · live inputs and alarm state':'IOC connected · waiting for fresh I/O samples';
    $('save').disabled=!valid || saving;
    $('acknowledge').disabled=!valid || saving;
    const alarm=state.alarm?'ALARM ACTIVE':state.latched?'Alarm cleared · acknowledgement required':'Normal';
    $('alarm-summary').textContent=alarm+(state.acknowledged?' · acknowledged':'');$('alarm-summary').classList.toggle('alarm',!!state.latched);
    $('capture-counts').textContent=`Capture requests: ${state.capture_accepted} accepted · ${state.capture_rejected} rejected (busy).`;
    const rows=[];
    config.digital.forEach((d,i)=>{
        const text=d.action===0?'Disabled':state.digital[i].alarm?'ALARM':state.digital[i].active?'Active':'Inactive';
        rows.push([`DI${i}`,d.name,text]);const el=document.querySelector(`[data-di="${i}"]`);if(el){el.textContent=valid?text:'Waiting for fresh samples';el.classList.toggle('alarm',state.digital[i].alarm);}
    });
    config.analog.forEach((a,i)=>{
        const reading=state.analog[i];
        const text=!a.mode?'Disabled':`${reading.value.toFixed(2)} ${a.unit} · ${reading.measured.toFixed(3)} ${a.mode===1?'mA':'V'}${reading.fault?' · SIGNAL FAULT':''}${reading.alarm?' · ALARM':''}`;
        rows.push([`AN${i}`,a.name,text]);const el=document.querySelector(`[data-an="${i}"]`);if(el){el.textContent=valid?text:'Waiting for fresh samples';el.classList.toggle('alarm',reading.alarm||reading.fault);}
    });
    rows.push(['Sensor communication','Alarm source',state.sensor_fault?'LOST':'OK']);
    rows.push(['DO0','Vibration alert',state.sensor_alert?'ON':'OFF'],['DO1','Vibration alarm',state.sensor_alarm?'ON':'OFF']);
    state.outputs.forEach((active,i)=>{rows.push([`DO${i+2}`,'Configured I/O function',active?'ON':'OFF']);const el=document.querySelector(`[data-out="${i}"]`);if(el)el.textContent=valid?(active?'ON':'OFF'):'Waiting for fresh samples';});
    const body=$('live-channels');body.replaceChildren();for(const row of rows){const tr=element('tr');row.forEach((value,index)=>tr.append(element('td',!valid&&index===2?'Unknown':value)));body.append(tr);}
}
async function load(reload=false) {
    const data=await (await request('/api/ioc')).json();
    if(!data.supported){$('connection').textContent='IOC I/O is available on the IRIV IO Controller build.';$('config-form').hidden=true;$('overview').hidden=true;return;}
    if(config&&!reload&&data.config.revision<config.revision)return;
    if(!config||reload){config=data.config;renderEditor();message('');}
    else if(config.revision!==data.config.revision)message('Saved settings changed. Reload before editing or saving.');
    state=data.state;connected=true;updateStatus();
}
function markUnavailable(){connected=false;$('connection').textContent='IOC unreachable ? retrying';$('alarm-summary').textContent='Unknown ? connection lost';document.querySelectorAll('[data-di],[data-an],[data-out]').forEach(el=>el.textContent='Unknown ? connection lost');document.querySelectorAll('#live-channels tr').forEach(row=>{if(row.cells[2])row.cells[2].textContent='Unknown';});$('save').disabled=true;$('acknowledge').disabled=true;}
async function poll(){if(polling)return;polling=true;try{await load();}catch(error){connected=false;$('connection').textContent='IOC unreachable · retrying';$('save').disabled=true;$('acknowledge').disabled=true;}finally{polling=false;timer=setTimeout(poll,1000);}}
$('reload').onclick=async()=>{try{await load(true);}catch(error){message('Reload failed: '+error.message);}};
$('config-form').onsubmit=async event=>{
    event.preventDefault();if(!config||!connected||saving)return;
    const candidate=JSON.parse(JSON.stringify(config));
    $('config-form').querySelectorAll('[name]').forEach(input=>{const keys=input.name.split('.');let target=candidate;keys.slice(0,-1).forEach(key=>target=target[key]);target[keys[keys.length-1]]=input.type==='checkbox'?input.checked:input.tagName==='SELECT'||input.type==='number'?Number(input.value):input.value;});
    saving=true;$('save').disabled=true;message('Saving…');
    try{const text=await (await request('/api/ioc/config',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify(candidate)})).text();await load(true);message(text);}catch(error){message('Save failed: '+error.message);}finally{saving=false;$('save').disabled=!connected;}
};
$('acknowledge').onclick=async()=>{try{message(await (await request('/api/ioc/acknowledge',{method:'POST'})).text());await load();}catch(error){message('Acknowledge failed: '+error.message);}};
poll();
</script></body></html>
)ioc";
