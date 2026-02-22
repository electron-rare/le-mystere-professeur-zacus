PIO ?= pio
ESP32_ENV ?= esp32dev
UI_OLED_ENV ?= esp8266_oled
UI_TFT_ENV ?= ui_rp2040_ili9488
FREENOVE_ENV ?= freenove_esp32s3_full_with_ui

ESP32_PORT ?=
UI_OLED_PORT ?=
UI_TFT_PORT ?=
FREENOVE_PORT ?=

# FAST_MONITOR=1: always open monitor
# FAST_MONITOR=0: never open monitor (useful for CI/non-interactive loops)
# FAST_MONITOR=auto (default): open monitor only on interactive terminal
FAST_MONITOR ?= auto

.PHONY: fast-esp32 fast-ui-oled fast-ui-tft fast-freenove fast-esp32-build fast-ui-oled-build fast-ui-tft-build fast-freenove-build

fast-esp32-build:
	$(PIO) run -e $(ESP32_ENV)

fast-ui-oled-build:
	$(PIO) run -e $(UI_OLED_ENV)

fast-ui-tft-build:
	$(PIO) run -e $(UI_TFT_ENV)

fast-freenove-build:
	$(PIO) run -e $(FREENOVE_ENV)

fast-esp32: fast-esp32-build
	@if [ -z "$(ESP32_PORT)" ]; then echo "ESP32_PORT is required"; exit 1; fi
	$(PIO) run -e $(ESP32_ENV) -t upload --upload-port "$(ESP32_PORT)"
	@if [ "$(FAST_MONITOR)" = "1" ] || { [ "$(FAST_MONITOR)" = "auto" ] && [ -t 0 ]; }; then \
		$(PIO) device monitor -e $(ESP32_ENV) --port "$(ESP32_PORT)"; \
	else \
		echo "Skipping monitor (FAST_MONITOR=$(FAST_MONITOR))"; \
	fi

fast-ui-oled: fast-ui-oled-build
	@if [ -z "$(UI_OLED_PORT)" ]; then echo "UI_OLED_PORT is required"; exit 1; fi
	$(PIO) run -e $(UI_OLED_ENV) -t upload --upload-port "$(UI_OLED_PORT)"
	@if [ "$(FAST_MONITOR)" = "1" ] || { [ "$(FAST_MONITOR)" = "auto" ] && [ -t 0 ]; }; then \
		$(PIO) device monitor -e $(UI_OLED_ENV) --port "$(UI_OLED_PORT)"; \
	else \
		echo "Skipping monitor (FAST_MONITOR=$(FAST_MONITOR))"; \
	fi

fast-ui-tft: fast-ui-tft-build
	@if [ -z "$(UI_TFT_PORT)" ]; then echo "UI_TFT_PORT is required"; exit 1; fi
	$(PIO) run -e $(UI_TFT_ENV) -t upload --upload-port "$(UI_TFT_PORT)"
	@if [ "$(FAST_MONITOR)" = "1" ] || { [ "$(FAST_MONITOR)" = "auto" ] && [ -t 0 ]; }; then \
		$(PIO) device monitor -e $(UI_TFT_ENV) --port "$(UI_TFT_PORT)"; \
	else \
		echo "Skipping monitor (FAST_MONITOR=$(FAST_MONITOR))"; \
	fi

fast-freenove: fast-freenove-build
	@if [ -z "$(FREENOVE_PORT)" ]; then echo "FREENOVE_PORT is required"; exit 1; fi
	$(PIO) run -e $(FREENOVE_ENV) -t upload --upload-port "$(FREENOVE_PORT)"
	@if [ "$(FAST_MONITOR)" = "1" ] || { [ "$(FAST_MONITOR)" = "auto" ] && [ -t 0 ]; }; then \
		$(PIO) device monitor -e $(FREENOVE_ENV) --port "$(FREENOVE_PORT)"; \
	else \
		echo "Skipping monitor (FAST_MONITOR=$(FAST_MONITOR))"; \
	fi
