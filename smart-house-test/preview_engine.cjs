// Line protocol adapter for the exact firmware effects compiled to WebAssembly.
const fs=require('node:fs'),path=require('node:path'),readline=require('node:readline');
(async()=>{
 const {instance}=await WebAssembly.instantiate(fs.readFileSync(path.join(__dirname,'effects.wasm')));
 const e=instance.exports;e.__wasm_call_ctors?.();
 readline.createInterface({input:process.stdin}).on('line',line=>{
  const v=line.trim().split(/\s+/);if(v.length!==11)throw Error('Invalid input');
  e.settings(+v[1],+v[3],+v[4]);
  for(let f=0;f<3;f++)e.floor_settings(f,+v[5+f],parseInt(v[8+f],16));
  e.render(+v[0]);
  console.log(JSON.stringify([10,10,10].map((n,f)=>Array.from({length:n},(_,p)=>'#'+e.pixel(f,p).toString(16).padStart(6,'0')))));
 });
})().catch(e=>{console.error(e);process.exit(1)});
