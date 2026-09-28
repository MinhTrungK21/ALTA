#ifndef PROTOCOL_HANDLER_H
#define PROTOCOL_HANDLER_H

#include <WiFi.h>
#include <ArduinoJson.h>
#include <MD5Builder.h>
#include "config.h"
#include <Preferences.h>

extern esp_now_recv_info lastRecvInfo;

extern uint8_t lastPacketMac[6];
extern volatile bool hasNewPacket;
extern int lastPacketLen;
extern uint8_t lastPacketData[sizeof(PayloadStruct)];

extern bool dang_gui;          // co dang gui
extern bool waitingSendResult; // Co cho ket qua gui
extern bool needRetry;         // Can gui lai
extern unsigned long lastTime; // Thoi diem gui cuoi cung
extern uint8_t lastResponseMac[6];
extern size_t lastResponseLen;
void enqueueReceivedPacket(const esp_now_recv_info *recv_info, const uint8_t *data, int len);

// Cau truc cho tin nhan ESP-NOW
extern PayloadStruct message;

// Khai bao Preferences la extern
extern Preferences preferences;
extern SemaphoreHandle_t preferencesMutex;

extern unsigned long runtime;
bool isValidLidString(const char *lid);

// Chuyen doi MAC thanh String
String macToString(const uint8_t *mac)
{
    char macStr[18];
    snprintf(macStr, sizeof(macStr), "%02X:%02X:%02X:%02X:%02X:%02X",
             mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
    return String(macStr);
}

// Tao tin nhan phan hoi
template <size_t DataCapacity>
String createMessage(const char *id_des, String mac_src, String mac_des, uint8_t opcode,
                     const StaticJsonDocument<DataCapacity> &data, unsigned long timestamp)
{
    // Tao data theo thu tu co dinh de hai ben tinh MD5 giong nhau.
    StaticJsonDocument<DataCapacity + JSON_OBJECT_SIZE(1)> responseData;
    JsonObjectConst sourceData = data.template as<JsonObjectConst>();
    for (JsonPairConst field : sourceData)
    {
        responseData[field.key()] = field.value();
    }

    String dataStr;
    serializeJson(responseData, dataStr);                                                   // Chuyen du lieu thanh chuoi JSON
    String auth = md5Hash(deviceUid, id_des, mac_src, mac_des, opcode, dataStr, timestamp); // Tao ma MD5 voi UID cua HUB

    StaticJsonDocument<512> jsonDoc;
    jsonDoc["id_src"] = deviceUid;  // UID duy nhat cua HUB, vi du HUB_72654
    jsonDoc["id_des"] = id_des;     // ID dich
    jsonDoc["mac_src"] = mac_src;   // MAC nguon, can de ben nhan kiem tra auth
    jsonDoc["mac_des"] = mac_des;   // MAC dich
    jsonDoc["opcode"] = opcode;     // Opcode
    jsonDoc["data"] = responseData; // Dung chinh data da dua vao chuoi MD5
    jsonDoc["time"] = timestamp;    // Thoi gian
    jsonDoc["auth"] = auth;         // Ma xac thuc

    String messageStr;
    serializeJson(jsonDoc, messageStr); // Chuyen thanh chuoi JSON
    return messageStr;
}

// Gui phan hoi
template <size_t DataCapacity>
void sendResponse(const char *id_des, String mac_src, uint8_t opcode,
                  const StaticJsonDocument<DataCapacity> &data, const uint8_t *targetMac,
                  unsigned long timestamp)
{
    String targetMacStr = macToString(targetMac);                                          // Chuyen doi targetMac thanh chuoi
    String output = createMessage(id_des, mac_src, targetMacStr, opcode, data, timestamp); // Phan hoi dung time cua request
    if (output.length() >= sizeof(message.payload))
    {
        // Serial.println(" Payload qua lon!");
        Serial.printf("❌ Payload qua lon (%u > %u), khong gui duoc\n", output.length(), sizeof(message.payload));
        led.setState(CONNECTION_ERROR);
        dang_gui = false;
        return;
    }
    output.toCharArray(message.payload, sizeof(message.payload)); // Chuyen vao payload
    lastResponseLen = output.length() + 1;
    memcpy(lastResponseMac, targetMac, sizeof(lastResponseMac));

    //---------------
    dang_gui = true;
    waitingSendResult = true;
    needRetry = false;
    lastTime = millis();
    //---------------

    esp_now_peer_info_t peerInfo = {};
    memcpy(peerInfo.peer_addr, targetMac, 6);
    peerInfo.channel = 1;
    peerInfo.encrypt = false;
    if (!esp_now_is_peer_exist(targetMac))
    {
        if (esp_now_add_peer(&peerInfo) != ESP_OK)
        {
            Serial.println("❌ Failed to add peer!");
            return;
        }
        Serial.println("Đã thêm peer");
        delay(100); // Doi mot chut de dam bao peer da duoc them
    }

    // esp_now_send(targetMac, (uint8_t *)&message, sizeof(message)); // Gui qua ESP-NOW
    esp_err_t sendResult = esp_now_send(targetMac, (uint8_t *)message.payload, lastResponseLen); // Gui qua ESP-NOW
    // Ghi ca khoi trong mot lan de callback ESP-NOW khong chen vao giua JSON.
    String sendLog;
    sendLog.reserve(output.length() + 40);
    sendLog = "\n Da gui phan hoi:\n";
    sendLog += output;
    sendLog += '\n';
    Serial.print(sendLog);

    //---------------
    if (sendResult != ESP_OK)
    {
        Serial.printf("❌ Loi gui ESP-NOW: %d\n", sendResult);
        waitingSendResult = false;
        needRetry = true;
    }
    //---------------

    // Khong xoa peer tai day: callback gui la bat dong bo va retry van can peer.
#if 0
    if (esp_now_is_peer_exist(targetMac))
    {
        esp_now_del_peer(targetMac);
    }
    else
    {
        Serial.println("Peer không tồn tại");
    }
#endif
}

// ---- Chong dung do khi Scan (xem beginScan()/handleScan() trong local_web.h) ----
// Hub broadcast Scan bang chinh opcode LIC_GET_LICENSE (id_des="0"), kem theo
// scan_id/slot_count/slot_ms de moi board tra loi vao 1 khe gio ngau nhien rai
// trong ca so, tranh viec ca tram board tra loi cung mot luc va dung song nhau
// tren khong trung (ESP-NOW khong co co che tranh dung do nhu CSMA/CA). Truoc
// ban sua nay node bo qua hoan toan cac truong do va tra loi ngay lap tuc cho
// moi LIC_GET_LICENSE - ke ca ban Scan - nen Scan cang nhieu board cang de mat
// goi giua chung (vd 67 board cam thi Scan chi thay ~63 board).
struct PendingScanReply
{
  bool active = false;
  uint32_t fireAtMillis = 0;
  char idSrc[16] = {};
  uint8_t mac[6] = {};
  unsigned long packetTime = 0;
};
static PendingScanReply pendingScanReply;

// Goi lien tuc (moi ~10ms, xem communicationTask trong rcv.ino) de gui phan hoi
// Scan dung luc da boc duoc, khong dung delay()/vTaskDelay() chan ca task cho.
inline void sendPendingScanReplyIfDue()
{
  if (!pendingScanReply.active ||
      static_cast<int32_t>(millis() - pendingScanReply.fireAtMillis) < 0)
  {
    return;
  }
  pendingScanReply.active = false;
  StaticJsonDocument<512> respDoc;
  respDoc["created"] = globalLicense.created;
  respDoc["expired"] = globalLicense.expired_flag ? 1 : 0;
  respDoc["duration"] = globalLicense.duration;
  respDoc["remain"] = globalLicense.remain;
  respDoc["lid"] = config_lid;
  respDoc["cardType"] = card_type;
  respDoc["group"] = config_group;
  JsonObject responseMatrix = respDoc.createNestedObject("Matrix");
  responseMatrix["x"] = matrix_x;
  responseMatrix["y"] = matrix_y;
  respDoc["status"] = 0;
  const String myMac = WiFi.macAddress();
  sendResponse(pendingScanReply.idSrc, myMac, LIC_GET_LICENSE | 0x80, respDoc,
              pendingScanReply.mac, pendingScanReply.packetTime);
  Serial.println("[LICENSE] Da gui thong tin (Scan) cho " + String(deviceUid));
  led.setState(FLASH_TWICE);
  if (globalLicense.expired_flag || globalLicense.remain <= 0)
  {
    led.setState(LICENSE_EXPIRED);
  }
  else
  {
    led.setState(NORMAL_STATUS);
  }
}

// LID chi la ten/vi tri do master dat, khong dung de dinh danh device.
bool isValidLidString(const char *lid)
{
    if (lid == nullptr)
        return false;

    const size_t len = strnlen(lid, LID_BUF_LEN);
    if (len == 0 || len > LID_MAX_LEN)
        return false;

    for (size_t i = 0; i < len; ++i)
    {
        const char c = lid[i];
        if (!((c >= 'A' && c <= 'Z') ||
              (c >= 'a' && c <= 'z') ||
              (c >= '0' && c <= '9')))
            return false;
    }
    return true;
}

bool isValidCardType(const char *value)
{
    return value != nullptr &&
           (strcmp(value, "OB") == 0 || strcmp(value, "R") == 0);
}

// Ten Group/vi tri chi la nhan hien thi do master gan (khong dung dinh danh
// device), nen chi kiem tra do dai vua voi buffer chu khong gioi han ky tu
// (ten group co the co dau tieng Viet, khoang trang...).
bool isValidGroupName(const char *value)
{
    if (value == nullptr)
        return false;
    const size_t len = strlen(value);
    return len > 0 && len < sizeof(config_group);
}

bool lockPreferences()
{
    return preferencesMutex != nullptr &&
           xSemaphoreTake(preferencesMutex, portMAX_DELAY) == pdTRUE;
}

void unlockPreferences()
{
    if (preferencesMutex != nullptr)
        xSemaphoreGive(preferencesMutex);
}

bool putNvsResult(const char *key, size_t written, size_t expected)
{
    if (written == expected)
        return true;
    Serial.printf("[NVS] LOI ghi key '%s' (%u/%u)\n",
                  key, static_cast<unsigned>(written), static_cast<unsigned>(expected));
    return false;
}

// Tong so giay hoat dong tinh tu lan dau flash, cong don qua cac lan mat nguon.
// bootUptimeBaseSec = tong da tich luy truoc lan khoi dong hien tai (doc tu NVS
// luc setup); esp_timer_get_time() = so micro giay tu luc board vua bat nguon
// lan nay. Cong hai gia tri lai la tong uptime "chay mai khong reset".
extern uint32_t bootUptimeBaseSec;
static const char *SYSINFO_NAMESPACE = "sysinfo";

uint32_t getTotalUptimeSeconds()
{
    const uint32_t elapsedThisBoot = static_cast<uint32_t>(esp_timer_get_time() / 1000000ULL);
    return bootUptimeBaseSec + elapsedThisBoot;
}

// Cung mot payload duoc dung ca cho phan hoi LIC_INFO (theo yeu cau) lan
// cho pushUptimeInfo() (tu day, khong can request) de 2 duong khong lech schema.
StaticJsonDocument<384> buildDeviceInfoDoc()
{
    StaticJsonDocument<384> doc;
    // temperatureRead() tra ve nhiet do ben trong chip ESP32-S3.
    const float temperatureC = roundf(temperatureRead() * 10.0f) / 10.0f;
    const uint32_t totalUptimeSeconds = getTotalUptimeSeconds();

    doc["device_id"] = deviceUid;
    doc["lid"] = config_lid;
    doc["firmware_version"] = FIRMWARE_VERSION;
    doc["voltage_v"] = 3.3;
    doc["temperature_c"] = temperatureC;
    doc["uptime_s"] = totalUptimeSeconds;
    doc["uptime_m"] = totalUptimeSeconds / 60;
    doc["protocol"] = "WiFi";
    doc["status"] = 0;
    return doc;
}

// Tu dong đẩy uptime/info len broadcast, khong can master hoi truoc (xem
// pushUptimeInfoIfDue() trong rcv.ino cho chu ky va jitter). Bo qua neu dang
// co mot response that (dang_gui) can gui/gui lai, de khong de push lam hong
// trang thai retry cua no.
void pushUptimeInfo()
{
    if (dang_gui)
    {
        Serial.println("[PUSH] Bo qua: dang ban gui/gui lai response khac");
        return;
    }
    String myMac = WiFi.macAddress();
    StaticJsonDocument<384> pushDoc = buildDeviceInfoDoc();
    sendResponse("BROADCAST", myMac, LIC_INFO_RESPONSE, pushDoc, senderMac, millis());
    Serial.println("📡 Tu dong day uptime/info (broadcast).");
}

// Ghi tong uptime hien tai xuong NVS moi phut (goi tu updateLicense() trong
// rcv.ino) de neu mat nguon dot ngot, lan khoi dong sau chi hut toi da 1 phut
// chua kip luu. Vung NVS 20KB (5 trang 4KB, ~126 entry/trang) voi tan suat
// ghi nay chi can ~4.170 lan xoa trang/nam, van an toan hang chuc nam ngay
// ca voi flash re chi chiu 10.000 chu ky xoa.
bool saveUptimeData()
{
    if (!lockPreferences())
    {
        Serial.println("[NVS] LOI: khong lock duoc mutex (uptime)");
        return false;
    }
    if (!preferences.begin(SYSINFO_NAMESPACE, false))
    {
        Serial.println("[NVS] Khong mo duoc de ghi uptime");
        unlockPreferences();
        return false;
    }
    const bool ok = putNvsResult("uptime_s", preferences.putULong("uptime_s", getTotalUptimeSeconds()), 4);
    preferences.end();
    unlockPreferences();
    return ok;
}

// Doc tong uptime da tich luy tu cac lan chay truoc lam moc khoi dau cho lan nay.
bool loadUptimeData()
{
    if (!lockPreferences())
    {
        Serial.println("[NVS] LOI: khong lock duoc mutex (uptime)");
        return false;
    }
    const bool opened = preferences.begin(SYSINFO_NAMESPACE, true);
    bootUptimeBaseSec = opened ? preferences.getULong("uptime_s", 0) : 0;
    if (opened)
        preferences.end();
    unlockPreferences();
    return opened;
}

// Ghi 1 lan (khong lock/verify) - dung noi bo boi saveLicenseData(), cung
// mot kieu voi writeDeviceConfigOnce().
static bool writeLicenseDataOnce()
{
    if (!preferences.begin("license", false))
    {
        Serial.println("[NVS] Khong mo duoc de ghi");
        return false;
    }
    bool ok = putNvsResult("lid", preferences.putString("lid", globalLicense.lid),
                           strlen(globalLicense.lid));
    ok = putNvsResult("created", preferences.putULong("created", static_cast<uint32_t>(globalLicense.created)), 4) && ok;
    ok = putNvsResult("duration", preferences.putULong("duration", globalLicense.duration), 4) && ok;
    ok = putNvsResult("remain", preferences.putULong("remain", globalLicense.remain), 4) && ok;
    ok = putNvsResult("expired_flag", preferences.putBool("expired_flag", globalLicense.expired_flag), 1) && ok;
    ok = putNvsResult("runtime", preferences.putULong("runtime", runtime), 4) && ok;
    ok = putNvsResult("expiredMode", preferences.putUChar("expiredMode", expiredLedMode), 1) && ok;
    ok = putNvsResult("cycleMin", preferences.putULong("cycleMin", expiredCycleMinMinutes), 4) && ok;
    ok = putNvsResult("cycleMax", preferences.putULong("cycleMax", expiredCycleMaxMinutes), 4) && ok;
    preferences.end();
    return ok;
}

// Doc lai ngay sau khi ghi de xac nhan - cung ly do voi verifyDeviceConfigWritten():
// nvs_commit() tren mot vai board da quan sat duoc bao "thanh cong" du du lieu
// khong thuc su duoc luu ben vung. Set License truoc day khong co buoc nay nen
// khong the biet duoc board nao dang gap chinh van de tuong tu Set Matrix.
static bool verifyLicenseDataWritten()
{
    if (!preferences.begin("license", true))
    {
        Serial.println("[NVS][VERIFY] Khong mo lai duoc license de doc xac nhan");
        return false;
    }
    char readLid[LID_BUF_LEN] = {};
    preferences.getString("lid", readLid, sizeof(readLid));
    const uint32_t readCreated = preferences.getULong("created", UINT32_MAX);
    const uint32_t readDuration = preferences.getULong("duration", UINT32_MAX);
    const uint32_t readRemain = preferences.getULong("remain", UINT32_MAX);
    const bool readExpiredFlag = preferences.getBool("expired_flag", !globalLicense.expired_flag);
    const uint32_t readRuntime = preferences.getULong("runtime", UINT32_MAX);
    const uint8_t readExpiredMode = preferences.getUChar("expiredMode", 0xFF);
    const uint32_t readCycleMin = preferences.getULong("cycleMin", UINT32_MAX);
    const uint32_t readCycleMax = preferences.getULong("cycleMax", UINT32_MAX);
    preferences.end();

    const bool match = strcmp(readLid, globalLicense.lid) == 0 &&
                       readCreated == static_cast<uint32_t>(globalLicense.created) &&
                       readDuration == globalLicense.duration &&
                       readRemain == globalLicense.remain &&
                       readExpiredFlag == globalLicense.expired_flag &&
                       readRuntime == runtime &&
                       readExpiredMode == expiredLedMode &&
                       readCycleMin == expiredCycleMinMinutes &&
                       readCycleMax == expiredCycleMaxMinutes;
    if (!match)
    {
        Serial.printf("[NVS][VERIFY] LOI: doc lai khong khop - lid=%s(muon %s) duration=%lu(muon %lu) "
                      "remain=%lu(muon %lu) runtime=%lu(muon %lu)\n",
                      readLid, globalLicense.lid,
                      static_cast<unsigned long>(readDuration), static_cast<unsigned long>(globalLicense.duration),
                      static_cast<unsigned long>(readRemain), static_cast<unsigned long>(globalLicense.remain),
                      static_cast<unsigned long>(readRuntime), static_cast<unsigned long>(runtime));
    }
    return match;
}

// Luu gia tri license hien tai vao NVS. Ghi-roi-doc-lai-xac-nhan, thu toi da 3
// lan - cung co che voi saveDeviceConfig() (xem comment tren
// verifyDeviceConfigWritten()), de license khong am tham "ghi thanh cong" tren
// board co flash yeu trong khi thuc ra khong luu duoc gi.
bool saveLicenseData(bool verbose = true)
{
    if (!isValidLidString(globalLicense.lid))
    {
        if (verbose)
            Serial.println("[NVS] LOI: lid khong hop le, bo qua ghi");
        return false;
    }

    if (!lockPreferences())
    {
        Serial.println("[NVS] LOI: khong lock duoc mutex");
        return false;
    }

    bool ok = false;
    constexpr int MAX_SAVE_ATTEMPTS = 3;
    for (int attempt = 1; attempt <= MAX_SAVE_ATTEMPTS && !ok; ++attempt)
    {
        if (!writeLicenseDataOnce())
        {
            Serial.printf("[NVS] Ghi license that bai lan %d/%d\n", attempt, MAX_SAVE_ATTEMPTS);
            continue;
        }
        if (verifyLicenseDataWritten())
        {
            ok = true;
        }
        else
        {
            Serial.printf("[NVS] Xac nhan sau ghi license that bai lan %d/%d, thu lai...\n", attempt, MAX_SAVE_ATTEMPTS);
        }
    }

    unlockPreferences();

    if (verbose)
    {
        Serial.printf("[NVS] SAVE: lid=%s duration=%lu remain=%lu runtime=%lu result=%s\n",
                      globalLicense.lid,
                      static_cast<unsigned long>(globalLicense.duration),
                      static_cast<unsigned long>(globalLicense.remain),
                      static_cast<unsigned long>(runtime), ok ? "OK" : "FAIL");
    }
    return ok;
}

// config_lid/cardType/matrix_x/matrix_y song rieng namespace "devcfg", tach
// khoi namespace "license" (noi remain/duration/expired_flag duoc ghi dinh
// ky moi phut hoac ngay khi license het han). Truoc day ca hai nhom deu ghi
// chung "license": neu rut nguon dung luc license dang ghi NVS (vi du dung
// luc remain vua chuyen ve 0), du lieu vi tri/loai board co the bi cuon theo
// mat luon du ban than no khong doi. Tach namespace de 2 nhom du lieu nay
// khong con dung chung chu ky ghi/trang flash voi nhau.
static const char *DEVCFG_NAMESPACE = "devcfg";
static const char *LEGACY_LICENSE_NAMESPACE = "license";

// Ghi mot blob "de dem" roi xoa ngay de buoc NVS cap trang moi cho lan ghi
// thuc su ngay sau do. Chi la phuong an thu them (CHUA duoc chung minh la co
// tac dung: 1 lan thu tren board dang loi that bai van khong cuu duoc). Nguyen
// nhan that cua loi ghi devcfg tren cac board nap qua Maker Workshop la
// bootloader/bang phan vung cu (xem useProgrammer trong .vscode/arduino.json),
// nap lai bang cong thuc upload day du da sua duoc - khong phai flash hong.
// Chi goi sau khi 3 lan ghi thuong da that bai (xem saveDeviceConfig()).
static void forceDevcfgPageRollover()
{
    if (!preferences.begin(DEVCFG_NAMESPACE, false))
        return;
    static const uint8_t padBuf[3584] = {};
    preferences.putBytes("__pad", padBuf, sizeof(padBuf));
    preferences.remove("__pad");
    preferences.end();
}

// Ghi 1 lan (khong lock/verify) - dung noi bo boi saveDeviceConfig().
static bool writeDeviceConfigOnce()
{
    if (!preferences.begin(DEVCFG_NAMESPACE, false))
    {
        Serial.println("[NVS] Khong mo duoc de ghi config_lid");
        return false;
    }
    bool ok = putNvsResult("config_lid",
                           preferences.putString("config_lid", config_lid),
                           strlen(config_lid));
    ok = putNvsResult("cardType", preferences.putString("cardType", card_type),
                      strlen(card_type)) && ok;
    ok = putNvsResult("matrix_x", preferences.putInt("matrix_x", matrix_x), 4) && ok;
    ok = putNvsResult("matrix_y", preferences.putInt("matrix_y", matrix_y), 4) && ok;
    ok = putNvsResult("group", preferences.putString("group", config_group),
                      strlen(config_group)) && ok;
    preferences.end();
    return ok;
}

// Doc lai ngay sau khi ghi de xac nhan gia tri tren flash khop voi RAM - phat
// hien truong hop nvs_commit() bao "thanh cong" (putXxx/putNvsResult deu OK)
// nhung du lieu thuc te khong duoc luu ben vung (quan sat duoc tren mot vai
// board cu the: doc lai o lan boot sau thi namespace khong con mo duoc, du
// flash dump cho thay chi co du lieu cua WiFi driver, khong co gi cua app -
// nghia la ban than nvs_commit() da bao ket qua sai tren chip flash bi yeu).
static bool verifyDeviceConfigWritten()
{
    if (!preferences.begin(DEVCFG_NAMESPACE, true))
    {
        Serial.println("[NVS][VERIFY] Khong mo lai duoc devcfg de doc xac nhan");
        return false;
    }
    char readLid[LID_BUF_LEN] = {};
    char readCardType[3] = {};
    char readGroup[24] = {};
    preferences.getString("config_lid", readLid, sizeof(readLid));
    preferences.getString("cardType", readCardType, sizeof(readCardType));
    preferences.getString("group", readGroup, sizeof(readGroup));
    const int32_t readMatrixX = preferences.getInt("matrix_x", INT32_MIN);
    const int32_t readMatrixY = preferences.getInt("matrix_y", INT32_MIN);
    preferences.end();

    const bool match = strcmp(readLid, config_lid) == 0 &&
                       strcmp(readCardType, card_type) == 0 &&
                       strcmp(readGroup, config_group) == 0 &&
                       readMatrixX == matrix_x && readMatrixY == matrix_y;
    if (!match)
    {
        Serial.printf("[NVS][VERIFY] LOI: doc lai khong khop - lid=%s(muon %s) cardType=%s(muon %s) group=%s(muon %s) matrix=(%ld,%ld)(muon (%ld,%ld))\n",
                      readLid, config_lid, readCardType, card_type, readGroup, config_group,
                      static_cast<long>(readMatrixX), static_cast<long>(readMatrixY),
                      static_cast<long>(matrix_x), static_cast<long>(matrix_y));
    }
    return match;
}

// KHONG con kiem tra lai isValidLidString(config_lid)/isValidCardType(card_type)
// o day nhu truoc: 2 gate do doc TRANG THAI HIEN TAI cua ca 2 bien toan cuc,
// bat ke request nay co dong cham toi truong do hay khong. CONFIG_LID (o tren,
// noi goi ham nay) da tu validate rieng gia tri MOI cua tung truong THUC SU co
// trong request (lidValid/cardTypeValid) truoc khi gan vao config_lid/card_type,
// nen kiem lai o day la du thua cho truong dang duoc ghi - nhung lai la 1 bay:
// vi du 1 lenh Set Matrix (khong dong toi cardType) se bi saveDeviceConfig() tu
// choi ghi hoan toan chi vi card_type dang giu 1 gia tri khong hop le tu truoc
// (con lai tu 1 lan ghi/khoi phuc loi cu) - board do se KHONG BAO GIO Set Matrix
// thanh cong duoc nua, vi khong co request nao rieng cho card_type de sua no.
// Day chinh la nguyen nhan Set Matrix/Set Card Type bi ket qua thap bat thuong
// trong khi Set License (dung bien rieng globalLicense.lid, khong dinh vao gate
// nay) van binh thuong.
bool saveDeviceConfig()
{
    if (!lockPreferences())
    {
        Serial.println("[NVS] LOI: khong lock duoc mutex");
        return false;
    }

    bool ok = false;
    constexpr int MAX_SAVE_ATTEMPTS = 3;
    for (int attempt = 1; attempt <= MAX_SAVE_ATTEMPTS && !ok; ++attempt)
    {
        if (!writeDeviceConfigOnce())
        {
            Serial.printf("[NVS] Ghi devcfg that bai lan %d/%d\n", attempt, MAX_SAVE_ATTEMPTS);
            continue;
        }
        if (verifyDeviceConfigWritten())
        {
            ok = true;
        }
        else
        {
            Serial.printf("[NVS] Xac nhan sau ghi that bai lan %d/%d, thu lai...\n", attempt, MAX_SAVE_ATTEMPTS);
        }
    }

    if (!ok)
    {
        // 3 lan ghi thang deu that bai xac nhan - thu buoc NVS chuyen trang
        // (xem forceDevcfgPageRollover()) roi ghi lai 1 lan cuoi. Chi den day
        // moi lam, khong phai tu dau, de khong hao mon them flash cho board
        // dang ghi binh thuong.
        Serial.println("[NVS] 3 lan ghi thang deu that bai - thu buoc chuyen trang NVS...");
        forceDevcfgPageRollover();
        if (writeDeviceConfigOnce() && verifyDeviceConfigWritten())
        {
            ok = true;
            Serial.println("[NVS] Thanh cong sau khi chuyen trang");
        }
        else
        {
            Serial.println("[NVS] Van that bai sau khi chuyen trang");
        }
    }

    unlockPreferences();
    return ok;
}

bool parseLidValue(JsonVariantConst value, char *destination)
{
    if (destination == nullptr)
        return false;

    destination[0] = '\0';
    if (value.is<const char *>())
    {
        const char *text = value.as<const char *>();
        if (text == nullptr || strnlen(text, LID_BUF_LEN) > LID_MAX_LEN)
            return false;
        strlcpy(destination, text, LID_BUF_LEN);
    }
    else if (value.is<int>())
    {
        const int written = snprintf(destination, LID_BUF_LEN, "%d", value.as<int>());
        if (written <= 0 || written > static_cast<int>(LID_MAX_LEN))
            return false;
    }
    else
    {
        return false;
    }

    destination[LID_MAX_LEN] = '\0';
    return isValidLidString(destination);
}

bool loadNvsLid(const char *key, char *destination, const char *defaultValue)
{
    strlcpy(destination, defaultValue, LID_BUF_LEN);
    destination[LID_MAX_LEN] = '\0';

    const size_t readLen = preferences.getString(key, destination, LID_BUF_LEN);
    destination[LID_MAX_LEN] = '\0';
    if (readLen == 0)
    {
        const bool keyExists = preferences.isKey(key);
        if (keyExists)
            Serial.printf("[NVS] LOI doc key '%s' (sai kieu hoac qua dai)\n", key);
        return !keyExists && isValidLidString(destination);
    }

    if (!isValidLidString(destination))
    {
        Serial.printf("[NVS] LOI: key '%s' khong hop le\n", key);
        strlcpy(destination, defaultValue, LID_BUF_LEN);
        destination[LID_MAX_LEN] = '\0';
        return false;
    }
    return true;
}

// Doc config_lid/cardType/matrix_x/matrix_y tu namespace "devcfg". Thiet bi
// da tung set truoc khi co ban firmware nay van con du lieu nam trong
// namespace "license" cu - neu "devcfg" chua co key nao thi tu dong doc bu
// tu "license" 1 lan roi ghi ngay sang "devcfg", de tu lan boot sau tro di
// khong con phai dung toi "license" cho nhom du lieu nay nua.
bool loadDeviceConfig()
{
    if (!lockPreferences())
    {
        Serial.println("[NVS] LOI: khong lock duoc mutex (devcfg)");
        return false;
    }

    bool hasLid = false;
    bool hasCardType = false;
    bool hasMatrix = false;
    bool hasGroup = false;
    const bool devcfgOpened = preferences.begin(DEVCFG_NAMESPACE, true);
    if (devcfgOpened)
    {
        hasLid = preferences.isKey("config_lid");
        hasCardType = preferences.isKey("cardType");
        hasMatrix = preferences.isKey("matrix_x") && preferences.isKey("matrix_y");
        hasGroup = preferences.isKey("group");

        if (hasGroup)
        {
            char loadedGroup[24] = {};
            const size_t groupLen = preferences.getString("group", loadedGroup, sizeof(loadedGroup));
            hasGroup = groupLen > 0;
            if (hasGroup)
                strlcpy(config_group, loadedGroup, sizeof(config_group));
        }
        if (hasLid)
        {
            char configLidDefault[LID_BUF_LEN] = {};
            strlcpy(configLidDefault, config_lid, sizeof(configLidDefault));
            hasLid = loadNvsLid("config_lid", config_lid, configLidDefault);
        }
        if (hasCardType)
        {
            char loadedCardType[3] = {};
            const size_t cardTypeLen = preferences.getString("cardType", loadedCardType, sizeof(loadedCardType));
            if (cardTypeLen > 0 && isValidCardType(loadedCardType))
                strlcpy(card_type, loadedCardType, sizeof(card_type));
            else
            {
                Serial.println("[NVS] LOI: cardType (devcfg) khong hop le, dung gia tri mac dinh");
                hasCardType = false;
            }
        }
        if (hasMatrix)
        {
            matrix_x = preferences.getInt("matrix_x", matrix_x);
            matrix_y = preferences.getInt("matrix_y", matrix_y);
        }
        preferences.end();
    }

    if ((!hasLid || !hasCardType || !hasMatrix) && preferences.begin(LEGACY_LICENSE_NAMESPACE, true))
    {
        if (!hasLid && preferences.isKey("config_lid"))
        {
            char configLidDefault[LID_BUF_LEN] = {};
            strlcpy(configLidDefault, config_lid, sizeof(configLidDefault));
            hasLid = loadNvsLid("config_lid", config_lid, configLidDefault);
        }
        if (!hasCardType && preferences.isKey("cardType"))
        {
            char loadedCardType[3] = {};
            const size_t cardTypeLen = preferences.getString("cardType", loadedCardType, sizeof(loadedCardType));
            if (cardTypeLen > 0 && isValidCardType(loadedCardType))
            {
                strlcpy(card_type, loadedCardType, sizeof(card_type));
                hasCardType = true;
            }
        }
        if (!hasMatrix && preferences.isKey("matrix_x") && preferences.isKey("matrix_y"))
        {
            matrix_x = preferences.getInt("matrix_x", matrix_x);
            matrix_y = preferences.getInt("matrix_y", matrix_y);
            hasMatrix = true;
        }
        preferences.end();

        if (preferences.begin(DEVCFG_NAMESPACE, false))
        {
            preferences.putString("config_lid", config_lid);
            preferences.putString("cardType", card_type);
            preferences.putInt("matrix_x", matrix_x);
            preferences.putInt("matrix_y", matrix_y);
            preferences.end();
            Serial.println("[NVS] Da migrate config_lid/cardType/matrix tu 'license' sang 'devcfg'");
        }
    }

    unlockPreferences();
    return hasLid || hasCardType || hasMatrix || hasGroup;
}

// Tai gia tri license cuoi cung tu NVS khi khoi dong.
bool loadLicenseData()
{
    loadDeviceConfig();

    if (!lockPreferences())
    {
        Serial.println("[NVS] LOI: khong lock duoc mutex");
        return false;
    }
    if (!preferences.begin(LEGACY_LICENSE_NAMESPACE, true))
    {
        Serial.println("[NVS] Khong mo duoc de doc");
        unlockPreferences();
        return false;
    }

    char lidDefault[LID_BUF_LEN] = {};
    strlcpy(lidDefault, config_lid, sizeof(lidDefault));
    const bool lidLoaded = loadNvsLid("lid", globalLicense.lid, lidDefault);
    globalLicense.created = preferences.getULong("created", 0);
    globalLicense.duration = preferences.getULong("duration", 0);
    globalLicense.remain = preferences.getULong("remain", 0);
    globalLicense.expired_flag = preferences.getBool("expired_flag", false);
    runtime = preferences.getULong("runtime", 0);
    expiredLedMode = preferences.getUChar("expiredMode", 0); // 0 = Random (mac dinh)
    expiredCycleMinMinutes = preferences.getULong("cycleMin", expiredCycleMinMinutes);
    expiredCycleMaxMinutes = preferences.getULong("cycleMax", expiredCycleMaxMinutes);
    const bool hasLicense = lidLoaded && preferences.isKey("lid") &&
                            preferences.isKey("duration") &&
                            preferences.isKey("remain");
    preferences.end();
    unlockPreferences();

    if (!hasLicense)
    {
        globalLicense.created = 0;
        globalLicense.duration = 0;
        globalLicense.remain = 0;
        globalLicense.expired_flag = true;
        runtime = 0;
    }

    Serial.printf("[NVS] LOAD: lid=%s config_lid=%s cardType=%s matrix=(%ld,%ld) duration=%lu remain=%lu runtime=%lu expiredLedMode=%u cycle=(%lu,%lu)phut result=%s\n",
                  globalLicense.lid, config_lid, card_type,
                  static_cast<long>(matrix_x), static_cast<long>(matrix_y),
                  static_cast<unsigned long>(globalLicense.duration),
                  static_cast<unsigned long>(globalLicense.remain),
                  static_cast<unsigned long>(runtime), expiredLedMode,
                  static_cast<unsigned long>(expiredCycleMinMinutes),
                  static_cast<unsigned long>(expiredCycleMaxMinutes), hasLicense ? "OK" : "FAIL");
    return hasLicense;
}
void onReceive(const esp_now_recv_info *recv_info, const uint8_t *incomingData, int len)
{
    // // Copy nguyen struct
    // lastRecvInfo = *recv_info;
    // // Copy payload (gioi han kich thuoc)
    // lastPacketLen = min(len, (int)sizeof(lastPacketData));
    // memcpy(lastPacketData, incomingData, lastPacketLen);
    // // Danh dau co goi moi
    // hasNewPacket = true;

    enqueueReceivedPacket(recv_info, incomingData, len);
}

// Xu ly du lieu nhan duoc
void xu_ly_data(const esp_now_recv_info *recv_info, const uint8_t *incomingData, int len)
{
    const uint8_t *mac_addr = lastPacketMac;
    (void)recv_info; // da sao luu MAC nen tranh canh bao bien khong dung
    // const uint8_t *mac_addr = recv_info->src_addr;
    String myMac = WiFi.macAddress();

    // Phan tich JSON
    StaticJsonDocument<512> doc;
    DeserializationError error = deserializeJson(doc, incomingData, len);
    if (error)
    {
        // Khong doc duoc JSON thi khong the kiem tra auth.
        // Bo qua im lang: khong gui ESP-NOW, khong in Serial, khong doi LED.
        return;
    }
    const bool validPacket =
        doc["id_src"].is<const char *>() &&
        (doc["id_des"].is<const char *>() ||
         (doc["id_des"].is<int>() && doc["id_des"].as<int>() == 0)) &&
        doc["mac_src"].is<const char *>() &&
        doc["mac_des"].is<const char *>() &&
        doc["opcode"].is<int>() &&
        doc["data"].is<JsonObject>() &&
        doc["time"].is<uint32_t>() &&
        doc["auth"].is<const char *>();
    if (!validPacket)
    {
        // Thieu truong bat buoc thi auth khong dang tin cay: bo qua im lang.
        return;
    }

    // Lay cac truong tu goi tin
    const char *id_src = doc["id_src"].as<const char *>(); // UID cua thiet bi gui, vi du LIC_E139C
    String idDes = doc["id_des"].is<const char *>()
                       ? doc["id_des"].as<String>()
                       : String(doc["id_des"].as<int>()); // UID dich hoac 0 (broadcast)
    String mac_src = doc["mac_src"].as<String>();          // MAC cua thiet bi gui
    String mac_des = doc["mac_des"].as<String>();          // MAC cua thiet bi nhan
    uint8_t opcode = doc["opcode"].as<uint8_t>();
    String dataStr;
    serializeJson(doc["data"], dataStr);
    unsigned long packetTime = doc["time"].as<long>();
    String receivedAuth = doc["auth"].as<String>();

    // Device ID co dinh la deviceUid; 0 duoc dung cho goi broadcast.
    const bool targetsDevice = (idDes.equalsIgnoreCase(deviceUid) || idDes == "0");
    if (!targetsDevice)
    {
        return;
    }

    // Kiem tra xac thuc MD5
    String calculatedAuth = md5Hash(id_src, idDes.c_str(), mac_src, mac_des, opcode, dataStr, packetTime);
    if (!receivedAuth.equalsIgnoreCase(calculatedAuth))
    {
        // Goi khong thuoc he thong: bo qua im lang de tranh bi loi dung gay DoS.
        return;
    }
    // Tao tron khoi debug truoc, sau do ghi Serial mot lan. Neu in tung dong,
    // callback gui ESP-NOW co the chen log vao giua va lam JSON bi dut/chong chu.
    String receiveLog;
    receiveLog.reserve(static_cast<size_t>(len) * 2U + 160U);
    receiveLog = "\n Nhan package tin:\n";
    receiveLog += "✅ Xac thuc MD5 thanh cong!\nFrom MAC: ";
    for (int i = 0; i < 6; i++)
    {
        char macByte[3];
        snprintf(macByte, sizeof(macByte), "%02X", mac_addr[i]);
        receiveLog += macByte;
        if (i < 5)
            receiveLog += ':';
    }
    receiveLog += "\nDevice ID: ";
    receiveLog += deviceUid;
    receiveLog += "\nFull received packet:\nOpcode: 0x";
    char opcodeText[3];
    snprintf(opcodeText, sizeof(opcodeText), "%02X", opcode);
    receiveLog += opcodeText;
    receiveLog += '\n';
    serializeJsonPretty(doc, receiveLog);
    receiveLog += '\n';
    Serial.print(receiveLog);

    // Xu ly theo opcode
    switch (opcode)
    {
    case LIC_SET_LICENSE:
    {
        JsonObject data = doc["data"].as<JsonObject>();
        char incomingLid[LID_BUF_LEN] = {};
        bool lidFitsBuffer = false;
        if (data.containsKey("lid"))
            lidFitsBuffer = parseLidValue(data["lid"], incomingLid);
        else
            lidFitsBuffer = parseLidValue(doc["lid"], incomingLid);
        String id = data["id"].is<const char *>()
                        ? data["id"].as<String>()
                        : String(data["id"].as<int>());
        time_t created = data["created"].as<long>();
        int duration = data["duration"].as<int>();
        int licenseExpired = data["expired"].as<int>();
        // Tuy chon: kieu hien thi LED khi license het han (xem config.h) -
        // gui kem Set License tu app. Khong co truong nay (goi tu noi khac
        // chua biet ve tinh nang nay) thi giu nguyen gia tri dang luu.
        const bool hasExpiredLedMode = data["expiredLedMode"].is<int>();
        const int requestedExpiredLedMode = hasExpiredLedMode ? data["expiredLedMode"].as<int>() : -1;
        const bool expiredLedModeValid = !hasExpiredLedMode ||
                                         (requestedExpiredLedMode >= 0 && requestedExpiredLedMode <= 3);
        // Chi dung khi expiredLedMode == 3 (Random theo chu ky) - khoang
        // thoi gian "binh thuong" giua 2 dot loi, tinh bang phut.
        const bool hasCycleMin = data["expiredCycleMinMinutes"].is<unsigned long>() ||
                                 data["expiredCycleMinMinutes"].is<int>();
        const bool hasCycleMax = data["expiredCycleMaxMinutes"].is<unsigned long>() ||
                                 data["expiredCycleMaxMinutes"].is<int>();
        const uint32_t requestedCycleMin = hasCycleMin ? data["expiredCycleMinMinutes"].as<uint32_t>() : 0;
        const uint32_t requestedCycleMax = hasCycleMax ? data["expiredCycleMaxMinutes"].as<uint32_t>() : 0;
        // Ca hai phai cung co mat, min >= 1 phut, max >= min.
        const bool cycleRangeValid = (!hasCycleMin && !hasCycleMax) ||
                                     (hasCycleMin && hasCycleMax && requestedCycleMin >= 1 &&
                                      requestedCycleMax >= requestedCycleMin);

        StaticJsonDocument<256> respDoc;
        const bool idIsValid = (id == "0" || id.equalsIgnoreCase(deviceUid));
        const bool lidIsValid = lidFitsBuffer && isValidLidString(incomingLid);

        if (!idIsValid || !lidIsValid || !expiredLedModeValid || !cycleRangeValid)
        {
            respDoc["status"] = 1;
            respDoc["error_msg"] = !idIsValid ? "ID khong danh cho thiet bi nay"
                                   : !lidIsValid ? "LID khong hop le"
                                   : !expiredLedModeValid ? "expiredLedMode phai tu 0 den 3"
                                                          : "Chu ky het han khong hop le (can ca 2 moc, max >= min >= 1)";
            sendResponse(id_src, myMac, LIC_SET_LICENSE | 0x80, respDoc, mac_addr, packetTime);
            Serial.println(!idIsValid ? "[LICENSE] ID khong hop le: " + id + ", deviceUid = " + String(deviceUid)
                           : !lidIsValid ? "[LICENSE] LID khong hop le"
                           : !expiredLedModeValid ? "[LICENSE] expiredLedMode khong hop le"
                                                  : "[LICENSE] Chu ky het han khong hop le");
            led.setState(CONNECTION_ERROR);
            break;
        }

        // "Danh dau het han ngay" (data["expired"]) tung TU CHOI ca lenh o
        // day (status=3, khong ap dung gi ca) - nghia la tick o do xong bam
        // OK thi lid/duration/mode/chu ky MOI deu bi bo qua het, node giu
        // nguyen y het trang thai CU (vi du van con dang o mode LED cua lan
        // Set License truoc). Sua lai dung y checkbox: van ap dung moi thu
        // nhu binh thuong, chi khac o cho remain/expired_flag duoc ep ve
        // "het han" ngay thay vi "con du duration" - de xem truoc hieu ung
        // LED ngay lap tuc thay vi phai doi het gio that.
        strlcpy(globalLicense.lid, incomingLid, LID_BUF_LEN); //sao chep LID vao cau truc license
        globalLicense.lid[LID_MAX_LEN] = '\0';
        globalLicense.created = created;
        start_time = millis() / 1000;
        runtime = 0;
        // Gop duration/remain/expired_flag vao 1 khoi khong ngat duoc (xem
        // licenseStateMux trong config.h) - thieu buoc nay, LedDisplay::
        // update() tren core 1 co the doc trung luc remain da la gia tri
        // moi (con han) nhung expired_flag con la true (cu) trong 1 khoanh
        // khac rat ngan, tuong nham la het han va nhay 1 nhip "loi" dung
        // ngay luc Set License thanh cong.
        portENTER_CRITICAL(&licenseStateMux);
        globalLicense.duration = duration;
        if (licenseExpired)
        {
            globalLicense.remain = 0;
            globalLicense.expired_flag = true;
        }
        else
        {
            globalLicense.remain = duration;
            globalLicense.expired_flag = false;
        }
        portEXIT_CRITICAL(&licenseStateMux);
        if (hasExpiredLedMode)
        {
            expiredLedMode = static_cast<uint8_t>(requestedExpiredLedMode);
        }
        if (hasCycleMin && hasCycleMax)
        {
            expiredCycleMinMinutes = requestedCycleMin;
            expiredCycleMaxMinutes = requestedCycleMax;
        }
        // KHONG lui expiredLedMode/cycle ve gia tri cu khi ghi NVS that bai
        // nua (truoc day co lui rieng 2 truong nay) - duration/remain/
        // expired_flag o tren khong lui, nen lui rieng mode se tao ra dung
        // kieu "thoi han dung nhung mode sai" nhu duration/remain: du NVS
        // co ghi duoc hay khong, moi thu app vua gui van cung co hieu luc
        // trong RAM ngay lap tuc - status=2 chi bao la LAN SAU khoi dong
        // lai se mat, khong bao la lenh nay khong ap dung.
        const bool saved = saveLicenseData();

        respDoc["status"] = saved ? 0 : 2;
        respDoc["expiredLedMode"] = expiredLedMode;
        respDoc["expiredCycleMinMinutes"] = expiredCycleMinMinutes;
        respDoc["expiredCycleMaxMinutes"] = expiredCycleMaxMinutes;
        if (!saved)
            respDoc["error_msg"] = "Khong luu duoc license vao NVS";

        sendResponse(id_src, myMac, LIC_SET_LICENSE | 0x80, respDoc, mac_addr, packetTime);
        if (saved)
        {
            Serial.println("[LICENSE] Cap nhat thanh cong: LID = " + String(globalLicense.lid) + ", ID = " + id);
            led.setState(FLASH_TWICE);
        }
        else
        {
            Serial.println("[LICENSE] Da cap nhat RAM nhung ghi NVS that bai");
            led.setState(CONNECTION_ERROR);
        }
        break;
    }

    case LIC_GET_LICENSE:
    {
        JsonObject data = doc["data"].as<JsonObject>();
        // Ban Scan discovery (broadcast, kem scan_id) - boc 1 khe gio ngau nhien
        // trong ca so slot_count*slot_ms roi hen gio tra loi (xem
        // sendPendingScanReplyIfDue() o tren), thay vi tra loi ngay lap tuc nhu
        // 1 yeu cau Get License unicast binh thuong.
        if (idDes == "0" && data["scan_id"].is<uint32_t>())
        {
            const uint16_t slotCount = data["slot_count"] | 1;
            const uint16_t slotMs = data["slot_ms"] | 0;
            const uint16_t slot = slotCount > 0 ? static_cast<uint16_t>(random(0, slotCount)) : 0;
            pendingScanReply.active = true;
            pendingScanReply.fireAtMillis = millis() + static_cast<uint32_t>(slot) * slotMs;
            snprintf(pendingScanReply.idSrc, sizeof(pendingScanReply.idSrc), "%s", id_src);
            memcpy(pendingScanReply.mac, mac_addr, 6);
            pendingScanReply.packetTime = packetTime;
            break;
        }
        StaticJsonDocument<512> respDoc;
        respDoc["created"] = globalLicense.created;
        respDoc["expired"] = globalLicense.expired_flag ? 1 : 0;
        respDoc["duration"] = globalLicense.duration;
        respDoc["remain"] = globalLicense.remain;
        respDoc["lid"] = config_lid;
        respDoc["cardType"] = card_type;
        respDoc["group"] = config_group;
        JsonObject responseMatrix = respDoc.createNestedObject("Matrix");
        responseMatrix["x"] = matrix_x;
        responseMatrix["y"] = matrix_y;
        respDoc["status"] = 0;
        sendResponse(id_src, myMac, LIC_GET_LICENSE | 0x80, respDoc, mac_addr, packetTime);
        Serial.println("[LICENSE] Da gui thong tin cho " + String(deviceUid));
        led.setState(FLASH_TWICE);

        if (globalLicense.expired_flag || globalLicense.remain <= 0)
        {
            led.setState(LICENSE_EXPIRED);
        }
        else
        {
            led.setState(NORMAL_STATUS);
        }
        break;
    }

    case CONFIG_LID:
    {
        JsonObject data = doc["data"].as<JsonObject>();
        char requestedLid[LID_BUF_LEN] = {};
        const bool hasNewLid = data.containsKey("new_lid");
        const bool hasLid = hasNewLid || data.containsKey("lid");
        bool lidValid = true;
        if (hasNewLid)
            lidValid = parseLidValue(data["new_lid"], requestedLid);
        else if (hasLid)
            lidValid = parseLidValue(data["lid"], requestedLid);

        const bool hasMatrix = data.containsKey("Matrix");
        JsonObject matrix = data["Matrix"].as<JsonObject>();
        // Cho phep lenh cu chi cap nhat LID. Neu Matrix duoc gui kem thi
        // bat buoc phai co du ca x va y, dung kieu so nguyen.
        const bool matrixValid = !hasMatrix ||
                                 (!matrix.isNull() &&
                                  matrix["x"].is<int32_t>() &&
                                  matrix["y"].is<int32_t>());
        const bool hasCardType = data.containsKey("cardType");
        const char *requestedCardType = hasCardType
                                            ? data["cardType"].as<const char *>()
                                            : nullptr;
        const bool cardTypeValid = !hasCardType ||
                                   (data["cardType"].is<const char *>() &&
                                    isValidCardType(requestedCardType));
        // "group" la ten Group/vi tri (xem Groups page cua app) - mot truong
        // doc lap khac, giong cardType: co the gui rieng le hoac kem chung
        // voi Matrix/new_lid trong cung 1 goi Set Matrix.
        const bool hasGroup = data.containsKey("group");
        const char *requestedGroup = hasGroup
                                         ? data["group"].as<const char *>()
                                         : nullptr;
        const bool groupValid = !hasGroup ||
                                (data["group"].is<const char *>() &&
                                 isValidGroupName(requestedGroup));
        const bool hasConfigField = hasLid || hasMatrix || hasCardType || hasGroup;
        StaticJsonDocument<256> respDoc;
        if (!hasConfigField || !lidValid || !matrixValid || !cardTypeValid || !groupValid)
        {
            respDoc["status"] = 1;
            respDoc["error_msg"] = !hasConfigField ? "Khong co truong cau hinh"
                                   : !lidValid ? "LID khong hop le"
                                   : !matrixValid ? "Matrix khong hop le"
                                   : !cardTypeValid ? "cardType chi chap nhan OB hoac R"
                                                    : "Ten group khong hop le";
            sendResponse(id_src, myMac, CONFIG_LID_ACK, respDoc, mac_addr, packetTime);
            Serial.println(!hasConfigField ? "[CONFIG_LID] Khong co truong cau hinh"
                           : !lidValid ? "[CONFIG_LID] LID khong hop le"
                           : !matrixValid ? "[CONFIG_LID] Matrix khong hop le"
                           : !cardTypeValid ? "[CONFIG_LID] cardType khong hop le"
                                            : "[CONFIG_LID] Ten group khong hop le");
            led.setState(CONNECTION_ERROR);
            break;
        }

        char previousLid[LID_BUF_LEN] = {};
        strlcpy(previousLid, config_lid, sizeof(previousLid));
        char previousCardType[3] = {};
        strlcpy(previousCardType, card_type, sizeof(previousCardType));
        char previousGroup[sizeof(config_group)] = {};
        strlcpy(previousGroup, config_group, sizeof(previousGroup));
        const int32_t previousMatrixX = matrix_x;
        const int32_t previousMatrixY = matrix_y;
        if (hasLid)
        {
            strlcpy(config_lid, requestedLid, LID_BUF_LEN);
            config_lid[LID_MAX_LEN] = '\0';
        }
        if (hasCardType)
            strlcpy(card_type, requestedCardType, sizeof(card_type));
        if (hasGroup)
            strlcpy(config_group, requestedGroup, sizeof(config_group));
        if (hasMatrix)
        {
            matrix_x = matrix["x"].as<int32_t>();
            matrix_y = matrix["y"].as<int32_t>();
        }

        const bool saved = saveDeviceConfig();
        if (!saved)
        {
            strlcpy(config_lid, previousLid, LID_BUF_LEN);
            config_lid[LID_MAX_LEN] = '\0';
            strlcpy(card_type, previousCardType, sizeof(card_type));
            strlcpy(config_group, previousGroup, sizeof(config_group));
            matrix_x = previousMatrixX;
            matrix_y = previousMatrixY;
        }

        respDoc["lid"] = saved ? config_lid : previousLid;
        respDoc["cardType"] = saved ? card_type : previousCardType;
        respDoc["group"] = saved ? config_group : previousGroup;
        JsonObject responseMatrix = respDoc.createNestedObject("Matrix");
        responseMatrix["x"] = saved ? matrix_x : previousMatrixX;
        responseMatrix["y"] = saved ? matrix_y : previousMatrixY;
        respDoc["status"] = saved ? 0 : 2;
        if (!saved)
            respDoc["error_msg"] = "Khong luu duoc cau hinh vao NVS";

        sendResponse(id_src, myMac, CONFIG_LID_ACK, respDoc, mac_addr, packetTime);
        Serial.println(saved ? "[CONFIG_LID] Cap nhat thanh cong: LID=" + String(config_lid) +
                                   ", cardType=" + String(card_type) +
                                   ", group=" + String(config_group) +
                                   ", Matrix=(" + String(matrix_x) + "," + String(matrix_y) + ")"
                             : "[CONFIG_LID] Ghi NVS that bai");
        led.setState(saved ? FLASH_TWICE : CONNECTION_ERROR);
        break;
    }

    case LIC_LICENSE_DELETE:
    {
        StaticJsonDocument<256> respDoc;
        char requestedLid[LID_BUF_LEN] = {};
        bool lidFitsBuffer = false;
        if (doc["data"].containsKey("lid"))
            lidFitsBuffer = parseLidValue(doc["data"]["lid"], requestedLid);
        else
            lidFitsBuffer = parseLidValue(doc["lid"], requestedLid);
        const bool found = lidFitsBuffer &&
                           strcmp(requestedLid, globalLicense.lid) == 0;

        if (found)
        {
            globalLicense.created = 0;
            portENTER_CRITICAL(&licenseStateMux);
            globalLicense.duration = 0;
            globalLicense.remain = 0;
            globalLicense.expired_flag = true;
            portEXIT_CRITICAL(&licenseStateMux);
            expired = 1;
            runtime = 0;
        }

        const bool saved = !found || saveLicenseData();
        respDoc["status"] = !found ? 3 : (saved ? 0 : 2);
        if (found && !saved)
            respDoc["error_msg"] = "Khong luu duoc trang thai xoa vao NVS";

        sendResponse(id_src, myMac, LIC_LICENSE_DELETE | 0x80, respDoc, mac_addr, packetTime);
        Serial.println(!found ? "[LICENSE] Khong co license de xoa"
                              : (saved ? "[LICENSE] Da xoa license"
                                       : "[LICENSE] Xoa license trong RAM nhung ghi NVS that bai"));
        led.setState(found && saved ? LICENSE_EXPIRED : CONNECTION_ERROR);
        break;
    }

    case LIC_LICENSE_DELETE_ALL:
    {
        // Xoa tat ca license
        globalLicense.created = 0;
        portENTER_CRITICAL(&licenseStateMux);
        globalLicense.duration = 0;
        globalLicense.remain = 0;
        globalLicense.expired_flag = true;
        portEXIT_CRITICAL(&licenseStateMux);
        expired = 1;
        runtime = 0;
        const bool saved = saveLicenseData(); // Luu trang thai moi
        StaticJsonDocument<256> respDoc;
        respDoc["status"] = saved ? 0 : 2;
        if (!saved)
            respDoc["error_msg"] = "Khong luu duoc trang thai xoa vao NVS";
        sendResponse(id_src, myMac, LIC_LICENSE_DELETE_ALL | 0x80, respDoc, mac_addr, packetTime);
        Serial.println(saved ? "[LICENSE] Da xoa tat ca license"
                             : "[LICENSE] Xoa license trong RAM nhung ghi NVS that bai");
        led.setState(saved ? LICENSE_EXPIRED : CONNECTION_ERROR);
        break;
    }

    case LIC_TIME_GET:
    {
        StaticJsonDocument<256> respDoc;
        respDoc["time"] = packetTime; // Phan hoi dung time cua request
        respDoc["status"] = 0;
        sendResponse(id_src, myMac, LIC_TIME_GET | 0x80, respDoc, mac_addr, packetTime);
        Serial.println("✅ Time info sent.");
        break;
    }

    case LIC_IDENTIFY:
    {
        // Fire-and-forget: khong gui phan hoi de 100 board khong tranh nhau
        // kenh. Chi doi LED 46, khong dong den license/NVS.
        JsonObject data = doc["data"].as<JsonObject>();
        const bool on = data["on"] | false;
        const uint16_t seconds = data["sec"] | 300;
        led.setIdentify(on, seconds);
        Serial.printf("[IDENTIFY] %s (%us)\n", on ? "BAT" : "TAT", static_cast<unsigned>(seconds));
        break;
    }

    case LIC_INFO:
    {
        StaticJsonDocument<384> respDoc = buildDeviceInfoDoc();
        sendResponse(id_src, myMac, LIC_INFO_RESPONSE, respDoc, mac_addr, packetTime);
        Serial.println("✅ Device info sent.");
        break;
    }

    default:
    {
        StaticJsonDocument<256> respDoc;
        respDoc["status"] = 255; // Opcode khong xac dinh
        sendResponse(id_src, myMac, opcode | 0x80, respDoc, mac_addr, packetTime);
        Serial.printf("❌ Unknown opcode: 0x%02X\n", opcode);
        led.setState(CONNECTION_ERROR);
        break;
    }
    }
}
#endif // PROTOCOL_HANDLER_H
