// UI integration uses simulated IOC responses; the C++ logic has separate native tests.
let playwright;
try { playwright = require('../.tools/node_modules/playwright'); }
catch { playwright = require('../../.tools/node_modules/playwright'); }
const {chromium} = playwright;
const fs = require('node:fs'), path = require('node:path'), http = require('node:http');
const assert = require('node:assert/strict');
const html = fs.readFileSync(path.join(__dirname,'../src/ioc/pages.h'),'utf8').match(/R"ioc\(([\s\S]*?)\)ioc"/)[1];
let config = {
  revision:0, reference_mv:3320,
  digital:Array.from({length:11},(_,i)=>({name:`DI${i}`,action:0,active_low:false,delay_ms:500})),
  analog:Array.from({length:2},(_,i)=>({name:`AN${i}`,unit:'bar',mode:0,minimum:0,maximum:100,low:10,high:90,hysteresis:1,low_enabled:false,high_enabled:false,fault_alarm:false,delay_ms:1000})),
  outputs:[{source:0,input:0},{source:0,input:0}]
};
let state={ready:true,sequence:0,age_ms:10,alarm:false,latched:false,acknowledged:false,sensor_alert:false,sensor_alarm:false,capture_accepted:2,capture_rejected:1,
  digital:Array.from({length:11},()=>({active:false,alarm:false})),
  analog:Array.from({length:2},()=>({adc:1000,measured:12,value:50,fault:false,alarm:false})),outputs:[false,false]};
let supported=true, offline=false, failSave=false, saves=0, acknowledgements=0, lastSave;
const server=http.createServer((req,res)=>{
  if(offline){res.destroy();return;}
  const send=(code,body,type='application/json')=>{res.writeHead(code,{'Content-Type':type});res.end(type==='application/json'?JSON.stringify(body):body);};
  if(req.url==='/api/ioc')return send(200,{supported,config,state});
  if(req.url==='/api/ioc/config'&&req.method==='POST'){
    let body='';req.on('data',chunk=>body+=chunk);req.on('end',()=>{
      lastSave=JSON.parse(body);
      if(failSave)return send(400,'Invalid I/O settings.','text/plain');
      if(lastSave.revision!==config.revision)return send(409,'Settings changed. Reload.','text/plain');
      config={...lastSave,revision:config.revision+1};saves++;send(200,'I/O settings saved and applied.','text/plain');
    });return;
  }
  if(req.url==='/api/ioc/acknowledge'&&req.method==='POST'){
    acknowledgements++;state.acknowledged=true;return send(200,'Acknowledgement requested. Active conditions remain alarms.','text/plain');
  }
  if(['/ioc','/ioc/inputs','/ioc/analog','/ioc/outputs'].includes(req.url))return send(200,html,'text/html; charset=utf-8');
  send(404,'Missing','text/plain');
});
(async()=>{
  await new Promise(resolve=>server.listen(0,'127.0.0.1',resolve));
  const base=`http://127.0.0.1:${server.address().port}`;
  const browser=await chromium.launch({headless:true,executablePath:process.env.CHROME_PATH||'C:/Program Files/Google/Chrome/Application/chrome.exe'});
  try{
    const page=await browser.newPage({viewport:{width:1440,height:1000}}),errors=[],external=[];
    page.on('pageerror',error=>errors.push(error.message));
    await page.route('**/*',route=>{if(!route.request().url().startsWith(base)){external.push(route.request().url());return route.abort();}return route.continue();});
    await page.goto(base+'/ioc/inputs');
    await page.waitForSelector('#field-digital\\.0\\.name');
    assert.equal(await page.locator('#editor article').count(),11);
    await page.getByRole('button',{name:'Use DI0 for capture and DI1 for acknowledge'}).click();
    await page.getByRole('button',{name:'Save and apply'}).click();
    await page.waitForFunction(()=>document.getElementById('message').textContent.includes('saved and applied'));
    assert.equal(saves,1);assert.equal(lastSave.digital[0].action,3);assert.equal(lastSave.digital[1].action,4);
    assert.deepEqual(lastSave.outputs,[{source:0,input:0},{source:0,input:0}]);
    assert.equal(await page.locator('#editor select[name^="analog"]').count(),0);
    await page.getByRole('link',{name:'Analog & 4–20 mA',exact:true}).click();
    await page.waitForSelector('[name="analog.0.mode"]');
    await page.locator('[name="analog.0.mode"]').selectOption('1');
    await page.locator('[name="analog.0.name"]').fill('<img src=x>');
    await page.locator('[name="analog.0.maximum"]').fill('10');
    await page.locator('[name="analog.0.high"]').fill('8');
    await page.locator('[name="analog.0.high_enabled"]').check();
    failSave=true;
    await page.getByRole('button',{name:'Save and apply'}).click();
    await page.waitForFunction(()=>document.getElementById('message').textContent.includes('Save failed'));
    assert.equal(await page.locator('[name="analog.0.maximum"]').inputValue(),'10');
    assert.equal(config.analog[0].mode,0); // Failed save keeps edits and old configuration.
    failSave=false;
    await page.getByRole('button',{name:'Save and apply'}).click();
    await page.waitForFunction(()=>document.getElementById('message').textContent.includes('saved and applied'));
    assert.equal(lastSave.analog[0].mode,1);assert.equal(lastSave.analog[0].maximum,10);
    assert.equal(lastSave.digital[0].action,3); // Saving this page preserves input choices.
    assert.equal(await page.locator('#editor img').count(),0);
    await page.getByRole('link',{name:'Alarm outputs',exact:true}).click();
    await page.waitForSelector('[name="outputs.0.source"]');
    await page.getByRole('button',{name:'Use DO2 as latched beacon and DO3 as sounder'}).click();
    await page.getByRole('button',{name:'Save and apply'}).click();
    await page.waitForFunction(()=>document.getElementById('message').textContent.includes('saved and applied'));
    assert.deepEqual(lastSave.outputs,[{source:2,input:0},{source:3,input:0}]);
    assert.equal(await page.locator('[name^="outputs."]').count(),4); // Only spare outputs are editable.
    state.alarm=true;state.latched=true;
    await page.getByRole('link',{name:'Live status',exact:true}).click();
    await page.waitForFunction(()=>document.getElementById('alarm-summary').textContent.includes('ALARM ACTIVE'));
    await page.getByRole('button',{name:'Acknowledge alarms'}).click();
    await page.waitForFunction(()=>document.getElementById('alarm-summary').textContent.includes('acknowledged'));
    assert.equal(acknowledgements,1);assert.equal(state.alarm,true);
    assert.equal(await page.locator('#live-channels img').count(),0);
    offline=true;
    await page.waitForFunction(()=>document.getElementById('connection').textContent.includes('unreachable'));
    assert(await page.getByRole('button',{name:'Acknowledge alarms'}).isDisabled());
    offline=false;
    await page.waitForFunction(()=>document.getElementById('connection').textContent.includes('IOC connected'));
    for(const route of ['/ioc','/ioc/inputs','/ioc/analog','/ioc/outputs']){
      await page.setViewportSize({width:390,height:844});await page.goto(base+route);
      await page.waitForFunction(()=>document.getElementById('connection').textContent.includes('IOC connected'));
      assert(await page.evaluate(()=>document.documentElement.scrollWidth<=innerWidth),route+' mobile overflow');
      await page.keyboard.press('Tab');assert(await page.evaluate(()=>document.activeElement!==document.body));
    }
    supported=false;await page.goto(base+'/ioc');
    await page.waitForFunction(()=>document.getElementById('connection').textContent.includes('available on the IRIV'));
    assert(await page.locator('#config-form').isHidden());
    assert.deepEqual(errors,[]);assert.deepEqual(external,[]);
    console.log('PASS: separate IOC pages, typed configuration saves/preservation, failed-save recovery, beacon/sounder presets, acknowledge, offline/recovery, mobile/focus, label injection and ESP32 unsupported state.');
  }finally{await browser.close();server.close();}
})().catch(error=>{console.error(error);server.close();process.exitCode=1;});
