PYTHON ?= python3
FQBN ?= esp32:esp32:esp32
PORT ?= /dev/ttyUSB0
ESP ?= http://jokenpo.local
BUILD ?= /tmp/esp32-jokenpo-build

.PHONY: setup run compile upload test
setup:
	$(PYTHON) -m venv .venv
	.venv/bin/python -m pip install -r requirements.txt
run:
	.venv/bin/python webcam.py --esp $(ESP)
compile:
	arduino-cli compile --fqbn $(FQBN) --build-path $(BUILD) .
upload: compile
	arduino-cli upload --port $(PORT) --fqbn $(FQBN) --input-dir $(BUILD) .
test:
	$(PYTHON) -m unittest discover -v
