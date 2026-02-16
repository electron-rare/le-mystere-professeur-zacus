# UML Controllers

Controllers orchestrate high-level modes and translate inputs to services.

## Main classes

```
StoryControllerV2
  + begin(nowMs)
  + setScenario(id)
  + update(nowMs)
  + onUnlock(nowMs)
  + jumpToStep(stepId)
  + postSerialEvent(name)
  + setTestMode(enabled)
  + setTestDelayMs(ms)

Mp3Controller
  + begin()
  + update(nowMs)
  + play/pause/stop
  + next/prev
  + setVolume

InputController
  + processButtonEvent
  + processTouchEvent
  + routeToActiveController

BootProtocolRuntime
  + runBootSequence
  + diagnostics
```

## Responsibilities

- StoryControllerV2: story scenario selection, event flow, ETAPE2 timer, and wiring StoryAppHost to audio/screen/gate/LA apps.
- Mp3Controller: SD playback UX.
- InputController: input routing to active mode.
- BootProtocolRuntime: boot and hardware checks.
