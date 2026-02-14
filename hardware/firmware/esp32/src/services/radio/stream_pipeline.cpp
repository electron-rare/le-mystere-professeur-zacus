#include "stream_pipeline.h"

namespace {

constexpr uint32_t kConnectMs = 450U;
constexpr uint32_t kBufferMs = 900U;
constexpr uint32_t kRetryBackoffMs = 1800U;

}  // namespace

void StreamPipeline::begin() {
  snap_ = Snapshot();
  stateSinceMs_ = millis();
}

void StreamPipeline::update(uint32_t nowMs, bool networkReady) {
  if (snap_.state == State::kIdle) {
    return;
  }

  if (!networkReady) {
    if (snap_.state != State::kRetrying) {
      copyText(snap_.lastError, sizeof(snap_.lastError), "NET_DOWN");
      setState(State::kRetrying, nowMs);
      nextRetryMs_ = nowMs + kRetryBackoffMs;
      ++snap_.retries;
    }
    return;
  }

  switch (snap_.state) {
    case State::kConnecting:
      if (nowMs - stateSinceMs_ >= kConnectMs) {
        snap_.bufferPercent = 20U;
        setState(State::kBuffering, nowMs);
      }
      break;
    case State::kBuffering:
      if (nowMs - stateSinceMs_ >= kBufferMs) {
        snap_.bufferPercent = 100U;
        if (snap_.title[0] == '\0') {
          copyText(snap_.title, sizeof(snap_.title), "Flux radio actif");
        }
        if (snap_.bitrateKbps == 0U) {
          snap_.bitrateKbps = 128U;
        }
        setState(State::kStreaming, nowMs);
      } else {
        const uint32_t elapsed = nowMs - stateSinceMs_;
        const uint8_t pct = static_cast<uint8_t>(20U + (elapsed * 80U) / kBufferMs);
        snap_.bufferPercent = pct;
      }
      break;
    case State::kStreaming:
      break;
    case State::kRetrying:
      if (static_cast<int32_t>(nowMs - nextRetryMs_) >= 0) {
        setState(State::kConnecting, nowMs);
        snap_.bufferPercent = 0U;
      }
      break;
    case State::kError:
    case State::kIdle:
    default:
      break;
  }
}

bool StreamPipeline::start(const char* url, const char* codec, const char* reason) {
  (void)reason;
  if (url == nullptr || url[0] == '\0') {
    copyText(snap_.lastError, sizeof(snap_.lastError), "URL_EMPTY");
    setState(State::kError, millis());
    return false;
  }
  copyText(snap_.url, sizeof(snap_.url), url);
  copyText(snap_.codec, sizeof(snap_.codec), (codec != nullptr && codec[0] != '\0') ? codec : "AUTO");
  copyText(snap_.title, sizeof(snap_.title), "");
  snap_.bitrateKbps = 0U;
  snap_.bufferPercent = 0U;
  copyText(snap_.lastError, sizeof(snap_.lastError), "OK");
  setState(State::kConnecting, millis());
  return true;
}

void StreamPipeline::stop(const char* reason) {
  copyText(snap_.lastError, sizeof(snap_.lastError), (reason != nullptr) ? reason : "STOP");
  snap_.bufferPercent = 0U;
  setState(State::kIdle, millis());
}

StreamPipeline::Snapshot StreamPipeline::snapshot() const {
  return snap_;
}

bool StreamPipeline::isActive() const {
  return snap_.state == State::kConnecting ||
         snap_.state == State::kBuffering ||
         snap_.state == State::kStreaming ||
         snap_.state == State::kRetrying;
}

const char* StreamPipeline::stateLabel(State state) {
  switch (state) {
    case State::kIdle:
      return "IDLE";
    case State::kConnecting:
      return "CONNECTING";
    case State::kBuffering:
      return "BUFFERING";
    case State::kStreaming:
      return "STREAMING";
    case State::kRetrying:
      return "RETRYING";
    case State::kError:
      return "ERROR";
    default:
      return "UNKNOWN";
  }
}

void StreamPipeline::setState(State state, uint32_t nowMs) {
  snap_.state = state;
  snap_.lastStateMs = nowMs;
  stateSinceMs_ = nowMs;
}

void StreamPipeline::copyText(char* out, size_t outLen, const char* text) {
  if (out == nullptr || outLen == 0U) {
    return;
  }
  out[0] = '\0';
  if (text == nullptr || text[0] == '\0') {
    return;
  }
  snprintf(out, outLen, "%s", text);
}
