// Uses the embedded page with simulated demo responses, never physical UARTs.
const assert=require('node:assert/strict');
const {chromium}=require('../../.tools/node_modules/playwright');
const {createDemoServer}=require('../demo/mock_server');
(async()=>{
 const server=createDemoServer();await new Promise(r=>server.listen(0,'127.0.0.1',r));
 const base=`http://127.0.0.1:${server.address().port}`;
 const browser=await chromium.launch({executablePath:'C:/Program Files/Google/Chrome/Application/chrome.exe',headless:true});
 try{
  const page=await browser.newPage();const errors=[];page.on('pageerror',e=>errors.push(e.message));
  await page.goto(base+'/sensor_setup');await page.waitForFunction(()=>!document.getElementById('search').disabled);
  assert.match(await page.locator('body').innerText(),/SIMULATED DEMO/);
  await page.locator('#search').click();await page.waitForFunction(()=>!document.getElementById('cancel').disabled);
  await page.locator('#cancel').click();await page.waitForFunction(()=>document.getElementById('status').textContent.includes('cancelled'));
  await page.locator('#search').click();await page.waitForFunction(()=>!document.getElementById('use').disabled);
  await page.locator('#use').click();await page.waitForFunction(()=>document.getElementById('status').textContent.includes('Gateway: 9600'));
  await page.locator('#baud').selectOption('9600');await page.locator('#id').fill('2');
  await page.locator('#configure').click();await page.waitForFunction(()=>document.getElementById('status').textContent.includes('ID 2'));
  assert.match(await page.locator('#diagnostics').innerText(),/CRC errors: 0/);
  const invalid=await page.request.post(base+'/api/sensor_setup',{data:{action:'configure',baud:19200,id:3}});
  assert.equal(invalid.status(),400);
  await page.setViewportSize({width:375,height:812});
  assert(await page.evaluate(()=>document.documentElement.scrollWidth<=innerWidth));
  await page.locator('#first').focus();await page.keyboard.press('Tab');assert.equal(await page.evaluate(()=>document.activeElement.id),'last');
  await page.context().setOffline(true);await page.waitForFunction(()=>document.getElementById('status').textContent.includes('Offline'));
  assert(await page.locator('#search').isDisabled());
  await page.context().setOffline(false);await page.waitForFunction(()=>!document.getElementById('search').disabled);
  assert.deepEqual(errors,[]);
  console.log('PASS simulated setup UI: search/cancel/use, configuration, diagnostics, rejected dual change, mobile, keyboard and offline/recovery.');
 }finally{await browser.close();await new Promise(r=>server.close(r));}
})().catch(e=>{console.error(e);process.exitCode=1});
