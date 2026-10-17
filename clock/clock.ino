/*
 * Relogio para ESP32 + OLED SSD1306 I2C 0,96", 128x64.
 * Bibliotecas: Adafruit SSD1306 e Adafruit GFX (com dependencias).
 * Ligacoes no ESP32 classico: VCC -> 3V3, GND -> GND,
 * SDA -> GPIO21, SCL -> GPIO22. Ajuste os pinos para outras placas.
 * Configure o Wi-Fi em wifi_config.h. Data/hora via NTP, fuso UTC-3.
 * Apos sincronizar, continua contando sem Wi-Fi enquanto houver energia.
 * Ao desligar, precisa da internet novamente para obter a hora correta.
 */

#include <WiFi.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <time.h>

#include "wifi_config.h"
constexpr int SDA_PIN = 21;
constexpr int SCL_PIN = 22;
constexpr long UTC_OFFSET_SECONDS = -3 * 60 * 60;
constexpr uint32_t WIFI_RETRY_MS = 30000;

Adafruit_SSD1306 display(128, 64, &Wire, -1);
const char* WEEKDAYS[] = {
  "Domingo", "Segunda-feira", "Terca-feira", "Quarta-feira",
  "Quinta-feira", "Sexta-feira", "Sabado"
};

bool displayReady = false;
bool ntpStarted = false;
uint32_t lastWifiAttempt = 0;
uint32_t lastFrame = 0;

void centeredText(const char* text, int16_t y, uint8_t size) {
  int16_t x1, y1;
  uint16_t width, height;
  display.setTextSize(size);
  display.getTextBounds(text, 0, y, &x1, &y1, &width, &height);
  display.setCursor((128 - static_cast<int16_t>(width)) / 2 - x1, y);
  display.print(text);
}

uint8_t findDisplayAddress() {
  // Os modulos SSD1306 geralmente usam 0x3C ou 0x3D.
  const uint8_t addresses[] = {0x3C, 0x3D};
  for (uint8_t address : addresses) {
    Wire.beginTransmission(address);
    if (Wire.endTransmission() == 0) return address;
  }
  return 0;
}

void setup() {
  Serial.begin(115200);
  Wire.begin(SDA_PIN, SCL_PIN);
  const uint8_t address = findDisplayAddress();
  // Wire ja foi iniciado com os pinos configurados acima.
  displayReady = address != 0 &&
    display.begin(SSD1306_SWITCHCAPVCC, address, true, false);
  if (!displayReady) {
    Serial.println("OLED nao encontrado. Verifique fios, pinos e modelo SSD1306.");
  } else {
    display.clearDisplay();
    display.setTextColor(SSD1306_WHITE);
    display.setTextWrap(false);
    centeredText("Relogio", 12, 2);
    centeredText("Conectando Wi-Fi...", 42, 1);
    display.display();
  }

  WiFi.mode(WIFI_STA);
  WiFi.setAutoReconnect(true);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  lastWifiAttempt = millis();
}

void loop() {
  const uint32_t now = millis();
  const bool connected = WiFi.status() == WL_CONNECTED;
  if (connected && !ntpStarted) {
    configTime(UTC_OFFSET_SECONDS, 0, "pool.ntp.org", "time.google.com",
               "time.cloudflare.com");
    ntpStarted = true;
    Serial.println("Wi-Fi conectado. Sincronizando data e hora via NTP...");
  }
  if (!connected && now - lastWifiAttempt >= WIFI_RETRY_MS) {
    lastWifiAttempt = now;
    WiFi.reconnect();
  }

  if (now - lastFrame < 250) {
    delay(10);
    return;
  }
  lastFrame = now;
  struct tm localTime = {};
  const bool hasTime = getLocalTime(&localTime, 0);
  if (!displayReady) {
    delay(10);
    return;
  }

  display.clearDisplay();
  if (!hasTime) {
    centeredText("Relogio", 4, 2);
    centeredText(connected ? "Sincronizando..." : "Conectando Wi-Fi...", 30, 1);
    centeredText(connected ? "Aguardando NTP" : "Confira rede e senha", 48, 1);
  } else {
    char dateText[11];
    char timeText[9];
    strftime(dateText, sizeof(dateText), "%d/%m/%Y", &localTime);
    strftime(timeText, sizeof(timeText), "%H:%M:%S", &localTime);
    centeredText(WEEKDAYS[localTime.tm_wday], 4, 1);
    // A faixa amarela ocupa y=0..15; mantenha a data na faixa azul.
    centeredText(dateText, 17, 1);
    display.drawFastHLine(8, 26, 112, SSD1306_WHITE);
    centeredText(timeText, 32, 2);
    centeredText(connected ? "UTC-3  Wi-Fi OK" : "UTC-3  Sem Wi-Fi", 56, 1);
  }
  display.display();
}
