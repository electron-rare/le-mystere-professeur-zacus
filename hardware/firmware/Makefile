PIO ?= pio
ESP32_ENV ?= esp32dev
UI_OLED_ENV ?= esp8266_oled
UI_TFT_ENV ?= ui_rp2040_ili9488

ESP32_PORT ?=
UI_OLED_PORT ?=
UI_TFT_PORT ?=

.PHONY: fast-esp32 fast-ui-oled fast-ui-tft fast-esp32-build fast-ui-oled-build fast-ui-tft-build

fast-esp32-build:
	$(PIO) run -e $(ESP32_ENV)

fast-ui-oled-build:
	$(PIO) run -e $(UI_OLED_ENV)

fast-ui-tft-build:
	$(PIO) run -e $(UI_TFT_ENV)

fast-esp32: fast-esp32-build
	@if [ -z "$(ESP32_PORT)" ]; then echo "ESP32_PORT is required"; exit 1; fi
	$(PIO) run -e $(ESP32_ENV) -t upload --upload-port "$(ESP32_PORT)"
	$(PIO) device monitor -e $(ESP32_ENV) --port "$(ESP32_PORT)"

fast-ui-oled: fast-ui-oled-build
	@if [ -z "$(UI_OLED_PORT)" ]; then echo "UI_OLED_PORT is required"; exit 1; fi
	$(PIO) run -e $(UI_OLED_ENV) -t upload --upload-port "$(UI_OLED_PORT)"
	$(PIO) device monitor -e $(UI_OLED_ENV) --port "$(UI_OLED_PORT)"

fast-ui-tft: fast-ui-tft-build
	@if [ -z "$(UI_TFT_PORT)" ]; then echo "UI_TFT_PORT is required"; exit 1; fi
	$(PIO) run -e $(UI_TFT_ENV) -t upload --upload-port "$(UI_TFT_PORT)"
	$(PIO) device monitor -e $(UI_TFT_ENV) --port "$(UI_TFT_PORT)"
