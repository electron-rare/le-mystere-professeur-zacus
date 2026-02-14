#include "web_ui_service.h"

#include <ESPAsyncWebServer.h>

#include "../network/wifi_service.h"
#include "../radio/radio_service.h"
#include "../../audio/mp3_player.h"

namespace {

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
    "input{width:100%;padding:9px;border-radius:10px;border:1px solid var(--line);background:#0b121a;color:var(--text)}"
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
    "<input id='ssid' placeholder='SSID'><input id='pass' placeholder='Mot de passe'>"
    "<div class='grid'><button onclick='wifiConnect()'>Connecter</button><button onclick=\"post('/api/wifi/ap?mode=on')\">AP ON</button></div>"
    "</div></div>"
    "<div class='card'><h2>Statut</h2><pre id='json' style='white-space:pre-wrap;font-size:.72rem;margin:0;max-height:220px;overflow:auto'></pre></div>"
    "</div>"
    "<script>"
    "async function post(u){try{await fetch(u,{method:'POST'});await refresh()}catch(e){}}"
    "async function act(cmd){await post('/api/player/action?cmd='+encodeURIComponent(cmd))}"
    "async function wifiConnect(){const s=document.getElementById('ssid').value.trim();const p=document.getElementById('pass').value;"
    "if(!s)return;await post('/api/wifi/connect?ssid='+encodeURIComponent(s)+'&pass='+encodeURIComponent(p))}"
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

  setupRoutes();
  server_->begin();
  snap_.started = true;
  setRoute("BEGIN");
}

void WebUiService::update(uint32_t nowMs) {
  (void)nowMs;
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

  server_->onNotFound([this](AsyncWebServerRequest* request) {
    setRoute("404");
    ++snap_.requestCount;
    request->send(404, "application/json", "{\"error\":\"not_found\"}");
  });
}

void WebUiService::sendJsonStatus(AsyncWebServerRequest* request) {
  if (request == nullptr) {
    return;
  }

  String json = "{";
  if (wifi_ != nullptr) {
    const WifiService::Snapshot w = wifi_->snapshot();
    json += "\"wifi\":{";
    json += "\"connected\":" + String(w.staConnected ? "true" : "false") + ",";
    json += "\"ap\":" + String(w.apEnabled ? "true" : "false") + ",";
    json += "\"scanning\":" + String(w.scanning ? "true" : "false") + ",";
    json += "\"mode\":\"" + String(w.mode) + "\",";
    json += "\"ssid\":\"" + String(w.ssid) + "\",";
    json += "\"ip\":\"" + String(w.ip) + "\",";
    json += "\"rssi\":" + String(w.rssi) + "},";
  }
  if (radio_ != nullptr) {
    const RadioService::Snapshot r = radio_->snapshot();
    json += "\"radio\":{";
    json += "\"active\":" + String(r.active ? "true" : "false") + ",";
    json += "\"id\":" + String(r.activeStationId) + ",";
    json += "\"station\":\"" + String(r.activeStationName) + "\",";
    json += "\"state\":\"" + String(r.streamState) + "\",";
    json += "\"title\":\"" + String(r.title) + "\",";
    json += "\"codec\":\"" + String(r.codec) + "\",";
    json += "\"bitrate\":" + String(r.bitrateKbps) + ",";
    json += "\"buffer\":" + String(r.bufferPercent) + "},";
  }
  if (mp3_ != nullptr) {
    json += "\"player\":{";
    json += "\"playing\":" + String(mp3_->isPlaying() ? "true" : "false") + ",";
    json += "\"paused\":" + String(mp3_->isPaused() ? "true" : "false") + ",";
    json += "\"track\":" + String(mp3_->currentTrackNumber()) + ",";
    json += "\"tracks\":" + String(mp3_->trackCount()) + ",";
    json += "\"volume\":" + String(mp3_->volumePercent()) + ",";
    json += "\"backend\":\"" + String(mp3_->activeBackendLabel()) + "\",";
    json += "\"scan\":\"" + String(mp3_->scanStateLabel()) + "\"}";
  } else {
    if (json.endsWith(",")) {
      json.remove(json.length() - 1);
    }
  }
  if (json.endsWith(",")) {
    json.remove(json.length() - 1);
  }
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
  json += "\"err\":\"" + String(w.lastError) + "\"}";
  request->send(200, "application/json", json);
}

void WebUiService::setRoute(const char* route) {
  copyText(snap_.lastRoute, sizeof(snap_.lastRoute), route);
}

void WebUiService::setError(const char* error) {
  copyText(snap_.lastError, sizeof(snap_.lastError), error);
}
