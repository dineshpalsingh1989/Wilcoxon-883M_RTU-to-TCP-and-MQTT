#!/usr/bin/env node
// File bridge between Codex and Claude Code review agents.
//   node scripts/agent_bridge.js start <run-id> [--expect codex,claude] [--scope "text"]
//   node scripts/agent_bridge.js status <run-id>
//   node scripts/agent_bridge.js wait <run-id> [--timeout <minutes>] [--summarize]
// Each producer saves .agent-reports/<run-id>/<producer>.md from docs/agent-report-template.md.
// A report counts as finished once its "- Status:" line holds complete, partial or failed.
// With --summarize, wait runs the Claude review-coordinator read-only and saves summary.md.
'use strict';
const fs = require('node:fs');
const path = require('node:path');
const { execFileSync, spawn } = require('node:child_process');

const ROOT = path.resolve(__dirname, '..');
const REPORTS = path.join(ROOT, '.agent-reports');
const POLL_MS = 5000;
const SETTLE_MS = 3000;

function fail(message, code = 1) {
  console.error(`agent_bridge: ${message}`);
  process.exit(code);
}

function parseArgs(argv) {
  const [command, runId, ...rest] = argv;
  const options = {};
  for (let i = 0; i < rest.length; i++) {
    const key = rest[i];
    if (!key.startsWith('--')) fail(`unexpected argument ${key}`);
    const name = key.slice(2);
    if (name === 'summarize') options.summarize = true;
    else if (i + 1 < rest.length) options[name] = rest[++i];
    else fail(`missing value for ${key}`);
  }
  return { command, runId, options };
}

function git(args) {
  try { return execFileSync('git', args, { cwd: ROOT, encoding: 'utf8' }).trim(); } catch { return ''; }
}

function revision() {
  return { commit: git(['rev-parse', '--short', 'HEAD']) || 'unknown', dirty: git(['status', '--porcelain']) !== '' };
}

function runDir(runId) {
  if (!runId || !/^[A-Za-z0-9._-]{1,64}$/.test(runId) || runId.startsWith('.')) {
    fail('run ID must be 1-64 letters, digits, dot, dash or underscore');
  }
  return path.join(REPORTS, runId);
}

function readManifest(dir) {
  const file = path.join(dir, 'manifest.json');
  if (!fs.existsSync(file)) fail(`no manifest at ${path.relative(ROOT, file)}; run "start" first`);
  return JSON.parse(fs.readFileSync(file, 'utf8'));
}

function producerPrompt(manifest, producer) {
  const rel = `.agent-reports/${manifest.runId}/${producer}.md`;
  return [
    `Review scope: ${manifest.scope}.`,
    `When all your work is finished, save one report to ${rel} using docs/agent-report-template.md.`,
    `Set Run ID to ${manifest.runId}, the producer to ${producer} plus the agents you used, and the reviewed commit to ${manifest.revision.commit}${manifest.revision.dirty ? ' (with uncommitted changes)' : ''}.`,
    'Set "- Status:" to exactly one of complete, partial or failed. Write the file only once, at the end, because the bridge treats a filled Status line as done.',
    'Do not change source code and do not include credentials.',
  ].join(' ');
}

function coordinatorPrompt(manifest, states) {
  const rel = `.agent-reports/${manifest.runId}`;
  const missing = states.filter(s => !s.done).map(s => s.producer);
  return [
    `Collect the reports in ${rel}/ (${states.filter(s => s.done).map(s => `${s.producer}.md`).join(', ') || 'none'}).`,
    missing.length ? `These producers did not finish and must be listed as missing coverage: ${missing.join(', ')}.` : '',
    `The run started at commit ${manifest.revision.commit}; treat reports for another revision as historical.`,
    'Verify the material findings against the current source, merge duplicate root causes while keeping agent attribution, and call out disagreements.',
    'Follow the "Coordinator summary" section of docs/agent-report-template.md and return only the Markdown summary.',
    'Do not launch new reviews, edit files or change source code.',
  ].filter(Boolean).join(' ');
}

function reportState(dir, producer) {
  const file = path.join(dir, `${producer}.md`);
  if (!fs.existsSync(file)) return { producer, done: false, note: 'no report yet' };
  const stat = fs.statSync(file);
  const text = fs.readFileSync(file, 'utf8');
  const status = /^\s*-\s*Status:\s*(complete|partial|failed)\s*$/im.exec(text);
  if (!status) return { producer, done: false, note: 'report has no final Status line' };
  if (Date.now() - stat.mtimeMs < SETTLE_MS) return { producer, done: false, note: 'report still being written' };
  return { producer, done: true, status: status[1].toLowerCase(), text };
}

function revisionWarnings(manifest, states) {
  const warnings = [];
  const now = revision();
  if (now.commit !== manifest.revision.commit) warnings.push(`HEAD moved from ${manifest.revision.commit} to ${now.commit} since the run started`);
  for (const s of states) {
    if (s.done && !s.text.includes(manifest.revision.commit)) warnings.push(`${s.producer}.md does not mention commit ${manifest.revision.commit}`);
  }
  return warnings;
}

function printStates(manifest, states) {
  console.log(`Run ${manifest.runId} at ${manifest.revision.commit}${manifest.revision.dirty ? ' (dirty)' : ''}`);
  for (const s of states) console.log(`  ${s.producer.padEnd(12)} ${s.done ? s.status : `waiting: ${s.note}`}`);
  for (const w of revisionWarnings(manifest, states)) console.log(`  warning: ${w}`);
}

function start(runId, options) {
  const dir = runDir(runId);
  if (fs.existsSync(path.join(dir, 'manifest.json'))) fail(`run ${runId} already exists; choose a new run ID`);
  const expected = (options.expect || 'codex,claude').split(',').map(s => s.trim()).filter(Boolean);
  if (!expected.length || expected.some(p => !/^[a-z0-9_-]{1,32}$/.test(p) || p === 'summary' || p === 'manifest')) {
    fail('--expect takes comma-separated lowercase producer names (not summary or manifest)');
  }
  if (new Set(expected).size !== expected.length) fail('--expect has duplicate producers');
  const manifest = {
    runId,
    expected,
    scope: options.scope || 'the gateway firmware and library in this repository',
    revision: revision(),
    created: new Date().toISOString(),
  };
  fs.mkdirSync(dir, { recursive: true });
  fs.writeFileSync(path.join(dir, 'manifest.json'), `${JSON.stringify(manifest, null, 2)}\n`);
  console.log(`Created .agent-reports/${runId}/manifest.json\n`);
  for (const producer of expected) console.log(`Send to ${producer}:\n\n${producerPrompt(manifest, producer)}\n`);
  console.log(`Then run: node scripts/agent_bridge.js wait ${runId} --summarize`);
}

function summarize(dir, manifest, states) {
  return new Promise((resolve, reject) => {
    const args = [
      '-p', '--agent', 'review-coordinator', '--output-format', 'text',
      '--disallowedTools', 'Agent,Write,Edit,NotebookEdit,Bash,PowerShell,WebFetch,WebSearch',
    ];
    // Windows needs a shell to run claude.cmd; args are fixed strings and the prompt goes through stdin.
    const child = process.platform === 'win32'
      ? spawn(['claude', ...args].join(' '), { cwd: ROOT, shell: true, stdio: ['pipe', 'pipe', 'inherit'] })
      : spawn('claude', args, { cwd: ROOT, stdio: ['pipe', 'pipe', 'inherit'] });
    let output = '';
    child.stdout.on('data', chunk => { output += chunk; });
    child.on('error', reject);
    child.on('close', code => {
      if (code !== 0 || !output.trim()) return reject(new Error(`claude exited with code ${code}`));
      const file = path.join(dir, 'summary.md');
      fs.writeFileSync(file, output.endsWith('\n') ? output : `${output}\n`);
      resolve(file);
    });
    child.stdin.end(coordinatorPrompt(manifest, states));
  });
}

async function wait(runId, options) {
  const dir = runDir(runId);
  const manifest = readManifest(dir);
  const timeoutMin = Number(options.timeout || 120);
  if (!(timeoutMin > 0)) fail('--timeout must be a positive number of minutes');
  const deadline = Date.now() + timeoutMin * 60000;
  let last = '';
  let states;
  for (;;) {
    states = manifest.expected.map(p => reportState(dir, p));
    const line = states.map(s => `${s.producer}:${s.done ? s.status : 'waiting'}`).join(' ');
    if (line !== last) { console.log(`[${new Date().toLocaleTimeString()}] ${line}`); last = line; }
    if (states.every(s => s.done) || Date.now() > deadline) break;
    await new Promise(r => setTimeout(r, POLL_MS));
  }
  printStates(manifest, states);
  const complete = states.every(s => s.done);
  if (!complete) console.log(`Timed out after ${timeoutMin} min; missing producers will be reported as gaps.`);
  if (!options.summarize) {
    console.log(`\nSend to the coordinator (Claude review-coordinator or Codex review_coordinator):\n\n${coordinatorPrompt(manifest, states)}`);
    process.exit(complete ? 0 : 2);
  }
  if (!states.some(s => s.done)) fail('no finished reports to summarize', 2);
  console.log('Running Claude review-coordinator (read-only)...');
  try {
    const file = await summarize(dir, manifest, states);
    console.log(`Saved ${path.relative(ROOT, file)}`);
  } catch (error) {
    fail(`${error.message}. Send this prompt to a coordinator instead:\n\n${coordinatorPrompt(manifest, states)}`);
  }
  process.exit(complete ? 0 : 2);
}

const { command, runId, options } = parseArgs(process.argv.slice(2));
if (command === 'start') start(runId, options);
else if (command === 'status') {
  const dir = runDir(runId);
  const manifest = readManifest(dir);
  printStates(manifest, manifest.expected.map(p => reportState(dir, p)));
} else if (command === 'wait') wait(runId, options);
else fail('usage: agent_bridge.js start|status|wait <run-id> [options]');
