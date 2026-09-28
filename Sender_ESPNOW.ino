#include <Arduino.h>
#include "stdio.h"
#include "WiFi.h"
#include "ESP32_NOW.h"
#include <ArduinoJson.h>
#include <MD5Builder.h>
#include "led_status.h"

#include "key_buttons.h"

#include "config.h"
#include "espnow_group.h"
#include "espnow_handler.h"
#include "protocol_handler.h"

#include "serial.h"

// Biến toàn cục
LedStatus led(LED_PIN); // LED nối chân 2
device_info Device;
char jsonBuffer[BUFFER_SIZE];
int bufferIndex;
uint8_t button = 0;
extern uint8_t button;
char deviceUid[10] = {0};

// Tao dinh danh co dinh tu 5 ky tu hex cuoi cua UUID eFuse ESP32.
static void initDeviceUid()
{
  const uint32_t uuidSuffix = static_cast<uint32_t>(ESP.getEfuseMac() & 0xFFFFFULL);
  snprintf(deviceUid, sizeof(deviceUid), "LIC_%05lX", static_cast<unsigned long>(uuidSuffix));
}

uint32_t nod = 0;                // số lượng thiết bị, cập nhật khi có node mới kết nối

// Local web owns the non-blocking scan, refresh, search and command jobs.
#include "local_web.h"

void setup()
{
  // The ESP32-S3 native USB-CDC (HWCDC) driver defaults to a 256-byte RX
  // ring buffer if not resized before begin() - fine for short commands,
  // but a JSON action carrying several MAC addresses (e.g. group.save with
  // ~9-10 members, "50:78:7D:18:1B:D8" x N) easily runs past 256 bytes.
  // Bytes that arrive once that buffer is full are silently dropped by the
  // USB ISR (xQueueSendFromISR just fails, no error surfaces anywhere),
  // corrupting the JSON line - which is why creating a Group with only a
  // few members worked but one with many intermittently lost some
  // (whichever MACs landed past the point the buffer overflowed), or
  // failed outright. Must be called before begin().
  Serial.setRxBufferSize(4096);
  Serial.begin(115200);
  Serial.println("Initializing board");

  initDeviceUid();
  Serial.printf("Source ID (id_src): %s\n", deviceUid);

  if (!beginLocalWeb())
  {
    Serial.println("Local Wi-Fi AP/WebSocket initialization failed");
  }

  beginUsbBridge();
  Serial.println("USB serial control link ready");

  if (!beginEspNowReceiveQueue())
  {
    Serial.println("ESP-NOW receive queue initialization failed");
    return;
  }

  if (esp_now_init() != ESP_OK)
  {
    Serial.println("❌ ESP-NOW init failed!");
    return;
  }

  esp_now_register_send_cb(OnDataSent);
  esp_now_register_recv_cb(onReceive);

  esp_now_peer_info_t peerInfo = {};
  memcpy(peerInfo.peer_addr, receiverMac, 6); // FF:FF:FF:FF:FF:FF
  peerInfo.channel = 1;                       // Kênh cố định để đồng bộ với sender
  peerInfo.encrypt = false;                   // tạm thời tắt mã hóa
  if (esp_now_add_peer(&peerInfo) != ESP_OK)
  {
    Serial.println("❌ Failed to add peer!");
  }
  else
    Serial.println("add peer ok");

  startLocalWebServices();

  led.setState(CONNECTION_ERROR);

  // Khoi tao PCF8575 tren I2C: SDA 18, SCL 17, dia chi 0x20.
  keyInit();
  Serial.println("LCD UI disabled; use http://192.168.4.1");
}

void loop()
{
  handleUsbBridge();
  led.update(); // Gọi liên tục trong loop()

  // keyPoll();
  // handleKeyButtons();

  handleLocalWeb();
  handleEspNowReceiveQueue();

  if (button != 0)
  {
    if (webLicenseJobBusy())
    {
      Serial.println("Web license job is busy; serial command rejected");
      button = 0;
      return;
    }
    Serial.println("button pressed: ");
    Serial.println(button);
    switch (button)
    {
    case 1:
      if (!startMasterSetLicense(Device_ID, datalic.lid, millis(),
                                 datalic.duration,
                                 static_cast<uint8_t>(datalic.expired)))
      {
        Serial.println("SET_LICENSE failed: Device ID is not in Node List");
      }
      break;
    case 2:
      Serial.println("Select a node from the web UI before CONFIG_DEVICE");
      break;
    case 4:
      Serial.println("Refresh known nodes, then scan for new nodes");
      startMasterRefresh();
      break;
    case 5:
      Serial.println("Broadcast discovery scan");
      startMasterScan();
      break;
    case 6:
      if (!requestNodeInfo(Device_ID, datalic.lid))
      {
        Serial.println("LIC_INFO request failed: Device ID is not in Node List");
      }
      break;
    default:
      break;
    }
    button = 0;
  }
}
