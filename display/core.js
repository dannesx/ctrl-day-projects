/* Shared by the browser and Node tests. Pixels are row-major 0/1 values. */
(function (root) {
  const WIDTH = 128,
    HEIGHT = 64,
    LENGTH = WIDTH * HEIGHT;
  const id = () =>
    globalThis.crypto?.randomUUID?.() ||
    `screen-${Date.now()}-${Math.random().toString(16).slice(2)}`;
  function screen(name = "Nova tela", pixels = Array(LENGTH).fill(0)) {
    return { id: id(), name, pixels: [...pixels] };
  }
  function validate(data) {
    if (
      !data ||
      data.format !== "oled-studio" ||
      data.version !== 1 ||
      data.width !== WIDTH ||
      data.height !== HEIGHT ||
      !Array.isArray(data.screens) ||
      !data.screens.length ||
      data.screens.length > 200
    )
      throw Error(
        "JSON incompatível. Use um projeto OLED 128×64, versão 1, com 1 a 200 telas.",
      );
    const ids = new Set();
    for (const s of data.screens) {
      if (
        !s ||
        typeof s.id !== "string" ||
        !s.id ||
        ids.has(s.id) ||
        typeof s.name !== "string" ||
        !s.name.trim() ||
        s.name.length > 80 ||
        !Array.isArray(s.pixels) ||
        s.pixels.length !== LENGTH ||
        s.pixels.some((p) => p !== 0 && p !== 1)
      )
        throw Error(
          "Tela inválida: confira nome, ID único e os 8.192 LEDs binários.",
        );
      ids.add(s.id);
    }
    return JSON.parse(JSON.stringify(data));
  }
  function project(screens, activeId) {
    return {
      format: "oled-studio",
      version: 1,
      width: WIDTH,
      height: HEIGHT,
      activeId: activeId || screens[0].id,
      screens,
    };
  }
  function addBundleOnce(current, bundle, revision) {
    const applied = Array.isArray(current.appliedBundles)
      ? current.appliedBundles
      : [];
    if (applied.includes(revision)) return 0;
    const incoming = validate(bundle);
    if (current.screens.length + incoming.screens.length > 200)
      throw Error(
        "Não há espaço para a coleção do jokenpo (limite de 200 telas). Exporte um backup e remova telas antes de recarregar.",
      );
    const copies = incoming.screens.map((s) =>
      screen("Jokenpo · " + s.name, s.pixels),
    );
    current.screens.push(...copies);
    current.appliedBundles = [...applied, revision];
    current.activeId = copies[0].id;
    return copies.length;
  }
  function removeOldJokenpoVersions(current, bundle) {
    const names = new Set(bundle.screens.map(s => s.name));
    const key = s => {
      const name = s.name.replace(/^Jokenpo · /, "");
      return names.has(name) && (s.id.startsWith("jokenpo-") || s.name.startsWith("Jokenpo · "))
        ? name : null;
    };
    const latest = new Map();
    for (const s of current.screens) if (key(s)) latest.set(key(s), s);
    const active = current.screens.find(s => s.id === current.activeId);
    const before = current.screens.length;
    current.screens = current.screens.filter(s => !key(s) || latest.get(key(s)) === s);
    if (active && key(active)) current.activeId = latest.get(key(active)).id;
    return before - current.screens.length;
  }
  function line(pixels, x0, y0, x1, y1, value, size = 1) {
    const offset = Math.floor((size - 1) / 2);
    let dx = Math.abs(x1 - x0),
      sx = x0 < x1 ? 1 : -1,
      dy = -Math.abs(y1 - y0),
      sy = y0 < y1 ? 1 : -1,
      err = dx + dy;
    for (;;) {
      for (
        let y = Math.max(0, y0 - offset);
        y < Math.min(HEIGHT, y0 - offset + size);
        y++
      )
        for (
          let x = Math.max(0, x0 - offset);
          x < Math.min(WIDTH, x0 - offset + size);
          x++
        )
          pixels[y * WIDTH + x] = value;
      if (x0 === x1 && y0 === y1) break;
      const e = 2 * err;
      if (e >= dy) {
        err += dy;
        x0 += sx;
      }
      if (e <= dx) {
        err += dx;
        y0 += sy;
      }
    }
  }
  function fill(pixels, x, y, value) {
    const start = y * WIDTH + x,
      old = pixels[start];
    if (old === value) return;
    const queue = [start];
    pixels[start] = value;
    for (let k = 0; k < queue.length; k++) {
      const i = queue[k],
        col = i % WIDTH;
      const neighbors = [];
      if (col) neighbors.push(i - 1);
      if (col < WIDTH - 1) neighbors.push(i + 1);
      if (i >= WIDTH) neighbors.push(i - WIDTH);
      if (i < LENGTH - WIDTH) neighbors.push(i + WIDTH);
      for (const n of neighbors)
        if (pixels[n] === old) {
          pixels[n] = value;
          queue.push(n);
        }
    }
  }
  // Move only the chosen polarity inside a fixed editing area; never clip LEDs.
  function reposition(pixels, area, ink = 1, dx = 0, dy = 0, center = false) {
    const { x, y, width, height } = area;
    const points = [];
    for (let yy = y; yy < y + height; yy++)
      for (let xx = x; xx < x + width; xx++)
        if (pixels[yy * WIDTH + xx] === ink) points.push([xx, yy]);
    if (!points.length) return false;
    const xs = points.map(p => p[0]), ys = points.map(p => p[1]);
    const minX = Math.min(...xs), maxX = Math.max(...xs);
    const minY = Math.min(...ys), maxY = Math.max(...ys);
    if (center) {
      dx = x + Math.floor((width - (maxX - minX + 1)) / 2) - minX;
      dy = y + Math.floor((height - (maxY - minY + 1)) / 2) - minY;
    }
    if (minX + dx < x || maxX + dx >= x + width ||
        minY + dy < y || maxY + dy >= y + height) return false;
    for (const [xx, yy] of points) pixels[yy * WIDTH + xx] = 1 - ink;
    for (const [xx, yy] of points) pixels[(yy + dy) * WIDTH + xx + dx] = ink;
    return true;
  }
  function pack(pixels) {
    const bytes = new Uint8Array(LENGTH / 8);
    for (let i = 0; i < LENGTH; i++)
      if (pixels[i]) bytes[i >> 3] |= 0x80 >> (i & 7);
    return bytes;
  }
  function code(s) {
    const symbol =
      "oled_" +
      Array.from(s.id, (c) => c.codePointAt(0).toString(16)).join("_");
    const bytes = pack(s.pixels);
    let rows = [];
    for (let i = 0; i < bytes.length; i += 16)
      rows.push(
        "  " +
          Array.from(
            bytes.slice(i, i + 16),
            (b) => "0x" + b.toString(16).padStart(2, "0").toUpperCase(),
          ).join(", ") +
          ",",
      );
    return (
      "#pragma once\n#include <Arduino.h>\n\n// OLED 128x64: MSB first, linhas consecutivas.\n// Incluir depois de declarar o objeto display.\nstatic const uint8_t " +
      symbol +
      "[] PROGMEM = {\n" +
      rows.join("\n") +
      "\n};\n\nstatic void show_" +
      symbol +
      "() {\n  display.clearDisplay();\n  display.drawBitmap(0, 0, " +
      symbol +
      ", 128, 64, SSD1306_WHITE);\n  display.display();\n}\n"
    );
  }
  const api = {
    WIDTH,
    HEIGHT,
    LENGTH,
    id,
    screen,
    project,
    validate,
    addBundleOnce,
    removeOldJokenpoVersions,
    line,
    fill,
    reposition,
    pack,
    code,
  };
  root.OLED = api;
  if (typeof module !== "undefined") module.exports = api;
})(globalThis);
