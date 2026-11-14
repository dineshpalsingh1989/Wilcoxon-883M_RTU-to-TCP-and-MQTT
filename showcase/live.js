// Live panel: subscribes (read-only) to the gateway's MQTT topics over WebSockets and shows the real readings.
// Payloads follow the gateway firmware (IRIV IOC/src/mqtt/mqtt.cpp): <base> type "metrics", <base>/io type "io_status",
// <base>/status type "health_status". The page never publishes.

// For customers: fill in a broker WebSocket URL and a READ-ONLY login (allowed to subscribe to the base topic only).
// Anything here is visible to everyone who opens the page. Left empty, viewers enter their own in the form.
const LIVE = { url: '', username: '', password: '', topic: '883M' };

(() => {
  const $ = id => document.getElementById(id);
  const AXES = ['x', 'y', 'z'], KEEP = 120;
  let client = null, trend = [], last = 0, sample = null;

  // Turn one MQTT message into a state patch. Returns null for messages the panel does not show (waveform chunks and so on).
  function parse(topic, base, text) {
    let m; try { m = JSON.parse(text); } catch { return null; }
    if (topic === base && m.type === 'metrics' && m.data) {
      const v = m.data.veloRms || {};
      return { kind: 'metrics', serial: m.serial_number, time: m.timestamp, vel: AXES.map(a => +v[a]), temp: +m.data.temperature };
    }
    if (topic === base + '/io' && m.type === 'io_status') {
      const r = (m.output_rules || [])[0] || {};
      return { kind: 'io', di0: !!(m.inputs || [])[0], di1: !!(m.inputs || [])[1], do0: !!(m.outputs || [])[0], alarm: !!m.alarm, fault: !!m.sensor_fault, latched: !!r.latched, acked: !!r.acknowledged };
    }
    if (topic === base + '/status' && m.type === 'health_status') return { kind: 'status', uptime: m.uptime, serial: m.serial_number };
    return null;
  }
  { // self-check against the firmware's field names
    const p = parse('883M', '883M', '{"type":"metrics","serial_number":"S1","data":{"veloRms":{"x":1.5,"y":2,"z":0.5},"temperature":41.2}}');
    const q = parse('883M/io', '883M', '{"type":"io_status","inputs":[false,true],"outputs":[true],"alarm":true,"output_rules":[{"latched":true,"acknowledged":true}]}');
    console.assert(p.vel.join() === '1.5,2,0.5' && p.temp === 41.2 && q.di1 && q.do0 && q.acked && !q.di0, 'live parse');
    console.assert(parse('883M', '883M', '{"type":"waveform_chunk"}') === null && parse('883M', '883M', 'not json') === null, 'live parse ignores');
  }

  const status = (text, kind) => { const s = $('live-status'); s.textContent = text; s.className = 'livepill ' + kind; };
  const fmt = n => Number.isFinite(n) ? n.toFixed(2) : '--';
  function apply(p) {
    last = Date.now();
    if (p.kind === 'metrics') {
      AXES.forEach((a, i) => { $('live-' + a).textContent = fmt(p.vel[i]); });
      $('live-temp').textContent = Number.isFinite(p.temp) ? p.temp.toFixed(1) : '--';
      if (p.serial) $('live-serial').textContent = p.serial;
      $('live-time').textContent = p.time || new Date().toLocaleTimeString();
      trend.push(p.vel); if (trend.length > KEEP) trend.shift(); draw();
    } else if (p.kind === 'io') {
      const set = (id, on, yes, no) => { const e = $(id); e.textContent = on ? yes : no; e.className = 'io ' + (on ? 'on' : ''); };
      set('live-alarm', p.alarm, 'ALARM', 'Normal'); set('live-do0', p.do0, 'ON', 'off'); set('live-di1', p.di1, 'pressed', 'released'); set('live-di0', p.di0, 'pressed', 'released');
      set('live-fault', p.fault, 'FAULT', 'OK');
      $('live-latch').textContent = p.latched ? (p.acked ? 'latched · acknowledged' : 'latched · waiting for acknowledge') : 'not latched';
      $('live-beacon').classList.toggle('on', p.do0);
    } else if (p.kind === 'status') {
      if (p.uptime) $('live-uptime').textContent = p.uptime;
      if (p.serial) $('live-serial').textContent = p.serial;
    }
  }

  function draw() {
    const cv = $('live-chart'), ctx = cv.getContext('2d'), w = cv.clientWidth, h = cv.clientHeight, dpr = Math.min(2, devicePixelRatio || 1);
    if (cv.width !== Math.round(w * dpr)) { cv.width = Math.round(w * dpr); cv.height = Math.round(h * dpr); }
    ctx.setTransform(dpr, 0, 0, dpr, 0, 0); ctx.clearRect(0, 0, w, h);
    const max = Math.max(5, ...trend.flat().filter(Number.isFinite)) * 1.15, y = v => h - 18 - (h - 30) * v / max;
    ctx.font = '12px Inter, system-ui, sans-serif'; ctx.fillStyle = '#6e6e73'; ctx.strokeStyle = '#ececf0'; ctx.lineWidth = 1;
    for (const g of [0, max / 2, max]) { ctx.beginPath(); ctx.moveTo(34, y(g)); ctx.lineTo(w, y(g)); ctx.stroke(); ctx.fillText(g.toFixed(1), 0, y(g) + 4); }
    ['#0a8a8f', '#0071e3', '#8e44ad'].forEach((col, i) => {
      ctx.strokeStyle = col; ctx.lineWidth = 2.2; ctx.beginPath();
      trend.forEach((v, k) => { const px = 34 + (w - 34) * k / (KEEP - 1); Number.isFinite(v[i]) && (k ? ctx.lineTo(px, y(v[i])) : ctx.moveTo(px, y(v[i]))); });
      ctx.stroke();
    });
  }

  const loadLib = () => window.mqtt ? Promise.resolve() : new Promise((ok, fail) => { const s = document.createElement('script'); s.src = 'https://unpkg.com/mqtt@5.10.1/dist/mqtt.min.js'; s.onload = ok; s.onerror = () => fail(Error('Could not load the MQTT library')); document.head.append(s); });

  async function connect(cfg) {
    stop();
    if (!/^wss?:\/\//.test(cfg.url)) return status('Enter a ws:// or wss:// broker address', 'bad');
    if (location.protocol === 'https:' && cfg.url.startsWith('ws:')) return status('This page is https, so the broker must use wss://', 'bad');
    status('Connecting…', 'wait');
    try { await loadLib(); } catch (e) { return status(e.message, 'bad'); }
    const base = cfg.topic.replace(/\/+$/, '') || '883M';
    client = mqtt.connect(cfg.url, { username: cfg.username || undefined, password: cfg.password || undefined, clean: true, reconnectPeriod: 5000, connectTimeout: 10000, clientId: 'web-viewer-' + Math.random().toString(16).slice(2, 10) });
    client.on('connect', () => { status('Connected · waiting for the gateway', 'wait'); client.subscribe([base, base + '/io', base + '/status'], { qos: 0 }, err => err && status('Subscribe refused: ' + err.message, 'bad')); });
    client.on('message', (t, buf) => { const p = parse(t, base, buf.toString()); if (p) apply(p); });
    client.on('error', e => status('Broker error: ' + e.message, 'bad'));
    client.on('close', () => { if (client) status('Disconnected · retrying', 'bad'); });
    $('live-go').textContent = 'Disconnect';
  }
  function stop() {
    clearInterval(sample); sample = null; last = 0;
    if (client) { const c = client; client = null; c.end(true); }
    $('live-go').textContent = 'Connect'; status('Not connected', 'off');
  }

  // Sample data runs the same parse/apply path with made-up values, clearly labelled.
  function preview() {
    stop(); status('SAMPLE DATA · not your machine', 'wait'); $('live-go').textContent = 'Disconnect';
    let t = 0;
    sample = setInterval(() => {
      t += 1; const high = (t % 40) > 22, v = AXES.map((_, i) => (high ? 5.2 : 1.9) + [0, .3, -.6][i] + .15 * Math.sin(t / 2 + i));
      const msg = { type: 'metrics', serial_number: 'SAMPLE', timestamp: new Date().toLocaleTimeString(), data: { veloRms: { x: v[0], y: v[1], z: v[2] }, temperature: 41 + Math.sin(t / 9) } };
      apply(parse('883M', '883M', JSON.stringify(msg)));
      const ack = (t % 40) > 30, on = high || ((t % 40) > 22 && !ack);
      apply(parse('883M/io', '883M', JSON.stringify({ type: 'io_status', inputs: [false, (t % 40) === 31], outputs: [on], alarm: high, sensor_fault: false, output_rules: [{ latched: on, acknowledged: ack && on }] })));
    }, 1000);
  }

  const saved = (() => { try { return JSON.parse(localStorage.getItem('live-broker') || '{}'); } catch { return {}; } })();
  const cfg0 = { ...LIVE, ...(LIVE.url ? {} : saved) };
  $('live-url').value = cfg0.url || ''; $('live-user').value = cfg0.username || ''; $('live-topic').value = cfg0.topic || '883M';
  $('live-pass').value = LIVE.url ? LIVE.password : '';
  $('live-form').addEventListener('submit', e => {
    e.preventDefault();
    if (client || sample) return stop();
    const cfg = { url: $('live-url').value.trim(), username: $('live-user').value.trim(), password: $('live-pass').value, topic: $('live-topic').value.trim() };
    try { localStorage.setItem('live-broker', JSON.stringify({ url: cfg.url, username: cfg.username, topic: cfg.topic })); } catch { }   // never the password
    connect(cfg);
  });
  $('live-sample').addEventListener('click', preview);
  setInterval(() => { if (client && last) { const s = Math.round((Date.now() - last) / 1000); status(s < 120 ? `Live · last message ${s} s ago` : `Connected · no message for ${Math.round(s / 60)} min`, s < 120 ? 'ok' : 'wait'); } }, 1000);
  addEventListener('resize', draw);
  draw();
  if (LIVE.url) connect(LIVE);
})();
