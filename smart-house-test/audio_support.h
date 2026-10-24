#pragma once
#include <LittleFS.h>
#include <ESP_I2S.h>
#include <atomic>
#include "wav_format.h"

// Only the worker touches I2S; filesystem mutations first suspend and join playback.
constexpr size_t AUDIO_LIMIT = 1200000;
constexpr size_t AUDIO_FILE_LIMIT = 128000;
const char* const AUDIO_NAMES[] = {"inicio", "tudo_on", "tudo_off", "superior_on", "superior_off",
  "central_on", "central_off", "terreo_on", "terreo_off", "manual", "ciclo", "neve", "halloween", "natal", "matrix"};
constexpr size_t AUDIO_COUNT = sizeof(AUDIO_NAMES) / sizeof(AUDIO_NAMES[0]);
bool audioAvailable[AUDIO_COUNT] = {};
size_t audioSizes[AUDIO_COUNT] = {};
bool audioMounted = false, audioWorkerReady = false;
std::atomic<bool> audioSuspended{false}, audioMuted{false}, audioFault{false};
std::atomic<int> audioVolume{25}, audioPlaying{-1};
// Sequence and clip share one atomic word: cancellation cannot race a worker dequeue.
std::atomic<uint32_t> audioRequest{255};
SemaphoreHandle_t audioMutex = nullptr;
I2SClass houseI2S;

String audioPath(int id) { return String("/") + AUDIO_NAMES[id] + ".wav"; }
int audioId(const String& name) {
  for (size_t i = 0; i < AUDIO_COUNT; ++i) if (name == String(AUDIO_NAMES[i]) + ".wav") return i;
  return -1;
}
bool audioWav(File& file, WavInfo& info) {
  return readWav(file.size(), [&](size_t offset, uint8_t* dst, size_t length) {
    return file.seek(offset) && file.read(dst, length) == length;
  }, info);
}
void audioRefresh() {
  for (size_t i = 0; i < AUDIO_COUNT; ++i) {
    File file = audioMounted ? LittleFS.open(audioPath(i), "r") : File();
    WavInfo info;
    audioAvailable[i] = file && file.size() <= AUDIO_FILE_LIMIT && audioWav(file, info);
    audioSizes[i] = file ? file.size() : 0;
  }
}
void audioStop() { audioRequest = ((audioRequest.load() + 256) & 0xffffff00U) | 255; }
void audioPause() {
  audioSuspended = true;
  audioStop();
  if (audioMutex) { xSemaphoreTake(audioMutex, portMAX_DELAY); xSemaphoreGive(audioMutex); }
}
void audioResume() { audioSuspended = false; }
void audioSay(int id) {
  if (!audioWorkerReady || audioSuspended || audioMuted || audioVolume == 0) return;
  audioStop(); // A missing new clip also cancels a now-outdated announcement.
  if (id >= 0 && id < int(AUDIO_COUNT) && audioAvailable[id]) audioRequest = (audioRequest.load() & 0xffffff00U) | uint32_t(id);
}
void audioTask(void*) {
  houseI2S.setPins(26, 25, 27);
  uint32_t handled = 255;
  for (;;) {
    uint32_t generation = audioRequest.load();
    int id = generation & 255;
    if (generation == handled || id == 255) { vTaskDelay(pdMS_TO_TICKS(10)); continue; }
    handled = generation;
    xSemaphoreTake(audioMutex, portMAX_DELAY);
    if (!audioSuspended && !audioMuted && generation == audioRequest) {
      File file = LittleFS.open(audioPath(id), "r");
      WavInfo info;
      if (file && audioWav(file, info)) {
        if (houseI2S.begin(I2S_MODE_STD, 16000, I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_STEREO)) {
          audioFault = false;
          audioPlaying = id;
          file.seek(info.offset);
          uint32_t remaining = info.length;
          int16_t mono[256], stereo[512];
          while (remaining && !audioSuspended && !audioMuted && generation == audioRequest) {
            size_t bytes = file.read(reinterpret_cast<uint8_t*>(mono), std::min(size_t(remaining), sizeof(mono)));
            if (!bytes || (bytes & 1)) { audioFault = true; break; }
            int volume = audioVolume.load();
            for (size_t n = 0; n < bytes / 2; ++n) stereo[n*2] = stereo[n*2+1] = int32_t(mono[n]) * volume / 100;
            size_t written = 0;
            // Bounded DMA wait so cancellation cannot block HTTP/OTA indefinitely.
            if (i2s_channel_write(houseI2S.txChan(), stereo, bytes * 2, &written, 100) != ESP_OK || written != bytes * 2) {
              audioFault = true; break;
            }
            remaining -= bytes;
          }
          // Drain the short DMA tail with silence only for a normally completed clip.
          if (!remaining && generation == audioRequest && !audioSuspended && !audioMuted) {
            memset(stereo, 0, sizeof(stereo));
            for (int n = 0; n < 8 && generation == audioRequest && !audioSuspended && !audioMuted; ++n) {
              size_t written;
              i2s_channel_write(houseI2S.txChan(), stereo, sizeof(stereo), &written, 100);
            }
          }
          houseI2S.end();
          audioPlaying = -1;
        } else { audioFault = true; houseI2S.end(); }
      }
      file.close();
    }
    xSemaphoreGive(audioMutex);
  }
}
void audioSetup() {
  // Never automatically format an existing or unrecognized data partition.
  audioMounted = LittleFS.begin(false);
  if (audioMounted) LittleFS.remove("/upload.tmp");
  audioRefresh();
  audioMutex = xSemaphoreCreateMutex();
  audioWorkerReady = audioMutex && xTaskCreate(audioTask, "house-audio", 6144, nullptr, 1, nullptr) == pdPASS;
  audioFault = !audioWorkerReady;
  audioSay(0);
}
String audioState() {
  String json = String("{\"storage_ready\":") + (audioMounted ? "true" : "false") +
    ",\"ready\":" + (audioWorkerReady && !audioFault ? "true" : "false") +
    ",\"volume\":" + String(audioVolume.load()) + ",\"muted\":" + (audioMuted ? "true" : "false") +
    ",\"playing\":" + String(audioPlaying.load()) + ",\"files\":[";
  for (size_t i = 0; i < AUDIO_COUNT; ++i) {
    if (i) json += ',';
    json += String("{\"name\":\"") + AUDIO_NAMES[i] + ".wav\",\"available\":" +
      (audioAvailable[i] ? "true" : "false") + ",\"size\":" + String(audioSizes[i]) + '}';
  }
  return json + "]}";
}
