const simulatorControls=String.raw`
<aside class="card" style="border:2px solid #d49b13;background:#fff8df;margin-bottom:20px" aria-label="Demo simulator">
<h2>SIMULATED IOC DEMO — no hardware connected</h2>
<p>All signals, captures and alarms below are simulated. Settings are held in memory and reset when the server restarts. Demo presets are enabled so you can try the pages.</p>
<details open><summary>Simulate signals and push-buttons</summary>
<div class="actions"><button type="button" data-demo="normal">Normal readings</button><button type="button" data-demo="pressure">High pressure</button><button type="button" data-demo="fault">Broken 4–20 mA loop</button><button type="button" data-demo="external">External alarm</button><button type="button" data-demo="capture">Press capture button (DI0)</button><button type="button" data-demo="ack">Press acknowledge button (DI1)</button></div>
<label for="demo-di">Digital input to simulate<select id="demo-di"><option value="0">DI0</option><option value="1">DI1</option><option value="2">DI2</option><option value="3">DI3</option><option value="4">DI4</option><option value="5">DI5</option><option value="6">DI6</option><option value="7">DI7</option><option value="8">DI8</option><option value="9">DI9</option><option value="10">DI10</option></select></label>
<div class="actions"><button type="button" data-demo="high">Set input HIGH</button><button type="button" data-demo="low">Set input LOW</button><button type="button" data-demo="pulse">Pulse input HIGH</button></div>
<small>These are simulated raw levels. Use LOW to activate a channel configured as active-low.</small>
<div class="pair"><label for="demo-an0">AN0 simulated signal (mA or V, matching chosen mode)<input id="demo-an0" type="number" min="0" max="43" step="0.1" value="12"></label><label for="demo-an1">AN1 simulated signal (mA or V, matching chosen mode)<input id="demo-an1" type="number" min="0" max="43" step="0.1" value="12"></label></div>
<div class="actions"><button type="button" data-demo="analog">Apply simulated analog values</button><button type="button" data-demo="sensorloss">Disconnect sensor</button><button type="button" data-demo="sensorrestore">Reconnect sensor</button><button type="button" data-demo="vibration">Vibration alarm ON</button><button type="button" data-demo="offline">Simulate device offline</button><button type="button" data-demo="online">Restore connection</button><button type="button" data-demo="reset" class="secondary">Reset demo presets</button></div>
<p id="demo-message" role="status" aria-live="polite">Try High pressure, wait for the alarm delay, then acknowledge. Select Normal readings to clear the condition.</p>
</details></aside>
<script>
(()=>{
const output=document.getElementById('demo-message');
async function action(value){const response=await fetch('/demo/action',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify(value)});if(!response.ok)throw new Error('Simulation command failed');}
document.querySelectorAll('[data-demo]').forEach(button=>button.addEventListener('click',async()=>{
button.disabled=true;
try{
const choice=button.dataset.demo;
if(choice==='normal')await action({type:'normal'});
if(choice==='pressure')await action({type:'analog',index:0,value:19});
if(choice==='fault')await action({type:'analog',index:0,value:0});
if(choice==='external')await action({type:'digital',index:2,active:true});
if(choice==='capture'||choice==='ack')await action({type:'digital',index:choice==='capture'?0:1,active:true,pulse:true});
if(['high','low','pulse'].includes(choice))await action({type:'digital',index:Number(document.getElementById('demo-di').value),active:choice!=='low',pulse:choice==='pulse'});
if(choice==='analog'){for(let i=0;i<2;i++){const input=document.getElementById('demo-an'+i);if(!input.reportValidity())return;await action({type:'analog',index:i,value:Number(input.value)});}}
if(choice==='sensorloss'||choice==='sensorrestore')await action({type:'sensor_loss',active:choice==='sensorloss'});
if(choice==='vibration')await action({type:'sensor',alarm:true});
if(choice==='offline'||choice==='online')await action({type:'offline',active:choice==='offline'});
if(choice==='reset'){await action({type:'reset'});location.reload();return;}
output.textContent='Simulated action applied: '+button.textContent+'. The IOC pages refresh automatically.';
}catch(error){output.textContent=error.message;}finally{button.disabled=false;}
}));
})();
</script>`;
module.exports={simulatorControls};
