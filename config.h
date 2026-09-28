#ifndef CONFIG_H
#define CONFIG_H

#include <Arduino.h>
#include <ArduinoJson.h>
#include <MD5Builder.h>
#include <WiFi.h>
// #include <Preferences.h> // Thư viện để lưu trữ dữ liệu vào bộ nhớ flash
#include "ESP32_NOW.h"
#include "led_status.h"
#include "device_state.h"

#define FW_VERSION "1.0.0" // Phiên bản firmware hiện tại
Licence datalic; 
int Device_ID;
// #define LED_PIN 2       // Chan led trang thai man hinh LCD
#define LED_PIN 46       // Chân LED board HUB66S

// Các opcode cho các lệnh
#define LIC_TIME_GET 0x01
#define LIC_SET_LICENSE 0x02        // tạo bản tin license
#define LIC_GET_LICENSE 0x03        // đọc trạng thái license của Hub66s
#define LIC_LICENSE_DELETE 0x04     // xóa 1 license
#define LIC_LICENSE_DELETE_ALL 0x05 // xóa tất cả license
#define LIC_INFO 0x06               // yêu cầu node báo cáo info (firmware/điện áp/nhiệt độ/uptime)
// cấu hình thiết bị. data có thể mang: "new_lid" (đổi LID), và/hoặc
// "Matrix":{"x":cột,"y":hàng} (ghi vị trí ma trận layout xuống node), và/hoặc
// "cardType":"OB"|"R" (loại board: card onboard / card rời) — node tự lưu
// NVS và báo lại các trường này trong các phản hồi sau. Có thể chỉ gửi 1
// trong 3, không nhất thiết phải đủ. Ack là (LIC_CONFIG_DEVICE | 0x80).
#define LIC_CONFIG_DEVICE 0x07
// Live-tested over USB serial against the real fleet (2026-09-08): nodes
// still on older firmware ack LIC_INFO with the flat literal 0x80 and a
// {deviceName,lid,version,status} payload, while a node already updated
// with the new telemetry firmware acks with (LIC_INFO | 0x80) = 0x86 and
// the richer {device_id,firmware_version,voltage_v,temperature_c,uptime_m,
// protocol,status} payload — both were observed on the wire in the same
// scan. Since the fleet is a mix until every node gets the new firmware,
// the hub has to recognize both; see LIC_INFO_RESPONSE_LEGACY below and
// its use in protocol_handler.h/local_web.h::webOnInfoResponse().
#define LIC_INFO_RESPONSE (LIC_INFO | 0x80)  // 0x86 - new telemetry firmware
#define LIC_INFO_RESPONSE_LEGACY 0x80        // pre-update node firmware
// Nhap nhay nhanh LED 46 cua node de nhan dien board dang chon tren app.
// Fire-and-forget: node khong phan hoi. data: {"on":bool,"sec":giay}.
#define LIC_IDENTIFY 0x08

// Biến dùng trong hàm nhận dữ liệu json từ PC
const int BUFFER_SIZE = 512; // Tăng kích thước buffer để chứa JSON lớn
//-----------------------------------------------
uint8_t receiverMac[] = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff}; // Broadcast
const char *private_key = "khoabi_mat_123";

typedef struct
{
    int lid;        // License ID
    int id;         // ID thiết bị
    String license; // Nội dung license
    uint32_t created;
    uint32_t expired;
    uint32_t duration;
    uint32_t remain;
    bool expired_flag; // Đã hết hạn chưa
    String deviceName; // Tên thiết bị
    String version;    // Phiên bản firmware
} LicenseInfo;

typedef struct
{
    char payload[512];
} PayloadStruct;

// Hàm mã hóa Auth MD5 theo schema protocol hiện tại.
// lid vẫn nằm trong JSON nhưng không tham gia chuỗi MD5. Các node hiện tại ký:
// id_src + id_des + mac_src + mac_des + opcode + data + time + private_key.
String md5Hash(const String &sourceId, int lid, const String &id_des,
    const String &mac_src, const String &mac_des, uint8_t opcode,
    const String &data, unsigned long timestamp)
{
    MD5Builder md5;
    md5.begin();
    md5.add(sourceId);
    (void)lid;
    md5.add(id_des);
    md5.add(mac_src);
    md5.add(mac_des);
    md5.add(String(opcode));
    md5.add(data);
    md5.add(String(timestamp));
    md5.add(private_key);
    md5.calculate();
    return md5.toString(); // Trả về chuỗi MD5 hex
}

//Biến toàn cục 
extern LedStatus led;
extern int id_des;
extern bool config_processed;
extern char jsonBuffer[BUFFER_SIZE];
extern int bufferIndex;
extern uint8_t button;
extern String device_id;
extern char deviceUid[10]; // Giá trị id_src: LIC_ + 5 ký tự cuối UUID ESP32 + '\0'
extern uint32_t nod; // number of device

#endif // CONFIG_H
