// Hardware-free simulation matching /monitoring and the update status API.
const alarm = { enabled: false, velocity: 4.5, temperature: 80, delay_seconds: 10 };
let scenario = 'normal', alarmSince = null, lastSuccess = Date.now();
let reads = 0, failures = 0, timeouts = 0, consecutive = 0;
let latest, trends = [], captures = [];
let update = { state: 'idle', message: 'Demo: no update running', progress: 0, active: false };
function sample(at = Date.now()) {
  if (update.active) return;
  reads++;
  if (scenario === 'offline') { failures++; timeouts++; consecutive++; alarmSince = null; return; }
  const high = scenario === 'alarm';
  const wave = Math.sin(at / 11000) * .15;
  latest = { x: (high ? 6 : 2.1) + wave, y: (high ? 5.2 : 1.7) + wave / 2, z: (high ? 7 : 2.5) - wave, temperature: high ? 85 + wave : 31.5 + wave };
  consecutive = 0; lastSuccess = at;
  const exceeded = Math.max(latest.x, latest.y, latest.z) >= alarm.velocity || latest.temperature >= alarm.temperature;
  if (alarm.enabled && exceeded) { if (alarmSince === null) alarmSince = at; } else alarmSince = null;
  trends.push({ ...latest, at }); trends = trends.slice(-120);
}
for (let i = 59; i >= 0; i--) sample(Date.now() - i * 2000);
function snapshot(capturing) {
  const fresh = !consecutive && Date.now() - lastSuccess < 15000;
  const alarmState = !alarm.enabled ? 'Disabled' : !fresh ? 'Unknown - sensor data unavailable' : alarmSince === null ? 'Within limits' : Date.now() - alarmSince >= alarm.delay_seconds * 1000 ? 'Alarm active' : 'Threshold exceeded - pending';
  return {
    health: { state: fresh ? 'Connected' : consecutive ? 'Communication error' : 'Readings stale', has_reading: true, age_ms: Date.now() - lastSuccess, reads, failures, timeouts, consecutive_errors: consecutive, last_error: consecutive ? 226 : 0, capturing, updating: update.active },
    alarm: { ...alarm, state: alarmState },
    trends: trends.map(({ at, ...point }) => ({ ...point, age_ms: Date.now() - at })), captures,
  };
}
function saveAlarm(body) {
  if (typeof body.enabled !== 'boolean' || !Number.isFinite(body.velocity) || body.velocity <= 0 || body.velocity > 1000 || !Number.isFinite(body.temperature) || body.temperature < -40 || body.temperature > 200 || !Number.isInteger(body.delay_seconds) || body.delay_seconds < 0 || body.delay_seconds > 3600) return false;
  for (const key of ['enabled','velocity','temperature','delay_seconds']) alarm[key] = body[key];
  alarmSince = null; return true;
}
function capture(axis, result = 'Complete') {
  captures.unshift({ time: new Date().toISOString().replace('T', ' ').slice(0, 19) + ' UTC', axis, result, transmission: result === 'Complete' ? 'Demo MQTT publish attempted' : 'Not sent' }); captures = captures.slice(0, 20);
}
function install(tag) {
  if (update.active || tag !== 'v4.9.0') return false;
  update = { state: 'starting', message: 'Demo: pausing sensor monitoring', progress: 0, active: true };
  const timer = setInterval(() => {
    update.progress = Math.min(100, update.progress + 20);
    update.state = update.progress === 100 ? 'verifying' : 'downloading';
    update.message = update.progress === 100 ? 'Demo: verifying checksum and image' : 'Demo: downloading firmware to inactive partition';
    if (update.progress === 100) {
      clearInterval(timer);
      setTimeout(() => { update = { state: 'complete', message: 'Demo update complete. No firmware was installed or device restarted.', progress: 100, active: false }; }, 1200);
    }
  }, 700);
  return true;
}
module.exports = { sample, snapshot, saveAlarm, capture, install, get latest() { return latest; }, get update() { return update; }, setScenario(value) { if (!['normal','alarm','offline'].includes(value)) return false; scenario = value; sample(); return true; } };
