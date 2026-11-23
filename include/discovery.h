/*
 * discovery.h — find UniSync devices on the LAN (UDP broadcast + multicast)
 *
 *   Receiver side : discovery_responder_start() answers "who is there?" requests
 *   Sender side   : discover_devices() asks and collects the answers
 */
#ifndef DISCOVERY_H
#define DISCOVERY_H

#define DISCOVERY_PORT  9877
#define MAX_DEVICES     32
#define ANNOUNCE_MAGIC  "UNSY_DISCOVER"
#define MCAST_GROUP     "239.255.42.99"

typedef struct {
    char name[64];
    char ip[16];
    int  tcp_port;
    int  needs_pin;
    int  encrypted;
} DeviceInfo;

/* Receiver: answer discovery requests in a background thread */
int  discovery_responder_start(int tcp_port, int needs_pin, int encrypted);
void discovery_responder_stop(void);

/* Sender: ask the LAN, wait timeout_sec, fill list, return count */
int discover_devices(DeviceInfo *devices, int max_devices, int timeout_sec);

#endif
