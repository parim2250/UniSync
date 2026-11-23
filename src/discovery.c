/*
 * discovery.c — UDP discovery
 *   request : "UNSY_WHO"                                   (broadcast / multicast / loopback)
 *   reply   : "UNSY_DISCOVER|<name>|<tcp port>|<flags>"    (unicast back to the asker)
 *             flags: bit0 = PIN required, bit1 = encryption required
 */
#include "common.h"
#include "discovery.h"
#include <ifaddrs.h>
#include <net/if.h>
#include <sys/time.h>

static int rsock = -1;
static volatile int rrun = 0;
static pthread_t rth;
static int r_port, r_flags;

static void *responder(void *a)
{
    (void)a;
    char buf[128];
    while (rrun) {
        struct sockaddr_in from; socklen_t fl = sizeof from;
        ssize_t n = recvfrom(rsock, buf, sizeof buf - 1, 0, (struct sockaddr *)&from, &fl);
        if (n <= 0) continue;
        buf[n] = 0;
        if (strncmp(buf, "UNSY_WHO", 8) != 0) continue;
        char msg[160];
        int m = snprintf(msg, sizeof msg, "%s|%s|%d|%d", ANNOUNCE_MAGIC, cfg.name, r_port, r_flags);
        sendto(rsock, msg, (size_t)m, 0, (struct sockaddr *)&from, fl);
    }
    return NULL;
}

int discovery_responder_start(int tcp_port, int needs_pin, int encrypted)
{
    r_port = tcp_port; r_flags = (needs_pin ? 1 : 0) | (encrypted ? 2 : 0);
    rsock = socket(AF_INET, SOCK_DGRAM, 0);
    if (rsock < 0) return -1;
    int one = 1;
    setsockopt(rsock, SOL_SOCKET, SO_REUSEADDR, &one, sizeof one);
    struct sockaddr_in a; memset(&a, 0, sizeof a);
    a.sin_family = AF_INET; a.sin_port = htons(DISCOVERY_PORT); a.sin_addr.s_addr = htonl(INADDR_ANY);
    if (bind(rsock, (struct sockaddr *)&a, sizeof a) < 0) { close(rsock); rsock = -1; return -1; }
    struct ip_mreq mr; memset(&mr, 0, sizeof mr);
    inet_pton(AF_INET, MCAST_GROUP, &mr.imr_multiaddr); mr.imr_interface.s_addr = htonl(INADDR_ANY);
    setsockopt(rsock, IPPROTO_IP, IP_ADD_MEMBERSHIP, &mr, sizeof mr);     /* multicast; ignore failure */
    struct timeval tv = { 1, 0 };
    setsockopt(rsock, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof tv);
    rrun = 1;
    pthread_create(&rth, NULL, responder, NULL);
    return 0;
}

void discovery_responder_stop(void)
{
    if (!rrun) return;
    rrun = 0; pthread_join(rth, NULL); close(rsock); rsock = -1;
}

static void send_who(int sock, struct in_addr dst)
{
    struct sockaddr_in a; memset(&a, 0, sizeof a);
    a.sin_family = AF_INET; a.sin_port = htons(DISCOVERY_PORT); a.sin_addr = dst;
    sendto(sock, "UNSY_WHO", 8, 0, (struct sockaddr *)&a, sizeof a);
}

int discover_devices(DeviceInfo *devices, int max_devices, int timeout_sec)
{
    int sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock < 0) return -1;
    int one = 1;
    setsockopt(sock, SOL_SOCKET, SO_BROADCAST, &one, sizeof one);
    struct timeval tv = { 0, 200000 };
    setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof tv);
    unsigned char ttl = 1; setsockopt(sock, IPPROTO_IP, IP_MULTICAST_TTL, &ttl, sizeof ttl);

    int count = 0; double start = (double)time(NULL), last_ask = 0;
    struct timespec t0; clock_gettime(CLOCK_MONOTONIC, &t0);
    for (;;) {
        struct timespec t1; clock_gettime(CLOCK_MONOTONIC, &t1);
        double el = (double)(t1.tv_sec - t0.tv_sec) + (double)(t1.tv_nsec - t0.tv_nsec) / 1e9;
        if (el >= timeout_sec || count >= max_devices) break;
        if (el - last_ask >= 0.7 || last_ask == 0) {                 /* (re)send the question a few times */
            last_ask = el + 0.001;
            struct in_addr b; b.s_addr = htonl(INADDR_BROADCAST); send_who(sock, b);
            inet_pton(AF_INET, MCAST_GROUP, &b); send_who(sock, b);
            inet_pton(AF_INET, "127.0.0.1", &b); send_who(sock, b);  /* receiver on this same machine */
            struct ifaddrs *ifa, *p;
            if (getifaddrs(&ifa) == 0) {
                for (p = ifa; p; p = p->ifa_next) {
                    if (!p->ifa_addr || p->ifa_addr->sa_family != AF_INET) continue;
                    if (!(p->ifa_flags & IFF_UP) || !(p->ifa_flags & IFF_BROADCAST) || !p->ifa_broadaddr) continue;
                    send_who(sock, ((struct sockaddr_in *)p->ifa_broadaddr)->sin_addr);
                }
                freeifaddrs(ifa);
            }
        }
        char buf[160]; struct sockaddr_in from; socklen_t fl = sizeof from;
        ssize_t n = recvfrom(sock, buf, sizeof buf - 1, 0, (struct sockaddr *)&from, &fl);
        if (n <= 0) continue;
        buf[n] = 0;
        if (strncmp(buf, ANNOUNCE_MAGIC, strlen(ANNOUNCE_MAGIC)) != 0) continue;
        char name[64]; int port = 0, flags = 0;
        if (sscanf(buf, ANNOUNCE_MAGIC "|%63[^|]|%d|%d", name, &port, &flags) < 2) continue;
        char ip[16]; inet_ntop(AF_INET, &from.sin_addr, ip, sizeof ip);
        int dup = 0;                      /* same device answering via loopback and LAN -> one entry */
        for (int i = 0; i < count; i++)
            if (devices[i].tcp_port == port && !strcmp(devices[i].name, name)) {
                if (!strncmp(devices[i].ip, "127.", 4) && strncmp(ip, "127.", 4)) snprintf(devices[i].ip, sizeof devices[i].ip, "%s", ip);
                dup = 1; break;
            }
        if (dup) continue;
        snprintf(devices[count].name, sizeof devices[count].name, "%s", name);
        snprintf(devices[count].ip, sizeof devices[count].ip, "%s", ip);
        devices[count].tcp_port = port; devices[count].needs_pin = flags & 1; devices[count].encrypted = (flags & 2) != 0;
        count++;
    }
    (void)start;
    close(sock);
    return count;
}
