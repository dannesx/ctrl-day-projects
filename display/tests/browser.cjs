const { chromium } = require(process.env.PLAYWRIGHT_PATH || "playwright");
const { pathToFileURL } = require("node:url");
const path = require("node:path");
const assert = require("node:assert/strict");
(async () => {
  const browser = await chromium.launch({
    headless: true,
    ...(process.env.CHROMIUM_PATH
      ? { executablePath: process.env.CHROMIUM_PATH }
      : {}),
  });
  try {
    const page = await browser.newPage({
        viewport: { width: 1440, height: 1000 },
      }),
      errors = [];
    page.on("pageerror", (e) => errors.push(e.message));
    await page.goto(
      pathToFileURL(path.resolve(__dirname, "../index.html")).href,
    );
    assert.equal(await page.locator(".screen-item").count(), 17);
    await page.locator("#new-screen").click();
    await page.locator("#screen-name").fill("Teste real");
    await page.locator("#editor").focus();
    await page.keyboard.press("Space");
    await page.reload();
    assert.equal(await page.locator("#screen-name").inputValue(), "Teste real");
    assert.equal(
      await page.locator("#pixel-count").textContent(),
      "1 LEDs acesos",
    );
    await page.locator("#center-pixels").click();
    assert.equal(await page.evaluate(() => {
      const p = JSON.parse(localStorage.getItem("esp32.oled.studio.v1"));
      return p.screens.find(s => s.id === p.activeId).pixels[31 * 128 + 63];
    }), 1);
    await page.locator('[data-move="1,0"]').click();
    await page.locator("#undo").click();
    await page.locator("#undo").click();
    assert.equal(await page.evaluate(() => {
      const p = JSON.parse(localStorage.getItem("esp32.oled.studio.v1"));
      return p.screens.find(s => s.id === p.activeId).pixels[16 * 128];
    }), 1);
    const pending = page.waitForEvent("download");
    await page.locator("#export-png").click();
    const png = await pending;
    assert.equal(png.suggestedFilename(), "Teste-real.png");
    await png.saveAs("/tmp/oled-browser-export.png");
    await page.screenshot({ path: "/tmp/oled-desktop.png", fullPage: true });
    await page.setViewportSize({ width: 390, height: 844 });
    await page.screenshot({ path: "/tmp/oled-mobile.png", fullPage: true });
    assert.equal(
      await page.evaluate(
        () => document.documentElement.scrollWidth > innerWidth,
      ),
      false,
    );
    assert.deepEqual(errors, []);
    console.log("Browser smoke test passed. Screenshots and PNG in /tmp.");
  } finally {
    await browser.close();
  }
})().catch((error) => {
  console.error(error);
  process.exitCode = 1;
});
