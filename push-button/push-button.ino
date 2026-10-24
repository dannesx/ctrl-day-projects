#include <Adafruit_NeoPixel.h>
#include "reset_gesture.h"

// Arduino Uno: botoes entre D2/D3 e GND. D4 -> DIN da matriz esquerda.
// DOUT da esquerda -> DIN da direita. Matrizes WS2812B, 8x8 cada.
constexpr uint8_t YELLOW_PIN = 2;
constexpr uint8_t BLUE_PIN = 3;
constexpr uint8_t LED_PIN = 4;
constexpr uint8_t SIDE = 8;
constexpr uint8_t PER_PLAYER = 64;
constexpr uint16_t DEBOUNCE_MS = 20;
// Fisicamente: colunas no mesmo sentido, primeiro LED em A8.
constexpr bool SERPENTINE = false;
constexpr bool LEFT_MATRIX_FIRST = true;
constexpr uint8_t BRIGHTNESS = 24;

Adafruit_NeoPixel leds(128, LED_PIN, NEO_GRB + NEO_KHZ800);

struct Button {
  uint8_t pin;
  bool raw;
  bool stable;
  uint32_t changedAt;
};
Button yellow = {YELLOW_PIN, HIGH, HIGH, 0};
Button blue = {BLUE_PIN, HIGH, HIGH, 0};
enum Phase { COUNTDOWN, PLAYING, FINISHED };
Phase phase = COUNTDOWN;
uint32_t phaseStarted = 0;
int8_t shownCue = -1;
uint8_t yellowScore = 0;
uint8_t blueScore = 0;
bool restartArmed = false;
bool winnerVisible = true;
ResetGesture resetGesture;

bool updateButton(Button &button, uint32_t now) {
  bool reading = digitalRead(button.pin);
  if (reading != button.raw) {
    button.raw = reading;
    button.changedAt = now;
  }
  if (button.stable != button.raw && now - button.changedAt >= DEBOUNCE_MS) {
    button.stable = button.raw;
    return button.stable == LOW;
  }
  return false;
}

uint16_t pixelAt(uint8_t x, uint8_t y) {
  uint8_t panel = x / SIDE;
  uint8_t column = x % SIDE;
  if (!LEFT_MATRIX_FIRST) panel = 1 - panel;
  // Coordenadas visuais: x cresce para a direita; y=0 e a linha 1 (topo).
  uint8_t row = SIDE - 1 - y;
  if (SERPENTINE && (column & 1)) row = y;
  return panel * PER_PLAYER + column * SIDE + row;
}

void drawGlyph(const uint8_t *rows, uint8_t width, uint8_t x, uint32_t color) {
  for (uint8_t y = 0; y < 5; ++y) {
    for (uint8_t column = 0; column < width; ++column) {
      if (rows[y] & (1 << (width - 1 - column))) {
        leds.setPixelColor(pixelAt(x + column, y + 1), color);
      }
    }
  }
}

void showCue(uint8_t cue) {
  const uint8_t digits[3][5] = {
    {7, 1, 7, 1, 7}, // 3
    {7, 1, 7, 4, 7}, // 2
    {2, 6, 2, 2, 7}  // 1
  };
  leds.clear();
  if (cue < 3) {
    drawGlyph(digits[cue], 3, 2, leds.Color(255, 160, 0));
    drawGlyph(digits[cue], 3, 10, leds.Color(0, 80, 255));
  } else {
    const uint8_t g[] = {7, 4, 5, 5, 7};
    const uint8_t o[] = {7, 5, 5, 5, 7};
    const uint8_t bang[] = {1, 1, 1, 0, 1};
    uint32_t green = leds.Color(0, 255, 0);
    drawGlyph(g, 3, 3, green);
    drawGlyph(o, 3, 7, green);
    drawGlyph(bang, 1, 12, green);
  }
  leds.show();
}

void startRound(uint32_t now) {
  yellowScore = blueScore = 0;
  shownCue = -1;
  restartArmed = false;
  phaseStarted = now;
  phase = COUNTDOWN;
}

void showScores() {
  leds.clear();
  // Ambos crescem das bordas externas em direcao ao centro.
  for (uint8_t i = 0; i < yellowScore; ++i) {
    leds.setPixelColor(pixelAt(i / SIDE, i % SIDE), leds.Color(255, 180, 0));
  }
  for (uint8_t i = 0; i < blueScore; ++i) {
    leds.setPixelColor(pixelAt(15 - i / SIDE, i % SIDE), leds.Color(0, 60, 255));
  }
  leds.show();
}

void setup() {
  pinMode(YELLOW_PIN, INPUT_PULLUP);
  pinMode(BLUE_PIN, INPUT_PULLUP);
  leds.begin();
  leds.setBrightness(BRIGHTNESS);
  leds.clear();
  leds.show();
  startRound(millis());
}

void loop() {
  uint32_t now = millis();
  // Atualiza inclusive durante a contagem: segurar nao gera pontos na largada.
  bool yellowPressed = updateButton(yellow, now);
  bool bluePressed = updateButton(blue, now);

  if(resetGesture.update(now,yellow.stable==LOW,blue.stable==LOW,phase==PLAYING)) startRound(now);

  if (phase == COUNTDOWN) {
    uint32_t elapsed = now - phaseStarted;
    if (elapsed >= 3500) {
      phase = PLAYING;
      showScores();
    } else {
      uint8_t cue = elapsed / 1000;
      if (shownCue != cue) {
        shownCue = cue;
        showCue(cue);
      }
    }
    return;
  }

  if (phase == PLAYING) {
    if (yellowPressed) ++yellowScore;
    if (bluePressed) ++blueScore;
    if (yellowPressed || bluePressed) showScores();
    if (yellowScore == PER_PLAYER || blueScore == PER_PLAYER) {
      phase = FINISHED;
      phaseStarted = now;
      winnerVisible = true;
    }
    return;
  }

  // Tres ciclos apagado/aceso de 800 ms; depois mantem o placar aceso.
  // Empate pisca os dois lados.
  uint32_t elapsed = now - phaseStarted;
  bool visible = elapsed >= 2400 || (elapsed / 400) % 2 == 1;
  if (visible != winnerVisible) {
    winnerVisible = visible;
    showScores();
    if (!visible) {
      for (uint8_t y = 0; y < SIDE; ++y) {
        for (uint8_t x = 0; x < SIDE; ++x) {
          if (yellowScore == PER_PLAYER) leds.setPixelColor(pixelAt(x, y), 0);
          if (blueScore == PER_PLAYER) leds.setPixelColor(pixelAt(x + SIDE, y), 0);
        }
      }
      leds.show();
    }
  }
  if (yellow.stable == HIGH && blue.stable == HIGH) restartArmed = true;
  if (elapsed >= 2400 && restartArmed && yellow.stable == LOW && blue.stable == LOW) {
    startRound(now);
  }
}
