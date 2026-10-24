const fs=require('node:fs'),assert=require('node:assert/strict'),path=require('node:path');
(async()=>{
 const {instance}=await WebAssembly.instantiate(fs.readFileSync(path.join(__dirname,'effects.wasm')));
 const e=instance.exports;e.__wasm_call_ctors?.();
 for(let mode=0;mode<6;mode++){
  e.settings(mode,3,4);e.floor_settings(0,1,0x123456);e.floor_settings(1,0,0x123456);e.floor_settings(2,1,0x123456);
  for(let now=0;now<6000;now+=20){e.render(now);for(let p=0;p<10;p++)assert.equal(e.pixel(1,p),0)}
 }
 e.floor_settings(1,1,0x123456);e.settings(3,3,4);
 const flashes=[0,0,0],blackouts=[0,0,0],palettes=[new Set(),new Set(),new Set()];
 for(let now=6000;now<186000;now+=20){e.render(now);for(let f=0;f<3;f++){const v=e.pixel(f,0);palettes[f].add(v);if(v===0xffd29b)flashes[f]++;if(v===0)blackouts[f]++;}}
 assert(flashes.every(n=>n>0));assert(blackouts.every(n=>n>0));assert(palettes.every(p=>p.size>50));
 e.settings(4,3,4);const all=new Set();
 for(let now=0;now<15000;now+=20){e.render(now);for(let f=0;f<3;f++)for(let p=0;p<10;p++)all.add(e.pixel(f,p));}
 assert(all.has(0xdc000c));assert(all.has(0x00b420));assert(all.size>100);
 e.settings(5,3,4);
 for(let f=0;f<3;f++){
  const step=110+f*35;
  for(let head=0;head<10;head++){
   const tick=17+head-f*7;e.render(tick*step);
   assert.equal(e.pixel(f,head),0x9bffb9,'Matrix head moves left to right');
   for(let p=head+1;p<10;p++)assert.equal(e.pixel(f,p),0,'Black ahead of head');
   if(head>0){const trail=e.pixel(f,head-1);assert(trail>0 && (trail>>16)===0,'Green trail');}
   assert.equal(e.pixel(f,10),0,'Exactly ten LEDs');
  }
 }
 e.render(0);assert.notEqual(e.pixel(0,0),e.pixel(1,0),'Independent row phases');
 console.log('PASS: disabled floors in all six modes; irregular Halloween flashes/blackouts across all floors; Christmas red/green and twinkles; horizontal Matrix on 3x10 LEDs.');
})().catch(e=>{console.error(e);process.exitCode=1});
