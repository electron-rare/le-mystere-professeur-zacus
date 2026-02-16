# UML Sequences

## Boot + Story flow (high level)

```
UI -> ESP32: HELLO
ESP32 -> UI: ACK
ESP32 -> UI: KEYFRAME (full state snapshot)
StoryEngine -> AudioService: startBaseFs
User -> UI: BTN OK
UI -> ESP32: BTN OK
StoryEngine: transition step
ESP32 -> UI: STAT (new app/ui_page)
```

## UI reconnection flow

```
ESP32 -> UI: PING
UI -> ESP32: PONG
UI: timeout -> link down
ESP32: timeout -> connected=false
UI reconnects -> HELLO
ESP32 -> UI: ACK
ESP32 -> UI: KEYFRAME (full state)
```

## ETAPE2 timer flow

```
UNLOCK event -> StoryControllerV2
StoryEngineV2 transition to STEP_WIN
Audio done -> StoryEngineV2 transition to STEP_WAIT_ETAPE2
Controller arms ETAPE2 timer on STEP_WAIT_ETAPE2
Timer event ETAPE2_DUE -> StoryEngineV2 transition to STEP_ETAPE2
mp3GateOpen follows the step definition (default stays closed until DONE)
```
