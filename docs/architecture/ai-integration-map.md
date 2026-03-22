# AI Integration Map

## Mindmap — AI Capabilities

```mermaid
mindmap
  root((Zacus AI))
    On-Device
      ESP-SR v2.0
        WakeNet "Hey Zacus"
        MultiNet 50 commands FR
        16-bit PCM 16kHz
        280 KB PSRAM
      ESP-DL v3.2
        YOLOv11n INT8
        8 prop classes
        5-7 FPS 320x240
        600 KB PSRAM
      Fallback
        Tap-to-talk UI
        QR code scan
        Pre-recorded hints
    Server
      mascarade LLM
        Adaptive hints
        Anti-spoiler prompts
        Conversation memory
        Ollama backend
      Coqui XTTS-v2
        Voice cloning 6s ref
        French TTS
        Chunked streaming
        Docker container
      MCP Server
        puzzle_set_state
        audio_play
        led_set
        camera_capture
        scenario_advance
    GPU KXKM-AI
      AudioCraft MusicGen
        Room ambients 30-60s
        SFX generation
        Batch pre-generation
        MP3 128kbps output
      Fine-tune Pipeline
        Qwen2.5-Coder-1.5B
        Unsloth + SimPO
        GGUF Q4_K_M export
        P2P distribute_task
    Analytics
      Difficulty adaptation
      Step timing telemetry
      Hint usage tracking
      Player count detection
```

## Flowchart — Data Flows

```mermaid
flowchart TD
    subgraph Input["Player Input"]
        MIC[Microphone I2S]
        CAM[Camera OV2640]
        BTN[Buttons / Touch]
        QR[QR Scanner]
    end

    subgraph OnDevice["ESP32-S3 On-Device AI"]
        SR[ESP-SR v2.0<br/>Wake + Commands]
        DL[ESP-DL v3.2<br/>Object Detection]
        RT3[Runtime 3 Engine<br/>Step Machine]
        AUD[Audio Manager<br/>I2S DMA]
        LED[LED Manager<br/>WS2812B]
        UI[LVGL Display]
    end

    subgraph Network["Network Layer"]
        WIFI[WiFi HTTP Client]
        ESPNOW[ESP-NOW Mesh]
        WS[WebSocket Events]
    end

    subgraph Server["mascarade Server"]
        API[mascarade API<br/>/api/v1/send]
        MCP[MCP Hardware Server<br/>JSON-RPC 2.0]
        TTS[Coqui XTTS-v2<br/>Docker :5002]
        OLLAMA[Ollama<br/>:11434]
        ADAPT[Adaptive Engine<br/>Difficulty Params]
    end

    subgraph GPU["KXKM-AI RTX 4090"]
        MUSICGEN[AudioCraft MusicGen<br/>Ambient Generation]
        STABAUD[Stable Audio Open<br/>SFX Generation]
        FINETUNE[Fine-tune Pipeline<br/>Unsloth]
    end

    subgraph Output["Player Output"]
        SPK[Speaker I2S]
        LEDS[LED Strips]
        SCREEN[LCD Display]
        SERVO[Puzzle Actuators]
    end

    %% Input -> On-Device
    MIC -->|PCM 16kHz| SR
    CAM -->|320x240 RGB| DL
    BTN -->|GPIO events| RT3
    QR -->|decoded text| RT3

    %% On-Device processing
    SR -->|command token| RT3
    DL -->|detection event| RT3
    RT3 -->|play command| AUD
    RT3 -->|color command| LED
    RT3 -->|screen update| UI

    %% On-Device -> Network
    SR -->|complex query| WIFI
    RT3 -->|hint request| WIFI
    RT3 -->|telemetry| WIFI
    RT3 <-->|peer sync| ESPNOW
    RT3 -->|state events| WS

    %% Network -> Server
    WIFI -->|POST /api/v1/send| API
    WIFI -->|JSON-RPC| MCP
    WS -->|status stream| MCP

    %% Server processing
    API -->|inference| OLLAMA
    OLLAMA -->|hint text| API
    API -->|text| TTS
    MCP -->|tool dispatch| API
    API -->|telemetry| ADAPT
    ADAPT -->|params| API

    %% Server -> Device
    TTS -->|PCM stream| AUD
    MCP -->|puzzle cmd| SERVO
    MCP -->|led cmd| LED
    MCP -->|audio cmd| AUD

    %% GPU -> Server (offline)
    MUSICGEN -.->|ambient MP3| AUD
    STABAUD -.->|SFX WAV| AUD
    FINETUNE -.->|GGUF model| OLLAMA

    %% Output
    AUD --> SPK
    LED --> LEDS
    UI --> SCREEN
    RT3 --> SERVO

    %% Styling
    style OnDevice fill:#1a3a1a,color:#fff
    style Server fill:#1a1a3a,color:#fff
    style GPU fill:#3a1a1a,color:#fff
```

## Latency Map

```mermaid
flowchart LR
    A["Player speaks"] -->|0ms| B["ESP-SR wake"]
    B -->|200ms| C["Command parse"]
    C -->|500ms| D{"On-device<br/>or server?"}
    D -->|"on-device<br/>0ms"| E["Runtime 3 action"]
    D -->|"server<br/>50ms network"| F["mascarade LLM"]
    F -->|"2000ms inference"| G["TTS synthesis"]
    G -->|"2000ms render"| H["Audio stream"]
    H -->|"100ms buffer"| I["Speaker plays"]

    E -->|"50ms"| J["LED/Audio/Servo"]

    style D fill:#dd6,stroke:#333
```

**Critical path (voice hint)**: 200 + 500 + 50 + 2000 + 2000 + 100 = ~4850 ms worst case, target < 3000 ms.

## Component Integration Matrix

```mermaid
flowchart TD
    subgraph Legend
        direction LR
        L1[Implemented] --- L2[Planned Phase B]
        L2 --- L3[Planned Phase C+]
    end

    subgraph Current["Currently Implemented"]
        R3[Runtime 3 Engine]
        AUDIO[Audio Manager I2S]
        LEDM[LED Manager]
        LVGL[LVGL UI]
        WIFIC[WiFi + HTTP API]
        ESPNM[ESP-NOW Manager]
        STOR[Storage Manager]
    end

    subgraph PhaseB["Phase B: Voice"]
        WAKE[WakeNet Custom]
        MULTI[MultiNet FR]
        XTTS[XTTS-v2 Docker]
        STREAM[Audio Streaming]
    end

    subgraph PhaseC["Phase C+: Vision, LLM, Music"]
        YOLO[YOLOv11n ESP-DL]
        HINTS[LLM Hints]
        MCPS[MCP Server]
        MGEN[MusicGen Batch]
    end

    R3 --> WAKE
    R3 --> YOLO
    R3 --> HINTS
    AUDIO --> STREAM
    STREAM --> XTTS
    WIFIC --> MCPS
    MCPS --> HINTS
    HINTS --> XTTS
    AUDIO --> MGEN
    WAKE --> MULTI

    style Current fill:#2d6,stroke:#333
    style PhaseB fill:#69d,stroke:#333
    style PhaseC fill:#d69,stroke:#333
```

## Notes
- On-device AI (ESP-SR, ESP-DL) runs without network — zero-latency fallback.
- Server AI (LLM, TTS, MCP) requires WiFi — graceful degradation to cached hints.
- GPU AI (AudioCraft) is offline batch only — no runtime dependency.
- All AI features are additive: the base Runtime 3 game works without any AI.
