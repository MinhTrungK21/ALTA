#ifndef KEY_BUTTONS_H
#define KEY_BUTTONS_H

#include <Arduino.h>

// PCF8575 dung chung I2C voi touch neu phan cung khong doi sang bus rieng.
#define KEY_PCF8575_SDA         18
#define KEY_PCF8575_SCL         17
#define KEY_PCF8575_I2C_ADDRESS 0x20

// Map phim KEY_* vao chan P0..P15 cua PCF8575.
// Quy uoc: nut tha = HIGH, nhan = LOW.
#define PIN_KEY_EMERGENCY 5
#define PIN_KEY_OK        7
#define PIN_KEY_ESC       0
#define PIN_KEY_FUNC      2

#define PIN_LED_EMERGENCY 12
#define PIN_LED_OK 14
#define PIN_LED_ESC 9
#define PIN_LED_FUNC 11

enum KeyId
{
    KEY_EMERGENCY = 0,
    KEY_OK,
    KEY_ESC,
    KEY_FUNC,
    KEY_COUNT
};

void keyInit();
void keyPoll();
// Xu ly hanh dong tu phim vat ly sau moi lan keyPoll().
void handleKeyButtons();

void clearAllKeyLeds();
// Tạm chưa dùng.
// bool keyConfigured(KeyId id);
// int keyPin(KeyId id);
// bool keyDown(KeyId id);
bool keyPressed(KeyId id);
#endif // KEY_BUTTONS_H
