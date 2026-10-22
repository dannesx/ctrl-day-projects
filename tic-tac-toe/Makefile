FQBN ?= esp32:esp32:esp32
PORT ?= /dev/ttyUSB0
BUILD ?= /tmp/esp32-tic-tac-toe-build

.PHONY: compile upload monitor test
compile:
	arduino-cli compile --fqbn $(FQBN) --build-path $(BUILD) .
upload: compile
	arduino-cli upload --port $(PORT) --fqbn $(FQBN) --input-dir $(BUILD) .
monitor:
	arduino-cli monitor --port $(PORT) --config baudrate=115200

test:
	node tests/polling.cjs
	$(CXX) -O2 -I . tests/learning.cpp -o /tmp/tic-tac-toe-learning-test
	/tmp/tic-tac-toe-learning-test
