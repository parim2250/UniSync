#ifndef DISCOVERY_H
#define DISCOVERY_H

#define DISCOVERY_PORT  9877
#define MAX_DEVICES     32
#define ANNOUNCE_MAGIC  "UNSY_DISCOVER"

typedef struct {
    char name[64];
    char ip[16];
    int  tcp_port;
} DeviceInfo;

/* Announce this device on the LAN once */
int announce_device(int tcp_port);

/* Listen for devices for a few seconds, fill list, return count */
int discover_devices(DeviceInfo *devices, int max_devices, int timeout_sec);

#endif