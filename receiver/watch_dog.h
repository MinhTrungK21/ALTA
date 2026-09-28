#ifndef HUB66S_WATCH_DOG_H
#define HUB66S_WATCH_DOG_H

#include <Arduino.h>
#include "esp_task_wdt.h"
#include "config.h" // de truy cap globalLicense.expired_flag
namespace Hub66s
{

    class WatchDog
    {
    public:
        /**
         * @param timeoutSeconds thoi gian cho (1- 120 giay).
         * @param panic          TRUE de cho phep reset khi treo task
         */
        static void begin(uint32_t timeoutSeconds = 10, bool panic = true)
        {
            // Kiem tra gia tri timeoutSeconds
            if (timeoutSeconds < 1 || timeoutSeconds > 120)
                timeoutSeconds = 10; // Gioi han trong khoang 1-120 giay

            // Dam bao khong bi loi khi WDT da init truoc do
            esp_task_wdt_deinit();

            esp_task_wdt_config_t cfg = {
                .timeout_ms = timeoutSeconds * 1000U,
                .idle_core_mask = (1 << portNUM_PROCESSORS) - 1, // watch both cores' idle tasks
                .trigger_panic = panic};

            esp_err_t ret = esp_task_wdt_init(&cfg);
            if (ret != ESP_OK)
            {
                Serial.printf("❌ Watchdog init failed: %d\n", ret);
            }
            else
            {
                Serial.println("✅ Watchdog initialized");
            }

            esp_err_t retAdd = esp_task_wdt_add(nullptr);
            if (retAdd == ESP_ERR_INVALID_STATE)
            {
                Serial.println("⚠️ Task đã được đăng ký TWDT từ trước, bỏ qua.");
            }
            else if (retAdd != ESP_OK)
            {
                Serial.printf("❌ Không thể thêm task vào TWDT: %d\n", retAdd);
            }

            // Task hien tai da duoc them o tren; khong dang ky trung lan hai.
        }

        /** Feed (reset) watchdog cua task hien tai */
        static inline void feed()
        {
            esp_task_wdt_reset();
        }

        /** Dang ky them task khac neu dung FreeRTOS task rieng */
        static inline void addTask(TaskHandle_t taskHandle = nullptr)
        {
            esp_task_wdt_add(taskHandle);
        }
    };

} // namespace Hub66s

#endif // HUB66S_WATCH_DOG_H
