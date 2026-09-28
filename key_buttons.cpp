#include "key_buttons.h"

#include "hardware/pcf8575_io.h"

namespace
{
    constexpr unsigned long kKeyDebounceMs = 30UL;
    constexpr unsigned long kKeyReconnectIntervalMs = 1000UL;
    // Theo so do phim co den: LED anode di qua R8, cathode xuong GND nen output HIGH se bat LED.
    // constexpr bool kLedOnLevel = HIGH;
    // constexpr bool kLedOffLevel = LOW;
    static constexpr bool LED_ACTIVE_LOW = false; // false: HIGH = LED on

    static uint8_t ledLevel(bool on)
    {
        if (LED_ACTIVE_LOW)
        {
            return on ? LOW : HIGH;
        }

        return on ? HIGH : LOW;
    }

    struct KeyState
    {
        int pin;
        bool stable;
        bool raw;
        bool pressed;
        unsigned long changedAt;
    };

    unsigned long g_lastReconnectAttempt = 0;
    bool g_pcfInitLogged = false;
    // Khi PCF8575 mat ket noi va khoi phuc lai, can dong bo LED theo trang thai phim hien tai.
    bool g_ledResyncPending = false;

    KeyState g_keys[KEY_COUNT] = {
        {PIN_KEY_EMERGENCY, HIGH, HIGH, false, 0},
        {PIN_KEY_OK, HIGH, HIGH, false, 0},
        {PIN_KEY_ESC, HIGH, HIGH, false, 0},
        {PIN_KEY_FUNC, HIGH, HIGH, false, 0},
    };

    bool isValidKeyId(KeyId id)
    {
        return id >= KEY_EMERGENCY && id < KEY_COUNT;
    }

    const char *keyName(KeyId id)
    {
        switch (id)
        {
        case KEY_EMERGENCY:
            return "KEY_EMERGENCY";
        case KEY_OK:
            return "KEY_OK";
        case KEY_ESC:
            return "KEY_ESC";
        case KEY_FUNC:
            return "KEY_FUNC";
        default:
            return "KEY_UNKNOWN";
        }
    }

    // Moi KEY_* di kem 1 LED_* tuong ung tren cung PCF8575.
    int ledPin(KeyId id)
    {
        switch (id)
        {
        case KEY_EMERGENCY:
            return PIN_LED_EMERGENCY;
        case KEY_OK:
            return PIN_LED_OK;
        case KEY_ESC:
            return PIN_LED_ESC;
        case KEY_FUNC:
            return PIN_LED_FUNC;
        default:
            return -1;
        }
    }

    void printKeyEvent(KeyId id, const KeyState &key, const char *event)
    {
        Serial.print("[KEY] ");
        Serial.print(keyName(id));
        Serial.print(" ");
        Serial.print(event);
        Serial.print(" PCF P");
        Serial.println(key.pin);
    }

    void syncKeyLed(KeyId id, bool isPressed)
    {
        if (!pcf8575_io::isReady())
        {
            return;
        }

        const int pin = ledPin(id);
        if (pin < 0)
        {
            return;
        }

        pcf8575_io::writePin(
            static_cast<uint8_t>(pin),
            ledLevel(isPressed));
    }

    // Dong bo lai toan bo LED sau luc boot hoac reconnect expander.
    void syncAllKeyLeds()
    {
        for (size_t i = 0; i < KEY_COUNT; i++)
        {
            syncKeyLed(static_cast<KeyId>(i), g_keys[i].stable == LOW);
        }
    }

    void initKeyState(KeyState &key, unsigned long now)
    {
        const bool rawState = pcf8575_io::readPin(static_cast<uint8_t>(key.pin));
        key.stable = rawState;
        key.raw = rawState;
        key.pressed = false;
        key.changedAt = now;
    }

    bool ensureKeyExpanderReady(unsigned long now)
    {
        if (pcf8575_io::isReady())
        {
            return true;
        }

        if (now - g_lastReconnectAttempt < kKeyReconnectIntervalMs)
        {
            return false;
        }

        g_lastReconnectAttempt = now;
        const bool ready = pcf8575_io::begin(KEY_PCF8575_SDA, KEY_PCF8575_SCL, KEY_PCF8575_I2C_ADDRESS);
        if (!ready)
        {
            if (!g_pcfInitLogged)
            {
                Serial.printf("[KEY] PCF8575 not found at SDA=%u SCL=%u ADDR=0x%02X\n",
                              KEY_PCF8575_SDA,
                              KEY_PCF8575_SCL,
                              KEY_PCF8575_I2C_ADDRESS);
                g_pcfInitLogged = true;
            }
            return false;
        }

        g_pcfInitLogged = false;
        g_ledResyncPending = true;
        return true;
    }
} // namespace

void keyInit()
{
    const unsigned long now = millis();

    if (!ensureKeyExpanderReady(now))
    {
        Serial.println("[KEY] PCF8575 init failed");
        return;
    }

    // PCF8575 doc input can ghi HIGH vao chan input
    pcf8575_io::writePin(PIN_KEY_EMERGENCY, HIGH);
    pcf8575_io::writePin(PIN_KEY_OK, HIGH);
    pcf8575_io::writePin(PIN_KEY_ESC, HIGH);
    pcf8575_io::writePin(PIN_KEY_FUNC, HIGH);

    clearAllKeyLeds();

    for (size_t i = 0; i < KEY_COUNT; i++)
    {
        initKeyState(g_keys[i], now);
    }

    g_ledResyncPending = false;
}

void keyPoll()
{
    const unsigned long now = millis();

    for (size_t i = 0; i < KEY_COUNT; i++)
    {
        g_keys[i].pressed = false;
    }

    if (!ensureKeyExpanderReady(now))
    {
        return;
    }

    if (!pcf8575_io::refresh())
    {
        return;
    }

    if (g_ledResyncPending)
    {
        clearAllKeyLeds();
        g_ledResyncPending = false;
    }

    for (size_t i = 0; i < KEY_COUNT; i++)
    {
        KeyState &key = g_keys[i];

        const bool rawState = pcf8575_io::readPin(static_cast<uint8_t>(key.pin));
        if (rawState != key.raw)
        {
            key.raw = rawState;
            key.changedAt = now;
        }

        if ((now - key.changedAt) < kKeyDebounceMs)
        {
            continue;
        }

        if (key.stable == rawState)
        {
            continue;
        }

        key.stable = rawState;
        syncKeyLed(static_cast<KeyId>(i), rawState == LOW);
        if (rawState == LOW)
        {
            key.pressed = true;
            printKeyEvent(static_cast<KeyId>(i), key, "PRESSED");
        }
    }
}

void handleKeyButtons()
{

}

bool keyPressed(KeyId id)
{
    if (!isValidKeyId(id))
    {
        return false;
    }

    return g_keys[id].pressed;
}

void clearAllKeyLeds()
{
    if (!pcf8575_io::isReady())
    {
        return;
    }

    pcf8575_io::writePin(PIN_LED_EMERGENCY, ledLevel(false));
    pcf8575_io::writePin(PIN_LED_OK, ledLevel(false));
    pcf8575_io::writePin(PIN_LED_ESC, ledLevel(false));
    pcf8575_io::writePin(PIN_LED_FUNC, ledLevel(false));
}
