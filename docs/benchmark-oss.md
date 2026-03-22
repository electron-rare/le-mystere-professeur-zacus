# OSS Benchmark

## Adopt / Align

| Tool | Why it matters | Reference |
| --- | --- | --- |
| Blockly | Canonical block-based authoring model for the Zacus studio. | https://developers.google.com/blockly |
| Mermaid | Lightweight diagrams embedded in docs and specs. | https://mermaid.js.org/ |
| Material for MkDocs | Fast documentation shell for the rebuilt knowledge base. | https://squidfunk.github.io/mkdocs-material/ |
| Charm Gum | Optional shell TUI layer with graceful fallback. | https://github.com/charmbracelet/gum |

## Benchmark / Inspiration

| Project | Why it is relevant | Reference |
| --- | --- | --- |
| Node-RED | Flow-based authoring patterns and graph ergonomics. | https://nodered.org/ |
| Ink | Narrative authoring and branching conventions. | https://www.inklestudios.com/ink/ |
| Yarn Spinner | Dialogue/runtime separation patterns. | https://www.yarnspinner.dev/ |
| LVGL | Embedded UI runtime constraints and display patterns. | https://lvgl.io/ |
| SquareLine Studio | Embedded UI workflow inspiration for screen authoring. | https://squareline.io/ |
| EEZ Studio | Embedded UI/editor workflow benchmark for device tooling. | https://www.envox.eu/studio/studio-introduction/ |

## Zacus-specific Decision
- Zacus Runtime 3 remains custom and portable.
- External OSS is used as a reference or shell, not as a drop-in replacement for the story runtime.

## Fresh Notes (2026-03-16)
- Blockly remains the strongest fit for Zacus authoring because it is still positioned as a customizable client-side block editor for application-specific languages.
- Node-RED remains the best ergonomics benchmark for event-driven flows that can run both on low-cost edge devices and in the cloud.
- LVGL stays the right embedded UI reference because it explicitly targets small memory footprints and broad MCU/OS/display portability.
- EEZ Studio is the strongest workflow benchmark for embedded authoring because it combines templates, visual debugging, execution logs, and timeline-driven UI tooling.
