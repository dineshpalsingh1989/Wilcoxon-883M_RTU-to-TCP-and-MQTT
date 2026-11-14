// Cloudflare Worker for private installation records. Configure the four
// environment values documented in docs/install-registry.md before deployment.
const MAC = /^(?:[0-9A-F]{2}:){5}[0-9A-F]{2}$/i;
const BOARD_ID = /^[A-Za-z0-9_-]{4,64}$/;

function cors(origin, env) {
  return {
    'Access-Control-Allow-Origin': env.ALLOWED_ORIGIN,
    'Access-Control-Allow-Methods': 'POST, OPTIONS',
    'Access-Control-Allow-Headers': 'Content-Type, X-Install-Key',
    'Vary': 'Origin',
  };
}

function reply(status, message, origin, env) {
  return new Response(JSON.stringify({message}), {
    status, headers: {'Content-Type':'application/json', ...cors(origin, env)},
  });
}

async function sameSecret(a, b) {
  const encoder = new TextEncoder();
  const [left, right] = await Promise.all([
    crypto.subtle.digest('SHA-256', encoder.encode(a)),
    crypto.subtle.digest('SHA-256', encoder.encode(b)),
  ]);
  return Array.from(new Uint8Array(left)).every((v, i) => v === new Uint8Array(right)[i]);
}

function validRecord(record) {
  return record && typeof record === 'object' &&
    typeof record.board === 'string' && record.board.length <= 64 &&
    typeof record.firmware_version === 'string' && record.firmware_version.length <= 40 &&
    typeof record.board_id === 'string' && BOARD_ID.test(record.board_id) &&
    typeof record.mac_address === 'string' && MAC.test(record.mac_address) &&
    ['wifi', 'ethernet'].includes(record.mac_type) &&
    (record.sensor_serial === null ||
      (typeof record.sensor_serial === 'string' && record.sensor_serial.length <= 80)) &&
    ['gateway_booted_sensor_identified', 'gateway_booted_sensor_not_identified'].includes(record.result) &&
    typeof record.checked_at === 'string' && !Number.isNaN(Date.parse(record.checked_at));
}

export default {
  async fetch(request, env) {
    const origin = request.headers.get('Origin') || '';
    if (!env.ALLOWED_ORIGIN || origin !== env.ALLOWED_ORIGIN) return new Response('Forbidden', {status:403});
    if (request.method === 'OPTIONS') return new Response(null, {status:204, headers:cors(origin, env)});
    if (request.method !== 'POST' || new URL(request.url).pathname !== '/installations') {
      return reply(404, 'Not found', origin, env);
    }
    if (!env.INSTALL_KEY || !env.GITHUB_TOKEN || !env.GITHUB_REPO) return reply(503, 'Registry not configured', origin, env);
    if (!(await sameSecret(request.headers.get('X-Install-Key') || '', env.INSTALL_KEY))) {
      return reply(401, 'Invalid installation key', origin, env);
    }
    if (request.headers.get('Content-Type')?.split(';')[0] !== 'application/json') {
      return reply(415, 'Expected JSON', origin, env);
    }
    const raw = await request.text();
    if (raw.length > 2048) return reply(413, 'Record too large', origin, env);
    let record;
    try { record = JSON.parse(raw); } catch { return reply(400, 'Invalid JSON', origin, env); }
    if (!validRecord(record)) return reply(400, 'Invalid device record', origin, env);
    const clean = {
      checked_at: record.checked_at, board: record.board, firmware_version: record.firmware_version,
      board_id: record.board_id, mac_address: record.mac_address, mac_type: record.mac_type,
      sensor_serial: record.sensor_serial, result: record.result,
    };
    const body = 'Gateway startup record\n\n' + JSON.stringify(clean, null, 2).split('\n').map(line => '    ' + line).join('\n');
    const github = await fetch(`https://api.github.com/repos/${env.GITHUB_REPO}/issues`, {
      method:'POST', headers:{
        'Accept':'application/vnd.github+json', 'Content-Type':'application/json',
        'Authorization':`Bearer ${env.GITHUB_TOKEN}`, 'User-Agent':'wilcoxon-install-registry',
        'X-GitHub-Api-Version':'2022-11-28',
      },
      body:JSON.stringify({title:`Gateway ${clean.board} ${clean.mac_address} ${clean.firmware_version}`, body}),
    });
    if (!github.ok) return reply(502, 'GitHub did not save the record', origin, env);
    const issue = await github.json();
    return new Response(JSON.stringify({saved:true, issue_number:issue.number, issue_url:issue.html_url}), {
      status:201, headers:{'Content-Type':'application/json', ...cors(origin, env)},
    });
  },
};
