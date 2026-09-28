#ifndef WEB_BRIDGE_H
#define WEB_BRIDGE_H

#include <Arduino.h>
#include <ArduinoJson.h>

// Bridge from the ESP-NOW protocol decoder to the local web job manager.
void webOnLicenseResponse(uint8_t opcode, const uint8_t *mac_addr, int lid,
                          uint32_t requestTime, JsonObject data);
// `data` is the GET_LICENSE ack's full data object - besides remain/status/
// id/nod (already unpacked into the params above), addOrUpdateNode() also
// reads any "Matrix"/"cardType" out of it (see protocol_handler.h's
// applyNodeMatrix()/applyNodeCardType()) and applies them before its own
// broadcast, so a freshly-discovered node's very first device.upsert
// already reflects them.
int addOrUpdateNode(const char *uid, const uint8_t *mac_addr, int lid,
                    uint32_t remain, int status, int deviceId, int deviceLimit,
                    JsonObjectConst data);
void webOnInfoResponse(const uint8_t *mac_addr, uint32_t requestTime, JsonObject data);
// Shared handler for a LIC_CONFIG_DEVICE ack that carries "Matrix" and/or
// "cardType" instead of (or alongside) "new_lid" - see Set Matrix / Set Card
// Type in local_web.h.
void webOnConfigExtraResponse(const uint8_t *mac_addr, uint32_t requestTime, JsonObject data);
bool masterJobWantsConfigExtras();  // true while a Set Matrix/Card Type job is running

#endif // WEB_BRIDGE_H
