# System Map

## Canonical System

```mermaid
flowchart TD
  subgraph Authoring
    YAML["game/scenarios/zacus_v2.yaml"]
    Blockly["frontend-scratch-v2 Blockly graph"]
  end

  subgraph Runtime3
    Compiler["compile_runtime3.py / runtime3.ts"]
    IR["Runtime 3 IR JSON"]
    Sim["simulate_runtime3.py"]
  end

  subgraph Delivery
    Audio["audio/manifests"]
    Printables["printables/manifests"]
    Kit["kit-maitre-du-jeu"]
    Docs["docs + MkDocs"]
  end

  subgraph Device
    Firmware["hardware/firmware adapter"]
    Board["freenove_esp32s3"]
    Media["media / network / UI"]
  end

  YAML --> Compiler
  Blockly --> Compiler
  Compiler --> IR
  IR --> Sim
  IR --> Firmware
  YAML --> Audio
  YAML --> Printables
  YAML --> Kit
  YAML --> Docs
  Firmware --> Board
  Firmware --> Media
```

## Design Notes
- YAML stays canonical for narrative truth during migration.
- Blockly is the preferred authoring UX, not the runtime artifact itself.
- Runtime 3 IR is the portable contract between authoring and execution.
- Firmware owns board integration, not story semantics.
