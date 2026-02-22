# Architecture UML du Firmware

Date: 15 février 2026  
Branche de référence: `main`  
Version: v1.0.0-draft

NOTE: A more readable, split UML documentation lives in docs/uml/INDEX.md.

## Vue d'ensemble système

Le firmware suit une **architecture multi-MCU** avec 3 firmwares indépendants communiquant via protocole UART :

```
┌─────────────────────────────────────────────────────────────┐
│                    ESP32 Audio Kit                          │
│              (Firmware principal C++)                       │
│  ┌──────────────────────────────────────────────────────┐  │
│  │ App → AppOrchestrator → Controllers → Services       │  │
│  └──────────────────────────────────────────────────────┘  │
└────────────────────┬───────────────────┬────────────────────┘
                     │ UI Link v2        │ UI Link v2
                     │ UART 57600        │ UART 57600
        ┌────────────┴────────┐   ┌──────┴─────────┐
        │   ESP8266 OLED      │   │  RP2040 TFT    │
        │  (UI légère C++)    │   │ (LVGL tactile) │
        └─────────────────────┘   └────────────────┘
```

### Caractéristiques techniques

| MCU | Role | Plateforme | Framework | Baud UART |
|-----|------|------------|-----------|-----------|
| ESP32 | Audio + logique | Espressif32 v6.12.0 | Arduino | 115200 (USB)<br>57600 (UI) |
| ESP8266 | UI OLED | Espressif8266 v4.2.1 | Arduino | 57600 (UI) |
| RP2040 | UI TFT tactile | RaspberryPi custom | Arduino | 57600 (UI) |

## Architecture ESP32 (firmware principal)

### Diagramme de classes - Couche Application

```
┌───────────────────────────────────────────────────────────────┐
│                         MAIN LOOP                              │
│  ┌──────┐                                                      │
│  │ App  │──setup()─→ app_orchestrator::setup()               │
│  │      │──loop()──→ app_orchestrator::loop()                │
│  └──────┘                                                      │
└───────────────────────────────────────────────────────────────┘
                              ↓
┌───────────────────────────────────────────────────────────────┐
│                    APP ORCHESTRATOR                            │
│  Coordonne l'initialisation et la boucle principale            │
│  ┌──────────────────────────────────────────────────────────┐ │
│  │ • Boot sequence (Serial, WiFi, Codec, I2C, SD)           │ │
│  │ • Runtime mode selection:                                 │ │
│  │   - STORY_V2 (mode quête, défaut)                        │ │
│  │   - MP3_PLAYER (lecteur SD)                               │ │
│  │   - RADIO (streaming web)                                 │ │
│  │ • Periodic service updates (audio, input, screen, web)   │ │
│  └──────────────────────────────────────────────────────────┘ │
└───────────────────────────────────────────────────────────────┘
```

### Diagramme de classes - Couche Controllers

```
┌───────────────────────────────────────────────────────────────┐
│                       CONTROLLERS LAYER                        │
│  Orchestrent les modes de fonctionnement                       │
├───────────────────────────────────────────────────────────────┤
│                                                                 │
│  ┌─────────────────────────────────────────────────────────┐  │
│  │ StoryControllerV2                                        │  │
│  ├──────────────────────────────────────────────────────────┤ │
│  │ + begin(nowMs): bool                                     │  │
│  │ + setScenario(scenarioId, nowMs, source): bool          │  │
│  │ + reset(nowMs, source): void                             │  │
│  │ + onUnlock(nowMs, source): void                          │  │
│  │ + update(nowMs): void                                    │  │
│  │ + postSerialEvent(eventName, nowMs, source): bool       │  │
│  │ + jumpToStep(stepId, nowMs, source): bool               │  │
│  │ + isMp3GateOpen(): bool                                  │  │
│  │ + forceEtape2DueNow(nowMs, source): void                │  │
│  │ + setTestMode(enabled, nowMs, source): void             │  │
│  ├──────────────────────────────────────────────────────────┤ │
│  │ - engine_: StoryEngineV2                                 │  │
│  │ - appHost_: StoryAppHost                                 │  │
│  │ - audio_: AudioService&                                  │  │
│  │ - hooks_: Hooks (callbacks actions)                      │  │
│  │ - etape2Timer_: uint32_t                                 │  │
│  │ - mp3GateOpen_: bool                                     │  │
│  │ - testMode_: bool                                        │  │
│  └──────────────────────────────────────────────────────────┘  │
│                                                                 │
│  ┌─────────────────────────────────────────────────────────┐  │
│  │ Mp3Controller                                            │  │
│  ├──────────────────────────────────────────────────────────┤ │
│  │ + begin(): bool                                          │  │
│  │ + update(nowMs): void                                    │  │
│  │ + play(), pause(), stop(): void                          │  │
│  │ + next(), prev(): void                                   │  │
│  │ + setVolume(vol): void                                   │  │
│  ├──────────────────────────────────────────────────────────┤ │
│  │ - player_: Mp3Player&                                    │  │
│  │ - catalog_: TrackCatalog                                 │  │
│  │ - currentTrack_: uint16_t                                │  │
│  │ - mode_: PlayMode (NORMAL/SHUFFLE/REPEAT)               │  │
│  └──────────────────────────────────────────────────────────┘  │
│                                                                 │
│  ┌─────────────────────────────────────────────────────────┐  │
│  │ InputController                                          │  │
│  ├──────────────────────────────────────────────────────────┤ │
│  │ + processButtonEvent(event): void                        │  │
│  │ + processTouchEvent(event): void                         │  │
│  │ + routeToActiveController(): void                        │  │
│  ├──────────────────────────────────────────────────────────┤ │
│  │ - inputService_: InputService&                           │  │
│  │ - storyCtrl_: StoryControllerV2*                         │  │
│  │ - mp3Ctrl_: Mp3Controller*                               │  │
│  └──────────────────────────────────────────────────────────┘  │
│                                                                 │
│  ┌─────────────────────────────────────────────────────────┐  │
│  │ BootProtocolRuntime                                      │  │
│  ├──────────────────────────────────────────────────────────┤ │
│  │ + runBootSequence(): bool                                │  │
│  │ + diagnostics(): void                                    │  │
│  └──────────────────────────────────────────────────────────┘  │
│                                                                 │
└───────────────────────────────────────────────────────────────┘
```

### Diagramme de classes - Couche Services

```
┌───────────────────────────────────────────────────────────────┐
│                         SERVICES LAYER                         │
│  Services métier indépendants et réutilisables                 │
├───────────────────────────────────────────────────────────────┤
│                                                                 │
│  ┌─────────────────────────────────────────────────────────┐  │
│  │ AudioService                                             │  │
│  ├──────────────────────────────────────────────────────────┤ │
│  │ + startBaseFs(fs, path, gain, timeout, tag): bool       │  │
│  │ + startBaseFx(effect, gain, duration, tag): bool        │  │
│  │ + startOverlayFx(effect, gain, duration, tag): bool     │  │
│  │ + stopBase(reason), stopOverlay(reason): void           │  │
│  │ + update(nowMs): void                                    │  │
│  │ + isBaseBusy(), isOverlayBusy(): bool                   │  │
│  ├──────────────────────────────────────────────────────────┤ │
│  │ - baseAsync_: AsyncAudioService& (MP3)                  │  │
│  │ - baseFx_: FmRadioScanFx& (effets I2S)                  │  │
│  │ - mp3_: Mp3Player&                                       │  │
│  │ - base_: ChannelSnapshot                                │  │
│  │ - overlay_: ChannelSnapshot                             │  │
│  └──────────────────────────────────────────────────────────┘  │
│                                                                 │
│  ┌─────────────────────────────────────────────────────────┐  │
│  │ RadioService                                             │  │
│  ├──────────────────────────────────────────────────────────┤ │
│  │ + startStation(stationId): bool                          │  │
│  │ + stop(): void                                           │  │
│  │ + update(nowMs): void                                    │  │
│  ├──────────────────────────────────────────────────────────┤ │
│  │ - pipeline_: StreamPipeline (HTTP→I2S)                  │  │
│  │ - stations_: StationRepository                           │  │
│  └──────────────────────────────────────────────────────────┘  │
│                                                                 │
│  ┌─────────────────────────────────────────────────────────┐  │
│  │ WifiService                                              │  │
│  ├──────────────────────────────────────────────────────────┤ │
│  │ + connect(ssid, password): bool                          │  │
│  │ + startAP(ssid): bool                                    │  │
│  │ + scan(): WifiNetwork[]                                  │  │
│  │ + isConnected(): bool                                    │  │
│  └──────────────────────────────────────────────────────────┘  │
│                                                                 │
│  ┌─────────────────────────────────────────────────────────┐  │
│  │ WebUiService                                             │  │
│  ├──────────────────────────────────────────────────────────┤ │
│  │ + begin(): bool                                          │  │
│  │ + registerHandlers(): void                               │  │
│  ├──────────────────────────────────────────────────────────┤ │
│  │ - server_: AsyncWebServer*                               │  │
│  │ - radioSvc_: RadioService&                               │  │
│  │ - mp3Player_: Mp3Player&                                 │  │
│  └──────────────────────────────────────────────────────────┘  │
│                                                                 │
│  ┌─────────────────────────────────────────────────────────┐  │
│  │ InputService                                             │  │
│  ├──────────────────────────────────────────────────────────┤ │
│  │ + update(nowMs): void                                    │  │
│  │ + pollKeypad(): void                                     │  │
│  ├──────────────────────────────────────────────────────────┤ │
│  │ - keypad_: KeypadAnalog                                  │  │
│  │ - router_: InputRouter (event queue)                     │  │
│  └──────────────────────────────────────────────────────────┘  │
│                                                                 │
│  ┌─────────────────────────────────────────────────────────┐  │
│  │ ScreenSyncService                                        │  │
│  ├──────────────────────────────────────────────────────────┤ │
│  │ + begin(): void                                          │  │
│  │ + update(nowMs, frame): void                             │  │
│  │ + poll(nowMs): void                                      │  │
│  ├──────────────────────────────────────────────────────────┤ │
│  │ - uiLink_: UiLink (UART protocol)                        │  │
│  │ - frame_: ScreenFrame (état sérialisé)                   │  │
│  └──────────────────────────────────────────────────────────┘  │
│                                                                 │
│  ┌─────────────────────────────────────────────────────────┐  │
│  │ SerialRouter                                             │  │
│  ├──────────────────────────────────────────────────────────┤ │
│  │ + update(): void                                         │  │
│  │ + handleCommand(cmd, args): void                         │  │
│  ├──────────────────────────────────────────────────────────┤ │
│  │ - commands_: CommandRegistry                             │  │
│  └──────────────────────────────────────────────────────────┘  │
│                                                                 │
│  ┌─────────────────────────────────────────────────────────┐  │
│  │ CatalogScanService                                       │  │
│  ├──────────────────────────────────────────────────────────┤ │
│  │ + scanSD(fs): TrackCatalog                               │  │
│  └──────────────────────────────────────────────────────────┘  │
│                                                                 │
│  ┌─────────────────────────────────────────────────────────┐  │
│  │ LaDetectorRuntimeService                                 │  │
│  ├──────────────────────────────────────────────────────────┤ │
│  │ + update(nowMs): void                                    │  │
│  │ + isLaDetected(): bool                                   │  │
│  │ - detector_: LaDetector (440Hz ADC)                      │  │
│  └──────────────────────────────────────────────────────────┘  │
│                                                                 │
└───────────────────────────────────────────────────────────────┘
```

### Diagramme de classes - Story Engine V2

```
┌───────────────────────────────────────────────────────────────┐
│                      STORY ENGINE V2                           │
│  Machine à états pour quêtes audio interactives                │
├───────────────────────────────────────────────────────────────┤
│                                                                 │
│  ┌─────────────────────────────────────────────────────────┐  │
│  │ StoryEngineV2                                            │  │
│  ├──────────────────────────────────────────────────────────┤ │
│  │ + loadScenario(scenario): bool                           │  │
│  │ + start(scenarioId, nowMs): bool                         │  │
│  │ + stop(reason): void                                     │  │
│  │ + update(nowMs): void                                    │  │
│  │ + postEvent(event): bool                                 │  │
│  │ + jumpToStep(stepId, reason, nowMs): bool               │  │
│  │ + snapshot(): StorySnapshot                              │  │
│  │ + scenario(): const ScenarioDef*                         │  │
│  │ + currentStep(): const StepDef*                          │  │
│  │ + consumeStepChanged(): bool                             │  │
│  │ + lastError(): const char*                               │  │
│  │ + droppedEvents(): uint32_t                              │  │
│  ├──────────────────────────────────────────────────────────┤ │
│  │ - scenario_: const ScenarioDef*                          │  │
│  │ - queue_: StoryEventQueue                                │  │
│  │ - currentStepIndex_: uint8_t                             │  │
│  │ - previousStepIndex_: uint8_t                            │  │
│  │ - running_: bool                                         │  │
│  │ - stepChanged_: bool                                     │  │
│  │ - enteredAtMs_: uint32_t                                 │  │
│  │ - lastError_[32]: char                                   │  │
│  └──────────────────────────────────────────────────────────┘  │
│                        uses ↓                                  │
│  ┌─────────────────────────────────────────────────────────┐  │
│  │ ScenarioDef                                              │  │
│  ├──────────────────────────────────────────────────────────┤ │
│  │ + id: const char*                                        │  │
│  │ + version: uint16_t                                      │  │
│  │ + steps: StepDef[] (tableau de steps)                   │  │
│  │ + stepCount: uint8_t                                     │  │
│  │ + initialStepId: const char*                             │  │
│  └──────────────────────────────────────────────────────────┘  │
│                        uses ↓                                  │
│  ┌─────────────────────────────────────────────────────────┐  │
│  │ StepDef                                                  │  │
│  ├──────────────────────────────────────────────────────────┤ │
│  │ + id: const char*                                        │  │
│  │ + resources: ResourceBindings                            │  │
│  │ + transitions: TransitionDef[] (event→next step)        │  │
│  │ + transitionCount: uint8_t                               │  │
│  │ + mp3GateOpen: bool                                      │  │
│  └──────────────────────────────────────────────────────────┘  │
│                        uses ↓                                  │
│  ┌─────────────────────────────────────────────────────────┐  │
│  │ ResourceBindings                                         │  │
│  ├──────────────────────────────────────────────────────────┤ │
│  │ + screenSceneId: const char*                             │  │
│  │ + audioPackId: const char*                               │  │
│  │ + actionIds: const char* const*                          │  │
│  │ + actionCount: uint8_t                                   │  │
│  │ + appIds: const char* const*                             │  │
│  │ + appCount: uint8_t                                      │  │
│  └──────────────────────────────────────────────────────────┘  │
│                        uses ↓                                  │
│  ┌─────────────────────────────────────────────────────────┐  │
│  │ TransitionDef                                             │  │
│  ├──────────────────────────────────────────────────────────┤ │
│  │ + id: const char*                                        │  │
│  │ + trigger: StoryTransitionTrigger                        │  │
│  │ + eventType: StoryEventType                              │  │
│  │ + eventName: const char*                                 │  │
│  │ + afterMs: uint32_t                                      │  │
│  │ + targetStepId: const char*                              │  │
│  │ + priority: uint8_t                                      │  │
│  └──────────────────────────────────────────────────────────┘  │
│                                                                 │
│  ┌─────────────────────────────────────────────────────────┐  │
│  │ StoryEvent                                                │  │
│  ├──────────────────────────────────────────────────────────┤ │
│  │ + type: StoryEventType                                   │  │
│  │ + name: char[24]                                         │  │
│  │ + value: int32_t                                         │  │
│  │ + atMs: uint32_t                                         │  │
│  └──────────────────────────────────────────────────────────┘  │
│                                                                 │
│  ┌─────────────────────────────────────────────────────────┐  │
│  │ StoryEventQueue                                           │  │
│  ├──────────────────────────────────────────────────────────┤ │
│  │ + push(event): bool                                      │  │
│  │ + pop(outEvent): bool                                    │  │
│  │ + size(): uint8_t                                        │  │
│  │ + droppedCount(): uint32_t                               │  │
│  ├──────────────────────────────────────────────────────────┤ │
│  │ - data_: StoryEvent[kCapacity=12]                        │  │
│  │ - head_/tail_/size_: uint8_t                             │  │
│  │ - dropped_: uint32_t                                     │  │
│  └──────────────────────────────────────────────────────────┘  │
│                        uses ↓                                  │
│  ┌─────────────────────────────────────────────────────────┐  │
│  │ StoryAppHost                                              │  │
│  ├──────────────────────────────────────────────────────────┤ │
│  │ + begin(context): bool                                   │  │
│  │ + stopAll(reason): void                                  │  │
│  │ + startStep(scenario, step, nowMs, source): bool         │  │
│  │ + update(nowMs, sink): void                              │  │
│  │ + handleEvent(event, sink): void                         │  │
│  │ + activeScreenSceneId(): const char*                     │  │
│  │ + validateScenario(scenario, validation): bool           │  │
│  │ + lastError(): const char*                               │  │
│  ├──────────────────────────────────────────────────────────┤ │
│  │ - activeApps_[6]: StoryApp*                              │  │
│  │ - laDetectorApp_: LaDetectorApp                          │  │
│  │ - audioPackApp_: AudioPackApp                            │  │
│  │ - screenSceneApp_: ScreenSceneApp                        │  │
│  │ - mp3GateApp_: Mp3GateApp                                │  │
│  └──────────────────────────────────────────────────────────┘  │
│                        ↑ extends                               │
│  ┌──────────────┬──────────────┬──────────────┬──────────┐   │
│  │ LaDetectorApp│ Mp3GateApp   │ScreenSceneApp│AudioPackApp│ │
│  │              │              │              │          │   │
│  │ Détection LA │ Gate MP3     │ Scene screen │ Audio FX │   │
│  │ 440Hz ADC    │ selon step   │ selon step   │ pack     │   │
│  └──────────────┴──────────────┴──────────────┴──────────┘   │
│                                                                 │
│  ┌─────────────────────────────────────────────────────────┐  │
│  │ <<abstract>> StoryApp                                    │  │
│  ├──────────────────────────────────────────────────────────┤ │
│  │ + begin(context): bool                                   │  │
│  │ + start(stepContext): void                               │  │
│  │ + update(nowMs, sink): void                              │  │
│  │ + stop(reason): void                                     │  │
│  │ + handleEvent(event, sink): bool                         │  │
│  │ + snapshot(): StoryAppSnapshot                           │  │
│  └──────────────────────────────────────────────────────────┘  │
│                                                                 │
│  ┌─────────────────────────────────────────────────────────┐  │
│  │ StoryAppContext                                          │  │
│  ├──────────────────────────────────────────────────────────┤ │
│  │ + audioService: AudioService*                            │  │
│  │ + startRandomTokenBase(): bool                           │  │
│  │ + startFallbackBaseFx(): bool                            │  │
│  │ + applyAction(action, nowMs, source): void               │  │
│  │ + laRuntime: LaDetectorRuntimeService*                   │  │
│  │ + onUnlockRuntimeApplied(nowMs, source): void            │  │
│  └──────────────────────────────────────────────────────────┘  │
│                                                                 │
│  ┌─────────────────────────────────────────────────────────┐  │
│  │ StoryEventSink                                           │  │
│  ├──────────────────────────────────────────────────────────┤ │
│  │ + post(event): bool                                      │  │
│  │ + emit(type, name, value, atMs): bool                    │  │
│  │ - postFn: bool (*)(const StoryEvent&, void*)             │  │
│  │ - user: void*                                            │  │
│  └──────────────────────────────────────────────────────────┘  │
│                                                                 │
│  ┌─────────────────────────────────────────────────────────┐  │
│  │ StoryActionDef                                           │  │
│  ├──────────────────────────────────────────────────────────┤ │
│  │ + id: const char*                                        │  │
│  │ + type: StoryActionType (TRACE/QUEUE_SONAR/REFRESH_SD)   │  │
│  │ + value: int32_t                                         │  │
│  └──────────────────────────────────────────────────────────┘  │
│                                                                 │
└───────────────────────────────────────────────────────────────┘
```

### Diagramme de classes - Audio Subsystem

```
┌───────────────────────────────────────────────────────────────┐
│                      AUDIO SUBSYSTEM                           │
│  Stack audio I2S (ES8388 codec)                               │
├───────────────────────────────────────────────────────────────┤
│                                                                 │
│  ┌─────────────────────────────────────────────────────────┐  │
│  │ AsyncAudioService                                        │  │
│  ├──────────────────────────────────────────────────────────┤ │
│  │ + startFile(fs, path, gain, timeout): bool              │  │
│  │ + stop(): void                                           │  │
│  │ + update(nowMs): void                                    │  │
│  │ + isPlaying(): bool                                      │  │
│  │ + result(): Result (DONE/TIMEOUT/FAILED)                │  │
│  ├──────────────────────────────────────────────────────────┤ │
│  │ - source_: AudioFileSourceFS* (SD/LittleFS)             │  │
│  │ - generator_: AudioGenerator* (MP3 decoder)             │  │
│  │ - output_: AudioOutputI2S* (ES8388 I2S)                 │  │
│  │ - deadlineMs_: uint32_t                                  │  │
│  └──────────────────────────────────────────────────────────┘  │
│            uses ESP8266Audio lib ↓                             │
│                                                                 │
│  ┌─────────────────────────────────────────────────────────┐  │
│  │ FmRadioScanFx                                            │  │
│  ├──────────────────────────────────────────────────────────┤ │
│  │ + startSweep(durationMs, gain): bool                     │  │
│  │ + startNoise(durationMs, gain): bool                     │  │
│  │ + startBeep(freq, durationMs, gain): bool               │  │
│  │ + stop(): void                                           │  │
│  │ + update(nowMs): void                                    │  │
│  ├──────────────────────────────────────────────────────────┤ │
│  │ - i2sStream_: I2SStream* (AudioTools)                   │  │
│  │ - generator_: SineWaveGenerator* (AudioTools)           │  │
│  │ - deadlineMs_: uint32_t                                  │  │
│  └──────────────────────────────────────────────────────────┘  │
│            uses arduino-audio-tools ↓                          │
│                                                                 │
│  ┌─────────────────────────────────────────────────────────┐  │
│  │ Mp3Player                                                │  │
│  ├──────────────────────────────────────────────────────────┤ │
│  │ + begin(fs): bool                                        │  │
│  │ + play(), pause(), stop(): void                          │  │
│  │ + next(), prev(): void                                   │  │
│  │ + setVolume(vol): void                                   │  │
│  │ + update(): void                                         │  │
│  ├──────────────────────────────────────────────────────────┤ │
│  │ - fs_: fs::FS&                                           │  │
│  │ - catalog_: TrackCatalog&                                │  │
│  │ - currentIndex_: uint16_t                                │  │
│  │ - playing_: bool                                         │  │
│  └──────────────────────────────────────────────────────────┘  │
│                                                                 │
│  ┌─────────────────────────────────────────────────────────┐  │
│  │ I2sJinglePlayer                                          │  │
│  ├──────────────────────────────────────────────────────────┤ │
│  │ + playRTTTL(melody): bool                                │  │
│  │ + stop(): void                                           │  │
│  │ + update(): void                                         │  │
│  ├──────────────────────────────────────────────────────────┤ │
│  │ - source_: AudioFileSourcePROGMEM*                       │  │
│  │ - generator_: AudioGeneratorRTTTL*                       │  │
│  │ - output_: AudioOutputI2S*                               │  │
│  └──────────────────────────────────────────────────────────┘  │
│                                                                 │
│  ┌─────────────────────────────────────────────────────────┐  │
│  │ CodecES8388Driver                                        │  │
│  ├──────────────────────────────────────────────────────────┤ │
│  │ + begin(i2cSda, i2cScl, mclk): bool                      │  │
│  │ + setVolume(vol): void                                   │  │
│  │ + mute(), unmute(): void                                 │  │
│  ├──────────────────────────────────────────────────────────┤ │
│  │ - i2c_: TwoWire&                                         │  │
│  │ - address_: uint8_t (0x10)                               │  │
│  └──────────────────────────────────────────────────────────┘  │
│                                                                 │
│  ┌─────────────────────────────────────────────────────────┐  │
│  │ TrackCatalog                                             │  │
│  ├──────────────────────────────────────────────────────────┤ │
│  │ + addTrack(path): void                                   │  │
│  │ + getTrack(index): const char*                           │  │
│  │ + count(): uint16_t                                      │  │
│  │ + clear(): void                                          │  │
│  ├──────────────────────────────────────────────────────────┤ │
│  │ - tracks_: String[256]                                   │  │
│  │ - count_: uint16_t                                       │  │
│  └──────────────────────────────────────────────────────────┘  │
│                                                                 │
└───────────────────────────────────────────────────────────────┘
```

### Diagramme de classes - UI Link Protocol

```
┌───────────────────────────────────────────────────────────────┐
│                       UI LINK PROTOCOL                         │
│  Communication UART vers UI externes                           │
├───────────────────────────────────────────────────────────────┤
│                                                                 │
│  ┌─────────────────────────────────────────────────────────┐  │
│  │ UiLink                                                   │  │
│  ├──────────────────────────────────────────────────────────┤ │
│  │ + begin(): void                                          │  │
│  │ + poll(nowMs): void                                      │  │
│  │ + update(frame, forceKeyframe): bool                     │  │
│  │ + consumeInputEvent(event): bool                         │  │
│  │ + connected(): bool                                      │  │
│  │ + txFrameCount(), rxFrameCount(): uint32_t              │  │
│  ├──────────────────────────────────────────────────────────┤ │
│  │ - serial_: HardwareSerial&                               │  │
│  │ - rxPin_: uint8_t (GPIO19)                               │  │
│  │ - txPin_: uint8_t (GPIO22)                               │  │
│  │ - baud_: uint32_t (57600)                                │  │
│  │ - updatePeriodMs_: uint16_t (250)                        │  │
│  │ - heartbeatMs_: uint16_t (1000)                          │  │
│  │ - timeoutMs_: uint16_t (1500)                            │  │
│  │ - lastTxMs_, lastRxMs_: uint32_t                         │  │
│  │ - connected_: bool                                       │  │
│  │ - sessionCounter_: uint32_t                              │  │
│  │ - inputQueue_: UiLinkInputEvent[8]                       │  │
│  └──────────────────────────────────────────────────────────┘  │
│                        uses ↓                                  │
│  ┌─────────────────────────────────────────────────────────┐  │
│  │ ScreenFrame                                              │  │
│  ├──────────────────────────────────────────────────────────┤ │
│  │ + mode: char[12] (STORY/MP3/RADIO/BOOT/SIGNAL)          │  │
│  │ + seq: uint32_t                                          │  │
│  │ + ms: uint32_t (uptime)                                  │  │
│  │ + track: uint16_t                                        │  │
│  │ + trackTotal: uint16_t                                   │  │
│  │ + volume: uint8_t (0-100)                                │  │
│  │ + tuningOffset: int8_t                                   │  │
│  │ + tuningConfidence: uint8_t                              │  │
│  │ + scene: char[16] (story scene ID)                       │  │
│  │ + hold: uint8_t, key: uint8_t                            │  │
│  └──────────────────────────────────────────────────────────┘  │
│                                                                 │
│  ┌─────────────────────────────────────────────────────────┐  │
│  │ UiLinkInputEvent                                         │  │
│  ├──────────────────────────────────────────────────────────┤ │
│  │ + type: UiLinkInputType (BUTTON/TOUCH)                   │  │
│  │ + btnId: UiBtnId (OK/NEXT/PREV/VOL_UP/VOL_DOWN/MODE)    │  │
│  │ + btnAction: UiBtnAction (PRESS/RELEASE/LONG_PRESS)     │  │
│  │ + touchAction: UiTouchAction (PRESS/RELEASE/MOVE)       │  │
│  │ + x, y: int16_t (touch coords)                           │  │
│  │ + tsMs: uint32_t (timestamp)                             │  │
│  └──────────────────────────────────────────────────────────┘  │
│                                                                 │
└───────────────────────────────────────────────────────────────┘
```

## Architecture UI ESP8266 OLED

```
┌───────────────────────────────────────────────────────────────┐
│                     ESP8266 OLED UI                            │
│  Firmware UI légère pour écran SSD1306                         │
├───────────────────────────────────────────────────────────────┤
│                                                                 │
│  ┌─────────────────────────────────────────────────────────┐  │
│  │ main.cpp (global scope)                                  │  │
│  ├──────────────────────────────────────────────────────────┤ │
│  │ - g_link: SoftwareSerial (D6 RX, D5 TX, 57600)          │  │
│  │ - g_display: Adafruit_SSD1306 (128x64, I2C)             │  │
│  │ - g_state: TelemetryState (état UI reçu)                │  │
│  │ - g_linkState: LinkMonitorState (monitoring link)       │  │
│  │ - g_displayReady: bool                                   │  │
│  └──────────────────────────────────────────────────────────┘  │
│                                                                 │
│  ┌─────────────────────────────────────────────────────────┐  │
│  │ TelemetryState                                           │  │
│  ├──────────────────────────────────────────────────────────┤ │
│  │ + mode: char[12]                                         │  │
│  │ + track, trackTotal: uint16_t                            │  │
│  │ + volume: uint8_t                                        │  │
│  │ + tuningOffset: int8_t                                   │  │
│  │ + scene: char[16]                                        │  │
│  │ + peerUptimeMs, seq: uint32_t                            │  │
│  └──────────────────────────────────────────────────────────┘  │
│                                                                 │
│  ┌─────────────────────────────────────────────────────────┐  │
│  │ LinkMonitor                                              │  │
│  ├──────────────────────────────────────────────────────────┤ │
│  │ + update(nowMs, rxOccurred): LinkStatus                  │  │
│  │ + isAlive(), isDown(): bool                              │  │
│  │ + consecutiveLoss(): uint32_t                            │  │
│  ├──────────────────────────────────────────────────────────┤ │
│  │ - lastRxMs_: uint32_t                                    │  │
│  │ - timeoutMs_: uint16_t (1500)                            │  │
│  │ - downConfirmMs_: uint16_t (450)                         │  │
│  └──────────────────────────────────────────────────────────┘  │
│                                                                 │
│  ┌─────────────────────────────────────────────────────────┐  │
│  │ RenderScheduler                                          │  │
│  ├──────────────────────────────────────────────────────────┤ │
│  │ + shouldRender(nowMs, stateDirty): bool                  │  │
│  ├──────────────────────────────────────────────────────────┤ │
│  │ - periodMs_: uint16_t (250)                              │  │
│  │ - lastRenderMs_: uint32_t                                │  │
│  └──────────────────────────────────────────────────────────┘  │
│                                                                 │
│  ┌─────────────────────────────────────────────────────────┐  │
│  │ StatParser                                               │  │
│  ├──────────────────────────────────────────────────────────┤ │
│  │ + parseFrame(line, state): ParseResult                   │  │
│  │ - helpers: parseMsgType(), parseField(), crc8()...       │  │
│  └──────────────────────────────────────────────────────────┘  │
│                                                                 │
│  ┌─────────────────────────────────────────────────────────┐  │
│  │ Apps (state machines)                                    │  │
│  ├──────────────────────────────────────────────────────────┤ │
│  │ BootApp: splash screen (3.6s min)                        │  │
│  │ LinkApp: "LINK DOWN" animations scroll                   │  │
│  │ Mp3App: affichage track/volume/tuning                    │  │
│  │ UlockApp: animation unlock 6 frames (2.5s chacune)       │  │
│  └──────────────────────────────────────────────────────────┘  │
│                                                                 │
└───────────────────────────────────────────────────────────────┘
```

## Architecture UI RP2040 TFT

```
┌───────────────────────────────────────────────────────────────┐
│                     RP2040 TFT UI                              │
│  Firmware UI tactile LVGL                                      │
├───────────────────────────────────────────────────────────────┤
│                                                                 │
│  ┌─────────────────────────────────────────────────────────┐  │
│  │ main.cpp                                                 │  │
│  ├──────────────────────────────────────────────────────────┤ │
│  │ - g_tft: TFT_eSPI (ILI9488/ILI9486 SPI)                 │  │
│  │ - g_touch: XPT2046_Touchscreen (CS:GP9, IRQ:GP15)       │  │
│  │ - g_link: UiLinkClient (UART1 GP0/GP1, 57600)           │  │
│  │ - g_snapshot: UiSnapshot (état UI local)                │  │
│  │ - LVGL widgets: g_labelLink, g_labelMode, etc.          │  │
│  └──────────────────────────────────────────────────────────┘  │
│                                                                 │
│  ┌─────────────────────────────────────────────────────────┐  │
│  │ UiLinkClient                                             │  │
│  ├──────────────────────────────────────────────────────────┤ │
│  │ + begin(): void                                          │  │
│  │ + poll(nowMs): void                                      │  │
│  │ + sendHello(): bool                                      │  │
│  │ + sendButton(btnId, action): bool                        │  │
│  │ + sendTouch(x, y, action): bool                          │  │
│  │ + connected(): bool                                      │  │
│  ├──────────────────────────────────────────────────────────┤ │
│  │ - serial_: HardwareSerial&                               │  │
│  │ - lastRxMs_, lastTxMs_: uint32_t                         │  │
│  │ - timeoutMs_: uint16_t (1500)                            │  │
│  └──────────────────────────────────────────────────────────┘  │
│                                                                 │
│  ┌─────────────────────────────────────────────────────────┐  │
│  │ LVGL Port                                                │  │
│  ├──────────────────────────────────────────────────────────┤ │
│  │ + lvgl_port_init(): void                                 │  │
│  │ + lvgl_port_disp_flush(drv, area, buf): void            │  │
│  │ + lvgl_port_touchpad_read(drv, data): void              │  │
│  ├──────────────────────────────────────────────────────────┤ │
│  │ - g_lvgl_disp_buf: static buffer 480x320x2              │  │
│  │ - flush: TFT_eSPI→pushImage()                           │  │
│  │ - touch: XPT2046→getPoint()→LVGL coords                 │  │
│  └──────────────────────────────────────────────────────────┘  │
│                                                                 │
│  ┌─────────────────────────────────────────────────────────┐  │
│  │ LVGL UI Hierarchy                                        │  │
│  ├──────────────────────────────────────────────────────────┤ │
│  │ lv_scr_act() (root screen)                               │  │
│  │  ├─ g_labelLink: "LINK: OK/DOWN"                         │  │
│  │  ├─ g_labelMode: "MODE: STORY/MP3/RADIO"                 │  │
│  │  ├─ g_labelTrack: "TRACK: 1 / 5"                         │  │
│  │  ├─ g_labelVolume: "VOL: 50"                             │  │
│  │  ├─ g_labelTune: "TUNE: +2 (85%)"                        │  │
│  │  └─ g_labelMeta: "UPTIME: 12345 ms, SEQ: 42"             │  │
│  └──────────────────────────────────────────────────────────┘  │
│                                                                 │
└───────────────────────────────────────────────────────────────┘
```

## Protocole UI Link v2 (contrat partagé)

```
┌───────────────────────────────────────────────────────────────┐
│                  ui_link_v2.h/md (C/C++)                       │
│  Protocole UART frame-based ASCII                              │
├───────────────────────────────────────────────────────────────┤
│                                                                 │
│  Format frame:                                                 │
│  @<TYPE>|key1=val1|key2=val2|...|crc=XX\n                     │
│                                                                 │
│  Types messages:                                               │
│  ┌──────────────────────────────────────────────────────────┐ │
│  │ HELLO    ESP32→UI  Handshake initial (proto, caps)       │ │
│  │ ACK      UI→ESP32  Acquittement HELLO                     │ │
│  │ KEYFRAME ESP32→UI  État UI complet (first/reconnect)     │ │
│  │ STAT     ESP32→UI  État UI partiel (delta)               │ │
│  │ BTN      UI→ESP32  Événement bouton                       │ │
│  │ TOUCH    UI→ESP32  Événement tactile                      │ │
│  │ PING     ESP32→UI  Heartbeat 1s                           │ │
│  │ PONG     UI→ESP32  Réponse heartbeat                      │ │
│  │ CMD      Bi→Bi     Commande debug (rare)                  │ │
│  └──────────────────────────────────────────────────────────┘ │
│                                                                 │
│  Fields état UI (KEYFRAME/STAT):                               │
│  ┌──────────────────────────────────────────────────────────┐ │
│  │ mode         STORY/MP3/RADIO/BOOT/SIGNAL                  │ │
│  │ seq          Numéro séquence frame (anti-dup)             │ │
│  │ ms           Uptime ESP32 (rollover detection)            │ │
│  │ track        Numéro piste courante                        │ │
│  │ track_tot    Total pistes                                 │ │
│  │ vol          Volume 0-100                                 │ │
│  │ tune_ofs     Radio tuning offset -50/+50                  │ │
│  │ tune_conf    Radio tuning confidence 0-100%               │ │
│  │ scene        Story scene ID (ex: "unlock", "win")         │ │
│  │ hold         Debug: hold flag                             │ │
│  │ key          Debug: last key                              │ │
│  └──────────────────────────────────────────────────────────┘ │
│                                                                 │
│  Fields bouton (BTN):                                          │
│  ┌──────────────────────────────────────────────────────────┐ │
│  │ btn          OK/NEXT/PREV/BACK/VOL_UP/VOL_DOWN/MODE       │ │
│  │ act          PRESS/RELEASE/LONG_PRESS                     │ │
│  │ ms           Timestamp UI local                           │ │
│  └──────────────────────────────────────────────────────────┘ │
│                                                                 │
│  Fields touch (TOUCH):                                         │
│  ┌──────────────────────────────────────────────────────────┐ │
│  │ x, y         Coordonnées tactiles                         │ │
│  │ act          PRESS/RELEASE/MOVE                           │ │
│  │ ms           Timestamp UI local                           │ │
│  └──────────────────────────────────────────────────────────┘ │
│                                                                 │
│  CRC8:                                                         │
│  - Polynomial 0x07                                             │
│  - Calculé sur tout le frame sauf crc=XX                       │
│  - Format hex 2 digits (ex: crc=A3)                           │
│                                                                 │
│  Constants:                                                    │
│  - UILINK_V2_MAX_LINE: 320                                     │
│  - UILINK_V2_MAX_FIELDS: 40                                    │
│  - UILINK_V2_HEARTBEAT_MS: 1000                                │
│  - UILINK_V2_TIMEOUT_MS: 1500                                  │
│                                                                 │
└───────────────────────────────────────────────────────────────┘
```

## Diagramme de séquence - Boot & Story Flow

```
ESP32             ESP8266_OLED      RP2040_TFT        User
  │                    │                  │             │
  │[Boot sequence]     │                  │             │
  │ I2C, SD, Codec     │                  │             │
  │ WiFi, WebUI        │                  │             │
  │                    │                  │             │
  │──HELLO────────────→│                  │             │
  │  proto=2, caps=... │                  │             │
  │                    │◄─[I2C scan]      │             │
  │                    │  [Init OLED]     │             │
  │◄─ACK───────────────│                  │             │
  │                    │                  │             │
  │──KEYFRAME─────────→│                  │             │
  │  mode=STORY        │                  │             │
  │  scene=intro       │                  │             │
  │  track=0, vol=50   │                  │             │
  │                    │                  │             │
  │                    │[Render]          │             │
  │                    │ "STORY"          │             │
  │                    │ "INTRO"          │             │
  │                    │                  │             │
  │[StoryEngine start] │                  │             │
  │ Step: INTRO        │                  │             │
  │ Action: audio_base │                  │             │
  │   token=INTRO      │                  │             │
  │                    │                  │             │
  │[AudioService]      │                  │             │
  │ startBaseFs()      │                  │             │
  │ /sd/INTRO_001.mp3  │                  │             │
  │                    │                  │             │
  │                    │                  │             │  [User presses OK]
  │                    │                  │             │──────────┐
  │                    │                  │             │          │
  │◄─BTN───────────────│                  │             │          │
  │  btn=OK, act=PRESS │                  │             │◄─────────┘
  │                    │                  │             │
  │[InputController]   │                  │             │
  │→StoryController    │                  │             │
  │ postEvent("OK")    │                  │             │
  │                    │                  │             │
  │[StoryEngine]       │                  │             │
  │ transition:        │                  │             │
  │  INTRO─(OK)→STEP1  │                  │             │
  │                    │                  │             │
  │[Step onExit]       │                  │             │
  │ stopBase()         │                  │             │
  │                    │                  │             │
  │[Step onEnter]      │                  │             │
  │ Action: audio_base │                  │             │
  │   token=STEP1      │                  │             │
  │ Action: screen     │                  │             │
  │   scene=puzzle     │                  │             │
  │                    │                  │             │
  │──STAT──────────────→│                  │             │
  │  scene=puzzle      │                  │             │
  │  track=1           │                  │             │
  │                    │                  │             │
  │                    │[Update display]  │             │
  │                    │ "PUZZLE"         │             │
  │                    │ "TRACK: 1/5"     │             │
  │                    │                  │             │
  │──PING──────────────→│                  │             │
  │  ms=45123          │                  │             │
  │◄─PONG──────────────│                  │             │
  │  ms=45123          │                  │             │
  │                    │                  │             │
  .                    .                  .             .
  .  [Story continues with transitions]                 .
  .                    .                  .             .
  │                    │                  │             │
  │[Transition WIN]    │                  │             │
  │ Action: audio_base │                  │             │
  │   token=WIN,       │                  │             │
  │   fallback=FX_WIN  │                  │             │
  │ Action: screen     │                  │             │
  │   scene=unlock     │                  │             │
  │                    │                  │             │
  │──KEYFRAME─────────→│                  │             │
  │  scene=unlock      │                  │             │
  │  mode=STORY        │                  │             │
  │                    │                  │             │
  │                    │[Unlock anim 6f]  │             │
  │                    │ 🔓 animation     │             │
  │                    │                  │             │
  │[ETAPE2 timer]      │                  │             │
  │ 15 min → event     │                  │             │
  │ ETAPE2_DUE         │                  │             │
  │                    │                  │             │
  │[Transition]        │                  │             │
  │ WAIT→ETAPE2        │                  │             │
  │ MP3 gate open      │                  │             │
  │                    │                  │             │
  │──STAT──────────────→│                  │             │
  │  mode=MP3          │                  │             │
  │  track=1,tot=10    │                  │             │
  │                    │                  │             │
  │                    │[Render MP3 UI]   │             │
  │                    │                  │             │
```

## Diagramme de séquence - UI Link Reconnection

```
ESP32             ESP8266_OLED
  │                    │
  │──PING──────────────→│
  │                    │ [Timeout RX]
  │                    │ [Link DOWN]
  │◄─(no PONG)─────────│
  │                    │
  │ [Timeout TX]       │
  │ [Disconnect]       │
  │                    │
  │                    │[Display]
  │                    │ "LINK DOWN"
  │                    │ "Waiting..."
  │                    │
  │                    │[Recovery loop]
  │                    │ Wait HELLO
  │                    │
  ├─ [User reconnects cable] ─┤
  │                    │
  │──HELLO────────────→│
  │  proto=2, sess=2   │
  │                    │
  │◄─ACK───────────────│
  │  sess=2            │
  │                    │
  │──KEYFRAME─────────→│
  │  (full state)      │
  │                    │
  │                    │[Restore display]
  │                    │ Mode, track...
  │                    │
  │──PING──────────────→│
  │◄─PONG──────────────│
  │                    │
  . [Link restored]    .
```

## Points clés de l'architecture

### 1. Séparation des responsabilités
- **Controllers** : Orchestration haut niveau (story, MP3, radio)
- **Services** : Logique métier réutilisable (audio, input, screen sync)
- **Drivers** : Accès matériel (codec, keypad, SD, I2C)

### 2. Event-driven Story Engine
- Machine à états déclarative (ScenarioDef YAML → C++)
- Event queue asynchrone (BTN, TOUCH, timers, conditions)
- Transition implicites (timeout, LA detection, MP3 gate)
- Actions modulaires via ActionRegistry

### 3. Multi-canal audio
- **Base** : Canal principal (MP3, effets longs, radio)
- **Overlay** : Effets courts non-bloquants (beep, jingle)
- Timeouts indépendants, fallback sur I2S FX si MP3 manquant

### 4. Protocol agnostic UI
- UI Link v2 abstrait l'implémentation UI
- Supporte multiples UIs simultanées (OLED + TFT)
- Heartbeat + timeout pour résilience
- Keyframe vs delta pour optimiser bandwidth

### 5. Résilience
- Reconnexion automatique UI (session counter)
- Timeouts audio (MP3 bloqué → fallback FX)
- CRC8 validation frames
- Graceful degradation (UI down → continue audio)

### 6. Modularité
- Story apps découplées (LaDetector, Mp3Gate, ScreenScene, AudioPack)
- Pluggable dans engine via StoryAppHost
- Activation conditionnelle par step ID pattern matching

### 7. Runtime modes flexibles
- **STORY_V2** : Mode quete (UNLOCK -> WIN -> WAIT_ETAPE2 -> ETAPE2 -> DONE)
- **MP3_PLAYER** : Lecteur SD pur (skip story)
- **RADIO** : Streaming web (skip story + MP3)
- Sélection boot via config.h flags

## Dépendances principales

### ESP32 Audio
| Lib | Version | Usage |
|-----|---------|-------|
| ESP8266Audio | 1.9.7 | MP3 async decoder |
| arduino-audio-tools | v1.2.2 | I2S pipeline, streaming |
| Mozzi | 2.0.2 | Synthèse audio (LA detector) |
| ArduinoJson | 6.21.5 | Parsing configs/web responses |
| AsyncTCP | latest | TCP async pour web/radio |
| ESPAsyncWebServer | latest | Serveur web UI contrôle |

### ESP8266 OLED
| Lib | Version | Usage |
|-----|---------|-------|
| Adafruit SSD1306 | 2.5.13 | Driver OLED I2C |
| Adafruit GFX | 1.12.1 | Primitives graphiques |
| EspSoftwareSerial | 8.2.0 | UART software UI Link |
| U8g2 | 2.36.2 | Fonts alternatifs |

### RP2040 TFT
| Lib | Version | Usage |
|-----|---------|-------|
| TFT_eSPI | 2.5.43 | Driver TFT SPI |
| XPT2046_Touchscreen | 0.0.0-alpha | Driver tactile résistif |
| LVGL | 8.3.11 | Framework GUI complet |

## Métriques

- **Code source ESP32** : ~18 600 lignes C++ (138 fichiers)
- **Code source UI** : ~76 fichiers (ESP8266 + RP2040)
- **Services** : 11 services modulaires
- **Story Apps** : 4 apps pluggables
- **Protocole** : 9 types messages UI Link v2
- **Audio canaux** : 2 (Base + Overlay)
- **Runtime modes** : 3 (STORY_V2, MP3_PLAYER, RADIO)

## Évolutivité

L'architecture permet facilement :

1. **Ajouter une nouvelle UI** : Implémenter client UI Link v2 sur nouveau MCU
2. **Ajouter une Story App** : Étendre StoryApp, register dans StoryAppHost
3. **Ajouter un service** : Créer service isolé, injecter via controllers
4. **Ajouter un runtime mode** : Nouvelle branche dans AppOrchestrator::setup()
5. **Étendre le protocole** : Ajouter fields dans ScreenFrame, backward compatible

## Documentation associée

- [État des lieux du firmware](STATE_ANALYSIS.md)
- [Protocole UI Link v2](../protocol/ui_link_v2.md)
- [Story Engine v2](STORY_ENGINE_V2.md) _(à créer)_
- [Guide développeur](../README.md)

---

**Dernière mise à jour** : 15 février 2026  
**Auteur** : Firmware team  
**Status** : ✅ Stable (prêt pour merge PR #86)
