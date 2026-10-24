// Hardware/browser smoke check. Requires the Windows Edge installation and local wifi_config.h.
const fs = require('node:fs');
const path = require('node:path');
const {spawn} = require('node:child_process');
const assert = require('node:assert/strict');
const config = fs.readFileSync(path.join(__dirname, 'wifi_config.h'), 'utf8');
const read = key => config.match(new RegExp(key + '\\s*=\\s*"([^"\\\\]+)"'))[1];
const base = 'http://' + (process.argv[2] || '192.168.137.173');
const authorization = 'Basic ' + Buffer.from(read('OTA_USER') + ':' + read('OTA_PASSWORD')).toString('base64');
const pause = ms => new Promise(resolve => setTimeout(resolve, ms));
async function api(data, headers = {}) {
  return fetch(base + (data ? '/control' : '/status'), {method: data ? 'POST' : 'GET', headers: {Authorization: authorization, ...headers}, body: data ? new URLSearchParams(data) : undefined});
}
async function main() {
  assert.equal((await fetch(base + '/status')).status, 401);
  assert.equal((await fetch(base + '/control', {method:'POST', body:new URLSearchParams({action:'all',value:'off'})})).status, 401);
  assert.equal((await api({action:'all',value:'off'}, {Origin:'http://external.invalid'})).status, 403);
  for (const data of [{action:'brightness',value:'101'}, {action:'floor',id:'TERREO',color:'#zz0000'}, {action:'floor',id:'unknown',on:'true'}, {action:'mode',value:'invalid'}]) assert.equal((await api(data)).status, 400);
  let state = await (await api({action:'all',value:'off'})).json();
  assert.deepEqual(state.floors.map(f=>[f.id,f.leds]),[['SUPERIOR',12],['CENTRAL',9],['TERREO',9]]);
  assert(state.floors.every(r=>!r.on));
  state = await (await api({action:'floor',id:'TERREO',color:'#123456'})).json();
  assert.equal(state.mode,'manual'); assert.equal(state.floors.find(r=>r.id==='TERREO').color,'#123456');
  assert.equal(state.floors.filter(r=>r.on).length,1);
  state = await (await api({action:'brightness',value:'40'})).json(); assert.equal(state.brightness,102);
  assert.equal((await fetch(base+'/update', {headers:{Authorization:authorization}})).status,200);
  console.log('Hardware API: authentication, origin, invalid commands, floor isolation, color, brightness and OTA page passed.');
  const edge = spawn('C:/Program Files (x86)/Microsoft/Edge/Application/msedge.exe', ['--headless','--disable-gpu','--no-first-run','--remote-debugging-port=9333','--user-data-dir='+path.resolve('.build/edge-smart-house-check'),'about:blank'], {windowsHide:true,stdio:'ignore'});
  let socket;
  try {
    let targets;
    for(let i=0;i<30;i++){try{targets=await(await fetch('http://127.0.0.1:9333/json/list')).json();break}catch{await pause(500)}}
    assert(targets,'Edge did not start');
    socket = new WebSocket(targets.find(t=>t.type==='page').webSocketDebuggerUrl);
    await new Promise((resolve,reject)=>{socket.onopen=resolve;socket.onerror=reject});
    let next=0; const pending=new Map();
    socket.onmessage=event=>{const data=JSON.parse(event.data);if(pending.has(data.id)){const {resolve,reject}=pending.get(data.id);pending.delete(data.id);data.error?reject(Error(data.error.message)):resolve(data.result)}};
    const cdp=(method,params={})=>new Promise((resolve,reject)=>{const id=++next;pending.set(id,{resolve,reject});socket.send(JSON.stringify({id,method,params}))});
    const evaluate=async expression=>{const r=await cdp('Runtime.evaluate',{expression,awaitPromise:true,returnByValue:true});if(r.exceptionDetails)throw Error(JSON.stringify(r.exceptionDetails));return r.result.value};
    await cdp('Network.enable');
    await cdp('Network.setExtraHTTPHeaders',{headers:{Authorization:authorization}});
    await cdp('Page.navigate',{url:base});
    for(let i=0;i<30;i++){await pause(300);if(await evaluate("document.querySelector('#connection')?.dataset.online==='true'"))break}
    assert.equal(await evaluate("document.querySelectorAll('.room').length"),3);
    assert.equal(await evaluate("document.querySelector('#connection').dataset.online"),'true');
    await evaluate("document.querySelector('[data-all=off]').click()"); await pause(700);
    assert((await(await api()).json()).floors.every(r=>!r.on));
    await evaluate("document.querySelector('[data-all=on]').click()"); await pause(700);
    assert((await(await api()).json()).floors.every(r=>r.on));
    for(const id of ['SUPERIOR','CENTRAL','TERREO']) {
      await evaluate(`document.querySelector('[data-room=${id}]').click()`);await pause(700);
      const floors=(await(await api()).json()).floors;
      assert.equal(floors.find(f=>f.id===id).on,false);
      assert(floors.filter(f=>f.id!==id).every(f=>f.on));
      await evaluate(`document.querySelector('[data-room=${id}]').click()`);await pause(700);
    }
    await evaluate("const c=document.querySelector('[data-hue=TERREO]');c.value='120';c.dispatchEvent(new Event('input'));c.dispatchEvent(new Event('change'))"); await pause(700);
    let actual=await(await api()).json();assert.equal(actual.mode,'manual');assert.equal(actual.floors.find(r=>r.id==='TERREO').color,'#00ff00');
    assert.equal(await evaluate("document.querySelectorAll('input[type=color]').length"),0);
    assert.equal(await evaluate("document.querySelectorAll('input[type=range][data-hue]').length"),3);
    assert.equal(await evaluate("document.querySelector('#hue-value-TERREO').textContent"),'120\u00b0');
    for(const [hue,color] of [[0,'#ff0000'],[60,'#ffff00'],[240,'#0000ff'],[359,'#ff0004']]) {
      await evaluate(`{const slider=document.querySelector('[data-hue=SUPERIOR]');slider.value='${hue}';slider.dispatchEvent(new Event('input'));slider.dispatchEvent(new Event('change'))}`);await pause(500);
      assert.equal((await(await api()).json()).floors.find(f=>f.id==='SUPERIOR').color,color);
    }
    console.log('Hue sliders: three ranges, no color input, degree label, primary colors and 359-degree endpoint passed.');
    await evaluate("const b=document.querySelector('#brightness');b.value='65';b.dispatchEvent(new Event('change'))");await pause(700);
    assert.equal((await(await api()).json()).brightness,166);
    await evaluate("const m=document.querySelector('#mode');m.value='hsl_cycle';m.dispatchEvent(new Event('change'))");await pause(700);
    assert.equal((await(await api()).json()).mode,'hsl_cycle');
    for(const effect of ['breathing','color_wave','chase']) {
      await evaluate(`{const selector=document.querySelector('#mode');selector.value='${effect}';selector.dispatchEvent(new Event('change'))}`);await pause(700);
      const samples=[];
      for(let i=0;i<5;i++) {
        const snapshot=await(await api()).json();
        assert.equal(snapshot.mode,effect);assert.equal(snapshot.brightness,166);
        const colors=snapshot.floors.map(f=>f.rendered_color);
        assert(colors.every(c=>/^#[0-9a-f]{6}$/.test(c)));
        if(effect==='color_wave')assert.equal(new Set(colors).size,3);
        if(effect==='chase')assert(colors.filter(c=>c!=='#000000').length<=1);
        samples.push(colors.join(','));await pause(200);
      }
      assert(new Set(samples).size>1, effect+' must animate');
      await api({action:'floor',id:'CENTRAL',on:'false'});await pause(50);
      assert.equal((await(await api()).json()).floors.find(f=>f.id==='CENTRAL').rendered_color,'#000000');
      await api({action:'floor',id:'CENTRAL',on:'true'});
    }
    console.log('Effects: browser selection, animation, distinct wave colors, chase isolation, floor off and preserved global brightness passed.');
    await evaluate("{const mode=document.querySelector('#mode');mode.value='snow';mode.dispatchEvent(new Event('change'))}");await pause(500);
    assert.equal(await evaluate("document.querySelector('#snowControls').hidden"),false);
    for (const [selector,value] of [['snowSpeed','5'],['snowDensity','8']]) {
      await evaluate(`{const input=document.querySelector('#${selector}');input.value='${value}';input.dispatchEvent(new Event('change'))}`);await pause(500);
    }
    const frames=[];
    for(let i=0;i<10;i++) {
      const snow=await(await api()).json();
      assert.equal(snow.mode,'snow');assert.equal(snow.snow_speed,5);assert.equal(snow.snow_density,8);assert.equal(snow.brightness,166);
      const pixels=snow.floors.flatMap(f=>{assert.equal(f.pixels.length,f.leds);return f.pixels});
      assert(pixels.some(c=>c==='#00030c'),'snow has a dim blue background');
      assert(pixels.some(c=>parseInt(c.slice(1,3),16)>0),'snow has bright flakes');
      frames.push(pixels.join(','));await pause(120);
    }
    assert(new Set(frames).size>1,'snow particles must animate');
    assert.equal((await api({action:'snow',speed:'0'})).status,400);
    assert.equal((await api({action:'snow',speed:'1',density:'9'})).status,400);
    assert.equal((await(await api()).json()).snow_speed,5,'invalid settings are atomic');
    await api({action:'floor',id:'CENTRAL',on:'false'});await pause(50);
    assert((await(await api()).json()).floors.find(f=>f.id==='CENTRAL').pixels.every(c=>c==='#000000'));
    await api({action:'floor',id:'CENTRAL',on:'true'});
    await api({action:'snow',speed:'3',density:'4'});
    console.log('Snow: browser selection, settings, animated sparse particles, blue background, disabled floor and invalid settings passed.');
    await evaluate("{const mode=document.querySelector('#mode');mode.value='halloween';mode.dispatchEvent(new Event('change'))}");await pause(500);
    assert.equal(await evaluate("document.querySelector('#snowControls').hidden"),true);
    const halloweenFrames=[];
    let orangeSeen=false,purpleSeen=false;
    const halloweenLevels=[[],[],[]];
    for(let i=0;i<40;i++) {
      const state=await(await api()).json();
      assert.equal(state.mode,'halloween');assert.equal(state.brightness,166);
      const pixels=state.floors.flatMap(f=>{assert.equal(f.pixels.length,f.leds);return f.pixels});
      const rgb=pixels.map(c=>[1,3,5].map(n=>parseInt(c.slice(n,n+2),16)));
      orangeSeen ||= rgb.some(([r,g,b])=>r>b*2&&g>0);
      purpleSeen ||= rgb.some(([r,g,b])=>b>r&&g<10);
      assert(rgb.every(([r,g,b])=>g<=55&&r>0),'warm/purple palette without blackouts');
      state.floors.forEach((f,index)=>halloweenLevels[index].push(f.pixels.reduce((sum,c)=>sum+parseInt(c.slice(1,3),16),0)));
      halloweenFrames.push(pixels.join(','));await pause(80);
    }
    assert(new Set(halloweenFrames).size>1,'Halloween must animate');
    assert(orangeSeen&&purpleSeen,'orange and purple must remain present');
    for(const levels of halloweenLevels)assert(Math.max(...levels)>Math.min(...levels)*4,'each floor must flash visibly');
    await api({action:'floor',id:'CENTRAL',on:'false'});await pause(50);
    assert((await(await api()).json()).floors.find(f=>f.id==='CENTRAL').pixels.every(c=>c==='#000000'));
    await api({action:'floor',id:'CENTRAL',on:'true'});
    console.log('Halloween: browser selection, orange/purple palette, candle animation, disabled floor and unchanged global brightness passed.');
    await evaluate("{const mode=document.querySelector('#mode');mode.value='blink';mode.dispatchEvent(new Event('change'))}");await pause(500);
    const blinkStates=[new Set(),new Set(),new Set()];
    for(let i=0;i<16;i++) {
      const state=await(await api()).json();assert.equal(state.mode,'blink');assert.equal(state.brightness,166);
      state.floors.forEach((f,index)=>{assert(f.rendered_color==='#000000'||f.rendered_color===f.color);blinkStates[index].add(f.rendered_color==='#000000'?'off':'on')});
      await pause(100);
    }
    assert(blinkStates.every(states=>states.size===2),'every floor must blink on and off');
    await api({action:'floor',id:'CENTRAL',on:'false'});
    for(let i=0;i<6;i++){await pause(100);assert.equal((await(await api()).json()).floors.find(f=>f.id==='CENTRAL').rendered_color,'#000000')}
    await api({action:'floor',id:'CENTRAL',on:'true'});
    await api({action:'mode',value:'halloween'});
    console.log('Flashing effects: strong Halloween flashes on each floor; alternating blink on/off; disabled floor and brightness preserved.');
    await cdp('Emulation.setDeviceMetricsOverride',{width:390,height:844,deviceScaleFactor:1,mobile:true});
    assert.equal(await evaluate('document.documentElement.scrollWidth <= innerWidth'),true);
    fs.writeFileSync(path.resolve('.build/smart-house-mobile.png'),Buffer.from((await cdp('Page.captureScreenshot',{format:'png',captureBeyondViewport:true})).data,'base64'));
    await cdp('Emulation.setDeviceMetricsOverride',{width:1280,height:1000,deviceScaleFactor:1,mobile:false});
    fs.writeFileSync(path.resolve('.build/smart-house-desktop.png'),Buffer.from((await cdp('Page.captureScreenshot',{format:'png',captureBeyondViewport:true})).data,'base64'));
    console.log('Edge: three floors, connection, on/off, color, brightness, mode and mobile width passed; screenshots saved in .build.');
    await cdp('Browser.close');
  } finally {if(socket)socket.close();edge.kill()}
}
main().catch(error=>{console.error(error.message);process.exitCode=1}).finally(async()=>{
  try{const mode=process.argv.includes('--halloween')?'halloween':process.argv.includes('--snow')?'snow':'hsl_cycle';for(const data of [{action:'all',value:'on'},{action:'brightness',value:'100'},{action:'snow',speed:'3',density:'4'},{action:'mode',value:mode}])await api(data);console.log('Final: all on, 100% brightness, mode='+mode)}catch(error){console.error('Restore failed: '+error.message);process.exitCode=1}
});
