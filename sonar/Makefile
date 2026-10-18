.DEFAULT_GOAL := run
PORT ?= /dev/ttyUSB0
FQBN ?= arduino:avr:uno
BUILD ?= /tmp/sonar-uno-build
PYTHON := .venv/bin/python

.PHONY: setup run demo compile upload preview
setup:
	uv venv --python 3.12 .venv
	uv pip install --python $(PYTHON) -r requirements.txt
run:
	$(PYTHON) radar.py --port $(PORT)
demo:
	$(PYTHON) radar.py --demo
compile:
	arduino-cli compile --fqbn $(FQBN) --build-path $(BUILD) .
upload: compile
	arduino-cli upload --port $(PORT) --fqbn $(FQBN) --input-dir $(BUILD) --verify .
preview:
	SDL_VIDEODRIVER=dummy $(PYTHON) radar.py --demo --screenshot preview.png
