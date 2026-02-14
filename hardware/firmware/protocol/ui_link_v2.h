#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#ifdef __cplusplus
extern "C" {
#endif

enum {
  UILINK_V2_PROTO = 2,
  UILINK_V2_MAX_LINE = 320,
  UILINK_V2_MAX_FIELDS = 40,
  UILINK_V2_TYPE_MAX = 16,
  UILINK_V2_KEY_MAX = 24,
  UILINK_V2_VALUE_MAX = 96,
  UILINK_V2_HEARTBEAT_MS = 1000,
  UILINK_V2_TIMEOUT_MS = 1500,
};

typedef enum UiLinkMsgType {
  UILINK_MSG_UNKNOWN = 0,
  UILINK_MSG_HELLO,
  UILINK_MSG_ACK,
  UILINK_MSG_CAPS,
  UILINK_MSG_STAT,
  UILINK_MSG_KEYFRAME,
  UILINK_MSG_BTN,
  UILINK_MSG_TOUCH,
  UILINK_MSG_CMD,
  UILINK_MSG_PING,
  UILINK_MSG_PONG,
} UiLinkMsgType;

typedef enum UiBtnId {
  UI_BTN_UNKNOWN = 0,
  UI_BTN_OK,
  UI_BTN_NEXT,
  UI_BTN_PREV,
  UI_BTN_BACK,
  UI_BTN_VOL_UP,
  UI_BTN_VOL_DOWN,
  UI_BTN_MODE,
} UiBtnId;

typedef enum UiBtnAction {
  UI_BTN_ACTION_UNKNOWN = 0,
  UI_BTN_ACTION_DOWN,
  UI_BTN_ACTION_UP,
  UI_BTN_ACTION_CLICK,
  UI_BTN_ACTION_LONG,
} UiBtnAction;

typedef enum UiTouchAction {
  UI_TOUCH_ACTION_UNKNOWN = 0,
  UI_TOUCH_ACTION_DOWN,
  UI_TOUCH_ACTION_MOVE,
  UI_TOUCH_ACTION_UP,
} UiTouchAction;

typedef struct UiLinkField {
  char key[UILINK_V2_KEY_MAX];
  char value[UILINK_V2_VALUE_MAX];
} UiLinkField;

typedef struct UiLinkFrame {
  UiLinkMsgType type;
  char type_token[UILINK_V2_TYPE_MAX];
  UiLinkField fields[UILINK_V2_MAX_FIELDS];
  uint8_t field_count;
  bool has_crc;
  uint8_t crc_expected;
  uint8_t crc_computed;
  bool crc_ok;
} UiLinkFrame;

static inline uint8_t uiLinkCrc8(const uint8_t* data, size_t len) {
  uint8_t crc = 0x00u;
  if (data == NULL) {
    return crc;
  }
  for (size_t i = 0; i < len; ++i) {
    crc ^= data[i];
    for (uint8_t bit = 0u; bit < 8u; ++bit) {
      if ((crc & 0x80u) != 0u) {
        crc = (uint8_t)((crc << 1u) ^ 0x07u);
      } else {
        crc <<= 1u;
      }
    }
  }
  return crc;
}

static inline bool uiLinkIsHex(char c) {
  return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F');
}

static inline uint8_t uiLinkHexValue(char c) {
  if (c >= '0' && c <= '9') {
    return (uint8_t)(c - '0');
  }
  if (c >= 'a' && c <= 'f') {
    return (uint8_t)(10 + (c - 'a'));
  }
  return (uint8_t)(10 + (c - 'A'));
}

static inline bool uiLinkParseHexByte(const char* s, uint8_t* out) {
  if (s == NULL || out == NULL) {
    return false;
  }
  if (!uiLinkIsHex(s[0]) || !uiLinkIsHex(s[1])) {
    return false;
  }
  *out = (uint8_t)((uiLinkHexValue(s[0]) << 4u) | uiLinkHexValue(s[1]));
  return true;
}

static inline UiLinkMsgType uiLinkMsgTypeFromToken(const char* token) {
  if (token == NULL) {
    return UILINK_MSG_UNKNOWN;
  }
  if (strcmp(token, "HELLO") == 0) return UILINK_MSG_HELLO;
  if (strcmp(token, "ACK") == 0) return UILINK_MSG_ACK;
  if (strcmp(token, "CAPS") == 0) return UILINK_MSG_CAPS;
  if (strcmp(token, "STAT") == 0) return UILINK_MSG_STAT;
  if (strcmp(token, "KEYFRAME") == 0) return UILINK_MSG_KEYFRAME;
  if (strcmp(token, "BTN") == 0) return UILINK_MSG_BTN;
  if (strcmp(token, "TOUCH") == 0) return UILINK_MSG_TOUCH;
  if (strcmp(token, "CMD") == 0) return UILINK_MSG_CMD;
  if (strcmp(token, "PING") == 0) return UILINK_MSG_PING;
  if (strcmp(token, "PONG") == 0) return UILINK_MSG_PONG;
  return UILINK_MSG_UNKNOWN;
}

static inline UiBtnId uiBtnIdFromToken(const char* token) {
  if (token == NULL) {
    return UI_BTN_UNKNOWN;
  }
  if (strcmp(token, "OK") == 0) return UI_BTN_OK;
  if (strcmp(token, "NEXT") == 0) return UI_BTN_NEXT;
  if (strcmp(token, "PREV") == 0) return UI_BTN_PREV;
  if (strcmp(token, "BACK") == 0) return UI_BTN_BACK;
  if (strcmp(token, "VOL_UP") == 0) return UI_BTN_VOL_UP;
  if (strcmp(token, "VOL_DOWN") == 0) return UI_BTN_VOL_DOWN;
  if (strcmp(token, "MODE") == 0) return UI_BTN_MODE;
  return UI_BTN_UNKNOWN;
}

static inline UiBtnAction uiBtnActionFromToken(const char* token) {
  if (token == NULL) {
    return UI_BTN_ACTION_UNKNOWN;
  }
  if (strcmp(token, "down") == 0) return UI_BTN_ACTION_DOWN;
  if (strcmp(token, "up") == 0) return UI_BTN_ACTION_UP;
  if (strcmp(token, "click") == 0) return UI_BTN_ACTION_CLICK;
  if (strcmp(token, "long") == 0) return UI_BTN_ACTION_LONG;
  return UI_BTN_ACTION_UNKNOWN;
}

static inline UiTouchAction uiTouchActionFromToken(const char* token) {
  if (token == NULL) {
    return UI_TOUCH_ACTION_UNKNOWN;
  }
  if (strcmp(token, "down") == 0) return UI_TOUCH_ACTION_DOWN;
  if (strcmp(token, "move") == 0) return UI_TOUCH_ACTION_MOVE;
  if (strcmp(token, "up") == 0) return UI_TOUCH_ACTION_UP;
  return UI_TOUCH_ACTION_UNKNOWN;
}

static inline const UiLinkField* uiLinkFindField(const UiLinkFrame* frame, const char* key) {
  if (frame == NULL || key == NULL) {
    return NULL;
  }
  for (uint8_t i = 0u; i < frame->field_count; ++i) {
    if (strcmp(frame->fields[i].key, key) == 0) {
      return &frame->fields[i];
    }
  }
  return NULL;
}

static inline bool uiLinkCopyBounded(char* dst, size_t dst_len, const char* src, size_t src_len) {
  if (dst == NULL || dst_len == 0u || src == NULL) {
    return false;
  }
  if (src_len >= dst_len) {
    return false;
  }
  memcpy(dst, src, src_len);
  dst[src_len] = '\0';
  return true;
}

static inline bool uiLinkParseLine(const char* line, UiLinkFrame* out) {
  if (line == NULL || out == NULL) {
    return false;
  }

  memset(out, 0, sizeof(*out));

  size_t line_len = strnlen(line, UILINK_V2_MAX_LINE + 1u);
  if (line_len == 0u || line_len > UILINK_V2_MAX_LINE) {
    return false;
  }

  while (line_len > 0u && (line[line_len - 1u] == '\n' || line[line_len - 1u] == '\r')) {
    --line_len;
  }
  if (line_len == 0u || line_len > UILINK_V2_MAX_LINE) {
    return false;
  }

  const char* star = NULL;
  for (size_t i = 0; i < line_len; ++i) {
    if (line[i] == '*') {
      star = &line[i];
      break;
    }
  }

  size_t payload_len = line_len;
  if (star != NULL) {
    out->has_crc = true;
    payload_len = (size_t)(star - line);
    const size_t crc_index = payload_len + 1u;
    if (crc_index + 2u > line_len) {
      return false;
    }
    if (line_len != (payload_len + 3u)) {
      return false;
    }
    if (!uiLinkParseHexByte(&line[crc_index], &out->crc_expected)) {
      return false;
    }
  }

  if (payload_len == 0u || payload_len > UILINK_V2_MAX_LINE) {
    return false;
  }

  char payload[UILINK_V2_MAX_LINE + 1u];
  memcpy(payload, line, payload_len);
  payload[payload_len] = '\0';

  out->crc_computed = uiLinkCrc8((const uint8_t*)payload, payload_len);
  out->crc_ok = (!out->has_crc) || (out->crc_expected == out->crc_computed);
  if (out->has_crc && !out->crc_ok) {
    return false;
  }

  char* cursor = payload;
  char* comma = strchr(cursor, ',');
  if (comma == NULL) {
    if (!uiLinkCopyBounded(out->type_token, sizeof(out->type_token), cursor, strlen(cursor))) {
      return false;
    }
    out->type = uiLinkMsgTypeFromToken(out->type_token);
    return out->type != UILINK_MSG_UNKNOWN;
  }

  const size_t type_len = (size_t)(comma - cursor);
  if (!uiLinkCopyBounded(out->type_token, sizeof(out->type_token), cursor, type_len)) {
    return false;
  }
  out->type = uiLinkMsgTypeFromToken(out->type_token);
  if (out->type == UILINK_MSG_UNKNOWN) {
    return false;
  }

  cursor = comma + 1;
  while (*cursor != '\0') {
    if (out->field_count >= UILINK_V2_MAX_FIELDS) {
      return false;
    }

    char* next = strchr(cursor, ',');
    size_t token_len = 0u;
    if (next == NULL) {
      token_len = strlen(cursor);
    } else {
      token_len = (size_t)(next - cursor);
    }

    char token[UILINK_V2_KEY_MAX + UILINK_V2_VALUE_MAX + 2u];
    if (!uiLinkCopyBounded(token, sizeof(token), cursor, token_len)) {
      return false;
    }

    char* eq = strchr(token, '=');
    if (eq == NULL || eq == token || *(eq + 1) == '\0') {
      return false;
    }
    *eq = '\0';

    UiLinkField* field = &out->fields[out->field_count];
    if (!uiLinkCopyBounded(field->key, sizeof(field->key), token, strlen(token))) {
      return false;
    }
    if (!uiLinkCopyBounded(field->value, sizeof(field->value), eq + 1, strlen(eq + 1))) {
      return false;
    }

    ++out->field_count;
    if (next == NULL) {
      break;
    }
    cursor = next + 1;
  }

  return true;
}

static inline size_t uiLinkBuildLine(char* out,
                                     size_t out_size,
                                     const char* type,
                                     const UiLinkField* fields,
                                     uint8_t field_count) {
  if (out == NULL || out_size < 8u || type == NULL || type[0] == '\0') {
    return 0u;
  }

  size_t len = 0u;
  const int type_len = snprintf(out, out_size, "%s", type);
  if (type_len <= 0) {
    return 0u;
  }
  len = (size_t)type_len;

  for (uint8_t i = 0u; i < field_count; ++i) {
    if (fields == NULL || fields[i].key[0] == '\0') {
      return 0u;
    }
    const int w = snprintf(out + len,
                           out_size - len,
                           ",%s=%s",
                           fields[i].key,
                           fields[i].value);
    if (w <= 0 || (size_t)w >= (out_size - len)) {
      return 0u;
    }
    len += (size_t)w;
  }

  const uint8_t crc = uiLinkCrc8((const uint8_t*)out, len);
  const int tail = snprintf(out + len, out_size - len, "*%02X\n", (unsigned int)crc);
  if (tail <= 0 || (size_t)tail >= (out_size - len)) {
    return 0u;
  }
  len += (size_t)tail;
  return len;
}

#ifdef __cplusplus
}
#endif
