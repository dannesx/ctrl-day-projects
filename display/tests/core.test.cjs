const { test } = require("node:test");
const assert = require("node:assert/strict");
const O = require("../core.js");
test("bitmap MSB first preserves corners and yellow/blue boundary", () => {
  const p = Array(8192).fill(0);
  for (const i of [0, 7, 127, 15 * 128, 16 * 128, 8191]) p[i] = 1;
  const b = O.pack(p);
  assert.equal(b.length, 1024);
  assert.equal(b[0], 129);
  assert.equal(b[15], 1);
  assert.equal(b[240], 128);
  assert.equal(b[256], 128);
  assert.equal(b[1023], 1);
  const decoded = Array.from(
    { length: 8192 },
    (_, i) => (b[i >> 3] >> (7 - (i & 7))) & 1,
  );
  assert.deepEqual(decoded, p);
});
test("continuous diagonal and vertical strokes include endpoints", () => {
  const p = Array(8192).fill(0);
  O.line(p, 0, 0, 127, 63, 1);
  assert.equal(p[0], 1);
  assert.equal(p[8191], 1);
  assert.equal(
    p.reduce((a, b) => a + b),
    128,
  );
  O.line(p, 0, 0, 0, 63, 1);
  assert.equal(p.filter((v, i) => i % 128 === 0 && v).length, 64);
});
test("flood fill respects border and never wraps rows", () => {
  const p = Array(8192).fill(0);
  O.line(p, 64, 0, 64, 63, 1);
  O.fill(p, 0, 0, 1);
  assert.equal(p[63], 1);
  assert.equal(p[65], 0);
  assert.equal(p[8191], 0);
  assert.equal(
    p.reduce((a, b) => a + b),
    65 * 64,
  );
  O.fill(p, 0, 0, 0);
  assert.equal(
    p.reduce((a, b) => a + b),
    0,
  );
});
test("JSON roundtrip and validation reject malformed imports", () => {
  const p = O.project([O.screen("Tela")]);
  assert.deepEqual(O.validate(JSON.parse(JSON.stringify(p))), p);
  const bad = structuredClone(p);
  bad.screens[0].pixels[10] = 255;
  assert.throws(() => O.validate(bad));
  assert.throws(() =>
    O.validate({ ...p, screens: [p.screens[0], p.screens[0]] }),
  );
  assert.throws(() => O.validate({ ...p, width: 64 }));
});
test("copies are independent", () => {
  const a = O.screen(),
    b = O.screen("Cópia", a.pixels);
  b.pixels[0] = 1;
  assert.equal(a.pixels[0], 0);
  assert.notEqual(a.id, b.id);
});

test("square brush draws continuous thick strokes and clips at edges", () => {
  const p = Array(8192).fill(0);
  O.line(p, 10, 10, 20, 10, 1, 3);
  assert.equal(
    p.reduce((a, b) => a + b),
    13 * 3,
  );
  O.line(p, 10, 10, 20, 10, 0, 3);
  assert.equal(
    p.reduce((a, b) => a + b),
    0,
  );
  O.line(p, 127, 63, 127, 63, 1, 4);
  assert.equal(
    p.reduce((a, b) => a + b),
    4,
  );
  assert.equal(p.length, 8192);
  assert.equal(p[62 * 128], 0);
  assert.equal(p[8191], 1);
});

test("bundle migration preserves edits and is idempotent", () => {
  const original = O.screen("Meu desenho");
  original.pixels[100] = 1;
  const project = O.project([original]);
  const before = structuredClone(original);
  const bundle = require("../screens/jokenpo.json");
  assert.equal(O.addBundleOnce(project, bundle, "jokenpo-completo-v2"), 17);
  assert.deepEqual(project.screens[0], before);
  assert.equal(project.screens.length, 18);
  O.validate(project);
  assert.notEqual(project.screens[1].id, bundle.screens[0].id);
  assert.equal(O.addBundleOnce(project, bundle, "jokenpo-completo-v2"), 0);
  project.screens.pop();
  assert.equal(O.addBundleOnce(project, bundle, "jokenpo-completo-v2"), 0);
  assert.equal(project.screens.length, 17);
});
test("bundle exceeding capacity leaves the project untouched", () => {
  const project = O.project(Array.from({ length: 190 }, () => O.screen()));
  const before = JSON.stringify(project);
  assert.throws(() =>
    O.addBundleOnce(
      project,
      require("../screens/jokenpo.json"),
      "jokenpo-completo-v2",
    ),
  );
  assert.equal(JSON.stringify(project), before);
});
test("positioning preserves shape and outside pixels, without clipping", () => {
  const p = Array(8192).fill(0), area = { x: 10, y: 20, width: 20, height: 10 };
  p[0] = p[21*128+11] = p[21*128+12] = p[22*128+11] = 1;
  const original = [...p];
  assert.equal(O.reposition(p, area, 1, -2, 0), false);
  assert.deepEqual(p, original);
  assert.equal(O.reposition(p, area, 1, 1, 0), true);
  assert.equal(p[21*128+11], 0);
  assert.equal(p[21*128+13], 1);
  assert.equal(p[0], 1);
  O.reposition(p, area, 1, 0, 0, true);
  assert.equal(p[24*128+19], 1);
  assert.equal(p[24*128+20], 1);
  assert.equal(p[25*128+19], 1);
  assert.equal(p.reduce((a,b)=>a+b), 4);
  const centered = [...p];
  O.reposition(p, area, 1, 0, 0, true);
  assert.deepEqual(p, centered);
});
test("centering black text preserves the lit band and empty areas are no-ops", () => {
  const p = Array(8192).fill(0);
  p.fill(1, 0, 2048);
  p[2*128+3] = p[2*128+4] = 0;
  O.reposition(p, {x:0,y:0,width:128,height:16}, 0, 0, 0, true);
  assert.equal(p[7*128+63], 0);
  assert.equal(p[7*128+64], 0);
  assert.equal(p[2*128+3], 1);
  assert.equal(p.slice(0,2048).filter(v=>!v).length, 2);
  assert.equal(O.reposition(p, {x:0,y:16,width:128,height:48}, 1, 0, 0, true), false);
  assert.equal(p.slice(2048).some(Boolean), false);
});
