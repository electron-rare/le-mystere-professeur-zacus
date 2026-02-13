PIO ?= pio
ESP32_ENV ?= esp32dev
SCREEN_ENV ?= esp8266_oled
ESP32_PORT ?=
SCREEN_PORT ?=

.PHONY: help build build-esp32 build-screen story-validate story-gen upload-esp32 upload-screen uploadfs-esp32 erasefs-esp32 monitor-esp32 monitor-screen clean

help:
	@echo "Targets:"
	@echo "  make build            # Build ESP32 + ESP8266 firmware"
	@echo "  make build-esp32      # Build ESP32 firmware only"
	@echo "  make build-screen     # Build ESP8266 OLED firmware only"
	@echo "  make upload-esp32     # Flash ESP32 (optional ESP32_PORT=/dev/ttyUSB0)"
	@echo "  make uploadfs-esp32   # Flash LittleFS image to ESP32"
	@echo "  make erasefs-esp32    # Erase LittleFS partition on ESP32"
	@echo "  make story-validate   # Validate YAML scenarios (StorySpec V1)"
	@echo "  make story-gen        # Generate C++ story sources from YAML"
	@echo "  make upload-screen    # Flash ESP8266 (optional SCREEN_PORT=/dev/ttyUSB1)"
	@echo "  make monitor-esp32    # Serial monitor ESP32 (optional ESP32_PORT=...)"
	@echo "  make monitor-screen   # Serial monitor ESP8266 (optional SCREEN_PORT=...)"
	@echo "  make clean            # Clean both environments"

build: build-esp32 build-screen

story-validate:
	python3 tools/story_gen/story_gen.py validate --spec-dir story_specs/scenarios

story-gen: story-validate
	python3 tools/story_gen/story_gen.py generate --spec-dir story_specs/scenarios --out-dir src/story/generated

build-esp32:
	$(PIO) run -e $(ESP32_ENV)

build-screen:
	$(PIO) run -e $(SCREEN_ENV)

upload-esp32:
	@if [ -n "$(ESP32_PORT)" ]; then \
		$(PIO) run -e $(ESP32_ENV) -t upload --upload-port "$(ESP32_PORT)"; \
	else \
		$(PIO) run -e $(ESP32_ENV) -t upload; \
	fi

uploadfs-esp32:
	@if [ -n "$(ESP32_PORT)" ]; then \
		$(PIO) run -e $(ESP32_ENV) -t uploadfs --upload-port "$(ESP32_PORT)"; \
	else \
		$(PIO) run -e $(ESP32_ENV) -t uploadfs; \
	fi

erasefs-esp32:
	@if [ -n "$(ESP32_PORT)" ]; then \
		$(PIO) run -e $(ESP32_ENV) -t erasefs --upload-port "$(ESP32_PORT)"; \
	else \
		$(PIO) run -e $(ESP32_ENV) -t erasefs; \
	fi

upload-screen:
	@if [ -n "$(SCREEN_PORT)" ]; then \
		$(PIO) run -e $(SCREEN_ENV) -t upload --upload-port "$(SCREEN_PORT)"; \
	else \
		$(PIO) run -e $(SCREEN_ENV) -t upload; \
	fi

monitor-esp32:
	@if [ -n "$(ESP32_PORT)" ]; then \
		$(PIO) device monitor -e $(ESP32_ENV) --port "$(ESP32_PORT)"; \
	else \
		$(PIO) device monitor -e $(ESP32_ENV); \
	fi

monitor-screen:
	@if [ -n "$(SCREEN_PORT)" ]; then \
		$(PIO) device monitor -e $(SCREEN_ENV) --port "$(SCREEN_PORT)"; \
	else \
		$(PIO) device monitor -e $(SCREEN_ENV); \
	fi

clean:
	$(PIO) run -e $(ESP32_ENV) -t clean
	$(PIO) run -e $(SCREEN_ENV) -t clean
