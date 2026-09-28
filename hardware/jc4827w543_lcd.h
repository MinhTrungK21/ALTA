#pragma once

#include <Arduino.h>
#include <Arduino_GFX_Library.h>
#include "TAMC_GT911.h"

#define TOUCH_SDA 8
#define TOUCH_SCL 4
#define TOUCH_INT 3
#define TOUCH_RST 38
#define TOUCH_WIDTH 480
#define TOUCH_HEIGHT 272
#define PIN_ON 22
#define PIN_OFF 21

namespace jc4827w543 {

Arduino_GFX *gfx();
TAMC_GT911 &touch();

bool begin();
bool touchAvailable();
bool readTouchPoint(uint16_t &x, uint16_t &y);
void setBacklight(bool enabled);

inline uint16_t width()
{
  return TOUCH_WIDTH;
}

inline uint16_t height()
{
  return TOUCH_HEIGHT;
}

}  // namespace jc4827w543
