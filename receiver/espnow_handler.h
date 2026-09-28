#ifndef ESPNOW_HANDLER_H
#define ESPNOW_HANDLER_H

#include <WiFi.h>
#include <ESP32_NOW.h>
#include "config.h"

extern bool dang_gui;          // co dang gui
extern bool waitingSendResult; // Co cho ket qua gui
extern bool needRetry;         // Can gui lai
extern uint8_t retries;        // so lan da thu gui lai
extern unsigned long lastTime; // Thoi diem gui cuoi cung

void onReceive(const esp_now_recv_info *recv_info, const uint8_t *data, int len);

inline void initEspNow()
{
  // Thiet lap che do wifi station
  //  WiFi.mode(WIFI_STA);
  //  delay(100); // Doi WiFi on dinh
  Serial.println("🌐 WiFi mode set to Station");
  //  Serial.println("MAC Address: " + WiFi.macAddress());

  // Khoi tao ESP-now
  if (esp_now_init() != ESP_OK)
  {
    Serial.println("❌ ESP-NOW init failed!");
    // networkConnected = false;
    return;
  }
  // networkConnected = true;

  /*
  // Callback xu ly trang thai gui goi tin
  esp_now_register_send_cb([](const uint8_t *mac_addr, esp_now_send_status_t status)
                           {
    Serial.print("Send: ");
    Serial.println(status == ESP_NOW_SEND_SUCCESS ? "OK" : "F");
    if (status == ESP_NOW_SEND_SUCCESS) {
      dang_gui = false;
    }
  });
  */

  // Callback xu ly trang thai gui goi tin
  esp_now_register_send_cb([](const uint8_t *mac_addr, esp_now_send_status_t status)
                           {
    Serial.print("Send: ");
    Serial.println(status == ESP_NOW_SEND_SUCCESS ? "OK" : "F");
    waitingSendResult = false;
    if (status == ESP_NOW_SEND_SUCCESS)
    {
      dang_gui = false;
      needRetry = false;
      retries = 0;
    }
    else
    {
      needRetry = true;
      lastTime = millis();
    } });
  //-----------------------

  // Callback xu ly goi tin nhan duoc
  esp_now_register_recv_cb([](const esp_now_recv_info *recv_info, const uint8_t *data, int len)
                           { onReceive(recv_info, data, len); });

  // Them peer broadcast de nhan goi tin tu moi thiet bi
  esp_now_peer_info_t peerInfo = {};
  memcpy(peerInfo.peer_addr, senderMac, 6); // FF:FF:FF:FF:FF:FF
  peerInfo.channel = 1;                     // Kenh co dinh de dong bo voi sender
  peerInfo.encrypt = false;                 // tam thoi tat ma hoa
  if (esp_now_add_peer(&peerInfo) != ESP_OK)
  {
    Serial.println("❌ Failed to add peer!");
    // networkConnected = false;
  }
  else
    Serial.println("add peer ok");
}

#endif
