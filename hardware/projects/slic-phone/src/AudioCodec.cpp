#include "AudioCodec.h"
#include <driver/i2s.h>
#include <Wire.h>
#include "config/a1s_board_pins.h"

// Documentation technique :
// ES8388 : Codec I2S + I2C, contrôle volume/mute/routage via registres.
// PCM5102 : Codec I2S, volume/mute via atténuation ou pin externe.
// Routage audio : géré par setRoute, peut impliquer multiplexeur/relais.
// Extensibilité : ajouter un nouveau codec = nouvelle classe dérivée.
// Testabilité : mock des méthodes, logs sur chaque action.
// --- ES8388Codec ---
bool ES8388Codec::init() {
    i2s_config_t i2s_config = {
        .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_TX | I2S_MODE_RX),
        .sample_rate = 16000,
        .bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT,
        .channel_format = I2S_CHANNEL_FMT_RIGHT_LEFT,
        .communication_format = I2S_COMM_FORMAT_STAND_I2S,
        .intr_alloc_flags = 0,
        .dma_buf_count = 8,
        .dma_buf_len = 64,
        .use_apll = false,
        .tx_desc_auto_clear = true,
        .fixed_mclk = 0
    };
    i2s_pin_config_t pin_config = {
        .bck_io_num = A1S_I2S_BCLK,
        .ws_io_num = A1S_I2S_LRCK,
        .data_out_num = A1S_I2S_DOUT,
        .data_in_num = A1S_I2S_DIN
    };
    esp_err_t err = i2s_driver_install(I2S_NUM_0, &i2s_config, 0, NULL);
    if (err != ESP_OK) return false;
    err = i2s_set_pin(I2S_NUM_0, &pin_config);
    if (err != ESP_OK) return false;
    Wire.begin(A1S_I2C_SDA, A1S_I2C_SCL);
    setVolume(80);
    return true;
}

bool ES8388Codec::setVolume(uint8_t volume) {
    Wire.beginTransmission(A1S_ES8388_I2C_ADDR);
    Wire.write(0x2B);
    Wire.write(volume);
    Wire.endTransmission();
    Wire.beginTransmission(A1S_ES8388_I2C_ADDR);
    Wire.write(0x2C);
    Wire.write(volume);
    Wire.endTransmission();
    return true;
}

bool ES8388Codec::mute(bool state) {
    Wire.beginTransmission(A1S_ES8388_I2C_ADDR);
    Wire.write(0x2F);
    Wire.write(state ? 0x01 : 0x00);
    Wire.endTransmission();
    return true;
}

bool ES8388Codec::setRoute(AudioRoute route) {
    Wire.beginTransmission(A1S_ES8388_I2C_ADDR);
    Wire.write(0x30);
    Wire.write(route == ROUTE_BLUETOOTH ? 0x01 : 0x00);
    Wire.endTransmission();
    return true;
}

bool PCM5102Codec::init() {
    i2s_config_t i2s_config = {
        .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_TX),
        .sample_rate = 16000,
        .bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT,
        .channel_format = I2S_CHANNEL_FMT_RIGHT_LEFT,
        .communication_format = I2S_COMM_FORMAT_STAND_I2S,
        .intr_alloc_flags = 0,
        .dma_buf_count = 8,
        .dma_buf_len = 64,
        .use_apll = false,
        .tx_desc_auto_clear = true,
        .fixed_mclk = 0
    };
    i2s_pin_config_t pin_config = {
        .bck_io_num = A1S_I2S_BCLK,
        .ws_io_num = A1S_I2S_LRCK,
        .data_out_num = A1S_I2S_DOUT,
        .data_in_num = A1S_I2S_DIN
    };
    esp_err_t err = i2s_driver_install(I2S_NUM_0, &i2s_config, 0, NULL);
    if (err != ESP_OK) return false;
    err = i2s_set_pin(I2S_NUM_0, &pin_config);
    if (err != ESP_OK) return false;
    setVolume(80);
    return true;
}

bool PCM5102Codec::setVolume(uint8_t volume) {
    return true;
}

bool PCM5102Codec::mute(bool state) {
    return true;
}

bool PCM5102Codec::setRoute(AudioRoute route) {
    return true;
}
