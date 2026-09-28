#ifndef SERIAL_H
#define SERIAL_H

#include <Arduino.h>
#include <ArduinoJson.h>
#include "config.h"
// #include "espnow_group.h"
// #include "protocol_handler.h" // cho createMessage()

// xử lý các lệnh nhập tay từ Serial
static void handleSerialCommand(const String &cmd)
{
  if (cmd.equalsIgnoreCase("scan"))
  {
    button = 5;
  }
  else if (cmd.equalsIgnoreCase("rescan"))
  {
    button = 4;
  }
  else if (cmd.startsWith("set"))
  {
    int did, lid, hour, minute;
    if (sscanf(cmd.c_str(), "set %d %d %d %d", &did, &lid, &hour, &minute) == 4)
    {
      Device_ID = did;
      datalic.lid = lid;
      datalic.duration = hour * 60 + minute;
      datalic.expired = true;
      button = 1;
      Serial.println("Đã cấu hình license");
    }
    else
    {
      Serial.println("Sai định dạng. set <device_id> <local_id> <hour> <minute>");
    }
  }
  else if (cmd.startsWith("info"))
  {
    int did, lid;
    if (sscanf(cmd.c_str(), "info %d %d", &did, &lid) == 2)
    {
      Device_ID = did;
      datalic.lid = lid;
      button = 6;
      Serial.println("Yêu cầu gửi LIC_INFO");
    }
    else
    {
      Serial.println("Sai định dạng. info <device_id> <local_id>");
    }
  }
  else
  {
    Serial.println("Lệnh không hợp lệ");
  }
}

// NOTE: the old byte-by-byte Serial reader that used to live here
// (serial_pc(), plus the JSON debug dump it did inline) has been replaced by
// handleUsbBridge() in local_web.h, which is now the single reader of
// Serial input — it dispatches JSON lines through the same command handler
// as the WebSocket and falls back to handleSerialCommand() below for the
// legacy plain-text debug commands. See local_web.h's "USB serial transport"
// section for the full read loop.

#endif // SERIAL_H
