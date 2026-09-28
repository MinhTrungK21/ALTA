#ifndef LED_STATUS_H
#define LED_STATUS_H

#include <Arduino.h>
#include <config.h> // de truy cap globalLicense.expired_flag

/* ───── Trang thai LED ────────────────────────────────────────── */
enum LedState : uint8_t
{
  NORMAL_STATUS,    // 50 ms ON / 950 ms OFF – nha nhoang 1 Hz
  CONNECTION_ERROR, // 500 ms ON / 500 ms OFF – loi mang
  BLINK_CONFIRM,    // 100 ms ON / 100 ms OFF – nhay N lan roi ve NORMAL
  FLASH_TWICE,      // 50 ms ON / 200 ms OFF ×2
  LICENSE_EXPIRED   // LED tat han
};

class LedStatus
{
public: /* <<< dung duoc ngoai file */
  explicit LedStatus(uint8_t pin, bool activeHigh = true)
      : pin_(pin), activeHigh_(activeHigh)
  {
    pinMode(pin_, OUTPUT);
    writeLed(false);
    setState(NORMAL_STATUS);
  }

  /** Dat trang thai; BLINK_CONFIRM truyen so nhay (default 3) */
  void setState(LedState s, uint8_t blinks = 3)
  {

    // Neu yeu cau trung trang thai hien tai thi khong lam gi
    if (s == state_ && s != BLINK_CONFIRM)
      return;

    if (state_ == FLASH_TWICE && busy_)
      return; // khong cat FLASH_TWICE

    if (s == BLINK_CONFIRM)
    {
      blinkEdges_ = 0;
      maxBlinkEdges_ = constrain(blinks, 1, 10) * 2; // moi nhay 2 canh
    }
    if (s == FLASH_TWICE)
    {
      prevState_ = state_;
      flashEdges_ = 0;
      busy_ = true;
    }

    state_ = s;
    // ledOn_ = false;
    lastTick_ = millis();
    // writeLed(true); // bat ngay
  }

  /** Goi lien tuc trong loop() */
  void update()
  {
    unsigned long now = millis();
    if (identifyActive_)
    {
      if (static_cast<int32_t>(now - identifyUntil_) >= 0)
      {
        endIdentify(); // het gio: quay lai che do binh thuong
      }
      else
      {
        if (now - lastTick_ >= IDENTIFY_HALF_PERIOD_MS)
        {
          ledOn_ = !ledOn_;
          writeLed(ledOn_);
          lastTick_ = now;
        }
        return;
      }
    }
    uint16_t onT, offT;
    getTiming(state_, onT, offT);

    /* Trang thai giu nguyen (ON hoac OFF) */
    if (onT == 0 && offT == 0)
    {
      writeLed(true);
      return;
    }
    if (offT == 0)
    {
      writeLed(false);
      return;
    }

    uint16_t interval = ledOn_ ? onT : offT;
    if (now - lastTick_ < interval)
      return;

    /* Toggle LED */
    ledOn_ = !ledOn_;
    writeLed(ledOn_);
    lastTick_ = now;

    /* Dem canh cho cac che do dac biet */
    if (state_ == BLINK_CONFIRM && ++blinkEdges_ >= maxBlinkEdges_)
    {
      state_ = NORMAL_STATUS; // quay ve binh thuong
      return;
    }
    if (state_ == FLASH_TWICE && ++flashEdges_ >= 4)
    { // 2 lan bat
      busy_ = false;
      state_ = prevState_;
    }
  }

  bool isBusy() const { return busy_; } // TRUE khi dang FLASH_TWICE

  /** Dang o che do nhan dien board (xem setIdentify)? LedDisplay dung 2 ham
   *  nay de dieu khien cac chan 1-12 cung nhip voi LED 46. */
  bool isIdentifying() const { return identifyActive_; }
  /** Muc hien tai cua LED 46 trong che do nhan dien (true = sang / HIGH). */
  bool identifyLevel() const { return ledOn_; }

  /** Nhap nhay nhanh (100 ms ON / 100 ms OFF) de nhan dien board, de len tren
   *  moi trang thai khac (ke ca FLASH_TWICE) cho toi khi het `seconds` giay
   *  hoac co lenh tat. Tu het han de app treo/mat ket noi khong de LED nhap
   *  nhay mai. Khong dung vao state_ nen tat xong LED quay lai dung che do cu. */
  void setIdentify(bool on, uint16_t seconds = 300)
  {
    if (on)
    {
      identifyUntil_ = millis() + static_cast<uint32_t>(constrain(seconds, 1, 900)) * 1000UL;
      if (!identifyActive_)
      {
        identifyActive_ = true;
        ledOn_ = false;
        lastTick_ = 0; // bat dau chu ky ngay
      }
    }
    else if (identifyActive_)
    {
      endIdentify();
    }
  }

private:
  const uint8_t pin_;
  const bool activeHigh_;

  LedState state_ = NORMAL_STATUS;
  LedState prevState_ = NORMAL_STATUS;

  unsigned long lastTick_ = 0;
  bool ledOn_ = false;
  bool busy_ = false;

  /* Che do nhan dien board (xem setIdentify) */
  static constexpr uint16_t IDENTIFY_HALF_PERIOD_MS = 100;
  volatile bool identifyActive_ = false;
  volatile uint32_t identifyUntil_ = 0;

  void endIdentify()
  {
    identifyActive_ = false;
    ledOn_ = false;
    writeLed(false);
    lastTick_ = millis();
  }

  /* Dem canh */
  uint8_t blinkEdges_ = 0;
  uint8_t maxBlinkEdges_ = 6; // default 3 nhay
  uint8_t flashEdges_ = 0;

  /* Bang thoi gian ON/OFF cho tung trang thai */
  static void getTiming(LedState st, uint16_t &onT, uint16_t &offT)
  {
    switch (st)
    {
    case NORMAL_STATUS:
    {
      onT = 50;
      offT = 950;
      break;
    }
    case CONNECTION_ERROR:
    {
      onT = 200;
      offT = 50;
      break;
    }
    case BLINK_CONFIRM:
    {
      onT = 100;
      offT = 100;
      break;
    }
    case FLASH_TWICE:
    {
      onT = 50;
      offT = 200;
      break;
    }
    case LICENSE_EXPIRED:
    {
      onT = 500;
      offT = 500;
      break;
    }
    } // ← dong switch
  }

  inline void writeLed(bool on)
  {
    digitalWrite(pin_, on);
    // ledOn_ = on;
  }
};

#endif /* LED_STATUS_H */

// #ifndef LED_STATUS_H
// #define LED_STATUS_H

// #include <Arduino.h>

// /**
//  * @brief LED status helper with optional active‑LOW wiring.
//  *
//  *  • NORMAL_STATUS     – LED sang lien tuc (thiet bi OK / license con han)
//  *  • CONNECTION_ERROR  – LED tat lien tuc (loi mang hoac thiet bi)
//  *  • BLINK_CONFIRM     – LED nhap nhay N lan de xac nhan cau hinh
//  *  • LICENSE_EXPIRED   – LED tat lien tuc khi license het han
//  */

// enum LedState {
//   NORMAL_STATUS,
//   CONNECTION_ERROR,
//   BLINK_CONFIRM,
//   LICENSE_EXPIRED
// };

// class LedStatus {
// private:
//   int   pin;                       // GPIO chan LED
//   bool  activeHigh;                // true  = HIGH -> LED ON
//                                     // false = LOW  -> LED ON (active‑LOW)
//   LedState state;                  // trang thai hien tai
//   unsigned long lastBlinkTime;     // moc thoi gian lan nhap nhay cuoi
//   bool  ledLevel;                  // muc logic hien tai tren chan LED (true = ON)
//   int   blinkCount;                // so lan da nhap nhay (LOW edge)
//   int   maxBlinkCount;             // tong so lan can nhap nhay (LOW edge)
//   static constexpr uint16_t BLINK_INTERVAL = 200; // ms giua cac lan toggle

//   /**
//    * @brief Ghi muc logic ra chan LED, tu dong dao theo activeHigh
//    */
//   inline void write(bool on) {
//     digitalWrite(pin, (on ^ !activeHigh) ? HIGH : LOW);
//     ledLevel = on;
//   }

// public:
//   /**
//    * @param ledPin      GPIO dung lam LED
//    * @param activeHigh  true  → HIGH = LED ON (mac dinh)
//    *                    false → LOW  = LED ON (noi LED kieu dao cuc)
//    * @param maxBlinks   so lan nhap nhay mac dinh cho BLINK_CONFIRM
//    */
//   LedStatus(int ledPin, bool activeHigh = true, int maxBlinks = 3)
//       : pin(ledPin), activeHigh(activeHigh), state(CONNECTION_ERROR),
//         lastBlinkTime(0), ledLevel(false), blinkCount(0),
//         maxBlinkCount(maxBlinks) {
//     pinMode(pin, OUTPUT);
//     write(false);                   // LED OFF mac dinh
//   }

//   /**
//    * @brief Dat trang thai LED
//    * @param newState  Trang thai moi
//    * @param blinks    So lan nhap nhay (chi ap dung cho BLINK_CONFIRM)
//    */
//   void setState(LedState newState, int blinks = 3) {
//     state = newState;
//     blinkCount = 0;
//     lastBlinkTime = millis();

//     switch (state) {
//       case NORMAL_STATUS:
//         write(true);
//         break;

//       case CONNECTION_ERROR:
//       case LICENSE_EXPIRED:
//         write(false);
//         break;

//       case BLINK_CONFIRM:
//         maxBlinkCount = blinks > 0 ? blinks : 3;
//         write(true);                // bat LED lan dau
//         break;
//     }
//   }

//   /**
//    * @brief Goi thuong xuyen trong loop() de dieu khien nhap nhay
//    */
//   void update() {
//     if (state == BLINK_CONFIRM) {
//       unsigned long now = millis();
//       if (now - lastBlinkTime >= BLINK_INTERVAL) {
//         write(!ledLevel);           // dao trang thai LED
//         lastBlinkTime = now;

//         if (!ledLevel) {            // chi dem canh roi (LED OFF)
//           ++blinkCount;
//           if (blinkCount >= maxBlinkCount) {
//             setState(NORMAL_STATUS); // tro ve sang lien tuc
//           }
//         }
//       }
//     }
//   }

//   /**
//    * @return true neu LED dang trong chuoi nhap nhay BLINK_CONFIRM
//    */
//   bool isBusy() const { return state == BLINK_CONFIRM; }
// };

// #endif // LED_STATUS_H
