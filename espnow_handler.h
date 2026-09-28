#ifndef ESPNOW_HANDLER_H
#define ESPNOW_HANDLER_H

#include <Arduino.h>
#include <ArduinoJson.h>
#include <esp_now.h>
#include <cstring>

#include "config.h"
#include "protocol_handler.h"

struct espnow_rx_packet_t
{
  uint8_t mac[6];
  uint16_t length;
  uint8_t payload[sizeof(PayloadStruct)];
};

static QueueHandle_t espNowRxQueue = nullptr;
static volatile uint32_t espNowDroppedPackets = 0;
constexpr uint16_t ESPNOW_RX_QUEUE_DEPTH = 64;

inline bool beginEspNowReceiveQueue()
{
  if (espNowRxQueue == nullptr)
  {
    espNowRxQueue = xQueueCreate(ESPNOW_RX_QUEUE_DEPTH, sizeof(espnow_rx_packet_t));
  }
  return espNowRxQueue != nullptr;
}

// esp_now_send_cb_t's signature changed across arduino-esp32 core versions:
// older cores passed the destination MAC directly, current cores pass a
// esp_now_send_info_t (alias of wifi_tx_info_t) whose des_addr field holds it.
inline void OnDataSent(const esp_now_send_info_t *txInfo, esp_now_send_status_t status)
{
  const uint8_t *mac = txInfo != nullptr ? txInfo->des_addr : nullptr;
  if (PROTOCOL_VERBOSE_LOG)
  {
    char macText[18] = "UNKNOWN";
    if (mac != nullptr)
    {
      formatMacAddress(mac, macText);
    }
    Serial.printf("[ESP-NOW TX STATUS] MAC=%s | %s\n", macText,
                  status == ESP_NOW_SEND_SUCCESS ? "DELIVERED" : "FAILED");
  }
  else if (status != ESP_NOW_SEND_SUCCESS)
  {
    Serial.println("ESP-NOW send failed");
  }
}

inline void onReceive(const esp_now_recv_info *info, const uint8_t *data, int length)
{
  if (info == nullptr || data == nullptr || length <= 0 ||
      length > static_cast<int>(sizeof(PayloadStruct)) || espNowRxQueue == nullptr)
  {
    espNowDroppedPackets++;
    return;
  }

  espnow_rx_packet_t packet = {};
  memcpy(packet.mac, info->src_addr, 6);
  memcpy(packet.payload, data, length);
  packet.length = static_cast<uint16_t>(length);
  if (xQueueSend(espNowRxQueue, &packet, 0) != pdTRUE)
  {
    espNowDroppedPackets++;
  }
}

inline size_t handleEspNowReceiveQueue(size_t maxPackets = 24)
{
  if (espNowRxQueue == nullptr)
  {
    return 0;
  }

  size_t processed = 0;
  espnow_rx_packet_t packet;
  while (processed < maxPackets && xQueueReceive(espNowRxQueue, &packet, 0) == pdTRUE)
  {
    JsonDocument doc;
    const DeserializationError error = deserializeJson(doc, packet.payload, packet.length);
    if (error)
    {
      Serial.printf("Invalid ESP-NOW JSON: %s\n", error.c_str());
    }
    else
    {
      logProtocolMessage("RX", packet.mac, doc);
      processReceivedData(doc, packet.mac);
    }
    processed++;
  }
  return processed;
}

#endif // ESPNOW_HANDLER_H
