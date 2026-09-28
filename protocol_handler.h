#ifndef PROTOCOL_HANDLER_H
#define PROTOCOL_HANDLER_H

#include <Arduino.h>
#include <ArduinoJson.h>
#include <WiFi.h>
#include <esp_now.h>

#include "config.h"
#include "espnow_group.h"
#include "web_bridge.h"

// Debug ESP-NOW protocol traffic on the USB Serial Monitor.
// Each valid packet is printed with its direction, peer MAC, opcode and JSON.
constexpr bool PROTOCOL_VERBOSE_LOG = true;

inline void formatMacAddress(const uint8_t mac[6], char out[18])
{
  snprintf(out, 18, "%02X:%02X:%02X:%02X:%02X:%02X",
           mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
}

inline void logProtocolMessage(const char *direction, const uint8_t mac[6],
                               JsonDocument &message)
{
  if (!PROTOCOL_VERBOSE_LOG)
  {
    return;
  }
  char macText[18];
  formatMacAddress(mac, macText);
  const uint8_t opcode = message["opcode"] | 0;
  Serial.printf("[ESP-NOW %s] MAC=%s | OPCODE=0x%02X | JSON=",
                direction, macText, opcode);
  serializeJson(message, Serial);
  Serial.println();
}

inline bool isBroadcastMac(const uint8_t mac[6])
{
  static const uint8_t broadcast[6] = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff};
  return memcmp(mac, broadcast, 6) == 0;
}

inline bool ensureEspNowPeer(const uint8_t mac[6])
{
  static uint8_t activeUnicastPeer[6] = {};
  static bool hasActiveUnicastPeer = false;

  if (isBroadcastMac(mac))
  {
    if (esp_now_is_peer_exist(mac))
    {
      return true;
    }
  }
  else
  {
    if (hasActiveUnicastPeer && memcmp(activeUnicastPeer, mac, 6) == 0 &&
        esp_now_is_peer_exist(mac))
    {
      return true;
    }
    if (hasActiveUnicastPeer)
    {
      esp_now_del_peer(activeUnicastPeer);
      hasActiveUnicastPeer = false;
    }
    if (esp_now_is_peer_exist(mac))
    {
      esp_now_del_peer(mac);
    }
  }
  esp_now_peer_info_t peer = {};
  memcpy(peer.peer_addr, mac, 6);
  peer.channel = 1;
  peer.encrypt = false;
  if (esp_now_add_peer(&peer) != ESP_OK)
  {
    return false;
  }
  if (!isBroadcastMac(mac))
  {
    memcpy(activeUnicastPeer, mac, 6);
    hasActiveUnicastPeer = true;
  }
  return true;
}

inline bool sendProtocolMessage(const uint8_t targetMac[6], int id_des, int lid,
                                uint8_t opcode, JsonDocument &data,
                                uint32_t *requestTime = nullptr,
                                bool includeLidInData = true)
{
  if (targetMac == nullptr || !ensureEspNowPeer(targetMac))
  {
    return false;
  }

  if (data.isNull())
  {
    data.to<JsonObject>();
  }
  if (includeLidInData)
  {
    data["lid"] = lid;
  }

  char macDes[18];
  formatMacAddress(targetMac, macDes);
  const String macSrc = WiFi.macAddress();
  const uint32_t timestamp = millis();
  if (requestTime != nullptr)
  {
    *requestTime = timestamp;
  }

  String dataText;
  serializeJson(data, dataText);
  const String auth = md5Hash(deviceUid, lid, String(id_des), macSrc, macDes,
                              opcode, dataText, timestamp);

  JsonDocument packet;
  packet["id_src"] = deviceUid;
  packet["id_des"] = id_des;
  packet["mac_src"] = macSrc;
  packet["mac_des"] = macDes;
  packet["opcode"] = opcode;
  packet["data"] = data;
  packet["time"] = timestamp;
  packet["auth"] = auth;

  PayloadStruct payload = {};
  const size_t length = serializeJson(packet, payload.payload, sizeof(payload.payload));
  if (length == 0 || length >= sizeof(payload.payload))
  {
    Serial.println("Protocol payload is too large");
    return false;
  }

  const esp_err_t result = esp_now_send(targetMac,
                                         reinterpret_cast<const uint8_t *>(&payload),
                                         length + 1);
  logProtocolMessage("TX", targetMac, packet);
  if (PROTOCOL_VERBOSE_LOG && result != ESP_OK)
  {
    Serial.printf("[ESP-NOW TX ERROR] MAC=%s | esp_now_send=%d\n",
                  macDes, static_cast<int>(result));
  }
  return result == ESP_OK;
}

inline bool sendGetLicense(const uint8_t targetMac[6], int id_des, int lid,
                           uint32_t *requestTime = nullptr)
{
  JsonDocument data;
  return sendProtocolMessage(targetMac, id_des, lid, LIC_GET_LICENSE, data, requestTime);
}

// Asks a node to report its LIC_INFO telemetry (firmware version, supply
// voltage, internal temperature, uptime...) - see webOnInfoResponse() in
// local_web.h for how the LIC_INFO_RESPONSE reply is parsed.
inline bool sendGetInfo(const uint8_t targetMac[6], int id_des, int lid,
                        uint32_t *requestTime = nullptr)
{
  JsonDocument data;
  return sendProtocolMessage(targetMac, id_des, lid, LIC_INFO, data, requestTime);
}

// Makes a node's status LED blink fast (LIC_IDENTIFY) so the operator can tell
// which physical board is selected in the app; `on=false` stops it. The node
// never replies, and it stops by itself after `seconds` (see
// LedStatus::setIdentify()). includeLidInData=false: nothing to do with the
// node's LID, so don't stamp one into the payload.
inline bool sendIdentify(const uint8_t targetMac[6], int id_des, int lid, bool on,
                         uint16_t seconds)
{
  JsonDocument data;
  data["on"] = on;
  data["sec"] = seconds;
  return sendProtocolMessage(targetMac, id_des, lid, LIC_IDENTIFY, data, nullptr, false);
}

// Writes this node's matrix layout position (and which Group/location it
// belongs to) into it (row/col 1-based), so it can report both back on
// later scans. Sent as a LIC_CONFIG_DEVICE with "Matrix":{"x":col,"y":row
// [,"alias":".."]} plus a top-level "group" (no "new_lid", so the node
// leaves its LID alone). `alias` is the operator's own board number/label;
// `group` is the Group's name (each Group is its own matrix/location on the
// app's Groups page) - both are included so the node can remember and echo
// them back. The node side must persist and echo "Matrix"/"group". See
// webOnConfigExtraResponse() in local_web.h.
inline bool sendSetMatrix(const uint8_t targetMac[6], int id_des, int lid,
                          int row, int col, const char *alias, const char *group,
                          uint32_t *requestTime = nullptr)
{
  JsonDocument data;
  JsonObject matrix = data["Matrix"].to<JsonObject>();
  matrix["x"] = col;
  matrix["y"] = row;
  if (alias != nullptr && alias[0] != '\0')
  {
    matrix["alias"] = alias;
  }
  if (group != nullptr && group[0] != '\0')
  {
    data["group"] = group;
  }
  // includeLidInData=false: this is a Matrix-only config write, not a LID
  // change. sendProtocolMessage() otherwise always stamps data["lid"]=lid
  // (0 for a node with no license yet), which the node's CONFIG_LID handler
  // treats as an explicit "change my LID" request (any "lid" key present is
  // enough) - silently overwriting the node's config_lid to "0" on every
  // Set Matrix call.
  return sendProtocolMessage(targetMac, id_des, lid, LIC_CONFIG_DEVICE, data, requestTime, false);
}

// Pulls the node's stored matrix position out of a response's data object.
// Accepts either the nested {"Matrix":{"x":col,"y":row}} shape or a flat
// {"row":..,"col":..}. Returns false (row/col untouched) if neither is
// present.
inline bool readMatrixPos(JsonObjectConst data, int &row, int &col)
{
  JsonVariantConst matrix = data["Matrix"];
  if (!matrix["x"].isNull() && !matrix["y"].isNull())
  {
    col = matrix["x"].as<int>();
    row = matrix["y"].as<int>();
    return true;
  }
  if (!data["row"].isNull() && !data["col"].isNull())
  {
    row = data["row"].as<int>();
    col = data["col"].as<int>();
    return true;
  }
  return false;
}

// Copies whatever matrix info a response carries into the live node entry:
// position (nodeRow/nodeCol) always, and the "Matrix.alias" the node
// remembers only when the hub has no alias of its own for this device (so
// an alias set from the app/Group stays the source of truth).
inline void applyNodeMatrix(int index, JsonObjectConst data)
{
  if (index < 0 || index >= Device.deviceCount)
  {
    return;
  }
  int r = 0, c = 0;
  if (readMatrixPos(data, r, c))
  {
    Device.nodes[index].nodeRow = r;
    Device.nodes[index].nodeCol = c;
  }
  JsonVariantConst matrixAlias = data["Matrix"]["alias"];
  const char *a = matrixAlias.is<const char *>() ? matrixAlias.as<const char *>() : nullptr;
  if (a != nullptr && a[0] != '\0' && Device.nodes[index].alias[0] == '\0')
  {
    snprintf(Device.nodes[index].alias, sizeof(Device.nodes[index].alias), "%s", a);
  }
}

// Copies the node's stored Group/location name echo into the live entry,
// same pattern as applyNodeCardType() - only overwrites when the response
// actually carries a non-empty top-level "group".
inline void applyNodeGroup(int index, JsonObjectConst data)
{
  if (index < 0 || index >= Device.deviceCount)
  {
    return;
  }
  const char *group = data["group"] | (const char *)nullptr;
  if (group != nullptr && group[0] != '\0')
  {
    snprintf(Device.nodes[index].nodeGroup, sizeof(Device.nodes[index].nodeGroup), "%s", group);
  }
}

// Writes this node's physical board variant into it: "OB" (card onboard) or
// "R" (card rời/removable). Sent as a LIC_CONFIG_DEVICE with only
// "cardType" (no "new_lid"/"Matrix"), so nothing else about the node
// changes. The node side must persist and echo "cardType" back - see
// applyNodeCardType() below and webOnConfigExtraResponse() in local_web.h.
inline bool sendSetCardType(const uint8_t targetMac[6], int id_des, int lid,
                            const char *cardType, uint32_t *requestTime = nullptr)
{
  JsonDocument data;
  data["cardType"] = cardType;
  // includeLidInData=false: same reasoning as sendSetMatrix() above - a
  // CardType-only write must not implicitly change the node's config_lid.
  return sendProtocolMessage(targetMac, id_des, lid, LIC_CONFIG_DEVICE, data, requestTime, false);
}

// Copies the node's stored board-variant echo into the live entry, same
// pattern as applyNodeMatrix() - only overwrites when the response actually
// carries a non-empty "cardType".
inline void applyNodeCardType(int index, JsonObjectConst data)
{
  if (index < 0 || index >= Device.deviceCount)
  {
    return;
  }
  const char *cardType = data["cardType"] | (const char *)nullptr;
  if (cardType != nullptr && cardType[0] != '\0')
  {
    snprintf(Device.nodes[index].cardType, sizeof(Device.nodes[index].cardType), "%s", cardType);
  }
}

// Gửi lệnh SET_LICENSE đến thiết bị đích
inline bool sendDiscoveryScan(const uint8_t targetMac[6], int id_des, int lid,
                              uint32_t scanId, uint8_t round,
                              uint8_t slotCount, uint16_t slotMs,
                              uint16_t scanLimit)
{
  JsonDocument data;
  data["scan_id"] = scanId;
  data["scan_round"] = round;
  data["slot_count"] = slotCount;
  data["slot_ms"] = slotMs;
  data["scan_limit"] = scanLimit;
  return sendProtocolMessage(targetMac, id_des, lid, LIC_GET_LICENSE, data);
}

inline bool set_license(const uint8_t targetMac[6], int id_des, int lid,
                        uint32_t created, uint32_t duration, uint8_t expired,
                        int expiredLedMode = -1, long expiredCycleMinMinutes = -1,
                        long expiredCycleMaxMinutes = -1, uint32_t *requestTime = nullptr)
{
  JsonDocument data;
  data["created"] = created;
  data["duration"] = duration;
  data["expired"] = expired;
  // -1 = khong gui truong nay, node tu giu nguyen kieu LED het han dang luu.
  if (expiredLedMode >= 0)
  {
    data["expiredLedMode"] = expiredLedMode;
  }
  // Chi co y nghia khi expiredLedMode == 3 (Random theo chu ky) - node bo
  // qua neu khong o mode do, nhung van gui khi app co truyen len.
  if (expiredCycleMinMinutes >= 0 && expiredCycleMaxMinutes >= 0)
  {
    data["expiredCycleMinMinutes"] = expiredCycleMinMinutes;
    data["expiredCycleMaxMinutes"] = expiredCycleMaxMinutes;
  }
  return sendProtocolMessage(targetMac, id_des, lid, LIC_SET_LICENSE, data, requestTime);
}

inline bool sendConfigDevice(const uint8_t targetMac[6], int id_des, const char *newLid,
                             uint32_t *requestTime = nullptr)
{
  JsonDocument data;
  data["new_lid"] = newLid;
  return sendProtocolMessage(targetMac, id_des, 0, LIC_CONFIG_DEVICE, data,
                             requestTime, false);
}

// Some node firmware versions return LID as a JSON string (for example
// "lid":"113"), while others return it as a number. A few legacy versions
// also put it at the root instead of inside data. ArduinoJson's `variant | 0`
// only accepts an actual JSON integer, so read through `as<int>()` after
// selecting the available field; this converts both numeric representations.
inline int readProtocolLid(const JsonDocument &message)
{
  JsonVariantConst lid = message["data"]["lid"];
  if (lid.isNull())
  {
    lid = message["lid"];
  }
  return lid.isNull() ? 0 : lid.as<int>();
}

inline uint32_t readProtocolUint32(JsonVariantConst value, uint32_t fallback = 0)
{
  return value.isNull() ? fallback : value.as<uint32_t>();
}

inline bool verifyProtocolAuth(JsonDocument &message)
{
  const char *sourceUid = message["id_src"];
  const char *macSrc = message["mac_src"];
  const char *macDes = message["mac_des"];
  const char *receivedAuth = message["auth"];
  if (sourceUid == nullptr || macSrc == nullptr || macDes == nullptr ||
      receivedAuth == nullptr || message["data"].isNull() || message["time"].isNull())
  {
    return false;
  }

  JsonObject data = message["data"].as<JsonObject>();
  const int lid = readProtocolLid(message);
  const String idDes = message["id_des"].as<String>();
  const uint8_t opcode = message["opcode"] | 0;
  const uint32_t timestamp = message["time"] | 0UL;
  String dataText;
  serializeJson(data, dataText);
  const String calculated = md5Hash(sourceUid, lid, idDes, macSrc, macDes,
                                    opcode, dataText, timestamp);
  return calculated.equalsIgnoreCase(receivedAuth);
}

inline void processReceivedData(JsonDocument &message, const uint8_t mac[6])
{
  if (!verifyProtocolAuth(message))
  {
    Serial.println("Invalid ESP-NOW protocol auth");
    return;
  }

  const uint8_t opcode = message["opcode"] | 0;
  const uint32_t requestTime = message["time"] | 0UL;
  JsonObject data = message["data"].as<JsonObject>();
  const int lid = readProtocolLid(message);

  if (opcode == (LIC_GET_LICENSE | 0x80))
  {
    const char *uid = message["id_src"];
    const uint32_t remain = readProtocolUint32(data["remain"]);
    const int status = data["status"] | 0;
    const int deviceId = data["id"] | 0;
    const int deviceLimit = data["nod"] | 0;
    // A node that has stored a matrix position/alias/card type reports it
    // right in its scan reply - addOrUpdateNode() applies Matrix/cardType
    // from `data` itself (see local_web.h) before it broadcasts the node,
    // so the app's very first device.upsert for a freshly-discovered node
    // already carries them instead of a stale zeroed value that silently
    // never gets corrected until some later, unrelated broadcast happens to
    // fire for the same node.
    const int idx = addOrUpdateNode(uid, mac, lid, remain, status, deviceId, deviceLimit, data);
    webOnLicenseResponse(opcode, mac, lid, requestTime, data);
    return;
  }

  // A Set Matrix (JOB_MATRIX) or Set Card Type (JOB_CARD_TYPE) job sends a
  // LIC_CONFIG_DEVICE carrying only "Matrix"/"cardType" and gets a config
  // ack back — route it to the shared handler so it never falls through
  // webOnLicenseResponse()'s generic block, which would write node.lid from
  // whatever (possibly absent) lid the ack has.
  if (opcode == (LIC_CONFIG_DEVICE | 0x80) && masterJobWantsConfigExtras())
  {
    webOnConfigExtraResponse(mac, requestTime, data);
    return;
  }

  if (opcode == LIC_INFO_RESPONSE || opcode == LIC_INFO_RESPONSE_LEGACY)
  {
    // Handled entirely separately from webOnLicenseResponse(): its payload
    // schema (device_id/voltage_v/temperature_c/uptime_m/...) doesn't
    // overlap with the license fields (lid/remain/status/id/nod) that
    // generic handler writes on any non-GET_LICENSE opcode, so routing it
    // through there would silently corrupt a node's real lid/status with
    // whatever LIC_INFO's unrelated "lid"/"status" placeholders happen to
    // be (e.g. a non-numeric "lid":"DEFAULTLID" parses to 0).
    webOnInfoResponse(mac, requestTime, data);
    return;
  }

  webOnLicenseResponse(opcode, mac, lid, requestTime, data);
}

#endif // PROTOCOL_HANDLER_H
