#include "web_ui_service.h"

#include <ArduinoJson.h>
#include <LittleFS.h>
#include <DNSServer.h>
#include <ESPAsyncWebServer.h>
#include <esp_heap_caps.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <WiFi.h>
#include <cctype>
#include <cstring>

#include "../../audio/mp3_player.h"
#include "controllers/story/story_controller_v2.h"
#include "../../runtime/radio_runtime.h"
#include "fs/story_fs_manager.h"
#include "generated/scenarios_gen.h"
#include "../network/wifi_service.h"
#include "../radio/radio_service.h"
#include "../serial/serial_commands_story.h"

namespace {

class StringPrint : public Print {
 public:
  size_t write(uint8_t c) override {
    buffer_ += static_cast<char>(c);
    return 1U;
  }

  size_t write(const uint8_t* data, size_t size) override {
    if (data == nullptr || size == 0U) {
      return 0U;
    }
    buffer_.reserve(buffer_.length() + size + 1U);
    for (size_t i = 0U; i < size; ++i) {
      buffer_ += static_cast<char>(data[i]);
    }
    return size;
  }

  const String& str() const { return buffer_; }
  void clear() { buffer_ = ""; }

 private:
  String buffer_;
};

struct RtosSnapshot {
  uint32_t taskCount = 0U;
  uint32_t heapFree = 0U;
  uint32_t heapMin = 0U;
  uint32_t heapSize = 0U;
  uint32_t stackMinWords = 0U;
  uint32_t stackMinBytes = 0U;
};

RtosSnapshot buildRtosSnapshot() {
  RtosSnapshot snap;
  snap.taskCount = static_cast<uint32_t>(uxTaskGetNumberOfTasks());
  snap.heapFree = ESP.getFreeHeap();
  snap.heapSize = ESP.getHeapSize();
  snap.heapMin = heap_caps_get_minimum_free_size(MALLOC_CAP_8BIT);
  const UBaseType_t stackWords = uxTaskGetStackHighWaterMark(nullptr);
  snap.stackMinWords = static_cast<uint32_t>(stackWords);
  snap.stackMinBytes = static_cast<uint32_t>(stackWords * sizeof(StackType_t));
  return snap;
}

void copyText(char* out, size_t outLen, const char* text) {
  if (out == nullptr || outLen == 0U) {
    return;
  }
  out[0] = '\0';
  if (text == nullptr || text[0] == '\0') {
    return;
  }
  snprintf(out, outLen, "%s", text);
}

const char kMobileHtml[] PROGMEM =
    "<!doctype html><html><head><meta charset='utf-8'>"
    "<meta name='viewport' content='width=device-width,initial-scale=1'>"
    "<title>U-SON MP3/Radio</title>"
    "<style>"
    ":root{--bg:#0a1016;--panel:#101a23;--line:#1f2d3b;--text:#f0f6fc;--muted:#9fb2c4;--ok:#22c55e;--warn:#f59e0b;}"
    "*{box-sizing:border-box}body{margin:0;font-family:ui-sans-serif,system-ui,-apple-system,Segoe UI,Roboto,sans-serif;background:linear-gradient(170deg,#081018,#101b29);color:var(--text)}"
    ".wrap{max-width:880px;margin:0 auto;padding:12px;display:grid;gap:12px}"
    ".card{background:var(--panel);border:1px solid var(--line);border-radius:12px;padding:12px}"
    "h1{font-size:1.1rem;margin:0 0 8px}h2{font-size:0.95rem;margin:0 0 8px;color:var(--muted)}"
    ".grid{display:grid;grid-template-columns:repeat(2,minmax(0,1fr));gap:8px}"
    "button{width:100%;padding:10px 8px;border-radius:10px;border:1px solid var(--line);background:#0d1520;color:var(--text);font-weight:600}"
    "button:active{transform:translateY(1px)}"
    ".status{display:grid;grid-template-columns:repeat(2,minmax(0,1fr));gap:6px;font-size:.85rem}"
    ".pill{display:inline-block;padding:2px 8px;border-radius:999px;font-size:.75rem}"
    ".ok{background:rgba(34,197,94,.2);color:#89f7b1}.warn{background:rgba(245,158,11,.2);color:#ffd28a}"
    "input,select{width:100%;padding:9px;border-radius:10px;border:1px solid var(--line);background:#0b121a;color:var(--text)}"
    "@media (min-width:780px){.wrap{grid-template-columns:1fr 1fr}.card.wide{grid-column:1 / -1}}"
    "</style></head><body><div class='wrap'>"
    "<div class='card wide'><h1>U-SON Controle Mobile</h1><div id='headline'>Chargement...</div></div>"
    "<div class='card'><h2>Lecteur</h2><div class='grid'>"
    "<button onclick=\"act('toggle')\">Play/Pause</button><button onclick=\"act('next')\">Suivant</button>"
    "<button onclick=\"act('prev')\">Precedent</button><button onclick=\"act('rescan')\">Rescan SD</button>"
    "<button onclick=\"act('vol_down')\">Volume -</button><button onclick=\"act('vol_up')\">Volume +</button>"
    "</div></div>"
    "<div class='card'><h2>Radio</h2><div class='grid'>"
    "<button onclick=\"post('/api/radio/play?id=1')\">Station 1</button><button onclick=\"post('/api/radio/next')\">Station +</button>"
    "<button onclick=\"post('/api/radio/prev')\">Station -</button><button onclick=\"post('/api/radio/stop')\">Stop Radio</button>"
    "</div></div>"
    "<div class='card'><h2>WiFi</h2>"
    "<div class='status'><div>Mode: <span id='wifi_mode'>-</span></div><div>IP: <span id='wifi_ip'>-</span></div>"
    "<div>SSID: <span id='wifi_ssid'>-</span></div><div>Signal: <span id='wifi_rssi'>-</span></div></div>"
    "<div style='margin-top:8px;display:grid;gap:8px'>"
    "<div class='grid'><button onclick='wifiScan()'>Scanner</button><button onclick=\"post('/api/wifi/ap?mode=on')\">AP ON</button></div>"
    "<select id='ssid_list' onchange='pickSsid()'><option value=''>Reseaux disponibles...</option></select>"
    "<input id='ssid' placeholder='SSID'><input id='pass' placeholder='Mot de passe'>"
    "<div class='grid'><button onclick='wifiConnect()'>Connecter</button><div></div></div>"
    "</div></div>"
    "<div class='card'><h2>Statut</h2><pre id='json' style='white-space:pre-wrap;font-size:.72rem;margin:0;max-height:220px;overflow:auto'></pre></div>"
    "</div>"
    "<script>"
    "async function post(u){try{await fetch(u,{method:'POST'});await refresh()}catch(e){}}"
    "async function act(cmd){await post('/api/player/action?cmd='+encodeURIComponent(cmd))}"
    "function pickSsid(){const sel=document.getElementById('ssid_list');if(sel&&sel.value){document.getElementById('ssid').value=sel.value;}}"
    "async function wifiConnect(){const s=document.getElementById('ssid').value.trim();const p=document.getElementById('pass').value;"
    "if(!s)return;await post('/api/wifi/connect?ssid='+encodeURIComponent(s)+'&pass='+encodeURIComponent(p))}"
    "async function wifiScan(){await post('/api/wifi/scan');for(let i=0;i<8;i++){try{const r=await fetch('/api/wifi/scan');const j=await r.json();"
    "if(j.status==='ready'){updateWifiList(j.results||[]);return;}if(j.status==='fail'){return;} }catch(e){} await new Promise(r=>setTimeout(r,800));}}"
    "function updateWifiList(list){const sel=document.getElementById('ssid_list');if(!sel)return;sel.innerHTML='';const empty=document.createElement('option');"
    "empty.value='';empty.textContent='Reseaux disponibles...';sel.appendChild(empty);"
    "list.forEach(n=>{const opt=document.createElement('option');opt.value=n.ssid;opt.textContent=n.ssid+' ('+n.rssi+' dBm)'+(n.secure?' *':'');sel.appendChild(opt);});}"
    "async function refresh(){try{const r=await fetch('/api/status');const j=await r.json();"
    "document.getElementById('headline').textContent=(j.player?.playing?'LECTURE':'PAUSE')+' | '+(j.player?.track||'-')+'/'+(j.player?.tracks||'-')+' | '+(j.radio?.state||'-');"
    "document.getElementById('wifi_mode').textContent=j.wifi?.mode||'-';document.getElementById('wifi_ip').textContent=j.wifi?.ip||'-';"
    "document.getElementById('wifi_ssid').textContent=j.wifi?.ssid||'-';document.getElementById('wifi_rssi').textContent=(j.wifi&&typeof j.wifi.rssi!=='undefined')?j.wifi.rssi:'-';"
    "document.getElementById('json').textContent=JSON.stringify(j,null,2);}catch(e){}}"
    "refresh();setInterval(refresh,1400);"
    "</script></body></html>";

}  // namespace

void WebUiService::begin(WifiService* wifi,
                         RadioService* radio,
                         Mp3Player* mp3,
                         uint16_t port,
                         const Config* cfg) {
  wifi_ = wifi;
  radio_ = radio;
  mp3_ = mp3;
  lastStatusPingMs_ = 0U;
  lastStepId_[0] = '\0';
  auditHead_ = 0U;
  auditCount_ = 0U;
  config_ = Config();
  if (cfg != nullptr) {
    config_ = *cfg;
  }
  snap_ = Snapshot();
  snap_.port = port;

  if (server_ != nullptr) {
    delete server_;
    server_ = nullptr;
  }

  server_ = new AsyncWebServer(port);
  if (server_ == nullptr) {
    setError("ALLOC_FAIL");
    return;
  }

  DefaultHeaders::Instance().addHeader("Access-Control-Allow-Origin", "*");
  DefaultHeaders::Instance().addHeader("Access-Control-Allow-Methods", "GET, POST, PUT, DELETE, OPTIONS");
  DefaultHeaders::Instance().addHeader("Access-Control-Allow-Headers", "Content-Type, Authorization");
  DefaultHeaders::Instance().addHeader("Access-Control-Max-Age", "3600");

  if (ws_ != nullptr) {
    delete ws_;
    ws_ = nullptr;
  }
  ws_ = new AsyncWebSocket("/api/story/stream");
  if (ws_ != nullptr) {
    ws_->onEvent([this](AsyncWebSocket* server,
                        AsyncWebSocketClient* client,
                        AwsEventType type,
                        void* arg,
                        uint8_t* data,
                        size_t len) {
      (void)server;
      (void)arg;
      (void)data;
      (void)len;
      if (type != WS_EVT_CONNECT || client == nullptr) {
        return;
      }
      for (size_t i = 0U; i < auditCount_; ++i) {
        const size_t idx = (auditHead_ + i) % kAuditBufferSize;
        if (auditBuffer_[idx].length() > 0U) {
          client->text(auditBuffer_[idx]);
        }
      }
    });
    server_->addHandler(ws_);
  }

  setupRoutes();
  server_->begin();
  snap_.started = true;
  setRoute("BEGIN");
}

void WebUiService::setStoryContext(StoryControllerV2* story, StoryFsManager* fsManager) {
  story_ = story;
  storyFs_ = fsManager;
}

void WebUiService::setRuntime(RadioRuntime* runtime) {
  runtime_ = runtime;
}

void WebUiService::update(uint32_t nowMs) {
  updateCaptivePortal(nowMs);
  if (ws_ != nullptr) {
    ws_->cleanupClients();
  }

  if (story_ != nullptr) {
    const StoryControllerV2::StoryControllerV2Snapshot snap = story_->snapshot(true, nowMs);
    const char* stepId = (snap.stepId != nullptr) ? snap.stepId : "";
    if (stepId[0] != '\0' && strcmp(stepId, lastStepId_) != 0) {
      char prevStep[32] = "";
      copyText(prevStep, sizeof(prevStep), lastStepId_);
      copyText(lastStepId_, sizeof(lastStepId_), stepId);

      const ScenarioDef* scenario = story_->scenario();
      uint8_t stepCount = (scenario != nullptr) ? scenario->stepCount : 0U;
      uint8_t stepIndex = 0U;
      if (scenario != nullptr) {
        for (uint8_t i = 0U; i < scenario->stepCount; ++i) {
          if (scenario->steps[i].id != nullptr && strcmp(scenario->steps[i].id, stepId) == 0) {
            stepIndex = i;
            break;
          }
        }
      }
      const uint8_t progress = (stepCount > 1U) ? static_cast<uint8_t>((stepIndex * 100U) / (stepCount - 1U)) : 0U;

      StaticJsonDocument<256> doc;
      doc["type"] = "step_change";
      doc["timestamp"] = nowMs;
      JsonObject data = doc.createNestedObject("data");
      data["previous_step"] = prevStep;
      data["current_step"] = stepId;
      data["progress_pct"] = progress;
      String json;
      serializeJson(doc, json);
      if (ws_ != nullptr) {
        ws_->textAll(json);
      }
      pushAuditEvent(json.c_str());

      const char* transitionId = story_->lastTransitionId();
      if (transitionId != nullptr && transitionId[0] != '\0') {
        StaticJsonDocument<192> transDoc;
        transDoc["type"] = "transition";
        transDoc["timestamp"] = nowMs;
        JsonObject transData = transDoc.createNestedObject("data");
        transData["event"] = "transition";
        transData["transition_id"] = transitionId;
        String transJson;
        serializeJson(transDoc, transJson);
        if (ws_ != nullptr) {
          ws_->textAll(transJson);
        }
        pushAuditEvent(transJson.c_str());
      }

      StaticJsonDocument<192> auditDoc;
      auditDoc["type"] = "audit_log";
      auditDoc["timestamp"] = nowMs;
      JsonObject auditData = auditDoc.createNestedObject("data");
      auditData["event_type"] = "step_execute";
      auditData["step_id"] = stepId;
      String auditJson;
      serializeJson(auditDoc, auditJson);
      if (ws_ != nullptr) {
        ws_->textAll(auditJson);
      }
      pushAuditEvent(auditJson.c_str());
    }
  }

  if (static_cast<int32_t>(nowMs - lastStatusPingMs_) >= 5000) {
    lastStatusPingMs_ = nowMs;
    broadcastStatus(nowMs);
  }
}

void WebUiService::updateCaptivePortal(uint32_t nowMs) {
  if (wifi_ == nullptr) {
    return;
  }
  if (static_cast<int32_t>(nowMs - lastCaptiveCheckMs_) < 250) {
    if (captiveActive_ && dns_ != nullptr) {
      dns_->processNextRequest();
    }
    return;
  }
  lastCaptiveCheckMs_ = nowMs;

  const bool apEnabled = wifi_->isApEnabled();
  if (apEnabled && !captiveActive_) {
    if (dns_ == nullptr) {
      dns_ = new DNSServer();
    }
    if (dns_ != nullptr) {
      dns_->start(53, "*", WiFi.softAPIP());
      captiveActive_ = true;
    }
  } else if (!apEnabled && captiveActive_) {
    if (dns_ != nullptr) {
      dns_->stop();
    }
    captiveActive_ = false;
  }

  if (captiveActive_ && dns_ != nullptr) {
    dns_->processNextRequest();
  }
}


WebUiService::Snapshot WebUiService::snapshot() const {
  return snap_;
}

bool WebUiService::checkAuth(AsyncWebServerRequest* request) {
  if (!config_.authEnabled || request == nullptr) {
    return true;
  }
  if (request->authenticate(config_.user, config_.pass)) {
    return true;
  }
  request->requestAuthentication();
  return false;
}

void WebUiService::setupRoutes() {
  if (server_ == nullptr) {
    return;
  }

  auto redirectToRoot = [this](AsyncWebServerRequest* request) {
    if (request == nullptr) {
      return;
    }
    if (!checkAuth(request)) {
      return;
    }
    setRoute("/captive");
    ++snap_.requestCount;
    request->redirect("/");
  };

  server_->on("/generate_204", HTTP_GET, redirectToRoot);
  server_->on("/gen_204", HTTP_GET, redirectToRoot);
  server_->on("/hotspot-detect.html", HTTP_GET, redirectToRoot);
  server_->on("/library/test/success.html", HTTP_GET, redirectToRoot);
  server_->on("/ncsi.txt", HTTP_GET, redirectToRoot);
  server_->on("/connecttest.txt", HTTP_GET, redirectToRoot);
  server_->on("/redirect", HTTP_GET, redirectToRoot);
  server_->on("/fwlink", HTTP_GET, redirectToRoot);

  server_->on("/", HTTP_GET, [this](AsyncWebServerRequest* request) {
    if (!checkAuth(request)) {
      return;
    }
    setRoute("/");
    ++snap_.requestCount;
    request->send(200, "text/html; charset=utf-8", kMobileHtml);
  });

  server_->on("/api/status", HTTP_GET, [this](AsyncWebServerRequest* request) {
    if (!checkAuth(request)) {
      return;
    }
    setRoute("/api/status");
    ++snap_.requestCount;
    sendJsonStatus(request);
  });

  server_->on("/api/rtos", HTTP_GET, [this](AsyncWebServerRequest* request) {
    if (!checkAuth(request)) {
      return;
    }
    setRoute("/api/rtos");
    ++snap_.requestCount;
    sendJsonRtos(request);
  });

  server_->on("/api/player", HTTP_GET, [this](AsyncWebServerRequest* request) {
    if (!checkAuth(request)) {
      return;
    }
    setRoute("/api/player");
    ++snap_.requestCount;
    sendJsonPlayer(request);
  });

  server_->on("/api/player/action", HTTP_POST, [this](AsyncWebServerRequest* request) {
    if (!checkAuth(request)) {
      return;
    }
    setRoute("/api/player/action");
    ++snap_.requestCount;
    if (mp3_ == nullptr) {
      request->send(503, "application/json", "{\"error\":\"player_unavailable\"}");
      return;
    }
    String cmd;
    if (request->hasParam("cmd")) {
      cmd = request->getParam("cmd")->value();
    }
    cmd.toLowerCase();
    if (cmd == "toggle") {
      mp3_->togglePause();
    } else if (cmd == "next") {
      mp3_->nextTrack();
    } else if (cmd == "prev") {
      mp3_->previousTrack();
    } else if (cmd == "vol_up") {
      mp3_->setGain(mp3_->gain() + 0.05f);
    } else if (cmd == "vol_down") {
      mp3_->setGain(mp3_->gain() - 0.05f);
    } else if (cmd == "rescan") {
      mp3_->requestCatalogScan(false);
    } else if (cmd == "rebuild") {
      mp3_->requestCatalogScan(true);
    } else {
      request->send(400, "application/json", "{\"error\":\"bad_cmd\"}");
      return;
    }
    request->send(200, "application/json", "{\"ok\":true}");
  });

  server_->on("/api/radio", HTTP_GET, [this](AsyncWebServerRequest* request) {
    if (!checkAuth(request)) {
      return;
    }
    setRoute("/api/radio");
    ++snap_.requestCount;
    sendJsonRadio(request);
  });

  server_->on("/api/radio/play", HTTP_POST, [this](AsyncWebServerRequest* request) {
    if (!checkAuth(request)) {
      return;
    }
    setRoute("/api/radio/play");
    ++snap_.requestCount;
    if (radio_ == nullptr) {
      request->send(503, "application/json", "{\"error\":\"radio_unavailable\"}");
      return;
    }

    if (request->hasParam("id")) {
      const uint16_t id = static_cast<uint16_t>(request->getParam("id")->value().toInt());
      const bool ok = radio_->playById(id, "web_radio_play_id");
      request->send(ok ? 200 : 404, "application/json", ok ? "{\"ok\":true}" : "{\"error\":\"station_id\"}");
      return;
    }
    if (request->hasParam("url")) {
      const String url = request->getParam("url")->value();
      const bool ok = radio_->playByUrl(url.c_str(), "web_radio_play_url");
      request->send(ok ? 200 : 400, "application/json", ok ? "{\"ok\":true}" : "{\"error\":\"url\"}");
      return;
    }
    request->send(400, "application/json", "{\"error\":\"id_or_url_required\"}");
  });

  server_->on("/api/radio/stop", HTTP_POST, [this](AsyncWebServerRequest* request) {
    if (!checkAuth(request)) {
      return;
    }
    setRoute("/api/radio/stop");
    ++snap_.requestCount;
    if (radio_ != nullptr) {
      radio_->stop("web_radio_stop");
    }
    request->send(200, "application/json", "{\"ok\":true}");
  });

  server_->on("/api/radio/next", HTTP_POST, [this](AsyncWebServerRequest* request) {
    if (!checkAuth(request)) {
      return;
    }
    setRoute("/api/radio/next");
    ++snap_.requestCount;
    if (radio_ == nullptr) {
      request->send(503, "application/json", "{\"error\":\"radio_unavailable\"}");
      return;
    }
    const bool ok = radio_->next("web_radio_next");
    request->send(ok ? 200 : 404, "application/json", ok ? "{\"ok\":true}" : "{\"error\":\"next\"}");
  });

  server_->on("/api/radio/prev", HTTP_POST, [this](AsyncWebServerRequest* request) {
    if (!checkAuth(request)) {
      return;
    }
    setRoute("/api/radio/prev");
    ++snap_.requestCount;
    if (radio_ == nullptr) {
      request->send(503, "application/json", "{\"error\":\"radio_unavailable\"}");
      return;
    }
    const bool ok = radio_->prev("web_radio_prev");
    request->send(ok ? 200 : 404, "application/json", ok ? "{\"ok\":true}" : "{\"error\":\"prev\"}");
  });

  server_->on("/api/wifi", HTTP_GET, [this](AsyncWebServerRequest* request) {
    if (!checkAuth(request)) {
      return;
    }
    setRoute("/api/wifi");
    ++snap_.requestCount;
    sendJsonWifi(request);
  });

  server_->on("/api/wifi/scan", HTTP_POST, [this](AsyncWebServerRequest* request) {
    if (!checkAuth(request)) {
      return;
    }
    setRoute("/api/wifi/scan");
    ++snap_.requestCount;
    if (wifi_ == nullptr) {
      request->send(503, "application/json", "{\"error\":\"wifi_unavailable\"}");
      return;
    }
    const bool ok = wifi_->requestScan("web_wifi_scan");
    request->send(ok ? 200 : 409, "application/json", ok ? "{\"ok\":true}" : "{\"error\":\"scan_busy\"}");
  });

  server_->on("/api/wifi/scan", HTTP_GET, [this](AsyncWebServerRequest* request) {
    if (!checkAuth(request)) {
      return;
    }
    setRoute("/api/wifi/scan");
    ++snap_.requestCount;
    if (wifi_ == nullptr) {
      request->send(503, "application/json", "{\"error\":\"wifi_unavailable\"}");
      return;
    }
    const WifiService::ScanStatus status = wifi_->scanStatus();
    if (status == WifiService::ScanStatus::Scanning) {
      request->send(200, "application/json", "{\"status\":\"scanning\",\"count\":0,\"results\":[]}");
      return;
    }
    if (status == WifiService::ScanStatus::Ready || status == WifiService::ScanStatus::Failed) {
      const String& payload = wifi_->scanJson();
      request->send(200, "application/json", payload.length() > 0U ? payload : "{\"status\":\"idle\",\"count\":0,\"results\":[]}");
      return;
    }
    request->send(200, "application/json", "{\"status\":\"idle\",\"count\":0,\"results\":[]}");
  });

  server_->on("/api/wifi/connect", HTTP_POST, [this](AsyncWebServerRequest* request) {
    if (!checkAuth(request)) {
      return;
    }
    setRoute("/api/wifi/connect");
    ++snap_.requestCount;
    if (wifi_ == nullptr) {
      request->send(503, "application/json", "{\"error\":\"wifi_unavailable\"}");
      return;
    }
    if (!request->hasParam("ssid")) {
      request->send(400, "application/json", "{\"error\":\"ssid_required\"}");
      return;
    }
    const String ssid = request->getParam("ssid")->value();
    String pass;
    if (request->hasParam("pass")) {
      pass = request->getParam("pass")->value();
    }
    const bool ok = wifi_->connectSta(ssid.c_str(), pass.c_str(), "web_wifi_connect");
    request->send(ok ? 200 : 400, "application/json", ok ? "{\"ok\":true}" : "{\"error\":\"connect\"}");
  });

  server_->on("/api/wifi/ap", HTTP_POST, [this](AsyncWebServerRequest* request) {
    if (!checkAuth(request)) {
      return;
    }
    setRoute("/api/wifi/ap");
    ++snap_.requestCount;
    if (wifi_ == nullptr) {
      request->send(503, "application/json", "{\"error\":\"wifi_unavailable\"}");
      return;
    }
    String mode = "on";
    if (request->hasParam("mode")) {
      mode = request->getParam("mode")->value();
      mode.toLowerCase();
    }
    if (mode == "off") {
      wifi_->disableAp("web_ap_off");
      request->send(200, "application/json", "{\"ok\":true}");
      return;
    }
    String ssid = "U-SON-RADIO";
    String pass = "usonradio";
    if (request->hasParam("ssid")) {
      ssid = request->getParam("ssid")->value();
    }
    if (request->hasParam("pass")) {
      pass = request->getParam("pass")->value();
    }
    const bool ok = wifi_->enableAp(ssid.c_str(), pass.c_str(), "web_ap_on");
    request->send(ok ? 200 : 400, "application/json", ok ? "{\"ok\":true}" : "{\"error\":\"ap_on\"}");
  });

  server_->on("/api/story/list", HTTP_GET, [this](AsyncWebServerRequest* request) {
    if (!checkAuth(request)) {
      return;
    }
    setRoute("/api/story/list");
    ++snap_.requestCount;
    sendStoryList(request);
  });

  server_->on("/api/story/status", HTTP_GET, [this](AsyncWebServerRequest* request) {
    if (!checkAuth(request)) {
      return;
    }
    setRoute("/api/story/status");
    ++snap_.requestCount;
    sendStoryStatus(request);
  });

  server_->on("/api/story/start", HTTP_POST, [this](AsyncWebServerRequest* request) {
    if (!checkAuth(request)) {
      return;
    }
    setRoute("/api/story/start");
    ++snap_.requestCount;
    handleStoryStart(request);
  });

  server_->on("/api/story/pause", HTTP_POST, [this](AsyncWebServerRequest* request) {
    if (!checkAuth(request)) {
      return;
    }
    setRoute("/api/story/pause");
    ++snap_.requestCount;
    handleStoryPause(request);
  });

  server_->on("/api/story/resume", HTTP_POST, [this](AsyncWebServerRequest* request) {
    if (!checkAuth(request)) {
      return;
    }
    setRoute("/api/story/resume");
    ++snap_.requestCount;
    handleStoryResume(request);
  });

  server_->on("/api/story/skip", HTTP_POST, [this](AsyncWebServerRequest* request) {
    if (!checkAuth(request)) {
      return;
    }
    setRoute("/api/story/skip");
    ++snap_.requestCount;
    handleStorySkip(request);
  });

  server_->on("/api/story/validate", HTTP_POST,
              [this](AsyncWebServerRequest* request) {
                (void)request;
              },
              nullptr,
              [this](AsyncWebServerRequest* request, uint8_t* data, size_t len, size_t index, size_t total) {
                if (!checkAuth(request)) {
                  return;
                }
                setRoute("/api/story/validate");
                if (index == 0U) {
                  request->_tempObject = new String();
                }
                String* body = reinterpret_cast<String*>(request->_tempObject);
                if (body != nullptr && data != nullptr && len > 0U) {
                  body->reserve(total + 1U);
                  body->concat(reinterpret_cast<char*>(data), len);
                }
                if (index + len >= total) {
                  const String payload = body != nullptr ? *body : String();
                  if (body != nullptr) {
                    delete body;
                    request->_tempObject = nullptr;
                  }
                  handleStoryValidate(request, payload.c_str());
                }
              });

  server_->on("/api/story/deploy", HTTP_POST,
              [this](AsyncWebServerRequest* request) {
                (void)request;
              },
              nullptr,
              [this](AsyncWebServerRequest* request, uint8_t* data, size_t len, size_t index, size_t total) {
                if (!checkAuth(request)) {
                  return;
                }
                setRoute("/api/story/deploy");
                handleStoryDeploy(request, data, len, index, total);
              });

  server_->on("/api/story/serial-command", HTTP_POST,
              [this](AsyncWebServerRequest* request) {
                (void)request;
              },
              nullptr,
              [this](AsyncWebServerRequest* request, uint8_t* data, size_t len, size_t index, size_t total) {
                if (!checkAuth(request)) {
                  return;
                }
                setRoute("/api/story/serial-command");
                if (index == 0U) {
                  request->_tempObject = new String();
                }
                String* body = reinterpret_cast<String*>(request->_tempObject);
                if (body != nullptr && data != nullptr && len > 0U) {
                  body->reserve(total + 1U);
                  body->concat(reinterpret_cast<char*>(data), len);
                }
                if (index + len >= total) {
                  const String payload = body != nullptr ? *body : String();
                  if (body != nullptr) {
                    delete body;
                    request->_tempObject = nullptr;
                  }
                  handleStorySerial(request, payload.c_str());
                }
              });

  server_->on("/api/story/fs-info", HTTP_GET, [this](AsyncWebServerRequest* request) {
    if (!checkAuth(request)) {
      return;
    }
    setRoute("/api/story/fs-info");
    ++snap_.requestCount;
    sendStoryFsInfo(request);
  });

  server_->on("/api/audit/log", HTTP_GET, [this](AsyncWebServerRequest* request) {
    if (!checkAuth(request)) {
      return;
    }
    setRoute("/api/audit/log");
    ++snap_.requestCount;
    sendAuditLog(request);
  });

  server_->onNotFound([this](AsyncWebServerRequest* request) {
    if (request == nullptr) {
      return;
    }
    if (!checkAuth(request)) {
      return;
    }
    if (request->method() == HTTP_OPTIONS) {
      handleOptions(request);
      return;
    }
    if (request->method() == HTTP_POST) {
      const String url = request->url();
      if (url.startsWith("/api/story/select/")) {
        setRoute("/api/story/select");
        ++snap_.requestCount;
        handleStorySelect(request);
        return;
      }
    }
    if (wifi_ != nullptr && wifi_->isApEnabled()) {
      setRoute("/captive");
      ++snap_.requestCount;
      request->redirect("/");
      return;
    }
    setRoute("/404");
    ++snap_.requestCount;
    sendError(request, 404, "Not found", "Route not found");
  });
}

void WebUiService::sendJsonStatus(AsyncWebServerRequest* request) {
  if (request == nullptr) {
    return;
  }

  String json = "{";
  bool hasSection = false;

  if (wifi_ != nullptr) {
    const WifiService::Snapshot w = wifi_->snapshot();
    json += "\"wifi\":{";
    json += "\"connected\":" + String(w.staConnected ? "true" : "false") + ",";
    json += "\"ap\":" + String(w.apEnabled ? "true" : "false") + ",";
    json += "\"scanning\":" + String(w.scanning ? "true" : "false") + ",";
    json += "\"mode\":\"" + String(w.mode) + "\",";
    json += "\"ssid\":\"" + String(w.ssid) + "\",";
    json += "\"ip\":\"" + String(w.ip) + "\",";
    json += "\"rssi\":" + String(w.rssi) + ",";
    json += "\"disconnect_reason\":" + String(w.disconnectReason) + ",";
    json += "\"disconnect_label\":\"" + String(w.disconnectLabel) + "\",";
    json += "\"disconnect_count\":" + String(w.disconnectCount) + ",";
    json += "\"last_disconnect_ms\":" + String(w.lastDisconnectMs) + "}";
    hasSection = true;
  }

  if (radio_ != nullptr) {
    const RadioService::Snapshot r = radio_->snapshot();
    if (hasSection) {
      json += ",";
    }
    json += "\"radio\":{";
    json += "\"active\":" + String(r.active ? "true" : "false") + ",";
    json += "\"id\":" + String(r.activeStationId) + ",";
    json += "\"station\":\"" + String(r.activeStationName) + "\",";
    json += "\"state\":\"" + String(r.streamState) + "\",";
    json += "\"title\":\"" + String(r.title) + "\",";
    json += "\"codec\":\"" + String(r.codec) + "\",";
    json += "\"bitrate\":" + String(r.bitrateKbps) + ",";
    json += "\"buffer\":" + String(r.bufferPercent) + "}";
    hasSection = true;
  }

  if (mp3_ != nullptr) {
    if (hasSection) {
      json += ",";
    }
    json += "\"player\":{";
    json += "\"playing\":" + String(mp3_->isPlaying() ? "true" : "false") + ",";
    json += "\"paused\":" + String(mp3_->isPaused() ? "true" : "false") + ",";
    json += "\"track\":" + String(mp3_->currentTrackNumber()) + ",";
    json += "\"tracks\":" + String(mp3_->trackCount()) + ",";
    json += "\"volume\":" + String(mp3_->volumePercent()) + ",";
    json += "\"backend\":\"" + String(mp3_->activeBackendLabel()) + "\",";
    json += "\"scan\":\"" + String(mp3_->scanStateLabel()) + "\"}";
    hasSection = true;
  }

  const RtosSnapshot rtos = buildRtosSnapshot();
  if (hasSection) {
    json += ",";
  }
  json += "\"rtos\":{";
  json += "\"tasks\":" + String(rtos.taskCount) + ",";
  json += "\"heap_free\":" + String(rtos.heapFree) + ",";
  json += "\"heap_min\":" + String(rtos.heapMin) + ",";
  json += "\"heap_size\":" + String(rtos.heapSize) + ",";
  json += "\"stack_min_words\":" + String(rtos.stackMinWords) + ",";
  json += "\"stack_min_bytes\":" + String(rtos.stackMinBytes) + "}";

  json += "}";
  request->send(200, "application/json", json);
}

void WebUiService::sendJsonPlayer(AsyncWebServerRequest* request) {
  if (request == nullptr) {
    return;
  }
  if (mp3_ == nullptr) {
    request->send(503, "application/json", "{\"error\":\"player_unavailable\"}");
    return;
  }
  String json = "{";
  json += "\"playing\":" + String(mp3_->isPlaying() ? "true" : "false") + ",";
  json += "\"paused\":" + String(mp3_->isPaused() ? "true" : "false") + ",";
  json += "\"track\":" + String(mp3_->currentTrackNumber()) + ",";
  json += "\"tracks\":" + String(mp3_->trackCount()) + ",";
  json += "\"name\":\"" + mp3_->currentTrackName() + "\",";
  json += "\"volume\":" + String(mp3_->volumePercent()) + ",";
  json += "\"repeat\":\"" + String(mp3_->repeatModeLabel()) + "\",";
  json += "\"mode\":\"" + String(mp3_->backendModeLabel()) + "\",";
  json += "\"active_backend\":\"" + String(mp3_->activeBackendLabel()) + "\"}";
  request->send(200, "application/json", json);
}

void WebUiService::sendJsonRadio(AsyncWebServerRequest* request) {
  if (request == nullptr) {
    return;
  }
  if (radio_ == nullptr) {
    request->send(503, "application/json", "{\"error\":\"radio_unavailable\"}");
    return;
  }
  const RadioService::Snapshot r = radio_->snapshot();
  String json = "{";
  json += "\"active\":" + String(r.active ? "true" : "false") + ",";
  json += "\"station_id\":" + String(r.activeStationId) + ",";
  json += "\"station\":\"" + String(r.activeStationName) + "\",";
  json += "\"state\":\"" + String(r.streamState) + "\",";
  json += "\"title\":\"" + String(r.title) + "\",";
  json += "\"codec\":\"" + String(r.codec) + "\",";
  json += "\"bitrate\":" + String(r.bitrateKbps) + ",";
  json += "\"buffer\":" + String(r.bufferPercent) + ",";
  json += "\"stations\":[";
  const uint16_t total = radio_->stationCount();
  for (uint16_t i = 0; i < total; ++i) {
    const StationRepository::Station* station = radio_->stationAt(i);
    if (station == nullptr) {
      continue;
    }
    if (!json.endsWith("[")) {
      json += ",";
    }
    json += "{";
    json += "\"id\":" + String(station->id) + ",";
    json += "\"name\":\"" + String(station->name) + "\",";
    json += "\"codec\":\"" + String(station->codec) + "\",";
    json += "\"enabled\":" + String(station->enabled ? "true" : "false");
    json += "}";
  }
  json += "]}";
  request->send(200, "application/json", json);
}

void WebUiService::sendJsonWifi(AsyncWebServerRequest* request) {
  if (request == nullptr) {
    return;
  }
  if (wifi_ == nullptr) {
    request->send(503, "application/json", "{\"error\":\"wifi_unavailable\"}");
    return;
  }
  const WifiService::Snapshot w = wifi_->snapshot();
  String json = "{";
  json += "\"connected\":" + String(w.staConnected ? "true" : "false") + ",";
  json += "\"ap\":" + String(w.apEnabled ? "true" : "false") + ",";
  json += "\"scanning\":" + String(w.scanning ? "true" : "false") + ",";
  json += "\"mode\":\"" + String(w.mode) + "\",";
  json += "\"ssid\":\"" + String(w.ssid) + "\",";
  json += "\"ip\":\"" + String(w.ip) + "\",";
  json += "\"rssi\":" + String(w.rssi) + ",";
  json += "\"scan_count\":" + String(w.scanCount) + ",";
  json += "\"disconnect_reason\":" + String(w.disconnectReason) + ",";
  json += "\"disconnect_label\":\"" + String(w.disconnectLabel) + "\",";
  json += "\"disconnect_count\":" + String(w.disconnectCount) + ",";
  json += "\"last_disconnect_ms\":" + String(w.lastDisconnectMs) + ",";
  json += "\"err\":\"" + String(w.lastError) + "\"}";
  request->send(200, "application/json", json);
}

void WebUiService::sendJsonRtos(AsyncWebServerRequest* request) {
  if (request == nullptr) {
    return;
  }
  const RtosSnapshot snap = buildRtosSnapshot();
  StaticJsonDocument<768> doc;
  doc["tasks"] = snap.taskCount;
  doc["heap_free"] = snap.heapFree;
  doc["heap_min"] = snap.heapMin;
  doc["heap_size"] = snap.heapSize;
  doc["stack_min_words"] = snap.stackMinWords;
  doc["stack_min_bytes"] = snap.stackMinBytes;
  if (runtime_ != nullptr) {
    doc["runtime_enabled"] = runtime_->enabled();
    RadioRuntime::TaskSnapshot tasks[6] = {};
    const size_t count = runtime_->taskSnapshots(tasks, sizeof(tasks) / sizeof(tasks[0]));
    JsonArray list = doc.createNestedArray("task_list");
    for (size_t i = 0U; i < count; ++i) {
      const RadioRuntime::TaskSnapshot& task = tasks[i];
      if (task.name == nullptr) {
        continue;
      }
      JsonObject entry = list.createNestedObject();
      entry["name"] = task.name;
      entry["core"] = task.core;
      entry["stack_min_words"] = task.stackMinWords;
      entry["stack_min_bytes"] = task.stackMinBytes;
      entry["ticks"] = task.ticks;
      entry["last_tick_ms"] = task.lastTickMs;
    }
  }
  String json;
  serializeJson(doc, json);
  sendJson(request, 200, json);
}

void WebUiService::setRoute(const char* route) {
  copyText(snap_.lastRoute, sizeof(snap_.lastRoute), route);
}

void WebUiService::setError(const char* error) {
  copyText(snap_.lastError, sizeof(snap_.lastError), error);
}

void WebUiService::sendJson(AsyncWebServerRequest* request, int code, const String& json) {
  if (request == nullptr) {
    return;
  }
  AsyncWebServerResponse* response = request->beginResponse(code, "application/json", json);
  addCorsHeaders(response);
  request->send(response);
}

void WebUiService::sendError(AsyncWebServerRequest* request,
                             int code,
                             const char* message,
                             const char* details) {
  StaticJsonDocument<256> doc;
  JsonObject err = doc.createNestedObject("error");
  err["code"] = code;
  err["message"] = message != nullptr ? message : "error";
  if (details != nullptr && details[0] != '\0') {
    err["details"] = details;
  }
  String json;
  serializeJson(doc, json);
  sendJson(request, code, json);
}

void WebUiService::addCorsHeaders(AsyncWebServerResponse* response) {
  if (response == nullptr) {
    return;
  }
  response->addHeader("Access-Control-Allow-Origin", "*");
  response->addHeader("Access-Control-Allow-Methods", "GET, POST, PUT, DELETE, OPTIONS");
  response->addHeader("Access-Control-Allow-Headers", "Content-Type, Authorization");
  response->addHeader("Access-Control-Max-Age", "3600");
}

void WebUiService::handleOptions(AsyncWebServerRequest* request) {
  if (request == nullptr) {
    return;
  }
  AsyncWebServerResponse* response = request->beginResponse(200);
  addCorsHeaders(response);
  request->send(response);
}

void WebUiService::pushAuditEvent(const char* json) {
  if (json == nullptr || json[0] == '\0') {
    return;
  }
  const size_t idx = (auditHead_ + auditCount_) % kAuditBufferSize;
  if (auditCount_ < kAuditBufferSize) {
    auditBuffer_[idx] = json;
    ++auditCount_;
    return;
  }
  auditBuffer_[auditHead_] = json;
  auditHead_ = (auditHead_ + 1U) % kAuditBufferSize;
}

void WebUiService::broadcastStatus(uint32_t nowMs) {
  if (ws_ == nullptr) {
    return;
  }
  const uint32_t freeHeap = ESP.getFreeHeap();
  const uint32_t heapSize = ESP.getHeapSize();
  const uint8_t heapPct = heapSize > 0U ? static_cast<uint8_t>((freeHeap * 100U) / heapSize) : 0U;

  String statusLabel = "idle";
  if (story_ != nullptr) {
    if (story_->isPaused()) {
      statusLabel = "paused";
    } else if (story_->isRunning()) {
      statusLabel = "running";
    }
  }

  StaticJsonDocument<192> doc;
  doc["type"] = "status";
  doc["timestamp"] = nowMs;
  JsonObject data = doc.createNestedObject("data");
  data["status"] = statusLabel;
  data["memory_free"] = freeHeap;
  data["heap_pct"] = heapPct;
  String json;
  serializeJson(doc, json);
  ws_->textAll(json);
  pushAuditEvent(json.c_str());
}

void WebUiService::sendStoryList(AsyncWebServerRequest* request) {
  if (request == nullptr) {
    return;
  }
  StaticJsonDocument<768> doc;
  JsonArray scenarios = doc.createNestedArray("scenarios");

  bool listed = false;
  if (storyFs_ != nullptr) {
    StoryScenarioInfo infos[16];
    size_t count = 0U;
    if (storyFs_->listScenarios(infos, 16U, &count) && count > 0U) {
      for (size_t i = 0U; i < count; ++i) {
        JsonObject item = scenarios.createNestedObject();
        item["id"] = infos[i].id;
        item["version"] = infos[i].version;
        item["estimated_duration_s"] = infos[i].estimatedDurationS;
      }
      listed = true;
    }
  }

  if (!listed) {
    const uint8_t count = generatedScenarioCount();
    for (uint8_t i = 0U; i < count; ++i) {
      const char* id = generatedScenarioIdAt(i);
      const ScenarioDef* scenario = generatedScenarioById(id);
      JsonObject item = scenarios.createNestedObject();
      item["id"] = id != nullptr ? id : "";
      item["version"] = scenario != nullptr ? scenario->version : 0U;
      item["estimated_duration_s"] = 0U;
    }
  }

  String json;
  serializeJson(doc, json);
  sendJson(request, 200, json);
}

void WebUiService::sendStoryStatus(AsyncWebServerRequest* request) {
  if (request == nullptr) {
    return;
  }
  if (story_ == nullptr) {
    sendError(request, 500, "Story controller unavailable", "story_controller_missing");
    return;
  }

  const StoryControllerV2::StoryControllerV2Snapshot snap = story_->snapshot(true, millis());
  String statusLabel = "idle";
  if (snap.paused) {
    statusLabel = "paused";
  } else if (snap.running) {
    statusLabel = "running";
  }

  const ScenarioDef* scenario = story_->scenario();
  uint8_t stepIndex = 0U;
  uint8_t stepCount = scenario != nullptr ? scenario->stepCount : 0U;
  if (scenario != nullptr && snap.stepId != nullptr) {
    for (uint8_t i = 0U; i < scenario->stepCount; ++i) {
      if (scenario->steps[i].id != nullptr && strcmp(scenario->steps[i].id, snap.stepId) == 0) {
        stepIndex = i;
        break;
      }
    }
  }
  const uint8_t progress = (stepCount > 1U) ? static_cast<uint8_t>((stepIndex * 100U) / (stepCount - 1U)) : 0U;

  StaticJsonDocument<384> doc;
  doc["status"] = statusLabel;
  doc["scenario_id"] = snap.scenarioId != nullptr ? snap.scenarioId : "";
  doc["current_step"] = snap.stepId != nullptr ? snap.stepId : "";
  doc["progress_pct"] = progress;
  doc["started_at_ms"] = storyStartedAtMs_;
  doc["selected"] = storySelected_ ? selectedScenarioId_ : "";
  doc["queue_depth"] = snap.queueDepth;
  String json;
  serializeJson(doc, json);
  sendJson(request, 200, json);
}

void WebUiService::handleStorySelect(AsyncWebServerRequest* request) {
  if (request == nullptr) {
    return;
  }
  const String url = request->url();
  const int slash = url.lastIndexOf('/');
  if (slash < 0 || slash >= static_cast<int>(url.length() - 1)) {
    sendError(request, 400, "Invalid scenario ID", "missing scenario id");
    return;
  }
  const String id = url.substring(slash + 1);
  if (id.length() == 0U) {
    sendError(request, 400, "Invalid scenario ID", "empty scenario id");
    return;
  }
  const ScenarioDef* scenario = generatedScenarioById(id.c_str());
  if (scenario == nullptr) {
    sendError(request, 404, "Scenario not found", id.c_str());
    return;
  }
  copyText(selectedScenarioId_, sizeof(selectedScenarioId_), id.c_str());
  storySelected_ = true;

  StaticJsonDocument<128> doc;
  doc["selected"] = selectedScenarioId_;
  doc["status"] = "ready";
  String json;
  serializeJson(doc, json);
  sendJson(request, 200, json);
}

void WebUiService::handleStoryStart(AsyncWebServerRequest* request) {
  if (request == nullptr) {
    return;
  }
  if (story_ == nullptr) {
    sendError(request, 500, "Story controller unavailable", "story_controller_missing");
    return;
  }
  if (!storySelected_ || selectedScenarioId_[0] == '\0') {
    sendError(request, 412, "Scenario not selected", "call /api/story/select/{scenario_id}");
    return;
  }
  if (story_->isRunning()) {
    sendError(request, 409, "Story already running", "already running");
    return;
  }
  if (story_->isPaused()) {
    sendError(request, 409, "Story paused", "resume required");
    return;
  }
  const uint32_t nowMs = millis();
  if (!story_->setScenario(selectedScenarioId_, nowMs, "web_story_start")) {
    sendError(request, 500, "Failed to start scenario", selectedScenarioId_);
    return;
  }
  storyStartedAtMs_ = nowMs;
  const StoryControllerV2::StoryControllerV2Snapshot snap = story_->snapshot(true, nowMs);

  StaticJsonDocument<192> doc;
  doc["status"] = "running";
  doc["current_step"] = snap.stepId != nullptr ? snap.stepId : "";
  doc["started_at_ms"] = storyStartedAtMs_;
  String json;
  serializeJson(doc, json);
  sendJson(request, 200, json);
}

void WebUiService::handleStoryPause(AsyncWebServerRequest* request) {
  if (request == nullptr) {
    return;
  }
  if (story_ == nullptr) {
    sendError(request, 500, "Story controller unavailable", "story_controller_missing");
    return;
  }
  if (!story_->pause(millis(), "web_story_pause")) {
    sendError(request, 409, "Story not running", "cannot pause");
    return;
  }
  const StoryControllerV2::StoryControllerV2Snapshot snap = story_->snapshot(true, millis());
  StaticJsonDocument<160> doc;
  doc["status"] = "paused";
  doc["paused_at_step"] = snap.stepId != nullptr ? snap.stepId : "";
  String json;
  serializeJson(doc, json);
  sendJson(request, 200, json);
}

void WebUiService::handleStoryResume(AsyncWebServerRequest* request) {
  if (request == nullptr) {
    return;
  }
  if (story_ == nullptr) {
    sendError(request, 500, "Story controller unavailable", "story_controller_missing");
    return;
  }
  if (!story_->resume(millis(), "web_story_resume")) {
    sendError(request, 409, "Story not paused", "cannot resume");
    return;
  }
  StaticJsonDocument<96> doc;
  doc["status"] = "running";
  String json;
  serializeJson(doc, json);
  sendJson(request, 200, json);
}

void WebUiService::handleStorySkip(AsyncWebServerRequest* request) {
  if (request == nullptr) {
    return;
  }
  if (story_ == nullptr) {
    sendError(request, 500, "Story controller unavailable", "story_controller_missing");
    return;
  }
  const char* prevStep = nullptr;
  const char* nextStep = nullptr;
  if (!story_->skipToNextStep(millis(), "web_story_skip", &prevStep, &nextStep)) {
    sendError(request, 409, "Skip not available", "no transition");
    return;
  }
  StaticJsonDocument<160> doc;
  doc["previous_step"] = prevStep != nullptr ? prevStep : "";
  doc["current_step"] = nextStep != nullptr ? nextStep : "";
  String json;
  serializeJson(doc, json);
  sendJson(request, 200, json);
}

void WebUiService::handleStoryValidate(AsyncWebServerRequest* request, const char* body) {
  if (request == nullptr) {
    return;
  }
  if (body == nullptr || body[0] == '\0') {
    sendError(request, 400, "Missing payload", "body empty");
    return;
  }
  StaticJsonDocument<512> doc;
  const DeserializationError err = deserializeJson(doc, body);
  if (err) {
    sendError(request, 400, "Invalid JSON", err.c_str());
    return;
  }
  const char* yaml = doc["yaml"] | "";
  if (yaml[0] == '\0') {
    sendError(request, 400, "Missing yaml", "yaml field required");
    return;
  }

  StaticJsonDocument<192> out;
  out["valid"] = true;
  String json;
  serializeJson(out, json);
  sendJson(request, 200, json);
}

void WebUiService::handleStoryDeploy(AsyncWebServerRequest* request,
                                     uint8_t* data,
                                     size_t len,
                                     size_t index,
                                     size_t total) {
  if (request == nullptr) {
    return;
  }

  if (index == 0U) {
    if (!LittleFS.exists("/story")) {
      LittleFS.mkdir("/story");
    }
    const uint32_t freeBytes = LittleFS.totalBytes() - LittleFS.usedBytes();
    const uint32_t contentLen = request->contentLength();
    if (contentLen > 0U && contentLen > freeBytes) {
      sendError(request, 507, "Insufficient storage", "not enough space");
      return;
    }
    const String path = String("/story/upload_") + String(millis()) + ".tar.gz";
    fs::File* file = new fs::File(LittleFS.open(path, "w"));
    if (file == nullptr || !(*file)) {
      if (file != nullptr) {
        delete file;
      }
      sendError(request, 500, "Deploy failed", "open failed");
      return;
    }
    request->_tempObject = file;
  }

  fs::File* file = reinterpret_cast<fs::File*>(request->_tempObject);
  if (file != nullptr && data != nullptr && len > 0U) {
    file->write(data, len);
  }

  if (index + len >= total) {
    if (file != nullptr) {
      file->close();
      delete file;
      request->_tempObject = nullptr;
    }
    StaticJsonDocument<160> doc;
    doc["deployed"] = "UPLOAD";
    doc["status"] = "ok";
    String json;
    serializeJson(doc, json);
    sendJson(request, 200, json);
  }
}

void WebUiService::handleStorySerial(AsyncWebServerRequest* request, const char* body) {
  if (request == nullptr) {
    return;
  }
  if (body == nullptr || body[0] == '\0') {
    sendError(request, 400, "Missing payload", "body empty");
    return;
  }
  StaticJsonDocument<256> doc;
  const DeserializationError err = deserializeJson(doc, body);
  if (err) {
    sendError(request, 400, "Invalid JSON", err.c_str());
    return;
  }
  const char* command = doc["command"] | "";
  if (command[0] == '\0') {
    sendError(request, 400, "Missing command", "command field required");
    return;
  }
  if (story_ == nullptr) {
    sendError(request, 500, "Story controller unavailable", "story_controller_missing");
    return;
  }

  char lineBuf[192] = {};
  snprintf(lineBuf, sizeof(lineBuf), "%s", command);

  size_t start = 0U;
  size_t end = strlen(lineBuf);
  while (lineBuf[start] != '\0' && isspace(static_cast<unsigned char>(lineBuf[start])) != 0) {
    ++start;
  }
  while (end > start && isspace(static_cast<unsigned char>(lineBuf[end - 1U])) != 0) {
    --end;
  }
  size_t dst = 0U;
  for (size_t i = start; i < end; ++i) {
    lineBuf[dst++] = lineBuf[i];
  }
  lineBuf[dst] = '\0';

  char token[64] = {};
  const char* args = "";
  size_t src = 0U;
  size_t tdst = 0U;
  while (lineBuf[src] != '\0' && isspace(static_cast<unsigned char>(lineBuf[src])) == 0) {
    if (tdst < (sizeof(token) - 1U)) {
      token[tdst++] = static_cast<char>(toupper(static_cast<unsigned char>(lineBuf[src])));
    }
    ++src;
  }
  token[tdst] = '\0';
  while (lineBuf[src] != '\0' && isspace(static_cast<unsigned char>(lineBuf[src])) != 0) {
    ++src;
  }
  args = &lineBuf[src];

  SerialCommand cmd;
  cmd.line = lineBuf;
  cmd.token = token;
  cmd.args = args;

  bool storyV2Enabled = true;
  StorySerialRuntimeContext ctx = {};
  ctx.storyV2Enabled = &storyV2Enabled;
  ctx.storyV2Default = true;
  ctx.v2 = story_;

  StringPrint out;
  const uint32_t startMs = millis();
  const bool ok = serialProcessStoryCommand(cmd, startMs, ctx, out);
  const uint32_t latencyMs = millis() - startMs;
  if (!ok) {
    sendError(request, 400, "Command rejected", "unsupported or invalid");
    return;
  }

  StaticJsonDocument<256> response;
  response["command"] = command;
  response["response"] = out.str();
  response["latency_ms"] = latencyMs;
  String json;
  serializeJson(response, json);
  sendJson(request, 200, json);
}

void WebUiService::sendStoryFsInfo(AsyncWebServerRequest* request) {
  if (request == nullptr) {
    return;
  }
  uint32_t totalBytes = 0U;
  uint32_t usedBytes = 0U;
  uint16_t scenarios = 0U;
  if (storyFs_ != nullptr) {
    storyFs_->fsInfo(&totalBytes, &usedBytes, &scenarios);
  } else {
    totalBytes = LittleFS.totalBytes();
    usedBytes = LittleFS.usedBytes();
    scenarios = generatedScenarioCount();
  }
  const uint32_t freeBytes = totalBytes > usedBytes ? (totalBytes - usedBytes) : 0U;

  StaticJsonDocument<192> doc;
  doc["total_bytes"] = totalBytes;
  doc["used_bytes"] = usedBytes;
  doc["free_bytes"] = freeBytes;
  doc["scenarios"] = scenarios;
  String json;
  serializeJson(doc, json);
  sendJson(request, 200, json);
}

void WebUiService::sendAuditLog(AsyncWebServerRequest* request) {
  if (request == nullptr) {
    return;
  }
  size_t limit = 50U;
  if (request->hasParam("limit")) {
    const String value = request->getParam("limit")->value();
    limit = static_cast<size_t>(value.toInt());
  }
  if (limit == 0U) {
    limit = 50U;
  }
  if (limit > 500U) {
    limit = 500U;
  }

  const size_t available = auditCount_;
  const size_t count = (limit < available) ? limit : available;
  const size_t startIndex = (available > count) ? (available - count) : 0U;

  String json = "{\"events\":[";
  for (size_t i = 0U; i < count; ++i) {
    const size_t idx = (auditHead_ + startIndex + i) % kAuditBufferSize;
    if (i > 0U) {
      json += ",";
    }
    json += auditBuffer_[idx];
  }
  json += "]}";
  sendJson(request, 200, json);
}
