#ifndef ESPNOW_GROUP_H
#define ESPNOW_GROUP_H

#include <Arduino.h>
#include <cstring>

#include "device_state.h"

inline int findDeviceIndexByMac(const uint8_t mac[6])
{
  if (mac == nullptr)
  {
    return -1;
  }
  for (int i = 0; i < Device.deviceCount; ++i)
  {
    if (memcmp(Device.nodes[i].mac, mac, 6) == 0)
    {
      return i;
    }
  }
  return -1;
}

inline int findDeviceIndexByUid(const char *uid)
{
  if (uid == nullptr || uid[0] == '\0')
  {
    return -1;
  }
  for (int i = 0; i < Device.deviceCount; ++i)
  {
    if (strcasecmp(Device.nodes[i].uid, uid) == 0)
    {
      return i;
    }
  }
  return -1;
}

inline int findDeviceIndexById(int deviceId)
{
  for (int i = 0; i < Device.deviceCount; ++i)
  {
    if (Device.nodes[i].deviceId == deviceId)
    {
      return i;
    }
  }
  return -1;
}

inline bool removeDeviceAt(int index)
{
  if (index < 0 || index >= Device.deviceCount)
  {
    return false;
  }
  for (int i = index; i + 1 < Device.deviceCount; ++i)
  {
    Device.nodes[i] = Device.nodes[i + 1];
  }
  Device.deviceCount--;
  Device.nodes[Device.deviceCount] = {};
  return true;
}

inline const char *nodeResponseStateName(node_response_state_t state)
{
  switch (state)
  {
  case NODE_ONLINE:
    return "ONLINE";
  case NODE_REFRESHING:
    return "REFRESHING";
  case NODE_NO_RESPONSE:
    return "NO_RESPONSE";
  default:
    return "UNKNOWN";
  }
}

#endif // ESPNOW_GROUP_H
