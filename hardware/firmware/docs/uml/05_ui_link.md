# UML UI Link v2

UI Link v2 is a frame-based UART protocol between ESP32 and UI MCUs.

## Key classes

```
UiLink
  + begin
  + poll(nowMs)
  + update(frame, forceKeyframe)
  + consumeInputEvent
  + connected

ScreenFrame
  + mode
  + seq
  + ms
  + track
  + trackTotal
  + volume
  + tuneOffset
  + tuneConfidence
  + startup
  + app
  + ui_page
```

## Frame format

```
<TYPE>,k=v,k=v*CC\n
```

## Message types

- HELLO / ACK
- KEYFRAME / STAT
- BTN / TOUCH
- PING / PONG

## Notes

- HEARTBEAT_MS = 1000
- TIMEOUT_MS = 1500
- CRC8 required on each frame
