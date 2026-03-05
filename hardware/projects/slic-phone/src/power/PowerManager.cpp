#include "power/PowerManager.h"

#include <driver/gpio.h>
#include <esp_sleep.h>

PowerManager::PowerManager() = default;

void PowerManager::monitorBattery(uint8_t pin) {
    const float voltage = getBatteryVoltage(pin);
    Serial.printf("[PowerManager] battery=%.2fV\n", voltage);
}

void PowerManager::enterDeepSleep(uint32_t ms) {
    esp_sleep_enable_timer_wakeup(static_cast<uint64_t>(ms) * 1000ULL);
    esp_deep_sleep_start();
}

void PowerManager::wakeupOnPin(uint8_t pin) {
    esp_sleep_enable_ext0_wakeup(static_cast<gpio_num_t>(pin), 0);
}

float PowerManager::getBatteryVoltage(uint8_t pin) {
    const int raw = analogRead(pin);
    return static_cast<float>(raw) * (3.3f / 4095.0f) * 2.0f;
}
