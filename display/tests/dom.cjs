const assert = require("node:assert/strict"),
  fs = require("node:fs");
const { JSDOM } = require(process.env.JSDOM_PATH || "jsdom");
const root = require("node:path").resolve(__dirname, "..") + "/";
function setup(saved) {
  const dom = new JSDOM(fs.readFileSync(root + "index.html", "utf8"), {
    url: "http://localhost:8080",
    runScripts: "outside-only",
    pretendToBeVisual: true,
  });
  const w = dom.window;
  w.HTMLCanvasElement.prototype.getContext = () => ({
    fillRect() {},
    strokeRect() {},
  });
  w.HTMLCanvasElement.prototype.setPointerCapture = () => {};
  w.HTMLDialogElement.prototype.showModal = function () {
    this.open = true;
  };
  w.HTMLDialogElement.prototype.close = function (result) {
    this.returnValue = result;
    this.open = false;
    this.onclose?.();
  };
  if (saved) w.localStorage.setItem("esp32.oled.studio.v1", saved);
  w.eval(
    ["core.js", "seeds.js", "app.js"]
      .map((name) => fs.readFileSync(root + name, "utf8"))
      .join("\n"),
  );
  return w;
}
(async () => {
  let w = setup();
  const $ = (id) => w.document.getElementById(id),
    read = () => JSON.parse(w.localStorage.getItem("esp32.oled.studio.v1")),
    last = () => read().screens.at(-1);
  assert.equal(read().screens.length, 17);
  $("new-screen").click();
  assert.equal(read().screens.length, 18);
  $("screen-name").value = "Minha tela";
  $("screen-name").dispatchEvent(new w.Event("input"));
  $("screen-name").dispatchEvent(new w.Event("blur"));
  assert.equal(last().name, "Minha tela");
  $("editor").dispatchEvent(new w.KeyboardEvent("keydown", { key: " " }));
  assert.equal(last().pixels[16 * 128], 1);
  $("undo").click();
  assert.equal(last().pixels[16 * 128], 0);
  $("redo").click();
  assert.equal(last().pixels[16 * 128], 1);
  $("duplicate").click();
  assert.equal(read().screens.length, 19);
  $("invert").click();
  assert.equal(last().pixels[16 * 128], 0);
  assert.equal(read().screens.at(-2).pixels[16 * 128], 1);
  $("clear").click();
  $("confirm-dialog").close("confirm");
  assert.equal(
    last().pixels.reduce((a, b) => a + b),
    0,
  );
  $("undo").click();
  assert.equal(
    last().pixels.reduce((a, b) => a + b),
    8191,
  );
  const saved = read();
  const fresh = setup(JSON.stringify(saved));
  assert.equal(
    JSON.parse(fresh.localStorage.getItem("esp32.oled.studio.v1")).activeId,
    saved.activeId,
  );
  fresh.close();
  $("file-input").files;
  Object.defineProperty($("file-input"), "files", {
    value: [{ size: 100, text: async () => JSON.stringify({ format: "bad" }) }],
    configurable: true,
  });
  await $("file-input").onchange({ target: $("file-input") });
  assert.match($("notice").textContent, /Importação não realizada/);
  assert.equal(read().screens.length, 19);
  const imported = { ...saved, screens: [saved.screens[0]] };
  Object.defineProperty($("file-input"), "files", {
    value: [{ size: 100, text: async () => JSON.stringify(imported) }],
    configurable: true,
  });
  await $("file-input").onchange({ target: $("file-input") });
  assert.equal(read().screens.length, 20);
  assert.notEqual(last().id, imported.screens[0].id);
  $("delete-screen").click();
  $("confirm-dialog").close("confirm");
  assert.equal(read().screens.length, 19);
  const original = w.localStorage.getItem("esp32.oled.studio.v1");
  w.dispatchEvent(
    new w.StorageEvent("storage", { key: "esp32.oled.studio.v1" }),
  );
  $("invert").click();
  assert.equal(w.localStorage.getItem("esp32.oled.studio.v1"), original);
  w.close();
  const corrupt = setup("{broken");
  assert.equal(corrupt.localStorage.getItem("esp32.oled.studio.v1"), "{broken");
  assert.match(
    corrupt.document.getElementById("save-status").textContent,
    /bloqueado/,
  );
  corrupt.close();
  console.log(
    "DOM interactions PASS: create, rename, edit, undo/redo, duplicate, clear, reload, reject/merge import, delete, multi-tab conflict, corrupt-data preservation",
  );
})();

// Shift previews replace the previous segment instead of leaving a fan of lines.
{
  const w = setup();
  const get = (id) => w.document.getElementById(id);
  get("new-screen").click();
  const canvas = get("editor");
  canvas.getBoundingClientRect = () => ({
    left: 0,
    top: 0,
    width: 128,
    height: 64,
  });
  get("brush-size").value = "3";
  const event = (x, y) => ({
    button: 0,
    pointerId: 1,
    clientX: x + 0.5,
    clientY: y + 0.5,
    shiftKey: true,
    preventDefault() {},
  });
  const pixels = () =>
    JSON.parse(w.localStorage.getItem("esp32.oled.studio.v1")).screens.at(-1)
      .pixels;
  canvas.onpointerdown(event(10, 10));
  canvas.onpointermove(event(20, 20));
  canvas.onpointermove(event(20, 10));
  canvas.onpointerup(event(20, 10));
  assert.equal(
    pixels().reduce((a, b) => a + b),
    39,
  );
  assert.equal(pixels()[20 * 128 + 20], 0);
  get("undo").click();
  assert.equal(
    pixels().reduce((a, b) => a + b),
    0,
  );
  get("redo").click();
  assert.equal(
    pixels().reduce((a, b) => a + b),
    39,
  );
  w.document.querySelector('[data-tool="eraser"]').click();
  canvas.onpointerdown(event(10, 10));
  canvas.onpointermove(event(20, 10));
  canvas.onpointermove(event(10, 20));
  canvas.onpointerup(event(10, 20));
  assert.equal(pixels()[10 * 128 + 20], 1);
  assert.equal(pixels()[10 * 128 + 10], 0);
  get("undo").click();
  assert.equal(
    pixels().reduce((a, b) => a + b),
    39,
  );
  w.close();
  console.log(
    "Shift lines PASS: live replacement, brush width, eraser restoration, single undo/redo",
  );
}

(async () => {
  const w = setup();
  const get = (id) => w.document.getElementById(id);
  const before = w.localStorage.getItem("esp32.oled.studio.v1");
  const zoom = get("zoom").value;
  await get("fullscreen").onclick();
  assert.equal(get("main-editor").classList.contains("expanded"), true);
  assert.equal(get("fullscreen").getAttribute("aria-pressed"), "true");
  assert.equal(w.document.querySelector(".library").inert, true);
  assert.equal(get("confirm-dialog").parentElement, get("main-editor"));
  w.document.dispatchEvent(new w.KeyboardEvent("keydown", { key: "Escape" }));
  assert.equal(get("main-editor").classList.contains("expanded"), false);
  assert.equal(get("zoom").value, zoom);
  assert.equal(w.document.querySelector(".library").inert, false);
  // Emulate native browser entry and exit; DOM tests cannot exercise browser chrome.
  Object.defineProperty(w.document, "fullscreenElement", {
    value: null,
    writable: true,
  });
  get("main-editor").requestFullscreen = async () => {
    w.document.fullscreenElement = get("main-editor");
    w.document.dispatchEvent(new w.Event("fullscreenchange"));
  };
  w.document.exitFullscreen = async () => {
    w.document.fullscreenElement = null;
    w.document.dispatchEvent(new w.Event("fullscreenchange"));
  };
  await get("fullscreen").onclick();
  assert.equal(w.document.fullscreenElement, get("main-editor"));
  await get("fullscreen").onclick();
  assert.equal(get("fullscreen").getAttribute("aria-pressed"), "false");
  assert.equal(w.localStorage.getItem("esp32.oled.studio.v1"), before);
  w.close();
  console.log(
    "Fullscreen DOM PASS: fallback/Escape, native events, zoom restoration, unchanged pixels",
  );
})().catch((error) => {
  console.error(error);
  process.exitCode = 1;
});

{
  const legacy = JSON.parse(
    fs.readFileSync(root + "screens/jokenpo.json", "utf8"),
  );
  delete legacy.appliedBundles;
  legacy.screens = [legacy.screens[0]];
  legacy.screens[0].name = "Desenho que editei";
  legacy.screens[0].pixels[0] = 1;
  const before = JSON.stringify(legacy.screens[0]);
  const w = setup(JSON.stringify(legacy));
  const saved = w.localStorage.getItem("esp32.oled.studio.v1");
  const migrated = JSON.parse(saved);
  assert.equal(migrated.screens.length, 18);
  assert.equal(JSON.stringify(migrated.screens[0]), before);
  assert.match(w.document.getElementById("notice").textContent, /17 telas/);
  assert.equal(w.document.getElementById("screen-name").value, "Jokenpo · JO");
  w.close();
  const again = setup(saved);
  assert.equal(
    JSON.parse(again.localStorage.getItem("esp32.oled.studio.v1")).screens
      .length,
    18,
  );
  again.close();
  console.log(
    "Jokenpo migration DOM PASS: existing saved edits preserved, 17 screens added once and persisted",
  );
}

{
  const patch = JSON.parse(fs.readFileSync(root+'seeds.js','utf8').match(/const OLED_WIN_HEADER = (.*);/)[1]);
  const project = JSON.parse(fs.readFileSync(root+'screens/jokenpo.json','utf8'));
  const s = project.screens[5];
  s.pixels.splice(0,2048,...patch.before);
  s.pixels[4000] = 1-s.pixels[4000];
  const blue = s.pixels.slice(2048);
  const w = setup(JSON.stringify(project));
  const saved = JSON.parse(w.localStorage.getItem('esp32.oled.studio.v1'));
  assert.deepEqual(saved.screens[5].pixels.slice(0,2048),patch.after);
  assert.deepEqual(saved.screens[5].pixels.slice(2048),blue);
  w.close();
  console.log('Bold header migration PASS: victory header upgraded, blue-area edits preserved');
}
// Selecting is non-destructive; transformations use the same history and autosave.
{
  const w = setup(), get = id => w.document.getElementById(id);
  get("new-screen").click();
  const canvas = get("editor");
  canvas.getBoundingClientRect = () => ({ left: 0, top: 0, width: 128, height: 64 });
  const event = (x,y) => ({button:0,pointerId:1,clientX:x+.5,clientY:y+.5,preventDefault(){}});
  const saved = () => JSON.parse(w.localStorage.getItem("esp32.oled.studio.v1"));
  const pixels = () => saved().screens.at(-1).pixels;
  canvas.onpointerdown(event(11,21)); canvas.onpointerup(event(11,21));
  canvas.onpointerdown(event(1,1)); canvas.onpointerup(event(1,1));
  const original = pixels();
  w.document.querySelector('[data-tool="select"]').click();
  canvas.onpointerdown(event(10,20)); canvas.onpointermove(event(29,29)); canvas.onpointerup(event(29,29));
  assert.deepEqual(pixels(), original);
  assert.equal(get("move-area").value, "selection");
  get("center-pixels").click();
  assert.equal(pixels()[24*128+19], 1);
  assert.equal(pixels()[1*128+1], 1);
  assert.equal(pixels()[21*128+11], 0);
  get("undo").click(); assert.deepEqual(pixels(), original);
  get("redo").click(); assert.equal(pixels()[24*128+19], 1);
  canvas.dispatchEvent(new w.KeyboardEvent("keydown", { key:"ArrowRight" }));
  assert.equal(pixels()[24*128+20], 1);
  assert.equal(pixels().reduce((a,b)=>a+b), 2);
  const reopened = setup(JSON.stringify(saved()));
  assert.equal(JSON.parse(reopened.localStorage.getItem("esp32.oled.studio.v1")).screens.at(-1).pixels[24*128+20],1);
  reopened.close();
  get("new-screen").click();
  assert.equal(get("move-area").value, "all");
  assert.equal(get("move-area").querySelector('[value="selection"]').disabled, true);
  get("center-pixels").click(); assert.equal(get("undo").disabled, true);
  w.close();
  console.log("Position DOM PASS: selection, center, outside preservation, undo/redo, keyboard nudge, reload, screen reset, empty no-op");
}
{
  const p = JSON.parse(fs.readFileSync(root + "screens/jokenpo.json", "utf8"));
  const originals = p.screens;
  const latest = originals.map((s,i) => ({...s,id:`copy-${i}`,name:`Jokenpo · ${s.name}`,pixels:[...s.pixels]}));
  latest[0].pixels[0] = 1;
  p.screens = [...originals, ...latest, {id:"custom",name:"Meu desenho",pixels:Array(8192).fill(0)}];
  p.activeId = originals[0].id;
  const w = setup(JSON.stringify(p));
  const saved = w.localStorage.getItem("esp32.oled.studio.v1"), cleaned = JSON.parse(saved);
  assert.equal(cleaned.screens.length,18);
  assert.equal(cleaned.activeId,"copy-0");
  assert.equal(cleaned.screens[0].pixels[0],1);
  assert.equal(cleaned.screens.at(-1).name,"Meu desenho");
  w.close();
  const again=setup(saved);
  assert.equal(JSON.parse(again.localStorage.getItem("esp32.oled.studio.v1")).screens.length,18);
  again.close();
  console.log("Cleanup DOM PASS: older copies removed once, latest edits/custom art/active screen preserved");
}
{
  const source = fs.readFileSync(root+'seeds.js','utf8');
  const m = JSON.parse(source.match(/const OLED_M_HEADERS = (.*);/)[1]);
  const status = JSON.parse(source.match(/const OLED_STATUS_TEXT = (.*);/)[1]);
  const p = JSON.parse(fs.readFileSync(root+'screens/jokenpo.json','utf8'));
  const expected = JSON.parse(JSON.stringify(p));
  for (const s of p.screens) {
    for (const patch of m)
      if (patch.after.every((v,i)=>s.pixels[i]===v)) s.pixels.splice(0,2048,...patch.before);
    for (const patch of status)
      if (s.name===patch.name) s.pixels.splice(patch.offset,patch.before.length,...patch.before);
  }
  p.screens[3].pixels[4000] = 1;
  expected.screens[3].pixels[4000] = 1;
  const w = setup(JSON.stringify(p));
  const saved = JSON.parse(w.localStorage.getItem('esp32.oled.studio.v1'));
  assert.equal(saved.screens.length,17);
  assert.deepEqual(saved.screens, expected.screens);
  assert.deepEqual(saved.appliedBundles,p.appliedBundles);
  w.close();
  console.log('Text corrections PASS: wide M, regular status text, same 17 screen IDs, unchanged bundle and custom art');
}
