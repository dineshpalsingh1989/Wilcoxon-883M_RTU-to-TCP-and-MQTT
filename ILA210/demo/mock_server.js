// Local preview of the actual embedded IOC pages. All I/O and storage are simulated.
const http = require('node:http');
const fs = require('node:fs');
const path = require('node:path');
const {simulatorControls} = require('./simulator_controls');
const embedded = fs.readFileSync(path.join(__dirname, '../IRIV IOC/src/ioc/pages.h'), 'utf8').match(/R"ioc\(([\s\S]*?)\)ioc"/)[1];
const healthPage = fs.readFileSync(path.join(__dirname, '../IRIV IOC/src/web/sensor_health_page.h'), 'utf8').match(/R"html\(([\s\S]*?)\)html"/)[1];
const clone = value => JSON.parse(JSON.stringify(value));
function defaults() {
  return {
    revision:0, reference_mv:3320,
    digital:Array.from({length:11},(_,i)=>({name:['Capture button','Acknowledge button','External machine alarm','Machine running'][i]||`DI${i}`, action:[3,4,2,1][i]||0,active_low:false,delay_ms:500})),
    analog:[{name:'Pressure',unit:'bar',mode:1,minimum:0,maximum:10,low:1,high:8,hysteresis:.5,low_enabled:true,high_enabled:true,fault_alarm:true,delay_ms:1000},
      {name:'Temperature',unit:'°C',mode:1,minimum:0,maximum:100,low:10,high:80,hysteresis:2,low_enabled:false,high_enabled:true,fault_alarm:true,delay_ms:1000}],
    outputs:[{source:2,input:0},{source:3,input:0}]
  };
}
function validConfig(c) {
  const num=(n,a,b)=>typeof n==='number'&&Number.isFinite(n)&&n>=a&&n<=b;
  const integer=(n,max)=>Number.isInteger(n)&&num(n,0,max);
  const text=(s,max)=>typeof s==='string'&&Buffer.byteLength(s)<=max&&!/[\x00-\x1f]/.test(s);
  if(!c||!integer(c.revision,0xffffffff)||!num(c.reference_mv,3000,3600)||!Array.isArray(c.digital)||c.digital.length!==11||!Array.isArray(c.analog)||c.analog.length!==2||!Array.isArray(c.outputs)||c.outputs.length!==2)return false;
  if(!c.digital.every(d=>d&&text(d.name,24)&&integer(d.action,4)&&typeof d.active_low==='boolean'&&integer(d.delay_ms,60000)))return false;
  if(!c.analog.every(a=>a&&text(a.name,24)&&text(a.unit,12)&&integer(a.mode,2)&&num(a.minimum,-1e6,1e6)&&num(a.maximum,-1e6,1e6)&&a.minimum<a.maximum&&num(a.low,-1e6,1e6)&&num(a.high,-1e6,1e6)&&num(a.hysteresis,0,a.maximum-a.minimum)&&integer(a.delay_ms,60000)&&['low_enabled','high_enabled','fault_alarm'].every(k=>typeof a[k]==='boolean')&&(!a.low_enabled||num(a.low,a.minimum,a.maximum))&&(!a.high_enabled||num(a.high,a.minimum,a.maximum))&&(!(a.low_enabled&&a.high_enabled)||(a.low<a.high&&2*a.hysteresis<a.high-a.low))))return false;
  return c.outputs.every(o=>o&&integer(o.source,4)&&integer(o.input,10));
}
function createDemoServer() {
  let sensorSignals, health, sensorOffline, lastSensor, config, state, pins, readings, debounce, delays, limits, latchMask, acknowledged, offline, captureUntil, sequence=0;
  const now=()=>Date.now();
  function reset() {
    sensorSignals={alert:false,alarm:false};
    health={timeout_ms:10000,fault_alarm:true,output_mode:0,gateway_id:"SIMULATED-IOC-01"};sensorOffline=false;lastSensor=now();
    config=defaults(); pins=Array(11).fill(false);readings=[12,12];
    state={ready:true,fresh:true,age_ms:0,sequence:0,alarm:false,latched:false,acknowledged:false,sensor_alert:false,sensor_alarm:false,capture_accepted:0,capture_rejected:0,
      digital:Array.from({length:11},()=>({active:false,alarm:false})),analog:Array.from({length:2},()=>({adc:0,measured:12,value:0,fault:false,alarm:false})),outputs:[false,false]};
    debounce=pins.map(value=>({stable:value,candidate:value,since:now(),initialized:false}));
    delays=Array(13).fill(null);limits=[{low:false,high:false},{low:false,high:false}];
    latchMask=0;acknowledged=false;offline=false;captureUntil=0;sequence++;
  }
  function acknowledge() { acknowledged=true; }
  function capture() { if(now()<captureUntil)state.capture_rejected++;else{state.capture_accepted++;captureUntil=now()+3000;} }
  function delayed(index,condition,ms,time) { if(!condition){delays[index]=null;return false;}if(delays[index]===null)delays[index]=time;return time-delays[index]>=ms; }
  function tick() {
    const time=now(); if(!sensorOffline)lastSensor=time;
    state.sensor_alert=sensorSignals.alert;state.sensor_alarm=sensorSignals.alarm;
    state.sensor_fault=time-lastSensor>=health.timeout_ms;
    if(state.sensor_fault&&health.output_mode===1)state.sensor_alarm=true;
    if(state.sensor_fault&&health.output_mode===2)state.sensor_alert=state.sensor_alarm=false;state.fault_alarm_enabled=health.fault_alarm;
    let mask=(state.sensor_fault&&health.fault_alarm?1<<18:0)|(state.sensor_alert?1<<16:0)|(state.sensor_alarm?1<<17:0),ack=false;
    config.digital.forEach((d,i)=>{
      const value=pins[i]!==d.active_low,b=debounce[i];let edge=false;
      if(!b.initialized){b.stable=b.candidate=value;b.since=time;b.initialized=true;}
      if(value!==b.candidate){b.candidate=value;b.since=time;}
      if(b.candidate!==b.stable&&time-b.since>=50){b.stable=b.candidate;edge=b.stable;}
      state.digital[i]={active:b.stable,alarm:delayed(i,d.action===2&&b.stable,d.delay_ms,time)};
      if(state.digital[i].alarm)mask|=1<<i;
      if(edge&&d.action===3)capture();if(edge&&d.action===4)ack=true;
    });
    config.analog.forEach((a,i)=>{
      const input=readings[i],value=a.minimum+(input-(a.mode===1?4:0))/(a.mode===1?16:10)*(a.maximum-a.minimum),t=limits[i];
      t.low=a.low_enabled&&(t.low?value<a.low+a.hysteresis:value<a.low);
      t.high=a.high_enabled&&(t.high?value>a.high-a.hysteresis:value>a.high);
      const fault=a.mode!==0&&((a.mode===1&&(input<3.6||input>21))||input>=(a.mode===1?42.78:10.61));
      const alarm=delayed(11+i,a.mode!==0&&(t.low||t.high||(a.fault_alarm&&fault)),a.delay_ms,time);
      state.analog[i]={adc:Math.max(0,Math.min(4095,Math.round(input*(a.mode===1?.248:1)/10.624*4095))),measured:a.mode?input:0,value:a.mode?value:0,fault,alarm};
      if(alarm)mask|=1<<(11+i);
    });
    if(mask&~latchMask)acknowledged=false;
    if(mask)state.latched=true;
    if(ack)acknowledge();
    if(!mask&&acknowledged)state.latched=false;
    if(!state.latched)acknowledged=false;
    latchMask=mask;state.alarm=!!mask;state.acknowledged=acknowledged;
    state.outputs=config.outputs.map(o=>o.source===1?!!mask:o.source===2?state.latched:o.source===3?state.latched&&!acknowledged:o.source===4?config.digital[o.input].action!==0&&state.digital[o.input].active:false);
    state.sequence=sequence++;state.age_ms=0;
  }
  reset(); tick();
  const timer=setInterval(tick,20);timer.unref();
  const server=http.createServer(async(req,res)=>{
    const url=new URL(req.url,'http://127.0.0.1');
    const send=(status,body,type='application/json')=>{res.writeHead(status,{'Content-Type':type,'Cache-Control':'no-store'});res.end(type==='application/json'?JSON.stringify(body):body);};
    const body=()=>new Promise((resolve,reject)=>{let data='';req.on('data',chunk=>{data+=chunk;if(Buffer.byteLength(data)>8192){reject(new Error('Request too large'));req.destroy();}});req.on('end',()=>{try{resolve(data?JSON.parse(data):{});}catch{reject(new Error('Invalid JSON'));}});req.on('error',reject);});
    try {
      if(url.pathname==='/'){res.writeHead(302,{Location:'/ioc'});res.end();return;}
      if(['/ioc','/ioc/inputs','/ioc/analog','/ioc/outputs'].includes(url.pathname))return send(200,embedded.replace('<main>','<main>'+simulatorControls),'text/html; charset=utf-8');
      if(['/settings','/relay_settings'].includes(url.pathname))return send(200,`<!doctype html><html lang="en"><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1"><title>Simulated IOC demo</title><body style="font:16px system-ui;max-width:800px;margin:40px auto;padding:20px"><h1>IOC demo · simulated only</h1><p>No device is connected. Changes are stored in demo memory only.</p><ul><li><a href="/ioc">Live status</a></li><li><a href="/ioc/inputs">Inputs &amp; push-buttons</a></li><li><a href="/ioc/analog">Analog &amp; 4–20 mA</a></li><li><a href="/ioc/outputs">Alarm outputs</a></li></ul><p>DO0/DO1 represent the existing vibration outputs. Use the simulator controls on an IOC page to switch their simulated states.</p></body></html>`,'text/html; charset=utf-8');
      if(url.pathname==='/sensor_fault_settings')return send(200,healthPage.replace('<h1>', '<p>SIMULATED DEMO: memory only; no hardware connected.</p><h1>'),'text/html; charset=utf-8');
      if(url.pathname==='/demo/state')return send(200,{simulated:true,offline,pins,readings,capture_busy:now()<captureUntil});
      if(url.pathname==='/demo/action'&&req.method==='POST'){
        const action=await body();
        if(action.type==='reset')reset();
        else if(action.type==='digital'&&Number.isInteger(action.index)&&action.index>=0&&action.index<11){
          pins[action.index]=!!action.active;
          if(action.pulse){const index=action.index;setTimeout(()=>{pins[index]=false;},250).unref();}
        } else if(action.type==='analog'&&[0,1].includes(action.index)&&typeof action.value==='number'&&Number.isFinite(action.value)&&action.value>=0&&action.value<=43)readings[action.index]=action.value;
        else if(action.type==='sensor'){sensorSignals={alert:!!action.alert,alarm:!!action.alarm};}
        else if(action.type==='sensor_loss')sensorOffline=!!action.active;
        else if(action.type==='offline')offline=!!action.active;
        else if(action.type==='normal'){sensorOffline=false;pins[2]=false;readings=[12,12];sensorSignals={alert:false,alarm:false};}
        else return send(400,{error:'Unknown simulated action'});
        tick();return send(200,{simulated:true});
      }
      if(offline&&url.pathname.startsWith('/api/'))return send(503,'Simulated device offline. Use Restore connection in the demo controls.','text/plain');
      if(url.pathname==='/api/sensor_health'){
        if(req.method==='POST'){
          const c=await body();
          if(!Number.isInteger(c.timeout_ms)||c.timeout_ms<2000||c.timeout_ms>600000||typeof c.fault_alarm!=='boolean'||!Number.isInteger(c.output_mode)||c.output_mode<0||c.output_mode>2)return send(400,'Invalid sensor-loss configuration.','text/plain');
          Object.assign(health,c);tick();
        }
        return send(200,{...health,sensor_fault:state.sensor_fault,sample_seen:true,simulated:true});
      }
      if(url.pathname==='/api/ioc'&&req.method==='GET')return send(200,{supported:true,simulated:true,config,state});
      if(url.pathname==='/api/ioc/acknowledge'&&req.method==='POST'){acknowledge();tick();return send(200,'Simulated acknowledgement applied. Active conditions remain alarms.','text/plain');}
      if(url.pathname==='/api/ioc/config'&&req.method==='POST'){
        const candidate=await body();
        if(!validConfig(candidate))return send(400,'Invalid simulated configuration. Check ranges and hysteresis.','text/plain');
        if(candidate.revision!==config.revision)return send(409,'Settings changed. Reload before saving.','text/plain');
        config=clone(candidate);config.revision++;delays.fill(null);limits=[{low:false,high:false},{low:false,high:false}];debounce.forEach(d=>d.initialized=false);tick();
        return send(200,'Demo settings saved in memory and applied.','text/plain');
      }
      send(404,{error:'Page not found'});
    }catch(error){if(!res.destroyed)send(400,{error:error.message});}
  });
  server.on('close',()=>clearInterval(timer));return server;
}
if(require.main===module){
  const port=Number(process.env.PORT||8091);
  const server=createDemoServer();
  server.on('error',error=>{console.error(error.message);process.exitCode=1;});
  server.listen(port,'127.0.0.1',()=>console.log(`SIMULATED IOC demo: http://localhost:${port}/ioc (no hardware connected)`));
}
module.exports={createDemoServer};
