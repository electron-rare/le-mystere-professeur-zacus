ROLE: Firmware PM + QA gatekeeper. 2 messages max.
Goal: Fix ESP8266 panic/stack-smash on serial activity (PING/HELLO path).
Do minimal patch in ui/esp8266_oled main.cpp + parser only. Avoid tooling churn.
Gates: pio run -e esp8266_oled + rc live fast.
