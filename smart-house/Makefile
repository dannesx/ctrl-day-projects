FQBN ?= esp32:esp32:esp32
PORT ?= /dev/ttyUSB0
BUILD ?= /tmp/esp32-smart-house-build

.PHONY: compile upload monitor

compile:
	arduino-cli compile --fqbn $(FQBN) --build-path $(BUILD) .

upload: compile
	arduino-cli upload --port $(PORT) --fqbn $(FQBN) --input-dir $(BUILD) .

monitor:
	arduino-cli monitor --port $(PORT) --config baudrate=115200
