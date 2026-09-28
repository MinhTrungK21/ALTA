#include "hardware/jc4827w543_lcd.h"

#include <esp_heap_caps.h>
#include <PINS_JC4827W543.h>
#include <Wire.h>

namespace
{

  TAMC_GT911 touchController(TOUCH_SDA, TOUCH_SCL, TOUCH_INT, TOUCH_RST, TOUCH_WIDTH, TOUCH_HEIGHT);
  bool s_initialized = false;
  bool s_touchAvailable = false;
  uint8_t s_touchAddress = GT911_ADDR1;

  constexpr uint32_t kTouchI2cClockHz = 400000;
  constexpr uint16_t kTouchI2cTimeoutMs = 50;
  constexpr uint8_t kGt911MaxTouches = 5;

  bool writeTouchByte(uint16_t reg, uint8_t value)
  {
    Wire.beginTransmission(s_touchAddress);
    Wire.write(highByte(reg));
    Wire.write(lowByte(reg));
    Wire.write(value);
    return Wire.endTransmission() == 0;
  }

  bool readTouchBlock(uint16_t reg, uint8_t *buffer, uint8_t size)
  {
    if (buffer == nullptr || size == 0 || size > 8)
    {
      return false;
    }

    Wire.beginTransmission(s_touchAddress);
    Wire.write(highByte(reg));
    Wire.write(lowByte(reg));
    if (Wire.endTransmission(false) != 0)
    {
      return false;
    }

    if (Wire.requestFrom(static_cast<int>(s_touchAddress), static_cast<int>(size)) != size)
    {
      return false;
    }

    for (uint8_t i = 0; i < size; ++i)
    {
      buffer[i] = Wire.read();
    }
    return true;
  }

  bool readTouchByte(uint16_t reg, uint8_t &value)
  {
    return readTouchBlock(reg, &value, 1);
  }

  void resetTouchController(uint8_t address)
  {
    pinMode(TOUCH_INT, OUTPUT);
    pinMode(TOUCH_RST, OUTPUT);
    digitalWrite(TOUCH_INT, LOW);
    digitalWrite(TOUCH_RST, LOW);
    delay(10);
    digitalWrite(TOUCH_INT, address == GT911_ADDR2 ? HIGH : LOW);
    delay(1);
    digitalWrite(TOUCH_RST, HIGH);
    delay(5);
    digitalWrite(TOUCH_INT, LOW);
    delay(50);
    pinMode(TOUCH_INT, INPUT);
    delay(50);
  }

  void logHeapIntegrity(const char *stage)
  {
    constexpr uint32_t internal8BitCaps = MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT;
    Serial.printf("[HEAP][LCD] %s integrity=%s\n",
                  stage,
                  heap_caps_check_integrity(internal8BitCaps, false) ? "ok" : "bad");
  }

  bool probeTouchAddress(uint8_t address)
  {
    Wire.beginTransmission(address);
    return Wire.endTransmission() == 0;
  }

  bool beginTouchController(uint8_t address)
  {
    s_touchAddress = address;
    resetTouchController(address);

    if (probeTouchAddress(address))
    {
      Serial.printf("[TOUCH] GT911 ready at 0x%02X\n", address);
      return true;
    }

    Serial.printf("[TOUCH] GT911 not responding at 0x%02X\n", address);
    return false;
  }

} // namespace

namespace jc4827w543
{

  Arduino_GFX *gfx()
  {
    return ::gfx;
  }

  TAMC_GT911 &touch()
  {
    return touchController;
  }

  bool touchAvailable()
  {
    return s_touchAvailable;
  }

  bool readTouchPoint(uint16_t &x, uint16_t &y)
  {
    if (!s_touchAvailable)
    {
      return false;
    }

    uint8_t pointInfo = 0;
    if (!readTouchByte(GT911_POINT_INFO, pointInfo))
    {
      s_touchAvailable = false;
      return false;
    }

    const bool bufferReady = (pointInfo & 0x80) != 0;
    const uint8_t touchCount = pointInfo & 0x0F;
    if (!bufferReady || touchCount == 0 || touchCount > kGt911MaxTouches)
    {
      if (bufferReady)
      {
        writeTouchByte(GT911_POINT_INFO, 0);
      }
      return false;
    }

    uint8_t pointData[7] = {0};
    if (!readTouchBlock(GT911_POINT_1, pointData, sizeof(pointData)))
    {
      s_touchAvailable = false;
      return false;
    }

    x = static_cast<uint16_t>(pointData[1]) | (static_cast<uint16_t>(pointData[2]) << 8);
    y = static_cast<uint16_t>(pointData[3]) | (static_cast<uint16_t>(pointData[4]) << 8);
    writeTouchByte(GT911_POINT_INFO, 0);
    return x < TOUCH_WIDTH && y < TOUCH_HEIGHT;
  }

  void setBacklight(bool enabled)
  {
    pinMode(GFX_BL, OUTPUT);
    digitalWrite(GFX_BL, enabled ? HIGH : LOW);
  }

  bool begin()
  {
    if (s_initialized)
    {
      return true;
    }

    // The JC4827W543 NV3041A panel is connected through a 4-data-line
    // QSPI bus.  Pass the board-specific clock explicitly; Arduino_GFX's
    // parameterless begin() would otherwise use the library default.
    if (!::gfx->begin(GFX_SPEED))
    {
      return false;
    }
    logHeapIntegrity("gfx-begin");

    setBacklight(true);
    ::gfx->fillScreen(RGB565_BLACK);
    logHeapIntegrity("fill-screen");

    Wire.begin(TOUCH_SDA, TOUCH_SCL);
    Wire.setClock(kTouchI2cClockHz);
    Wire.setTimeOut(kTouchI2cTimeoutMs);
    s_touchAvailable = beginTouchController(GT911_ADDR1);
    if (!s_touchAvailable)
    {
      s_touchAvailable = beginTouchController(GT911_ADDR2);
    }
    logHeapIntegrity("touch-init");

    s_initialized = true;
    return true;
  }

} // namespace jc4827w543
