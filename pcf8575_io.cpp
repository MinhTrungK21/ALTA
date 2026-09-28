#include "hardware/pcf8575_io.h"

#include <Wire.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

namespace
{
    TwoWire s_wire1(1);                     // tao bus I2C 1
    TwoWire *s_wire = nullptr;              // con trỏ đến bus I2C đang su dung
    bool s_ready = false;                   // danh dau PCF857 da san sang
    uint8_t s_address = 0x20;               // dia chi I2C mac dinh cua PCF8575
    uint16_t s_state = 0xFFFF;              // trang thai doc duoc tu IC
    uint16_t s_latchedState = 0xFFFF;       // trang thai ghi vao IC, ban dau tat ca chan o muc HIGH
    SemaphoreHandle_t s_busMutex = nullptr; // mutex de bao ve truy cap bus I2C tu nhieu task

    constexpr uint32_t kPcfI2cClockHz = 100000; // toc do dong ho I2C mac dinh cua PCF8575 la 100kHz
    constexpr uint16_t kPcfI2cTimeoutMs = 50;   // thoi gian timeout mac dinh cua I2C la 50ms

    // So luong chan tren IC PCF8575
    bool ensureBusMutex()
    {
        if (s_busMutex != nullptr)
        {
            return true;
        }

        s_busMutex = xSemaphoreCreateMutex();
        return s_busMutex != nullptr;
    }

    // Khoa mutex de truy cap bus I2C, tra ve true neu thanh cong, false neu timeout
    bool lockBus()
    {
        return ensureBusMutex() && xSemaphoreTake(s_busMutex, pdMS_TO_TICKS(50)) == pdTRUE;
    }

    // Giai phong mutex de cho phep cac task khac truy cap bus I2C
    void unlockBus()
    {
        if (s_busMutex != nullptr)
        {
            xSemaphoreGive(s_busMutex); // nha mutex de cho phep cac task khac truy cap bus I2C
        }
    }

    // Ghi gia tri 16-bit vao IC PCF8575 qua bus I2C
    bool writePort(TwoWire &wire, uint16_t value)
    {
        wire.beginTransmission(s_address);
        wire.write(static_cast<uint8_t>(value & 0xFF));        // ghi byte thap
        wire.write(static_cast<uint8_t>((value >> 8) & 0xFF)); // ghi byte cao
        return wire.endTransmission() == 0;                    // tra ve true neu ghi thanh cong, false neu that bai
    }

    // Kiem tra xem IC PCF8575 co phan hoi tren bus I2C hay khong
    bool probeAddress(TwoWire &wire)
    {
        wire.beginTransmission(s_address);
        return wire.endTransmission() == 0;
    }

    // Doc trang thai tu bus I2C
    bool refreshFromWire(TwoWire &wire)
    {
        if (wire.requestFrom(static_cast<int>(s_address), 2) != 2)
        {
            return false;
        }

        const uint8_t lowByte = wire.read();
        const uint8_t highByte = wire.read();
        s_state = static_cast<uint16_t>(lowByte) |
                  (static_cast<uint16_t>(highByte) << 8);
        return true;
    }

    // Khoi tao IC PCF8575 tren bus I2C, tra ve true neu thanh cong, false neu that bai
    bool beginOnWire(TwoWire &wire, uint8_t sda, uint8_t scl)
    {
        wire.begin(sda, scl);
        wire.setClock(kPcfI2cClockHz);     // set toc do dong ho I2C mac dinh cua PCF8575 la 100kHz
        wire.setTimeOut(kPcfI2cTimeoutMs); // set thoi gian timeout cua I2C la 50ms
        s_latchedState = 0xFFFF;

        if (!probeAddress(wire))
        {
            return false;
        }

        // Giu tat ca chan o muc HIGH de dung nhu input pull-up / LED off active-low.
        if (!writePort(wire, s_latchedState))
        {
            return false;
        }

        // Doc trang thai ban dau tu IC PCF8575
        if (!refreshFromWire(wire))
        {
            return false;
        }

        s_wire = &wire; // luu con tro den bus I2C dang su dung de truy cap IC PCF8575
        return true;
    }
} // namespace

namespace pcf8575_io
{

    bool begin(uint8_t sda, uint8_t scl, uint8_t address)
    {
        if (!lockBus())
        {
            s_ready = false;
            return false;
        }

        s_address = address;
        s_wire = nullptr;
        s_ready = beginOnWire(s_wire1, sda, scl);
        unlockBus();
        return s_ready;
    }

    bool isReady()
    {
        return s_ready;
    }

    bool refresh()
    {
        if (!lockBus())
        {
            s_ready = false;
            return false;
        }

        const bool ok = s_wire && refreshFromWire(*s_wire);
        s_ready = ok;
        unlockBus();
        return ok;
    }

    bool writePin(uint8_t pin, bool level)
    {
        if (pin >= kPinCount)
        {
            return false;
        }

        const uint16_t mask = static_cast<uint16_t>(1U) << pin;
        if (level)
        {
            s_latchedState |= mask;
        }
        else
        {
            s_latchedState &= static_cast<uint16_t>(~mask);
        }

        if (!lockBus())
        {
            s_ready = false;
            return false;
        }

        const bool writeOk = s_wire && writePort(*s_wire, s_latchedState);
        unlockBus();
        if (!writeOk)
        {
            s_ready = false;
            return false;
        }

        if (level)
        {
            s_state |= mask;
        }
        else
        {
            s_state &= static_cast<uint16_t>(~mask);
        }

        s_ready = true;
        return true;
    }

    bool readPin(uint8_t pin)
    {
        if (!s_ready || pin >= kPinCount)
        {
            return true;
        }

        return (s_state & (static_cast<uint16_t>(1U) << pin)) != 0;
    }

    uint16_t readPort()
    {
        return s_state;
    }

} // namespace pcf8575_io
