#ifndef CONFIG_H
#define CONFIG_H

#include <Arduino.h>
#include <ArduinoJson.h>
#include <time.h>
#include <MD5Builder.h>
#include "led_status.h"
#include <Preferences.h> //Thu vien luu tru du lieu khong mat khi tat nguon
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

// Phien ban firmware gui trong phan hoi thong tin thiet bi.
#define FIRMWARE_VERSION "1.0.0"

// Gioi han LID dang chuoi luu trong license/NVS.
constexpr size_t LID_MAX_LEN = 30;
constexpr size_t LID_BUF_LEN = LID_MAX_LEN + 1;

// Dinh nghia chan LED
#define LED_PIN 46

// Dinh nghia cac opcode
#define LIC_TIME_GET 0x01
#define LIC_SET_LICENSE 0x02
#define LIC_GET_LICENSE 0x03
#define LIC_LICENSE_DELETE 0x04
#define LIC_LICENSE_DELETE_ALL 0x05
#define LIC_INFO 0x06
// Opcode 0x07 chi cau hinh LID; deviceUid van co dinh theo MAC.
#define CONFIG_LID 0x07
#define CONFIG_LID_ACK (CONFIG_LID | 0x80)
#define LIC_INFO_RESPONSE (LIC_INFO | 0x80)

// Kich thuoc buffer cho JSON
#define BUFFER_SIZE 512

// Dia chi MAC broadcast va khoa bi mat
static uint8_t senderMac[] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};   // MAC cua LIC66S
static uint8_t receiverMac[] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF}; // MAC broadcast
#define private_key "khoabi_mat_123"

// Tao auth khi dinh danh nguon la UID dang chuoi (LIC_xxxxx hoac HUB_xxxxx).
String md5Hash(const char *id_src, const char *id_des, String mac_src, String mac_des, uint8_t opcode, const String &data,
               unsigned long timestamp)
{
    MD5Builder md5;
    md5.begin();
    md5.add(id_src);
    md5.add(id_des);
    md5.add(mac_src);
    md5.add(mac_des);
    md5.add(String(opcode));
    md5.add(data);
    md5.add(String(timestamp));
    md5.add(private_key);
    md5.calculate();
    return md5.toString();
}

// Cau truc du lieu
typedef struct
{
    char lid[LID_BUF_LEN]; // Ten/vi tri do master gan cho license
    String license; // Noi dung license
    time_t created;
    time_t expired;
    uint32_t duration;
    uint32_t remain;
    bool expired_flag; // Da het han chua
    String deviceName; // Ten thiet bi
    String version;
} LicenseInfo;

typedef struct
{
    char payload[512]; // Kich thuoc payload co the dieu chinh
} PayloadStruct;

// Lay dia chi MAC cua thiet bi
String getDeviceMacAddress()
{
    return WiFi.macAddress();
}

// Bien toan cuc
extern LedStatus led;
extern LicenseInfo globalLicense;
extern PayloadStruct message;
extern SemaphoreHandle_t preferencesMutex;
extern char config_lid[LID_BUF_LEN];
extern char card_type[3]; // "OB" hoac "R"
extern int32_t matrix_x;
extern int32_t matrix_y;
extern char deviceUid[10]; // HUB_ + 5 ky tu cuoi UUID ESP32 + '\0'
extern time_t start_time;
extern const uint32_t duration;
extern bool expired_flag;
extern uint8_t expired;
extern uint32_t now;
extern uint32_t lastSendTime;

#endif // CONFIG_H
