#ifndef MEDIA_MEDIA_ROUTING_H
#define MEDIA_MEDIA_ROUTING_H

#include <Arduino.h>

enum class MediaSource : uint8_t {
    AUTO = 0,
    SD,
    LITTLEFS,
};

enum class MediaRouteKind : uint8_t {
    FILE = 0,
    TONE,
};

enum class ToneProfile : uint8_t {
    FR_FR = 0,
    ETSI_EU,
    UK_GB,
    NA_US,
    NONE,
};

enum class ToneEvent : uint8_t {
    DIAL = 0,
    SECONDARY_DIAL,
    SPECIAL_DIAL_STUTTER,
    RECALL_DIAL,
    RINGBACK,
    BUSY,
    CONGESTION,
    CALL_WAITING,
    CONFIRMATION,
    SIT_INTERCEPT,
    NONE,
};

inline String sanitizeMediaPath(const String& raw_path) {
    String path = raw_path;
    path.trim();
    if (path.isEmpty()) {
        return "";
    }

    if (path.length() >= 2U && path[0] == '"' && path[path.length() - 1U] == '"') {
        path = path.substring(1U, path.length() - 1U);
    }
    path.trim();
    if (path.isEmpty()) {
        return "";
    }

    String lower = path;
    lower.toLowerCase();
    if (lower == "null" || path.startsWith("{") || path.startsWith("[")) {
        return "";
    }
    if (!path.startsWith("/")) {
        path = "/" + path;
    }

    String lower_path = path;
    lower_path.toLowerCase();
    if (!lower_path.endsWith(".wav") && !lower_path.endsWith(".mp3")) {
        path += ".wav";
    }
    return path;
}

struct ToneRouteEntry {
    ToneProfile profile = ToneProfile::FR_FR;
    ToneEvent event = ToneEvent::DIAL;
};

struct FilePlaybackPolicy {
    bool loop = false;
    uint16_t pause_ms = 0U;
};

struct MediaRouteEntry {
    MediaRouteKind kind = MediaRouteKind::FILE;
    ToneRouteEntry tone{};
    String path;
    MediaSource source = MediaSource::AUTO;
    FilePlaybackPolicy playback{};
};

inline const char* mediaSourceToString(MediaSource source) {
    switch (source) {
        case MediaSource::SD:
            return "SD";
        case MediaSource::LITTLEFS:
            return "LITTLEFS";
        case MediaSource::AUTO:
        default:
            return "AUTO";
    }
}

inline bool parseMediaSource(const String& raw, MediaSource& out) {
    String value = raw;
    value.trim();
    value.toLowerCase();
    if (value.isEmpty() || value == "auto") {
        out = MediaSource::AUTO;
        return true;
    }
    if (value == "sd") {
        out = MediaSource::SD;
        return true;
    }
    if (value == "littlefs" || value == "ffat" || value == "flash") {
        out = MediaSource::LITTLEFS;
        return true;
    }
    return false;
}

inline const char* mediaRouteKindToString(MediaRouteKind kind) {
    switch (kind) {
        case MediaRouteKind::TONE:
            return "tone";
        case MediaRouteKind::FILE:
        default:
            return "file";
    }
}

inline bool parseMediaRouteKind(const String& raw, MediaRouteKind& out) {
    String value = raw;
    value.trim();
    value.toLowerCase();
    if (value == "tone") {
        out = MediaRouteKind::TONE;
        return true;
    }
    if (value.isEmpty() || value == "file") {
        out = MediaRouteKind::FILE;
        return true;
    }
    return false;
}

inline const char* toneProfileToString(ToneProfile profile) {
    switch (profile) {
        case ToneProfile::FR_FR:
            return "FR_FR";
        case ToneProfile::ETSI_EU:
            return "ETSI_EU";
        case ToneProfile::UK_GB:
            return "UK_GB";
        case ToneProfile::NA_US:
            return "NA_US";
        case ToneProfile::NONE:
        default:
            return "NONE";
    }
}

inline bool parseToneProfile(const String& raw, ToneProfile& out) {
    String value = raw;
    value.trim();
    value.toLowerCase();
    if (value == "fr_fr" || value == "fr") {
        out = ToneProfile::FR_FR;
        return true;
    }
    if (value == "etsi_eu" || value == "eu" || value == "etsi") {
        out = ToneProfile::ETSI_EU;
        return true;
    }
    if (value == "uk_gb" || value == "uk" || value == "gb") {
        out = ToneProfile::UK_GB;
        return true;
    }
    if (value == "na_us" || value == "us" || value == "na") {
        out = ToneProfile::NA_US;
        return true;
    }
    if (value == "none") {
        out = ToneProfile::NONE;
        return true;
    }
    return false;
}

inline const char* toneEventToString(ToneEvent event) {
    switch (event) {
        case ToneEvent::DIAL:
            return "dial";
        case ToneEvent::SECONDARY_DIAL:
            return "secondary_dial";
        case ToneEvent::SPECIAL_DIAL_STUTTER:
            return "special_dial_stutter";
        case ToneEvent::RECALL_DIAL:
            return "recall_dial";
        case ToneEvent::RINGBACK:
            return "ringback";
        case ToneEvent::BUSY:
            return "busy";
        case ToneEvent::CONGESTION:
            return "congestion";
        case ToneEvent::CALL_WAITING:
            return "call_waiting";
        case ToneEvent::CONFIRMATION:
            return "confirmation";
        case ToneEvent::SIT_INTERCEPT:
            return "sit_intercept";
        case ToneEvent::NONE:
        default:
            return "none";
    }
}

inline bool parseToneEvent(const String& raw, ToneEvent& out) {
    String value = raw;
    value.trim();
    value.toLowerCase();
    if (value == "dial") {
        out = ToneEvent::DIAL;
        return true;
    }
    if (value == "secondary_dial") {
        out = ToneEvent::SECONDARY_DIAL;
        return true;
    }
    if (value == "special_dial_stutter" || value == "special_dial_mwi_stutter") {
        out = ToneEvent::SPECIAL_DIAL_STUTTER;
        return true;
    }
    if (value == "recall_dial") {
        out = ToneEvent::RECALL_DIAL;
        return true;
    }
    if (value == "ringback") {
        out = ToneEvent::RINGBACK;
        return true;
    }
    if (value == "busy") {
        out = ToneEvent::BUSY;
        return true;
    }
    if (value == "congestion" || value == "reorder") {
        out = ToneEvent::CONGESTION;
        return true;
    }
    if (value == "call_waiting") {
        out = ToneEvent::CALL_WAITING;
        return true;
    }
    if (value == "confirmation") {
        out = ToneEvent::CONFIRMATION;
        return true;
    }
    if (value == "sit_intercept") {
        out = ToneEvent::SIT_INTERCEPT;
        return true;
    }
    if (value == "none") {
        out = ToneEvent::NONE;
        return true;
    }
    return false;
}

inline bool isLegacyToneWavPath(const String& raw_path, ToneRouteEntry* out_tone = nullptr) {
    String path = raw_path;
    path.trim();
    if (path.isEmpty()) {
        return false;
    }
    if (path.length() >= 2U && path[0] == '"' && path[path.length() - 1U] == '"') {
        path = path.substring(1, path.length() - 1);
    }
    path.trim();
    if (path.isEmpty()) {
        return false;
    }
    if (!path.startsWith("/")) {
        path = "/" + path;
    }
    path.toLowerCase();
    if (!path.startsWith("/assets/wav/") || !path.endsWith(".wav")) {
        return false;
    }

    const int profile_begin = static_cast<int>(strlen("/assets/wav/"));
    const int profile_end = path.indexOf('/', profile_begin);
    if (profile_end <= profile_begin) {
        return false;
    }
    const int event_begin = profile_end + 1;
    const int ext_pos = path.lastIndexOf('.');
    if (ext_pos <= event_begin) {
        return false;
    }

    const String profile_raw = path.substring(profile_begin, profile_end);
    const String event_raw = path.substring(event_begin, ext_pos);

    ToneProfile profile = ToneProfile::NONE;
    ToneEvent event = ToneEvent::NONE;
    if (!parseToneProfile(profile_raw, profile) || !parseToneEvent(event_raw, event)) {
        return false;
    }

    if (out_tone != nullptr) {
        out_tone->profile = profile;
        out_tone->event = event;
    }
    return true;
}

inline bool mediaRouteHasPayload(const MediaRouteEntry& route) {
    if (route.kind == MediaRouteKind::TONE) {
        return route.tone.profile != ToneProfile::NONE && route.tone.event != ToneEvent::NONE;
    }
    return !route.path.isEmpty();
}

#endif  // MEDIA_MEDIA_ROUTING_H
