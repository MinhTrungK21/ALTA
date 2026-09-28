#pragma once

#include <Arduino.h>

namespace pcf8575_io
{

    constexpr uint8_t kPinCount = 16;

    bool begin(uint8_t sda, uint8_t scl, uint8_t address);
    bool isReady();
    bool refresh();
    bool writePin(uint8_t pin, bool level);
    bool readPin(uint8_t pin);
    uint16_t readPort();

} // namespace pcf8575_io
