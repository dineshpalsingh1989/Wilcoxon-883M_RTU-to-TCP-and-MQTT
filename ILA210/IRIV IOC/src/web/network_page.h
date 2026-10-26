#pragma once
// Ethernet address page for the IRIV IO Controller. Values come from GET /api/network;
// saving POSTs JSON to the same route, and the gateway restarts with the new address.
static const char NETWORK_PAGE[] PROGMEM = R"html(<!DOCTYPE html><html lang="en"><head><meta charset="utf-8"><title>Network</title><meta name="viewport" content="width=device-width, initial-scale=1">
<style>body{font-family:-apple-system,BlinkMacSystemFont,"Segoe UI",Roboto,Helvetica,Arial,sans-serif;margin:0;background:#f2f5f7;color:#192f3d;line-height:1.5;display:flex;justify-content:center;padding:20px 16px}.container{width:100%;max-width:500px;background:white;border:1px solid #dfe8ed;border-radius:16px;overflow:hidden}.header{background:linear-gradient(115deg,#102c3b,#194655);color:#f8fafc;text-align:center;padding:20px}.header h2{margin:0}table{width:100%;border-collapse:collapse}th,td{padding:10px 20px;border-bottom:1px solid #e9ecef;text-align:left;font-weight:400;overflow-wrap:anywhere}th{color:#536e7d;width:40%}form{padding:20px}fieldset{border:0;margin:0 0 12px;padding:0}legend{font-weight:600;margin-bottom:8px}.choice{display:flex;align-items:center;gap:10px;min-height:44px;font-weight:400}.choice input{width:20px;height:20px;accent-color:#147d78}label{display:block;font-size:14px;font-weight:600;margin:12px 0 6px}input[type=text]{width:100%;min-height:44px;padding:10px 12px;box-sizing:border-box;border:1px solid #ced4da;border-radius:8px;font:inherit}input[aria-invalid=true]{border-color:#c0392b}[hidden]{display:none!important}.note{color:#536e7d;font-size:14px}button{width:100%;min-height:44px;margin-top:8px;border:0;border-radius:8px;background:#147d78;color:white;font:inherit;font-weight:600;cursor:pointer}button:disabled{opacity:.6;cursor:default}#result{padding:0 20px;margin:0 0 12px;white-space:pre-wrap}#result.error{color:#c0392b}.back{display:block;text-align:center;padding:16px;color:#147d78;font-weight:600}a:focus-visible,button:focus-visible,input:focus-visible{outline:3px solid #42a8bd;outline-offset:3px}</style>
</head><body><main class="container"><div class="header"><h2>Network (Ethernet)</h2></div>
<table><tr><th scope="row">Status</th><td id="link">Loading...</td></tr><tr><th scope="row">Address mode</th><td id="desc"></td></tr><tr><th scope="row">IP address</th><td id="cur-ip"></td></tr><tr><th scope="row">Subnet mask</th><td id="cur-mask"></td></tr><tr><th scope="row">Gateway</th><td id="cur-gateway"></td></tr><tr><th scope="row">DNS server</th><td id="cur-dns"></td></tr><tr><th scope="row">MAC address</th><td id="mac"></td></tr><tr><th scope="row">Name</th><td id="hostname"></td></tr></table>
<form id="form" novalidate><fieldset><legend>IP address assignment</legend>
<label class="choice"><input type="radio" name="mode" value="dhcp" id="mode-dhcp"> Automatic (DHCP)</label>
<label class="choice"><input type="radio" name="mode" value="static" id="mode-static"> Static IP address</label></fieldset>
<div id="static-fields" hidden>
<label for="ip">IP address</label><input type="text" id="ip" inputmode="decimal" autocomplete="off" placeholder="192.168.1.50">
<label for="mask">Subnet mask</label><input type="text" id="mask" inputmode="decimal" autocomplete="off" placeholder="255.255.255.0">
<label for="gateway">Gateway (optional)</label><input type="text" id="gateway" inputmode="decimal" autocomplete="off" placeholder="192.168.1.1">
<label for="dns">DNS server (optional, the gateway is used when empty)</label><input type="text" id="dns" inputmode="decimal" autocomplete="off" placeholder="192.168.1.1">
</div>
<p class="note" id="dhcp-note">With DHCP, the gateway uses <span class="fallback"></span> when no DHCP server answers within 10 seconds after power-up.</p>
<p class="note">Saving restarts the gateway. If the new address cannot be reached, hold the USER button for 5 seconds: the gateway then answers at <span class="fallback"></span> (set the laptop to the same subnet, mask 255.255.255.0) until the next restart.</p>
<button type="submit" id="save" disabled>Save and restart</button></form>
<p id="result" role="status"></p><a class="back" href="/settings">&larr; Back to Settings</a></main>
<script>
const el=id=>document.getElementById(id),fields=['ip','mask','gateway','dns'],names={ip:'IP address',mask:'subnet mask',gateway:'gateway',dns:'DNS server'};
const ipv4=/^(25[0-5]|2[0-4]\d|1\d\d|[1-9]?\d)(\.(25[0-5]|2[0-4]\d|1\d\d|[1-9]?\d)){3}$/;
function say(text,error){el('result').textContent=text;el('result').className=error?'error':''}
function showMode(){const s=el('mode-static').checked;el('static-fields').hidden=!s;el('dhcp-note').hidden=s;fields.forEach(f=>el(f).disabled=!s)}
fetch('/api/network',{cache:'no-store'}).then(async r=>{if(!r.ok)throw Error(await r.text());return r.json()}).then(d=>{const c=d.current||{};
el('link').textContent=c.link?'Cable connected':'No link: check the Ethernet cable';el('desc').textContent=c.description||'';
el('cur-ip').textContent=c.ip||'';el('cur-mask').textContent=c.mask||'';el('cur-gateway').textContent=c.gateway||'';el('cur-dns').textContent=c.dns||'';el('mac').textContent=c.mac||'';el('hostname').textContent=c.hostname?c.hostname+'.local':'';
document.querySelectorAll('.fallback').forEach(n=>n.textContent=d.fallback_ip||'');
el(d.mode==='static'?'mode-static':'mode-dhcp').checked=true;fields.forEach(f=>el(f).value=d[f]||'');if(!el('mask').value)el('mask').value='255.255.255.0';
showMode();el('save').disabled=false}).catch(e=>say('Unable to load network settings: '+e.message,true));
document.querySelectorAll('input[name=mode]').forEach(r=>r.addEventListener('change',showMode));
el('form').addEventListener('submit',async e=>{e.preventDefault();const body={mode:el('mode-static').checked?'static':'dhcp'};
if(body.mode==='static')for(const f of fields){const v=el(f).value.trim(),bad=(f==='ip'||f==='mask'||v)&&!ipv4.test(v);el(f).setAttribute('aria-invalid',bad);if(bad){say('Enter a valid '+names[f]+', for example '+el(f).placeholder+'.',true);el(f).focus();return}body[f]=v}
el('save').disabled=true;say('Saving...');
try{const r=await fetch('/api/network',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify(body)});const t=await r.text();if(!r.ok)throw Error(t);
say(t+(body.mode==='static'?'\nOpen http://'+body.ip+'/ in about 20 seconds.':'\nFind the new address on your DHCP server or at the gateway name in about 20 seconds.'))}
catch(err){say('Save failed: '+err.message,true);el('save').disabled=false}});
</script></body></html>)html";
