// Simulated setup responses only. Does not access serial ports or hardware.
const fs = require('node:fs');
const path = require('node:path');
const page = fs.readFileSync(path.join(__dirname, '../IRIV IOC/src/web/sensor_setup_page.h'), 'utf8').match(/R"html\(([\s\S]*?)\)html"/)[1];
function createSensorSetupDemo() {
  let state, epoch = 0;
  function reset() {
    epoch++;
    state = {busy:false, can_cancel:false, message:'SIMULATED setup: no hardware connected', tested:0,total:0,found:false,found_baud:0,found_id:0,baud:115200,id:1,format:'8N1',read_successes:20,timeouts:2,crc_errors:0,exceptions:0,other_errors:0,sample_seen:true,reading_age_ms:100,mqtt_connected:false,tcp_clients:0,tcp_requests:0,tcp_malformed:0,tcp_timeouts:0};
  }
  reset();
  async function handle(req, url, send, readBody) {
    if (url.pathname === '/sensor_setup') {
      send(200,page.replace('<h1>','<p>SIMULATED DEMO: no sensor is connected; changes affect memory only.</p><h1>'),'text/html; charset=utf-8'); return true;
    }
    if (url.pathname !== '/api/sensor_setup') return false;
    if (req.method === 'GET') { send(200,state); return true; }
    if (req.method !== 'POST') { send(405,'Method not allowed','text/plain'); return true; }
    const b = await readBody();
    const reply = (code,text) => {send(code,text,'text/plain');return true;};
    if (b.action === 'cancel') {
      if (!state.can_cancel) return reply(409,'No search to cancel');
      epoch++;state.busy=state.can_cancel=false;state.message='SIMULATED: search cancelled; original connection restored';return reply(202,'Cancellation queued');
    }
    if (state.busy) return reply(409,'Capture or setup busy');
    if (b.action === 'search') {
      if (!Number.isInteger(b.first_id)||!Number.isInteger(b.last_id)||b.first_id<1||b.last_id>247||b.first_id>b.last_id) return reply(400,'Invalid ID range');
      state.busy=state.can_cancel=true;state.found=false;state.tested=0;state.total=5*(b.last_id-b.first_id+1);state.message='SIMULATED: searching';
      const job=++epoch;
      setTimeout(()=>{if(job!==epoch)return;state.busy=state.can_cancel=false;state.tested=state.total;state.found=b.first_id<=1&&b.last_id>=1;state.found_baud=9600;state.found_id=1;state.message=state.found?'SIMULATED: compatible sensor found':'SIMULATED: no sensor found';},1500).unref();
      return reply(202,'Simulated search queued');
    }
    if (!['use','configure'].includes(b.action)) return reply(400,'Unknown action');
    if (![9600,19200,38400,57600,115200].includes(b.baud)||!Number.isInteger(b.id)||b.id<1||b.id>247) return reply(400,'Unsupported baud or ID');
    if (b.action==='configure'&&((b.baud!==state.baud)===(b.id!==state.id))) return reply(400,'Change exactly one setting at a time');
    state.busy=true;state.message='SIMULATED: verifying';const job=++epoch;
    setTimeout(()=>{if(job!==epoch)return;state.busy=false;state.baud=b.baud;state.id=b.id;state.message='SIMULATED: connection verified and saved in memory';},500).unref();
    return reply(202,'Simulated operation queued');
  }
  return {handle,reset};
}
module.exports={createSensorSetupDemo};
