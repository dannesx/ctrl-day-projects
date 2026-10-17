"use strict";
const $ = (id) => document.getElementById(id);
const KEY = "esp32.oled.studio.v1";
let project,
  blocked = false,
  rawBackup = null;
try {
  const raw = localStorage.getItem(KEY);
  project = raw ? OLED.validate(JSON.parse(raw)) : OLED.validate(OLED_SEEDS);
} catch (error) {
  blocked = true;
  try {
    rawBackup = localStorage.getItem(KEY);
  } catch {}
  project = OLED.validate(OLED_SEEDS);
  $("notice").hidden = false;
  $("notice").textContent =
    "Não foi possível carregar o salvamento local. Seus dados anteriores não serão sobrescritos. Exporte o projeto para guardar seu trabalho. " +
    error.message;
}
if (!blocked) {
  try {
    const count = OLED.addBundleOnce(
      project,
      OLED_SEEDS,
      "jokenpo-bold-v6",
    );
    if (count) {
      $("notice").hidden = false;
      $("notice").textContent =
        `${count} telas do jokenpo recriadas. Seus desenhos anteriores foram preservados. O IP da tela de espera é um exemplo.`;
    }
  } catch (error) {
    $("notice").hidden = false;
    $("notice").textContent = error.message;
  }
}
// One-time cleanup requested by the user; keep the latest copies and custom art.
if (!blocked && project.jokenpoCleanup !== "current-only-v1") {
  const removed = OLED.removeOldJokenpoVersions(project, OLED_SEEDS);
  project.jokenpoCleanup = "current-only-v1";
  if (removed) {
    $("notice").hidden = false;
    $("notice").textContent = `${removed} versões antigas do Jokenpo removidas. Mantida a cópia mais recente de cada tela e seus desenhos próprios.`;
  }
}
// Upgrade only untouched victory headers; preserve all blue-area edits.
if (!blocked && typeof OLED_WIN_HEADER !== "undefined") {
  for (const screen of project.screens) {
    if (OLED_WIN_HEADER.before.every((pixel, i) => screen.pixels[i] === pixel))
      screen.pixels.splice(0, 2048, ...OLED_WIN_HEADER.after);
  }
}
// Correct existing unedited headers in place: same screen IDs and no new bundle.
if (!blocked && typeof OLED_M_HEADERS !== "undefined") {
  for (const screen of project.screens) {
    for (const patch of OLED_M_HEADERS) {
      if (patch.before.every((pixel, i) => screen.pixels[i] === pixel)) {
        screen.pixels.splice(0, 2048, ...patch.after);
        break;
      }
    }
  }
}
// Restore regular text only where status rows still match the bundled bold text.
if (!blocked && typeof OLED_STATUS_TEXT !== "undefined") {
  for (const screen of project.screens) {
    for (const patch of OLED_STATUS_TEXT) {
      if (screen.name.replace(/^Jokenpo · /, "") === patch.name &&
          patch.before.every((pixel, i) => screen.pixels[patch.offset + i] === pixel))
        screen.pixels.splice(patch.offset, patch.after.length, ...patch.after);
    }
  }
}
let active =
  project.screens.find((s) => s.id === project.activeId) || project.screens[0];
let selection = null, selecting = null, selectionScreen = active.id;
let tool = "pencil",
  stroke = null,
  cursor = { x: 0, y: 16 },
  timer = null;
const histories = new Map(),
  editor = $("editor"),
  ctx = editor.getContext("2d");
const history = () => {
  if (!histories.has(active.id))
    histories.set(active.id, { undo: [], redo: [] });
  return histories.get(active.id);
};
function notify(message) {
  $("notice").textContent = message;
  $("notice").hidden = false;
}
function save() {
  clearTimeout(timer);
  timer = null;
  project.activeId = active.id;
  if (blocked) {
    $("save-status").textContent = "Salvamento bloqueado · exporte JSON";
    return;
  }
  try {
    localStorage.setItem(KEY, JSON.stringify(project));
    $("save-status").textContent = "Salvo neste navegador";
    $("retry-save").hidden = true;
  } catch (error) {
    $("save-status").textContent = "Não salvo · exporte JSON";
    $("retry-save").hidden = false;
    notify(
      "O navegador não conseguiu salvar. Exporte o projeto para manter uma cópia e tente salvar novamente.",
    );
  }
}
function scheduleSave() {
  if (blocked) return;
  $("save-status").textContent = "Salvando…";
  clearTimeout(timer);
  timer = setTimeout(save, 250);
}
function drawPixels(canvas, pixels) {
  const context = canvas.getContext("2d");
  context.fillStyle = "#000";
  context.fillRect(0, 0, 128, 64);
  for (let y = 0; y < 64; y++) {
    context.fillStyle = y < 16 ? "#ffe353" : "#39caff";
    for (let x = 0; x < 128; x++)
      if (pixels[y * 128 + x]) context.fillRect(x, y, 1, 1);
  }
}
function moveArea() {
  const mode = $("move-area").value;
  if (mode === "selection" && selection) return selection;
  if (mode === "yellow") return { x: 0, y: 0, width: 128, height: 16 };
  if (mode === "blue") return { x: 0, y: 16, width: 128, height: 48 };
  return { x: 0, y: 0, width: 128, height: 64 };
}
function updateSelection(end) {
  selection = { x: Math.min(selecting.x, end.x), y: Math.min(selecting.y, end.y),
    width: Math.abs(selecting.x - end.x) + 1, height: Math.abs(selecting.y - end.y) + 1 };
  $("move-area").querySelector('[value="selection"]').disabled = false;
  $("move-area").value = "selection";
  renderCanvas();
}
function reposition(dx = 0, dy = 0, center = false) {
  let moved;
  edit(p => { moved = OLED.reposition(p, moveArea(), Number($("move-ink").value), dx, dy, center); });
  $("selection-status").textContent = moved
    ? (center ? "Pixels centralizados na área." : "Pixels deslocados em 1 px.")
    : "Sem pixels para mover ou limite da área atingido. Nenhum pixel foi cortado.";
}
for (const b of document.querySelectorAll("[data-move]"))
  b.onclick = () => reposition(...b.dataset.move.split(",").map(Number));
$("center-pixels").onclick = () => reposition(0, 0, true);
$("move-area").onchange = () => { selecting = null; renderCanvas(); };
function renderCanvas() {
  if (selectionScreen !== active.id) {
    selection = selecting = null;
    selectionScreen = active.id;
    $("move-area").value = "all";
    $("move-area").querySelector('[value="selection"]').disabled = true;
  }
  const area = moveArea();
  $("selection-status").textContent = `${$("move-area").selectedOptions[0].textContent} · ${area.width} × ${area.height} px · x ${area.x}, y ${area.y}`;
  const z = Number($("zoom").value),
    grid = $("grid").checked;
  editor.width = 128 * z;
  editor.height = 64 * z;
  ctx.fillStyle = "#030505";
  ctx.fillRect(0, 0, editor.width, editor.height);
  for (let y = 0; y < 64; y++)
    for (let x = 0; x < 128; x++) {
      ctx.fillStyle = active.pixels[y * 128 + x]
        ? y < 16
          ? "#ffe353"
          : "#39caff"
        : grid
          ? y < 16
            ? "#292614"
            : "#10242a"
          : "#000";
      ctx.fillRect(x * z, y * z, z - (grid ? 1 : 0), z - (grid ? 1 : 0));
    }
  // Editor-only overlay: boundaries between columns 63/64 and rows 31/32.
  if ($("quadrants").checked) {
    const centerX = 64 * z,
      centerY = 32 * z;
    ctx.fillStyle = "#000";
    ctx.fillRect(centerX - 1, 0, 3, editor.height);
    ctx.fillRect(0, centerY - 1, editor.width, 3);
    ctx.fillStyle = "#f5a7e3";
    for (let y = 0; y < editor.height; y += 10)
      ctx.fillRect(centerX, y, 1, Math.min(6, editor.height - y));
    for (let x = 0; x < editor.width; x += 10)
      ctx.fillRect(x, centerY, Math.min(6, editor.width - x), 1);
  }
  if ($("move-area").value !== "all") {
    ctx.strokeStyle = "#fff";
    ctx.lineWidth = 1;
    ctx.strokeRect(area.x * z + 0.5, area.y * z + 0.5, area.width * z - 1, area.height * z - 1);
  }
  if (document.activeElement === editor && tool !== "select") {
    const size = tool === "fill" ? 1 : Number($("brush-size").value);
    const offset = Math.floor((size - 1) / 2);
    ctx.strokeStyle = "#fff";
    ctx.lineWidth = 1;
    ctx.strokeRect(
      (cursor.x - offset) * z + 0.5,
      (cursor.y - offset) * z + 0.5,
      size * z - 1,
      size * z - 1,
    );
  }
  drawPixels($("preview"), active.pixels);
  $("pixel-count").textContent =
    active.pixels.reduce((a, b) => a + b, 0).toLocaleString("pt-BR") +
    " LEDs acesos";
}
function renderList() {
  const list = $("screen-list");
  list.replaceChildren();
  for (const s of project.screens) {
    const b = document.createElement("button");
    b.className = "screen-item";
    b.setAttribute("aria-current", String(s === active));
    b.title = s.name;
    const c = document.createElement("canvas");
    c.width = 128;
    c.height = 64;
    c.setAttribute("aria-hidden", "true");
    drawPixels(c, s.pixels);
    const label = document.createElement("span");
    label.textContent = s.name;
    b.append(c, label);
    b.onclick = () => {
      finishStroke();
      commitName();
      active = s;
      render();
      save();
    };
    list.append(b);
  }
}
function render() {
  renderCanvas();
  renderList();
  $("screen-name").value = active.name;
  $("code").value = OLED.code(active);
  buttons();
}
function buttons() {
  $("undo").disabled = !history().undo.length;
  $("redo").disabled = !history().redo.length;
  $("delete-screen").disabled = project.screens.length <= 1;
}
function record(before) {
  if (before.every((v, i) => v === active.pixels[i])) return;
  const h = history();
  h.undo.push(before);
  if (h.undo.length > 60) h.undo.shift();
  h.redo = [];
  renderCanvas();
  renderList();
  $("code").value = OLED.code(active);
  buttons();
  save();
}
function edit(fn) {
  finishStroke();
  const before = [...active.pixels];
  fn(active.pixels);
  record(before);
}
function point(e) {
  const r = editor.getBoundingClientRect();
  return {
    x: Math.max(
      0,
      Math.min(127, Math.floor(((e.clientX - r.left) / r.width) * 128)),
    ),
    y: Math.max(
      0,
      Math.min(63, Math.floor(((e.clientY - r.top) / r.height) * 64)),
    ),
  };
}
function finishStroke() {
  if (!stroke) return;
  const before = stroke.before;
  stroke = null;
  record(before);
}
function moveStroke(end, straight) {
  if (straight) {
    if (!stroke.straight)
      stroke.straight = { anchor: stroke.last, pixels: [...active.pixels] };
    active.pixels = [...stroke.straight.pixels];
  } else {
    stroke.straight = null;
  }
  const start = stroke.straight?.anchor || stroke.last;
  OLED.line(
    active.pixels,
    start.x,
    start.y,
    end.x,
    end.y,
    stroke.value,
    stroke.size,
  );
  stroke.last = end;
  renderCanvas();
}
editor.oncontextmenu = (e) => e.preventDefault();
editor.onpointerdown = (e) => {
  if ((e.button !== 0 && e.button !== 2) || stroke) return;
  e.preventDefault();
  editor.focus({ preventScroll: true });
  editor.setPointerCapture(e.pointerId);
  cursor = point(e);
  if (tool === "select" && e.button === 0) {
    selecting = { ...cursor, id: e.pointerId };
    updateSelection(cursor);
    return;
  }
  const value = e.button === 2 || tool === "eraser" ? 0 : 1;
  if (tool === "fill") {
    edit((p) => OLED.fill(p, cursor.x, cursor.y, value));
    return;
  }
  stroke = {
    before: [...active.pixels],
    last: cursor,
    value,
    id: e.pointerId,
    size: Number($("brush-size").value),
  };
  moveStroke(cursor, e.shiftKey);
};
editor.onpointermove = (e) => {
  cursor = point(e);
  $("coordinates").textContent = `x ${cursor.x} · y ${cursor.y}`;
  if (selecting && selecting.id === e.pointerId) {
    updateSelection(cursor);
    return;
  }
  if (stroke && stroke.id === e.pointerId) {
    moveStroke(cursor, e.shiftKey);
  } else if (document.activeElement === editor) {
    renderCanvas();
  }
};
editor.onpointerup = (e) => {
  if (selecting && selecting.id === e.pointerId) {
    updateSelection(point(e));
    selecting = null;
    return;
  }
  if (stroke && stroke.id === e.pointerId) {
    cursor = point(e);
    moveStroke(cursor, e.shiftKey);
    finishStroke();
  }
};
editor.onpointercancel = editor.onlostpointercapture = () => {
  selecting = null;
  finishStroke();
};
editor.onfocus = renderCanvas;
editor.onblur = () => {
  selecting = null;
  finishStroke();
  renderCanvas();
};
editor.onkeydown = (e) => {
  if (e.key === "Shift" && stroke) {
    moveStroke(cursor, true);
    return;
  }
  const moves = {
    ArrowLeft: [-1, 0],
    ArrowRight: [1, 0],
    ArrowUp: [0, -1],
    ArrowDown: [0, 1],
  };
  if (e.key === "Escape") {
    selecting = null;
    $("move-area").value = "all";
    renderCanvas();
    return;
  }
  if (moves[e.key]) {
    e.preventDefault();
    const [x, y] = moves[e.key];
    if (tool === "select" && !e.shiftKey && !selecting) { reposition(x, y); return; }
    if (tool === "select" && !selecting) selecting = { ...cursor };
    cursor = {
      x: Math.max(0, Math.min(127, cursor.x + x)),
      y: Math.max(0, Math.min(63, cursor.y + y)),
    };
    $("coordinates").textContent = `x ${cursor.x} · y ${cursor.y}`;
    if (tool === "select" && selecting) updateSelection(cursor);
    renderCanvas();
  } else if (tool === "select" && (e.key === " " || e.key === "Enter")) {
    e.preventDefault();
    if (selecting) { updateSelection(cursor); selecting = null; }
    else { selecting = { ...cursor }; updateSelection(cursor); }
  } else if (e.key === " " || e.key === "Enter") {
    e.preventDefault();
    edit((p) => {
      const i = cursor.y * 128 + cursor.x;
      if (e.key === " ") p[i] = 1 - p[i];
      else if (tool === "fill") OLED.fill(p, cursor.x, cursor.y, 1);
      else
        OLED.line(
          p,
          cursor.x,
          cursor.y,
          cursor.x,
          cursor.y,
          tool === "eraser" ? 0 : 1,
          Number($("brush-size").value),
        );
    });
  }
};
editor.onkeyup = (e) => {
  if (e.key === "Shift" && stroke) moveStroke(cursor, false);
};
for (const b of document.querySelectorAll("[data-tool]"))
  b.onclick = () => {
    finishStroke();
    selecting = null;
    tool = b.dataset.tool;
    $("brush-size").disabled = tool === "fill" || tool === "select";
    for (const item of document.querySelectorAll("[data-tool]"))
      item.setAttribute("aria-pressed", String(item === b));
    renderCanvas();
  };
$("brush-size").oninput = () => {
  const size = Number($("brush-size").value);
  $("brush-size-value").textContent = `${size} × ${size} px`;
  renderCanvas();
};
$("grid").onchange = renderCanvas;
$("quadrants").onchange = renderCanvas;
$("zoom").onchange = renderCanvas;
const mainEditor = $("main-editor");
let expanded = false,
  previousZoom = null;
// Keep confirmation dialogs inside the element promoted to native fullscreen.
mainEditor.append($("confirm-dialog"));
function fitFullscreen() {
  if (!expanded) return;
  const area = mainEditor.querySelector(".canvas-scroll");
  const style = getComputedStyle(area);
  const width =
    area.clientWidth -
    parseFloat(style.paddingLeft) -
    parseFloat(style.paddingRight);
  const height =
    area.clientHeight -
    parseFloat(style.paddingTop) -
    parseFloat(style.paddingBottom);
  const fit = Math.max(1, Math.floor(Math.min(width / 128, height / 64)));
  const sizes = Array.from($("zoom").options, (option) => Number(option.value));
  $("zoom").value = String(Math.max(...sizes.filter((size) => size <= fit)));
  renderCanvas();
}
function setExpanded(value) {
  if (value === expanded) return;
  finishStroke();
  expanded = value;
  mainEditor.classList.toggle("expanded", value);
  document.body.classList.toggle("editor-expanded", value);
  $("fullscreen").textContent = value
    ? "Sair da tela cheia · Esc"
    : "Tela cheia";
  $("fullscreen").setAttribute("aria-pressed", String(value));
  for (const element of document.querySelectorAll(
    ".topbar, .library, .inspector, #notice",
  ))
    element.inert = value;
  if (value) {
    previousZoom = $("zoom").value;
    requestAnimationFrame(fitFullscreen);
  } else {
    $("zoom").value = previousZoom;
    renderCanvas();
    $("fullscreen").focus();
  }
}
$("fullscreen").onclick = async () => {
  if (expanded) {
    if (document.fullscreenElement === mainEditor) {
      try {
        await document.exitFullscreen();
      } catch {
        return;
      }
    }
    setExpanded(false);
  } else {
    setExpanded(true);
    // Browsers without Fullscreen API still get a full-window editor.
    try {
      await mainEditor.requestFullscreen?.();
    } catch {
      /* Keep full-window mode. */
    }
    requestAnimationFrame(fitFullscreen);
  }
};
document.addEventListener("fullscreenchange", () => {
  if (document.fullscreenElement === mainEditor) fitFullscreen();
  else setExpanded(false);
});
document.addEventListener("keydown", (e) => {
  if (
    e.key === "Escape" &&
    expanded &&
    !document.fullscreenElement &&
    !$("confirm-dialog").open
  ) {
    e.preventDefault();
    setExpanded(false);
  }
});
window.addEventListener("resize", fitFullscreen);
function undo(redo = false) {
  finishStroke();
  const h = history(),
    from = redo ? h.redo : h.undo,
    to = redo ? h.undo : h.redo;
  if (!from.length) return;
  to.push([...active.pixels]);
  active.pixels = from.pop();
  render();
  save();
}
$("undo").onclick = () => undo();
$("redo").onclick = () => undo(true);
document.addEventListener("keydown", (e) => {
  if (
    /INPUT|TEXTAREA|SELECT/.test(e.target.tagName) ||
    $("confirm-dialog").open
  )
    return;
  if ((e.ctrlKey || e.metaKey) && e.key.toLowerCase() === "z") {
    e.preventDefault();
    undo(e.shiftKey);
  } else if (!e.ctrlKey && !e.metaKey && !e.altKey) {
    const t = { b: "pencil", e: "eraser", f: "fill", s: "select" }[e.key.toLowerCase()];
    if (t) {
      e.preventDefault();
      document.querySelector(`[data-tool="${t}"]`).click();
    }
  }
});
function commitName() {
  const name = $("screen-name").value.trim();
  if (name) active.name = name;
  else $("screen-name").value = active.name;
  renderList();
}
$("screen-name").oninput = () => {
  const name = $("screen-name").value.trim();
  if (name) {
    active.name = name;
    renderList();
    scheduleSave();
  }
};
$("screen-name").onblur = () => {
  commitName();
  save();
};
function add(s) {
  if (project.screens.length >= 200) {
    notify(
      "Limite de 200 telas atingido. Exporte este projeto antes de criar outra coleção.",
    );
    return;
  }
  project.screens.push(s);
  active = s;
  render();
  save();
  $("screen-name").focus();
  $("screen-name").select();
}
$("new-screen").onclick = () => add(OLED.screen());
$("duplicate").onclick = () =>
  add(OLED.screen((active.name + " — cópia").slice(0, 80), active.pixels));
function confirmAction(title, body, action) {
  $("confirm-title").textContent = title;
  $("confirm-body").textContent = body;
  const d = $("confirm-dialog");
  d.returnValue = "cancel";
  d.onclose = () => {
    if (d.returnValue === "confirm") action();
  };
  d.showModal();
}
$("delete-screen").onclick = () =>
  confirmAction(
    "Excluir esta tela?",
    `“${active.name}” será removida do navegador. Exporte o JSON se quiser guardar uma cópia.`,
    () => {
      histories.delete(active.id);
      project.screens = project.screens.filter((s) => s !== active);
      active = project.screens[0];
      render();
      save();
    },
  );
$("clear").onclick = () =>
  confirmAction(
    "Apagar todos os LEDs?",
    "Você pode recuperar o desenho com Desfazer.",
    () => edit((p) => p.fill(0)),
  );
$("invert").onclick = () => edit((p) => p.forEach((v, i) => (p[i] = 1 - v)));
function slug(name) {
  return (
    name
      .normalize("NFD")
      .replace(/[\u0300-\u036f]/g, "")
      .replace(/[^a-zA-Z0-9_-]+/g, "-")
      .replace(/^-|-$/g, "")
      .slice(0, 80) || "tela"
  );
}
function download(blob, name) {
  const a = document.createElement("a"),
    url = URL.createObjectURL(blob);
  a.href = url;
  a.download = name;
  document.body.append(a);
  a.click();
  a.remove();
  setTimeout(() => URL.revokeObjectURL(url), 1000);
}
function jsonDownload(value, name) {
  download(
    new Blob([JSON.stringify(value, null, 2) + "\n"], {
      type: "application/json",
    }),
    name,
  );
}
$("export-project").onclick = () => {
  finishStroke();
  commitName();
  jsonDownload(project, "oled-projeto.json");
  if (rawBackup !== null) {
    download(
      new Blob([rawBackup], { type: "application/json" }),
      "oled-salvamento-original.json",
    );
  }
};
$("export-screen").onclick = () =>
  jsonDownload(OLED.project([active], active.id), slug(active.name) + ".json");
$("export-code").onclick = () =>
  download(
    new Blob([OLED.code(active)], { type: "text/plain" }),
    slug(active.name) + ".h",
  );
$("export-png").onclick = () => {
  const c = document.createElement("canvas");
  c.width = 128;
  c.height = 64;
  drawPixels(c, active.pixels);
  c.toBlob((blob) => {
    if (blob) download(blob, slug(active.name) + ".png");
    else notify("Não foi possível gerar o PNG. Tente novamente.");
  }, "image/png");
};
$("copy-code").onclick = async () => {
  try {
    await navigator.clipboard.writeText(OLED.code(active));
    notify("Código copiado. Cole depois de declarar o objeto display.");
  } catch {
    $("code").focus();
    $("code").select();
    notify("Selecionei o código. Use Ctrl+C para copiar.");
  }
};
$("retry-save").onclick = save;
$("import-project").onclick = () => $("file-input").click();
$("file-input").onchange = async (e) => {
  const file = e.target.files[0];
  if (!file) return;
  try {
    if (file.size > 20 * 1024 * 1024)
      throw Error("Arquivo muito grande: limite de 20 MB.");
    const imported = OLED.validate(JSON.parse(await file.text()));
    if (project.screens.length + imported.screens.length > 200)
      throw Error("A importação ultrapassaria o limite de 200 telas.");
    const screens = imported.screens.map((s) => OLED.screen(s.name, s.pixels));
    project.screens.push(...screens);
    active = screens[0];
    render();
    save();
    notify(
      `${screens.length} tela(s) importada(s) como novas cópias. As telas anteriores foram preservadas.`,
    );
  } catch (error) {
    notify("Importação não realizada. " + error.message);
  } finally {
    e.target.value = "";
  }
};
window.addEventListener("storage", (e) => {
  if (e.key === KEY) {
    blocked = true;
    $("save-status").textContent = "Outra aba alterou o projeto";
    notify(
      "Outra aba alterou o salvamento. Exporte suas alterações e recarregue esta página antes de continuar, para evitar sobrescrever telas.",
    );
  }
});
window.addEventListener("pagehide", () => {
  finishStroke();
  save();
});
render();
save();
