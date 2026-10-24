#pragma once

#include <WiFi.h>
#include <WebServer.h>
#include <ESPmDNS.h>
#include <Update.h>
#include "wifi_config.h"

constexpr const char* FIRMWARE_VERSION = "smart-house-web-8";
class OtaWebServer : public WebServer {
 public:
  OtaWebServer() : WebServer(80) {}
  bool isListening() { return static_cast<bool>(_server); }
};
OtaWebServer otaServer;
bool uploadAllowed = false;
bool uploadComplete = false;
bool networkAnnounced = false;
bool mdnsStarted = false;
uint32_t lastWifiRetry = 0;
uint32_t rebootAt = 0;

const char UPDATE_PAGE[] PROGMEM = R"HTML(<!doctype html>
<html lang="pt-BR"><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1">
<title>Smart House</title><style>
body{font:17px system-ui;background:#f3f5f7;color:#17232c;max-width:600px;margin:60px auto;padding:24px}
main{background:white;padding:28px;border-radius:18px}h1{margin-top:0}input,button{font:inherit;margin:12px 0;max-width:100%}
button{background:#146a54;color:white;border:0;border-radius:8px;padding:12px 20px;cursor:pointer}
</style><main><h1>Smart House</h1><p>30 LEDs nos tres andares (D14/D12/D13), com ciclo de cores HSL sincronizado a 100% de brilho. Uma volta a cada 8 segundos.</p>
<h2>Atualizar programa</h2><p>Escolha o arquivo de firmware <b>.ino.bin</b>. Mantenha a alimenta&ccedil;&atilde;o ligada at&eacute; concluir.</p>
<form method="post" action="/update" enctype="multipart/form-data"><input type="file" name="firmware" accept=".bin" required><br>
<button type="submit">Enviar atualiza&ccedil;&atilde;o</button></form>
<p>Use um firmware que mantenha a atualiza&ccedil;&atilde;o por Wi-Fi. N&atilde;o envie arquivos merged, bootloader ou partitions.</p>
<p><a href="/status">Ver estado do dispositivo</a></p></main></html>)HTML";

bool otaAuthenticate() {
  if (otaServer.authenticate(OTA_USER, OTA_PASSWORD)) return true;
  otaServer.requestAuthentication();
  return false;
}

void printNetworkStatus() {
  Serial.printf("Firmware: %s\n", FIRMWARE_VERSION);
  if (WiFi.status() == WL_CONNECTED) {
    Serial.printf("Wi-Fi conectado. Atualizacao: http://%s/\n", WiFi.localIP().toString().c_str());
    Serial.println("Nome local: http://smart-house.local/ (se a rede suportar mDNS)");
    Serial.printf("Sinal Wi-Fi: %d dBm | gateway: %s\n", WiFi.RSSI(), WiFi.gatewayIP().toString().c_str());
    Serial.printf("Servidor HTTP em escuta: %s\n", otaServer.isListening() ? "SIM" : "NAO");
  } else {
    Serial.printf("Wi-Fi desconectado (status %d). Tentando conectar.\n", static_cast<int>(WiFi.status()));
  }
}

void otaSetup() {
  WiFi.persistent(false);
  WiFi.mode(WIFI_STA);
  WiFi.setHostname("smart-house");
  WiFi.setAutoReconnect(true);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  WiFi.setSleep(false);
  lastWifiRetry = millis();

  const char* headers[] = {"Origin", "Host"};
  otaServer.collectHeaders(headers, 2);
  otaServer.on("/update", HTTP_GET, []() {
    if (!otaAuthenticate()) return;
    otaServer.sendHeader("Cache-Control", "no-store");
    otaServer.send_P(200, "text/html; charset=utf-8", UPDATE_PAGE);
  });
  otaServer.on("/status", HTTP_GET, []() {
    if (!otaAuthenticate()) return;
    otaServer.sendHeader("Cache-Control", "no-store");
    otaServer.send(200, "application/json", controlState());
  });
  otaServer.on("/update", HTTP_POST, []() {
    if (!otaAuthenticate()) return;
    if (!uploadAllowed) {
      otaServer.send(403, "text/plain; charset=utf-8", "Origem ou arquivo invalido.");
      return;
    }
    otaServer.sendHeader("Connection", "close");
    if (!uploadComplete || Update.hasError()) {
      otaServer.send(400, "text/plain; charset=utf-8", "Atualizacao falhou. Envie um firmware .ino.bin valido para ESP32.");
    } else {
      otaServer.send(200, "text/plain; charset=utf-8", "Atualizacao concluida. O ESP32 vai reiniciar; abra a pagina novamente em alguns segundos.");
      rebootAt = millis() + 1000;
    }
    uploadAllowed = false;
    uploadComplete = false;
  }, []() {
    HTTPUpload& upload = otaServer.upload();
    if (upload.status == UPLOAD_FILE_START) {
      uploadAllowed = false;
      uploadComplete = false;
      // Autentica antes de tocar na flash. Origem vazia permite clientes CLI autenticados.
      if (!otaServer.authenticate(OTA_USER, OTA_PASSWORD)) return;
      const String origin = otaServer.header("Origin");
      if (origin.length() && origin != String("http://") + otaServer.header("Host")) return;
      if (!upload.filename.endsWith(".bin")) return;
      uploadAllowed = true;
      if (!Update.begin(UPDATE_SIZE_UNKNOWN, U_FLASH)) Update.printError(Serial);
    } else if (uploadAllowed && upload.status == UPLOAD_FILE_WRITE) {
      if (!Update.hasError() && Update.write(upload.buf, upload.currentSize) != upload.currentSize) {
        Update.printError(Serial);
      }
    } else if (uploadAllowed && upload.status == UPLOAD_FILE_END) {
      uploadComplete = !Update.hasError() && Update.end(true);
      if (!uploadComplete) Update.printError(Serial);
      else Serial.printf("OTA concluido: %u bytes.\n", static_cast<unsigned>(upload.totalSize));
    } else if (upload.status == UPLOAD_FILE_ABORTED) {
      if (uploadAllowed) Update.abort();
      uploadAllowed = false;
      uploadComplete = false;
    }
  });
  otaServer.onNotFound([]() {
    if (!otaAuthenticate()) return;
    otaServer.send(404, "text/plain", "Pagina nao encontrada.");
  });
}

void otaLoop() {
  const uint32_t now = millis();
  if (rebootAt && static_cast<int32_t>(now - rebootAt) >= 0) ESP.restart();
  if (WiFi.status() == WL_CONNECTED) {
    if (!networkAnnounced) {
      // Inicia somente depois de obter IP e renova o socket ao reconectar.
      otaServer.begin();
      if (mdnsStarted) MDNS.end();
      mdnsStarted = MDNS.begin("smart-house");
      if (mdnsStarted) MDNS.addService("http", "tcp", 80);
      networkAnnounced = true;
      printNetworkStatus();
    }
    otaServer.handleClient();
  } else {
    if (networkAnnounced) otaServer.stop();
    networkAnnounced = false;
    if (now - lastWifiRetry >= 30000) {
      lastWifiRetry = now;
      WiFi.reconnect();
    }
  }
}
