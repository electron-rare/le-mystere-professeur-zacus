// storage_manager.cpp - LittleFS + SD story provisioning helpers.
#include "storage_manager.h"

#include <ArduinoJson.h>
#include <FS.h>
#include <LittleFS.h>

#if defined(ARDUINO_ARCH_ESP32) && __has_include(<SD_MMC.h>)
#include <SD_MMC.h>
#include "ui_freenove_config.h"
#define ZACUS_HAS_SD_MMC 1
#else
#define ZACUS_HAS_SD_MMC 0
#endif

#include <cstring>
#include <cctype>

#include "scenarios/default_scenario_v2.h"

namespace {

constexpr const char* kRequiredDirectories[] = {
    "/data",
    "/picture",
    "/music",
    "/audio",
    "/recorder",
    "/story",
    "/story/scenarios",
    "/story/screens",
    "/story/audio",
    "/story/apps",
    "/story/actions",
    "/scenarios",
    "/scenarios/data",
    "/screens",
};

struct EmbeddedStoryAsset {
  const char* path;
  const char* payload;
};

constexpr EmbeddedStoryAsset kEmbeddedStoryAssets[] = {
    {"/story/actions/ACTION_CAMERA_SNAPSHOT.json", R"JSON({"id":"ACTION_CAMERA_SNAPSHOT","type":"camera_snapshot","config":{"filename":"story_capture.jpg","event_on_success":"SERIAL:CAMERA_CAPTURED"}})JSON"},
    {"/story/actions/ACTION_FORCE_ETAPE2.json", R"JSON({"id":"ACTION_FORCE_ETAPE2","type":"emit_story_event","config":{"event_type":"action","event_name":"ACTION_FORCE_ETAPE2","target":"STEP_ETAPE2"}})JSON"},
    {"/story/actions/ACTION_HW_LED_ALERT.json", R"JSON({"id":"ACTION_HW_LED_ALERT","type":"hardware_led","config":{"mode":"alert","r":255,"g":60,"b":32,"brightness":92,"pulse":true}})JSON"},
    {"/story/actions/ACTION_HW_LED_READY.json", R"JSON({"id":"ACTION_HW_LED_READY","type":"hardware_led","config":{"mode":"ready","auto_from_scene":true}})JSON"},
    {"/story/actions/ACTION_MEDIA_PLAY_FILE.json", R"JSON({"id":"ACTION_MEDIA_PLAY_FILE","type":"media_play","config":{"file":"/music/boot_radio.mp3"}})JSON"},
    {"/story/actions/ACTION_QUEUE_SONAR.json", R"JSON({"id":"ACTION_QUEUE_SONAR","type":"queue_audio_pack","config":{"pack_id":"PACK_SONAR_HINT","priority":"normal"}})JSON"},
    {"/story/actions/ACTION_REC_START.json", R"JSON({"id":"ACTION_REC_START","type":"recorder_start","config":{"seconds":20,"filename":"story_voice.wav"}})JSON"},
    {"/story/actions/ACTION_REC_STOP.json", R"JSON({"id":"ACTION_REC_STOP","type":"recorder_stop","config":{"reason":"step_change"}})JSON"},
    {"/story/actions/ACTION_REFRESH_SD.json", R"JSON({"id":"ACTION_REFRESH_SD","type":"refresh_storage","config":{"targets":["story/scenarios","story/screens","story/audio"]}})JSON"},
    {"/story/actions/ACTION_TRACE_STEP.json", R"JSON({"id":"ACTION_TRACE_STEP","type":"trace_step","config":{"serial_log":true,"tag":"story_step"}})JSON"},
    {"/story/apps/APP_AUDIO.json", R"JSON({"id":"APP_AUDIO","app":"AUDIO_PACK","config":{"player":"littlefs_mp3","fallback":"builtin_tone","autoplay":true}})JSON"},
    {"/story/apps/APP_CAMERA.json", R"JSON({"id":"APP_CAMERA","app":"CAMERA_STACK","config":{"enabled_on_boot":true,"frame_size":"VGA","jpeg_quality":12,"fb_count":1,"xclk_hz":20000000,"snapshot_dir":"/picture"}})JSON"},
    {"/story/apps/APP_ESPNOW.json", R"JSON({"id":"APP_ESPNOW","app":"ESPNOW_STACK","config":{"enabled_on_boot":true,"bridge_to_story_event":true,"peers":[],"payload_format":"Preferred: JSON envelope {msg_id,seq,type,payload,ack}. Legacy accepted: SC_EVENT <type> <name> | SC_EVENT_RAW <event> | JSON{cmd|raw|event|event_type/event_name} | SERIAL:<event> | TIMER:<event> | ACTION:<event> | UNLOCK | AUDIO_DONE"}})JSON"},
    {"/story/apps/APP_GATE.json", R"JSON({"id":"APP_GATE","app":"MP3_GATE","config":{"mode":"strict","close_on_step_done":true}})JSON"},
    {"/story/apps/APP_QR_UNLOCK.json", R"JSON({"id":"APP_QR_UNLOCK","app":"QR_UNLOCK_APP","config":{"mode":"strict_qr_gate"}})JSON"},
    {"/story/apps/APP_HARDWARE.json", R"JSON({"id":"APP_HARDWARE","app":"HARDWARE_STACK","config":{"enabled_on_boot":true,"telemetry_period_ms":2500,"led_auto_from_scene":true,"mic_enabled":true,"mic_event_threshold_pct":72,"mic_event_name":"SERIAL:MIC_SPIKE","la_trigger_enabled":true,"la_target_hz":440,"la_tolerance_hz":10,"la_max_abs_cents":42,"la_min_confidence":28,"la_min_level_pct":8,"la_stable_ms":3000,"la_release_ms":50,"la_cooldown_ms":1400,"la_timeout_ms":60000,"la_event_name":"SERIAL:BTN_NEXT","battery_enabled":true,"battery_low_pct":20,"battery_low_event_name":"SERIAL:BATTERY_LOW"}})JSON"},
    {"/story/apps/APP_LA.json", R"JSON({"id":"APP_LA","app":"LA_DETECTOR","config":{"unlock_event":"UNLOCK","timeout_ms":30000}})JSON"},
    {"/story/apps/APP_MEDIA.json", R"JSON({"id":"APP_MEDIA","app":"MEDIA_STACK","config":{"music_dir":"/music","picture_dir":"/picture","record_dir":"/recorder","record_max_seconds":30,"auto_stop_record_on_step_change":true}})JSON"},
    {"/story/apps/APP_SCREEN.json", R"JSON({"id":"APP_SCREEN","app":"SCREEN_SCENE","config":{"renderer":"lvgl_fx","mode":"effect_first","show_title":false,"show_symbol":true}})JSON"},
    {"/story/apps/APP_WIFI.json", R"JSON({"id":"APP_WIFI","app":"WIFI_STACK","config":{"hostname":"zacus-freenove","local_ssid":"Les cils","local_password":"mascarade","ap_policy":"if_no_known_wifi","pause_local_retry_when_ap_client":true,"local_retry_ms":15000,"test_ssid":"Les cils","test_password":"mascarade","ap_default_ssid":"Freenove-Setup","ap_default_password":"mascarade"}})JSON"},
    {"/story/audio/PACK_BOOT_RADIO.json", R"JSON({"id":"PACK_BOOT_RADIO","file":"/music/boot_radio.mp3","volume":100})JSON"},
    {"/story/audio/PACK_MORSE_HINT.json", R"JSON({"id":"PACK_MORSE_HINT","file":"/music/morse_hint.mp3","volume":100})JSON"},
    {"/story/audio/PACK_SONAR_HINT.json", R"JSON({"id":"PACK_SONAR_HINT","file":"/music/sonar_hint.mp3","volume":100})JSON"},
    {"/story/audio/PACK_WIN.json", R"JSON({"id":"PACK_WIN","file":"/music/win.mp3","volume":100})JSON"},
        {"/story/actions/ACTION_ESP_NOW_SEND_ETAPE1.json", R"JSON({"id":"ACTION_ESP_NOW_SEND_ETAPE1","type":"espnow_send","config":{"target":"broadcast","payload":"ACK_WIN1"}})JSON"},
    {"/story/actions/ACTION_ESP_NOW_SEND_ETAPE2.json", R"JSON({"id":"ACTION_ESP_NOW_SEND_ETAPE2","type":"espnow_send","config":{"target":"broadcast","payload":"ACK_WIN2"}})JSON"},
    {"/story/actions/ACTION_QR_CODE_SCANNER_START.json", R"JSON({"id":"ACTION_QR_CODE_SCANNER_START","type":"qr_scanner_start","config":{"enable":true}})JSON"},
    {"/story/actions/ACTION_SET_BOOT_MEDIA_MANAGER.json", R"JSON({"id":"ACTION_SET_BOOT_MEDIA_MANAGER","type":"set_boot_mode","config":{"mode":"media_manager","persist_nvs":true}})JSON"},
    {"/story/actions/ACTION_WINNER.json", R"JSON({"id":"ACTION_WINNER","type":"winner_fx","config":{"mode":"final_win"}})JSON"},
    {"/story/audio/PACK_CONFIRM_WIN_ETAPE1.json", R"JSON({"id":"PACK_CONFIRM_WIN_ETAPE1","file":"/music/confirm_win_etape1.mp3","volume":100})JSON"},
    {"/story/audio/PACK_CONFIRM_WIN_ETAPE2.json", R"JSON({"id":"PACK_CONFIRM_WIN_ETAPE2","file":"/music/confirm_win_etape2.mp3","volume":100})JSON"},
    {"/story/screens/SCENE_U_SON_PROTO.json", R"JSON({"id":"SCENE_U_SON_PROTO","title":"PROTO U-SON","subtitle":"Signal brouille","symbol":"ALERT","effect":"blink","visual":{"show_title":true,"show_subtitle":true,"show_symbol":true,"effect_speed_ms":180},"theme":{"bg":"#2A0508","accent":"#FF4A45","text":"#FFF1F1"},"transition":{"effect":"glitch","duration_ms":160}})JSON"},
    {"/story/screens/SCENE_WARNING.json", R"JSON({"id":"SCENE_WARNING","title":"ALERTE","subtitle":"Signal anormal","symbol":"WARN","effect":"blink","visual":{"show_title":true,"show_subtitle":true,"show_symbol":true,"effect_speed_ms":240},"theme":{"bg":"#261209","accent":"#FF9A4A","text":"#FFF2E6"},"transition":{"effect":"fade","duration_ms":200}})JSON"},
    {"/story/screens/SCENE_LEFOU_DETECTOR.json", R"JSON({"id":"SCENE_LEFOU_DETECTOR","title":"DETECTEUR LEFOU","subtitle":"Analyse en cours","symbol":"AUDIO","effect":"wave","visual":{"show_title":true,"show_subtitle":true,"show_symbol":true,"effect_speed_ms":460},"theme":{"bg":"#071B1A","accent":"#46E6C8","text":"#E9FFF9"},"transition":{"effect":"zoom","duration_ms":250}})JSON"},
    {"/story/screens/SCENE_WIN_ETAPE1.json", R"JSON({"id":"SCENE_WIN_ETAPE1","title":"WIN ETAPE 1","subtitle":"Validation distante","symbol":"WIN","effect":"celebrate","visual":{"show_title":true,"show_subtitle":true,"show_symbol":true,"effect_speed_ms":360},"theme":{"bg":"#1E0F32","accent":"#F5C64A","text":"#FFF8E4"},"transition":{"effect":"zoom","duration_ms":280}})JSON"},
    {"/story/screens/SCENE_WIN_ETAPE2.json", R"JSON({"id":"SCENE_WIN_ETAPE2","title":"WIN ETAPE 2","subtitle":"ACK en attente","symbol":"WIN","effect":"celebrate","visual":{"show_title":true,"show_subtitle":true,"show_symbol":true,"effect_speed_ms":340},"theme":{"bg":"#220F3A","accent":"#FFCE62","text":"#FFF8EA"},"transition":{"effect":"zoom","duration_ms":280}})JSON"},
    {"/story/screens/SCENE_QR_DETECTOR.json", R"JSON({"id":"SCENE_QR_DETECTOR","title":"ZACUS QR VALIDATION","subtitle":"Scan du QR final","symbol":"QR","effect":"none","visual":{"show_title":true,"show_subtitle":true,"show_symbol":true,"effect_speed_ms":0},"theme":{"bg":"#102040","accent":"#5CA3FF","text":"#F3F7FF"},"transition":{"effect":"fade","duration_ms":180}})JSON"},
    {"/story/screens/SCENE_FINAL_WIN.json", R"JSON({"id":"SCENE_FINAL_WIN","title":"FINAL WIN","subtitle":"Mission accomplie","symbol":"WIN","effect":"celebrate","visual":{"show_title":true,"show_subtitle":true,"show_symbol":true,"effect_speed_ms":320},"theme":{"bg":"#1C0C2E","accent":"#FFCC5C","text":"#FFF7E4"},"transition":{"effect":"fade","duration_ms":240}})JSON"},
{"/story/scenarios/DEFAULT.json", R"JSON({"id":"DEFAULT","scenario":"DEFAULT","version":2,"initial_step":"SCENE_U_SON_PROTO","hardware_events":{"button_short_1":"BTN_NEXT","button_short_2":"BTN_NEXT","button_short_3":"BTN_NEXT","button_short_4":"BTN_NEXT","button_short_5":"BTN_NEXT","button_long_3":"FORCE_ETAPE2","button_long_4":"FORCE_DONE","espnow_event":"ESPNOW:<payload>"},"app_bindings":["APP_AUDIO","APP_SCREEN","APP_GATE","APP_WIFI","APP_ESPNOW","APP_QR_UNLOCK"],"actions_catalog":["ACTION_TRACE_STEP","ACTION_HW_LED_ALERT","ACTION_QUEUE_SONAR","ACTION_ESP_NOW_SEND_ETAPE1","ACTION_ESP_NOW_SEND_ETAPE2","ACTION_QR_CODE_SCANNER_START","ACTION_WINNER","ACTION_SET_BOOT_MEDIA_MANAGER"],"steps":[{"id":"SCENE_U_SON_PROTO","screen_scene_id":"SCENE_U_SON_PROTO","audio_pack_id":"PACK_BOOT_RADIO","action_ids":["ACTION_TRACE_STEP","ACTION_HW_LED_ALERT"],"transitions":[{"id":"TR_SCENE_U_SON_PROTO_1","trigger":"on_event","event_type":"audio_done","event_name":"AUDIO_DONE","target_step_id":"SCENE_U_SON_PROTO","after_ms":0,"priority":80},{"id":"TR_SCENE_U_SON_PROTO_2","trigger":"on_event","event_type":"button","event_name":"ANY","target_step_id":"SCENE_LA_DETECTOR","after_ms":0,"priority":120},{"id":"TR_SCENE_U_SON_PROTO_3","trigger":"on_event","event_type":"serial","event_name":"FORCE_ETAPE2","target_step_id":"SCENE_LA_DETECTOR","after_ms":0,"priority":140}]},{"id":"SCENE_LA_DETECTOR","screen_scene_id":"SCENE_LA_DETECTOR","action_ids":["ACTION_TRACE_STEP","ACTION_QUEUE_SONAR"],"transitions":[{"id":"TR_SCENE_LA_DETECTOR_1","trigger":"on_event","event_type":"timer","event_name":"ETAPE2_DUE","target_step_id":"SCENE_U_SON_PROTO","after_ms":0,"priority":100},{"id":"TR_SCENE_LA_DETECTOR_2","trigger":"on_event","event_type":"serial","event_name":"BTN_NEXT","target_step_id":"RTC_ESP_ETAPE1","after_ms":0,"priority":110},{"id":"TR_SCENE_LA_DETECTOR_3","trigger":"on_event","event_type":"unlock","event_name":"UNLOCK","target_step_id":"RTC_ESP_ETAPE1","after_ms":0,"priority":115},{"id":"TR_SCENE_LA_DETECTOR_4","trigger":"on_event","event_type":"action","event_name":"ACTION_FORCE_ETAPE2","target_step_id":"RTC_ESP_ETAPE1","after_ms":0,"priority":120},{"id":"TR_SCENE_LA_DETECTOR_5","trigger":"on_event","event_type":"serial","event_name":"FORCE_WIN_ETAPE1","target_step_id":"RTC_ESP_ETAPE1","after_ms":0,"priority":130}]},{"id":"RTC_ESP_ETAPE1","screen_scene_id":"SCENE_WIN_ETAPE1","audio_pack_id":"PACK_CONFIRM_WIN_ETAPE1","action_ids":["ACTION_TRACE_STEP","ACTION_HW_LED_ALERT","ACTION_ESP_NOW_SEND_ETAPE1","ACTION_QUEUE_SONAR"],"transitions":[{"id":"TR_RTC_ESP_ETAPE1_1","trigger":"on_event","event_type":"espnow","event_name":"ACK_WIN1","target_step_id":"WIN_ETAPE1","after_ms":0,"priority":130},{"id":"TR_RTC_ESP_ETAPE1_2","trigger":"on_event","event_type":"serial","event_name":"FORCE_DONE","target_step_id":"WIN_ETAPE1","after_ms":0,"priority":120}]},{"id":"WIN_ETAPE1","screen_scene_id":"SCENE_WIN_ETAPE1","audio_pack_id":"PACK_WIN","action_ids":["ACTION_TRACE_STEP","ACTION_HW_LED_ALERT"],"transitions":[{"id":"TR_WIN_ETAPE1_1","trigger":"on_event","event_type":"serial","event_name":"BTN_NEXT","target_step_id":"STEP_WARNING","after_ms":0,"priority":110},{"id":"TR_WIN_ETAPE1_2","trigger":"on_event","event_type":"serial","event_name":"FORCE_DONE","target_step_id":"STEP_WARNING","after_ms":0,"priority":120},{"id":"TR_WIN_ETAPE1_3","trigger":"on_event","event_type":"espnow","event_name":"ACK_WARNING","target_step_id":"STEP_WARNING","after_ms":0,"priority":130}]},{"id":"STEP_WARNING","screen_scene_id":"SCENE_WARNING","audio_pack_id":"PACK_BOOT_RADIO","action_ids":["ACTION_TRACE_STEP","ACTION_HW_LED_ALERT"],"transitions":[{"id":"TR_STEP_WARNING_1","trigger":"on_event","event_type":"audio_done","event_name":"AUDIO_DONE","target_step_id":"STEP_WARNING","after_ms":0,"priority":80},{"id":"TR_STEP_WARNING_2","trigger":"on_event","event_type":"button","event_name":"ANY","target_step_id":"SCENE_LEFOU_DETECTOR","after_ms":0,"priority":120},{"id":"TR_STEP_WARNING_3","trigger":"on_event","event_type":"serial","event_name":"FORCE_ETAPE2","target_step_id":"SCENE_LEFOU_DETECTOR","after_ms":0,"priority":140}]},{"id":"SCENE_LEFOU_DETECTOR","screen_scene_id":"SCENE_LEFOU_DETECTOR","action_ids":["ACTION_TRACE_STEP","ACTION_QUEUE_SONAR"],"transitions":[{"id":"TR_SCENE_LEFOU_DETECTOR_1","trigger":"on_event","event_type":"timer","event_name":"ETAPE2_DUE","target_step_id":"STEP_WARNING","after_ms":0,"priority":100},{"id":"TR_SCENE_LEFOU_DETECTOR_2","trigger":"on_event","event_type":"serial","event_name":"BTN_NEXT","target_step_id":"RTC_ESP_ETAPE2","after_ms":0,"priority":110},{"id":"TR_SCENE_LEFOU_DETECTOR_3","trigger":"on_event","event_type":"unlock","event_name":"UNLOCK","target_step_id":"RTC_ESP_ETAPE2","after_ms":0,"priority":115},{"id":"TR_SCENE_LEFOU_DETECTOR_4","trigger":"on_event","event_type":"action","event_name":"ACTION_FORCE_ETAPE2","target_step_id":"RTC_ESP_ETAPE2","after_ms":0,"priority":120},{"id":"TR_SCENE_LEFOU_DETECTOR_5","trigger":"on_event","event_type":"serial","event_name":"FORCE_WIN_ETAPE2","target_step_id":"RTC_ESP_ETAPE2","after_ms":0,"priority":130}]},{"id":"RTC_ESP_ETAPE2","screen_scene_id":"SCENE_WIN_ETAPE2","audio_pack_id":"PACK_CONFIRM_WIN_ETAPE2","action_ids":["ACTION_TRACE_STEP","ACTION_HW_LED_ALERT","ACTION_ESP_NOW_SEND_ETAPE2","ACTION_QUEUE_SONAR"],"transitions":[{"id":"TR_RTC_ESP_ETAPE2_1","trigger":"on_event","event_type":"espnow","event_name":"ACK_WIN2","target_step_id":"SCENE_QR_DETECTOR","after_ms":0,"priority":130},{"id":"TR_RTC_ESP_ETAPE2_2","trigger":"on_event","event_type":"serial","event_name":"FORCE_DONE","target_step_id":"SCENE_QR_DETECTOR","after_ms":0,"priority":120}]},{"id":"SCENE_QR_DETECTOR","screen_scene_id":"SCENE_QR_DETECTOR","action_ids":["ACTION_TRACE_STEP","ACTION_QR_CODE_SCANNER_START"],"transitions":[{"id":"TR_SCENE_QR_DETECTOR_1","trigger":"on_event","event_type":"serial","event_name":"QR_OK","target_step_id":"SCENE_FINAL_WIN","after_ms":0,"priority":140},{"id":"TR_SCENE_QR_DETECTOR_2","trigger":"on_event","event_type":"unlock","event_name":"UNLOCK_QR","target_step_id":"SCENE_FINAL_WIN","after_ms":0,"priority":150},{"id":"TR_SCENE_QR_DETECTOR_3","trigger":"on_event","event_type":"serial","event_name":"BTN_NEXT","target_step_id":"SCENE_FINAL_WIN","after_ms":0,"priority":110},{"id":"TR_SCENE_QR_DETECTOR_4","trigger":"on_event","event_type":"action","event_name":"ACTION_FORCE_ETAPE2","target_step_id":"SCENE_FINAL_WIN","after_ms":0,"priority":120},{"id":"TR_SCENE_QR_DETECTOR_5","trigger":"on_event","event_type":"serial","event_name":"FORCE_WIN_ETAPE2","target_step_id":"SCENE_FINAL_WIN","after_ms":0,"priority":130}]},{"id":"SCENE_FINAL_WIN","screen_scene_id":"SCENE_FINAL_WIN","action_ids":["ACTION_TRACE_STEP","ACTION_WINNER"],"transitions":[{"id":"TR_SCENE_FINAL_WIN_1","trigger":"on_event","event_type":"timer","event_name":"WIN_DUE","target_step_id":"STEP_MEDIA_MANAGER","after_ms":0,"priority":140},{"id":"TR_SCENE_FINAL_WIN_2","trigger":"on_event","event_type":"serial","event_name":"BTN_NEXT","target_step_id":"STEP_MEDIA_MANAGER","after_ms":0,"priority":110},{"id":"TR_SCENE_FINAL_WIN_3","trigger":"on_event","event_type":"unlock","event_name":"UNLOCK","target_step_id":"STEP_MEDIA_MANAGER","after_ms":0,"priority":120},{"id":"TR_SCENE_FINAL_WIN_4","trigger":"on_event","event_type":"action","event_name":"FORCE_WIN_ETAPE2","target_step_id":"STEP_MEDIA_MANAGER","after_ms":0,"priority":130},{"id":"TR_SCENE_FINAL_WIN_5","trigger":"on_event","event_type":"serial","event_name":"FORCE_WIN_ETAPE2","target_step_id":"STEP_MEDIA_MANAGER","after_ms":0,"priority":125}]},{"id":"STEP_MEDIA_MANAGER","screen_scene_id":"SCENE_MEDIA_MANAGER","action_ids":["ACTION_TRACE_STEP","ACTION_SET_BOOT_MEDIA_MANAGER"],"mp3_gate_open":true}],"source":"story_selector","screen_root":"/story/screens","audio_root":"/story/audio"})JSON"},
    {"/story/scenarios/EXAMPLE_UNLOCK_EXPRESS.json", R"JSON({"id":"EXAMPLE_UNLOCK_EXPRESS","scenario":"EXAMPLE_UNLOCK_EXPRESS","version":2,"initial_step":"STEP_WAIT_UNLOCK","hardware_events":{"button_short_1":"UNLOCK","button_short_5":"BTN_NEXT","button_long_4":"FORCE_DONE"},"app_bindings":["APP_LA","APP_SCREEN","APP_GATE","APP_AUDIO","APP_WIFI","APP_ESPNOW"],"actions_catalog":["ACTION_TRACE_STEP","ACTION_REFRESH_SD"],"steps":[{"id":"STEP_WAIT_UNLOCK","screen_scene_id":"SCENE_LOCKED"},{"id":"STEP_WIN","screen_scene_id":"SCENE_REWARD","audio_pack_id":"PACK_WIN"},{"id":"STEP_DONE","screen_scene_id":"SCENE_READY"}],"source":"story_selector","screen_root":"/story/screens","audio_root":"/story/audio"})JSON"},
    {"/story/scenarios/EXEMPLE_UNLOCK_EXPRESS_DONE.json", R"JSON({"id":"EXEMPLE_UNLOCK_EXPRESS_DONE","scenario":"EXEMPLE_UNLOCK_EXPRESS_DONE","version":2,"initial_step":"STEP_WAIT_UNLOCK","hardware_events":{"button_short_1":"UNLOCK","button_short_5":"BTN_NEXT","button_long_4":"FORCE_DONE"},"app_bindings":["APP_LA","APP_SCREEN","APP_GATE","APP_AUDIO","APP_WIFI","APP_ESPNOW"],"actions_catalog":["ACTION_TRACE_STEP","ACTION_REFRESH_SD"],"steps":[{"id":"STEP_WAIT_UNLOCK","screen_scene_id":"SCENE_LOCKED"},{"id":"STEP_WIN","screen_scene_id":"SCENE_REWARD","audio_pack_id":"PACK_WIN"},{"id":"STEP_DONE","screen_scene_id":"SCENE_READY"}],"source":"story_selector","screen_root":"/story/screens","audio_root":"/story/audio"})JSON"},
    {"/story/scenarios/SPECTRE_RADIO_LAB.json", R"JSON({"id":"SPECTRE_RADIO_LAB","scenario":"SPECTRE_RADIO_LAB","version":2,"initial_step":"STEP_WAIT_UNLOCK","hardware_events":{"button_short_1":"UNLOCK","button_short_5":"BTN_NEXT","button_long_4":"FORCE_DONE","espnow_event":"SERIAL:<payload>"},"app_bindings":["APP_LA","APP_AUDIO","APP_SCREEN","APP_GATE","APP_WIFI","APP_ESPNOW"],"actions_catalog":["ACTION_TRACE_STEP","ACTION_QUEUE_SONAR","ACTION_REFRESH_SD"],"steps":[{"id":"STEP_WAIT_UNLOCK","screen_scene_id":"SCENE_LOCKED"},{"id":"STEP_SONAR_SEARCH","screen_scene_id":"SCENE_SEARCH","audio_pack_id":"PACK_SONAR_HINT"},{"id":"STEP_MORSE_CLUE","screen_scene_id":"SCENE_SEARCH","audio_pack_id":"PACK_MORSE_HINT"},{"id":"STEP_WIN","screen_scene_id":"SCENE_REWARD","audio_pack_id":"PACK_WIN"},{"id":"STEP_DONE","screen_scene_id":"SCENE_READY"}],"source":"story_selector","screen_root":"/story/screens","audio_root":"/story/audio"})JSON"},
    {"/story/scenarios/ZACUS_V1_UNLOCK_ETAPE2.json", R"JSON({"id":"ZACUS_V1_UNLOCK_ETAPE2","scenario":"ZACUS_V1_UNLOCK_ETAPE2","version":2,"initial_step":"STEP_BOOT_WAIT","hardware_events":{"button_short_1":"UNLOCK","button_short_5":"BTN_NEXT","button_long_3":"FORCE_ETAPE2","button_long_4":"FORCE_DONE","espnow_event":"SERIAL:<payload>"},"app_bindings":["APP_LA","APP_AUDIO","APP_SCREEN","APP_GATE","APP_WIFI","APP_ESPNOW"],"actions_catalog":["ACTION_TRACE_STEP","ACTION_REFRESH_SD"],"steps":[{"id":"STEP_BOOT_WAIT","screen_scene_id":"SCENE_LOCKED"},{"id":"STEP_BOOT_USON","screen_scene_id":"SCENE_LOCKED","audio_pack_id":"PACK_BOOT_RADIO"},{"id":"STEP_LA_DETECT","screen_scene_id":"SCENE_SEARCH"},{"id":"STEP_WIN","screen_scene_id":"SCENE_REWARD","audio_pack_id":"PACK_WIN"},{"id":"STEP_DONE","screen_scene_id":"SCENE_READY"}],"source":"story_selector","screen_root":"/story/screens","audio_root":"/story/audio"})JSON"},
    {"/story/screens/SCENE_BROKEN.json", R"JSON({"id":"SCENE_BROKEN","title":"PROTO U-SON","subtitle":"Signal brouille","symbol":"ALERT","effect":"blink","visual":{"show_title":false,"show_symbol":true,"effect_speed_ms":180},"theme":{"bg":"#2A0508","accent":"#FF4A45","text":"#FFF1F1"},"timeline":{"loop":true,"duration_ms":900,"keyframes":[{"at_ms":0,"effect":"blink","speed_ms":180,"theme":{"bg":"#2A0508","accent":"#FF4A45","text":"#FFF1F1"}},{"at_ms":900,"effect":"scan","speed_ms":520,"theme":{"bg":"#3A0A10","accent":"#FF7873","text":"#FFF7F7"}}]},"transition":{"effect":"camera_flash","duration_ms":160}})JSON"},
    {"/story/screens/SCENE_CAMERA_SCAN.json", R"JSON({"id":"SCENE_CAMERA_SCAN","title":"CAMERA SCAN","subtitle":"Capture des indices visuels","symbol":"SCAN","effect":"radar","visual":{"show_title":false,"show_symbol":true,"effect_speed_ms":840},"theme":{"bg":"#041A24","accent":"#5CE6FF","text":"#E9FBFF"},"timeline":{"loop":true,"duration_ms":2200,"keyframes":[{"at_ms":0,"effect":"radar","speed_ms":840,"theme":{"bg":"#041A24","accent":"#5CE6FF","text":"#E9FBFF"}},{"at_ms":1200,"effect":"wave","speed_ms":620,"theme":{"bg":"#072838","accent":"#8AF1FF","text":"#F5FEFF"}},{"at_ms":2200,"effect":"radar","speed_ms":760,"theme":{"bg":"#041A24","accent":"#5CE6FF","text":"#E9FBFF"}}]},"transition":{"effect":"wipe","duration_ms":230}})JSON"},
    {"/story/screens/SCENE_LA_DETECT.json", R"JSON({"id":"SCENE_LA_DETECT","title":"DETECTEUR DE RESONNANCE","subtitle":"","symbol":"AUDIO","effect":"wave","visual":{"show_title":true,"show_symbol":true,"effect_speed_ms":480,"waveform":{"enabled":true,"sample_count":16,"amplitude_pct":100,"jitter":true}},"theme":{"bg":"#000000","accent":"#49D9FF","text":"#E8F6FF"},"timeline":{"loop":true,"duration_ms":2400,"keyframes":[{"at_ms":0,"effect":"wave","speed_ms":480,"theme":{"bg":"#000000","accent":"#49D9FF","text":"#E8F6FF"}},{"at_ms":800,"effect":"radar","speed_ms":620,"theme":{"bg":"#000000","accent":"#7EE8FF","text":"#F2FAFF"}},{"at_ms":1600,"effect":"wave","speed_ms":340,"theme":{"bg":"#000000","accent":"#D8FF6B","text":"#F9FFD8"}},{"at_ms":2400,"effect":"radar","speed_ms":700,"theme":{"bg":"#000000","accent":"#49D9FF","text":"#E8F6FF"}}]},"transition":{"effect":"zoom","duration_ms":260}})JSON"},
    {"/story/screens/SCENE_LA_DETECTOR.json", R"JSON({"id":"SCENE_LA_DETECTOR","title":"DETECTEUR DE RESONNANCE","subtitle":"","symbol":"AUDIO","effect":"wave","visual":{"show_title":true,"show_symbol":true,"effect_speed_ms":480,"waveform":{"enabled":true,"sample_count":16,"amplitude_pct":100,"jitter":true}},"theme":{"bg":"#000000","accent":"#49D9FF","text":"#E8F6FF"},"timeline":{"loop":true,"duration_ms":2400,"keyframes":[{"at_ms":0,"effect":"wave","speed_ms":480,"theme":{"bg":"#000000","accent":"#49D9FF","text":"#E8F6FF"}},{"at_ms":800,"effect":"radar","speed_ms":620,"theme":{"bg":"#000000","accent":"#7EE8FF","text":"#F2FAFF"}},{"at_ms":1600,"effect":"wave","speed_ms":340,"theme":{"bg":"#000000","accent":"#D8FF6B","text":"#F9FFD8"}},{"at_ms":2400,"effect":"radar","speed_ms":700,"theme":{"bg":"#000000","accent":"#49D9FF","text":"#E8F6FF"}}]},"transition":{"effect":"zoom","duration_ms":260}})JSON"},
    {"/story/screens/SCENE_LOCKED.json", R"JSON({"id":"SCENE_LOCKED","title":"Module U-SON PROTO","subtitle":"VERIFICATION EN COURS","symbol":"LOCK","effect":"glitch","visual":{"show_title":true,"show_symbol":true,"effect_speed_ms":90},"theme":{"bg":"#06060E","accent":"#FFC766","text":"#F8FCFF"},"demo":{"mode":"arcade","particle_count":4,"strobe_level":100},"timeline":{"loop":true,"duration_ms":1500,"keyframes":[{"at_ms":0,"effect":"glitch","speed_ms":90,"theme":{"bg":"#06060E","accent":"#FFC766","text":"#F8FCFF"}},{"at_ms":220,"effect":"celebrate","speed_ms":170,"theme":{"bg":"#0F0B15","accent":"#FFE17D","text":"#FFFDEE"}},{"at_ms":460,"effect":"glitch","speed_ms":80,"theme":{"bg":"#15090F","accent":"#FF6A5F","text":"#FFF3F0"}},{"at_ms":700,"effect":"wave","speed_ms":150,"theme":{"bg":"#050914","accent":"#6CB9FF","text":"#EAF5FF"}},{"at_ms":920,"effect":"glitch","speed_ms":70,"theme":{"bg":"#17090E","accent":"#FF8E78","text":"#FFF8F3"}},{"at_ms":1160,"effect":"celebrate","speed_ms":150,"theme":{"bg":"#0E0C14","accent":"#FFD86A","text":"#FFFCEB"}},{"at_ms":1360,"effect":"glitch","speed_ms":65,"theme":{"bg":"#16090E","accent":"#FF7A64","text":"#FFF6F1"}},{"at_ms":1500,"effect":"celebrate","speed_ms":180,"theme":{"bg":"#06060E","accent":"#FFE17D","text":"#FFFDEE"}}]},"transition":{"effect":"fade","duration_ms":70}})JSON"},
    {"/story/screens/SCENE_MEDIA_ARCHIVE.json", R"JSON({"id":"SCENE_MEDIA_ARCHIVE","title":"ARCHIVES MEDIA","subtitle":"Photos et enregistrements sauvegardes","symbol":"READY","effect":"radar","visual":{"show_title":false,"show_symbol":true,"effect_speed_ms":760},"theme":{"bg":"#0D1A34","accent":"#7CB1FF","text":"#EEF4FF"},"timeline":{"loop":true,"duration_ms":2000,"keyframes":[{"at_ms":0,"effect":"radar","speed_ms":760,"theme":{"bg":"#0D1A34","accent":"#7CB1FF","text":"#EEF4FF"}},{"at_ms":1000,"effect":"pulse","speed_ms":620,"theme":{"bg":"#132245","accent":"#9CC7FF","text":"#F7FAFF"}},{"at_ms":2000,"effect":"radar","speed_ms":760,"theme":{"bg":"#0D1A34","accent":"#7CB1FF","text":"#EEF4FF"}}]},"transition":{"effect":"fade","duration_ms":240}})JSON"},
    {"/story/screens/SCENE_READY.json", R"JSON({"id":"SCENE_READY","title":"PRET","subtitle":"Scenario termine","symbol":"READY","effect":"wave","visual":{"show_title":false,"show_symbol":true,"effect_speed_ms":560},"theme":{"bg":"#0F2A12","accent":"#6CD96B","text":"#EDFFED"},"timeline":{"loop":true,"duration_ms":1600,"keyframes":[{"at_ms":0,"effect":"wave","speed_ms":560,"theme":{"bg":"#0F2A12","accent":"#6CD96B","text":"#EDFFED"}},{"at_ms":1600,"effect":"radar","speed_ms":740,"theme":{"bg":"#133517","accent":"#9EE49D","text":"#F4FFF4"}}]},"transition":{"effect":"fade","duration_ms":220}})JSON"},
    {"/story/screens/SCENE_REWARD.json", R"JSON({"id":"SCENE_REWARD","title":"RECOMPENSE","subtitle":"Indice debloque","symbol":"WIN","effect":"celebrate","visual":{"show_title":false,"show_symbol":true,"effect_speed_ms":420},"theme":{"bg":"#2A103E","accent":"#F9D860","text":"#FFF9E6"},"timeline":{"loop":true,"duration_ms":1200,"keyframes":[{"at_ms":0,"effect":"celebrate","speed_ms":420,"theme":{"bg":"#2A103E","accent":"#F9D860","text":"#FFF9E6"}},{"at_ms":1200,"effect":"pulse","speed_ms":280,"theme":{"bg":"#3E1A52","accent":"#FFD97D","text":"#FFFDF2"}}]},"transition":{"effect":"zoom","duration_ms":300}})JSON"},
    {"/story/screens/SCENE_SEARCH.json", R"JSON({"id":"SCENE_SEARCH","title":"RECHERCHE","subtitle":"Analyse des indices","symbol":"SCAN","effect":"scan","visual":{"show_title":false,"show_symbol":true,"effect_speed_ms":920},"theme":{"bg":"#05261F","accent":"#35E7B0","text":"#EFFFF8"},"timeline":{"loop":true,"duration_ms":3000,"keyframes":[{"at_ms":0,"effect":"scan","speed_ms":920,"theme":{"bg":"#05261F","accent":"#35E7B0","text":"#EFFFF8"}},{"at_ms":1600,"effect":"wave","speed_ms":520,"theme":{"bg":"#07322A","accent":"#67F0C4","text":"#F2FFF9"}},{"at_ms":3000,"effect":"scan","speed_ms":820,"theme":{"bg":"#05261F","accent":"#35E7B0","text":"#EFFFF8"}}]},"transition":{"effect":"camera_flash","duration_ms":230}})JSON"},
    {"/story/screens/SCENE_SIGNAL_SPIKE.json", R"JSON({"id":"SCENE_SIGNAL_SPIKE","title":"PIC DE SIGNAL","subtitle":"Interference soudaine detectee","symbol":"ALERT","effect":"wave","visual":{"show_title":false,"show_symbol":true,"effect_speed_ms":260},"theme":{"bg":"#24090C","accent":"#FF6A52","text":"#FFF2EB"},"timeline":{"loop":true,"duration_ms":1400,"keyframes":[{"at_ms":0,"effect":"wave","speed_ms":260,"theme":{"bg":"#24090C","accent":"#FF6A52","text":"#FFF2EB"}},{"at_ms":700,"effect":"blink","speed_ms":180,"theme":{"bg":"#2F1014","accent":"#FF8C73","text":"#FFF8F5"}},{"at_ms":1400,"effect":"wave","speed_ms":320,"theme":{"bg":"#24090C","accent":"#FF6A52","text":"#FFF2EB"}}]},"transition":{"effect":"camera_flash","duration_ms":170}})JSON"},
    {"/story/screens/SCENE_WIN.json", R"JSON({"id":"SCENE_WIN","title":"VICTOIRE","subtitle":"Etape validee","symbol":"WIN","effect":"celebrate","visual":{"show_title":false,"show_symbol":true,"effect_speed_ms":420},"theme":{"bg":"#231038","accent":"#F4CB4A","text":"#FFF8E2"},"timeline":{"loop":true,"duration_ms":1000,"keyframes":[{"at_ms":0,"effect":"celebrate","speed_ms":420,"theme":{"bg":"#231038","accent":"#F4CB4A","text":"#FFF8E2"}},{"at_ms":1000,"effect":"blink","speed_ms":240,"theme":{"bg":"#341A4D","accent":"#FFE083","text":"#FFFDF3"}}]},"transition":{"effect":"zoom","duration_ms":280}})JSON"},
};

uint32_t fnv1aUpdate(uint32_t hash, uint8_t value) {
  hash ^= value;
  hash *= 16777619UL;
  return hash;
}

bool ensureParentDirectories(fs::FS& file_system, const char* file_path) {
  if (file_path == nullptr || file_path[0] != '/') {
    return false;
  }

  String parent_path = file_path;
  const int last_slash = parent_path.lastIndexOf('/');
  if (last_slash <= 0) {
    return true;
  }
  parent_path = parent_path.substring(0, static_cast<unsigned int>(last_slash));
  if (parent_path.isEmpty()) {
    return true;
  }

  int segment_start = 1;
  String current_path;
  while (segment_start < static_cast<int>(parent_path.length())) {
    const int next_slash = parent_path.indexOf('/', segment_start);
    const int segment_end = (next_slash < 0) ? static_cast<int>(parent_path.length()) : next_slash;
    if (segment_end <= segment_start) {
      break;
    }
    current_path += "/";
    current_path += parent_path.substring(segment_start, static_cast<unsigned int>(segment_end));
    if (!file_system.exists(current_path.c_str()) && !file_system.mkdir(current_path.c_str())) {
      return false;
    }
    if (next_slash < 0) {
      break;
    }
    segment_start = next_slash + 1;
  }
  return true;
}

String normalizeAssetPath(const char* raw_path) {
  if (raw_path == nullptr || raw_path[0] == '\0') {
    return String();
  }
  String normalized = raw_path;
  if (!normalized.startsWith("/")) {
    normalized = "/" + normalized;
  }
  return normalized;
}

String sceneIdToSlug(const char* scene_id) {
  if (scene_id == nullptr || scene_id[0] == '\0') {
    return String();
  }
  String slug = scene_id;
  if (slug.startsWith("SCENE_")) {
    slug = slug.substring(6);
  }
  slug.toLowerCase();
  return slug;
}

String packIdToSlug(const char* pack_id) {
  if (pack_id == nullptr || pack_id[0] == '\0') {
    return String();
  }
  String slug = pack_id;
  if (slug.startsWith("PACK_")) {
    slug = slug.substring(5);
  }
  slug.toLowerCase();
  return slug;
}

bool startsWithIgnoreCase(const char* text, const char* prefix) {
  if (text == nullptr || prefix == nullptr) {
    return false;
  }
  for (size_t index = 0U;; ++index) {
    const char lhs = text[index];
    const char rhs = prefix[index];
    if (rhs == '\0') {
      return true;
    }
    if (lhs == '\0') {
      return false;
    }
    const char lhs_lower = static_cast<char>(std::tolower(static_cast<unsigned char>(lhs)));
    const char rhs_lower = static_cast<char>(std::tolower(static_cast<unsigned char>(rhs)));
    if (lhs_lower != rhs_lower) {
      return false;
    }
  }
}

}  // namespace

bool StorageManager::begin() {
  if (!LittleFS.begin()) {
    Serial.println("[FS] LittleFS mount failed");
    return false;
  }

  for (const char* path : kRequiredDirectories) {
    ensurePath(path);
  }
  sd_ready_ = mountSdCard();
  Serial.printf("[FS] LittleFS ready (sd=%u)\n", sd_ready_ ? 1U : 0U);
  return true;
}

bool StorageManager::mountSdCard() {
#if ZACUS_HAS_SD_MMC
  SD_MMC.end();
  SD_MMC.setPins(FREENOVE_SDMMC_CLK, FREENOVE_SDMMC_CMD, FREENOVE_SDMMC_D0);
  if (!SD_MMC.begin("/sdcard", true)) {
    Serial.println("[FS] SD_MMC unavailable");
    return false;
  }
  const uint8_t card_type = SD_MMC.cardType();
  if (card_type == CARD_NONE) {
    SD_MMC.end();
    Serial.println("[FS] SD_MMC card not detected");
    return false;
  }
  Serial.printf("[FS] SD_MMC mounted size=%lluMB\n",
                static_cast<unsigned long long>(SD_MMC.cardSize() / (1024ULL * 1024ULL)));
  return true;
#else
  return false;
#endif
}

bool StorageManager::ensurePath(const char* path) {
  if (path == nullptr || path[0] == '\0') {
    return false;
  }
  if (LittleFS.exists(path)) {
    return true;
  }
  if (!LittleFS.mkdir(path)) {
    Serial.printf("[FS] mkdir failed: %s\n", path);
    return false;
  }
  Serial.printf("[FS] mkdir: %s\n", path);
  return true;
}

String StorageManager::normalizeAbsolutePath(const char* path) const {
  if (path == nullptr || path[0] == '\0') {
    return String();
  }
  String normalized = path;
  normalized.trim();
  if (normalized.isEmpty()) {
    return String();
  }
  if (!normalized.startsWith("/")) {
    normalized = "/" + normalized;
  }
  return normalized;
}

String StorageManager::stripSdPrefix(const char* path) const {
  String normalized = normalizeAbsolutePath(path);
  if (normalized.isEmpty()) {
    return normalized;
  }
  if (startsWithIgnoreCase(normalized.c_str(), "/sd/")) {
    return normalized.substring(3);
  }
  if (startsWithIgnoreCase(normalized.c_str(), "/sd")) {
    return "/";
  }
  return normalized;
}

bool StorageManager::pathExistsOnLittleFs(const char* path) const {
  const String normalized = normalizeAbsolutePath(path);
  if (normalized.isEmpty()) {
    return false;
  }
  return LittleFS.exists(normalized.c_str());
}

bool StorageManager::pathExistsOnSdCard(const char* path) const {
  if (!sd_ready_) {
    return false;
  }
#if ZACUS_HAS_SD_MMC
  const String sd_path = stripSdPrefix(path);
  if (sd_path.isEmpty()) {
    return false;
  }
  return SD_MMC.exists(sd_path.c_str());
#else
  (void)path;
  return false;
#endif
}

bool StorageManager::fileExists(const char* path) const {
  const String normalized = normalizeAbsolutePath(path);
  if (normalized.isEmpty()) {
    return false;
  }
  if (startsWithIgnoreCase(normalized.c_str(), "/sd/")) {
    return pathExistsOnSdCard(normalized.c_str());
  }
  return pathExistsOnLittleFs(normalized.c_str()) || pathExistsOnSdCard(normalized.c_str());
}

bool StorageManager::readTextFromLittleFs(const char* path, String* out_payload) const {
  if (out_payload == nullptr || !pathExistsOnLittleFs(path)) {
    return false;
  }
  File file = LittleFS.open(path, "r");
  if (!file) {
    return false;
  }
  out_payload->remove(0);
  out_payload->reserve(static_cast<size_t>(file.size()) + 1U);
  while (file.available()) {
    *out_payload += static_cast<char>(file.read());
  }
  file.close();
  return !out_payload->isEmpty();
}

bool StorageManager::readTextFromSdCard(const char* path, String* out_payload) const {
  if (out_payload == nullptr || !pathExistsOnSdCard(path)) {
    return false;
  }
#if ZACUS_HAS_SD_MMC
  const String sd_path = stripSdPrefix(path);
  File file = SD_MMC.open(sd_path.c_str(), "r");
  if (!file) {
    return false;
  }
  out_payload->remove(0);
  out_payload->reserve(static_cast<size_t>(file.size()) + 1U);
  while (file.available()) {
    *out_payload += static_cast<char>(file.read());
  }
  file.close();
  return !out_payload->isEmpty();
#else
  (void)path;
  return false;
#endif
}

bool StorageManager::readTextFileWithOrigin(const char* path, String* out_payload, String* out_origin) const {
  if (out_payload == nullptr) {
    return false;
  }
  const String normalized = normalizeAbsolutePath(path);
  if (normalized.isEmpty()) {
    return false;
  }
  const bool force_sd = startsWithIgnoreCase(normalized.c_str(), "/sd/");
  const bool prefer_sd = !force_sd && startsWithIgnoreCase(normalized.c_str(), "/story/");

  String payload;
  if (force_sd) {
    if (!readTextFromSdCard(normalized.c_str(), &payload)) {
      return false;
    }
    *out_payload = payload;
    if (out_origin != nullptr) {
      *out_origin = stripSdPrefix(normalized.c_str());
      *out_origin = "/sd" + *out_origin;
    }
    return true;
  }

  if (prefer_sd && readTextFromSdCard(normalized.c_str(), &payload)) {
    *out_payload = payload;
    if (out_origin != nullptr) {
      *out_origin = "/sd" + stripSdPrefix(normalized.c_str());
    }
    return true;
  }
  if (readTextFromLittleFs(normalized.c_str(), &payload)) {
    *out_payload = payload;
    if (out_origin != nullptr) {
      *out_origin = normalized;
    }
    return true;
  }
  if (readTextFromSdCard(normalized.c_str(), &payload)) {
    *out_payload = payload;
    if (out_origin != nullptr) {
      *out_origin = "/sd" + stripSdPrefix(normalized.c_str());
    }
    return true;
  }
  return false;
}

String StorageManager::loadTextFile(const char* path) const {
  String payload;
  String origin;
  if (!readTextFileWithOrigin(path, &payload, &origin)) {
    return String();
  }
  return payload;
}

String StorageManager::resolveReadableAssetPath(const String& absolute_path) const {
  if (absolute_path.isEmpty()) {
    return String();
  }
  if (startsWithIgnoreCase(absolute_path.c_str(), "/sd/")) {
    return pathExistsOnSdCard(absolute_path.c_str()) ? absolute_path : String();
  }
  if (pathExistsOnLittleFs(absolute_path.c_str())) {
    return absolute_path;
  }
  if (pathExistsOnSdCard(absolute_path.c_str())) {
    return "/sd" + absolute_path;
  }
  return String();
}

String StorageManager::loadScenePayloadById(const char* scene_id) const {
  if (scene_id == nullptr || scene_id[0] == '\0') {
    return String();
  }

  const String id = scene_id;
  const String slug = sceneIdToSlug(scene_id);
  const String candidates[] = {
      "/story/screens/" + id + ".json",
      "/story/screens/" + slug + ".json",
      "/screens/" + id + ".json",
      "/screens/" + slug + ".json",
      "/scenarios/data/scene_" + slug + ".json",
      "/sd/story/screens/" + id + ".json",
      "/sd/story/screens/" + slug + ".json",
  };

  for (const String& candidate : candidates) {
    String payload;
    String origin;
    if (!readTextFileWithOrigin(candidate.c_str(), &payload, &origin)) {
      continue;
    }
    Serial.printf("[FS] scene %s -> %s\n", scene_id, origin.c_str());
    return payload;
  }

  Serial.printf("[FS] scene payload missing for id=%s\n", scene_id);
  return String();
}

String StorageManager::resolveAudioPathByPackId(const char* pack_id) const {
  if (pack_id == nullptr || pack_id[0] == '\0') {
    return String();
  }

  const String id = pack_id;
  const String slug = packIdToSlug(pack_id);
  const String json_candidates[] = {
      "/story/audio/" + id + ".json",
      "/story/audio/" + slug + ".json",
      "/audio/" + id + ".json",
      "/audio/" + slug + ".json",
      "/sd/story/audio/" + id + ".json",
      "/sd/story/audio/" + slug + ".json",
  };

  for (const String& json_path : json_candidates) {
    String payload;
    String origin;
    if (!readTextFileWithOrigin(json_path.c_str(), &payload, &origin) || payload.isEmpty()) {
      continue;
    }

    StaticJsonDocument<384> document;
    const DeserializationError error = deserializeJson(document, payload);
    if (error) {
      Serial.printf("[FS] invalid audio pack json %s (%s)\n", origin.c_str(), error.c_str());
      continue;
    }

    const char* file_path = document["file"] | document["path"] | document["asset"];
    if (file_path == nullptr || file_path[0] == '\0') {
      file_path = document["content"]["file"] | document["content"]["path"] | document["content"]["asset"];
    }

    const char* asset_id = "";
    if (file_path == nullptr || file_path[0] == '\0') {
      asset_id = document["asset_id"] | "";
      if (asset_id[0] == '\0') {
        asset_id = document["assetId"] | "";
      }
      if (asset_id[0] == '\0') {
        asset_id = document["content"]["asset_id"] | "";
      }
      if (asset_id[0] == '\0') {
        asset_id = document["content"]["assetId"] | "";
      }
      if (asset_id[0] != '\0') {
        const String asset_name = asset_id;
        const String asset_candidates[] = {
            "/music/" + asset_name,
            "/audio/" + asset_name,
            "/music/" + asset_name + ".mp3",
            "/audio/" + asset_name + ".mp3",
            "/music/" + asset_name + ".wav",
            "/audio/" + asset_name + ".wav",
        };
        for (const String& asset_candidate : asset_candidates) {
          const String resolved = resolveReadableAssetPath(asset_candidate);
          if (resolved.isEmpty()) {
            continue;
          }
          Serial.printf("[FS] audio pack %s asset_id -> %s (%s)\n",
                        pack_id,
                        resolved.c_str(),
                        origin.c_str());
          return resolved;
        }
      }
      Serial.printf("[FS] audio pack missing file/path: %s\n", origin.c_str());
      continue;
    }

    const String normalized = normalizeAssetPath(file_path);
    const String resolved = resolveReadableAssetPath(normalized);
    if (resolved.isEmpty()) {
      Serial.printf("[FS] audio pack path missing on storage: %s (%s)\n", normalized.c_str(), origin.c_str());
      continue;
    }
    Serial.printf("[FS] audio pack %s -> %s (%s)\n", pack_id, resolved.c_str(), origin.c_str());
    return resolved;
  }

  const String direct_candidates[] = {
      "/music/" + id + ".mp3",
      "/music/" + id + ".wav",
      "/audio/" + id + ".mp3",
      "/audio/" + id + ".wav",
      "/music/" + slug + ".mp3",
      "/music/" + slug + ".wav",
      "/audio/" + slug + ".mp3",
      "/audio/" + slug + ".wav",
  };
  for (const String& candidate : direct_candidates) {
    const String resolved = resolveReadableAssetPath(candidate);
    if (resolved.isEmpty()) {
      continue;
    }
    Serial.printf("[FS] audio pack %s fallback direct=%s\n", pack_id, resolved.c_str());
    return resolved;
  }

  return String();
}

bool StorageManager::ensureParentDirectoriesOnLittleFs(const char* file_path) const {
  return ensureParentDirectories(LittleFS, file_path);
}

bool StorageManager::writeTextToLittleFs(const char* path, const char* payload) const {
  if (path == nullptr || payload == nullptr || path[0] != '/') {
    return false;
  }
  if (!ensureParentDirectoriesOnLittleFs(path)) {
    return false;
  }
  File file = LittleFS.open(path, "w");
  if (!file) {
    return false;
  }
  const size_t written = file.print(payload);
  file.close();
  return written > 0U;
}

bool StorageManager::copyFileFromSdToLittleFs(const char* src_path, const char* dst_path) const {
  if (!sd_ready_ || src_path == nullptr || dst_path == nullptr || src_path[0] != '/' || dst_path[0] != '/') {
    return false;
  }
#if ZACUS_HAS_SD_MMC
  if (!pathExistsOnSdCard(src_path)) {
    return false;
  }
  const String sd_path = stripSdPrefix(src_path);
  File src = SD_MMC.open(sd_path.c_str(), "r");
  if (!src) {
    return false;
  }
  if (!ensureParentDirectoriesOnLittleFs(dst_path)) {
    src.close();
    return false;
  }
  File dst = LittleFS.open(dst_path, "w");
  if (!dst) {
    src.close();
    return false;
  }
  uint8_t buffer[512];
  while (src.available()) {
    const size_t read_bytes = src.read(buffer, sizeof(buffer));
    if (read_bytes == 0U) {
      break;
    }
    if (dst.write(buffer, read_bytes) != read_bytes) {
      dst.close();
      src.close();
      return false;
    }
  }
  dst.close();
  src.close();
  return true;
#else
  (void)src_path;
  (void)dst_path;
  return false;
#endif
}

bool StorageManager::syncStoryFileFromSd(const char* story_path) {
  if (!sd_ready_ || story_path == nullptr || story_path[0] == '\0') {
    return false;
  }
  const String normalized = normalizeAbsolutePath(story_path);
  if (normalized.isEmpty() || !pathExistsOnSdCard(normalized.c_str())) {
    return false;
  }
  const bool copied = copyFileFromSdToLittleFs(normalized.c_str(), normalized.c_str());
  if (copied) {
    Serial.printf("[FS] synced story file from SD: %s\n", normalized.c_str());
  }
  return copied;
}

bool StorageManager::copyStoryDirectoryFromSd(const char* relative_dir) {
  if (!sd_ready_ || relative_dir == nullptr || relative_dir[0] == '\0') {
    return false;
  }
#if ZACUS_HAS_SD_MMC
  String source_dir = "/story/";
  source_dir += relative_dir;
  if (!pathExistsOnSdCard(source_dir.c_str())) {
    return false;
  }

  File dir = SD_MMC.open(source_dir.c_str());
  if (!dir || !dir.isDirectory()) {
    return false;
  }

  bool copied_any = false;
  File entry = dir.openNextFile();
  while (entry) {
    if (!entry.isDirectory()) {
      String src_path = entry.name();
      if (!src_path.isEmpty()) {
        if (copyFileFromSdToLittleFs(src_path.c_str(), src_path.c_str())) {
          copied_any = true;
        }
      }
    }
    entry.close();
    entry = dir.openNextFile();
  }
  return copied_any;
#else
  (void)relative_dir;
  return false;
#endif
}

bool StorageManager::provisionEmbeddedAsset(const char* path,
                                            const char* payload,
                                            bool* out_written) const {
  if (out_written != nullptr) {
    *out_written = false;
  }
  if (path == nullptr || path[0] == '\0' || payload == nullptr) {
    return false;
  }
  if (pathExistsOnLittleFs(path)) {
    return true;
  }
  if (!writeTextToLittleFs(path, payload)) {
    return false;
  }
  if (out_written != nullptr) {
    *out_written = true;
  }
  return true;
}

bool StorageManager::syncStoryTreeFromSd() {
  if (!sd_ready_) {
    return false;
  }
  const char* story_dirs[] = {"scenarios", "screens", "audio", "apps", "actions"};
  bool copied_any = false;
  for (const char* relative_dir : story_dirs) {
    copied_any = copyStoryDirectoryFromSd(relative_dir) || copied_any;
  }
  if (copied_any) {
    Serial.println("[FS] story tree refreshed from SD");
  }
  return copied_any;
}

bool StorageManager::ensureDefaultStoryBundle() {
  uint16_t written_count = 0U;
  for (const EmbeddedStoryAsset& asset : kEmbeddedStoryAssets) {
    bool written = false;
    if (provisionEmbeddedAsset(asset.path, asset.payload, &written) && written) {
      ++written_count;
    }
  }
  if (written_count > 0U) {
    Serial.printf("[FS] provisioned embedded story assets: %u\n", written_count);
  }
  return true;
}

bool StorageManager::ensureDefaultScenarioFile(const char* path) {
  const String normalized = normalizeAbsolutePath(path);
  if (normalized.isEmpty()) {
    return false;
  }
  if (pathExistsOnLittleFs(normalized.c_str())) {
    return true;
  }
  if (syncStoryFileFromSd(normalized.c_str())) {
    return true;
  }

  const ScenarioDef* scenario = storyScenarioV2Default();
  if (scenario == nullptr) {
    Serial.println("[FS] built-in scenario unavailable");
    return false;
  }

  StaticJsonDocument<256> document;
  document["scenario"] = (scenario->id != nullptr) ? scenario->id : "DEFAULT";
  document["source"] = "auto-fallback";
  document["version"] = scenario->version;
  document["step_count"] = scenario->stepCount;
  String payload;
  serializeJson(document, payload);
  payload += "\n";
  if (!writeTextToLittleFs(normalized.c_str(), payload.c_str())) {
    Serial.printf("[FS] cannot create default scenario file: %s\n", normalized.c_str());
    return false;
  }
  Serial.printf("[FS] default scenario provisioned: %s\n", normalized.c_str());
  return true;
}

bool StorageManager::hasSdCard() const {
  return sd_ready_;
}

uint32_t StorageManager::checksum(const char* path) const {
  const String normalized = normalizeAbsolutePath(path);
  if (normalized.isEmpty()) {
    return 0U;
  }

  File file;
  if (pathExistsOnLittleFs(normalized.c_str())) {
    file = LittleFS.open(normalized.c_str(), "r");
  } else if (pathExistsOnSdCard(normalized.c_str())) {
#if ZACUS_HAS_SD_MMC
    file = SD_MMC.open(stripSdPrefix(normalized.c_str()).c_str(), "r");
#endif
  }
  if (!file) {
    return 0U;
  }

  uint32_t hash = 2166136261UL;
  while (file.available()) {
    const int value = file.read();
    if (value < 0) {
      break;
    }
    hash = fnv1aUpdate(hash, static_cast<uint8_t>(value));
  }
  file.close();
  return hash;
}
