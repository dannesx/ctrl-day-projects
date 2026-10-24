#pragma once
#include "web_ui.h"

String controlState() {
  String state = String("{\"firmware\":\"") + FIRMWARE_VERSION + "\",\"ip\":\"" + WiFi.localIP().toString() +
    "\",\"brightness\":" + String(FastLED.getBrightness()) + ",\"mode\":\"" + lightingMode +
    "\",\"cycle_ms\":" + String(HUE_CYCLE_MS) + ",\"hue\":" + String(currentHue) + ",\"leds\":30,\"uptime_ms\":" + String(millis()) + ",\"snow_speed\":" + String(snowSpeed) + ",\"snow_density\":" + String(snowDensity) + ",\"floors\":[";
  for (size_t i = 0; i < 3; ++i) {
    const auto& level = floors[i];
    char color[8];
    snprintf(color, sizeof(color), "#%02x%02x%02x", level.color.r, level.color.g, level.color.b);
    char rendered[8];
    const CRGB actual = level.strip[level.start];
    snprintf(rendered, sizeof(rendered), "#%02x%02x%02x", actual.r, actual.g, actual.b);
    if (i) state += ',';
    state += String("{\"id\":\"") + level.id + "\",\"on\":" + (level.on ? "true" : "false") +
      ",\"color\":\"" + color + "\",\"leds\":" + String(level.count) + ",\"rendered_color\":\"" + rendered + "\"";
    if (lightingMode == "snow" || lightingMode == "halloween" || lightingMode == "christmas" || lightingMode == "matrix") {
      state += ",\"pixels\":[";
      for (uint8_t pixel = 0; pixel < level.count; ++pixel) {
        const CRGB value = level.strip[pixel];
        char hex[8];
        snprintf(hex, sizeof(hex), "#%02x%02x%02x", value.r, value.g, value.b);
        if (pixel) state += ',';
        state += String("\"") + hex + "\"";
      }
      state += ']';
    }
    state += '}';
  }
  return state + "],\"brightness_percent\":" + String(brightnessPercent) + ",\"audio\":" + audioState() + "}";
}

bool decimalValue(const String& text, uint16_t maximum, uint16_t& value) {
  if (!text.length() || text.length() > 5) return false;
  uint32_t result = 0;
  for (size_t i = 0; i < text.length(); ++i) {
    if (text[i] < '0' || text[i] > '9') return false;
    result = result * 10 + text[i] - '0';
    if (result > maximum) return false;
  }
  value = result;
  return true;
}

void controlSetup() {
  otaServer.on("/", HTTP_GET, []() {
    if (!otaAuthenticate()) return;
    otaServer.sendHeader("Cache-Control", "no-store");
    otaServer.send_P(200, "text/html; charset=utf-8", CONTROL_PAGE);
  });
  otaServer.on("/control", HTTP_POST, []() {
    if (!otaAuthenticate()) return;
    const String origin = otaServer.header("Origin");
    if (origin.length() && origin != String("http://") + otaServer.header("Host")) {
      otaServer.send(403, "text/plain", "Origem invalida"); return;
    }
    auto invalid = []() { otaServer.send(400, "text/plain", "Comando invalido"); };
    const String action = otaServer.arg("action");
    int announcement = -1;
    if (action == "audio") {
      uint16_t volume = audioVolume.load();
      bool muted = audioMuted.load();
      if ((!otaServer.hasArg("volume") && !otaServer.hasArg("muted")) ||
          (otaServer.hasArg("volume") && !decimalValue(otaServer.arg("volume"), 100, volume))) { invalid(); return; }
      if (otaServer.hasArg("muted")) {
        const String value = otaServer.arg("muted");
        if (value != "true" && value != "false") { invalid(); return; }
        muted = value == "true";
      }
      audioVolume = volume; audioMuted = muted;
      if (muted || !volume) audioStop();
    } else if (action == "brightness") {
      uint16_t value;
      if (!decimalValue(otaServer.arg("value"), 100, value)) { invalid(); return; }
      brightnessPercent = value;
      FastLED.setBrightness((value * MAX_LED_BRIGHTNESS + 50) / 100);
    } else if (action == "mode") {
      const String value = otaServer.arg("value");
      if (value != "manual" && value != "hsl_cycle" && value != "snow" && value != "halloween" && value != "christmas" && value != "matrix") { invalid(); return; }
      if (lightingMode != value) announcement = value == "manual" ? 9 : value == "hsl_cycle" ? 10 : value == "snow" ? 11 : value == "halloween" ? 12 : value == "christmas" ? 13 : 14;
      lightingMode = value;
    } else if (action == "snow") {
      uint16_t speed = snowSpeed, density = snowDensity;
      if ((!otaServer.hasArg("speed") && !otaServer.hasArg("density")) ||
          (otaServer.hasArg("speed") && (!decimalValue(otaServer.arg("speed"), 5, speed) || speed < 1)) ||
          (otaServer.hasArg("density") && (!decimalValue(otaServer.arg("density"), 8, density) || density < 1))) { invalid(); return; }
      snowSpeed = speed;
      snowDensity = density;
    } else if (action == "all") {
      const String value = otaServer.arg("value");
      if (value != "on" && value != "off") { invalid(); return; }
      for (auto& level : floors) {
        if (level.on != (value == "on")) announcement = value == "on" ? 1 : 2;
        level.on = value == "on";
      }
    } else if (action == "floor") {
      Floor* target = nullptr;
      for (auto& level : floors) if (otaServer.arg("id") == level.id) target = &level;
      if (!target || (otaServer.hasArg("on") == otaServer.hasArg("color"))) { invalid(); return; }
      if (otaServer.hasArg("on")) {
        const String value = otaServer.arg("on");
        if (value != "true" && value != "false") { invalid(); return; }
        if (target->on != (value == "true")) announcement = 3 + 2 * (target - floors) + (value == "true" ? 0 : 1);
        target->on = value == "true";
      } else {
        const String color = otaServer.arg("color");
        if (color.length() != 7 || color[0] != '#') { invalid(); return; }
        for (size_t i = 1; i < 7; ++i) if (!isxdigit(static_cast<unsigned char>(color[i]))) { invalid(); return; }
        target->color = CRGB(strtoul(color.c_str() + 1, nullptr, 16));
        target->on = true;
        lightingMode = "manual";
      }
    } else { invalid(); return; }
    if (announcement >= 0) audioSay(announcement);
    renderLights(millis());
    otaServer.sendHeader("Cache-Control", "no-store");
    otaServer.send(200, "application/json", controlState());
  });
}
