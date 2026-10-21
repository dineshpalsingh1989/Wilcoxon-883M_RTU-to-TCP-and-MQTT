#include "core/modules.h"

// Public release metadata is fetched by the browser; no GitHub credential is stored.
void handleGithubUpdates() {
    server.send(200, "text/html", R"rawliteral(
<!doctype html><html lang="en"><head><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1"><title>GitHub firmware updates</title>
<style>*{box-sizing:border-box}body{margin:0;background:#f2f5f7;color:#192f3d;font:14px/1.6 system-ui,sans-serif;padding:32px 16px}main{max-width:720px;margin:auto;background:white;padding:28px;border:1px solid #e2e9ed;border-radius:14px}h1{font-size:26px;line-height:1.2}h2{font-size:17px}a{color:#147d78}button,.button{display:inline-block;background:#147d78;color:white;padding:11px 16px;border:0;border-radius:7px;text-decoration:none;font:inherit;cursor:pointer}button:disabled{opacity:.5;cursor:wait}section{padding:18px;background:#f4f8fa;border-radius:8px;margin:18px 0}pre{white-space:pre-wrap;overflow-wrap:anywhere;font:inherit}#message{color:#536e7d}small{color:#647e8d}button:focus-visible,a:focus-visible{outline:3px solid #51b4c2;outline-offset:3px}</style></head><body><main>
<a href="/settings">&larr; Settings</a><h1>GitHub firmware updates</h1>
<p>Update the ESP32 gateway connected to your Wilcoxon 883M.</p>
<section><strong>Installed gateway firmware:</strong> <span id="installed">Loading...</span><br><small>Board target: <span id="board">Loading...</span></small></section>
<button id="check" onclick="checkRelease()">Check for release</button><p id="message" role="status">Check the latest published release before downloading firmware.</p>
<section id="release" hidden><h2 id="version"></h2><pre id="notes"></pre><p><a id="release-link" target="_blank" rel="noopener noreferrer">View release on GitHub</a></p><p><a id="download" class="button" hidden>Download board firmware</a></p><small id="digest"></small><p><button id="install" onclick="installRelease()" hidden>Install on gateway</button></p></section>
<section><strong>Update progress</strong><p id="update-status" role="status">No update running</p><progress id="update-progress" max="100" value="0" style="width:100%"></progress></section>
<p>Direct installation verifies the release checksum and that the image matches this board's ESP32 chip, then restarts the gateway. It requires a newer stable release, Wi-Fi, and a correct device clock.</p>
<h2>Manual upload</h2><p>You can also download the binary and upload it through Device Info.</p>
<a class="button" href="/device_info">Open firmware uploader</a>
<script>
const repository = 'dineshpalsingh1989/Wilcoxon-883M_RTU-to-TCP-and-MQTT';
const demoMode = document.documentElement.dataset.demo === 'true';
let selectedRelease = null, pollingUpdate = false;
// Each board has its own release binary, named after the board target the gateway reports.
const boardInfo=fetch('/firmware_info').then(r=>{if(!r.ok)throw Error();return r.json()}).then(d=>{document.getElementById('installed').textContent=d.version;document.getElementById('board').textContent=d.board;return d.board}).catch(()=>{document.getElementById('installed').textContent='Unavailable';document.getElementById('board').textContent='Unavailable';return null});
async function checkRelease(){
 const button=document.getElementById('check'),message=document.getElementById('message');
 button.disabled=true;selectedRelease=null;document.getElementById('release').hidden=true;document.getElementById('download').hidden=true;document.getElementById('install').hidden=true;
 message.textContent='Checking GitHub Releases...';
 try{
  const board=await boardInfo;
  const response=await fetch(demoMode?'/demo/github_release':'https://api.github.com/repos/'+repository+'/releases/latest',{headers:{Accept:'application/vnd.github+json'}});
  if(response.status===404)throw Error('No public release is available yet. Publish a release with the board firmware attached.');
  if(!response.ok)throw Error('GitHub could not be reached or its API rate limit was exceeded. Try again later.');
  const release=await response.json();
  selectedRelease=release;
  const asset=board&&(release.assets||[]).find(a=>a.name==='firmware-'+board+'.bin');
  const safeUrl=url=>{const u=new URL(url);if(u.protocol!=='https:'||u.hostname!=='github.com'||!u.pathname.startsWith('/'+repository+'/releases/'))throw Error('Unexpected release URL');return u.href};
  document.getElementById('version').textContent=release.tag_name;
  document.getElementById('notes').textContent=release.body||'No release notes provided.';
  document.getElementById('release-link').href=safeUrl(release.html_url);
  document.getElementById('digest').textContent=asset&&asset.digest?'Published checksum: '+asset.digest:'';
  if(asset){document.getElementById('download').href=safeUrl(asset.browser_download_url);document.getElementById('download').hidden=false;}
  document.getElementById('install').hidden=!asset||!/^sha256:[a-f0-9]{64}$/i.test(asset.digest||'')||!/^v\d+\.\d+\.\d+$/.test(release.tag_name)||release.prerelease||release.draft;
  if(demoMode){document.getElementById('download').hidden=true;document.getElementById('install').textContent='Simulate verified update';}
  document.getElementById('release').hidden=false;
  message.textContent=asset?'Latest release loaded. Compare it with your installed version before updating.':'Release found, but the binary for this board is missing.';
 }catch(error){message.textContent=error.message;}finally{button.disabled=false;}
}
async function installRelease(){
 if(!selectedRelease||!confirm(demoMode?'Simulate a firmware update? No hardware will be changed.':'Install '+selectedRelease.tag_name+'? Monitoring will pause and the gateway will restart. Keep power connected.'))return;
 const button=document.getElementById('install');button.disabled=true;
 try{const response=await fetch('/github_install',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify({tag:selectedRelease.tag_name})});if(!response.ok)throw Error(await response.text());await pollUpdate();}catch(error){document.getElementById('message').textContent=error.message;button.disabled=false;}
}
async function pollUpdate(){
 if(pollingUpdate)return;pollingUpdate=true;
 try{const response=await fetch('/github_update_status',{signal:AbortSignal.timeout(10000)});if(!response.ok)throw Error();const data=await response.json();document.getElementById('update-status').textContent=data.message;document.getElementById('update-progress').value=data.progress;document.getElementById('install').disabled=data.active;}
 catch(error){document.getElementById('update-status').textContent='Gateway unavailable. If an update finished, wait for reboot and reload this page.';}
 finally{pollingUpdate=false;}
}
pollUpdate();setInterval(pollUpdate,2000);
</script></main></body></html>
)rawliteral");
}

void handleFirmwareInfo() {
    DynamicJsonDocument doc(256);
    doc["version"] = FIRMWARE_VERSION;
    doc["board"] = GATEWAY_BOARD;
    String json;
    serializeJson(doc, json);
    server.send(200, "application/json", json);
}
