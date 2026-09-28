
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

            initPersonality();
        }

        /**
         * @brief Goi dinh ky trong loop().
         *        Dang nhan dien board → chan 1-12 nhap nhay cung LED 46.
         *        Neu license con han → tat het (LOW) ca 12 chan.
         *        Neu het han → theo expiredLedMode (app chon qua o "Mode
         *        khi hết license" trong hop Set License, xem config.h):
         *          0 = Random 7 kieu gia lap loi - updateExpiredEffect()
         *          1 = Tat het, khong nhap nhay gi
         *          2 = Nhap nhay deu chu ky 2 giay, khong random
         *          3 = Random theo chu ky - da phan thoi gian trong nhu
         *              binh thuong, thinh thoang gap 1 dot loi ngan roi tu
         *              het - xem updateCycleMode()
         */
        void update()
        {
            // Che do nhan dien board (app dang chon board nay): toan bo chan
            // 1-12 nhap nhay HIGH/LOW cung nhip voi LED 46, de len hieu ung
            // license cho toi khi het gio hoac co lenh tat.
            if (led.isIdentifying())
            {
                const uint8_t level = led.identifyLevel() ? HIGH : LOW;
                for (uint8_t i = 1; i < 13; ++i)
                {
                    pinMode(i, OUTPUT);
                    digitalWrite(i, level);
                }
                identifyWasActive_ = true;
                return;
            }
            if (identifyWasActive_)
            {
                // Vua thoat che do nhan dien: dua het ve LOW nhu luc khoi tao
                // ngay lap tuc, thay vi doi toi nhip cap nhat ke tiep cua
                // hieu ung loi/license con han moi don lai cac chan.
                for (uint8_t i = 1; i < 13; ++i)
                {
                    digitalWrite(i, LOW);
                }
                identifyWasActive_ = false;
            }
            // Doc ca 2 truong cung 1 luc, khong bi Communication task (core
            // 0, xu ly Set License/Xoa license) ghi xen giua chung - xem
            // licenseStateMux trong config.h. Thieu buoc nay co the doc
            // trung luc remain da la gia tri moi nhung expired_flag con cu,
            // nham tuong het han va nhay 1 nhip "loi" dung luc Set License.
            bool licenseExpiredNow;
            uint32_t licenseRemainNow;
            portENTER_CRITICAL(&licenseStateMux);
            licenseExpiredNow = globalLicense.expired_flag;
            licenseRemainNow = globalLicense.remain;
            portEXIT_CRITICAL(&licenseStateMux);
            if (!licenseExpiredNow && licenseRemainNow > 0)
            {
                expiredModeActive_ = false;
                cycleInitialized_ = false;
                for (uint8_t i = 1; i < 13; ++i)
                {
                    pinMode(i, OUTPUT);
                    digitalWrite(i, LOW);
                }
            }
            else if (expiredLedMode == 1)
            {
                // Tat het: thu muc tin hieu HIGH thay vi LOW - LOW dang
                // trung voi trang thai "license con han" (binh thuong) o
                // nhanh tren, nen ghi LOW o day khong tao khac biet dien ap
                // nao ca. Neu HIGH van chua dung thi phai lat lai LOW.
                expiredModeActive_ = false; // de Random khoi dong lai sach neu doi mode sau
                cycleInitialized_ = false;
                for (uint8_t i = 1; i < 13; ++i)
                {
                    digitalWrite(i, HIGH);
                }
                logExpiredModeOnce(1);
            }
            else if (expiredLedMode == 2)
            {
                expiredModeActive_ = false;
                cycleInitialized_ = false;
                updateFixedBlink2s();
                logExpiredModeOnce(2);
            }
            else if (expiredLedMode == 3)
            {
                updateCycleMode();
            }
            else
            {
                cycleInitialized_ = false; // de mode 3 khoi dong lai sach neu doi mode sau
                updateExpiredEffect();
            }
        }

    private:
        // const uint8_t screenPin_ = 13;                           // Co the thay bang chan dieu khien man hinh that

        // Man hinh LED co 8 khe cam (JH1..JH8 tren board), di theo 4 cap -
        // moi cap dung chung 1 "port" duoc dieu khien qua 3 trong so 12 chan
        // 1-12, theo so do day thuc te:
        //   Port 1 (khe cap 1) -> chan 7, 8, 9
        //   Port 2 (khe cap 2) -> chan 10, 11, 12
        //   Port 3 (khe cap 3) -> chan 1, 2, 3
        //   Port 4 (khe cap 4) -> chan 4, 5, 6
        static constexpr uint8_t PORT_COUNT = 4;
        static constexpr uint8_t PINS_PER_PORT = 3;
        const uint8_t ports_[PORT_COUNT][PINS_PER_PORT] = {
            {7, 8, 9},
            {10, 11, 12},
            {1, 2, 3},
            {4, 5, 6},
        };

        // Het han license co tinh chat "loi" gia lap - CO Y khong dung mot
        // kieu nhap nhay co dinh, de nguoi dung khong doan duoc day la loi
        // license (chu khong phai loi day/nguon/board that) roi di tim cach
        // qua mat. Moi lan doi kieu (pickNextExpiredMode(), sau moi
        // modeDurationMs_ ngau nhien) se boc lai 1 trong cac kieu ben duoi,
        // tu do trong nhung lan chay khac nhau se khong bao gio thay dung 1
        // "ma loi" quen thuoc.
        enum class ExpiredMode : uint8_t
        {
            PORT_RANDOM,      // Tung port (3 chan cung nhau) tu boc sang/tat doc lap
            ALL_SYNC_RANDOM,  // Ca 12 chan cung sang/tat 1 luot, nhu chap chon nguon
            CHASE,            // Sang lan luot tung port 1 nhu dang quet loi
            PIN_CHAOS,        // Tung chan rieng le nhay lung tung, khong theo port
            SLOW_FLICKER,     // Giong PORT_RANDOM nhung nhip cham hon han
            BLACKOUT_PULSE,   // Da phan thoi gian tat het, thinh thoang loe 1 port
            SYNC_BLINK_1S,    // Ca 4 port cung nhap nhay deu, chu ky 1 giay (500ms sang/500ms tat)
            MODE_COUNT,
        };

        // "Tinh cach" rieng cua BOARD NAY, suy tu MAC luc boot - co dinh
        // suot vong doi (khong doi qua cac lan het han). Ly do: random()
        // cua ESP32 da doc lap giua cac chip (moi board tu boc rieng), nhung
        // vi 100 board deu dung chung 7 kieu + chung khoang toc do, soi ca
        // dan may van co the thay ro "1 cum board dang chay dung y het 1
        // kieu/1 toc do cung luc" - van la 1 dau hieu de nhan ra. Cho moi
        // board 1 toc do rieng (speedScale_) va 1 kieu "ua thich" rieng
        // (favoriteMode_) giup ca dan nhin loang ra tu nhien hon, dong thoi
        // van giu duoc chut ngau nhien that su moi lan doi kieu (xem
        // pickNextExpiredMode()) chu khong bi cung nhac lap lai 1 khuon.
        float speedScale_ = 1.0f;         // 0.6x (nhanh/gap) .. 1.6x (cham/tu tu)
        ExpiredMode favoriteMode_ = ExpiredMode::PORT_RANDOM;

        void initPersonality()
        {
            // Tinh % va / thang tren so 64-bit goc, KHONG ep kieu xuong
            // 32-bit truoc - ep kieu la chat bo, se mat luon nua tren cua
            // MAC truoc khi kip tinh toan gi. Giu nguyen 64-bit thi phep %
            // da tu dung ca 64 bit roi, khong can buoc gap doi/XOR nao ca.
            const uint64_t mac = ESP.getEfuseMac();
            speedScale_ = 0.6f + (mac % 1000) / 1000.0f; // 0.6 .. 1.6
            favoriteMode_ = static_cast<ExpiredMode>((mac / 1000) % static_cast<uint64_t>(ExpiredMode::MODE_COUNT));
        }

        /** Nhan 1 moc thoi gian "chuan" (ms) va co gian theo toc do rieng
         *  cua board nay - dung cho MOI khoang cho trong moi mode, de tinh
         *  cach nhanh/cham cua board the hien xuyen suot, khong chi rieng 1
         *  kieu nao. */
        unsigned long paced(unsigned long baseMs) const
        {
            return static_cast<unsigned long>(baseMs * speedScale_);
        }

        bool identifyWasActive_ = false;
        bool expiredModeActive_ = false;
        ExpiredMode mode_ = ExpiredMode::PORT_RANDOM;
        unsigned long modeStartedAt_ = 0;
        unsigned long modeDurationMs_ = 0;
        unsigned long lastTick_ = 0;
        unsigned long lastLog_ = 0;
        uint8_t chaseIndex_ = 0;
        unsigned long blackoutUntil_ = 0;
        bool blackoutFlashing_ = false;
        bool syncBlinkOn_ = false;

        // Rieng cho expiredLedMode == 2 (nhap nhay deu 2 giay) - tach khoi
        // lastTick_/syncBlinkOn_ cua he thong Random ben tren de 2 kieu
        // khong dam vao trang thai cua nhau khi app doi qua lai giua chung.
        unsigned long fixedBlinkLastTick_ = 0;
        bool fixedBlinkOn_ = false;

        /** expiredLedMode == 2: ca 4 port cung sang 1 giay, tat 1 giay, lap
         *  lai deu dan - KHONG random, khong co gian theo tinh cach board. */
        void updateFixedBlink2s()
        {
            const unsigned long now = millis();
            if (now - fixedBlinkLastTick_ >= 1000)
            {
                fixedBlinkLastTick_ = now;
                fixedBlinkOn_ = !fixedBlinkOn_;
                writeAllPorts(fixedBlinkOn_ ? HIGH : LOW);
            }
        }

        // Rieng cho expiredLedMode == 3 (Random theo chu ky). "Quiet" = dang
        // gia lam nhu binh thuong (LOW, giong nhanh license con han). Khi
        // het gio quiet, chuyen sang "glitch" - chay updateExpiredEffect()
        // (dung lai toan bo he 7 kieu + tinh cach board o tren) trong 1 dot
        // ngan 10-20s, xong quay lai quiet voi 1 khoang cho ngau nhien moi.
        bool cycleInitialized_ = false;
        bool cycleInGlitch_ = false;
        unsigned long cyclePhaseEndsAt_ = 0;

        void updateCycleMode()
        {
            const unsigned long now = millis();
            if (!cycleInitialized_)
            {
                cycleInitialized_ = true;
                cycleInGlitch_ = false;
                scheduleNextQuietPhase(now);
            }

            if (!cycleInGlitch_)
            {
                for (uint8_t i = 1; i < 13; ++i)
                {
                    digitalWrite(i, LOW);
                }
                if (now >= cyclePhaseEndsAt_)
                {
                    // Het gio "binh thuong" - bat dau 1 dot loi ngan.
                    cycleInGlitch_ = true;
                    expiredModeActive_ = false; // ep updateExpiredEffect() boc kieu/tinh gio moi cho dot nay
                    // Khong dung paced(): day la khoang co dinh nguoi dung
                    // muon (10-20s), khong nen bi tinh cach rieng cua board
                    // keo dai/rut ngan them.
                    cyclePhaseEndsAt_ = now + static_cast<unsigned long>(random(10000, 20000));
                    Serial.println(F("[Chu ky] Bat dau 1 dot loi ngan"));
                }
            }
            else
            {
                updateExpiredEffect();
                if (now >= cyclePhaseEndsAt_)
                {
                    // Het dot loi - quay lai "binh thuong", boc khoang cho moi.
                    cycleInGlitch_ = false;
                    expiredModeActive_ = false;
                    for (uint8_t i = 1; i < 13; ++i)
                    {
                        digitalWrite(i, LOW);
                    }
                    scheduleNextQuietPhase(now);
                    Serial.println(F("[Chu ky] Het dot loi, tro lai binh thuong"));
                }
            }
        }

        /** Boc 1 khoang thoi gian "binh thuong" moi, ngau nhien trong
         *  [expiredCycleMinMinutes, expiredCycleMaxMinutes] (phut, app
         *  chon qua Set License). Khong co gian theo tinh cach board - day
         *  la khoang nguoi dung tu tay dat, phai dung y muon. */
        void scheduleNextQuietPhase(unsigned long now)
        {
            uint32_t minMinutes = expiredCycleMinMinutes > 0 ? expiredCycleMinMinutes : 1;
            uint32_t maxMinutes = expiredCycleMaxMinutes >= minMinutes ? expiredCycleMaxMinutes : minMinutes;
            const unsigned long minMs = minMinutes * 60000UL;
            const unsigned long maxMs = maxMinutes * 60000UL;
            const unsigned long waitMs = minMs < maxMs
                                             ? static_cast<unsigned long>(random(minMs, maxMs))
                                             : minMs;
            cyclePhaseEndsAt_ = now + waitMs;
            Serial.printf("[Chu ky] Binh thuong trong %lu phut toi\n", waitMs / 60000UL);
        }

        /** In log moi 50s bao dang chay expiredLedMode nao (1=Tat het,
         *  2=Nhap nhay 2s) - de doi chieu qua Serial xem node co dang ap
         *  dung dung mode app da chon hay khong. */
        void logExpiredModeOnce(uint8_t mode)
        {
            const unsigned long now = millis();
            if (now - lastLog_ >= 50000)
            {
                lastLog_ = now;
                Serial.printf("Het han: dang chay expiredLedMode=%u\n", mode);
            }
        }

        void writePort(uint8_t portIndex, uint8_t level)
        {
            for (uint8_t pin : ports_[portIndex])
            {
                digitalWrite(pin, level);
            }
        }

        void writeAllPorts(uint8_t level)
        {
            for (uint8_t i = 0; i < PORT_COUNT; ++i)
            {
                writePort(i, level);
            }
        }

        /** Boc 1 kieu nhap nhay moi va thoi luong chay kieu do - goi khi moi
         *  vao che do het han va moi khi het gio. 40% roi dung mode "ua
         *  thich" rieng cua board nay (favoriteMode_), 60% con lai boc that
         *  su ngau nhien trong ca 7 kieu (ke ca chinh no) - vua co net rieng
         *  on dinh theo tung board, vua khong cung nhac/de doan qua. */
        void pickNextExpiredMode()
        {
            mode_ = random(0, 100) < 40
                        ? favoriteMode_
                        : static_cast<ExpiredMode>(random(0, static_cast<int>(ExpiredMode::MODE_COUNT)));
            modeStartedAt_ = millis();
            modeDurationMs_ = random(paced(6000), paced(18000));
            chaseIndex_ = 0;
            blackoutUntil_ = 0;
            blackoutFlashing_ = false;
            syncBlinkOn_ = false;
            lastTick_ = 0; // buoc tick dau tien chay ngay, khong doi het chu ky
        }

        /**
         * @brief Het han license: chay 1 trong nhieu kieu nhap nhay gia lap
         *        loi (xem ExpiredMode), tu doi kieu ngau nhien sau moi
         *        modeDurationMs_ de khong tao thanh 1 "dau hieu" co dinh.
         */
        void updateExpiredEffect()
        {
            const unsigned long now = millis();
            if (!expiredModeActive_)
            {
                expiredModeActive_ = true;
                pickNextExpiredMode();
            }
            else if (now - modeStartedAt_ >= modeDurationMs_)
            {
                pickNextExpiredMode();
            }

            switch (mode_)
            {
            case ExpiredMode::PORT_RANDOM:
                if (now - lastTick_ >= paced(500))
                {
                    lastTick_ = now;
                    for (uint8_t i = 0; i < PORT_COUNT; ++i)
                    {
                        writePort(i, random(0, 2));
                    }
                }
                break;

            case ExpiredMode::ALL_SYNC_RANDOM:
                if (now - lastTick_ >= paced(400))
                {
                    lastTick_ = now;
                    writeAllPorts(random(0, 2));
                }
                break;

            case ExpiredMode::CHASE:
                if (now - lastTick_ >= paced(300))
                {
                    lastTick_ = now;
                    writeAllPorts(LOW);
                    writePort(chaseIndex_, HIGH);
                    chaseIndex_ = (chaseIndex_ + 1) % PORT_COUNT;
                }
                break;

            case ExpiredMode::PIN_CHAOS:
                if (now - lastTick_ >= paced(150))
                {
                    lastTick_ = now;
                    for (uint8_t pin = 1; pin < 13; ++pin)
                    {
                        digitalWrite(pin, random(0, 2));
                    }
                }
                break;

            case ExpiredMode::SLOW_FLICKER:
                if (now - lastTick_ >= paced(2500))
                {
                    lastTick_ = now;
                    for (uint8_t i = 0; i < PORT_COUNT; ++i)
                    {
                        writePort(i, random(0, 2));
                    }
                }
                break;

            case ExpiredMode::SYNC_BLINK_1S:
                // Deu dan, khong ngau nhien: ca 4 port cung sang 500ms, tat
                // 500ms, lap lai - chu ky dung 1 giay (co gian theo tinh cach
                // rieng cua board qua paced()).
                if (now - lastTick_ >= paced(500))
                {
                    lastTick_ = now;
                    syncBlinkOn_ = !syncBlinkOn_;
                    writeAllPorts(syncBlinkOn_ ? HIGH : LOW);
                }
                break;

            case ExpiredMode::BLACKOUT_PULSE:
            default:
                // Da phan thoi gian tat het; cu moi 1.5-5s loe sang 1 port
                // ngau nhien trong 80ms roi tat lai - giong 1 board dang hong
                // chap chon hon la 1 "ma loi" co dinh.
                if (blackoutFlashing_)
                {
                    if (now - lastTick_ >= paced(80))
                    {
                        blackoutFlashing_ = false;
                        writeAllPorts(LOW);
                        blackoutUntil_ = now + random(paced(1500), paced(5000));
                    }
                }
                else if (now >= blackoutUntil_)
                {
                    blackoutFlashing_ = true;
                    lastTick_ = now;
                    writeAllPorts(LOW);
                    writePort(random(0, PORT_COUNT), HIGH);
                }
                break;
            }

            if (now - lastLog_ >= 50000)
            {
                lastLog_ = now;
                Serial.println(F("Het han: dang chay hieu ung loi gia lap (tu doi kieu)"));
            }
        }
    };

} // namespace Hub66s

#endif // HUB66S_LED_DISPLAY_H
