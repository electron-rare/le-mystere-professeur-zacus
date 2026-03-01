#pragma once

#include <Arduino.h>

#include <cstdint>

#define ZACUS_RL_JOIN2(a, b) a##b
#define ZACUS_RL_JOIN(a, b) ZACUS_RL_JOIN2(a, b)

#define ZACUS_RL_LOG_MS(interval_ms, fmt, ...)                                                          \
  do {                                                                                                  \
    static uint32_t ZACUS_RL_JOIN(_zacus_rl_last_, __LINE__) = 0U;                                     \
    const uint32_t ZACUS_RL_JOIN(_zacus_rl_now_, __LINE__) = millis();                                 \
    if (ZACUS_RL_JOIN(_zacus_rl_last_, __LINE__) == 0U ||                                               \
        (ZACUS_RL_JOIN(_zacus_rl_now_, __LINE__) - ZACUS_RL_JOIN(_zacus_rl_last_, __LINE__)) >=        \
            static_cast<uint32_t>(interval_ms)) {                                                       \
      ZACUS_RL_JOIN(_zacus_rl_last_, __LINE__) = ZACUS_RL_JOIN(_zacus_rl_now_, __LINE__);              \
      Serial.printf(fmt, ##__VA_ARGS__);                                                                \
    }                                                                                                   \
  } while (0)

#define ZACUS_RL_PRINTLN_MS(interval_ms, text)                                                          \
  do {                                                                                                  \
    static uint32_t ZACUS_RL_JOIN(_zacus_rl_last_ln_, __LINE__) = 0U;                                  \
    const uint32_t ZACUS_RL_JOIN(_zacus_rl_now_ln_, __LINE__) = millis();                              \
    if (ZACUS_RL_JOIN(_zacus_rl_last_ln_, __LINE__) == 0U ||                                            \
        (ZACUS_RL_JOIN(_zacus_rl_now_ln_, __LINE__) - ZACUS_RL_JOIN(_zacus_rl_last_ln_, __LINE__)) >=  \
            static_cast<uint32_t>(interval_ms)) {                                                       \
      ZACUS_RL_JOIN(_zacus_rl_last_ln_, __LINE__) = ZACUS_RL_JOIN(_zacus_rl_now_ln_, __LINE__);        \
      Serial.println(text);                                                                             \
    }                                                                                                   \
  } while (0)
