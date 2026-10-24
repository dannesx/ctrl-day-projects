const fs=require('node:fs'),assert=require('node:assert/strict');
(async()=>{
 const {instance}=await WebAssembly.instantiate(fs.readFileSync(process.argv[2]||'.build/rules.wasm'));
 const e=instance.exports;e.__wasm_call_ctors?.();
 assert.equal(e.remaining(0,0,0),0);assert.equal(e.remaining(1,10000,10000),300000);
 assert.equal(e.remaining(1,10000,309999),1);assert.equal(e.remaining(1,10000,310000),0);
 assert.equal(e.remaining(1,310000,310000),300000);
 assert.equal(e.remaining(1,0xfffffff0,0x10),299968);
 assert.equal(e.reset(0,1,1,1),0);assert.equal(e.reset(2999,1,1,1),0);
 assert.equal(e.reset(2999,0,1,1),0);assert.equal(e.reset(3000,1,1,1),0);
 assert.equal(e.reset(5999,1,1,1),0);assert.equal(e.reset(6000,1,1,1),1);
 assert.equal(e.reset(9500,1,1,0),0);assert.equal(e.reset(10000,1,1,1),0);
 assert.equal(e.reset(13000,1,1,1),0);assert.equal(e.reset(13001,0,0,1),0);
 assert.equal(e.reset(14000,1,1,1),0);assert.equal(e.reset(17000,1,1,1),1);
 console.log('PASS: five-minute validity, retraining, millis rollover; reset at exactly three seconds, cancellation and release lock.');
})().catch(e=>{console.error(e);process.exitCode=1});
