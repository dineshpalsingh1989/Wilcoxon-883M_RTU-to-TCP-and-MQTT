#include "core/modules.h"

void handleMonitoringScript() {
    server.send_P(200, "application/javascript", R"rawliteral(
let monitoringSnapshot = null, monitoringPending = false, alarmFormLoaded = false;
function drawTrend(id, series, colors) {
 const canvas=document.getElementById(id), rect=canvas.getBoundingClientRect();
 if(!rect.width)return;
 const ratio=window.devicePixelRatio||1;canvas.width=rect.width*ratio;canvas.height=200*ratio;
 const ctx=canvas.getContext('2d');ctx.scale(ratio,ratio);
 const width=rect.width,height=200,pad=42,points=monitoringSnapshot.trends;
 ctx.font='11px system-ui';ctx.fillStyle='#637c8b';
 if(points.length<2){ctx.fillText('Waiting for at least two sensor readings...',12,100);return;}
 const values=points.flatMap(p=>series.map(key=>p[key])).filter(Number.isFinite);
 const min=Math.min(0,...values),max=Math.max(1,...values)*1.12;
 const oldest=points[0].age_ms,newest=points[points.length-1].age_ms,span=Math.max(1,oldest-newest);
 for(let i=0;i<4;i++){const y=16+i*(height-pad-16)/3;ctx.strokeStyle='#e8eff2';ctx.beginPath();ctx.moveTo(pad,y);ctx.lineTo(width-12,y);ctx.stroke();ctx.fillStyle='#637c8b';ctx.fillText((max-(max-min)*i/3).toFixed(1),2,y+4);}
 series.forEach((key,index)=>{ctx.strokeStyle=colors[index];ctx.lineWidth=2;ctx.beginPath();let previous=null;points.forEach(p=>{if(!Number.isFinite(p[key])){previous=null;return;}const x=pad+(oldest-p.age_ms)/span*(width-pad-12),y=16+(max-p[key])/(max-min)*(height-pad-16);if(!previous||previous.age_ms-p.age_ms>Math.max(15000,span/points.length*3))ctx.moveTo(x,y);else ctx.lineTo(x,y);previous=p;});ctx.stroke();});
 ctx.fillStyle='#637c8b';ctx.fillText((oldest/1000).toFixed(0)+'s ago',pad,height-8);ctx.fillText((newest/1000).toFixed(0)+'s ago',Math.max(pad,width-65),height-8);
}
function redrawTrends(){if(!monitoringSnapshot)return;drawTrend('velocity-chart',['x','y','z'],['#147d78','#5688c3','#9680c5']);drawTrend('temperature-chart',['temperature'],['#b58136']);}
async function updateMonitoring(){
 if(monitoringPending)return;monitoringPending=true;
 try{
  const response=await fetch('/monitoring',{signal:AbortSignal.timeout(10000)});if(!response.ok)throw Error('Monitoring unavailable');
  const data=await response.json();monitoringSnapshot=data;
  const h=data.health,a=data.alarm;
  document.getElementById('sensor-health').textContent=h.state;
  document.getElementById('health-details').textContent=(h.has_reading?'Last successful read '+Math.floor(h.age_ms/1000)+'s ago':'No successful metric read yet')+' · '+h.reads+' attempts · '+h.failures+' failed · '+h.timeouts+' timed out · '+h.consecutive_errors+' consecutive errors'+(h.last_error?' · Modbus error 0x'+h.last_error.toString(16):'')+(h.capturing?' · Capture in progress':'')+(h.updating?' · Updating firmware':'');
  const state=document.getElementById('alarm-state');state.textContent=a.state;state.className=/active|Unknown/.test(a.state)?'alarm-active':'';
  if(!alarmFormLoaded){document.getElementById('alarm-enabled').checked=a.enabled;document.getElementById('alarm-velocity').value=a.velocity;document.getElementById('alarm-temperature').value=a.temperature;document.getElementById('alarm-delay').value=a.delay_seconds;alarmFormLoaded=true;}
  const tbody=document.getElementById('capture-history');tbody.replaceChildren();
  if(!data.captures.length){const row=tbody.insertRow();const cell=row.insertCell();cell.colSpan=4;cell.textContent='No captures yet.';}
  data.captures.forEach(c=>{const row=tbody.insertRow();[c.time,c.axis,c.result,c.transmission].forEach(value=>row.insertCell().textContent=value);});
  document.getElementById('trend-summary').textContent=data.trends.length+' sensor snapshots available.';
  redrawTrends();
 }catch(error){document.getElementById('sensor-health').textContent='Health updates unavailable';document.getElementById('alarm-state').textContent='Unknown - monitoring connection lost';document.getElementById('alarm-state').className='alarm-active';document.getElementById('trend-summary').textContent='Trend updates unavailable; displayed history may be stale.';}
 finally{monitoringPending=false;}
}
document.getElementById('alarm-form').addEventListener('submit',async event=>{
 event.preventDefault();const button=event.currentTarget.querySelector('button');button.disabled=true;
 try{const response=await fetch('/alarm_settings',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify({enabled:document.getElementById('alarm-enabled').checked,velocity:Number(document.getElementById('alarm-velocity').value),temperature:Number(document.getElementById('alarm-temperature').value),delay_seconds:Number(document.getElementById('alarm-delay').value)})});if(!response.ok)throw Error(await response.text());showToast('Alarm settings saved');await updateMonitoring();}catch(error){showToast(error.message);}finally{button.disabled=false;}
});
function exportCsv(name,rows){const csv=rows.map(row=>row.map(v=>'"'+String(v).replace(/"/g,'""')+'"').join(',')).join('\r\n');const url=URL.createObjectURL(new Blob([csv],{type:'text/csv'}));const a=document.createElement('a');a.href=url;a.download=name;a.click();setTimeout(()=>URL.revokeObjectURL(url),1000);}
function downloadTrends(){if(!monitoringSnapshot)return;exportCsv('sensor-trends.csv',[['age_ms_at_export','x_mm_s','y_mm_s','z_mm_s','temperature_C'],...monitoringSnapshot.trends.map(p=>[p.age_ms,p.x,p.y,p.z,p.temperature])]);}
function downloadCaptureHistory(){if(!monitoringSnapshot)return;exportCsv('capture-history.csv',[['device_local_time','axis','result','transmission'],...monitoringSnapshot.captures.map(c=>[c.time,c.axis,c.result,c.transmission])]);}
window.addEventListener('resize',redrawTrends);updateMonitoring();setInterval(updateMonitoring,5000);
)rawliteral");
}
