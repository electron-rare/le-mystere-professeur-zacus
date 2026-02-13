# MP3 RC Review Checklist

## Scope

- [ ] MPRC-10/11: serial MP3 extraction + controller wiring
- [ ] MPRC-20/21: scan runtime non-bloquant + progression enrichie
- [ ] MPRC-30/31: UX MP3 NOW/BROWSE/QUEUE/SET + parite clavier/serie
- [ ] MPRC-40/42: trame ecran MP3 UI et parser ESP8266 backward-compatible
- [ ] Build profile: `esp32dev` ON story V2 / `esp32_release` OFF story V2

## Canonical commands

- [ ] `MP3_STATUS`
- [ ] `MP3_UI_STATUS`
- [ ] `MP3_QUEUE_PREVIEW [n]`
- [ ] `MP3_CAPS`
- [ ] `MP3_SCAN START|STATUS|CANCEL|REBUILD`
- [ ] `MP3_SCAN_PROGRESS`
- [ ] `MP3_BACKEND STATUS|SET <AUTO|AUDIO_TOOLS|LEGACY>`
- [ ] `MP3_BACKEND_STATUS`

## Runtime expectations

- [ ] no blocking transition during scan
- [ ] command latency acceptable under scan load
- [ ] keyboard remains responsive during scan/audio
- [ ] screen remains stable and recovers after reset/relink
- [ ] `MP3_BACKEND_STATUS` inclut `last_fallback_reason` + compteurs `tools_*` / `legacy_*`
- [ ] `MP3_CAPS` expose des capacites runtime dynamiques (pas de promesse hardcodee)

## Evidence

- [ ] `tools/qa/mp3_rc_smoke.sh` output attached
- [ ] `tools/qa/mp3_client_demo_smoke.sh` output attached
- [ ] live USB runbook executed (`tools/qa/mp3_rc_runbook.md`)
- [ ] checklist operateur client verifiee (`tools/qa/mp3_client_live_checklist.md`)
- [ ] anomalies classified (Critique/Majeure/Mineure)
