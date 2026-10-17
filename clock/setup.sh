#!/usr/bin/env bash
set -euo pipefail

clock_dir="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
esp32_index="https://espressif.github.io/arduino-esp32/package_esp32_index.json"
# Altere FQBN ao executar se estiver usando outra variante do ESP32.
board_fqbn="${FQBN:-esp32:esp32:esp32}"

arduino-cli version
arduino-cli core update-index --additional-urls "$esp32_index"
arduino-cli core install esp32:esp32 --additional-urls "$esp32_index"
arduino-cli lib update-index
arduino-cli lib install "Adafruit SSD1306" "Adafruit GFX Library"
arduino-cli compile --fqbn "$board_fqbn" "$clock_dir"
