// Explicit hardware API check. Never uploads firmware; restores initial lighting.
const fs=require('node:fs'),path=require('node:path'),assert=require('node:assert/strict');
const address=process.argv[2];
if(!address)throw Error('Uso: node smart-house-test/check-web.cjs IP_DA_PLACA');
const config=fs.readFileSync(path.join(__dirname,'wifi_config.h'),'utf8');
const read=key=>config.match(new RegExp(key+'\\s*=\\s*"([^"\\\\]+)"'))[1];
const authorization='Basic '+Buffer.from(read('OTA_USER')+':'+read('OTA_PASSWORD')).toString('base64');
const base='http://'+address;
async function api(data,extra={}){return fetch(base+(data?'/control':'/status'),{method:data?'POST':'GET',headers:{Authorization:authorization,...extra},body:data?new URLSearchParams(data):undefined,signal:AbortSignal.timeout(4000)})}
let initial;
async function main(){
 assert.equal((await fetch(base+'/status')).status,401);
 assert.equal((await fetch(base+'/control',{method:'POST',body:new URLSearchParams({action:'all',value:'off'})})).status,401);
 initial=await(await api()).json();
 assert.equal((await api({action:'all',value:'off'},{Origin:'http://external.invalid'})).status,403);
 for(const value of ['breathing','color_wave','chase','blink','invalid'])assert.equal((await api({action:'mode',value})).status,400);
 for(const data of [{action:'brightness',value:'101'},{action:'floor',id:'TERREO',color:'#zz0000'},{action:'floor',id:'unknown',on:'true'}])assert.equal((await api(data)).status,400);
 for(const value of ['manual','hsl_cycle','snow','halloween','christmas']){
  assert.equal((await api({action:'mode',value})).status,200);
  const state=await(await api({action:'floor',id:'CENTRAL',on:'false'})).json();
  assert.equal(state.mode,value);assert.equal(state.floors[1].on,false);
  if(state.floors[1].pixels)assert(state.floors[1].pixels.every(c=>c==='#000000'));
  assert.equal(state.floors[1].rendered_color,'#000000');
 }
 const state=await(await api({action:'floor',id:'TERREO',color:'#123456'})).json();
 assert.equal(state.mode,'manual');assert.equal(state.floors[2].color,'#123456');
 assert.equal((await fetch(base+'/update',{headers:{Authorization:authorization}})).status,200);
 console.log('PASS: authentication, origin, five modes, removed modes, disabled floor, color and OTA page.');
}
main().catch(e=>{console.error(e.message);process.exitCode=1}).finally(async()=>{
 if(!initial)return;
 try{
  for(const f of initial.floors)await api({action:'floor',id:f.id,color:f.color});
  for(const f of initial.floors)await api({action:'floor',id:f.id,on:String(f.on)});
  await api({action:'brightness',value:String(Math.round(initial.brightness*100/255))});
  await api({action:'snow',speed:String(initial.snow_speed),density:String(initial.snow_density)});
  await api({action:'mode',value:initial.mode});
 }catch(e){console.error('Falha ao restaurar luzes: '+e.message);process.exitCode=1}
});
