#include <WiFi.h>
#include <WebServer.h>
#include <ESPmDNS.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <esp_random.h>
#include "wifi_config.h"
#include "hands.h"

Adafruit_SSD1306 display(128, 64, &Wire, -1);
WebServer server(80);
enum Phase { STANDBY, COUNTDOWN, RESULT };
Phase phase = STANDBY;
uint32_t started = 0, resultStarted = 0, lastDraw = 0;
uint32_t roundId = 0;
int player = -1, computer = -1;
bool oled = false;
const char* choices[] = {"PEDRA", "PAPEL", "TESOURA"};
String verdict;
uint16_t textColor = SSD1306_WHITE;
constexpr uint8_t BUZZER_PIN = 23;
constexpr uint32_t CUE_MS = 700;
bool buzzerReady = false;
int buzzerCue = -1;

void setBuzzerTone(uint16_t frequency) {
  static uint16_t playing = 0;
  if (frequency != playing) {
    ledcWriteTone(BUZZER_PIN, frequency);
    playing = frequency;
  }
}

void updateBuzzer() {
  if (!buzzerReady) return;
  if (phase == RESULT) {
    uint32_t elapsed = millis() - resultStarted;
    // Durations include a short silent gap between the first two notes.
    const uint16_t win[] = {523, 659, 1047};
    const uint16_t lose[] = {392, 330, 196};
    const uint16_t tie[] = {660, 660, 660};
    const uint16_t* notes = verdict == "VOCE GANHOU!" ? win
      : verdict == "ESP GANHOU!" ? lose : verdict == "EMPATE!" ? tie : nullptr;
    uint16_t frequency = 0;
    if (notes && elapsed < 900) {
      const int note = elapsed < 220 ? 0 : elapsed < 440 ? 1 : 2;
      const uint32_t within = elapsed - note * 220;
      if (within < (note == 2 ? 460U : 160U)) frequency = notes[note];
    }
    setBuzzerTone(frequency);
    buzzerCue = -1;
    return;
  }
  if (phase != COUNTDOWN) {
    setBuzzerTone(0);
    buzzerCue = -1;
    return;
  }
  const uint32_t elapsed = millis() - started;
  const int cue = elapsed / CUE_MS;
  const uint16_t frequencies[] = {660, 880, 1320};
  const uint16_t durations[] = {140, 140, 600};
  setBuzzerTone(cue < 3 && elapsed % CUE_MS < durations[cue] ? frequencies[cue] : 0);
  buzzerCue = cue;
}

void text(const String& value, int y, int size = 1, bool bold = true, int left = 0, int width = 128) {
  int16_t x1, y1; uint16_t w, h;
  display.setTextSize(size);
  display.getTextBounds(value, 0, y, &x1, &y1, &w, &h);
  int visibleWidth = (int)w;
  if (bold) {
    visibleWidth = -size;
    for (unsigned int i = 0; i < value.length(); ++i)
      visibleWidth += (value[i] == 'M' ? 8 : 6) * size + 1;
  }
  const int x = left + (width - visibleWidth) / 2 - x1;
  if (bold) {
    // M needs seven columns so its diagonals remain open after emboldening.
    const uint8_t wideM[] = {0x7F, 0x02, 0x04, 0x08, 0x04, 0x02, 0x7F};
    int glyphX = x;
    for (unsigned int i = 0; i < value.length(); ++i) {
      for (int offset = 0; offset <= 1; ++offset) {
        if (value[i] == 'M') {
          for (int col = 0; col < 7; ++col)
            for (int row = 0; row < 7; ++row)
              if (wideM[col] & (1 << row))
                display.fillRect(glyphX + col * size + offset, y + row * size,
                                 size, size, textColor);
        } else {
          display.setCursor(glyphX + offset, y);
          display.write(value[i]);
        }
      }
      glyphX += (value[i] == 'M' ? 8 : 6) * size + 1;
    }
  } else {
    display.setCursor(x, y);
    display.print(value);
  }
}

void columnText(const char* value, int column, int y) {
  text(value, y, 1, true, column * 64, 64);
}

void drawHand(int choice, int column) {
  const int x = column * 64 + (64 - HAND_WIDTH) / 2;
  if (choice < 0) {
    text("?", 30, 3, true, column * 64, 64);
    return;
  }
  display.drawBitmap(x, 32, HANDS[choice], HAND_WIDTH, HAND_HEIGHT, SSD1306_WHITE);
}


void status() {
  const char* name = phase == STANDBY ? "standby" : phase == COUNTDOWN ? "countdown" : "result";
  String body = "{\"state\":\"" + String(name) + "\",\"round\":" + String(roundId);
  body += ",\"elapsed_ms\":" + String(phase == COUNTDOWN ? millis() - started : 0);
  body += ",\"player\":" + String(player) + ",\"computer\":" + String(phase == RESULT ? computer : -1);
  body += ",\"result\":\"" + verdict + "\"}";
  server.send(200, "application/json", body);
}

void finishRound() {
  if (player < 0) verdict = "SEM JOGADA";
  else if (player == computer) verdict = "EMPATE!";
  else if ((player - computer + 3) % 3 == 1) verdict = "VOCE GANHOU!";
  else verdict = "ESP GANHOU!";
  phase = RESULT;
  resultStarted = millis();
}

void draw() {
  if (!oled) return;
  display.clearDisplay();
  if (phase == RESULT) {
    display.fillRect(0, 0, 128, 16, SSD1306_WHITE);
    textColor = SSD1306_BLACK;
    display.setTextColor(textColor);
  }
  const String heading = phase != RESULT ? String("JOKENPO")
    : verdict == "VOCE GANHOU!" ? String("> VOCE VENCEU <") : verdict;
  if (phase != COUNTDOWN) text(heading, 4, 1, phase == RESULT && WiFi.status() == WL_CONNECTED);
  textColor = SSD1306_WHITE;
  display.setTextColor(textColor);
  if (WiFi.status() != WL_CONNECTED) {
    text("Conectando Wi-Fi", 24, 1, false); text("Confira rede/senha", 44, 1, false);
  } else if (phase == STANDBY) {
    text("Mostre sua mao", 22, 1, false); text("PEDRA PAPEL TESOURA", 34, 1, false);
    text(WiFi.localIP().toString(), 54, 1, false);
  } else if (phase == COUNTDOWN) {
    uint32_t elapsed = millis() - started;
    const char* cue = elapsed < CUE_MS ? "JO" : elapsed < 2 * CUE_MS ? "KEN" : "PO!!";
    text(cue, 22, 5);
  } else {
    display.drawFastVLine(64, 19, 43, SSD1306_WHITE);
    columnText("VOCE", 0, 20);
    columnText("ESP32", 1, 20);
    drawHand(player, 0);
    drawHand(computer, 1);
  }
  display.display();
}

void setup() {
  Serial.begin(115200);
  pinMode(BUZZER_PIN, OUTPUT);
  digitalWrite(BUZZER_PIN, LOW);
  buzzerReady = ledcAttach(BUZZER_PIN, 660, 10);
  if (!buzzerReady) Serial.println("Buzzer: falha ao configurar GPIO23");
  Wire.begin(21, 22);
  for (uint8_t address : {0x3C, 0x3D}) {
    Wire.beginTransmission(address);
    if (Wire.endTransmission() == 0) {
      oled = display.begin(SSD1306_SWITCHCAPVCC, address, true, false);
      break;
    }
  }
  if (!oled) Serial.println("OLED nao encontrado");
  display.setTextColor(SSD1306_WHITE); display.setTextWrap(false);
  WiFi.mode(WIFI_STA); WiFi.setAutoReconnect(true);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  WiFi.setSleep(false);  // Keep HTTP gesture delivery responsive during countdown.
  server.on("/status", HTTP_GET, status);
  server.on("/start", HTTP_POST, []() {
    if (phase != STANDBY) { server.send(409, "text/plain", "Rodada em andamento"); return; }
    const String move = server.arg("move");
    if (move != "0" && move != "1" && move != "2") {
      server.send(400, "text/plain", "Escolha pedra, papel ou tesoura para iniciar"); return;
    }
    computer = esp_random() % 3;
    player = move.toInt();  // Commit once, together with the new round.
    ++roundId; phase = COUNTDOWN; started = millis(); verdict = "";
    buzzerCue = -1;
    updateBuzzer();
    status();
  });
  server.on("/gesture", HTTP_POST, []() {
    server.send(409, "text/plain", "Jogada ja confirmada no inicio da rodada");
  });
  server.begin(); draw();
}

void loop() {
  // The choice is committed at start and remains fixed through the countdown.
  if (phase == COUNTDOWN && millis() - started >= 3 * CUE_MS) finishRound();
  if (phase == RESULT && millis() - resultStarted >= 3500) phase = STANDBY;
  updateBuzzer();
  server.handleClient();
  static bool announced = false;
  static uint32_t retryAt = 0;
  if (WiFi.status() == WL_CONNECTED && !announced) {
    Serial.print("Jokenpo IP: "); Serial.println(WiFi.localIP());
    MDNS.begin("jokenpo"); announced = true;
  } else if (WiFi.status() != WL_CONNECTED) {
    announced = false;
    if (millis() - retryAt >= 30000) { retryAt = millis(); WiFi.reconnect(); }
  }
  if (millis() - lastDraw >= 50) { lastDraw = millis(); draw(); }
  delay(2);
}
