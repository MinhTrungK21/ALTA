#ifndef DEVICE_STATE_H
#define DEVICE_STATE_H

#include <Arduino.h>

struct Licence
{
    long lid;
    long created;
    long expired;
    long duration;
};

#define MAX_DEVICES 100
#define DEVICE_ALIAS_LENGTH 41

enum node_response_state_t
{
    NODE_UNKNOWN = 0,
    NODE_ONLINE,
    NODE_REFRESHING,
    NODE_NO_RESPONSE
};

struct node_info_t
{
    uint8_t mac[6];
    char uid[16];
    char alias[DEVICE_ALIAS_LENGTH];
    int deviceId;
    int lid;
    uint32_t remain;
    int deviceLimit;
    int protocolStatus;
    node_response_state_t responseState;
    uint32_t lastResponseMillis;
    // LIC_INFO telemetry (see protocol_handler.h::sendGetInfo() and
    // local_web.h::webOnInfoResponse()) — kept as its own fields rather than
    // reusing lid/protocolStatus/remain above: LIC_INFO_RESPONSE's payload
    // uses a different, non-overlapping schema (e.g. its own "status" and a
    // placeholder "lid" that isn't a real license id), and is handled in its
    // own branch specifically so it can never overwrite the license fields
    // the way falling through the generic response handler would.
    char firmwareVersion[16];
    float voltageV;
    float temperatureC;
    uint32_t uptimeMinutes;
    int infoStatus;
    // Which link the node itself reports talking over (e.g. "ESP-NOW",
    // "WiFi", "RS485") - self-reported in LIC_INFO_RESPONSE's "protocol"
    // field, not something the hub infers (the hub always reaches the node
    // over ESP-NOW regardless; a node with its own downstream link, e.g.
    // relaying to sub-devices over RS485, would report that instead).
    char linkProtocol[12];
    uint32_t lastInfoMillis; // 0 = no LIC_INFO_RESPONSE received yet
    // Matrix layout position the NODE has stored for itself (0 = not set).
    // Written by a Set Matrix job (LIC_SET_MATRIX) and reported back by the
    // node in its later responses; the app shows it next to the current
    // grid slot so a board sitting in the wrong place is obvious.
    int nodeRow;
    int nodeCol;
    // Physical board variant the NODE has stored for itself: "OB" (card
    // onboard) or "R" (card rời/removable), empty if never configured.
    // Written by a Set Card Type job and reported back by the node in its
    // later responses, same pattern as nodeRow/nodeCol above — the app uses
    // it to pick which board photo to show, without asking every rescan.
    char cardType[8];
    // Name of the Group/location the NODE has stored for itself (empty if
    // never configured), same self-reported pattern as nodeRow/nodeCol —
    // written alongside them by a Set Matrix job on the Groups page (each
    // Group is its own matrix/location, see the app's Groups page) and
    // echoed back by the node so a board dragged into the wrong Group's
    // grid is visible even before the operator hits Set Matrix again.
    char nodeGroup[24];
};

struct device_info
{
    node_info_t nodes[MAX_DEVICES];
    int deviceCount;
};

extern device_info Device;

#endif // DEVICE_STATE_H
