#include "serial_commands_radio.h"

#include <cctype>
#include <cstdio>
#include <cstring>

#include "serial_dispatch.h"
#include "../network/wifi_service.h"
#include "../radio/radio_service.h"
#include "../web/web_ui_service.h"

namespace {

const char* skipSpaces(const char* text) {
  if (text == nullptr) {
    return "";
  }
  while (*text != '\0' && isspace(static_cast<unsigned char>(*text)) != 0) {
    ++text;
  }
  return text;
}

bool equalsIgnoreCase(const char* lhs, const char* rhs) {
  if (lhs == nullptr || rhs == nullptr) {
    return false;
  }
  while (*lhs != '\0' && *rhs != '\0') {
    if (toupper(static_cast<unsigned char>(*lhs)) != toupper(static_cast<unsigned char>(*rhs))) {
      return false;
    }
    ++lhs;
    ++rhs;
  }
  return *lhs == '\0' && *rhs == '\0';
}

void printWifiStatus(Print& out, const WifiService::Snapshot& s, const char* source) {
  out.printf("[WIFI_STATUS] %s connected=%u ap=%u scanning=%u mode=%s ssid=%s ip=%s rssi=%ld scan=%u disc=%u disc_label=%s disc_count=%lu err=%s evt=%s\n",
             source,
             s.staConnected ? 1U : 0U,
             s.apEnabled ? 1U : 0U,
             s.scanning ? 1U : 0U,
             s.mode,
             s.ssid,
             s.ip,
             static_cast<long>(s.rssi),
             static_cast<unsigned int>(s.scanCount),
             static_cast<unsigned int>(s.disconnectReason),
             s.disconnectLabel,
             static_cast<unsigned long>(s.disconnectCount),
             s.lastError,
             s.lastEvent);
}

void printRadioStatus(Print& out, const RadioService::Snapshot& s, const char* source) {
  out.printf("[RADIO_STATUS] %s active=%u id=%u station=%s state=%s codec=%s bitrate=%u buffer=%u err=%s evt=%s\n",
             source,
             s.active ? 1U : 0U,
             static_cast<unsigned int>(s.activeStationId),
             s.activeStationName,
             s.streamState,
             s.codec,
             static_cast<unsigned int>(s.bitrateKbps),
             static_cast<unsigned int>(s.bufferPercent),
             s.lastError,
             s.lastEvent);
}

void printWebStatus(Print& out, const WebUiService::Snapshot& s, const char* source) {
  out.printf("[WEB_STATUS] %s started=%u port=%u req=%lu route=%s err=%s\n",
             source,
             s.started ? 1U : 0U,
             static_cast<unsigned int>(s.port),
             static_cast<unsigned long>(s.requestCount),
             s.lastRoute,
             s.lastError);
}

}  // namespace

bool serialIsRadioCommand(const char* token) {
  if (token == nullptr) {
    return false;
  }
  return strncmp(token, "RADIO_", 6U) == 0 || strncmp(token, "WIFI_", 5U) == 0 ||
         strncmp(token, "WEB_", 4U) == 0;
}

bool serialProcessRadioCommand(const SerialCommand& cmd,
                               uint32_t nowMs,
                               const RadioSerialRuntimeContext& ctx,
                               Print& out) {
  (void)nowMs;
  if (cmd.token == nullptr || cmd.token[0] == '\0') {
    return false;
  }
  const char* args = skipSpaces(cmd.args);

  if (serialTokenEquals(cmd, "RADIO_HELP")) {
    if (ctx.printHelp != nullptr) {
      ctx.printHelp();
    }
    serialDispatchReply(out, "RADIO", SerialDispatchResult::kOk, "help");
    return true;
  }

  if (serialTokenEquals(cmd, "RADIO_STATUS")) {
    if (ctx.radio == nullptr) {
      serialDispatchReply(out, "RADIO", SerialDispatchResult::kOutOfContext, "missing_radio");
      return true;
    }
    printRadioStatus(out, ctx.radio->snapshot(), "status");
    serialDispatchReply(out, "RADIO", SerialDispatchResult::kOk, "status");
    return true;
  }

  if (serialTokenEquals(cmd, "RADIO_LIST")) {
    if (ctx.radio == nullptr) {
      serialDispatchReply(out, "RADIO", SerialDispatchResult::kOutOfContext, "missing_radio");
      return true;
    }
    int offset = 0;
    int limit = 8;
    if (args[0] != '\0' && sscanf(args, "%d %d", &offset, &limit) < 1) {
      serialDispatchReply(out, "RADIO", SerialDispatchResult::kBadArgs, "[offset limit]");
      return true;
    }
    if (offset < 0) {
      offset = 0;
    }
    if (limit <= 0 || limit > 40) {
      limit = 8;
    }

    const uint16_t total = ctx.radio->stationCount();
    out.printf("[RADIO_LIST] total=%u offset=%u limit=%u\n",
               static_cast<unsigned int>(total),
               static_cast<unsigned int>(offset),
               static_cast<unsigned int>(limit));
    for (uint16_t i = static_cast<uint16_t>(offset);
         i < total && i < static_cast<uint16_t>(offset + limit);
         ++i) {
      const StationRepository::Station* station = ctx.radio->stationAt(i);
      if (station == nullptr) {
        continue;
      }
      out.printf("  [%u] id=%u %s codec=%s enabled=%u fav=%u\n",
                 static_cast<unsigned int>(i),
                 static_cast<unsigned int>(station->id),
                 station->name,
                 station->codec,
                 station->enabled ? 1U : 0U,
                 station->favorite ? 1U : 0U);
    }
    serialDispatchReply(out, "RADIO", SerialDispatchResult::kOk, "list");
    return true;
  }

  if (serialTokenEquals(cmd, "RADIO_PLAY")) {
    if (ctx.radio == nullptr) {
      serialDispatchReply(out, "RADIO", SerialDispatchResult::kOutOfContext, "missing_radio");
      return true;
    }
    if (args[0] == '\0') {
      serialDispatchReply(out, "RADIO", SerialDispatchResult::kBadArgs, "RADIO_PLAY <id|url>");
      return true;
    }
    uint16_t id = 0U;
    if (sscanf(args, "%hu", &id) == 1) {
      const bool ok = ctx.radio->playById(id, "serial_radio_play_id");
      serialDispatchReply(out,
                          "RADIO",
                          ok ? SerialDispatchResult::kOk : SerialDispatchResult::kNotFound,
                          ok ? "play_id" : "id");
      return true;
    }
    const bool ok = ctx.radio->playByUrl(args, "serial_radio_play_url");
    serialDispatchReply(out,
                        "RADIO",
                        ok ? SerialDispatchResult::kOk : SerialDispatchResult::kNotFound,
                        ok ? "play_url" : "url");
    return true;
  }

  if (serialTokenEquals(cmd, "RADIO_STOP")) {
    if (ctx.radio == nullptr) {
      serialDispatchReply(out, "RADIO", SerialDispatchResult::kOutOfContext, "missing_radio");
      return true;
    }
    ctx.radio->stop("serial_radio_stop");
    serialDispatchReply(out, "RADIO", SerialDispatchResult::kOk, "stop");
    return true;
  }

  if (serialTokenEquals(cmd, "RADIO_NEXT")) {
    if (ctx.radio == nullptr) {
      serialDispatchReply(out, "RADIO", SerialDispatchResult::kOutOfContext, "missing_radio");
      return true;
    }
    const bool ok = ctx.radio->next("serial_radio_next");
    serialDispatchReply(out, "RADIO", ok ? SerialDispatchResult::kOk : SerialDispatchResult::kNotFound, "next");
    return true;
  }

  if (serialTokenEquals(cmd, "RADIO_PREV")) {
    if (ctx.radio == nullptr) {
      serialDispatchReply(out, "RADIO", SerialDispatchResult::kOutOfContext, "missing_radio");
      return true;
    }
    const bool ok = ctx.radio->prev("serial_radio_prev");
    serialDispatchReply(out, "RADIO", ok ? SerialDispatchResult::kOk : SerialDispatchResult::kNotFound, "prev");
    return true;
  }

  if (serialTokenEquals(cmd, "RADIO_META")) {
    if (ctx.radio == nullptr) {
      serialDispatchReply(out, "RADIO", SerialDispatchResult::kOutOfContext, "missing_radio");
      return true;
    }
    printRadioStatus(out, ctx.radio->snapshot(), "meta");
    serialDispatchReply(out, "RADIO", SerialDispatchResult::kOk, "meta");
    return true;
  }

  if (serialTokenEquals(cmd, "WIFI_STATUS")) {
    if (ctx.wifi == nullptr) {
      serialDispatchReply(out, "WIFI", SerialDispatchResult::kOutOfContext, "missing_wifi");
      return true;
    }
    printWifiStatus(out, ctx.wifi->snapshot(), "status");
    serialDispatchReply(out, "WIFI", SerialDispatchResult::kOk, "status");
    return true;
  }

  if (serialTokenEquals(cmd, "WIFI_SCAN")) {
    if (ctx.wifi == nullptr) {
      serialDispatchReply(out, "WIFI", SerialDispatchResult::kOutOfContext, "missing_wifi");
      return true;
    }
    const bool ok = ctx.wifi->requestScan("serial_wifi_scan");
    serialDispatchReply(out, "WIFI", ok ? SerialDispatchResult::kOk : SerialDispatchResult::kBusy, "scan");
    return true;
  }

  if (serialTokenEquals(cmd, "WIFI_CONNECT")) {
    if (ctx.wifi == nullptr) {
      serialDispatchReply(out, "WIFI", SerialDispatchResult::kOutOfContext, "missing_wifi");
      return true;
    }
    char ssid[33] = {};
    char pass[65] = {};
    if (sscanf(args, "%32s %64s", ssid, pass) < 1) {
      serialDispatchReply(out, "WIFI", SerialDispatchResult::kBadArgs, "WIFI_CONNECT <ssid> <pass>");
      return true;
    }
    const bool ok = ctx.wifi->connectSta(ssid, pass, "serial_wifi_connect");
    serialDispatchReply(out, "WIFI", ok ? SerialDispatchResult::kOk : SerialDispatchResult::kBadArgs, "connect");
    return true;
  }

  if (serialTokenEquals(cmd, "WIFI_AP_ON")) {
    if (ctx.wifi == nullptr) {
      serialDispatchReply(out, "WIFI", SerialDispatchResult::kOutOfContext, "missing_wifi");
      return true;
    }
    char ssid[33] = {};
    char pass[65] = {};
    if (args[0] == '\0') {
      const bool ok = ctx.wifi->enableAp("U-SON-RADIO", "usonradio", "serial_ap_on");
      serialDispatchReply(out, "WIFI", ok ? SerialDispatchResult::kOk : SerialDispatchResult::kBusy, "ap_on");
      return true;
    }
    if (sscanf(args, "%32s %64s", ssid, pass) < 1) {
      serialDispatchReply(out, "WIFI", SerialDispatchResult::kBadArgs, "WIFI_AP_ON [ssid pass]");
      return true;
    }
    const bool ok = ctx.wifi->enableAp(ssid, pass, "serial_ap_on");
    serialDispatchReply(out, "WIFI", ok ? SerialDispatchResult::kOk : SerialDispatchResult::kBusy, "ap_on");
    return true;
  }

  if (serialTokenEquals(cmd, "WIFI_AP_OFF")) {
    if (ctx.wifi == nullptr) {
      serialDispatchReply(out, "WIFI", SerialDispatchResult::kOutOfContext, "missing_wifi");
      return true;
    }
    ctx.wifi->disableAp("serial_ap_off");
    serialDispatchReply(out, "WIFI", SerialDispatchResult::kOk, "ap_off");
    return true;
  }

  if (serialTokenEquals(cmd, "WEB_STATUS")) {
    if (ctx.web == nullptr) {
      serialDispatchReply(out, "WEB", SerialDispatchResult::kOutOfContext, "missing_web");
      return true;
    }
    printWebStatus(out, ctx.web->snapshot(), "status");
    serialDispatchReply(out, "WEB", SerialDispatchResult::kOk, "status");
    return true;
  }

  return false;
}
