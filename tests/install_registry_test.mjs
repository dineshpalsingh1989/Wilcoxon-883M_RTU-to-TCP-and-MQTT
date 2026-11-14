import assert from 'node:assert/strict';
import {readFileSync} from 'node:fs';

const source = readFileSync(new URL('../services/install-registry/worker.js', import.meta.url), 'utf8');
const {default: worker} = await import('data:text/javascript;base64,' + Buffer.from(source).toString('base64'));
const env = {
  ALLOWED_ORIGIN:'https://dineshpalsingh1989.github.io',
  GITHUB_REPO:'example/private-records', GITHUB_TOKEN:'test-token', INSTALL_KEY:'test-install-key',
};
const record = {
  checked_at:'2026-11-14T00:00:00.000Z', board:'cytron_iriv_io_controller',
  firmware_version:'v5.0.0', board_id:'ABC12345', mac_address:'12:34:56:78:9A:BC',
  mac_type:'ethernet', sensor_serial:null, result:'gateway_booted_sensor_not_identified',
};
const post = (key, body=record, origin=env.ALLOWED_ORIGIN) => new Request('https://registry.example/installations', {
  method:'POST', headers:{Origin:origin, 'Content-Type':'application/json', 'X-Install-Key':key},
  body:JSON.stringify(body),
});
let saved = null;
const realFetch = globalThis.fetch;
globalThis.fetch = async (url, options) => {
  assert.equal(url, 'https://api.github.com/repos/example/private-records/issues');
  assert.equal(options.headers.Authorization, 'Bearer test-token');
  saved = JSON.parse(options.body);
  return new Response(JSON.stringify({number:12,html_url:'https://github.com/example/private-records/issues/12'}), {status:201});
};
try {
  assert.equal((await worker.fetch(post('wrong'), env)).status, 401);
  assert.equal((await worker.fetch(post('test-install-key', record, 'https://evil.example'), env)).status, 403);
  assert.equal((await worker.fetch(post('test-install-key', {...record, mac_address:'invalid'}), env)).status, 400);
  const response = await worker.fetch(post('test-install-key'), env);
  assert.equal(response.status, 201);
  const result=await response.json();
  assert.equal(result.issue_number, 12);
  assert.equal(result.issue_url, 'https://github.com/example/private-records/issues/12');
  assert(saved.body.includes('"sensor_serial": null'));
  assert(!saved.body.includes('test-install-key'));
  console.log('PASS: private registry accepts valid records and rejects invalid key, origin and MAC.');
} finally { globalThis.fetch = realFetch; }
