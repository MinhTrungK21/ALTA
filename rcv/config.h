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

// Nap firmware qua WiFi (ArduinoOTA) - CHI DE TIEN DEV/TEST TREN BENCH.
// Tat (0) truoc khi flash cho board that trien khai: mo OTA nghia la bat
// them mot duong ghi flash qua mang, khong phu hop de bat tran lan cho ca
// dan 50+ board (AP cua master gioi han 4 client cung luc - xem
// WiFi.softAP(...) trong local_web.h).
#define ENABLE_OTA 0
#if ENABLE_OTA
#include <ArduinoOTA.h>
// SSID cua AP master phat ra la "HUB66S-XXXXXX" (dat theo MAC cua board
// master, xem localApSsid trong local_web.h) - do la ten random moi board
// master, phai tu do scan WiFi tren laptop/dien thoai de xem chinh xac ten
// roi dien lai vao day. Dung AP nay (thay vi WiFi nha) de giu dung kenh 1
// nhu ESP-NOW dang dung, tranh doi kenh lam gian doan lien lac voi sender
// trong luc dang OTA.
#define OTA_WIFI_SSID "HUB66S-XXXXXX"
#define OTA_WIFI_PASSWORD "hub66s66" // Mat khau AP mac dinh trong local_web.h
#define OTA_PASSWORD "hub66s_ota"    // Mat khau rieng cho OTA, nen doi truoc khi dung that
#endif

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
// Nhap nhay nhanh LED 46 va cac chan 1-12 de nguoi van hanh nhan ra board nao dang duoc chon
// trong app. data: {"on":true|false, "sec":giay}. Khong phan hoi (fire-and-forget).
#define LIC_IDENTIFY 0x08

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
// Bao ve globalLicense.expired_flag/remain/duration khoi doc/ghi dan xen
// giua 2 nhan that su chay song song: Communication (LIC_SET_LICENSE, xoa
// license...) tren core 0, con LedDisplay::update() doc no moi 10ms tren
// core 1 (xem systemTask trong rcv.ino). Ghi cac truong nay la NHIEU dong
// lenh rieng le (duration, roi remain, roi expired_flag) - khong dung
// portENTER_CRITICAL se co luc core 1 doc trung luc core 0 ghi duoc mot
// nua, thay expired_flag con cu (true) trong khi remain da la gia tri moi
// (con han) - LedDisplay tuong nham la het han, nhay 1 nhip "loi" ngay tai
// thoi diem gui lenh Set License du lenh thanh cong hoan toan.
extern portMUX_TYPE licenseStateMux;
extern char config_lid[LID_BUF_LEN];
extern char card_type[3]; // "OB" hoac "R"
extern int32_t matrix_x;
extern int32_t matrix_y;
extern char config_group[24]; // Ten Group/vi tri (rong = chua duoc gan)
// Kieu hien thi LED 46 + cac chan 1-12 khi license het han - app chon qua o
// "Mode khi hết license" trong hop Set License, gui kem voi Set License:
//   0 = Random  - tu doi giua nhieu kieu nhap nhay gia lap loi (mac dinh)
//   1 = Tat het - khong nhap nhay gi ca
//   2 = Nhap nhay 2 giay - ca 4 port cung sang/tat deu, chu ky co dinh 2s
//   3 = Random theo chu ky - da phan thoi gian "binh thuong" (tat het nhu
//       con han), thinh thoang tu boc 1 khoang cho ngau nhien trong
//       [expiredCycleMinMinutes, expiredCycleMaxMinutes] roi chay 1 trong 7
//       kieu loi (nhu mode 0) trong 1 dot ngan (10-20s), xong quay lai "binh
//       thuong" va boc khoang cho moi - de man hinh trong nhu thinh thoang
//       tu gap loi roi tu het, khong phai bi khoa hoan toan 1 luc.
// Xem Hub66s::LedDisplay::update() trong led_display.h.
extern uint8_t expiredLedMode;
// Chi dung khi expiredLedMode == 3 (xem tren) - khoang thoi gian "binh
// thuong" giua 2 dot loi, tinh bang phut. App gui kem qua o "Chu ky binh
// thuong" trong hop Set License khi chon mode 3.
extern uint32_t expiredCycleMinMinutes;
extern uint32_t expiredCycleMaxMinutes;
extern char deviceUid[10]; // HUB_ + 5 ky tu cuoi UUID ESP32 + '\0'
extern time_t start_time;
extern const uint32_t duration;
extern bool expired_flag;
extern uint8_t expired;
extern uint32_t now;
extern uint32_t lastSendTime;
extern uint32_t bootUptimeBaseSec; // Tong uptime tich luy truoc lan khoi dong hien tai (doc tu NVS)

#endif // CONFIG_H
