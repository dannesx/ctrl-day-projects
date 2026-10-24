// Run against the local preview only. WAV parser WASM is built from firmware's header.
const fs=require('node:fs'),assert=require('node:assert/strict');
const base=process.env.PREVIEW_URL||'http://127.0.0.1:8765';
function wav(length=3200){
 const b=Buffer.alloc(44+length);b.write('RIFF');b.writeUInt32LE(b.length-8,4);b.write('WAVEfmt ',8);
 b.writeUInt32LE(16,16);b.writeUInt16LE(1,20);b.writeUInt16LE(1,22);b.writeUInt32LE(16000,24);
 b.writeUInt32LE(32000,28);b.writeUInt16LE(2,32);b.writeUInt16LE(16,34);b.write('data',36);b.writeUInt32LE(length,40);return b;
}
async function status(){return (await fetch(base+'/status')).json()}
async function command(data){const r=await fetch(base+'/control',{method:'POST',body:new URLSearchParams(data)});assert.equal(r.status,200);return r.json()}
async function upload(name,bytes,headers={}){const data=new FormData();data.append('audio',new Blob([bytes]),name);return fetch(base+'/audio/upload',{method:'POST',body:data,headers})}
async function main(){
 const {instance}=await WebAssembly.instantiate(fs.readFileSync(process.argv[2]||'.build/wav-test.wasm'));
 const e=instance.exports;const validate=b=>{new Uint8Array(e.memory.buffer,e.wav_buffer(),b.length).set(b);return Boolean(e.wav_validate(b.length))};
 const good=wav();assert(validate(good));
 for(const offset of [0,4,8,16,20,22,24,28,32,34,40]){const b=Buffer.from(good);b[offset]^=1;assert(!validate(b),'Invalid header offset '+offset)}
 assert(!validate(good.subarray(0,good.length-1)));assert(!validate(wav(0)));
 const padded=Buffer.concat([good.subarray(0,12),Buffer.from('JUNK\x01\0\0\0X\0','binary'),good.subarray(12)]);padded.writeUInt32LE(padded.length-8,4);assert(validate(padded));
 const oversized=Buffer.from(good);oversized.writeUInt32LE(0xffffffff,40);assert(!validate(oversized));
 const duplicate=Buffer.concat([good,good.subarray(36)]);duplicate.writeUInt32LE(duplicate.length-8,4);assert(!validate(duplicate));
 for(let i=0;i<300;i++){const b=Buffer.from(good.subarray(0,i));if(b.length>=8)b.writeUInt32LE(b.length-8,4);assert(!validate(b))}
 assert((await status()).audio.preview,'Use only the local preview');
 if(!(await status()).audio.storage_ready){
  assert.equal((await upload('inicio.wav',good)).status,409);
  assert.equal((await fetch(base+'/audio/initialize',{method:'POST',body:new URLSearchParams({confirm:'wrong'})})).status,400);
  assert.equal((await fetch(base+'/audio/initialize',{method:'POST',body:new URLSearchParams({confirm:'ERASE_AUDIO_DATA'})})).status,200);
 }
 assert.equal((await upload('../inicio.wav',good)).status,400);
 assert.equal((await upload('inicio.wav',good,{Origin:'http://untrusted.invalid'})).status,403);
 assert.equal((await upload('inicio.wav',Buffer.from('not audio'))).status,400);
 assert.equal((await upload('inicio.wav',wav(128000))).status,413);
 assert.equal((await upload('inicio.wav',good)).status,200);
 assert.equal((await upload('inicio.wav',wav(6400))).status,200);
 assert.equal((await upload('inicio.wav',oversized)).status,400);
 assert.equal((await status()).audio.files[0].size,6444);
 for(const name of ['tudo_on','tudo_off','superior_off','manual','neve','matrix'])assert.equal((await upload(name+'.wav',good)).status,200);
 await command({action:'audio',volume:25,muted:false});await command({action:'all',value:'on'});
 let s=await command({action:'all',value:'off'});assert.equal(s.audio.last_announcement,'tudo_off');let count=s.audio.announcement_count;
 s=await command({action:'all',value:'off'});assert.equal(s.audio.announcement_count,count);
 s=await command({action:'brightness',value:50});assert.equal(s.brightness,96);assert.equal(s.brightness_percent,50);assert.equal(s.audio.announcement_count,count);
 s=await command({action:'floor',id:'SUPERIOR',color:'#aabbcc'});assert.equal(s.audio.announcement_count,count);
 s=await command({action:'floor',id:'SUPERIOR',on:false});assert.equal(s.audio.last_announcement,'superior_off');count=s.audio.announcement_count;
 s=await command({action:'floor',id:'CENTRAL',on:true});assert.equal(s.audio.announcement_count,count+(s.audio.files.find(f=>f.name==='central_on.wav').available?1:0)); // missing clips stay silent on a fresh preview
 s=await command({action:'mode',value:'snow'});assert.equal(s.audio.last_announcement,'neve');count=s.audio.announcement_count;
 s=await command({action:'mode',value:'matrix'});assert.equal(s.audio.last_announcement,'matrix');count=s.audio.announcement_count;
 s=await command({action:'mode',value:'matrix'});assert.equal(s.audio.announcement_count,count);
 await command({action:'audio',muted:true});s=await command({action:'mode',value:'manual'});assert.equal(s.audio.announcement_count,count);
 await command({action:'audio',muted:false,volume:0});s=await command({action:'mode',value:'snow'});assert.equal(s.audio.announcement_count,count);
 for(const data of [{action:'audio',volume:101},{action:'audio',muted:'yes'},{action:'audio',volume:10,muted:'invalid'}]){
  assert.equal((await fetch(base+'/control',{method:'POST',body:new URLSearchParams(data)})).status,400);
  assert.equal((await status()).audio.volume,0);
 }
 const names=(await status()).audio.files.map(f=>f.name);let rejected=false;
 for(const name of names){const r=await upload(name,wav(127000));if(r.status===413){rejected=true;break}assert.equal(r.status,200)}
 assert(rejected,'Total size limit should reject before all 15 files');
 await command({action:'audio',muted:false,volume:25});
 console.log('PASS: firmware WAV parser, malformed chunks, uploads/replacement/limits/origin, brightness, effective-action announcements, missing audio, mute and volume.');
}
main().catch(e=>{console.error(e);process.exitCode=1});
