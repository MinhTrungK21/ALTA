// Arduino only compiles source files in the sketch root (and its src folder).
// Keep the board implementation organized under hardware/, while exposing it
// as a sketch translation unit for the Arduino build system.
#include "hardware/jc4827w543_lcd.cpp"
