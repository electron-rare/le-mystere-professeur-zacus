#pragma once

// Board: Ai-Thinker ESP32 Audio Kit v2.2 + ESP32-A1S (ES8388)
// Memory: N4R8 => 4MB Flash, 8MB PSRAM
// DIP: 1 OFF, 2 ON, 3 ON, 4 OFF, 5 OFF
// Result: SD(SPI) active, KEY2 unavailable, JTAG disconnected.

#include <driver/i2c.h>
#include <driver/i2s.h>

// ===== ES8388 control (I2C) =====
#define A1S_I2C_PORT I2C_NUM_0
#define A1S_I2C_SCL 32
#define A1S_I2C_SDA 33
#define A1S_ES8388_I2C_ADDR 0x10  // 7-bit address

// ===== Audio data (I2S) =====
#define A1S_I2S_PORT I2S_NUM_0
#define A1S_I2S_MCLK 0
#define A1S_I2S_BCLK 27
#define A1S_I2S_LRCK 25
#define A1S_I2S_DOUT 26
#define A1S_I2S_DIN 35  // input-only pin

// ===== Speaker amp + headphone detect =====
#define A1S_PA_ENABLE 21
#define A1S_HP_DETECT 39  // input-only; typical active LOW

// ===== SLIC / telephony front-end wiring (project-specific on A252) =====
#define A1S_SLIC_RM 18
#define A1S_SLIC_FR 5
#define A1S_SLIC_SHK 23
#define A1S_SLIC_PD 19

// ===== SD card (SPI) =====
#define A1S_SD_CS 13
#define A1S_SD_MISO 2
#define A1S_SD_MOSI 15
#define A1S_SD_SCK 14

// ===== Keys =====
#define A1S_KEY1 36
#define A1S_KEY2 13  // NOT AVAILABLE with DIP1=OFF when SD CS is active (DIP2=ON)
#define A1S_KEY3 19
#define A1S_KEY4 23
#define A1S_KEY5 18
#define A1S_KEY6 5
