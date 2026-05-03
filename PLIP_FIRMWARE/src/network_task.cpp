// network_task — WiFi station + mDNS discovery for the Zacus master.
//
// REST contract (consumed by the Zacus master ESP32, server impl TBD):
//   POST /ring     { "duration_ms": 4000 }    -> trigger SLIC ring (or beep on dev kit)
//   POST /play     { "source": "sd:/intro.mp3" | "http://tower:8001/..." }
//   POST /stop                                  -> stop current playback
//   GET  /status                                -> { "off_hook": bool, "playing": bool }
//
// WiFi credentials come from NVS (provisioned via the desktop NvsConfigurator,
// namespace "wifi", keys "ssid"/"pwd"). On a fresh device with no NVS entry we
// fall back to the open SSID `ZACUS-SETUP` so a bringup operator can still
// reach the unit on a captive AP. Replace the open fallback with a proper
// SoftAP provisioning flow before shipping.
//
// Once the station is up we advertise ourselves as `plip.local` and try to
// resolve `zacus-master.local` (slice 12 contract) so `zacus_hook_client`
// can POST to http://zacus-master.local/voice/hook without DNS surprises.

#include <Arduino.h>
#include <ESPmDNS.h>
#include <Preferences.h>
#include <WiFi.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

namespace {

constexpr char kFallbackSsid[] = "ZACUS-SETUP";
constexpr char kFallbackPwd[]  = "";  // open AP, dev only
constexpr char kHostname[]     = "plip";
constexpr char kMasterHost[]   = "zacus-master.local";

constexpr uint32_t kConnectTimeoutMs = 30000;
constexpr uint32_t kPollPeriodMs     = 500;
constexpr uint32_t kHealthPeriodMs   = 5000;

struct WifiCreds {
  String ssid;
  String pwd;
  bool   from_nvs;
};

WifiCreds load_credentials() {
  WifiCreds creds{};
  Preferences prefs;
  // Read-only open; if the namespace is missing this still returns true on
  // ESP32 Preferences but the keys come back empty.
  if (prefs.begin("wifi", true)) {
    creds.ssid = prefs.getString("ssid", "");
    creds.pwd  = prefs.getString("pwd", "");
    prefs.end();
  }
  if (creds.ssid.length() > 0) {
    creds.from_nvs = true;
  } else {
    creds.ssid = kFallbackSsid;
    creds.pwd  = kFallbackPwd;
    creds.from_nvs = false;
  }
  return creds;
}

bool connect_wifi(const WifiCreds &creds) {
  Serial.printf("[net] WiFi connecting to SSID=\"%s\" (source=%s)\n",
                creds.ssid.c_str(),
                creds.from_nvs ? "NVS" : "fallback");
  WiFi.persistent(false);
  WiFi.mode(WIFI_STA);
  WiFi.setHostname(kHostname);
  WiFi.begin(creds.ssid.c_str(), creds.pwd.c_str());

  const uint32_t deadline = millis() + kConnectTimeoutMs;
  while (WiFi.status() != WL_CONNECTED && millis() < deadline) {
    vTaskDelay(pdMS_TO_TICKS(kPollPeriodMs));
  }
  if (WiFi.status() != WL_CONNECTED) {
    Serial.printf("[net] WiFi connect timeout after %lu ms (status=%d)\n",
                  static_cast<unsigned long>(kConnectTimeoutMs),
                  static_cast<int>(WiFi.status()));
    return false;
  }
  Serial.printf("[net] WiFi up — IP=%s RSSI=%d dBm\n",
                WiFi.localIP().toString().c_str(), WiFi.RSSI());
  return true;
}

void start_mdns_and_probe_master() {
  if (!MDNS.begin(kHostname)) {
    Serial.println(F("[net] mDNS init failed"));
    return;
  }
  Serial.printf("[net] mDNS hostname=%s.local\n", kHostname);

  // ESPmDNS::queryHost wants the bare label, not the fully-qualified form.
  const char *probe_label = "zacus-master";
  const IPAddress master_ip = MDNS.queryHost(probe_label, 2000);
  if (master_ip == IPAddress(0, 0, 0, 0)) {
    Serial.printf("[net] mDNS probe %s.local — not found (master may be offline)\n",
                  probe_label);
  } else {
    Serial.printf("[net] mDNS probe %s.local — IP=%s\n",
                  probe_label, master_ip.toString().c_str());
  }
}

void network_task(void *) {
  Serial.println(F("[net] task ready"));

  const WifiCreds creds = load_credentials();
  if (connect_wifi(creds)) {
    start_mdns_and_probe_master();
  } else {
    Serial.println(F("[net] proceeding offline — will retry every 5 s"));
  }

  // TODO(bringup): start ESPAsyncWebServer with the 4 REST handlers above.

  for (;;) {
    vTaskDelay(pdMS_TO_TICKS(kHealthPeriodMs));
    if (WiFi.status() == WL_CONNECTED) {
      continue;
    }
    Serial.println(F("[net] WiFi dropped — reconnecting"));
    // Re-read creds in case the desktop pushed new ones via NVS while up.
    const WifiCreds fresh = load_credentials();
    if (connect_wifi(fresh)) {
      start_mdns_and_probe_master();
    }
  }
}

}  // namespace

void start_network_task() {
  xTaskCreatePinnedToCore(network_task, "net", 8192, nullptr, 3, nullptr, 0);
}
