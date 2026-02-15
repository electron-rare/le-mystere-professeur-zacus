ROLE: Firmware PM + QA gatekeeper. 2 messages max.
Goal: Make UI_LINK_STATUS gate pass (connected=1) reliably.
Check baud/pins, handshake state, and ESP32 UiLink runtime.
Gates: pio run -e esp32dev + rc live fast + ui_link log.
