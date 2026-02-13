#include "serial_commands_mp3.h"

#include <cstring>
#include <cctype>
#include <cstdio>

#include "../../config.h"
#include "serial_dispatch.h"

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

bool textEquals(const char* lhs, const char* rhs) {
  if (lhs == nullptr || rhs == nullptr) {
    return false;
  }
  return strcmp(lhs, rhs) == 0;
}

bool textEqualsIgnoreCase(const char* lhs, const char* rhs) {
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
  return (*lhs == '\0' && *rhs == '\0');
}

bool allowPlayback(const Mp3SerialRuntimeContext& ctx) {
  return (ctx.allowPlaybackNow != nullptr) ? ctx.allowPlaybackNow() : false;
}

void printUiStatus(Print& out, const Mp3SerialRuntimeContext& ctx, const char* source) {
  if (ctx.printUiStatus != nullptr) {
    ctx.printUiStatus(source != nullptr ? source : "status");
    return;
  }
  if (ctx.ui == nullptr || ctx.player == nullptr) {
    serialDispatchReply(out, "MP3_UI", SerialDispatchResult::kOutOfContext, "missing_context");
    return;
  }
  out.printf("[MP3_UI] %s page=%s cursor=%u offset=%u browse=%u queue_off=%u set_idx=%u tracks=%u\n",
             source != nullptr ? source : "status",
             playerUiPageLabel(ctx.ui->page()),
             static_cast<unsigned int>(ctx.ui->cursor()),
             static_cast<unsigned int>(ctx.ui->offset()),
             static_cast<unsigned int>(ctx.ui->browseCount()),
             static_cast<unsigned int>(ctx.ui->queueOffset()),
             static_cast<unsigned int>(ctx.ui->settingsIndex()),
             static_cast<unsigned int>(ctx.player->trackCount()));
}

}  // namespace

bool serialIsMp3Command(const char* token) {
  return token != nullptr && strncmp(token, "MP3_", 4U) == 0;
}

bool serialProcessMp3Command(const SerialCommand& cmd,
                             uint32_t nowMs,
                             const Mp3SerialRuntimeContext& ctx,
                             Print& out) {
  if (cmd.token == nullptr || cmd.token[0] == '\0' || ctx.player == nullptr) {
    return false;
  }

  Mp3Player& player = *ctx.player;
  PlayerUiModel* ui = ctx.ui;
  const char* args = skipSpaces(cmd.args);

  if (serialTokenEquals(cmd, "MP3_HELP")) {
    if (ctx.printHelp != nullptr) {
      ctx.printHelp();
    }
    serialDispatchReply(out, "MP3", SerialDispatchResult::kOk, "help");
    return true;
  }

  if (serialTokenEquals(cmd, "MP3_STATUS")) {
    if (ctx.printStatus != nullptr) {
      ctx.printStatus("status");
    }
    serialDispatchReply(out, "MP3", SerialDispatchResult::kOk, "status");
    return true;
  }

  if (serialTokenEquals(cmd, "MP3_BACKEND_STATUS")) {
    if (ctx.printBackendStatus != nullptr) {
      ctx.printBackendStatus("status");
    }
    serialDispatchReply(out, "MP3_BACKEND", SerialDispatchResult::kOk, "status");
    return true;
  }

  if (serialTokenEquals(cmd, "MP3_BACKEND")) {
    if (args[0] == '\0' || textEqualsIgnoreCase(args, "STATUS")) {
      out.printf("[MP3_BACKEND] mode=%s active=%s err=%s\n",
                 player.backendModeLabel(),
                 player.activeBackendLabel(),
                 player.lastBackendError());
      serialDispatchReply(out, "MP3_BACKEND", SerialDispatchResult::kOk, "status");
      return true;
    }

    char modeToken[24] = {};
    if (sscanf(args, "SET %23s", modeToken) == 1) {
      PlayerBackendMode mode = PlayerBackendMode::kAutoFallback;
      if (ctx.parseBackendModeToken == nullptr || !ctx.parseBackendModeToken(modeToken, &mode)) {
        serialDispatchReply(out, "MP3_BACKEND", SerialDispatchResult::kBadArgs, "AUTO|AUDIO_TOOLS|LEGACY");
        return true;
      }
      player.setBackendMode(mode);
      out.printf("[MP3_BACKEND] SET mode=%s\n", player.backendModeLabel());
      if (ctx.printStatus != nullptr) {
        ctx.printStatus("backend_set");
      }
      serialDispatchReply(out, "MP3_BACKEND", SerialDispatchResult::kOk, player.backendModeLabel());
      return true;
    }

    serialDispatchReply(out, "MP3_BACKEND", SerialDispatchResult::kBadArgs, "STATUS|SET <mode>");
    return true;
  }

  if (serialTokenEquals(cmd, "MP3_SCAN")) {
    if (args[0] == '\0' || textEqualsIgnoreCase(args, "STATUS")) {
      if (ctx.printScanStatus != nullptr) {
        ctx.printScanStatus("status");
      }
      serialDispatchReply(out, "MP3_SCAN", SerialDispatchResult::kOk, "status");
      return true;
    }
    if (textEqualsIgnoreCase(args, "START")) {
      player.requestCatalogScan(false);
      player.update(nowMs, allowPlayback(ctx));
      if (ctx.printScanStatus != nullptr) {
        ctx.printScanStatus("start");
      }
      serialDispatchReply(out, "MP3_SCAN", SerialDispatchResult::kOk, "start");
      return true;
    }
    if (textEqualsIgnoreCase(args, "REBUILD")) {
      player.requestCatalogScan(true);
      player.update(nowMs, allowPlayback(ctx));
      if (ctx.printScanStatus != nullptr) {
        ctx.printScanStatus("rebuild");
      }
      serialDispatchReply(out, "MP3_SCAN", SerialDispatchResult::kOk, "rebuild");
      return true;
    }
    if (textEqualsIgnoreCase(args, "CANCEL")) {
      const bool canceled = player.cancelCatalogScan();
      serialDispatchReply(out,
                          "MP3_SCAN",
                          canceled ? SerialDispatchResult::kOk : SerialDispatchResult::kOutOfContext,
                          canceled ? "canceled" : "idle");
      return true;
    }
    serialDispatchReply(out, "MP3_SCAN", SerialDispatchResult::kBadArgs, "START|STATUS|CANCEL|REBUILD");
    return true;
  }

  if (serialTokenEquals(cmd, "MP3_SCAN_PROGRESS")) {
    if (ctx.printScanProgress != nullptr) {
      ctx.printScanProgress("status");
    }
    serialDispatchReply(out, "MP3_SCAN_PROGRESS", SerialDispatchResult::kOk, "status");
    return true;
  }

  if (serialTokenEquals(cmd, "MP3_BROWSE")) {
    if (strncmp(args, "LS", 2) == 0 && (args[2] == '\0' || args[2] == ' ')) {
      const char* path = skipSpaces(args + 2);
      const char* target = (path[0] == '\0' && ctx.currentBrowsePath != nullptr) ? ctx.currentBrowsePath() : path;
      if (ctx.printBrowseList != nullptr) {
        ctx.printBrowseList("ls", target, 0U, 12U);
      }
      serialDispatchReply(out, "MP3_BROWSE", SerialDispatchResult::kOk, "ls");
      return true;
    }
    if (strncmp(args, "CD", 2) == 0 && (args[2] == '\0' || args[2] == ' ')) {
      const char* path = skipSpaces(args + 2);
      if (path[0] == '\0') {
        serialDispatchReply(out, "MP3_BROWSE", SerialDispatchResult::kBadArgs, "path required");
        return true;
      }
      String normalized(path);
      if (!normalized.startsWith("/")) {
        normalized = "/" + normalized;
      }
      const uint16_t count = player.countTracks(normalized.c_str());
      if (count == 0U) {
        serialDispatchReply(out, "MP3_BROWSE", SerialDispatchResult::kNotFound, normalized.c_str());
        return true;
      }
      if (ctx.setBrowsePath != nullptr) {
        ctx.setBrowsePath(normalized.c_str());
      }
      if (ctx.setUiPage != nullptr) {
        ctx.setUiPage(PlayerUiPage::kBrowser);
      }
      out.printf("[MP3_BROWSE] CD path=%s count=%u\n", normalized.c_str(), static_cast<unsigned int>(count));
      serialDispatchReply(out, "MP3_BROWSE", SerialDispatchResult::kOk, "cd");
      return true;
    }
    serialDispatchReply(out, "MP3_BROWSE", SerialDispatchResult::kBadArgs, "LS|CD");
    return true;
  }

  if (serialTokenEquals(cmd, "MP3_PLAY_PATH")) {
    if (args[0] == '\0') {
      serialDispatchReply(out, "MP3", SerialDispatchResult::kBadArgs, "MP3_PLAY_PATH <path>");
      return true;
    }
    if (!player.playPath(args)) {
      serialDispatchReply(out, "MP3", SerialDispatchResult::kNotFound, args);
      return true;
    }
    if (ctx.printStatus != nullptr) {
      ctx.printStatus("play_path");
    }
    serialDispatchReply(out, "MP3", SerialDispatchResult::kOk, "play_path");
    return true;
  }

  if (serialTokenEquals(cmd, "MP3_UI_STATUS")) {
    printUiStatus(out, ctx, "status");
    serialDispatchReply(out, "MP3_UI", SerialDispatchResult::kOk, "status");
    return true;
  }

  if (serialTokenEquals(cmd, "MP3_UI")) {
    if (args[0] == '\0' || textEqualsIgnoreCase(args, "STATUS")) {
      printUiStatus(out, ctx, "status");
      serialDispatchReply(out, "MP3_UI", SerialDispatchResult::kOk, "status");
      return true;
    }

    char pageToken[16] = {};
    if (sscanf(args, "PAGE %15s", pageToken) == 1) {
      PlayerUiPage page = PlayerUiPage::kNowPlaying;
      if (ctx.parsePlayerUiPageToken == nullptr || !ctx.parsePlayerUiPageToken(pageToken, &page)) {
        serialDispatchReply(out, "MP3_UI", SerialDispatchResult::kBadArgs, "NOW|BROWSE|QUEUE|SET");
        return true;
      }
      if (ctx.setUiPage != nullptr) {
        ctx.setUiPage(page);
      } else if (ui != nullptr) {
        ui->setPage(page);
      }
      out.printf("[MP3_UI] PAGE %s\n", (ui != nullptr) ? playerUiPageLabel(ui->page()) : playerUiPageLabel(page));
      serialDispatchReply(out, "MP3_UI", SerialDispatchResult::kOk, "page");
      return true;
    }

    serialDispatchReply(out, "MP3_UI", SerialDispatchResult::kBadArgs, "STATUS|PAGE <NOW|BROWSE|QUEUE|SET>");
    return true;
  }

  if (serialTokenEquals(cmd, "MP3_QUEUE_PREVIEW")) {
    int count = 5;
    if (args[0] != '\0' && sscanf(args, "%d", &count) != 1) {
      serialDispatchReply(out, "MP3_QUEUE", SerialDispatchResult::kBadArgs, "[n]");
      return true;
    }
    if (ctx.printQueuePreview != nullptr) {
      ctx.printQueuePreview(static_cast<uint8_t>(count), "preview");
      serialDispatchReply(out, "MP3_QUEUE", SerialDispatchResult::kOk, "preview");
    } else {
      serialDispatchReply(out, "MP3_QUEUE", SerialDispatchResult::kOutOfContext, "missing_callback");
    }
    return true;
  }

  if (serialTokenEquals(cmd, "MP3_CAPS")) {
    if (ctx.printCaps != nullptr) {
      ctx.printCaps("status");
      serialDispatchReply(out, "MP3_CAPS", SerialDispatchResult::kOk, "status");
    } else {
      serialDispatchReply(out, "MP3_CAPS", SerialDispatchResult::kOutOfContext, "missing_callback");
    }
    return true;
  }

  if (serialTokenEquals(cmd, "MP3_STATE")) {
    if (textEqualsIgnoreCase(args, "SAVE")) {
      serialDispatchReply(out,
                          "MP3_STATE",
                          player.savePlayerState() ? SerialDispatchResult::kOk : SerialDispatchResult::kBusy,
                          "save");
      return true;
    }
    if (textEqualsIgnoreCase(args, "LOAD")) {
      const bool ok = player.loadPlayerState();
      if (ok) {
        player.requestStorageRefresh(false);
        player.update(nowMs, allowPlayback(ctx));
      }
      serialDispatchReply(out, "MP3_STATE", ok ? SerialDispatchResult::kOk : SerialDispatchResult::kBusy, "load");
      return true;
    }
    if (textEqualsIgnoreCase(args, "RESET")) {
      serialDispatchReply(out,
                          "MP3_STATE",
                          player.resetPlayerState() ? SerialDispatchResult::kOk : SerialDispatchResult::kBusy,
                          "reset");
      return true;
    }
    serialDispatchReply(out, "MP3_STATE", SerialDispatchResult::kBadArgs, "SAVE|LOAD|RESET");
    return true;
  }

  if (serialTokenEquals(cmd, "MP3_UNLOCK")) {
    if (ctx.forceUsonFunctional != nullptr) {
      ctx.forceUsonFunctional("serial_mp3_unlock");
    }
    player.requestStorageRefresh(false);
    player.update(nowMs, false);
    if (ctx.printStatus != nullptr) {
      ctx.printStatus("unlock");
    }
    serialDispatchReply(out, "MP3", SerialDispatchResult::kOk, "unlock");
    return true;
  }

  if (serialTokenEquals(cmd, "MP3_REFRESH")) {
    player.requestStorageRefresh(true);
    player.update(nowMs, allowPlayback(ctx));
    if (ctx.printStatus != nullptr) {
      ctx.printStatus("refresh");
    }
    serialDispatchReply(out, "MP3", SerialDispatchResult::kOk, "refresh");
    return true;
  }

  if (serialTokenEquals(cmd, "MP3_LIST")) {
    const char* browse = (ctx.currentBrowsePath != nullptr) ? ctx.currentBrowsePath() : "/";
    if (ctx.printBrowseList != nullptr) {
      ctx.printBrowseList("list", browse, 0U, 24U);
    }
    serialDispatchReply(out, "MP3", SerialDispatchResult::kOk, "list");
    return true;
  }

  if (serialTokenEquals(cmd, "MP3_NEXT")) {
    player.nextTrack();
    if (ctx.printStatus != nullptr) {
      ctx.printStatus("next");
    }
    serialDispatchReply(out, "MP3", SerialDispatchResult::kOk, "next");
    return true;
  }

  if (serialTokenEquals(cmd, "MP3_PREV")) {
    player.previousTrack();
    if (ctx.printStatus != nullptr) {
      ctx.printStatus("prev");
    }
    serialDispatchReply(out, "MP3", SerialDispatchResult::kOk, "prev");
    return true;
  }

  if (serialTokenEquals(cmd, "MP3_RESTART")) {
    player.restartTrack();
    if (ctx.printStatus != nullptr) {
      ctx.printStatus("restart");
    }
    serialDispatchReply(out, "MP3", SerialDispatchResult::kOk, "restart");
    return true;
  }

  if (serialTokenEquals(cmd, "MP3_PLAY")) {
    int trackNum = 0;
    if (sscanf(args, "%d", &trackNum) != 1 || trackNum < 1) {
      serialDispatchReply(out, "MP3", SerialDispatchResult::kBadArgs, "MP3_PLAY <track>=1");
      return true;
    }
    player.requestStorageRefresh(false);
    player.update(nowMs, allowPlayback(ctx));
    const uint16_t count = player.trackCount();
    if (!player.isSdReady() || count == 0U) {
      serialDispatchReply(out, "MP3", SerialDispatchResult::kOutOfContext, "sd/tracks unavailable");
      return true;
    }
    if (trackNum > static_cast<int>(count)) {
      serialDispatchReply(out, "MP3", SerialDispatchResult::kBadArgs, "track>count");
      return true;
    }
    if (!player.selectTrackByIndex(static_cast<uint16_t>(trackNum - 1), true)) {
      serialDispatchReply(out, "MP3", SerialDispatchResult::kBusy, "select failed");
      return true;
    }
    if (ctx.printStatus != nullptr) {
      ctx.printStatus("play");
    }
    serialDispatchReply(out, "MP3", SerialDispatchResult::kOk, "play");
    return true;
  }

  if (serialTokenEquals(cmd, "MP3_FX_STOP")) {
    if (ctx.stopOverlayFx != nullptr) {
      ctx.stopOverlayFx("serial_mp3_fx_stop");
    }
    if (ctx.printStatus != nullptr) {
      ctx.printStatus("fx_stop");
    }
    serialDispatchReply(out, "MP3_FX", SerialDispatchResult::kOk, "stop");
    return true;
  }

  if (serialTokenEquals(cmd, "MP3_FX_MODE")) {
    char modeToken[16] = {};
    if (sscanf(args, "%15s", modeToken) != 1) {
      serialDispatchReply(out, "MP3_FX", SerialDispatchResult::kBadArgs, "DUCKING|OVERLAY");
      return true;
    }
    if (textEqualsIgnoreCase(modeToken, "DUCKING")) {
      player.setFxMode(Mp3FxMode::kDucking);
      if (ctx.printStatus != nullptr) {
        ctx.printStatus("fx_mode");
      }
      serialDispatchReply(out, "MP3_FX", SerialDispatchResult::kOk, "DUCKING");
      return true;
    }
    if (textEqualsIgnoreCase(modeToken, "OVERLAY")) {
      player.setFxMode(Mp3FxMode::kOverlay);
      if (ctx.printStatus != nullptr) {
        ctx.printStatus("fx_mode");
      }
      serialDispatchReply(out, "MP3_FX", SerialDispatchResult::kOk, "OVERLAY");
      return true;
    }
    serialDispatchReply(out, "MP3_FX", SerialDispatchResult::kBadArgs, "DUCKING|OVERLAY");
    return true;
  }

  if (serialTokenEquals(cmd, "MP3_FX_GAIN")) {
    int duckPct = -1;
    int mixPct = -1;
    if (sscanf(args, "%d %d", &duckPct, &mixPct) != 2) {
      serialDispatchReply(out, "MP3_FX", SerialDispatchResult::kBadArgs, "<duck%> <mix%>");
      return true;
    }
    if (duckPct < 0 || duckPct > 100 || mixPct < 0 || mixPct > 100) {
      serialDispatchReply(out, "MP3_FX", SerialDispatchResult::kBadArgs, "0..100 0..100");
      return true;
    }
    player.setFxDuckingGain(static_cast<float>(duckPct) / 100.0f);
    player.setFxOverlayGain(static_cast<float>(mixPct) / 100.0f);
    if (ctx.printStatus != nullptr) {
      ctx.printStatus("fx_gain");
    }
    serialDispatchReply(out, "MP3_FX", SerialDispatchResult::kOk, "gain");
    return true;
  }

  if (serialTokenEquals(cmd, "MP3_FX")) {
    char fxToken[16] = {};
    int durationMs = static_cast<int>(config::kMp3FxDefaultDurationMs);
    if (sscanf(args, "%15s %d", fxToken, &durationMs) < 1) {
      serialDispatchReply(out, "MP3_FX", SerialDispatchResult::kBadArgs, "FM|SONAR|MORSE|WIN [ms]");
      return true;
    }
    Mp3FxEffect effect = Mp3FxEffect::kFmSweep;
    if (ctx.parseMp3FxEffectToken == nullptr || !ctx.parseMp3FxEffectToken(fxToken, &effect)) {
      serialDispatchReply(out, "MP3_FX", SerialDispatchResult::kBadArgs, "FM|SONAR|MORSE|WIN");
      return true;
    }
    if (ctx.forceUsonFunctional != nullptr) {
      ctx.forceUsonFunctional("serial_mp3_fx");
    }
    player.requestStorageRefresh(false);
    player.update(nowMs, allowPlayback(ctx));
    const uint32_t safeDuration = (durationMs > 0) ? static_cast<uint32_t>(durationMs)
                                                   : static_cast<uint32_t>(config::kMp3FxDefaultDurationMs);
    const bool ok = (ctx.triggerMp3Fx != nullptr) && ctx.triggerMp3Fx(effect, safeDuration, "serial_mp3_fx");
    if (ctx.printStatus != nullptr) {
      ctx.printStatus("fx");
    }
    serialDispatchReply(out, "MP3_FX", ok ? SerialDispatchResult::kOk : SerialDispatchResult::kBusy, "trigger");
    return true;
  }

  if (serialTokenEquals(cmd, "MP3_TEST_START")) {
    int dwellMs = 3500;
    if (args[0] != '\0' && sscanf(args, "%d", &dwellMs) != 1) {
      serialDispatchReply(out, "MP3_TEST", SerialDispatchResult::kBadArgs, "[ms]");
      return true;
    }
    if (dwellMs < 1600) {
      dwellMs = 1600;
    } else if (dwellMs > 15000) {
      dwellMs = 15000;
    }
    if (ctx.startFormatTest == nullptr) {
      serialDispatchReply(out, "MP3_TEST", SerialDispatchResult::kOutOfContext, "missing_callback");
      return true;
    }
    const bool ok = ctx.startFormatTest(nowMs, static_cast<uint32_t>(dwellMs));
    serialDispatchReply(out, "MP3_TEST", ok ? SerialDispatchResult::kOk : SerialDispatchResult::kOutOfContext, "start");
    return true;
  }

  if (serialTokenEquals(cmd, "MP3_TEST_STOP")) {
    if (ctx.stopFormatTest != nullptr) {
      ctx.stopFormatTest("serial_stop");
    }
    serialDispatchReply(out, "MP3_TEST", SerialDispatchResult::kOk, "stop");
    return true;
  }

  return false;
}
