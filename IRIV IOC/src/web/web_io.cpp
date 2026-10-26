#include "core/modules.h"

#if GATEWAY_ETHERNET
#include <cmath>
// IOC I/O page: live relay, input and analog state, alarm acknowledge and analog input settings.

namespace {
void sendIoJson(JsonDocument& doc) {
    String payload;
    serializeJson(doc, payload);
    server.sendHeader("Cache-Control", "no-store");
    server.send(200, "application/json", payload);
}

bool readNumber(const String& text, float& out) {
    if (!text.length()) return false;
    char* end = nullptr;
    out = strtof(text.c_str(), &end);
    return end && *end == '\0' && std::isfinite(out) && fabsf(out) <= 1000000.0f;
}

bool readText(const String& text, char* out, size_t capacity) {
    if (text.length() >= capacity) return false;
    for (size_t i = 0; i < text.length(); ++i) if (uint8_t(text[i]) < 32) return false;
    snprintf(out, capacity, "%s", text.c_str());
    return true;
}
}

void handleIoPage() {
    server.setContentLength(CONTENT_LENGTH_UNKNOWN);
    server.send(200, "text/html", "");
    server.sendContent(R"rawliteral(
<!DOCTYPE html><html lang="en"><head><title>IOC I/O</title><meta name="viewport" content="width=device-width, initial-scale=1">
<style>
:root{color-scheme:light;--ink:#192f3d;--muted:#5b6b75;--line:#e6edf1;--brand:#147d78;--ok:#1d7a45;--ok-bg:#e6f4ec;--bad:#b3261e;--bad-bg:#fbe9e7;--idle-bg:#eef2f4;--warn-bg:#fff3cd}
*{box-sizing:border-box}body{font-family:-apple-system,BlinkMacSystemFont,"Segoe UI",Roboto,Helvetica,Arial,sans-serif;margin:0;background:#f2f5f7;color:var(--ink);line-height:1.5;padding:20px 16px}
.container{max-width:720px;margin:0 auto}
.header{background:linear-gradient(115deg,#102c3b,#194655);color:#f8fafc;border-radius:16px;padding:22px 20px;text-align:center}.header h2{margin:0;font-size:24px;letter-spacing:-.5px}.header p{margin:4px 0 0;font-size:14px;color:#cfe3ea}
.card{background:white;border:1px solid #dfe8ed;border-radius:14px;box-shadow:0 8px 28px #173c490a;margin-top:14px;padding:16px 18px}
.card-head{display:flex;align-items:center;gap:8px;flex-wrap:wrap}.card-head h3{margin:0;font-size:17px;flex:1 1 auto}
.note{margin:4px 0 10px;font-size:13px;color:var(--muted)}
.pill{font-size:12px;font-weight:600;border-radius:999px;padding:2px 10px;background:var(--idle-bg);color:var(--muted)}.pill.on{background:var(--bad-bg);color:var(--bad)}.pill.ok{background:var(--ok-bg);color:var(--ok)}
.chips{display:grid;grid-template-columns:repeat(auto-fill,minmax(150px,1fr));gap:8px}
.chip{border:1px solid var(--line);border-radius:10px;padding:8px 10px;font-size:13px}.chip b{display:block;font-size:14px}.chip.on{background:var(--ok-bg);border-color:#bfe3cd}.chip.alert{background:var(--bad-bg);border-color:#f3c7bd}
table{width:100%;border-collapse:collapse;font-size:14px}td{padding:6px 0;border-bottom:1px solid var(--line);vertical-align:top}tr:last-child td{border-bottom:none}td:first-child{color:var(--muted);width:44%;padding-right:10px}
.grid{display:grid;grid-template-columns:1fr 1fr;gap:10px}label{display:block;font-size:13px;font-weight:600;margin:8px 0 4px}
input,select{width:100%;min-height:44px;padding:8px 10px;border:1px solid #c5d1d8;border-radius:8px;font:inherit;background:white}
button{font:inherit;min-height:44px;padding:8px 14px;border:none;border-radius:8px;background:var(--brand);color:white;font-weight:600;cursor:pointer}button:disabled{opacity:.55;cursor:not-allowed}button.secondary{background:var(--idle-bg);color:var(--ink)}
.actions{display:flex;flex-wrap:wrap;gap:8px;margin-top:12px;align-items:center}.status{font-size:14px}.status:empty{display:none}
.value{font-size:26px;font-weight:700}.value small{font-size:14px;color:var(--muted);font-weight:500}
.nav-link{display:inline-block;color:var(--brand);font-weight:600;text-decoration:none;padding:16px 0}
a:focus-visible,button:focus-visible,input:focus-visible,select:focus-visible{outline:3px solid #42a8bd;outline-offset:3px}
@media(max-width:520px){body{padding:12px}.card{padding:14px}.grid{grid-template-columns:1fr}td:first-child{width:50%}}
</style></head><body><div class="container">
<div class="header"><h2>IOC I/O</h2><p>Relays, digital inputs and 4-20 mA / 0-10 V inputs of the IRIV IO Controller</p></div>
<section class="card" aria-labelledby="out-title"><div class="card-head"><h3 id="out-title">Relay outputs (DO0-DO3)</h3><span class="pill" id="alarm-pill">--</span></div>
<p class="note">Choose what switches each solid-state relay (50 V / 500 mA max). A source can drive only one output; choices already used by another output are greyed out. <b>Dashboard alarm</b> uses the vibration/temperature alarm limits from the dashboard. <b>883M value</b> compares one sensor value on one axis (or the highest of X, Y, Z) with your threshold. <b>AN0/AN1</b> compare the scaled analog value. A latched output stays on until acknowledged here, with a short press of the USER button or a pulse on DI1; acknowledging also silences the buzzer and retries a paused sensor.</p>
<div class="chips" id="outputs-live"></div>
<form id="outputs-form" onsubmit="saveOutputs(event)"><div class="grid" id="outputs-form-fields"></div>
<div class="actions"><button type="submit" id="save-outputs">Save output settings</button><button type="button" class="secondary" id="ack" onclick="acknowledgeAlarm()">Acknowledge</button><span class="status" id="out-status" role="status"></span><span class="status" id="ack-status" role="status"></span></div></form></section>
<section class="card" aria-labelledby="di-title"><div class="card-head"><h3 id="di-title">Digital inputs (DI0-DI10)</h3></div>
<p class="note">DI0: a rising edge starts an XYZ capture (ignored while busy). DI1: a rising edge acknowledges the alarm. DI2-DI10 are reported only. Captures from DI0: <span id="di-captures">--</span>.</p>
<div class="chips" id="inputs"></div></section>
<section class="card" aria-labelledby="an-title"><div class="card-head"><h3 id="an-title">Analog inputs (AN0 / AN1)</h3></div>
<p class="note">Set the mode to match the AN-IN MODE DIP switch under the IOC. Scaling maps 4 mA / 0 V to the minimum and 20 mA / 10 V to the maximum. A 4-20 mA input below 3.6 mA or above 21 mA, or any over-range reading, shows a fault. Readings use the nominal ADC reference and are not calibrated.</p>
<div class="grid" id="analog-live"></div>
<form id="analog-form" onsubmit="saveAnalog(event)">
<div class="grid" id="analog-form-fields"></div>
<div class="actions"><button type="submit" id="save-analog">Save analog settings</button><span class="status" id="save-status" role="status"></span></div></form></section>
<a href="/settings" class="nav-link">&larr; Back to Settings</a></div>
<script>
const inputNames=['DI0 - start XYZ capture','DI1 - acknowledge alarm','DI2','DI3','DI4','DI5','DI6','DI7','DI8','DI9','DI10'];
const set=(id,text)=>{document.getElementById(id).textContent=text;};
let formBuilt=false,outputsBuilt=false,metrics=[],analogNames=['AN0','AN1'],analogUnits=['',''];
const sourceNames=['Off','Dashboard alarm','Sensor fault','883M value','AN0 (analog)','AN1 (analog)'];
const axisNames=['X','Y','Z','Any axis (highest)'];
function sourceText(r,i){
  if(r.source_id===3){const m=metrics[r.metric_index]||{};return (m.per_axis?axisNames[r.axis_id]+' ':'')+m.label+' '+(r.direction_id?'<=':'>=')+' '+r.threshold_value+' '+(m.unit||'');}
  if(r.source_id>=4){const a=r.source_id-4;return analogNames[a]+' '+(r.direction_id?'<=':'>=')+' '+r.threshold_value+' '+analogUnits[a];}
  return sourceNames[r.source_id];
}
function buildOutputs(rules){
  const fields=document.getElementById('outputs-form-fields');fields.innerHTML='';
  rules.forEach((r,i)=>{
    const metricOptions=metrics.map((m,k)=>'<option value="'+k+'">'+esc(m.label+(m.unit?' ('+m.unit+')':''))+'</option>').join('');
    fields.insertAdjacentHTML('beforeend','<fieldset style="border:1px solid #e6edf1;border-radius:10px;padding:10px 12px;margin:10px 0 0"><legend style="font-weight:700">DO'+i+'</legend>'+
      '<label for="do'+i+'_src">Switches on for</label><select id="do'+i+'_src" name="do'+i+'_src" onchange="updateOutputForm()">'+sourceNames.map((n,k)=>'<option value="'+k+'">'+n+'</option>').join('')+'</select>'+
      '<div id="do'+i+'_metric_fields"><label for="do'+i+'_met">883M value</label><select id="do'+i+'_met" name="do'+i+'_met" onchange="updateOutputForm()">'+metricOptions+'</select>'+
      '<label for="do'+i+'_axis">Axis</label><select id="do'+i+'_axis" name="do'+i+'_axis" onchange="updateOutputForm()">'+axisNames.map((n,k)=>'<option value="'+k+'">'+n+'</option>').join('')+'</select></div>'+
      '<div id="do'+i+'_limit_fields"><label for="do'+i+'_dir">Condition</label><select id="do'+i+'_dir" name="do'+i+'_dir"><option value="0">At or above threshold</option><option value="1">At or below threshold</option></select>'+
      '<label for="do'+i+'_thr">Threshold <span id="do'+i+'_unit"></span></label><input id="do'+i+'_thr" name="do'+i+'_thr" type="number" step="any" required>'+
      '<label for="do'+i+'_hys">Hysteresis (release margin)</label><input id="do'+i+'_hys" name="do'+i+'_hys" type="number" step="any" min="0" required>'+
      '<label for="do'+i+'_dly">Delay before switching (s)</label><input id="do'+i+'_dly" name="do'+i+'_dly" type="number" min="0" max="3600" step="1" required></div>'+
      '<label style="display:flex;gap:8px;align-items:center;font-weight:500"><input type="checkbox" id="do'+i+'_lat" name="do'+i+'_lat" value="1" style="width:auto;min-height:0"> Latch until acknowledged</label></fieldset>');
    document.getElementById('do'+i+'_src').value=String(r.source_id);
    document.getElementById('do'+i+'_met').value=String(r.metric_index);
    document.getElementById('do'+i+'_axis').value=String(r.axis_id);
    document.getElementById('do'+i+'_dir').value=String(r.direction_id);
    document.getElementById('do'+i+'_thr').value=r.threshold_value;
    document.getElementById('do'+i+'_hys').value=r.hysteresis_value;
    document.getElementById('do'+i+'_dly').value=r.delay_value;
    document.getElementById('do'+i+'_lat').checked=!!r.latch;
  });
  outputsBuilt=true;updateOutputForm();
}
// One output per source: disable sources, and 883M value/axis pairs, used by another output.
function updateOutputForm(){
  const n=4,get=(i,f)=>Number(document.getElementById('do'+i+'_'+f).value);
  for(let i=0;i<n;i++){
    const src=get(i,'src'),m=metrics[get(i,'met')]||{};
    document.getElementById('do'+i+'_metric_fields').hidden=src!==3;
    document.getElementById('do'+i+'_axis').disabled=src!==3||!m.per_axis;
    document.getElementById('do'+i+'_limit_fields').hidden=src<3;
    document.getElementById('do'+i+'_lat').disabled=src===0;
    document.getElementById('do'+i+'_unit').textContent=src===3?(m.unit?'('+m.unit+')':''):src>=4?(analogUnits[src-4]?'('+analogUnits[src-4]+')':''):'';
    [...document.getElementById('do'+i+'_src').options].forEach(o=>{const v=Number(o.value);
      o.disabled=v!==0&&v!==3&&[...Array(n).keys()].some(k=>k!==i&&get(k,'src')===v);});
    [...document.getElementById('do'+i+'_axis').options].forEach(o=>{const a=Number(o.value);
      o.disabled=[...Array(n).keys()].some(k=>k!==i&&get(k,'src')===3&&get(k,'met')===get(i,'met')&&(get(k,'axis')===a||get(k,'axis')===3||a===3));});
  }
}
function saveOutputs(e){
  e.preventDefault();
  const b=document.getElementById('save-outputs');b.disabled=true;set('out-status','Saving...');
  fetch('/io_outputs',{method:'POST',body:new URLSearchParams(new FormData(document.getElementById('outputs-form')))})
    .then(r=>r.text().then(t=>{set('out-status',r.ok?t:'Not saved: '+t);if(r.ok){outputsBuilt=false;refresh();}}))
    .catch(()=>set('out-status','Not saved: gateway unavailable')).finally(()=>{b.disabled=false;});
}
function esc(text){const d=document.createElement('div');d.textContent=text;return d.innerHTML;}
function buildForm(analog){
  const fields=document.getElementById('analog-form-fields');fields.innerHTML='';
  analog.forEach((a,i)=>{
    const mode=a.mode==='4-20mA'?1:a.mode==='0-10V'?2:0;
    fields.insertAdjacentHTML('beforeend','<fieldset style="border:1px solid #e6edf1;border-radius:10px;padding:10px 12px;margin:10px 0 0"><legend style="font-weight:700">AN'+i+'</legend>'+
      '<label for="an'+i+'_mode">Mode</label><select id="an'+i+'_mode" name="an'+i+'_mode"><option value="0">Off</option><option value="1">4-20 mA</option><option value="2">0-10 V</option></select>'+
      '<label for="an'+i+'_name">Name</label><input id="an'+i+'_name" name="an'+i+'_name" maxlength="24" value="'+esc(a.name)+'">'+
      '<label for="an'+i+'_unit">Unit</label><input id="an'+i+'_unit" name="an'+i+'_unit" maxlength="12" value="'+esc(a.unit)+'" placeholder="e.g. RPM">'+
      '<label for="an'+i+'_min">Value at 4 mA / 0 V</label><input id="an'+i+'_min" name="an'+i+'_min" type="number" step="any" required value="'+a.minimum+'">'+
      '<label for="an'+i+'_max">Value at 20 mA / 10 V</label><input id="an'+i+'_max" name="an'+i+'_max" type="number" step="any" required value="'+a.maximum+'"></fieldset>');
    document.getElementById('an'+i+'_mode').value=String(mode);
  });
  formBuilt=true;
}
function render(d){
  if(d.metrics)metrics=d.metrics;
  d.analog.forEach((a,i)=>{analogNames[i]=a.name;analogUnits[i]=a.unit;});
  const on=d.outputs.filter(Boolean).length;
  const pill=document.getElementById('alarm-pill');pill.textContent=on?on+' relay(s) ON':'All relays off';pill.className='pill '+(on?'on':'ok');
  const outs=document.getElementById('outputs-live');outs.textContent='';
  d.output_rules.forEach((r,i)=>{
    const c=document.createElement('div');c.className='chip'+(d.outputs[i]?' alert':'');
    const state=r.source_id===0?'Not used':d.outputs[i]?(r.condition?'ON':'ON (latched'+(r.acknowledged?', acknowledged':'')+')'):'Off';
    c.innerHTML='<b>DO'+i+' - '+esc(state)+'</b>'+esc(sourceText(r,i));outs.appendChild(c);
  });
  if(!outputsBuilt)buildOutputs(d.output_rules);
  set('di-captures',d.capture_accepted+' started, '+d.capture_rejected+' ignored');
  const inputs=document.getElementById('inputs');inputs.textContent='';
  d.inputs.forEach((on,i)=>{const c=document.createElement('div');c.className='chip'+(on?' on':'');c.innerHTML='<b>'+esc(inputNames[i])+'</b>'+(on?'Active':'Inactive');inputs.appendChild(c);});
  const live=document.getElementById('analog-live');live.textContent='';
  d.analog.forEach((a,i)=>{
    const c=document.createElement('div');c.className='chip'+(a.fault?' alert':'');
    const reading=a.mode==='off'?'Off':(a.fault?'Fault - ':'')+Number(a.value).toFixed(2)+' '+a.unit;
    const input=a.mode==='off'?'':' ('+Number(a.input).toFixed(2)+(a.mode==='4-20mA'?' mA':' V')+')';
    c.innerHTML='<b>AN'+i+' - '+esc(a.name)+'</b><span class="value">'+esc(reading)+'</span><small style="display:block;color:#5b6b75">'+esc(a.mode==='off'?'Not used':a.mode+input)+'</small>';
    live.appendChild(c);
  });
  if(!formBuilt)buildForm(d.analog);
}
function refresh(){fetch('/io_status',{cache:'no-store'}).then(r=>{if(!r.ok)throw 0;return r.json();}).then(render).catch(()=>set('ack-status','Gateway unavailable or restarting.'));}
function acknowledgeAlarm(){
  const b=document.getElementById('ack');b.disabled=true;
  fetch('/io_ack',{method:'POST'}).then(r=>r.text().then(t=>set('ack-status',r.ok?t:'Failed: '+t))).catch(()=>set('ack-status','Failed: gateway unavailable')).finally(()=>{b.disabled=false;refresh();});
}
function saveAnalog(e){
  e.preventDefault();
  const b=document.getElementById('save-analog');b.disabled=true;set('save-status','Saving...');
  fetch('/io_config',{method:'POST',body:new URLSearchParams(new FormData(document.getElementById('analog-form')))})
    .then(r=>r.text().then(t=>{set('save-status',r.ok?t:'Not saved: '+t);if(r.ok){formBuilt=false;refresh();}}))
    .catch(()=>set('save-status','Not saved: gateway unavailable')).finally(()=>{b.disabled=false;});
}
document.addEventListener('DOMContentLoaded',()=>{refresh();setInterval(refresh,2000);});
</script></body></html>)rawliteral");
    server.sendContent("");
}

void handleIoStatus() {
    io::Snapshot s;
    if (!io::snapshot(s) || !s.ready) { server.send(503, "text/plain", "I/O not ready"); return; }
    DynamicJsonDocument doc(6144);
    JsonObject out = doc.to<JsonObject>();
    io::writeJson(out, s, true);
    JsonArray metrics = out.createNestedArray("metrics");
    for (const io::MetricInfo& m : io::Metrics) {
        JsonObject row = metrics.createNestedObject();
        row["id"] = m.id; row["label"] = m.label; row["unit"] = m.unit; row["per_axis"] = m.perAxis;
    }
    sendIoJson(doc);
}

void handleIoConfig() {
    io::AnalogConfig analog[io::AnalogCount];
    for (int i = 0; i < io::AnalogCount; ++i) {
        const String key = "an" + String(i) + "_";
        const long mode = server.arg(key + "mode").toInt();
        float minimum = 0, maximum = 0;
        if (mode < io::AnalogOff || mode > io::AnalogVoltage || server.arg(key + "mode").length() != 1 ||
            !readText(server.arg(key + "name"), analog[i].name, sizeof(analog[i].name)) ||
            !readText(server.arg(key + "unit"), analog[i].unit, sizeof(analog[i].unit)) ||
            !readNumber(server.arg(key + "min"), minimum) || !readNumber(server.arg(key + "max"), maximum) ||
            minimum >= maximum) {
            server.send(400, "text/plain", "AN" + String(i) + ": check the mode, name (24 characters), unit (12) and that the minimum is below the maximum.");
            return;
        }
        if (!analog[i].name[0]) snprintf(analog[i].name, sizeof(analog[i].name), "AN%d", i);
        analog[i].mode = uint8_t(mode);
        analog[i].minimum = minimum;
        analog[i].maximum = maximum;
    }
    if (!io::setAnalogConfig(analog)) { server.send(503, "text/plain", "Settings storage busy; nothing was changed."); return; }
    logMessage("[I/O] Analog input settings saved");
    server.send(200, "text/plain", "Analog settings saved and applied.");
}

void handleIoAcknowledge() {
    acknowledgeAlarms("web page");
    server.send(200, "text/plain", "Acknowledged. The alarm relay turns off once the alarm has cleared.");
}
void handleIoOutputs() {
    io::OutputConfig output[io::OutputCount];
    for (int i = 0; i < io::OutputCount; ++i) {
        const String key = "do" + String(i) + "_";
        io::OutputConfig& o = output[i];
        o.source = uint8_t(server.arg(key + "src").toInt());
        o.latch = server.hasArg(key + "lat") && o.source != io::SourceOff;
        if (o.source == io::SourceMetric) {
            o.metric = uint8_t(server.arg(key + "met").toInt());
            o.axis = server.hasArg(key + "axis") ? uint8_t(server.arg(key + "axis").toInt()) : io::AxisAny;
        }
        if (o.source >= io::SourceMetric) {
            float delay = 0;
            o.direction = uint8_t(server.arg(key + "dir").toInt());
            if (!readNumber(server.arg(key + "thr"), o.threshold) || !readNumber(server.arg(key + "hys"), o.hysteresis) ||
                !readNumber(server.arg(key + "dly"), delay) || delay < 0 || delay > 3600 || delay != floorf(delay)) {
                server.send(400, "text/plain", "DO" + String(i) + ": enter a threshold, a hysteresis of 0 or more and a delay of 0-3600 s.");
                return;
            }
            o.delaySeconds = uint16_t(delay);
        }
    }
    if (const char* error = io::validateOutputs(output)) { server.send(400, "text/plain", error); return; }
    if (!io::setOutputConfig(output)) { server.send(503, "text/plain", "Settings storage busy; nothing was changed."); return; }
    logMessage("[I/O] Output settings saved");
    server.send(200, "text/plain", "Output settings saved and applied.");
}
#endif
