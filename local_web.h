#ifndef LOCAL_WEB_H
#define LOCAL_WEB_H

#include <Arduino.h>
#include <ArduinoJson.h>
#include <Preferences.h>
#include <WebServer.h>
#include <WebSocketsServer.h>
#include <WiFi.h>

#include "config.h"
#include "espnow_group.h"
#include "logo/images.h"
#include "local_web_page.h"
#include "protocol_handler.h"

constexpr uint8_t LOCAL_WIFI_CHANNEL = 1;
constexpr uint8_t MAX_JOB_TARGETS = MAX_DEVICES;
constexpr uint8_t MAX_GROUPS = 10;
constexpr uint8_t MAX_REQUEST_ATTEMPTS = 3;
constexpr uint32_t RESPONSE_TIMEOUT_MS = 3500;
// Refresh gets only ONE unicast attempt per node per pass before moving on
// (unlike Set/Config's MAX_REQUEST_ATTEMPTS retries within RESPONSE_TIMEOUT_MS
// each) - a genuinely-online node whose reply round-trip (radio + its own
// processing time) runs a bit long under load can miss this window every
// single pass and get reported as offline even though it would have
// answered given a little more time. Widened from 450ms/3 passes to give
// borderline-latency nodes more headroom before Refresh gives up on them.
constexpr uint32_t REFRESH_RESPONSE_TIMEOUT_MS = 700;
constexpr uint8_t REFRESH_MAX_PASSES = 4;
constexpr uint32_t SCAN_RESPONSE_WINDOW_MS = 5500;
// >= MAX_DEVICES so a full-fleet scan (up to 100 boards) always has at
// least as many reply slots as there are boards - below that, the
// pigeonhole principle *guarantees* collisions every round regardless of
// randomness (100 boards into 64 slots forces >= 36 boards to double up on
// some slot's timing). This used to sit at 64, which is why a scan kept
// losing a handful of boards even after nodes got proper random-slot
// replies (see rcv/protocol_handler.h's PendingScanReply): e.g. 70 boards
// into 64 slots forces >= 6 collisions on EVERY round, not just the first,
// because the re-broadcast below can only go out as another broadcast to
// everyone (ESP-NOW has no way to unicast-invite "just the still-missing
// MACs" before they're even known) - so all 70 re-roll into the same 64
// slots again next round too.
constexpr uint8_t SCAN_RESPONSE_SLOT_COUNT = 128;
// Kept low enough that SCAN_RESPONSE_SLOT_COUNT * SCAN_RESPONSE_SLOT_MS
// (the latest possible reply delay) stays comfortably under
// SCAN_RESPONSE_WINDOW_MS, with margin for propagation + the Hub's own
// receive-queue drain: 128 * 20 = 2560 ms vs a 5500 ms window.
constexpr uint16_t SCAN_RESPONSE_SLOT_MS = 20;
constexpr uint16_t DEFAULT_SCAN_LIMIT = 15;
// With SCAN_RESPONSE_SLOT_COUNT random reply slots, two or more nodes can
// still pick the same slot and collide on air, silently losing both
// replies for that round (birthday-paradox odds climb fast). A single
// broadcast can't target just the still-missing nodes (see above), so once
// a round's window closes without reaching targetCount we re-broadcast
// (same scanId, next round) to have EVERY node re-roll a slot — independent
// rounds make near-total collisions across all of them unlikely.
constexpr uint8_t SCAN_MAX_ROUNDS = 4;
constexpr size_t GROUP_NAME_LENGTH = 40;
constexpr size_t GROUP_MEMBER_KEY_LENGTH = 18;
constexpr uint8_t GROUP_STORAGE_VERSION = 4;
constexpr const char *GROUP_STORAGE_NAMESPACE = "hub66_groups";
constexpr const char *LEGACY_GROUP_STORAGE_PARTITION = "nvs";
constexpr const char *PERSISTENT_STORAGE_PARTITION = "nvsnodes";
constexpr const char *NODE_STORAGE_NAMESPACE = "hub66_nodes";
constexpr uint8_t NODE_STORAGE_VERSION = 1;

struct __attribute__((packed)) StoredNodeInfo
{
  uint8_t mac[6];
  char uid[16];
  int32_t deviceId;
  int32_t lid;
  uint32_t remain;
  int32_t deviceLimit;
  int32_t protocolStatus;
};

struct __attribute__((packed)) StoredGroupDeviceInfo
{
  uint8_t mac[6];
  char uid[16];
  int32_t deviceId;
  int32_t lid;
  uint32_t remain;
  int32_t deviceLimit;
  int32_t protocolStatus;
  char alias[DEVICE_ALIAS_LENGTH];
};

enum MasterJobType : uint8_t
{
  JOB_NONE,
  JOB_REFRESH,
  JOB_SEARCH,
  JOB_SET,
  JOB_CONFIG,
  JOB_INFO,
  JOB_MATRIX,
  JOB_CARD_TYPE
};

struct MasterJob
{
  MasterJobType type;
  uint8_t targets[MAX_JOB_TARGETS][6];
  node_info_t targetNodes[MAX_JOB_TARGETS];
  uint16_t total;
  uint16_t current;
  uint8_t attempts;
  uint32_t sentAt;
  uint32_t requestTime;
  bool waiting;
  bool scanAfter;
  bool useNodeLid;
  int lid;
  char newLid[12];
  uint32_t created;
  uint32_t durationMinutes;
  uint8_t expired;
  int expiredLedMode; // -1 = khong gui (giu nguyen o node); 0/1/2/3 = Random/Tat het/Nhap nhay 2s/Random theo chu ky
  long expiredCycleMinMinutes; // -1 = khong gui - chi dung khi expiredLedMode == 3
  long expiredCycleMaxMinutes;
  uint16_t successCount;
  uint16_t noResponseCount;
  uint16_t sendFailureCount;
  uint8_t retryTargets[MAX_JOB_TARGETS][6];
  node_info_t retryNodes[MAX_JOB_TARGETS];
  uint16_t retryTotal;
  uint16_t passTotal;
  uint16_t completedAttempts;
  uint16_t firstPassTotal;
  uint8_t refreshPass;
  uint32_t droppedAtStart;
};

struct ScanState
{
  bool active;
  uint32_t sentAt;
  uint32_t scanId;
  uint16_t targetCount;
  uint16_t responseCount;
  uint8_t seenMacs[MAX_DEVICES][6];
  uint16_t seenCount;
  uint32_t droppedAtStart;
  uint8_t round;
};

struct GroupInfo
{
  uint32_t id;
  char name[GROUP_NAME_LENGTH];
  uint8_t memberCount;
  char members[MAX_DEVICES][GROUP_MEMBER_KEY_LENGTH];
};

struct LegacyGroupInfo
{
  uint32_t id;
  char name[GROUP_NAME_LENGTH];
  uint8_t memberCount;
  char members[MAX_DEVICES][16];
};

static WebServer localHttpServer(80);
static WebSocketsServer localWebSocket(81);
static MasterJob masterJob = {};
static ScanState scanState = {};
static GroupInfo groups[MAX_GROUPS] = {};
static uint8_t groupCount = 0;
// Official Group member catalog. Unlike Device (All Devices), this catalog is
// persisted and is never copied into the temporary scan list on boot.
static node_info_t groupDevices[MAX_DEVICES] = {};
static uint8_t groupDeviceCount = 0;
static StoredNodeInfo legacyStoredNodes[MAX_DEVICES] = {};
static GroupInfo groupBackup[MAX_GROUPS] = {};
static node_info_t groupDeviceBackup[MAX_DEVICES] = {};
static LegacyGroupInfo legacyGroups[MAX_GROUPS] = {};
static char localApSsid[32] = {};
static bool localWebStarted = false;

#include "account_auth.h"

enum WebAuthState : uint8_t
{
  AUTH_SIGNED_OUT,
  AUTH_WAITING_OTP,
  AUTH_SIGNED_IN
};

// The USB serial link is treated as one extra pseudo "client" so it can reuse
// webHandleCommand()/webSendJson()/webBroadcastJson() as-is instead of a
// second copy of the dispatch logic. It gets the slot right after the real
// WebSocket client range and requires the same auth.login as a WebSocket
// client (see beginUsbBridge()) — it starts AUTH_SIGNED_OUT like any other
// client, so the app must sign in with a real account either way.
constexpr uint8_t USB_CLIENT_ID = WEBSOCKETS_SERVER_CLIENT_MAX;

static WebAuthState webAuthState[USB_CLIENT_ID + 1] = {};
static int8_t webAuthAccount[USB_CLIENT_ID + 1] = {};
static uint8_t webPendingTotp[USB_CLIENT_ID + 1][ACCOUNT_TOTP_LENGTH] = {};
static bool webHasPendingTotp[USB_CLIENT_ID + 1] = {};

inline bool webLicenseJobBusy();
static void webSendState(uint8_t client);
static bool groupHasMember(const GroupInfo &group, const char *memberKey);
static bool parseMac(const char *value, uint8_t out[6]);
static void recordScanResponse(const uint8_t mac[6]);

static int findGroupDeviceIndexByMac(const uint8_t mac[6])
{
  if (mac == nullptr)
  {
    return -1;
  }
  for (uint8_t i = 0; i < groupDeviceCount; ++i)
  {
    if (memcmp(groupDevices[i].mac, mac, 6) == 0)
    {
      return i;
    }
  }
  return -1;
}

static int findGroupDeviceIndexByUid(const char *uid)
{
  if (uid == nullptr || uid[0] == '\0')
  {
    return -1;
  }
  for (uint8_t i = 0; i < groupDeviceCount; ++i)
  {
    if (strcasecmp(groupDevices[i].uid, uid) == 0)
    {
      return i;
    }
  }
  return -1;
}

static void copyNodeDetails(node_info_t &target, const node_info_t &source)
{
  target = source;
}

static bool upsertGroupDevice(const node_info_t &source)
{
  int index = findGroupDeviceIndexByMac(source.mac);
  if (index < 0)
  {
    if (groupDeviceCount >= MAX_DEVICES)
    {
      return false;
    }
    index = groupDeviceCount++;
  }
  copyNodeDetails(groupDevices[index], source);
  return true;
}

static int findLegacyStoredNodeByMac(const uint8_t mac[6])
{
  for (uint8_t i = 0; i < MAX_DEVICES; ++i)
  {
    if (memcmp(legacyStoredNodes[i].mac, mac, 6) == 0)
    {
      return i;
    }
  }
  return -1;
}

static int findLegacyStoredNodeByUid(const char *uid)
{
  if (uid == nullptr || uid[0] == '\0')
  {
    return -1;
  }
  for (uint8_t i = 0; i < MAX_DEVICES; ++i)
  {
    if (strcasecmp(legacyStoredNodes[i].uid, uid) == 0)
    {
      return i;
    }
  }
  return -1;
}

static node_info_t nodeFromStored(const StoredNodeInfo &saved)
{
  node_info_t node = {};
  memcpy(node.mac, saved.mac, 6);
  snprintf(node.uid, sizeof(node.uid), "%s", saved.uid);
  node.deviceId = saved.deviceId;
  node.lid = saved.lid;
  node.remain = saved.remain;
  node.deviceLimit = saved.deviceLimit;
  node.protocolStatus = saved.protocolStatus;
  node.responseState = NODE_NO_RESPONSE;
  return node;
}

static node_info_t nodeFromStored(const StoredGroupDeviceInfo &saved)
{
  node_info_t node = {};
  memcpy(node.mac, saved.mac, 6);
  snprintf(node.uid, sizeof(node.uid), "%s", saved.uid);
  snprintf(node.alias, sizeof(node.alias), "%s", saved.alias);
  node.deviceId = saved.deviceId;
  node.lid = saved.lid;
  node.remain = saved.remain;
  node.deviceLimit = saved.deviceLimit;
  node.protocolStatus = saved.protocolStatus;
  node.responseState = NODE_NO_RESPONSE;
  return node;
}

static void loadLegacyNodeCatalogForMigration()
{
  memset(legacyStoredNodes, 0, sizeof(legacyStoredNodes));
  Preferences storage;
  if (!storage.begin(NODE_STORAGE_NAMESPACE, true, PERSISTENT_STORAGE_PARTITION))
  {
    return;
  }
  const uint8_t version = storage.getUChar("version", 0);
  const uint8_t count = storage.getUChar("count", 0);
  const size_t size = storage.getBytesLength("items");
  if (version == NODE_STORAGE_VERSION && count <= MAX_DEVICES &&
      size == count * sizeof(StoredNodeInfo) && count > 0)
  {
    storage.getBytes("items", legacyStoredNodes, size);
    for (uint8_t i = 0; i < count; ++i)
    {
      legacyStoredNodes[i].uid[sizeof(legacyStoredNodes[i].uid) - 1] = '\0';
    }
  }
  storage.end();
}

static bool groupReferencesMac(const uint8_t mac[6])
{
  char macText[GROUP_MEMBER_KEY_LENGTH];
  formatMacAddress(mac, macText);
  for (uint8_t i = 0; i < groupCount; ++i)
  {
    if (groupHasMember(groups[i], macText))
    {
      return true;
    }
  }
  return false;
}

static void pruneUnusedGroupDevices()
{
  for (uint8_t i = 0; i < groupDeviceCount;)
  {
    // Keep the catalog entry as long as either a saved Group still
    // references this MAC, or it's just carrying a custom alias for a
    // device that was never added to any Group — that's the only thing
    // that lets alias.save's changes survive a reboot (see saveAlias()).
    if (groupReferencesMac(groupDevices[i].mac) || groupDevices[i].alias[0] != '\0')
    {
      ++i;
      continue;
    }
    for (uint8_t p = i; p + 1 < groupDeviceCount; ++p)
    {
      groupDevices[p] = groupDevices[p + 1];
    }
    groupDevices[--groupDeviceCount] = {};
  }
}

// Serializes `doc` into a single buffer sized to fit exactly, instead of
// through an Arduino String (the old webSendJson()/webBroadcastJson() both
// did `serializeJson(doc, output)` into a growing String). ArduinoJson's
// String writer appends in small (ARDUINOJSON_STRING_BUFFER_SIZE, 64-byte)
// chunks via String::concat(), and Arduino String grows by reallocating +
// copying the ENTIRE string so far on every chunk - for a document this
// size (a full state.get with 70+ devices can be >25KB) that is dozens of
// ever-bigger reallocations. On this board (no PSRAM, ~320KB total RAM
// shared with the WiFi/ESP-NOW/WebSocket/HTTP stacks) that fragments the
// heap badly enough to fail the final allocation well before the actual
// free-heap-byte total would predict, which used to make the client
// silently receive a cut-off, unparseable line ("[SEND] CANH BAO: JSON bi
// cat cut..." - state.get/groups.changed making every known device/Group
// "disappear" with no error anywhere once the fleet got large enough).
// measureJson() gives the exact final size up front, so ONE malloc() of
// that size (no growth, no fragmentation from repeated copies) either
// succeeds outright or fails cleanly - nothing in between.
//
// Returns the malloc'd buffer (caller must free() it) and its length via
// `outLength`, or nullptr if the allocation itself failed (logged here).
static char *serializeJsonToBuffer(JsonDocument &doc, size_t &outLength)
{
  const size_t measured = measureJson(doc);
  char *buf = static_cast<char *>(malloc(measured + 1));
  if (buf == nullptr)
  {
    Serial.printf("[SEND] LOI: khong cap phat duoc %u byte de gui JSON (freeHeap=%u) - bo qua goi tin nay\n",
                  static_cast<unsigned>(measured + 1), ESP.getFreeHeap());
    outLength = 0;
    return nullptr;
  }
  outLength = serializeJson(doc, buf, measured + 1);
  return buf;
}

static void webSendJson(uint8_t client, JsonDocument &doc)
{
  size_t length = 0;
  char *buf = serializeJsonToBuffer(doc, length);
  if (buf == nullptr)
  {
    return;
  }
  if (client == USB_CLIENT_ID)
  {
    Serial.write(reinterpret_cast<const uint8_t *>(buf), length);
    Serial.write('\n');
  }
  else
  {
    localWebSocket.sendTXT(client, buf, length);
  }
  free(buf);
}

static void webBroadcastJson(JsonDocument &doc)
{
  size_t length = 0;
  char *buf = serializeJsonToBuffer(doc, length);
  if (buf == nullptr)
  {
    return;
  }
  for (uint8_t client = 0; client < WEBSOCKETS_SERVER_CLIENT_MAX; ++client)
  {
    if (webAuthState[client] == AUTH_SIGNED_IN)
    {
      localWebSocket.sendTXT(client, buf, length);
    }
  }
  if (webAuthState[USB_CLIENT_ID] == AUTH_SIGNED_IN)
  {
    Serial.write(reinterpret_cast<const uint8_t *>(buf), length);
    Serial.write('\n');
  }
  free(buf);
}

static void webSendError(uint8_t client, const char *code, const char *message)
{
  JsonDocument doc;
  doc["type"] = "error";
  doc["code"] = code;
  doc["message"] = message;
  webSendJson(client, doc);
}

static void webSendAck(uint8_t client, const char *action)
{
  JsonDocument doc;
  doc["type"] = "ack";
  doc["action"] = action;
  webSendJson(client, doc);
}

static void webSendAuthRequired(uint8_t client)
{
  JsonDocument doc;
  doc["type"] = "auth.required";
  webSendJson(client, doc);
}

static void appendAccounts(JsonArray list)
{
  for (uint8_t i = 0; i < accountCount; ++i)
  {
    JsonObject item = list.add<JsonObject>();
    item["username"] = accounts[i].username;
    item["twoFactorEnabled"] = accounts[i].totpEnabled != 0;
  }
}

static void webSendAccounts(uint8_t client)
{
  JsonDocument doc;
  doc["type"] = "accounts";
  appendAccounts(doc["accounts"].to<JsonArray>());
  webSendJson(client, doc);
}

static void webBroadcastAccounts()
{
  JsonDocument doc;
  doc["type"] = "accounts";
  appendAccounts(doc["accounts"].to<JsonArray>());
  webBroadcastJson(doc);
}

static void webFinishLogin(uint8_t client, int accountIndex)
{
  webAuthState[client] = AUTH_SIGNED_IN;
  webAuthAccount[client] = accountIndex;
  JsonDocument doc;
  doc["type"] = "auth.success";
  doc["username"] = accounts[accountIndex].username;
  webSendJson(client, doc);
  webSendState(client);
}

static void webHandleLogin(uint8_t client, JsonObject data)
{
  const int accountIndex = findAccount(data["username"] | "");
  if (accountIndex < 0 || !checkAccountPassword(accounts[accountIndex], data["password"] | ""))
  {
    webSendError(client, "LOGIN_FAILED", "Invalid username or password");
    return;
  }
  accountEpoch(data["epoch"] | 0UL);
  if (accounts[accountIndex].totpEnabled)
  {
    webAuthState[client] = AUTH_WAITING_OTP;
    webAuthAccount[client] = accountIndex;
    JsonDocument doc;
    doc["type"] = "auth.otp_required";
    webSendJson(client, doc);
    return;
  }
  webFinishLogin(client, accountIndex);
}

static void webVerifyLoginOtp(uint8_t client, JsonObject data)
{
  const int index = webAuthAccount[client];
  if (webAuthState[client] != AUTH_WAITING_OTP || index < 0 || index >= accountCount ||
      !checkTotp(accounts[index].totpSecret, data["code"] | "",
                 accountEpoch(data["epoch"] | 0UL)))
  {
    webSendError(client, "OTP_FAILED", "Invalid authentication code");
    return;
  }
  webFinishLogin(client, index);
}

static void webHandleAccountCommand(uint8_t client, const char *action, JsonObject data)
{
  if (strcmp(action, "account.list") == 0)
  {
    webSendAccounts(client);
  }
  else if (strcmp(action, "account.create") == 0)
  {
    if (!createAccount(data["username"] | "", data["password"] | ""))
    {
      webSendError(client, "ACCOUNT_CREATE_FAILED", "Username is invalid, duplicated, or account list is full");
      return;
    }
    webSendAck(client, action);
    webBroadcastAccounts();
  }
  else if (strcmp(action, "account.password") == 0)
  {
    if (!changeAccountPassword(data["username"] | "", data["password"] | ""))
    {
      webSendError(client, "PASSWORD_FAILED", "Password must contain at least 6 characters");
      return;
    }
    webSendAck(client, action);
  }
  else if (strcmp(action, "account.delete") == 0)
  {
    const int removed = findAccount(data["username"] | "");
    if (removed == webAuthAccount[client])
    {
      webSendError(client, "DELETE_FAILED", "The current account cannot be deleted");
      return;
    }
    if (removed < 0 || !deleteAccount(data["username"] | ""))
    {
      webSendError(client, "DELETE_FAILED", "The last account cannot be deleted");
      return;
    }
    for (uint8_t peer = 0; peer <= USB_CLIENT_ID; ++peer)
    {
      if (webAuthAccount[peer] == removed)
      {
        webAuthState[peer] = AUTH_SIGNED_OUT;
        webAuthAccount[peer] = -1;
      }
      else if (webAuthAccount[peer] > removed)
      {
        --webAuthAccount[peer];
      }
    }
    webSendAck(client, action);
    webBroadcastAccounts();
  }
  else if (strcmp(action, "account.2fa.setup") == 0)
  {
    if (webAuthAccount[client] < 0 || webAuthAccount[client] >= accountCount)
    {
      // No specific account is signed in on this client (e.g. the USB link,
      // which is pre-authenticated without picking an account) — 2FA is
      // per-account, so there is nothing to attach it to.
      webSendError(client, "NO_ACCOUNT", "Sign in to a specific account before enabling 2FA");
      return;
    }
    esp_fill_random(webPendingTotp[client], ACCOUNT_TOTP_LENGTH);
    webHasPendingTotp[client] = true;
    const char *username = accounts[webAuthAccount[client]].username;
    if (!makeAccountQr(username, webPendingTotp[client]))
    {
      webHasPendingTotp[client] = false;
      webSendError(client, "QR_FAILED", "Could not create QR code");
      return;
    }
    JsonDocument doc;
    doc["type"] = "account.2fa.setup";
    doc["secret"] = base32Secret(webPendingTotp[client], ACCOUNT_TOTP_LENGTH);
    doc["qrSize"] = accountQrSize;
    doc["qrData"] = accountQrData;
    webSendJson(client, doc);
  }
  else if (strcmp(action, "account.2fa.confirm") == 0)
  {
    if (!webHasPendingTotp[client] ||
        !checkTotp(webPendingTotp[client], data["code"] | "",
                   accountEpoch(data["epoch"] | 0UL)) ||
        !enableAccountTotp(accounts[webAuthAccount[client]].username,
                           webPendingTotp[client]))
    {
      webSendError(client, "OTP_FAILED", "Invalid authentication code");
      return;
    }
    webHasPendingTotp[client] = false;
    webSendAck(client, action);
    webBroadcastAccounts();
  }
  else if (strcmp(action, "account.2fa.disable") == 0)
  {
    if (!disableAccountTotp(data["username"] | ""))
    {
      webSendError(client, "TWO_FACTOR_FAILED", "Account was not found");
      return;
    }
    webSendAck(client, action);
    webBroadcastAccounts();
  }
  else
  {
    webSendError(client, "UNKNOWN_ACTION", "Unsupported account command");
  }
}

static void appendNodeInfo(JsonObject item, const node_info_t &node)
{
  char mac[18];
  formatMacAddress(node.mac, mac);
  item["id_src"] = node.uid;
  item["uid"] = node.uid;
  // Keep the empty field in upsert events so deleting an alias clears it in the browser.
  item["alias"] = node.alias;
  item["deviceId"] = node.deviceId;
  item["mac"] = mac;
  item["lid"] = node.lid;
  item["remain"] = node.remain;
  item["protocolStatus"] = node.protocolStatus;
  item["responseState"] = nodeResponseStateName(node.responseState);
  item["numberOfDevices"] = node.deviceLimit;
  item["lastSeenAgoMs"] = node.lastResponseMillis == 0 ? 0 : millis() - node.lastResponseMillis;
  // Only present once a LIC_INFO_RESPONSE has actually been seen for this
  // node (see webOnInfoResponse()), so the app doesn't render misleading
  // 0.0V/0.0°C placeholders for nodes that were never asked for INFO.
  if (node.lastInfoMillis != 0)
  {
    item["firmwareVersion"] = node.firmwareVersion;
    item["voltageV"] = node.voltageV;
    item["temperatureC"] = node.temperatureC;
    item["uptimeMinutes"] = node.uptimeMinutes;
    item["infoStatus"] = node.infoStatus;
    item["linkProtocol"] = node.linkProtocol;
    item["infoAgoMs"] = millis() - node.lastInfoMillis;
  }
  // Matrix position the node has stored for itself (0 = never set). Sent
  // always so the app can compare it to the card's current grid slot.
  item["nodeRow"] = node.nodeRow;
  item["nodeCol"] = node.nodeCol;
  // Board variant the node has stored for itself ("OB"/"R", "" = never
  // set). Sent always so the app knows which board photo to show.
  item["cardType"] = node.cardType;
  // Group/location name the node has stored for itself ("" = never set).
  // Sent always so the app can flag a board sitting in the wrong Group's
  // matrix grid even before the next Set Matrix.
  item["nodeGroup"] = node.nodeGroup;
}

static void appendNode(JsonObject item, int index)
{
  appendNodeInfo(item, Device.nodes[index]);
}

static void broadcastNode(int index)
{
  if (index < 0 || index >= Device.deviceCount)
  {
    return;
  }
  JsonDocument event;
  event["type"] = "device.upsert";
  appendNode(event["data"].to<JsonObject>(), index);
  webBroadcastJson(event);
}

static int findGroupIndex(uint32_t id)
{
  for (uint8_t i = 0; i < groupCount; ++i)
  {
    if (groups[i].id == id)
    {
      return i;
    }
  }
  return -1;
}

static bool groupHasMember(const GroupInfo &group, const char *memberKey)
{
  if (memberKey == nullptr || memberKey[0] == '\0')
  {
    return false;
  }
  for (uint8_t i = 0; i < group.memberCount; ++i)
  {
    if (strcasecmp(group.members[i], memberKey) == 0)
    {
      return true;
    }
  }
  return false;
}

static bool saveGroupsToNvs()
{
  Preferences storage;
  if (!storage.begin(GROUP_STORAGE_NAMESPACE, false, PERSISTENT_STORAGE_PARTITION))
  {
    Serial.println("Failed to open Group NVS for writing");
    return false;
  }

  bool saved = true;
  if (groupCount == 0)
  {
    storage.remove("items");
  }
  else
  {
    const size_t size = groupCount * sizeof(GroupInfo);
    const size_t written = storage.putBytes("items", groups, size);
    saved = written == size;
    if (!saved)
    {
      // GroupInfo is a fixed sizeof() regardless of how many of its 100
      // member slots are actually used, so this blob's size only grows
      // with groupCount (number of Groups), not with how many members are
      // in any one of them - but NVS still caps how big a single blob can
      // be, so enough Groups (or one with a very roomy struct) can still
      // hit that ceiling. Logged because putBytes() failing here silently
      // rolls back the *entire* group.save the caller was doing.
      Serial.printf("[GROUP] Ghi NVS 'items' that bai: can %u byte (groupCount=%u x %u byte/group), "
                    "chi ghi duoc %u byte\n",
                    static_cast<unsigned>(size), groupCount, static_cast<unsigned>(sizeof(GroupInfo)),
                    static_cast<unsigned>(written));
    }
  }
  if (groupDeviceCount == 0)
  {
    if (storage.isKey("devices"))
    {
      saved = storage.remove("devices") && saved;
    }
  }
  else
  {
    StoredGroupDeviceInfo *persistedGroupDevices =
        static_cast<StoredGroupDeviceInfo *>(
            calloc(groupDeviceCount, sizeof(StoredGroupDeviceInfo)));
    if (persistedGroupDevices == nullptr)
    {
      storage.end();
      Serial.println("Not enough memory to save Group member catalog");
      return false;
    }
    for (uint8_t i = 0; i < groupDeviceCount; ++i)
    {
      memcpy(persistedGroupDevices[i].mac, groupDevices[i].mac, 6);
      snprintf(persistedGroupDevices[i].uid, sizeof(persistedGroupDevices[i].uid), "%s", groupDevices[i].uid);
      persistedGroupDevices[i].deviceId = groupDevices[i].deviceId;
      persistedGroupDevices[i].lid = groupDevices[i].lid;
      persistedGroupDevices[i].remain = groupDevices[i].remain;
      persistedGroupDevices[i].deviceLimit = groupDevices[i].deviceLimit;
      persistedGroupDevices[i].protocolStatus = groupDevices[i].protocolStatus;
      snprintf(persistedGroupDevices[i].alias, sizeof(persistedGroupDevices[i].alias), "%s", groupDevices[i].alias);
    }
    const size_t persistedSize = groupDeviceCount * sizeof(StoredGroupDeviceInfo);
    saved = storage.putBytes("devices", persistedGroupDevices, persistedSize) == persistedSize && saved;
    free(persistedGroupDevices);
  }
  const bool countSaved = storage.putUChar("count", groupCount) == sizeof(uint8_t);
  const bool deviceCountSaved =
      storage.putUChar("device_count", groupDeviceCount) == sizeof(uint8_t);
  const bool versionSaved = storage.putUChar("version", GROUP_STORAGE_VERSION) == sizeof(uint8_t);
  saved = saved && countSaved && deviceCountSaved && versionSaved;
  storage.end();

  if (!saved)
  {
    Serial.println("Failed to save Group List to NVS");
  }
  return saved;
}

static void loadGroupsFromNvs()
{
  memset(groups, 0, sizeof(groups));
  memset(groupDevices, 0, sizeof(groupDevices));
  memset(legacyGroups, 0, sizeof(legacyGroups));
  groupCount = 0;
  groupDeviceCount = 0;
  StoredGroupDeviceInfo *persistedGroupDevices = nullptr;

  Preferences storage;
  if (!storage.begin(GROUP_STORAGE_NAMESPACE, false, PERSISTENT_STORAGE_PARTITION))
  {
    Serial.println("Failed to open Group NVS");
    return;
  }

  const uint8_t version = storage.getUChar("version", 0);
  const uint8_t storedCount = storage.getUChar("count", 0);
  const size_t storedSize = storage.getBytesLength("items");
  const uint8_t storedDeviceCount = storage.getUChar("device_count", 0);
  const size_t storedDeviceSize = storage.getBytesLength("devices");
  const bool legacyMacGroups = version == 3 && storedCount <= MAX_GROUPS &&
                               storedSize == storedCount * sizeof(GroupInfo);
  if ((!legacyMacGroups && version != GROUP_STORAGE_VERSION) || storedCount > MAX_GROUPS ||
      storedSize != storedCount * sizeof(GroupInfo))
  {
    storage.end();
    if (version == 0 && storedCount == 0 && storedSize == 0)
    {
      Preferences legacyStorage;
      loadLegacyNodeCatalogForMigration();
      if (legacyStorage.begin(GROUP_STORAGE_NAMESPACE, true,
                              LEGACY_GROUP_STORAGE_PARTITION))
      {
        const uint8_t legacyVersion = legacyStorage.getUChar("version", 0);
        const uint8_t legacyCount = legacyStorage.getUChar("count", 0);
        const size_t legacySize = legacyStorage.getBytesLength("items");
        if (legacyVersion == 2 && legacyCount <= MAX_GROUPS &&
            legacySize == legacyCount * sizeof(LegacyGroupInfo) &&
            (legacyCount == 0 ||
             legacyStorage.getBytes("items", legacyGroups, legacySize) == legacySize))
        {
          legacyStorage.end();
          for (uint8_t i = 0; i < legacyCount; ++i)
          {
            if (legacyGroups[i].id == 0 || legacyGroups[i].name[0] == '\0' ||
                legacyGroups[i].memberCount > MAX_DEVICES)
            {
              continue;
            }
        GroupInfo &group = groups[groupCount++];
        group.id = legacyGroups[i].id;
        snprintf(group.name, sizeof(group.name), "%s", legacyGroups[i].name);
        for (uint8_t m = 0; m < legacyGroups[i].memberCount; ++m)
        {
          legacyGroups[i].members[m][15] = '\0';
          if (legacyGroups[i].members[m][0] == '\0' ||
              group.memberCount >= MAX_DEVICES)
          {
            continue;
          }
           char memberKey[GROUP_MEMBER_KEY_LENGTH];
           const int nodeIndex = findLegacyStoredNodeByUid(legacyGroups[i].members[m]);
           if (nodeIndex >= 0)
           {
             const node_info_t member = nodeFromStored(legacyStoredNodes[nodeIndex]);
             formatMacAddress(member.mac, memberKey);
             upsertGroupDevice(member);
           }
           else
           {
             // Without a MAC there is no stable official member key.
             continue;
           }
          if (!groupHasMember(group, memberKey))
          {
            snprintf(group.members[group.memberCount++], GROUP_MEMBER_KEY_LENGTH,
                     "%s", memberKey);
          }
        }
          }
          saveGroupsToNvs();
          Preferences legacyNodes;
          if (legacyNodes.begin(NODE_STORAGE_NAMESPACE, false, PERSISTENT_STORAGE_PARTITION))
          {
            legacyNodes.clear();
            legacyNodes.end();
          }
          Serial.printf("Migrated %u Group(s) to MAC keys\n", groupCount);
          return;
        }
        legacyStorage.end();
      }
    }
    else
    {
      Serial.println("Ignoring invalid Group NVS data");
    }
    return;
  }

  if (storedCount > 0 &&
      storage.getBytes("items", groups, storedSize) != storedSize)
  {
    memset(groups, 0, sizeof(groups));
    storage.end();
    Serial.println("Failed to load Group List from NVS");
    return;
  }
  if (!legacyMacGroups && storedDeviceCount > 0)
  {
    persistedGroupDevices = static_cast<StoredGroupDeviceInfo *>(
        calloc(storedDeviceCount, sizeof(StoredGroupDeviceInfo)));
  }
  if (!legacyMacGroups &&
      (storedDeviceCount > MAX_DEVICES ||
       storedDeviceSize != storedDeviceCount * sizeof(StoredGroupDeviceInfo) ||
       (storedDeviceCount > 0 && persistedGroupDevices == nullptr) ||
       (storedDeviceCount > 0 &&
        storage.getBytes("devices", persistedGroupDevices, storedDeviceSize) != storedDeviceSize)))
  {
    memset(groups, 0, sizeof(groups));
    memset(groupDevices, 0, sizeof(groupDevices));
    free(persistedGroupDevices);
    storage.end();
    Serial.println("Failed to load Group member catalog from NVS");
    return;
  }
  storage.end();
  groupDeviceCount = legacyMacGroups ? 0 : storedDeviceCount;

  for (uint8_t i = 0; i < groupDeviceCount; ++i)
  {
    persistedGroupDevices[i].uid[sizeof(persistedGroupDevices[i].uid) - 1] = '\0';
    persistedGroupDevices[i].alias[sizeof(persistedGroupDevices[i].alias) - 1] = '\0';
    groupDevices[i] = nodeFromStored(persistedGroupDevices[i]);
  }
  free(persistedGroupDevices);

  for (uint8_t i = 0; i < storedCount; ++i)
  {
    groups[i].name[GROUP_NAME_LENGTH - 1] = '\0';
    if (groups[i].id == 0 || groups[i].name[0] == '\0' ||
        groups[i].memberCount > MAX_DEVICES)
    {
      continue;
    }
    uint8_t validMembers = 0;
    for (uint8_t m = 0; m < groups[i].memberCount; ++m)
    {
      groups[i].members[m][GROUP_MEMBER_KEY_LENGTH - 1] = '\0';
      if (groups[i].members[m][0] == '\0')
      {
        continue;
      }
      bool duplicateMember = false;
      for (uint8_t p = 0; p < validMembers; ++p)
      {
        duplicateMember |= strcasecmp(groups[i].members[p], groups[i].members[m]) == 0;
      }
      if (!duplicateMember)
      {
        if (validMembers != m)
        {
          snprintf(groups[i].members[validMembers], GROUP_MEMBER_KEY_LENGTH,
                   "%s", groups[i].members[m]);
        }
        validMembers++;
      }
    }
    groups[i].memberCount = validMembers;
    bool duplicate = false;
    for (uint8_t p = 0; p < groupCount; ++p)
    {
      duplicate |= groups[p].id == groups[i].id;
    }
    if (!duplicate)
    {
      groups[groupCount++] = groups[i];
    }
  }
  for (uint8_t i = groupCount; i < MAX_GROUPS; ++i)
  {
    groups[i] = {};
  }
  pruneUnusedGroupDevices();
  if (legacyMacGroups)
  {
    loadLegacyNodeCatalogForMigration();
    // Version 3 already had persistent MAC membership. Import metadata from
    // the old Node List only for MACs referenced by a Group; All Devices stays empty.
    for (uint8_t g = 0; g < groupCount; ++g)
    {
      for (uint8_t m = 0; m < groups[g].memberCount; ++m)
      {
        uint8_t mac[6];
        if (!parseMac(groups[g].members[m], mac))
        {
          continue;
        }
        const int oldIndex = findLegacyStoredNodeByMac(mac);
        if (oldIndex >= 0)
        {
          upsertGroupDevice(nodeFromStored(legacyStoredNodes[oldIndex]));
        }
      }
    }
    saveGroupsToNvs();
    Preferences legacyNodes;
    if (legacyNodes.begin(NODE_STORAGE_NAMESPACE, false, PERSISTENT_STORAGE_PARTITION))
    {
      legacyNodes.clear();
      legacyNodes.end();
    }
  }
  Serial.printf("Loaded %u Group(s) from NVS\n", groupCount);
}

static void appendGroup(JsonObject item, uint8_t index)
{
  const GroupInfo &saved = groups[index];
  item["id"] = saved.id;
  item["name"] = saved.name;
  JsonArray members = item["members"].to<JsonArray>();
  JsonArray memberDevices = item["devices"].to<JsonArray>();
  for (uint8_t i = 0; i < saved.memberCount; ++i)
  {
    members.add(saved.members[i]);
    uint8_t mac[6];
    const bool parsed = parseMac(saved.members[i], mac);
    // A member already in the LIVE flat Device.nodes[] table (which
    // webSendState()/broadcastNode() always send alongside this) doesn't
    // need its full node info repeated here too - the client already has
    // it from that flat list by this same mac. Sending it twice for every
    // member of every Group used to roughly double an already-large
    // payload (rich per-device JSON: firmware/voltage/uptime/Matrix/...),
    // and on this board (no PSRAM) a big enough device+Group count made
    // the combined state.get response run out of heap mid-serialize and
    // get silently truncated into invalid JSON the app just discarded -
    // "everything vanished" with no error anywhere. Only members the Hub
    // doesn't currently have live still need their own entry here, backed
    // by the durable groupDevices[]/NVS catalog (see updateSavedGroupDevice()).
    if (parsed && findDeviceIndexByMac(mac) >= 0)
    {
      continue;
    }
    const int deviceIndex = parsed
                                ? findGroupDeviceIndexByMac(mac)
                                : findGroupDeviceIndexByUid(saved.members[i]);
    if (deviceIndex >= 0)
    {
      appendNodeInfo(memberDevices.add<JsonObject>(), groupDevices[deviceIndex]);
    }
    else
    {
      JsonObject placeholder = memberDevices.add<JsonObject>();
      placeholder["id_src"] = saved.members[i];
      placeholder["uid"] = saved.members[i];
      placeholder["alias"] = "";
      placeholder["mac"] = saved.members[i];
      placeholder["responseState"] = "NO_RESPONSE";
      placeholder["lastSeenAgoMs"] = 0;
    }
  }
  item["memberCount"] = saved.memberCount;
}

static void appendGroups(JsonArray output)
{
  for (uint8_t i = 0; i < groupCount; ++i)
  {
    appendGroup(output.add<JsonObject>(), i);
  }
}

static void broadcastGroups()
{
  JsonDocument event;
  event["type"] = "groups.changed";
  appendGroups(event["groups"].to<JsonArray>());
  webBroadcastJson(event);
}

static void migrateGroupMemberToMac(const char *uid, const uint8_t mac[6])
{
  if (uid == nullptr || uid[0] == '\0')
  {
    return;
  }
  char macText[GROUP_MEMBER_KEY_LENGTH];
  formatMacAddress(mac, macText);
  memcpy(groupBackup, groups, sizeof(groups));
  bool changed = false;
  for (uint8_t g = 0; g < groupCount; ++g)
  {
    GroupInfo &group = groups[g];
    for (uint8_t m = 0; m < group.memberCount;)
    {
      if (strcasecmp(group.members[m], uid) != 0)
      {
        m++;
        continue;
      }
      if (!groupHasMember(group, macText))
      {
        snprintf(group.members[m], GROUP_MEMBER_KEY_LENGTH, "%s", macText);
        m++;
      }
      else
      {
        for (uint8_t p = m; p + 1 < group.memberCount; ++p)
        {
          memcpy(group.members[p], group.members[p + 1], GROUP_MEMBER_KEY_LENGTH);
        }
        group.memberCount--;
        memset(group.members[group.memberCount], 0, GROUP_MEMBER_KEY_LENGTH);
      }
      changed = true;
    }
  }
  if (!changed)
  {
    return;
  }
  if (!saveGroupsToNvs())
  {
    memcpy(groups, groupBackup, sizeof(groups));
    return;
  }
  broadcastGroups();
}

static void updateSavedGroupDevice(const node_info_t &node)
{
  // Gated on actual Group membership again (not just "has a catalog
  // entry"): this fires on every discovery response via addOrUpdateNode(),
  // and each call does a blocking NVS flash write (saveGroupsToNvs()). With
  // the gate removed, aliasing most/all devices meant a Scan triggered a
  // flash write per aliased device in rapid succession, stalling the main
  // loop long enough to drop other nodes' concurrent responses — scan
  // counts became flaky (5/6/7/8 out of N) even with everything powered.
  // The alias itself doesn't need this: saveAlias() already does its own
  // one-time NVS write right when the operator sets it. This only costs
  // letting an alias-only (non-Group) entry's lid/remain go stale in the
  // persisted catalog, which nothing currently displays anyway.
  const int savedIndex = findGroupDeviceIndexByMac(node.mac);
  if (savedIndex < 0 || !groupReferencesMac(node.mac))
  {
    return;
  }
  char alias[DEVICE_ALIAS_LENGTH];
  snprintf(alias, sizeof(alias), "%s", groupDevices[savedIndex].alias);
  copyNodeDetails(groupDevices[savedIndex], node);
  if (alias[0] != '\0')
  {
    snprintf(groupDevices[savedIndex].alias, sizeof(groupDevices[savedIndex].alias), "%s", alias);
  }
  if (saveGroupsToNvs())
  {
    broadcastGroups();
  }
}

int addOrUpdateNode(const char *uid, const uint8_t *mac, int lid,
                     uint32_t remain, int status, int deviceId, int deviceLimit,
                     JsonObjectConst data)
{
  if (mac == nullptr)
  {
    return -1;
  }
  int index = findDeviceIndexByMac(mac);
  const bool isNewNode = index < 0;
  if (scanState.active && index < 0)
  {
    // Used to reject any newly-discovered device once seenCount reached
    // the requested "Device Count", on the assumption that meant everyone
    // expected had already answered. That assumption breaks the moment
    // there's more devices actually in radio range than the operator
    // typed in (extra/leftover boards, a miscount) or discovery order just
    // happens to bring a straggler in late: whichever devices' random
    // scan slots happen to land first fill the quota, and a device that
    // was always going to answer - just a little later - gets silently
    // dropped instead of counted. Setting "Device Count" higher than the
    // real total worked around this by making the quota unreachable, so
    // nothing was ever rejected - which is exactly why it stopped missing
    // devices. Fixed at the source instead: never reject a genuine
    // response here. targetCount now only affects *when this round's
    // wait can end early* (see handleScan()), never *whether a response
    // that did arrive gets kept*.
    recordScanResponse(mac);
  }
  if (index < 0)
  {
    // Responses outside an active Scan may refresh official Group metadata,
    // but they must never populate the temporary All Devices list.
    if (!scanState.active)
    {
      const int savedIndex = findGroupDeviceIndexByMac(mac);
      if (savedIndex >= 0)
      {
        node_info_t &saved = groupDevices[savedIndex];
        if (uid != nullptr && uid[0] != '\0')
        {
          snprintf(saved.uid, sizeof(saved.uid), "%s", uid);
        }
        saved.lid = lid;
        saved.remain = remain;
        saved.protocolStatus = status;
        if (deviceId != 0)
        {
          saved.deviceId = deviceId;
        }
        if (deviceLimit != 0)
        {
          saved.deviceLimit = deviceLimit;
        }
        saved.responseState = NODE_ONLINE;
        saved.lastResponseMillis = millis();
        if (saveGroupsToNvs())
        {
          broadcastGroups();
        }
      }
      return -1;
    }
    if (Device.deviceCount >= MAX_DEVICES)
    {
      return -1;
    }
    index = Device.deviceCount++;
    Device.nodes[index] = {};
    memcpy(Device.nodes[index].mac, mac, 6);
    const int savedIndex = findGroupDeviceIndexByMac(mac);
    if (savedIndex >= 0)
    {
      snprintf(Device.nodes[index].alias, sizeof(Device.nodes[index].alias),
               "%s", groupDevices[savedIndex].alias);
    }
  }

  node_info_t &node = Device.nodes[index];
  memcpy(node.mac, mac, 6);
  if (uid != nullptr && uid[0] != '\0')
  {
    snprintf(node.uid, sizeof(node.uid), "%s", uid);
  }
  node.lid = lid;
  node.remain = remain;
  node.protocolStatus = status;
  if (deviceId != 0)
  {
    node.deviceId = deviceId;
  }
  if (deviceLimit != 0)
  {
    node.deviceLimit = deviceLimit;
  }
  node.responseState = NODE_ONLINE;
  node.lastResponseMillis = millis();
  nod = Device.deviceCount;

  migrateGroupMemberToMac(node.uid, node.mac);
  updateSavedGroupDevice(node);
  // Applied before the broadcast below so a brand-new node's very first
  // device.upsert already carries whatever matrix position/card type it
  // reported in this same reply, instead of the freshly-zero-inited values
  // above (Device.nodes[index] = {}) going out first and the corrected
  // ones only reaching the app whenever some later, unrelated broadcast
  // happens to fire for this node.
  applyNodeMatrix(index, data);
  applyNodeCardType(index, data);
  applyNodeGroup(index, data);

  broadcastNode(index);
  if (scanState.active && !isNewNode)
  {
    recordScanResponse(mac);
  }
  return index;
}

static void webSendState(uint8_t client)
{
  JsonDocument doc;
  doc["type"] = "state";
  doc["ip"] = WiFi.softAPIP().toString();
  doc["hubMac"] = WiFi.macAddress();
  doc["ssid"] = localApSsid;
  doc["droppedPackets"] = espNowDroppedPackets;

  JsonArray devices = doc["devices"].to<JsonArray>();
  for (int i = 0; i < Device.deviceCount; ++i)
  {
    appendNode(devices.add<JsonObject>(), i);
  }

  appendGroups(doc["groups"].to<JsonArray>());
  webSendJson(client, doc);
}

static void broadcastJob(const char *state, const char *message)
{
  JsonDocument doc;
  doc["type"] = "job";
  doc["state"] = state;
  if (scanState.active)
  {
    doc["done"] = scanState.seenCount;
    doc["total"] = scanState.targetCount;
    doc["success"] = scanState.responseCount;
    doc["noResponse"] = 0;
    doc["sendFailures"] = 0;
    doc["dropped"] = espNowDroppedPackets - scanState.droppedAtStart;
    doc["round"] = scanState.round;
    doc["maxRounds"] = SCAN_MAX_ROUNDS;
  }
  else
  {
    doc["done"] = masterJob.type == JOB_REFRESH
                        ? masterJob.completedAttempts
                        : masterJob.current;
    doc["total"] = masterJob.type == JOB_REFRESH
                         ? masterJob.passTotal
                         : masterJob.total;
    doc["success"] = masterJob.successCount;
    doc["noResponse"] = masterJob.noResponseCount;
    doc["sendFailures"] = masterJob.sendFailureCount;
    doc["dropped"] = espNowDroppedPackets - masterJob.droppedAtStart;
    if (masterJob.type == JOB_REFRESH)
    {
      doc["phase"] = masterJob.refreshPass;
      doc["phaseDone"] = masterJob.current;
      doc["phaseTotal"] = masterJob.total;
      doc["retryPending"] = masterJob.retryTotal;
      doc["nodesTotal"] = masterJob.firstPassTotal;
    }
  }
  doc["message"] = message;
  doc["freeHeap"] = ESP.getFreeHeap();
  doc["minFreeHeap"] = ESP.getMinFreeHeap();
  webBroadcastJson(doc);
}

inline bool webLicenseJobBusy()
{
  return masterJob.type != JOB_NONE || scanState.active;
}

bool masterJobWantsConfigExtras()
{
  return masterJob.type == JOB_MATRIX || masterJob.type == JOB_CARD_TYPE;
}

static bool addJobTarget(const uint8_t mac[6])
{
  if (masterJob.total >= MAX_JOB_TARGETS)
  {
    return false;
  }
  for (uint16_t i = 0; i < masterJob.total; ++i)
  {
    if (memcmp(masterJob.targets[i], mac, 6) == 0)
    {
      return true;
    }
  }
  memcpy(masterJob.targets[masterJob.total++], mac, 6);
  return true;
}

static bool addJobTargetNode(const node_info_t &node)
{
  const uint16_t previousTotal = masterJob.total;
  if (!addJobTarget(node.mac))
  {
    return false;
  }
  if (masterJob.total > previousTotal)
  {
    masterJob.targetNodes[previousTotal] = node;
  }
  return true;
}

static bool addRefreshRetryTarget(const node_info_t &node)
{
  if (masterJob.retryTotal >= MAX_JOB_TARGETS)
  {
    return false;
  }
  for (uint16_t i = 0; i < masterJob.retryTotal; ++i)
  {
    if (memcmp(masterJob.retryTargets[i], node.mac, 6) == 0)
    {
      return true;
    }
  }
  memcpy(masterJob.retryTargets[masterJob.retryTotal], node.mac, 6);
  masterJob.retryNodes[masterJob.retryTotal++] = node;
  return true;
}

static bool beginNextRefreshPass()
{
  if (masterJob.type != JOB_REFRESH || masterJob.retryTotal == 0 ||
      masterJob.refreshPass >= REFRESH_MAX_PASSES)
  {
    return false;
  }
  masterJob.refreshPass++;
  masterJob.total = masterJob.retryTotal;
  masterJob.passTotal += masterJob.total;
  memcpy(masterJob.targets, masterJob.retryTargets,
         masterJob.retryTotal * sizeof(masterJob.retryTargets[0]));
  memcpy(masterJob.targetNodes, masterJob.retryNodes,
         masterJob.retryTotal * sizeof(masterJob.retryNodes[0]));
  masterJob.retryTotal = 0;
  masterJob.current = 0;
  masterJob.attempts = 0;
  masterJob.waiting = false;
  char message[48];
  snprintf(message, sizeof(message), "Retry pass %u/2: %u Node(s)",
           masterJob.refreshPass - 1, masterJob.total);
  broadcastJob("retry", message);
  return true;
}

static void recordScanResponse(const uint8_t mac[6])
{
  if (!scanState.active || scanState.seenCount >= MAX_DEVICES)
  {
    return;
  }
  for (uint16_t i = 0; i < scanState.seenCount; ++i)
  {
    if (memcmp(scanState.seenMacs[i], mac, 6) == 0)
    {
      return;
    }
  }
  memcpy(scanState.seenMacs[scanState.seenCount++], mac, 6);
  scanState.responseCount = scanState.seenCount;
  broadcastJob("scanning", "Receiving unique discovery responses");
}


static bool parseMac(const char *value, uint8_t out[6])
{
  unsigned int bytes[6];
  if (value == nullptr || sscanf(value, "%02x:%02x:%02x:%02x:%02x:%02x",
                                 &bytes[0], &bytes[1], &bytes[2],
                                 &bytes[3], &bytes[4], &bytes[5]) != 6)
  {
    return false;
  }
  for (uint8_t i = 0; i < 6; ++i)
  {
    out[i] = static_cast<uint8_t>(bytes[i]);
  }
  return true;
}

static bool buildTargets(JsonObject data)
{
  const char *targetType = data["targetType"] | "devices";
  if (strcmp(targetType, "group") == 0)
  {
    const uint32_t groupId = data["groupId"] | 0UL;
    const int groupIndex = findGroupIndex(groupId);
    if (groupIndex < 0)
    {
      return false;
    }

    const GroupInfo &group = groups[groupIndex];
    for (uint8_t i = 0; i < group.memberCount; ++i)
    {
      uint8_t mac[6];
      int nodeIndex = -1;
      if (parseMac(group.members[i], mac))
      {
        const int liveIndex = findDeviceIndexByMac(mac);
        if (liveIndex >= 0)
        {
          addJobTargetNode(Device.nodes[liveIndex]);
          continue;
        }
        nodeIndex = findGroupDeviceIndexByMac(mac);
      }
      else
      {
        nodeIndex = findGroupDeviceIndexByUid(group.members[i]);
      }
      if (nodeIndex >= 0)
      {
        addJobTargetNode(groupDevices[nodeIndex]);
      }
    }
  }
  else
  {
    for (JsonVariant value : data["members"].as<JsonArray>())
    {
      uint8_t mac[6];
      if (parseMac(value.as<const char *>(), mac) && findDeviceIndexByMac(mac) >= 0)
      {
        addJobTargetNode(Device.nodes[findDeviceIndexByMac(mac)]);
      }
    }
  }
  return masterJob.total != 0;
}

static void clearAllDevices(uint8_t client, bool acknowledge = true)
{
  scanState.active = false;
  masterJob = {};
  Device = {};
  nod = 0;
  JsonDocument event;
  event["type"] = "devices.cleared";
  webBroadcastJson(event);
  if (acknowledge)
  {
    webSendAck(client, "devices.clear");
  }
}

static void beginScan(uint16_t requestedLimit = DEFAULT_SCAN_LIMIT)
{
  const uint16_t scanLimit = constrain(requestedLimit, 1, MAX_DEVICES);
  clearAllDevices(0, false);
  scanState = {};
  scanState.droppedAtStart = espNowDroppedPackets;
  scanState.scanId = esp_random();
  scanState.targetCount = scanLimit;
  scanState.round = 1;
  if (!sendDiscoveryScan(receiverMac, Device_ID, datalic.lid,
                          scanState.scanId, scanState.round,
                          SCAN_RESPONSE_SLOT_COUNT, SCAN_RESPONSE_SLOT_MS,
                          scanLimit))
  {
    broadcastJob("failed", "Broadcast discovery could not be sent");
    return;
  }
  scanState.active = true;
  scanState.sentAt = millis();
  broadcastJob("scanning", "Discovery broadcast sent");
}

inline void startMasterScan()
{
  if (!webLicenseJobBusy())
  {
    beginScan();
  }
}

// Re-broadcasts the same scan session (same scanId, next round) so nodes
// that missed the last window - most often because they collided in a
// reply slot with another node - get a fresh random slot to try again.
// Already-seen MACs are left untouched; recordScanResponse() dedupes any
// repeat replies from nodes that already got through.
static void continueScanRound()
{
  scanState.round++;
  if (!sendDiscoveryScan(receiverMac, Device_ID, datalic.lid,
                          scanState.scanId, scanState.round,
                          SCAN_RESPONSE_SLOT_COUNT, SCAN_RESPONSE_SLOT_MS,
                          scanState.targetCount))
  {
    broadcastJob("completed", "Scan timeout reached");
    scanState.active = false;
    return;
  }
  scanState.sentAt = millis();
  char note[64];
  snprintf(note, sizeof(note), "Retry round %u/%u: inviting missing devices",
           scanState.round, SCAN_MAX_ROUNDS);
  broadcastJob("scanning", note);
}

static void handleScan()
{
  if (!scanState.active)
  {
    return;
  }
  const uint32_t now = millis();
  if (now - scanState.sentAt < SCAN_RESPONSE_WINDOW_MS)
  {
    // Still inside this round's response window - let it run out fully
    // rather than declaring victory the instant seenCount happens to
    // reach targetCount. Devices pick a random reply slot across the
    // whole window (see SCAN_RESPONSE_SLOT_COUNT); stopping mid-window
    // because an early batch of replies filled the requested "Device
    // Count" would cut off anyone whose slot just hadn't come up yet -
    // this was the actual cause of "set count = real total -> devices go
    // missing" (see the comment on addOrUpdateNode()'s scan branch).
    return;
  }
  // Window fully elapsed: only now is it safe to ask "did we already get
  // everyone we expected?" - and even then, only to skip further retry
  // rounds, never to discard a response that did arrive (that never
  // happens here regardless; recordScanResponse() already has everything).
  if (scanState.seenCount >= scanState.targetCount)
  {
    broadcastJob("completed", "Scan limit reached");
    scanState.active = false;
    return;
  }
  if (scanState.round < SCAN_MAX_ROUNDS)
  {
    continueScanRound();
    return;
  }
  broadcastJob("completed", "Scan timeout reached");
  scanState.active = false;
}

static void beginRefresh(bool scanAfter)
{
  masterJob = {};
  masterJob.type = JOB_REFRESH;
  masterJob.droppedAtStart = espNowDroppedPackets;
  masterJob.scanAfter = scanAfter;
  masterJob.refreshPass = 1;
  for (int i = 0; i < Device.deviceCount; ++i)
  {
    addJobTargetNode(Device.nodes[i]);
  }
  masterJob.firstPassTotal = masterJob.total;
  masterJob.passTotal = masterJob.total;
  broadcastJob("started", "First pass: checking every known Node once");
  if (masterJob.total == 0)
  {
    masterJob = {};
    broadcastJob("completed", "All Devices is empty");
  }
}

inline void startMasterRefresh()
{
  if (!webLicenseJobBusy())
  {
    beginRefresh(false);
  }
}

static void beginSearch(uint8_t client, const char *uid)
{
  const int index = findDeviceIndexByUid(uid);
  if (index < 0)
  {
    webSendError(client, "NOT_FOUND", "Device ID is not in Node List");
    return;
  }
  masterJob = {};
  masterJob.type = JOB_SEARCH;
  masterJob.droppedAtStart = espNowDroppedPackets;
  addJobTargetNode(Device.nodes[index]);
  webSendAck(client, "device.search");
  broadcastJob("started", "Refreshing selected Device ID");
}

static void beginGetLicenseJob(uint8_t client, JsonObject data)
{
  masterJob = {};
  masterJob.type = JOB_SEARCH;
  masterJob.droppedAtStart = espNowDroppedPackets;
  if (!buildTargets(data))
  {
    masterJob = {};
    webSendError(client, "NO_TARGET", "Group or selected Node has no known member");
    return;
  }
  webSendAck(client, "license.get");
  broadcastJob("started", "Getting license from selected Node(s)");
}

// Bulk, MAC-addressed LIC_INFO request (firmware/voltage/temperature/uptime
// telemetry - see sendGetInfo()/webOnInfoResponse()), queued through
// masterJob exactly like license.get so it gets the same retry/timeout
// handling and progress reporting, and can target a multi-select of cards
// or a whole Group via buildTargets() instead of the legacy single-node,
// deviceId-only "info" serial debug command (which can't reliably address
// a specific just-scanned node - see requestNodeInfo()).
static void beginGetInfoJob(uint8_t client, JsonObject data)
{
  masterJob = {};
  masterJob.type = JOB_INFO;
  masterJob.droppedAtStart = espNowDroppedPackets;
  if (!buildTargets(data))
  {
    masterJob = {};
    webSendError(client, "NO_TARGET", "Group or selected Node has no known member");
    return;
  }
  webSendAck(client, "device.getInfo");
  broadcastJob("started", "Getting info from selected Node(s)");
}

// data = {"members": [{"mac": "..", "row": 1, "col": 2}, ...]}. Each online
// device in the app's arranged grid gets its 1-based row/col written into
// it (LIC_SET_MATRIX), queued through masterJob like the other jobs.
static void beginSetMatrixJob(uint8_t client, JsonObject data)
{
  masterJob = {};
  masterJob.type = JOB_MATRIX;
  masterJob.droppedAtStart = espNowDroppedPackets;
  for (JsonVariantConst member : data["members"].as<JsonArrayConst>())
  {
    const char *macText = member["mac"] | (const char *)nullptr;
    uint8_t mac[6];
    if (macText == nullptr || !parseMac(macText, mac))
    {
      continue;
    }
    const int index = findDeviceIndexByMac(mac);
    if (index < 0)
    {
      continue;
    }
    const uint16_t before = masterJob.total;
    if (addJobTargetNode(Device.nodes[index]) && masterJob.total > before)
    {
      masterJob.targetNodes[before].nodeRow = member["row"] | 0;
      masterJob.targetNodes[before].nodeCol = member["col"] | 0;
      // Carry the operator-typed alias on the job copy so sendSetMatrix()
      // ships it in "Matrix.alias" (falls back to the device's own alias if
      // the app didn't send one).
      const char *memberAlias = member["alias"] | (const char *)nullptr;
      if (memberAlias != nullptr)
      {
        snprintf(masterJob.targetNodes[before].alias,
                 sizeof(masterJob.targetNodes[before].alias), "%s", memberAlias);
      }
      // Carry the Group's name on the job copy too, so sendCurrentJobRequest()
      // can ship it as the top-level "group" field alongside Matrix (see
      // sendSetMatrix()) - the app sends the same group name for every
      // member of a Set Matrix from the Groups page.
      const char *memberGroup = member["group"] | (const char *)nullptr;
      if (memberGroup != nullptr)
      {
        snprintf(masterJob.targetNodes[before].nodeGroup,
                 sizeof(masterJob.targetNodes[before].nodeGroup), "%s", memberGroup);
      }
    }
  }
  if (masterJob.total == 0)
  {
    masterJob = {};
    webSendError(client, "NO_TARGET", "Không có thiết bị online nào để ghi vị trí");
    return;
  }
  webSendAck(client, "matrix.set");
  broadcastJob("started", "Ghi vị trí ma trận xuống các node");
}

// data = {"members": [{"mac": "..", "cardType": "OB"|"R"}, ...]}. Each
// target device gets its board-variant label written into it
// (LIC_CONFIG_DEVICE's "cardType"), queued through masterJob. Unlike Set
// Matrix, targets don't have to be online right now — the job's normal
// retry/timeout handles whoever isn't reachable yet.
static void beginSetCardTypeJob(uint8_t client, JsonObject data)
{
  masterJob = {};
  masterJob.type = JOB_CARD_TYPE;
  masterJob.droppedAtStart = espNowDroppedPackets;
  for (JsonVariantConst member : data["members"].as<JsonArrayConst>())
  {
    const char *macText = member["mac"] | (const char *)nullptr;
    uint8_t mac[6];
    if (macText == nullptr || !parseMac(macText, mac))
    {
      continue;
    }
    const int index = findDeviceIndexByMac(mac);
    if (index < 0)
    {
      continue;
    }
    const uint16_t before = masterJob.total;
    if (addJobTargetNode(Device.nodes[index]) && masterJob.total > before)
    {
      const char *cardType = member["cardType"] | "";
      snprintf(masterJob.targetNodes[before].cardType,
               sizeof(masterJob.targetNodes[before].cardType), "%s", cardType);
    }
  }
  if (masterJob.total == 0)
  {
    masterJob = {};
    webSendError(client, "NO_TARGET", "Không có thiết bị nào để ghi loại card");
    return;
  }
  webSendAck(client, "cardtype.set");
  broadcastJob("started", "Ghi loại card xuống các node");
}

static void beginCommandJob(uint8_t client, MasterJobType type, JsonObject data)
{
  masterJob = {};
  masterJob.type = type;
  masterJob.droppedAtStart = espNowDroppedPackets;
  masterJob.useNodeLid = strcmp(data["targetType"] | "devices", "group") == 0;
  masterJob.lid = data["lid"] | 0;
  if (type == JOB_CONFIG)
  {
    const char *newLid = data["new_lid"] | "";
    const size_t newLidLength = strlen(newLid);
    bool validNewLid = newLidLength > 0 && newLidLength < sizeof(masterJob.newLid);
    for (size_t i = 0; validNewLid && i < newLidLength; ++i)
    {
      validNewLid = static_cast<uint8_t>(newLid[i]) >= 0x20 &&
                    static_cast<uint8_t>(newLid[i]) <= 0x7E;
    }
    if (!validNewLid)
    {
      masterJob = {};
      webSendError(client, "BAD_LID", "New LID must contain 1 to 11 characters");
      return;
    }
    snprintf(masterJob.newLid, sizeof(masterJob.newLid), "%s", newLid);
  }
  masterJob.durationMinutes = data["durationMinutes"] | 60;
  masterJob.expired = data["expired"] | 0;
  // "Mode khi hết license" trong hop Set License tren app (0=Random,
  // 1=Tat het, 2=Nhap nhay 2s, 3=Random theo chu ky) - tuy chon, -1 khi app
  // cu hon khong gui truong nay de node tu giu nguyen gia tri dang luu (xem
  // LIC_SET_LICENSE trong rcv/protocol_handler.h).
  masterJob.expiredLedMode = data["expiredLedMode"].is<int>() ? data["expiredLedMode"].as<int>() : -1;
  // Chi dung khi expiredLedMode == 3 - khoang thoi gian "binh thuong" giua
  // 2 dot loi (phut), app gui kem qua o "Chu ky binh thuong" khi chon mode 3.
  masterJob.expiredCycleMinMinutes = data["expiredCycleMinMinutes"].is<long>() ? data["expiredCycleMinMinutes"].as<long>() : -1;
  masterJob.expiredCycleMaxMinutes = data["expiredCycleMaxMinutes"].is<long>() ? data["expiredCycleMaxMinutes"].as<long>() : -1;
  if (!buildTargets(data))
  {
    masterJob = {};
    webSendError(client, "NO_TARGET", "No known node was selected");
    return;
  }
  if (type == JOB_CONFIG && (masterJob.useNodeLid || masterJob.total != 1))
  {
    masterJob = {};
    webSendError(client, "ONE_NODE_REQUIRED", "Config Device requires exactly one selected Node");
    return;
  }
  webSendAck(client, type == JOB_SET ? "license.set" : "device.config");
  broadcastJob("started", type == JOB_SET ? "Setting license" : "Configuring device LID");
}

inline bool startMasterSetLicense(int deviceId, int lid, uint32_t created,
                                  uint32_t duration, uint8_t expired)
{
  if (webLicenseJobBusy())
  {
    return false;
  }
  const int index = findDeviceIndexById(deviceId);
  if (index < 0)
  {
    return false;
  }

  masterJob = {};
  masterJob.type = JOB_SET;
  masterJob.droppedAtStart = espNowDroppedPackets;
  masterJob.lid = lid;
  masterJob.created = created;
  masterJob.durationMinutes = duration;
  masterJob.expired = expired;
  addJobTargetNode(Device.nodes[index]);
  broadcastJob("started", "Setting license");
  return true;
}

// Fires a single one-shot LIC_INFO request at a known device (see
// sendGetInfo() / webOnInfoResponse()). Deliberately NOT routed through
// masterJob like Scan/Refresh/Set/Config: this mirrors the legacy "info"
// serial debug command it backs (see serial.h/Sender_ESPNOW.ino's button
// case 6) — a single fire-and-forget probe, not a batch job with retries.
// The reply (if any) arrives asynchronously via processReceivedData() and
// updates/broadcasts the node regardless of whether anything is "waiting"
// for it here.
inline bool requestNodeInfo(int deviceId, int lid)
{
  if (webLicenseJobBusy())
  {
    return false;
  }
  const int index = findDeviceIndexById(deviceId);
  if (index < 0)
  {
    return false;
  }
  return sendGetInfo(Device.nodes[index].mac, deviceId, lid);
}

static bool sendCurrentJobRequest()
{
  const uint8_t *mac = masterJob.targets[masterJob.current];
  node_info_t &node = masterJob.targetNodes[masterJob.current];
  const int ramIndex = findDeviceIndexByMac(mac);
  uint32_t requestTime = 0;
  bool sent = false;
  if (masterJob.type == JOB_REFRESH || masterJob.type == JOB_SEARCH)
  {
    if (masterJob.attempts == 1 && masterJob.created == 0)
    {
      node.responseState = NODE_REFRESHING;
      if (ramIndex >= 0)
      {
        Device.nodes[ramIndex].responseState = NODE_REFRESHING;
        broadcastNode(ramIndex);
      }
    }
    sent = sendGetLicense(mac, node.deviceId != 0 ? node.deviceId : Device_ID,
                          node.lid, &requestTime);
  }
  else if (masterJob.type == JOB_SET)
  {
    if (masterJob.attempts == 1 && masterJob.created == 0)
    {
      masterJob.created = millis();
    }
    const int targetLid = masterJob.useNodeLid || masterJob.lid <= 0
                              ? node.lid
                              : masterJob.lid;
    sent = set_license(mac, node.deviceId != 0 ? node.deviceId : Device_ID,
                        targetLid, masterJob.created,
                       masterJob.durationMinutes, masterJob.expired,
                       masterJob.expiredLedMode, masterJob.expiredCycleMinMinutes,
                       masterJob.expiredCycleMaxMinutes, &requestTime);
  }
  else if (masterJob.type == JOB_INFO)
  {
    if (masterJob.attempts == 1 && masterJob.created == 0)
    {
      node.responseState = NODE_REFRESHING;
      if (ramIndex >= 0)
      {
        Device.nodes[ramIndex].responseState = NODE_REFRESHING;
        broadcastNode(ramIndex);
      }
    }
    sent = sendGetInfo(mac, node.deviceId != 0 ? node.deviceId : Device_ID,
                       node.lid, &requestTime);
  }
  else if (masterJob.type == JOB_MATRIX)
  {
    // nodeRow/nodeCol/alias/nodeGroup were stashed on the target snapshot
    // by beginSetMatrixJob() from the app's per-device {row,col,alias,group}.
    sent = sendSetMatrix(mac, node.deviceId != 0 ? node.deviceId : Device_ID,
                         node.lid, node.nodeRow, node.nodeCol, node.alias,
                         node.nodeGroup, &requestTime);
  }
  else if (masterJob.type == JOB_CARD_TYPE)
  {
    // cardType was stashed on the target snapshot by beginSetCardTypeJob().
    sent = sendSetCardType(mac, node.deviceId != 0 ? node.deviceId : Device_ID,
                           node.lid, node.cardType, &requestTime);
  }
  else
  {
    const int targetDeviceId = node.deviceId != 0 ? node.deviceId : Device_ID;
    sent = sendConfigDevice(mac, targetDeviceId, masterJob.newLid, &requestTime);
  }
  if (sent)
  {
    masterJob.requestTime = requestTime;
  }
  return sent;
}

// `nodeResponded` = the node actually sent back a reply for this target (its
// content just didn't count as success - e.g. CONFIG_LID_ACK/SET_LICENSE_ACK
// carrying status != 0, see webOnConfigExtraResponse()/webOnLicenseResponse()).
// A node that's clearly reachable and talking shouldn't flip to NO_RESPONSE/
// OFFLINE just because the specific config/license it tried to save didn't
// persist - that used to make e.g. a Set Matrix run visibly flash boards red
// then have to wait for the next Refresh to show ONLINE again, even though
// they never actually stopped answering. Only a genuine timeout (no reply at
// all - the call sites in sendCurrentJobRequest()) should mark it offline.
static void finishCurrentTarget(bool success, bool sendFailure = false, bool nodeResponded = false)
{
  const uint8_t *currentMac = masterJob.targets[masterJob.current];
  const int index = findDeviceIndexByMac(currentMac);
  const bool refreshWillRetry = masterJob.type == JOB_REFRESH && !success &&
                                masterJob.refreshPass < REFRESH_MAX_PASSES;
  if (refreshWillRetry)
  {
    addRefreshRetryTarget(masterJob.targetNodes[masterJob.current]);
  }
  else if (!success && !nodeResponded && index >= 0)
  {
    Device.nodes[index].responseState = NODE_NO_RESPONSE;
    broadcastNode(index);
  }
  if (!success && !nodeResponded && !refreshWillRetry)
  {
    // Gated on !refreshWillRetry same as the All-Devices branch above: a
    // Refresh pass that still has retries left for this node hasn't given
    // up on it yet, so flipping the Groups page to NO_RESPONSE here would
    // just be a premature flicker that self-corrects (or doesn't) a pass
    // later - only report it once Refresh has actually exhausted retries.
    const int savedIndex = findGroupDeviceIndexByMac(currentMac);
    if (savedIndex >= 0)
    {
      groupDevices[savedIndex].responseState = NODE_NO_RESPONSE;
      groupDevices[savedIndex].lastResponseMillis = 0;
      // Deliberately do not remove or rewrite Group membership on NO_RESPONSE.
      broadcastGroups();
    }
  }
  if (success)
  {
    masterJob.successCount++;
  }
  else if (!refreshWillRetry)
  {
    masterJob.noResponseCount++;
  }
  if (sendFailure)
  {
    masterJob.sendFailureCount++;
  }
  masterJob.current++;
  if (masterJob.type == JOB_REFRESH)
  {
    masterJob.completedAttempts++;
  }
  masterJob.attempts = 0;
  if (masterJob.type != JOB_SET)
  {
    masterJob.created = 0;
  }
  masterJob.waiting = false;
  broadcastJob("progress", success
                                ? "Node responded"
                                : (refreshWillRetry
                                       ? "Node queued for retry"
                                       : "Node did not respond"));
}

static void handleMasterJob()
{
  if (masterJob.type == JOB_NONE)
  {
    return;
  }
  if (masterJob.current >= masterJob.total)
  {
    if (beginNextRefreshPass())
    {
      return;
    }
    const bool scanAfter = masterJob.scanAfter;
    broadcastJob("completed", scanAfter
                                  ? "Refresh completed; starting discovery"
                                  : "Operation completed");
    if (scanAfter)
    {
      masterJob = {};
      beginScan();
    }
    else
    {
      masterJob = {};
    }
    return;
  }
  if (!masterJob.waiting)
  {
    masterJob.attempts++;
    if (!sendCurrentJobRequest())
    {
      if (masterJob.type == JOB_REFRESH ||
          masterJob.attempts >= MAX_REQUEST_ATTEMPTS)
      {
        finishCurrentTarget(false, true);
      }
      return;
    }
    masterJob.sentAt = millis();
    masterJob.waiting = true;
    return;
  }
  const uint32_t responseTimeout = masterJob.type == JOB_REFRESH
                                       ? REFRESH_RESPONSE_TIMEOUT_MS
                                       : RESPONSE_TIMEOUT_MS;
  if (millis() - masterJob.sentAt >= responseTimeout)
  {
    masterJob.waiting = false;
    if (masterJob.type == JOB_REFRESH ||
        masterJob.attempts >= MAX_REQUEST_ATTEMPTS)
    {
      finishCurrentTarget(false);
    }
    else
    {
      broadcastJob("retry", "Retrying node");
    }
  }
}

void webOnLicenseResponse(uint8_t opcode, const uint8_t *mac, int lid,
                          uint32_t requestTime, JsonObject data)
{
  const int index = findDeviceIndexByMac(mac);
  const bool matchesCurrentRequest = masterJob.type != JOB_NONE && masterJob.waiting &&
                                     masterJob.current < masterJob.total &&
                                     memcmp(masterJob.targets[masterJob.current], mac, 6) == 0 &&
                                     masterJob.requestTime == requestTime;
  const bool isCurrentSetResponse = matchesCurrentRequest &&
                                    masterJob.type == JOB_SET &&
                                    opcode == (LIC_SET_LICENSE | 0x80);
  // A LIC_CONFIG_DEVICE ack that carries "Matrix" or "cardType" is a Set
  // Matrix/Set Card Type reply (a late/stale one, since the live path is
  // caught in processReceivedData) - it must not run the block below, whose
  // `node.lid = lid` would zero the node's real LID when that ack has no
  // lid of its own.
  const bool isConfigExtraAck = !data["Matrix"].isNull() || !data["cardType"].isNull() || !data["group"].isNull();
  if (index >= 0 && opcode != (LIC_GET_LICENSE | 0x80) && !isCurrentSetResponse && !isConfigExtraAck)
  {
    node_info_t &node = Device.nodes[index];
    node.lid = lid;
    node.protocolStatus = data["status"] | node.protocolStatus;
    if (!data["remain"].isNull())
    {
      node.remain = data["remain"].as<uint32_t>();
    }
    if (!data["id"].isNull())
    {
      node.deviceId = data["id"].as<int>();
    }
    if (!data["nod"].isNull())
    {
      node.deviceLimit = data["nod"].as<int>();
    }
    node.responseState = NODE_ONLINE;
    node.lastResponseMillis = millis();
    masterJob.targetNodes[masterJob.current] = node;
    updateSavedGroupDevice(node);
    broadcastNode(index);
  }

  if (!matchesCurrentRequest)
  {
    return;
  }
  const uint8_t expected = masterJob.type == JOB_SET
                               ? (LIC_SET_LICENSE | 0x80)
                               : (masterJob.type == JOB_CONFIG
                                      ? (LIC_CONFIG_DEVICE | 0x80)
                                      : (LIC_GET_LICENSE | 0x80));
  if (opcode == expected)
  {
    if (masterJob.type == JOB_SET)
    {
      // Trust what the node's SET-ack actually reports (the `lid` param,
      // read the same way as every other response via readProtocolLid() —
      // see the unsolicited-update branch above, which already does
      // `node.lid = lid`), not the value the hub merely asked for. A node
      // that acks the SET opcode without actually applying/persisting the
      // new license would otherwise make the hub's cache lie about success
      // until the next Get License/Scan re-queries it and reveals the real
      // (unchanged) value — which is exactly the "looks set, reverts after
      // reconnect+refresh" symptom this fixes.
      const int appliedLid = lid;
      const uint32_t appliedRemain = data["remain"].isNull()
                                           ? masterJob.durationMinutes
                                           : data["remain"].as<uint32_t>();
      if (index >= 0)
      {
        node_info_t &node = Device.nodes[index];
        node.lid = appliedLid;
        node.remain = appliedRemain;
        node.protocolStatus = data["status"] | node.protocolStatus;
        node.responseState = NODE_ONLINE;
        node.lastResponseMillis = millis();
        masterJob.targetNodes[masterJob.current] = node;
        updateSavedGroupDevice(node);
        broadcastNode(index);
      }
      else
      {
        const int savedIndex = findGroupDeviceIndexByMac(mac);
        if (savedIndex >= 0)
        {
          node_info_t &node = groupDevices[savedIndex];
          node.lid = appliedLid;
          node.remain = appliedRemain;
          node.protocolStatus = data["status"] | node.protocolStatus;
          node.responseState = NODE_ONLINE;
          node.lastResponseMillis = millis();
          masterJob.targetNodes[masterJob.current] = node;
          if (saveGroupsToNvs())
          {
            broadcastGroups();
          }
        }
      }
      // Same reasoning as webOnConfigExtraResponse(): the node acking SET
      // doesn't mean it actually persisted the new license - status 0 means
      // it did, anything else (see rcv/protocol_handler.h::saveLicenseData())
      // means it reverted internally, so counting every ack as success here
      // used to hide the exact same silent-NVS-failure class of bug Set
      // Matrix had, just for the license instead of Matrix/cardType/group.
      const int status = data["status"] | -1;
      finishCurrentTarget(status == 0, false, true);
      return;
    }
    finishCurrentTarget(true);
  }
}

// Handles a LIC_INFO_RESPONSE (see protocol_handler.h::sendGetInfo()).
// Kept separate from webOnLicenseResponse() on purpose: LIC_INFO's payload
// uses its own schema (device_id/firmware_version/voltage_v/temperature_c/
// uptime_m/status) that doesn't line up with the license fields
// (lid/remain/status/id/nod) webOnLicenseResponse() writes for every other
// opcode, so routing it through there would silently corrupt a node's real
// lid/status (e.g. LIC_INFO's placeholder "lid":"DEFAULTLID" parses to the
// integer 0 via readProtocolLid()).
void webOnInfoResponse(const uint8_t *mac, uint32_t requestTime, JsonObject data)
{
  const int index = findDeviceIndexByMac(mac);
  if (index < 0)
  {
    // INFO telemetry is live-only (voltage/temperature/uptime are stale the
    // instant they're read), so unlike addOrUpdateNode() there is no saved
    // Group entry to fall back to - a node has to already be in the live
    // All Devices list (via Scan/Refresh) before its INFO reply means
    // anything to display.
    return;
  }

  node_info_t &node = Device.nodes[index];
  // A currently-deployed node was tested live (2026-09-08) and still replies
  // with the older {deviceName,lid,version,status} shape, not yet the richer
  // {device_id,firmware_version,voltage_v,temperature_c,uptime_m,protocol,
  // status} one the new node firmware is meant to send - fall back to the
  // old field name so this keeps working against nodes not updated yet.
  JsonVariantConst firmwareVariant = data["firmware_version"];
  if (firmwareVariant.isNull())
  {
    firmwareVariant = data["version"];
  }
  const char *firmwareVersion = firmwareVariant | "";
  snprintf(node.firmwareVersion, sizeof(node.firmwareVersion), "%s", firmwareVersion);
  node.voltageV = data["voltage_v"] | 0.0f;
  node.temperatureC = data["temperature_c"] | 0.0f;
  node.uptimeMinutes = readProtocolUint32(data["uptime_m"]);
  node.infoStatus = data["status"] | 0;
  // Older node firmware's {deviceName,lid,version,status} reply has no
  // "protocol" field at all - left blank rather than guessed at, same as
  // firmwareVersion above, since this is meant to be what the node itself
  // says, not an assumption.
  const char *linkProtocol = data["protocol"] | "";
  snprintf(node.linkProtocol, sizeof(node.linkProtocol), "%s", linkProtocol);
  applyNodeMatrix(index, data);    // node may echo its stored "Matrix" here too
  applyNodeCardType(index, data);  // ...and/or its stored "cardType"
  applyNodeGroup(index, data);     // ...and/or its stored "group"
  node.lastInfoMillis = millis();
  node.responseState = NODE_ONLINE;
  node.lastResponseMillis = millis();

  Serial.printf("[LIC_INFO] %s fw=%s voltage=%.2fV temp=%.1fC uptime=%lum status=%d protocol=%s\n",
                node.uid, node.firmwareVersion, node.voltageV, node.temperatureC,
                static_cast<unsigned long>(node.uptimeMinutes), node.infoStatus, node.linkProtocol);

  broadcastNode(index);

  // If this reply is what a running device.getInfo job (JOB_INFO) is
  // currently waiting on, close out that target the same way every other
  // job response does - otherwise the job would just sit there until the
  // per-node timeout, wrongly counting a node that actually answered as a
  // failure.
  const bool matchesCurrentRequest = masterJob.type == JOB_INFO && masterJob.waiting &&
                                     masterJob.current < masterJob.total &&
                                     memcmp(masterJob.targets[masterJob.current], mac, 6) == 0 &&
                                     masterJob.requestTime == requestTime;
  if (matchesCurrentRequest)
  {
    finishCurrentTarget(true);
  }
}

// Handles the config ack for a Set Matrix or Set Card Type job: the node
// confirms it stored the extra field(s) (and, once its firmware supports
// it, echoes them back). Update the live entry and close out the target if
// this is the reply whichever of those two jobs was waiting on.
void webOnConfigExtraResponse(const uint8_t *mac, uint32_t requestTime, JsonObject data)
{
  const int index = findDeviceIndexByMac(mac);
  if (index >= 0)
  {
    node_info_t &node = Device.nodes[index];
    applyNodeMatrix(index, data);
    applyNodeCardType(index, data);
    applyNodeGroup(index, data);
    node.responseState = NODE_ONLINE;
    node.lastResponseMillis = millis();
    Serial.printf("[CONFIG] %s -> H%dC%d alias=%s cardType=%s group=%s\n", node.uid,
                  node.nodeRow, node.nodeCol, node.alias, node.cardType, node.nodeGroup);
    broadcastNode(index);
  }

  const bool matchesCurrentRequest = masterJobWantsConfigExtras() && masterJob.waiting &&
                                     masterJob.current < masterJob.total &&
                                     memcmp(masterJob.targets[masterJob.current], mac, 6) == 0 &&
                                     masterJob.requestTime == requestTime;
  if (matchesCurrentRequest)
  {
    // The node replying at all doesn't mean it actually saved the new
    // config - CONFIG_LID_ACK's own "status" says that: 0 = saved, 1 =
    // request was invalid, 2 = its NVS write failed even after its own
    // internal retry (see rcv/protocol_handler.h::saveDeviceConfig()), and
    // either way it reverts and echoes back its OLD config instead of the
    // new one (already applied above via applyNodeMatrix/CardType/Group,
    // so the Hub's own record stays correct either way). Counting every
    // reply as success regardless used to make a job report e.g.
    // "success=35/35" while several of those boards had actually failed to
    // persist anything.
    const int status = data["status"] | -1;
    finishCurrentTarget(status == 0, false, true);
  }
}

static void deleteNode(uint8_t client, const char *macText)
{
  uint8_t mac[6];
  if (!parseMac(macText, mac))
  {
    webSendError(client, "BAD_MAC", "Invalid MAC address");
    return;
  }
  const int index = findDeviceIndexByMac(mac);
  if (index < 0)
  {
    webSendError(client, "NOT_FOUND", "MAC is not in Node List");
    return;
  }
  char deletedUid[16];
  snprintf(deletedUid, sizeof(deletedUid), "%s", Device.nodes[index].uid);
  char deletedMac[GROUP_MEMBER_KEY_LENGTH];
  formatMacAddress(Device.nodes[index].mac, deletedMac);
  removeDeviceAt(index);
  nod = Device.deviceCount;
  webSendAck(client, "node.delete");
  JsonDocument event;
  event["type"] = "device.deleted";
  event["uid"] = deletedUid;
  event["mac"] = deletedMac;
  webBroadcastJson(event);
}

static void saveAlias(uint8_t client, const char *macText, const char *value)
{
  uint8_t mac[6];
  if (!parseMac(macText, mac))
  {
    webSendError(client, "BAD_MAC", "Invalid MAC address");
    return;
  }
  const int index = findDeviceIndexByMac(mac);
  int savedIndex = findGroupDeviceIndexByMac(mac);
  if (index < 0 && savedIndex < 0)
  {
    webSendError(client, "NOT_FOUND", "Device is not in All Devices or a Group");
    return;
  }
  if (value == nullptr || value[0] == '\0' || strlen(value) >= DEVICE_ALIAS_LENGTH)
  {
    webSendError(client, "BAD_ALIAS", "Alias must contain 1 to 40 characters");
    return;
  }
  if (index >= 0)
  {
    snprintf(Device.nodes[index].alias, sizeof(Device.nodes[index].alias), "%s", value);
    // "All Devices" itself is intentionally RAM-only and clears on reboot,
    // but the alias the operator just set shouldn't disappear with it —
    // mirror it into the persistent catalog even if this device was never
    // added to a Group (upsertGroupDevice() also keeps its lid/remain/etc.
    // in sync from here on, see updateSavedGroupDevice()).
    if (!upsertGroupDevice(Device.nodes[index]))
    {
      webSendError(client, "CATALOG_FULL", "Too many known devices to remember an alias for");
      return;
    }
    savedIndex = findGroupDeviceIndexByMac(mac);
  }
  else if (savedIndex >= 0)
  {
    snprintf(groupDevices[savedIndex].alias, sizeof(groupDevices[savedIndex].alias), "%s", value);
  }
  if (savedIndex >= 0)
  {
    if (!saveGroupsToNvs())
    {
      webSendError(client, "NVS_WRITE_FAILED", "Alias was not saved");
      return;
    }
    broadcastGroups();
  }
  webSendAck(client, "alias.save");
  if (index >= 0)
  {
    broadcastNode(index);
  }
}

static void deleteAlias(uint8_t client, const char *macText)
{
  uint8_t mac[6];
  if (!parseMac(macText, mac))
  {
    webSendError(client, "BAD_MAC", "Invalid MAC address");
    return;
  }
  const int index = findDeviceIndexByMac(mac);
  const int savedIndex = findGroupDeviceIndexByMac(mac);
  if (index < 0 && savedIndex < 0)
  {
    webSendError(client, "NOT_FOUND", "Device is not in All Devices or a Group");
    return;
  }
  if (index >= 0)
  {
    Device.nodes[index].alias[0] = '\0';
  }
  if (savedIndex >= 0)
  {
    groupDevices[savedIndex].alias[0] = '\0';
    if (!saveGroupsToNvs())
    {
      webSendError(client, "NVS_WRITE_FAILED", "Group member alias was not deleted");
      return;
    }
    broadcastGroups();
  }
  webSendAck(client, "alias.delete");
  if (index >= 0)
  {
    broadcastNode(index);
  }
}

static void saveGroup(uint8_t client, uint32_t id, const char *name,
                      JsonArrayConst members)
{
  if (name == nullptr || name[0] == '\0')
  {
    webSendError(client, "BAD_GROUP", "Group name is required");
    return;
  }

  int index = id == 0 ? -1 : findGroupIndex(id);
  if (id != 0 && index < 0)
  {
    webSendError(client, "GROUP_NOT_FOUND", "Group is not in Group List");
    return;
  }
  const bool isNew = index < 0;
  memcpy(groupBackup, groups, sizeof(groups));
  memcpy(groupDeviceBackup, groupDevices, sizeof(groupDevices));
  const uint8_t previousGroupDeviceCount = groupDeviceCount;
  if (index < 0)
  {
    if (groupCount >= MAX_GROUPS)
    {
      webSendError(client, "GROUP_FULL", "Group list is full");
      return;
    }
    index = groupCount++;
    groups[index] = {};
    uint32_t nextId = 1;
    for (uint8_t i = 0; i + 1 < groupCount; ++i)
    {
      nextId = max(nextId, groups[i].id + 1);
    }
    groups[index].id = nextId;
  }
  snprintf(groups[index].name, sizeof(groups[index].name), "%s", name);
  groups[index].memberCount = 0;
  memset(groups[index].members, 0, sizeof(groups[index].members));
  for (JsonVariantConst value : members)
  {
    const char *memberKey = value.as<const char *>();
    uint8_t memberMac[6];
    const int ramIndex = memberKey != nullptr && parseMac(memberKey, memberMac)
                             ? findDeviceIndexByMac(memberMac)
                             : -1;
    const int savedIndex = memberKey != nullptr && parseMac(memberKey, memberMac)
                               ? findGroupDeviceIndexByMac(memberMac)
                               : -1;
    if (memberKey == nullptr || (ramIndex < 0 && savedIndex < 0) ||
        groupHasMember(groups[index], memberKey) ||
        groups[index].memberCount >= MAX_DEVICES)
    {
      // Silent otherwise - there was no way to tell, from the outside,
      // whether a member the app sent actually landed in the saved Group
      // or quietly never made it in (e.g. reported "80 scanned" not
      // matching what actually got added to a Group).
      Serial.printf("[GROUP] Bo qua thanh vien '%s': %s\n", memberKey ? memberKey : "(null)",
                    memberKey == nullptr             ? "id_src/mac rong"
                    : (ramIndex < 0 && savedIndex < 0) ? "Hub khong biet thiet bi nay (chua co trong Device.nodes[] lan groupDevices[])"
                    : groupHasMember(groups[index], memberKey) ? "da co san trong group nay"
                                                                : "group da day (MAX_DEVICES)");
      continue;
    }
    snprintf(groups[index].members[groups[index].memberCount++],
              GROUP_MEMBER_KEY_LENGTH, "%s", memberKey);
    if (ramIndex >= 0)
    {
      upsertGroupDevice(Device.nodes[ramIndex]);
    }
  }
  pruneUnusedGroupDevices();
  if (!saveGroupsToNvs())
  {
    memcpy(groups, groupBackup, sizeof(groups));
    if (isNew)
    {
      groupCount--;
    }
    memcpy(groupDevices, groupDeviceBackup, sizeof(groupDevices));
    groupDeviceCount = previousGroupDeviceCount;
    webSendError(client, "NVS_WRITE_FAILED", "Group was not saved");
    return;
  }
  webSendAck(client, "group.save");
  broadcastGroups();
}

static void deleteGroup(uint8_t client, uint32_t id)
{
  const int index = findGroupIndex(id);
  if (index < 0)
  {
    webSendError(client, "GROUP_NOT_FOUND", "Group is not in Group List");
    return;
  }
  const GroupInfo removed = groups[index];
  memcpy(groupDeviceBackup, groupDevices, sizeof(groupDevices));
  const uint8_t previousGroupDeviceCount = groupDeviceCount;
  for (uint8_t i = index; i + 1 < groupCount; ++i)
  {
    groups[i] = groups[i + 1];
  }
  groupCount--;
  groups[groupCount] = {};
  pruneUnusedGroupDevices();
  if (!saveGroupsToNvs())
  {
    for (uint8_t i = groupCount; i > index; --i)
    {
      groups[i] = groups[i - 1];
    }
    groups[index] = removed;
    groupCount++;
    memcpy(groupDevices, groupDeviceBackup, sizeof(groupDevices));
    groupDeviceCount = previousGroupDeviceCount;
    webSendError(client, "NVS_WRITE_FAILED", "Group was not deleted");
    return;
  }
  webSendAck(client, "group.delete");
  broadcastGroups();
}

// ---- device.identify (blink a node's LED 46 while it is selected in the app)
//
// data = {"members": ["AA:BB:..", ...], "on": bool, "seconds": n}. Kept out of
// masterJob on purpose: it is a fire-and-forget nudge (the node never
// replies), it must work while a Scan/Refresh is running, and it must not
// make the Hub look "busy" to the app. Packets go out from a small queue,
// one per IDENTIFY_SEND_GAP_MS, so ticking a whole row of boards doesn't
// dump 20 back-to-back esp_now_send() calls and overflow the radio's TX
// queue. Every request is sent IDENTIFY_SEND_TRIES times (unicast has no
// end-to-end confirmation here, and a repeat is harmless: the node just
// re-arms the same state). A newer request for the same MAC replaces the
// one still waiting in the queue, so a quick tick/untick ends up as "off".
constexpr uint8_t IDENTIFY_QUEUE_SIZE = 128;
constexpr uint8_t IDENTIFY_SEND_TRIES = 2;
constexpr uint32_t IDENTIFY_SEND_GAP_MS = 12;

struct IdentifyRequest
{
  uint8_t mac[6];
  bool on;
  uint16_t seconds;
  uint8_t triesLeft;
};

static IdentifyRequest identifyQueue[IDENTIFY_QUEUE_SIZE];
static uint8_t identifyQueueCount = 0;

static void queueIdentify(const uint8_t mac[6], bool on, uint16_t seconds)
{
  for (uint8_t i = 0; i < identifyQueueCount; ++i)
  {
    if (memcmp(identifyQueue[i].mac, mac, 6) == 0)
    {
      identifyQueue[i].on = on;
      identifyQueue[i].seconds = seconds;
      identifyQueue[i].triesLeft = IDENTIFY_SEND_TRIES;
      return;
    }
  }
  if (identifyQueueCount >= IDENTIFY_QUEUE_SIZE)
  {
    Serial.println("[IDENTIFY] Hang doi day, bo qua 1 board");
    return;
  }
  IdentifyRequest &request = identifyQueue[identifyQueueCount++];
  memcpy(request.mac, mac, 6);
  request.on = on;
  request.seconds = seconds;
  request.triesLeft = IDENTIFY_SEND_TRIES;
}

static void handleIdentifyCommand(uint8_t client, JsonObject data)
{
  const bool on = data["on"] | false;
  const uint16_t seconds = constrain(data["seconds"] | 300, 1, 900);
  uint16_t queued = 0;
  for (JsonVariantConst member : data["members"].as<JsonArrayConst>())
  {
    const char *macText = member.as<const char *>();
    uint8_t mac[6];
    if (macText == nullptr || !parseMac(macText, mac) || findDeviceIndexByMac(mac) < 0)
    {
      continue;
    }
    queueIdentify(mac, on, seconds);
    queued++;
  }
  // No webSendAck(): the app fires this on every tick/untick of a board's
  // checkbox and doesn't wait on anything, an "ack" status-bar toast per
  // click would just be noise.
  Serial.printf("[IDENTIFY] %s x%u (queue=%u)\n", on ? "BAT" : "TAT",
                static_cast<unsigned>(queued), static_cast<unsigned>(identifyQueueCount));
  (void)client;
}

static void handleIdentifyQueue()
{
  static uint32_t lastSendMillis = 0;
  if (identifyQueueCount == 0)
  {
    return;
  }
  const uint32_t nowMillis = millis();
  if (nowMillis - lastSendMillis < IDENTIFY_SEND_GAP_MS)
  {
    return;
  }
  lastSendMillis = nowMillis;

  IdentifyRequest request = identifyQueue[0];
  const int index = findDeviceIndexByMac(request.mac);
  if (index >= 0)
  {
    const node_info_t &node = Device.nodes[index];
    sendIdentify(request.mac, node.deviceId != 0 ? node.deviceId : Device_ID,
                 node.lid, request.on, request.seconds);
  }
  // Pop the head; if it still owes a repeat, it goes to the back so the
  // repeat lands after every other board has had its first packet.
  for (uint8_t i = 0; i + 1 < identifyQueueCount; ++i)
  {
    identifyQueue[i] = identifyQueue[i + 1];
  }
  identifyQueueCount--;
  if (index >= 0 && request.triesLeft > 1)
  {
    request.triesLeft--;
    identifyQueue[identifyQueueCount++] = request;
  }
}

static void webHandleCommand(uint8_t client, const uint8_t *payload, size_t length)
{
  JsonDocument doc;
  if (deserializeJson(doc, payload, length))
  {
    webSendError(client, "BAD_JSON", "Invalid JSON command");
    return;
  }
  const char *action = doc["action"] | "";
  JsonObject data = doc["data"].as<JsonObject>();
  if (strcmp(action, "auth.login") == 0)
  {
    webHandleLogin(client, data);
    return;
  }
  if (strcmp(action, "auth.verify_otp") == 0)
  {
    webVerifyLoginOtp(client, data);
    return;
  }
  if (webAuthState[client] != AUTH_SIGNED_IN)
  {
    webSendAuthRequired(client);
    return;
  }
  if (strcmp(action, "auth.logout") == 0)
  {
    webAuthState[client] = AUTH_SIGNED_OUT;
    webAuthAccount[client] = -1;
    webHasPendingTotp[client] = false;
    webSendAuthRequired(client);
  }
  else if (strncmp(action, "account.", 8) == 0)
  {
    webHandleAccountCommand(client, action, data);
  }
  else if (strcmp(action, "state.get") == 0)
  {
    webSendState(client);
  }
  else if (strcmp(action, "alias.save") == 0)
  {
    saveAlias(client, data["mac"] | "", data["alias"] | "");
  }
  else if (strcmp(action, "alias.delete") == 0)
  {
    deleteAlias(client, data["mac"] | "");
  }
  else if (strcmp(action, "devices.clear") == 0)
  {
    clearAllDevices(client);
  }
  else if (strcmp(action, "node.delete") == 0)
  {
    // node.delete/group.save/group.delete only touch local bookkeeping
    // (Device/groups/groupDevices + NVS) - unlike scan.start/license.*/
    // device.config/device.getInfo they never key an ESP-NOW packet, so
    // there is no real reason for them to wait behind an in-flight job.
    // They used to sit after the webLicenseJobBusy() gate below along with
    // those radio-using actions, which meant e.g. creating a Group while a
    // Scan was still running (easily 5-20s with the multi-round retry
    // Scan does) silently failed with a BUSY error the app only shows in
    // the status bar for a few seconds - exactly the "works sometimes,
    // not others" the user reported. Moved up here with alias.save/
    // alias.delete/devices.clear, which already got this right.
    deleteNode(client, data["mac"] | "");
  }
  else if (strcmp(action, "group.save") == 0)
  {
    saveGroup(client, data["id"] | 0UL, data["name"] | "",
              data["members"].as<JsonArrayConst>());
  }
  else if (strcmp(action, "group.delete") == 0)
  {
    deleteGroup(client, data["id"] | 0UL);
  }
  else if (webLicenseJobBusy())
  {
    webSendError(client, "BUSY", "Master is running another ESP-NOW operation");
  }
  else if (strcmp(action, "scan.start") == 0)
  {
    const int requestedLimit = data["limit"] | DEFAULT_SCAN_LIMIT;
    if (requestedLimit < 1 || requestedLimit > MAX_DEVICES)
    {
      webSendError(client, "BAD_SCAN_LIMIT", "Enter a valid Node quantity");
      return;
    }
    beginScan(static_cast<uint16_t>(requestedLimit));
    webSendAck(client, action);
  }
  else if (strcmp(action, "scan.refresh") == 0)
  {
    beginRefresh(false);
    webSendAck(client, action);
  }
  else if (strcmp(action, "device.search") == 0)
  {
    beginSearch(client, data["deviceId"] | "");
  }
  else if (strcmp(action, "license.get") == 0)
  {
    beginGetLicenseJob(client, data);
  }
  else if (strcmp(action, "device.getInfo") == 0)
  {
    beginGetInfoJob(client, data);
  }
  else if (strcmp(action, "matrix.set") == 0)
  {
    beginSetMatrixJob(client, data);
  }
  else if (strcmp(action, "cardtype.set") == 0)
  {
    beginSetCardTypeJob(client, data);
  }
  else if (strcmp(action, "license.set") == 0)
  {
    beginCommandJob(client, JOB_SET, data);
  }
  else if (strcmp(action, "device.config") == 0)
  {
    beginCommandJob(client, JOB_CONFIG, data);
  }
  else if (strcmp(action, "device.identify") == 0)
  {
    handleIdentifyCommand(client, data);
  }
  else
  {
    webSendError(client, "UNKNOWN_ACTION", "Unsupported command");
  }
}

// ===================== USB serial transport =====================
// The ESP32-S3 in this project runs its Arduino Serial over the native USB
// (USBMode=hwcdc/CDCOnBoot=cdc — see .vscode/arduino.json), so the same USB
// cable used for flashing/monitoring can double as a wired, interference-free
// link for the desktop app: more stable than the WiFi AP (no RF, no reconnect
// churn, no 4-client cap). It speaks the exact same newline-delimited JSON
// {"action":...,"data":{...}} protocol as the WebSocket, dispatched through
// the same webHandleCommand() via the USB_CLIENT_ID pseudo-client, so every
// action and broadcast event (device.upsert, job, groups.changed, ...) is
// identical on both transports — only the framing/byte-transport differs.
constexpr size_t USB_LINE_MAX_LENGTH = 8192; // headroom for a full state/groups snapshot
static String usbLineBuffer;

inline void beginUsbBridge()
{
  // Requires the same auth.login as a WebSocket client: webAuthState[]
  // already defaults to AUTH_SIGNED_OUT (zero-initialized static array), so
  // this only needs to reset the account slot. The app's first command over
  // USB gets an auth.required reply just like a fresh WebSocket connection,
  // which drives the normal login dialog.
  webAuthAccount[USB_CLIENT_ID] = -1;
  usbLineBuffer = "";
  usbLineBuffer.reserve(256);
}

inline void handleUsbBridge()
{
  while (Serial.available() > 0)
  {
    const char c = static_cast<char>(Serial.read());
    if (c == '\n')
    {
      usbLineBuffer.trim();
      if (usbLineBuffer.length() > 0)
      {
        if (usbLineBuffer[0] == '{')
        {
          webHandleCommand(USB_CLIENT_ID,
                           reinterpret_cast<const uint8_t *>(usbLineBuffer.c_str()),
                           usbLineBuffer.length());
        }
        else
        {
          // Backward-compatible plain-text debug commands (scan/rescan/set/info).
          handleSerialCommand(usbLineBuffer);
        }
      }
      usbLineBuffer = "";
      continue;
    }
    if (c == '\r')
    {
      continue;
    }
    usbLineBuffer += c;
    if (usbLineBuffer.length() > USB_LINE_MAX_LENGTH)
    {
      Serial.println("USB line too long; dropped");
      usbLineBuffer = "";
    }
  }
}

static void webSocketEvent(uint8_t client, WStype_t type, uint8_t *payload, size_t length)
{
  if (type == WStype_CONNECTED)
  {
    webAuthState[client] = AUTH_SIGNED_OUT;
    webAuthAccount[client] = -1;
    webHasPendingTotp[client] = false;
    webSendAuthRequired(client);
  }
  else if (type == WStype_TEXT)
  {
    webHandleCommand(client, payload, length);
  }
  else if (type == WStype_DISCONNECTED)
  {
    webAuthState[client] = AUTH_SIGNED_OUT;
    webAuthAccount[client] = -1;
    webHasPendingTotp[client] = false;
  }
}

inline bool beginLocalWeb()
{
  // All Devices is intentionally RAM-only and always starts empty.
  Device = {};
  nod = 0;
  loadGroupsFromNvs();
  loadAccounts();
  const uint64_t suffix = ESP.getEfuseMac() & 0xFFFFFFULL;
  snprintf(localApSsid, sizeof(localApSsid), "HUB66S-%06llX", suffix);
  WiFi.mode(WIFI_AP_STA);
  WiFi.setSleep(false);
  WiFi.softAPConfig(IPAddress(192, 168, 4, 1), IPAddress(192, 168, 4, 1),
                    IPAddress(255, 255, 255, 0));
  return WiFi.softAP(localApSsid, "hub66s66", LOCAL_WIFI_CHANNEL, false, 4);
}

inline void startLocalWebServices()
{
  if (localWebStarted)
  {
    return;
  }
  localHttpServer.on("/", HTTP_GET, []()
                     {
                       localHttpServer.sendHeader("Cache-Control", "no-store");
                       localHttpServer.send_P(200, "text/html; charset=utf-8", LOCAL_WEB_PAGE);
                     });
  localHttpServer.on("/logo.png", HTTP_GET, []()
                     {
                       localHttpServer.sendHeader("Cache-Control", "public, max-age=86400");
                       localHttpServer.send_P(200, "image/png",
                                              reinterpret_cast<PGM_P>(IMAGE_PNG),
                                              sizeof(IMAGE_PNG));
                     });
  localHttpServer.begin();
  localWebSocket.begin();
  localWebSocket.onEvent(webSocketEvent);
  localWebSocket.enableHeartbeat(15000, 3000, 2);
  localWebStarted = true;
  Serial.printf("Local web ready: http://%s\n", WiFi.softAPIP().toString().c_str());
}

// Node tu day uptime/info moi UPTIME_PUSH_INTERVAL_MS (xem rcv/rcv.ino, hien
// la 1 phut) nhung day la broadcast khong ACK/retry, thinh thoang rot 1 goi
// la binh thuong - nen dat nguong im lang gap ~3 lan chu ky day (bo qua toi
// da ~2 lan rot lien tiep) truoc khi tu chuyen ONLINE -> OFFLINE, tranh bao
// sai chi vi mot goi bi mat song. Day la co che thu dong duy nhat tu chuyen
// mot node dang ONLINE sang OFFLINE ma khong can hub chay Scan/Refresh.
constexpr uint32_t NODE_STALE_TIMEOUT_MS = 3UL * 60UL * 1000UL; // 3 phut
constexpr uint32_t OFFLINE_SWEEP_INTERVAL_MS = 5000;             // Quet moi 5s, re voi <=100 node

inline void handleOfflineSweep()
{
  static uint32_t lastSweepMillis = 0;
  const uint32_t nowMillis = millis();
  if (nowMillis - lastSweepMillis < OFFLINE_SWEEP_INTERVAL_MS)
  {
    return;
  }
  lastSweepMillis = nowMillis;

  for (int i = 0; i < Device.deviceCount; ++i)
  {
    node_info_t &node = Device.nodes[i];
    if (node.responseState == NODE_ONLINE && node.lastResponseMillis != 0 &&
        nowMillis - node.lastResponseMillis > NODE_STALE_TIMEOUT_MS)
    {
      node.responseState = NODE_NO_RESPONSE;
      broadcastNode(i);
      Serial.printf("[OFFLINE] %s im lang qua %lus, chuyen sang OFFLINE\n",
                    node.uid, static_cast<unsigned long>(NODE_STALE_TIMEOUT_MS / 1000));
    }
  }
}

inline void handleLocalWeb()
{
  if (!localWebStarted)
  {
    return;
  }
  localHttpServer.handleClient();
  localWebSocket.loop();
  handleMasterJob();
  handleScan();
  handleOfflineSweep();
  handleIdentifyQueue();
}

#endif // LOCAL_WEB_H
