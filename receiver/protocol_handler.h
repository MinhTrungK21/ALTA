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

// Luu gia tri license hien tai vao NVS.
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

    bool ok = true;
    if (!preferences.begin("license", false))
    {
        Serial.println("[NVS] Khong mo duoc de ghi");
        unlockPreferences();
        return false;
    }

    ok = putNvsResult("lid", preferences.putString("lid", globalLicense.lid),
                      strlen(globalLicense.lid)) && ok;
    ok = putNvsResult("created", preferences.putULong("created", static_cast<uint32_t>(globalLicense.created)), 4) && ok;
    ok = putNvsResult("duration", preferences.putULong("duration", globalLicense.duration), 4) && ok;
    ok = putNvsResult("remain", preferences.putULong("remain", globalLicense.remain), 4) && ok;
    ok = putNvsResult("expired_flag", preferences.putBool("expired_flag", globalLicense.expired_flag), 1) && ok;
    ok = putNvsResult("runtime", preferences.putULong("runtime", runtime), 4) && ok;
    preferences.end();
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

bool saveDeviceConfig()
{
    if (!isValidLidString(config_lid))
    {
        Serial.println("[NVS] LOI: config_lid khong hop le, bo qua ghi");
        return false;
    }
    if (!isValidCardType(card_type))
    {
        Serial.println("[NVS] LOI: cardType khong hop le, bo qua ghi");
        return false;
    }

    if (!lockPreferences())
    {
        Serial.println("[NVS] LOI: khong lock duoc mutex");
        return false;
    }

    if (!preferences.begin("license", false))
    {
        Serial.println("[NVS] Khong mo duoc de ghi config_lid");
        unlockPreferences();
        return false;
    }

    bool ok = putNvsResult("config_lid",
                           preferences.putString("config_lid", config_lid),
                           strlen(config_lid));
    ok = putNvsResult("cardType", preferences.putString("cardType", card_type),
                      strlen(card_type)) && ok;
    ok = putNvsResult("matrix_x", preferences.putInt("matrix_x", matrix_x), 4) && ok;
    ok = putNvsResult("matrix_y", preferences.putInt("matrix_y", matrix_y), 4) && ok;
    preferences.end();
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

// Tai gia tri license cuoi cung tu NVS khi khoi dong.
bool loadLicenseData()
{
    if (!lockPreferences())
    {
        Serial.println("[NVS] LOI: khong lock duoc mutex");
        return false;
    }
    if (!preferences.begin("license", true))
    {
        Serial.println("[NVS] Khong mo duoc de doc");
        unlockPreferences();
        return false;
    }

    char configLidDefault[LID_BUF_LEN] = {};
    strlcpy(configLidDefault, config_lid, sizeof(configLidDefault));
    const bool configLidLoaded = loadNvsLid("config_lid", config_lid, configLidDefault);
    char loadedCardType[3] = {};
    const size_t cardTypeLen = preferences.getString("cardType", loadedCardType, sizeof(loadedCardType));
    const bool cardTypeKeyExists = preferences.isKey("cardType");
    const bool cardTypeLoaded = !cardTypeKeyExists ||
                                (cardTypeLen > 0 && isValidCardType(loadedCardType));
    if (cardTypeKeyExists && cardTypeLoaded)
        strlcpy(card_type, loadedCardType, sizeof(card_type));
    else if (cardTypeKeyExists)
        Serial.println("[NVS] LOI: cardType khong hop le, dung gia tri mac dinh");
    matrix_x = preferences.getInt("matrix_x", matrix_x);
    matrix_y = preferences.getInt("matrix_y", matrix_y);

    char lidDefault[LID_BUF_LEN] = {};
    strlcpy(lidDefault, config_lid, sizeof(lidDefault));
    const bool lidLoaded = loadNvsLid("lid", globalLicense.lid, lidDefault);
    globalLicense.created = preferences.getULong("created", 0);
    globalLicense.duration = preferences.getULong("duration", 0);
    globalLicense.remain = preferences.getULong("remain", 0);
    globalLicense.expired_flag = preferences.getBool("expired_flag", false);
    runtime = preferences.getULong("runtime", 0);
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

    Serial.printf("[NVS] LOAD: lid=%s config_lid=%s cardType=%s matrix=(%ld,%ld) duration=%lu remain=%lu runtime=%lu result=%s\n",
                  globalLicense.lid, config_lid, card_type,
                  static_cast<long>(matrix_x), static_cast<long>(matrix_y),
                  static_cast<unsigned long>(globalLicense.duration),
                  static_cast<unsigned long>(globalLicense.remain),
                  static_cast<unsigned long>(runtime), hasLicense ? "OK" : "FAIL");
    return hasLicense || (configLidLoaded && cardTypeLoaded);
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

        StaticJsonDocument<256> respDoc;
        const bool idIsValid = (id == "0" || id.equalsIgnoreCase(deviceUid));
        const bool lidIsValid = lidFitsBuffer && isValidLidString(incomingLid);

        if (!idIsValid || !lidIsValid)
        {
            respDoc["status"] = 1;
            respDoc["error_msg"] = !idIsValid
                                       ? "ID khong danh cho thiet bi nay"
                                       : "LID khong hop le";
            sendResponse(id_src, myMac, LIC_SET_LICENSE | 0x80, respDoc, mac_addr, packetTime);
            Serial.println(!idIsValid
                               ? "[LICENSE] ID khong hop le: " + id + ", deviceUid = " + String(deviceUid)
                               : "[LICENSE] LID khong hop le");
            led.setState(CONNECTION_ERROR);
            break;
        }

        if (licenseExpired)
        {
            respDoc["status"] = 3;
            sendResponse(id_src, myMac, LIC_SET_LICENSE | 0x80, respDoc, mac_addr, packetTime);
            Serial.println("[LICENSE] License het hieu luc");
            led.setState(CONNECTION_ERROR);
            break;
        }

        strlcpy(globalLicense.lid, incomingLid, LID_BUF_LEN); //sao chep LID vao cau truc license
        globalLicense.lid[LID_MAX_LEN] = '\0';
        globalLicense.created = created;
        globalLicense.duration = duration;
        start_time = millis() / 1000;
        runtime = 0;
        globalLicense.remain = duration;
        globalLicense.expired_flag = false;
        const bool saved = saveLicenseData();

        respDoc["status"] = saved ? 0 : 2;
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
        StaticJsonDocument<512> respDoc;
        respDoc["created"] = globalLicense.created;
        respDoc["expired"] = globalLicense.expired_flag ? 1 : 0;
        respDoc["duration"] = globalLicense.duration;
        respDoc["remain"] = globalLicense.remain;
        respDoc["lid"] = config_lid;
        respDoc["cardType"] = card_type;
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
        const bool hasConfigField = hasLid || hasMatrix || hasCardType;
        StaticJsonDocument<256> respDoc;
        if (!hasConfigField || !lidValid || !matrixValid || !cardTypeValid)
        {
            respDoc["status"] = 1;
            respDoc["error_msg"] = !hasConfigField ? "Khong co truong cau hinh"
                                   : !lidValid ? "LID khong hop le"
                                   : !matrixValid ? "Matrix khong hop le"
                                                  : "cardType chi chap nhan OB hoac R";
            sendResponse(id_src, myMac, CONFIG_LID_ACK, respDoc, mac_addr, packetTime);
            Serial.println(!hasConfigField ? "[CONFIG_LID] Khong co truong cau hinh"
                           : !lidValid ? "[CONFIG_LID] LID khong hop le"
                           : !matrixValid ? "[CONFIG_LID] Matrix khong hop le"
                                          : "[CONFIG_LID] cardType khong hop le");
            led.setState(CONNECTION_ERROR);
            break;
        }

        char previousLid[LID_BUF_LEN] = {};
        strlcpy(previousLid, config_lid, sizeof(previousLid));
        char previousCardType[3] = {};
        strlcpy(previousCardType, card_type, sizeof(previousCardType));
        const int32_t previousMatrixX = matrix_x;
        const int32_t previousMatrixY = matrix_y;
        if (hasLid)
        {
            strlcpy(config_lid, requestedLid, LID_BUF_LEN);
            config_lid[LID_MAX_LEN] = '\0';
        }
        if (hasCardType)
            strlcpy(card_type, requestedCardType, sizeof(card_type));
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
            matrix_x = previousMatrixX;
            matrix_y = previousMatrixY;
        }

        respDoc["lid"] = saved ? config_lid : previousLid;
        respDoc["cardType"] = saved ? card_type : previousCardType;
        JsonObject responseMatrix = respDoc.createNestedObject("Matrix");
        responseMatrix["x"] = saved ? matrix_x : previousMatrixX;
        responseMatrix["y"] = saved ? matrix_y : previousMatrixY;
        respDoc["status"] = saved ? 0 : 2;
        if (!saved)
            respDoc["error_msg"] = "Khong luu duoc cau hinh vao NVS";

        sendResponse(id_src, myMac, CONFIG_LID_ACK, respDoc, mac_addr, packetTime);
        Serial.println(saved ? "[CONFIG_LID] Cap nhat thanh cong: LID=" + String(config_lid) +
                                   ", cardType=" + String(card_type) +
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
            globalLicense.duration = 0;
            globalLicense.remain = 0;
            globalLicense.expired_flag = true;
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
        globalLicense.duration = 0;
        globalLicense.remain = 0;
        globalLicense.expired_flag = true;
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

    case LIC_INFO:
    {
        StaticJsonDocument<384> respDoc;
        // temperatureRead() tra ve nhiet do ben trong chip ESP32-S3.
        const float temperatureC = roundf(temperatureRead() * 10.0f) / 10.0f;
        // Bo dem 64-bit tinh tu luc khoi dong, doi tu micro giay sang phut.
        const uint64_t uptimeMinutes = static_cast<uint64_t>(esp_timer_get_time()) / 60000000ULL;

        respDoc["device_id"] = deviceUid;
        respDoc["lid"] = config_lid;
        respDoc["firmware_version"] = FIRMWARE_VERSION;
        respDoc["voltage_v"] = 3.3;
        respDoc["temperature_c"] = temperatureC;
        respDoc["uptime_m"] = uptimeMinutes;
        respDoc["protocol"] = "ESP-NOW";
        respDoc["status"] = 0;
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
