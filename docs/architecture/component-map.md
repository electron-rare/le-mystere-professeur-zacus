# Component Map

```mermaid
flowchart LR
  subgraph Studio
    App["App.tsx"]
    Designer["BlocklyDesigner.tsx"]
    ScenarioLib["lib/scenario.ts"]
    Runtime3Lib["lib/runtime3.ts"]
    ApiLib["lib/api.ts"]
  end

  subgraph Canon
    ScenarioYaml["game/scenarios/zacus_v2.yaml"]
  end

  subgraph Runtime3
    CompilerPy["tools/scenario/compile_runtime3.py"]
    RuntimeCommon["tools/scenario/runtime3_common.py"]
    SimPy["tools/scenario/simulate_runtime3.py"]
  end

  subgraph Device
    Firmware["hardware/firmware"]
    StoryCore["lib/story runtime adapter"]
    UI["Freenove UI + API"]
  end

  ScenarioYaml --> ScenarioLib
  Designer --> ScenarioLib
  ScenarioLib --> Runtime3Lib
  Runtime3Lib --> App
  ScenarioYaml --> CompilerPy
  CompilerPy --> RuntimeCommon
  RuntimeCommon --> SimPy
  RuntimeCommon --> Firmware
  Firmware --> StoryCore
  StoryCore --> UI
  ApiLib --> UI
```

## Notes
- The React studio owns authoring ergonomics and previews.
- The Python compiler/simulator owns deterministic offline validation.
- Firmware consumes the Runtime 3 contract and exposes runtime APIs.
