const {createDemoServer}=require('../demo/mock_server');
let playwright;
try{playwright=require('../.tools/node_modules/playwright');}
catch{playwright=require('../../.tools/node_modules/playwright');}
const assert=require('node:assert/strict');
(async()=>{
  const server=createDemoServer();
  await new Promise(resolve=>server.listen(0,'127.0.0.1',resolve));
  const base=`http://127.0.0.1:${server.address().port}`;
  const browser=await playwright.chromium.launch({headless:true,executablePath:process.env.CHROME_PATH||'C:/Program Files/Google/Chrome/Application/chrome.exe'});
  try{
    const page=await browser.newPage({viewport:{width:1440,height:1000}}),errors=[];
    page.on('pageerror',error=>errors.push(error.message));
    await page.goto(base);
    await page.waitForFunction(()=>document.getElementById('alarm-summary').textContent==='Normal');
    assert(await page.getByRole('heading',{name:'SIMULATED IOC DEMO — no hardware connected'}).isVisible());
    await page.getByRole('button',{name:'High pressure',exact:true}).click();
    await page.waitForFunction(()=>document.getElementById('alarm-summary').textContent.includes('ALARM ACTIVE'));
    await page.getByRole('button',{name:'Acknowledge alarms',exact:true}).click();
    await page.waitForFunction(()=>document.getElementById('alarm-summary').textContent.includes('acknowledged'));
    assert.match(await page.locator('#live-channels').innerText(),/DO3[\s\S]*OFF/);
    await page.getByRole('button',{name:'Normal readings',exact:true}).click();
    await page.waitForFunction(()=>document.getElementById('alarm-summary').textContent==='Normal');
    await page.getByRole('button',{name:'Press capture button (DI0)',exact:true}).click();
    await page.waitForFunction(()=>document.getElementById('capture-counts').textContent.includes('1 accepted'));
    await page.getByRole('button',{name:'Broken 4–20 mA loop',exact:true}).click();
    await page.waitForFunction(()=>document.getElementById('live-channels').textContent.includes('SIGNAL FAULT'));
    await page.getByRole('button',{name:'Simulate device offline',exact:true}).click();
    await page.waitForFunction(()=>document.getElementById('connection').textContent.includes('unreachable'));
    await page.getByRole('button',{name:'Restore connection',exact:true}).click();
    await page.waitForFunction(()=>document.getElementById('connection').textContent.includes('IOC connected'));
    for(const route of ['/ioc/inputs','/ioc/analog','/ioc/outputs']){
      await page.goto(base+route);
      await page.waitForFunction(()=>document.getElementById('connection').textContent.includes('IOC connected'));
      assert(await page.getByRole('heading',{name:'SIMULATED IOC DEMO — no hardware connected'}).isVisible());
      await page.setViewportSize({width:390,height:844});
      assert(await page.evaluate(()=>document.documentElement.scrollWidth<=innerWidth),route);
    }
    await page.getByRole('button',{name:'Save and apply',exact:true}).click();
    await page.waitForFunction(()=>document.getElementById('message').textContent.includes('Demo settings saved'));
    const snapshot=await(await page.request.get(base+'/api/ioc')).json();
    const stale=await page.request.post(base+'/api/ioc/config',{data:{...snapshot.config,revision:snapshot.config.revision-1}});
    assert.equal(stale.status(),409);
    const invalid=await page.request.post(base+'/api/ioc/config',{data:{...snapshot.config,reference_mv:0}});
    assert.equal(invalid.status(),400);
    await Promise.all([page.waitForEvent('load'),page.getByRole('button',{name:'Reset demo presets',exact:true}).click()]);
    await page.waitForFunction(()=>document.getElementById('connection').textContent.includes('IOC connected'));
    await page.goto(base+'/sensor_fault_settings');
    await page.locator('#timeout').fill('2000');
    await page.locator('#mode').selectOption('1');
    await page.getByRole('button',{name:'Save settings',exact:true}).click();
    await page.waitForFunction(()=>document.getElementById('result').textContent.includes('saved and applied'));
    await page.request.post(base+'/demo/action',{data:{type:'sensor_loss',active:true}});
    await page.goto(base+'/ioc');
    await page.waitForFunction(()=>document.getElementById('live-channels').textContent.includes('LOST'));
    let loss=await(await page.request.get(base+'/api/ioc')).json();
    assert.equal(loss.state.sensor_fault,true);assert.equal(loss.state.sensor_alarm,true);assert.equal(loss.state.alarm,true);
    await page.getByRole('button',{name:'Acknowledge alarms',exact:true}).click();
    await page.request.post(base+'/demo/action',{data:{type:'sensor_loss',active:false}});
    await page.waitForFunction(()=>document.getElementById('alarm-summary').textContent==='Normal');
    loss=await(await page.request.get(base+'/api/ioc')).json();assert.equal(loss.state.sensor_fault,false);assert.equal(loss.state.sensor_alarm,false);
    assert.deepEqual(errors,[]);
    console.log('PASS: real demo server, simulated alarm/acknowledge/reset, capture button, loop fault, offline/reconnect, all pages/mobile, save validation, stale revisions and sensor-loss timeout/recovery/output policy.');
  }finally{await browser.close();await new Promise(resolve=>server.close(resolve));}
})().catch(error=>{console.error(error);process.exitCode=1;});
