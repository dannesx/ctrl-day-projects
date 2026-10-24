// Local preview integration and viewport checks; no hardware or credentials.
const fs=require('node:fs'),path=require('node:path'),{spawn}=require('node:child_process'),assert=require('node:assert/strict');
const pause=ms=>new Promise(r=>setTimeout(r,ms));
const base='http://127.0.0.1:8765';
async function api(data){const r=await fetch(base+(data?'/control':'/status'),{method:data?'POST':'GET',body:data?new URLSearchParams(data):undefined});return r}
async function main(){
 await api({action:'all',value:'on'});
 for(const value of ['breathing','color_wave','chase','blink','invalid'])assert.equal((await api({action:'mode',value})).status,400);
 for(const value of ['manual','hsl_cycle','snow','halloween','christmas'])assert.equal((await api({action:'mode',value})).status,200);
 assert.equal((await fetch(base+'/control',{method:'POST',headers:{Origin:'http://untrusted.invalid'},body:new URLSearchParams({action:'all',value:'off'})})).status,403);
 const edgePath=process.env.EDGE_PATH||'C:/Program Files (x86)/Microsoft/Edge/Application/msedge.exe';
 const edge=spawn(edgePath,['--headless','--disable-gpu','--no-first-run','--remote-debugging-port=9341','--user-data-dir='+path.resolve('.build/edge-fair-preview'),'about:blank'],{windowsHide:true,stdio:'ignore'});
 let socket;
 try{
  let targets;
  for(let i=0;i<30;i++){try{targets=await(await fetch('http://127.0.0.1:9341/json/list')).json();break}catch{await pause(200)}}
  assert(targets,'Edge did not start');socket=new WebSocket(targets.find(t=>t.type==='page').webSocketDebuggerUrl);
  await new Promise((resolve,reject)=>{socket.onopen=resolve;socket.onerror=reject});
  let next=0;const pending=new Map();
  socket.onmessage=event=>{const d=JSON.parse(event.data);if(pending.has(d.id)){const {resolve,reject}=pending.get(d.id);pending.delete(d.id);d.error?reject(Error(d.error.message)):resolve(d.result)}};
  const cdp=(method,params={})=>new Promise((resolve,reject)=>{const id=++next;pending.set(id,{resolve,reject});socket.send(JSON.stringify({id,method,params}))});
  const evaluate=async expression=>{const r=await cdp('Runtime.evaluate',{expression,awaitPromise:true,returnByValue:true});if(r.exceptionDetails)throw Error(JSON.stringify(r.exceptionDetails));return r.result.value};
  await cdp('Page.navigate',{url:base});
  for(let i=0;i<30;i++){await pause(100);if(await evaluate("document.querySelector('#connection')?.dataset.online==='true'"))break}
  assert.equal(await evaluate("document.querySelectorAll('.room').length"),3);
  for(const [width,height] of [[1366,768],[1920,1080],[390,844]]){
   await cdp('Emulation.setDeviceMetricsOverride',{width,height,deviceScaleFactor:1,mobile:width<700});
   for(const mode of ['manual','hsl_cycle','snow','halloween','christmas']){
    await evaluate(`document.querySelector('[data-mode=${mode}]').click()`);await pause(250);
    const sizes=await evaluate(`({w:innerWidth,h:innerHeight,sw:document.documentElement.scrollWidth,sh:document.documentElement.scrollHeight,off:[...document.querySelectorAll('button,input,a')].filter(e=>e.getClientRects().length).filter(e=>{const r=e.getBoundingClientRect();return r.left<0||r.right>innerWidth+1||r.top<0||r.bottom>innerHeight+1}).map(e=>e.outerHTML)})`);
    assert(sizes.sw<=width && sizes.sh<=height,JSON.stringify({width,height,mode,sizes}));assert.deepEqual(sizes.off,[]);
    assert.equal((await(await api()).json()).mode,mode);
   }
   await evaluate("document.querySelector('[data-mode=snow]').click()");await pause(250);
   fs.mkdirSync('.build',{recursive:true});fs.writeFileSync(`.build/casa-${width}x${height}.png`,Buffer.from((await cdp('Page.captureScreenshot',{format:'png'})).data,'base64'));
  }
  await evaluate("document.querySelector('[data-room=CENTRAL]').click()");await pause(300);
  assert.equal((await(await api()).json()).floors[1].on,false);
  await evaluate("{const c=document.querySelector('[data-color=TERREO]');c.value='#123456';c.dispatchEvent(new Event('change'))}");await pause(300);
  const state=await(await api()).json();assert.equal(state.mode,'manual');assert.equal(state.floors[2].color,'#123456');assert.equal(state.floors[1].on,false);
  for(const value of ['off','on']){
   await evaluate(`document.querySelector('[data-all=${value}]').click()`);await pause(250);
   assert((await(await api()).json()).floors.every(f=>f.on===(value==='on')));
  }
  for(const [id,value] of [['brightness',42],['speed',5],['density',8]]){
   await evaluate(`{const e=document.getElementById('${id}');e.value=${value};e.dispatchEvent(new Event('change'))}`);await pause(250);
  }
  const adjusted=await(await api()).json();assert.equal(adjusted.brightness,107);assert.equal(adjusted.snow_speed,5);assert.equal(adjusted.snow_density,8);
  await cdp('Network.enable');
  await cdp('Network.emulateNetworkConditions',{offline:true,latency:0,downloadThroughput:0,uploadThroughput:0});await pause(500);
  assert.equal(await evaluate("document.querySelector('#connection').dataset.online"),'false');
  assert.equal(await evaluate("[...document.querySelectorAll('button,input')].every(e=>e.disabled)"),true);
  await cdp('Network.emulateNetworkConditions',{offline:false,latency:0,downloadThroughput:-1,uploadThroughput:-1});await pause(500);
  assert.equal(await evaluate("document.querySelector('#connection').dataset.online"),'true');
  await cdp('Browser.close');console.log('PASS: five modes, removed API modes, floor/color/power/brightness/snow controls, disconnect/reconnect, 15 viewport/mode combinations; screenshots in .build.');
 }finally{socket?.close();edge.kill()}
}
main().catch(e=>{console.error(e);process.exitCode=1});
