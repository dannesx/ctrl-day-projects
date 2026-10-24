#include <FastLED.h>

// HSL: matiz 0..359 graus, saturacao 100%, luminosidade 50%.
// Com L=50%, as cores ficam saturadas; L=100% produziria somente branco.
constexpr uint8_t TEST_BRIGHTNESS = 255;
constexpr uint32_t HUE_CYCLE_MS = 8000;
uint16_t currentHue = 0;
String lightingMode = "hsl_cycle";
uint8_t snowSpeed = 3;
uint8_t snowDensity = 4;
CRGB floor3[12], floor2[9], floor1[9];
struct Floor { const char* id; CRGB* strip; uint8_t start, count; CRGB color; bool on; };
Floor floors[] = {
  {"SUPERIOR", floor3, 0, 12, CRGB(255,160,75), true},
  {"CENTRAL", floor2, 0, 9, CRGB(255,160,75), true},
  {"TERREO", floor1, 0, 9, CRGB(255,160,75), true}
};
String controlState();
void controlSetup();
void renderLights(uint32_t now);
#include "ota_support.h"
#include "control.h"

uint32_t lastFrame = 0;

CRGB hslHueColor(uint16_t hue) {
  // Conversao HSL para S=100% e L=50%: croma 255, sem componente branca.
  const uint8_t rising = ((hue % 60) * 255 + 30) / 60;
  const uint8_t falling = 255 - rising;
  switch ((hue / 60) % 6) {
    case 0: return CRGB(255, rising, 0);
    case 1: return CRGB(falling, 255, 0);
    case 2: return CRGB(0, 255, rising);
    case 3: return CRGB(0, falling, 255);
    case 4: return CRGB(rising, 0, 255);
    default: return CRGB(255, 0, falling);
  }
}

void renderSnow(uint32_t now) {
  for (auto& level : floors) fill_solid(level.strip, level.count, CRGB(0, 3, 12));
  const uint32_t duration = 8000UL - snowSpeed * 1000UL;
  for (uint8_t flake = 0; flake < snowDensity; ++flake) {
    const uint32_t shifted = now + (duration * flake) / snowDensity;
    const uint32_t phase = ((shifted % duration) * 768UL) / duration;
    const uint8_t floorIndex = phase / 256;
    auto& level = floors[floorIndex];
    // Each flake keeps its horizontal position while falling through all floors.
    uint32_t seed = flake * 2654435761UL + (shifted / duration) * 2246822519UL;
    seed ^= seed >> 16;
    const uint8_t pixel = ((seed & 255) * level.count) / 256;
    CRGB white(220, 235, 255);
    white.nscale8_video(32 + (static_cast<uint16_t>(triwave8(phase % 256)) * 223) / 255);
    level.strip[pixel] += white;
    white.nscale8_video(60);
    if (pixel + 1 < level.count) level.strip[pixel + 1] += white;
  }
  for (auto& level : floors) if (!level.on) fill_solid(level.strip, level.count, CRGB::Black);
}

void renderHalloween(uint32_t now) {
  const uint8_t phase = ((now % 6000UL) * 256UL) / 6000UL;
  for (size_t i = 0; i < 3; ++i) {
    auto& level = floors[i];
    const uint8_t pulse = sin8(phase + i * 85);
    const CRGB atmosphere = blend(CRGB(36, 0, 65), CRGB(255, 55, 0), scale8(pulse, pulse));
    for (uint8_t pixel = 0; pixel < level.count; ++pixel) {
      CRGB candle = atmosphere;
      candle.nscale8_video(192 + sin8(((now % 1700UL) * 256UL) / 1700UL + pixel * 37 + i * 53) / 4);
      // Two short orange flashes, followed by darkness and candlelight.
      const uint32_t flashPhase = (now + i * 1000UL) % 3000UL;
      if (flashPhase < 120 || (flashPhase >= 300 && flashPhase < 420)) candle = CRGB(255, 55, 0);
      else if (flashPhase < 600) candle.nscale8_video(12);
      level.strip[pixel] = level.on ? candle : CRGB::Black;
    }
  }
}

void renderLights(uint32_t now) {
  currentHue = ((now % HUE_CYCLE_MS) * 360UL) / HUE_CYCLE_MS;
  if (lightingMode == "snow" || lightingMode == "halloween") {
    if (lightingMode == "snow") renderSnow(now);
    else renderHalloween(now);
    FastLED.show();
    return;
  }
  for (size_t i = 0; i < 3; ++i) {
    auto& level = floors[i];
    CRGB color = level.color;
    if (lightingMode == "hsl_cycle") color = hslHueColor(currentHue);
    else if (lightingMode == "breathing") {
      const uint8_t wave = sin8(((now % 4000UL) * 256UL) / 4000UL);
      color.nscale8_video(20 + (static_cast<uint16_t>(wave) * 235) / 255);
    } else if (lightingMode == "color_wave") {
      color = hslHueColor((currentHue + (2 - i) * 120) % 360);
    } else if (lightingMode == "blink") {
      if ((now + i * 300UL) % 900UL >= 450) color = CRGB::Black;
    } else if (lightingMode == "chase") {
      const uint8_t order[] = {2, 1, 0, 1};
      if (i != order[(now / 900UL) % 4]) color = CRGB::Black;
      else color.nscale8_video(triwave8(((now % 900UL) * 256UL) / 900UL));
    }
    fill_solid(level.strip + level.start, level.count, level.on ? color : CRGB::Black);
  }
  FastLED.show();
}

void setup() {
  Serial.begin(115200);
  FastLED.addLeds<WS2812B, 14, GRB>(floor3, 12);
  FastLED.addLeds<WS2812B, 12, GRB>(floor2, 9);
  FastLED.addLeds<WS2812B, 13, GRB>(floor1, 9);
  FastLED.setBrightness(TEST_BRIGHTNESS);
  FastLED.clear();
  renderLights(millis());
  Serial.println("TESTE: ciclo HSL nos 3 andares (30 LEDs); brilho 255/255 (100%); ciclo de 8 s.");
  otaSetup();
  controlSetup();
}

void loop() {
  otaLoop();
  const uint32_t now = millis();
  if (now - lastFrame >= 20) {
    lastFrame = now;
    renderLights(now);
  }
  if (Serial.available()) {
    while (Serial.available()) Serial.read();
    Serial.printf("TESTE ATIVO: D14/D12/D13, 30 LEDs, ciclo HSL H=%u S=100%% L=50%%; brilho 255/255; ciclo de 8 s.\n", currentHue);
    printNetworkStatus();
  }
  delay(10);
}
