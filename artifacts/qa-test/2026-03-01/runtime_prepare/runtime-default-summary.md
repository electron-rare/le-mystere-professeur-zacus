# Runtime DEFAULT prepare summary

## Scope
- Firmware runtime not modified (prepare-only mode).
- Source runtime bundle: `scenario-ai-coherence/zacus_conversation_bundle_v3/scenario_runtime.json`.
- Source firmware current: `hardware/firmware/data/story/scenarios/DEFAULT.json`.

## High-level delta
- initial_step current: `RTC_ESP_ETAPE1`
- initial_step bundle: `STEP_U_SON_PROTO`
- steps count current: 11
- steps count bundle: 9
- app_bindings count current: 6
- app_bindings count bundle: 16
- transitions count current: 20
- transitions count bundle: 34

## Step IDs only in current DEFAULT.json
- `RTC_ESP_ETAPE1`
- `RTC_ESP_ETAPE2`
- `SCENE_CREDIT`
- `SCENE_FINAL_WIN`
- `SCENE_LA_DETECTOR`
- `SCENE_LEFOU_DETECTOR`
- `SCENE_QR_DETECTOR`
- `SCENE_U_SON_PROTO`
- `STEP_MEDIA_MANAGER`
- `WIN_ETAPE1`

## Step IDs only in bundle runtime
- `STEP_FINAL_WIN`
- `STEP_LA_DETECTOR`
- `STEP_LEFOU_DETECTOR`
- `STEP_QR_DETECTOR`
- `STEP_RTC_ESP_ETAPE1`
- `STEP_RTC_ESP_ETAPE2`
- `STEP_U_SON_PROTO`
- `STEP_WIN_ETAPE1`

## App bindings only in current DEFAULT.json
- `APP_QR_UNLOCK`

## App bindings only in bundle runtime
- `APP_ACTION`
- `APP_CAMERA`
- `APP_INPUT`
- `APP_LED`
- `APP_LOG`
- `APP_QR`
- `APP_SD`
- `APP_SERIAL`
- `APP_SONAR`
- `APP_TIMER`
- `APP_UNLOCK`

## Transition event keys only in current DEFAULT.json
- `audio_done:AUDIO_DONE`
- `button:ANY`
- `espnow:ACK_WIN2`
- `serial:QR_OK`
- `timer:WIN_ETAPE1_TO_CREDIT`

## Transition event keys only in bundle runtime
- `BTN:ANY`
- `action:ACTION_FORCE_ETAPE2`
- `action:FORCE_WIN_ETAPE2`
- `audio_done:loop`
- `esp_now:ACK_WARNING`
- `esp_now:ACK_WIN1`
- `esp_now:ACK_WIN2`
- `event:QR_TIMEOUT`
- `serial:FORCE_ETAPE2`
- `serial:FORCE_WIN_ETAPE1`
