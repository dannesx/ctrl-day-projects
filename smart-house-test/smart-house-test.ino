#include <FastLED.h>
#include <esp_random.h>

// HSL: matiz 0..359 graus, saturacao 100%, luminosidade 50%.
// Com L=50%, as cores ficam saturadas; L=100% produziria somente branco.
constexpr uint8_t MAX_LED_BRIGHTNESS = 191;
uint8_t brightnessPercent = 100;
constexpr uint32_t HUE_CYCLE_MS = 8000;
uint16_t currentHue = 0;
String lightingMode = "hsl_cycle";
uint8_t snowSpeed = 3;
uint8_t snowDensity = 4;
#include "effects.h"
CRGB floor3[House::LEDS_PER_FLOOR], floor2[House::LEDS_PER_FLOOR], floor1[House::LEDS_PER_FLOOR];
struct Floor { const char* id; CRGB* strip; uint8_t start, count; CRGB color; bool on; };
Floor floors[] = {
  {"SUPERIOR", floor3, 0, House::LEDS_PER_FLOOR, CRGB(255,160,75), true},
  {"CENTRAL", floor2, 0, House::LEDS_PER_FLOOR, CRGB(255,160,75), true},
  {"TERREO", floor1, 0, House::LEDS_PER_FLOOR, CRGB(255,160,75), true}
};
String controlState();
void controlSetup();
void renderLights(uint32_t now);
#include "audio_support.h"
#include "ota_support.h"
#include "audio_web.h"
#include "control.h"

uint32_t lastFrame = 0;
House::Engine effectEngine;
void renderLights(uint32_t now) {
  House::Config cfg;
  cfg.mode=lightingMode=="manual"?House::Manual:lightingMode=="snow"?House::Snow:lightingMode=="halloween"?House::Halloween:lightingMode=="christmas"?House::Christmas:lightingMode=="matrix"?House::Matrix:House::Cycle;
  cfg.speed=snowSpeed;cfg.density=snowDensity;
  currentHue=((now%HUE_CYCLE_MS)*360UL)/HUE_CYCLE_MS;
  for(int i=0;i<3;++i){cfg.on[i]=floors[i].on;auto c=floors[i].color;cfg.colors[i]={c.r,c.g,c.b};}
  effectEngine.render(now,cfg);
  for(int i=0;i<3;++i)for(int p=0;p<floors[i].count;++p){auto c=effectEngine.pixels[i][p];floors[i].strip[p]=CRGB(c.r,c.g,c.b);}
  FastLED.show();
}

void setup() {
  Serial.begin(115200);
  FastLED.addLeds<WS2812B, 14, GRB>(floor3, House::LEDS_PER_FLOOR);
  FastLED.addLeds<WS2812B, 12, GRB>(floor2, House::LEDS_PER_FLOOR);
  FastLED.addLeds<WS2812B, 13, GRB>(floor1, House::LEDS_PER_FLOOR);
  FastLED.setBrightness(MAX_LED_BRIGHTNESS);
  FastLED.clear();
  effectEngine.seed=esp_random();
  renderLights(millis());
  Serial.println("TESTE: ciclo HSL nos 3 andares (30 LEDs); brilho 191/255 (interface 100%); ciclo de 8 s.");
  audioSetup();
  otaSetup();
  controlSetup();
  audioWebSetup();
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
    Serial.printf("TESTE ATIVO: D14/D12/D13, 30 LEDs, ciclo HSL H=%u S=100%% L=50%%; brilho limitado a 191/255; ciclo de 8 s.\n", currentHue);
    printNetworkStatus();
  }
  delay(10);
}
