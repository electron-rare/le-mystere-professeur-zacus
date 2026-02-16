# UML Services

Services isolate business logic and hardware access.

## Major services

```
AudioService
  + startBaseFs
  + startBaseFx
  + startOverlayFx
  + stopBase/stopOverlay
  + update(nowMs)

RadioService
  + startStation
  + stop
  + update

WifiService
  + connect
  + startAP
  + scan

WebUiService
  + begin
  + registerHandlers

InputService
  + update
  + pollKeypad

ScreenSyncService
  + begin
  + update(nowMs, frame)
  + poll(nowMs)

SerialRouter
  + update
  + handleCommand

CatalogScanService
  + scanSD

LaDetectorRuntimeService
  + update
  + isLaDetected
```

## Notes

- Services are called from AppOrchestrator or Controllers.
- Most services are designed for non-blocking update loops.
