#include <FastLED.h>
#include <ctype.h>
#include <stdlib.h>
#include <string.h>

// ESP32 DevKit V1 + WS2812B (5 V), uma linha de dados por andar.
#define LED_TYPE WS2812B
#define COLOR_ORDER GRB
#define MAX_BRIGHTNESS 80

constexpr uint8_t PIN_FLOOR_3 = 14;
constexpr uint8_t PIN_FLOOR_2 = 12;
constexpr uint8_t PIN_FLOOR_1 = 13;
constexpr uint16_t NUM_FLOOR_3 = 12;
constexpr uint16_t NUM_FLOOR_2 = 9;
constexpr uint16_t NUM_FLOOR_1 = 9;

CRGB floor3[NUM_FLOOR_3];
CRGB floor2[NUM_FLOOR_2];
CRGB floor1[NUM_FLOOR_1];

struct Room {
  const char* name;
  CRGB* strip;
  uint16_t start;
  uint16_t count;
  CRGB color;
  bool on;
};

Room rooms[] = {
  {"QUARTO_SUP", floor3, 0, 8, CRGB(255, 160, 75), false},
  {"SACADA", floor3, 8, 4, CRGB(255, 160, 75), false},
  {"QUARTO_MEIO", floor2, 0, 6, CRGB(255, 160, 75), false},
  {"VARANDA", floor2, 6, 3, CRGB(255, 160, 75), false},
  {"SALA", floor1, 0, 6, CRGB(255, 160, 75), false},
  {"BANHEIRO", floor1, 6, 3, CRGB(255, 255, 255), false}
};
constexpr size_t ROOM_COUNT = sizeof(rooms) / sizeof(rooms[0]);

char serialLine[101];
size_t serialLength = 0;
bool serialOverflow = false;
uint8_t brightness = MAX_BRIGHTNESS;

void render() {
  for (auto& room : rooms) {
    fill_solid(room.strip + room.start, room.count, room.on ? room.color : CRGB::Black);
  }
  FastLED.show();
}

void setAll(bool enabled) {
  for (auto& room : rooms) room.on = enabled;
  render();
}

void showHelp() {
  Serial.println("\n=== SMART HOUSE MVP ===");
  Serial.println("AJUDA | LISTA | LIGAR SALA | DESLIGAR BANHEIRO");
  Serial.println("COR QUARTO_SUP 255 0 0 | BRILHO 60 (0 a 80)");
  Serial.println("TUDO ON | TUDO OFF | DEMO");
  Serial.println("Comodos: QUARTO_SUP, SACADA, QUARTO_MEIO, VARANDA, SALA, BANHEIRO");
}

void listRooms() {
  Serial.printf("Brilho: %u/%u (escala FastLED: 0 a 255)\n", brightness, MAX_BRIGHTNESS);
  for (const auto& room : rooms) {
    Serial.printf("%s: %s | RGB %u %u %u | %u LEDs\n", room.name,
                  room.on ? "ON" : "OFF", room.color.r, room.color.g, room.color.b, room.count);
  }
}

void demo() {
  setAll(false);
  for (auto& room : rooms) {
    room.on = true;
    render();
    delay(550);
  }
  delay(1000);
}

bool parseValue(const char* token, int maximum, uint8_t& result) {
  if (!token || !*token) return false;
  // Somente digitos: rejeita texto, negativos e valores parcialmente numericos.
  unsigned int value = 0;
  for (const char* p = token; *p; ++p) {
    if (*p < '0' || *p > '9') return false;
    value = value * 10 + (*p - '0');
    if (value > static_cast<unsigned int>(maximum)) return false;
  }
  result = static_cast<uint8_t>(value);
  return true;
}

void handleCommand(char* line) {
  for (char* p = line; *p; ++p) *p = toupper(static_cast<unsigned char>(*p));
  char* args[6];
  size_t count = 0;
  char* context = nullptr;
  for (char* token = strtok_r(line, " \t", &context); token;
       token = strtok_r(nullptr, " \t", &context)) {
    if (count == 6) { Serial.println("Argumentos demais. Digite AJUDA."); return; }
    args[count++] = token;
  }
  if (!count) return;
  if (count == 1) {
    if (!strcmp(args[0], "AJUDA") || !strcmp(args[0], "HELP")) { showHelp(); return; }
    if (!strcmp(args[0], "LISTA")) { listRooms(); return; }
    if (!strcmp(args[0], "DEMO")) { demo(); Serial.println("Demo concluida."); return; }
  }
  if (count == 2 && !strcmp(args[0], "TUDO") &&
      (!strcmp(args[1], "ON") || !strcmp(args[1], "OFF"))) {
    setAll(!strcmp(args[1], "ON"));
    Serial.println(!strcmp(args[1], "ON") ? "Tudo ligado." : "Tudo desligado.");
    return;
  }
  if (count == 2 && !strcmp(args[0], "BRILHO")) {
    uint8_t value;
    if (!parseValue(args[1], MAX_BRIGHTNESS, value)) {
      Serial.println("Use: BRILHO 60 (0 a 80)"); return;
    }
    brightness = value;
    FastLED.setBrightness(brightness);
    render();
    Serial.printf("Brilho: %u/%u\n", brightness, MAX_BRIGHTNESS);
    return;
  }
  const bool power = !strcmp(args[0], "LIGAR") || !strcmp(args[0], "DESLIGAR");
  const bool color = !strcmp(args[0], "COR");
  if ((power && count == 2) || (color && count == 5)) {
    for (auto& room : rooms) {
      if (strcmp(args[1], room.name)) continue;
      if (power) room.on = !strcmp(args[0], "LIGAR");
      else {
        uint8_t r, g, b;
        if (!parseValue(args[2], 255, r) || !parseValue(args[3], 255, g) ||
            !parseValue(args[4], 255, b)) {
          Serial.println("Use: COR SALA 255 0 0 (valores de 0 a 255)"); return;
        }
        room.color = CRGB(r, g, b);
        room.on = true;
      }
      render();
      Serial.printf("%s: %s | RGB %u %u %u\n", room.name, room.on ? "ON" : "OFF",
                    room.color.r, room.color.g, room.color.b);
      return;
    }
    Serial.println("Comodo nao encontrado. Digite LISTA.");
    return;
  }
  Serial.println("Comando invalido. Digite AJUDA.");
}

void setup() {
  Serial.begin(115200);
  delay(350);
  FastLED.addLeds<LED_TYPE, PIN_FLOOR_3, COLOR_ORDER>(floor3, NUM_FLOOR_3);
  FastLED.addLeds<LED_TYPE, PIN_FLOOR_2, COLOR_ORDER>(floor2, NUM_FLOOR_2);
  FastLED.addLeds<LED_TYPE, PIN_FLOOR_1, COLOR_ORDER>(floor1, NUM_FLOOR_1);
  FastLED.setBrightness(brightness);
  FastLED.clear(true);
  showHelp();
  demo();
}

void loop() {
  while (Serial.available()) {
    const char c = static_cast<char>(Serial.read());
    if (c == '\r') continue;
    if (c == '\n') {
      if (serialOverflow) Serial.println("Linha muito longa (maximo 100 caracteres).");
      else {
        serialLine[serialLength] = '\0';
        handleCommand(serialLine);
      }
      serialLength = 0;
      serialOverflow = false;
    } else if (serialLength < sizeof(serialLine) - 1 && !serialOverflow) {
      // NUL nao pode encerrar antecipadamente um comando recebido.
      if (c == '\0') serialOverflow = true;
      else serialLine[serialLength++] = c;
    } else serialOverflow = true;
  }
}
