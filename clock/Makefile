FQBN ?= esp32:esp32:esp32
PORT ?= /dev/ttyUSB0
BUILD ?= /tmp/esp32-clock-build

.PHONY: setup compile upload monitor

setup:
	FQBN="$(FQBN)" bash setup.sh

compile:
	arduino-cli compile --fqbn $(FQBN) --build-path $(BUILD) .

upload: compile
	arduino-cli upload --port $(PORT) --fqbn $(FQBN) --input-dir $(BUILD) .

monitor:
	arduino-cli monitor --port $(PORT) --config baudrate=115200
