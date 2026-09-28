
#ifndef HUB66S_LED_DISPLAY_H
#define HUB66S_LED_DISPLAY_H

#include "config.h" // de truy cap globalLicense.expired_flag
#include <Arduino.h>
// #include "serial.h"

namespace Hub66s
{
    class LedDisplay
    {
    public:
        /**
         * @brief Khoi tao phan cung dieu khien man hinh LED.
         *        Tat ca cac chan tu 1 den 12 duoc cau hinh OUTPUT va o muc LOW.
         */
        void begin()
        {
            Serial.println(F("🖥️ Khoi tao dieu khien man hinh LED..."));

            // Thiet lap cac chan LED va tat tat ca
            for (uint8_t i = 1; i < 13; ++i)
            {
                pinMode(i, OUTPUT);
                digitalWrite(i, LOW);
            }

            // pinMode(screenPin_, OUTPUT);   // chan dieu khien chinh
            // digitalWrite(screenPin_, LOW); // man hinh ban dau tat

            lastFlash_ = millis();
            lastRandom_ = millis();
        }

        /**
         * @brief Goi dinh ky trong loop().
         *        Neu license con han → nhay cac chan flashPins_[].
         *        Neu het han          → hien thi hieu ung randomRGB.
         */
        void update()
        {
            if (!globalLicense.expired_flag && globalLicense.remain >0)
            {
                for (uint8_t i = 1; i < 13; ++i)
                {
                    pinMode(i, OUTPUT);
                    digitalWrite(i, LOW);
                }
            }
            else
            {
                randomRGB();
            }
        }

    private:
        // const uint8_t screenPin_ = 13;                           // Co the thay bang chan dieu khien man hinh that
        const uint8_t flashPins_[4] = {8, 10, 1, 4};             // Cac chan nhap nhay khi con han
        const uint8_t groupRGB_[8] = {7, 9, 12, 11, 3, 2, 6, 5}; // Nhom RGB ngau nhien khi het han

        unsigned long lastFlash_ = 0;
        bool flashState_ = false;
        unsigned long lastRandom_ = 0;
        unsigned long lastLog_ = 0;

        /**
         * @brief Hieu ung nhap nhay cac chan flashPins_ moi giay.
         */
        void flashPinData()
        {
            unsigned long now = millis();
            if (now - lastFlash_ >= 1000)
            {
                lastFlash_ = now;
                flashState_ = !flashState_;
                for (uint8_t pin : flashPins_)
                {
                    digitalWrite(pin, flashState_ ? HIGH : LOW);
                }
            }
        }

        /**
         * @brief Hien thi hieu ung ngau nhien tren groupRGB_ moi 500 ms.
         */
        void randomRGB()
        {
            unsigned long now = millis();
            if (now - lastRandom_ >= 500)
            {
                lastRandom_ = now;
                for (uint8_t pin : groupRGB_)
                {
                    digitalWrite(pin, random(0, 2));
                }
            }
            if (now - lastLog_ >= 50000)
            {
                lastLog_ = now;
                Serial.println(F("Het han: Nhap nhay RGB"));
            }
        }
    };

} // namespace Hub66s

#endif // HUB66S_LED_DISPLAY_H
