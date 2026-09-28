#include <WiFi.h>
#include <ArduinoJson.h>

#include "config.h"
#include "led_status.h"
#include "espnow_handler.h"
#include "protocol_handler.h"
#include "watch_dog.h"
#include "led_display.h"

// Define Preferences object
Preferences preferences;

LedStatus led(LED_PIN, /*activeHigh=*/false); // neu LED noi kieu active-LOW
Hub66s::LedDisplay ledDisplay;                // Khoi tao doi tuong tu lop LedDisplay

// Cau truc du lieu License
LicenseInfo globalLicense = {};
PayloadStruct message;

char config_lid[LID_BUF_LEN] = "DEFAULTLID";
char card_type[3] = "R";
int32_t matrix_x = 0;
int32_t matrix_y = 0;
char deviceUid[10] = {}; // HUB_ + 5 ky tu cuoi UUID ESP32 + '\0'

bool expired_flag = false; // Co het han
uint8_t expired = 0;       // Bien luu trang thai het han

// Bao ve truy cap dong thoi vao Preferences tu cac task FreeRTOS.
SemaphoreHandle_t preferencesMutex = nullptr;

//-----------
constexpr uint8_t MAX_RETRIES = 3; // So lan thu gui lai toi da
bool waitingSendResult = false;    // Co cho ket qua gui
bool needRetry = false;            // Can gui lai
//-----------

uint32_t now;
time_t start_time = 0;          // thoi diem bat dau tinh thoi gian
const uint32_t duration = 60;   // Gia tri co dinh sau khi gan lan dau cho license
uint32_t lastSendTime = 0;      // Thoi diem gui goi tin gan nhat
uint32_t lastRuntimeUpdate = 0; // Thoi diem cap nhat runtime gan nhat
constexpr uint8_t NVS_SAVE_INTERVAL_MINUTES = 10; //10 phut
uint8_t minutesSinceLastNvsSave = 0;
// uint32_t lastPacketNodeId = 0; // Chi o mot cho duy nhat!

// bool networkConnected = false;
uint32_t runtime = 0;
bool dang_gui = false; // co dang gui
uint32_t lastTime = 0; // thoi diem gui lan cuoi
uint8_t retries = 0;   // so lan da thu gui
uint8_t lastResponseMac[6] = {};
size_t lastResponseLen = 0;

struct ReceivedPacket
{
  uint8_t mac[6];
  int len;
  uint8_t data[sizeof(PayloadStruct)];
};
QueueHandle_t receivedPacketQueue = nullptr;

uint8_t lastPacketData[512]; // cu 250 Luu payload (co the dieu chinh kich thuoc tuy theo nhu cau, o day bang toi da cua PayloadStruct)
esp_now_recv_info lastRecvInfo;

// Luu MAC cua goi vua nhan
uint8_t lastPacketMac[6];
volatile bool hasNewPacket = false; // Luu do dai payload
int lastPacketLen;
// Luu payload (co the dieu chinh kich thuoc tuy theo nhu cau, o day bang toi da cua PayloadStruct)

// Ba task FreeRTOS duoc phan chia theo chuc nang.
TaskHandle_t communicationTaskHandle = nullptr;
TaskHandle_t licenseTaskHandle = nullptr;
TaskHandle_t systemTaskHandle = nullptr;

void communicationTask(void *parameter);
void licenseTask(void *parameter);
void systemTask(void *parameter);

// void generateDeviceUid()
// {
//   const uint32_t uuidSuffix = static_cast<uint32_t>(ESP.getEfuseMac() & 0xFFFFFULL);
//   snprintf(deviceUid, sizeof(deviceUid), "HUB_%05lX", static_cast<unsigned long>(uuidSuffix));
// }

void generateDeviceUid()
{
  const uint64_t efuseMac = ESP.getEfuseMac();

  uint8_t mac[6];
  mac[0] = (efuseMac >> 0)  & 0xFF;
  mac[1] = (efuseMac >> 8)  & 0xFF;
  mac[2] = (efuseMac >> 16) & 0xFF;
  mac[3] = (efuseMac >> 24) & 0xFF;
  mac[4] = (efuseMac >> 32) & 0xFF;
  mac[5] = (efuseMac >> 40) & 0xFF;

  // 5 HEX cuoi cua MAC hien thi
  const uint32_t suffix =
      ((uint32_t)(mac[3] & 0x0F) << 16) |
      ((uint32_t)mac[4] << 8) |
      mac[5];

  snprintf(deviceUid, sizeof(deviceUid),
           "HUB_%05lX",
           (unsigned long)suffix);
}

void enqueueReceivedPacket(const esp_now_recv_info *recv_info, const uint8_t *data, int len)
{
  if (receivedPacketQueue == nullptr || recv_info == nullptr || data == nullptr || len <= 0)
    return;

  ReceivedPacket packet = {};
  memcpy(packet.mac, recv_info->src_addr, sizeof(packet.mac));
  packet.len = min(len, (int)sizeof(packet.data));
  memcpy(packet.data, data, packet.len);
  xQueueSend(receivedPacketQueue, &packet, 0);
}

void xu_ly_dang_gui()
{
  // Chi xu ly khi dang trong trang thai gui
  if (!dang_gui)
    return;

  // Neu dang cho ket qua gui thi chua lam gi ca
  if (waitingSendResult)
    return;

  // Neu khong can gui lai thi ket thuc trang thai gui
  if (!needRetry)
  {
    dang_gui = false;
    return;
  }

  if (retries >= MAX_RETRIES)
  {
    // Da thu 3 lan ma van fail → dung gui
    dang_gui = false;
    needRetry = false;
    waitingSendResult = false;
    retries = 0;
    Serial.println("Gui that bai sau 3 lan thu, dung gui lai.");
    return;
  }

  uint32_t now = millis(); // hoac dung gia tri unsigned long
  // Chua du 1s ke tu lan gui truoc thi bo qua
  if (now - lastTime < 1000)
    return;

  // Da du 1s, cap nhat thoi diem va thu gui
  lastTime = now;
  retries++;
  needRetry = false;        // chi gui lai mot lan
  waitingSendResult = true; // cho ket qua gui
  Serial.printf("Gui lai lan %d...\n", retries);
  // Chi gui lai response da tao; khong xu ly lai request va tac dung phu.
  esp_err_t sendResult = esp_now_send(lastResponseMac, (uint8_t *)message.payload, lastResponseLen);
  if (sendResult != ESP_OK)
  {
    waitingSendResult = false;
    needRetry = true;
    lastTime = millis();
  }
  if (!waitingSendResult && !needRetry)
  {
    // Khong co goi nao duoc gui trong lan nay
    dang_gui = false;
  }
}

void setup()
{
  Serial.begin(115200);
  delay(1000);
  Serial.println("\n🌟 HUB66S Receiver Started");
  generateDeviceUid();
  globalLicense.deviceName = deviceUid;
  Serial.printf("Device UID: %s\n", deviceUid);
  ledDisplay.begin();          // Khoi tao module LED hien thi
  Hub66s::WatchDog::begin(10); // khoi tao WDT 10 s (toan chip reset khi qua han)

  WiFi.mode(WIFI_STA); // Enable Wi-Fi in Station mode for ESP-NOW
  delay(100);
  WiFi.setTxPower(WIFI_POWER_2dBm);
  preferencesMutex = xSemaphoreCreateMutex();
  if (preferencesMutex == nullptr)
  {
    Serial.println("Khong tao duoc mutex NVS");
  }

  receivedPacketQueue = xQueueCreate(6, sizeof(ReceivedPacket));
  if (receivedPacketQueue == nullptr)
  {
    Serial.println("Khong tao duoc queue nhan ESP-NOW");
  }
  initEspNow();                        // Initialize ESP-NOW
  configTime(0, 0, "pool.ntp.org");    // Configure NTP for time synchronization
  esp_now_register_recv_cb(onReceive); // Register receive callback

  const bool licenseLoaded = loadLicenseData();
  // Khoi tao trang thai expired dua tren globalLicense
  if (globalLicense.remain > 0 && !globalLicense.expired_flag)
  {
    expired = 0; // Giay phep con han
  }
  else
  {
    expired = 1; // Giay phep het han hoac khong hop le
  }
  // Khong ghi gia tri mac dinh 0 de len NVS neu doc NVS that bai/mat key.
  if (licenseLoaded)
  {
    Serial.println("[BOOT] License da duoc khoi phuc tu NVS");
  }
  else
  {
    Serial.println("[BOOT] Khong khoi phuc duoc license; giu NVS nguyen trang de debug");
  }
  led.setState(CONNECTION_ERROR);

  // Core 0: uu tien xu ly giao tiep. Core 1: nghiep vu nen va hien thi.
  xTaskCreatePinnedToCore(communicationTask, "Communication", 8192, nullptr, 3,
                          &communicationTaskHandle, 0);
  xTaskCreatePinnedToCore(licenseTask, "License", 4096, nullptr, 1,
                          &licenseTaskHandle, 1);
  xTaskCreatePinnedToCore(systemTask, "System", 4096, nullptr, 2,
                          &systemTaskHandle, 1);
}

void updateLicense()
{
  // Kiem tra license va gui thong tin dinh ky
  uint32_t nowMillis = millis();
  if (nowMillis - lastRuntimeUpdate >= 60000)
  {
    lastRuntimeUpdate = nowMillis;
    now = time(nullptr);

    if (globalLicense.duration > 0)
    {
      runtime++; // tang thoi gian chay tung phut
      globalLicense.remain = globalLicense.duration > runtime ? globalLicense.duration - runtime : 0; // Ngan remain am
      bool licenseStateChanged = false;

      // Kiem tra license het han
      if (globalLicense.remain <= 0 && !globalLicense.expired_flag)
      {
        globalLicense.expired_flag = true;
        globalLicense.remain = 0;
        expired = 1; // Giay phep het han
        licenseStateChanged = true;
      }
      else if (globalLicense.remain > 0 && globalLicense.expired_flag)
      {
        globalLicense.expired_flag = false;
        expired = 0; // Giay phep con han
        licenseStateChanged = true;
      }

      minutesSinceLastNvsSave++;
      if (licenseStateChanged || minutesSinceLastNvsSave >= NVS_SAVE_INTERVAL_MINUTES)
      {
        if (saveLicenseData(false))
          minutesSinceLastNvsSave = 0;
        else
          Serial.println("[NVS] LOI: khong luu duoc runtime");
      }
    }
    else
    {
      // Giay phep khong hop le
      expired = 1; // Het han
      globalLicense.expired_flag = true;
      globalLicense.remain = 0;
      minutesSinceLastNvsSave = 0;
    }
    // Cap nhat LED trang thai
    if (globalLicense.expired_flag || globalLicense.remain <= 0)
    {
      led.setState(LICENSE_EXPIRED); // LED tat
    }
    else
    {
      led.setState(NORMAL_STATUS); // LED sang lien tuc
    }
  }
}

// Task 1: giao tiep Serial va quan ly gui/thu lai ESP-NOW.
void updateCommunication()
{
  xu_ly_dang_gui();
}

// Task 2: xu ly goi ESP-NOW da duoc callback dua vao bo dem.
void updateProtocol()
{
  ReceivedPacket packet;
  if (receivedPacketQueue != nullptr && xQueueReceive(receivedPacketQueue, &packet, 0) == pdTRUE)
  {
    // Reset trang thai truoc khi xu ly goi moi
    retries = 0;
    dang_gui = true;
    needRetry = false;
    waitingSendResult = false;
    // Goi ham xu ly voi dung kieu
    memcpy(lastPacketMac, packet.mac, sizeof(lastPacketMac));
    lastPacketLen = packet.len;
    memcpy(lastPacketData, packet.data, packet.len);
    xu_ly_data(nullptr, packet.data, packet.len);
  }
}

// Task 4: cap nhat hien thi va giam sat he thong.
void updateSystem()
{
  led.update();
  ledDisplay.update();
}

void communicationTask(void *parameter)
{
  (void)parameter;
  Hub66s::WatchDog::addTask();
  for (;;)
  {
    updateCommunication();
    updateProtocol();
    Hub66s::WatchDog::feed();
    vTaskDelay(pdMS_TO_TICKS(10));
  }
}

void licenseTask(void *parameter)
{
  (void)parameter;
  Hub66s::WatchDog::addTask();
  for (;;)
  {
    updateLicense();
    Hub66s::WatchDog::feed();
    vTaskDelay(pdMS_TO_TICKS(1000));
  }
}

void systemTask(void *parameter)
{
  (void)parameter;
  Hub66s::WatchDog::addTask();
  for (;;)
  {
    updateSystem();
    Hub66s::WatchDog::feed();
    vTaskDelay(pdMS_TO_TICKS(10));
  }
}

void loop()
{
  // Arduino loop task van duoc watchdog giam sat nhung khong xu ly nghiep vu.
  Hub66s::WatchDog::feed();
  vTaskDelay(pdMS_TO_TICKS(1000));
}
