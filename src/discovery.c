#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netdb.h>
#include <time.h>

#include "discovery.h"

int announce_device(int tcp_port)
{
    int sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock < 0) { perror("socket"); return -1; }

    int broadcast = 1;
    setsockopt(sock, SOL_SOCKET, SO_BROADCAST, &broadcast, sizeof(broadcast));

    char hostname[64];
    gethostname(hostname, sizeof(hostname));

    char msg[128];
    snprintf(msg, sizeof(msg), "%s|%s|%d", ANNOUNCE_MAGIC, hostname, tcp_port);

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(DISCOVERY_PORT);
    addr.sin_addr.s_addr = inet_addr("255.255.255.255");

    sendto(sock, msg, strlen(msg)+1, 0, (struct sockaddr*)&addr, sizeof(addr));
    close(sock);
    return 0;
}

int discover_devices(DeviceInfo *devices, int max_devices, int timeout_sec)
{
    int sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock < 0) { perror("socket"); return -1; }

    int reuse = 1;
    setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(DISCOVERY_PORT);
    addr.sin_addr.s_addr = INADDR_ANY;

    if (bind(sock, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        perror("bind");
        close(sock);
        return -1;
    }

    /* Also announce ourselves so others can see us */
    announce_device(9876);

    struct timeval tv = { .tv_sec = timeout_sec, .tv_usec = 0 };
    setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));

    int count = 0;
    time_t start = time(NULL);

    while (time(NULL) - start < timeout_sec && count < max_devices) {
        char buf[128];
        struct sockaddr_in sender;
        socklen_t slen = sizeof(sender);

        int n = recvfrom(sock, buf, sizeof(buf)-1, 0, (struct sockaddr*)&sender, &slen);
        if (n <= 0) continue;
        buf[n] = '\0';

        if (strncmp(buf, ANNOUNCE_MAGIC, strlen(ANNOUNCE_MAGIC)) != 0) continue;

        char name[64];
        int port = 0;
        if (sscanf(buf, "UNSY_DISCOVER|%63[^|]|%d", name, &port) != 2) continue;

        char *ip = inet_ntoa(sender.sin_addr);

        /* Skip duplicates */
        int dup = 0;
        for (int i = 0; i < count; i++) {
            if (strcmp(devices[i].ip, ip) == 0) { dup = 1; break; }
        }
        if (dup) continue;

        strncpy(devices[count].name, name, 63);
        strncpy(devices[count].ip, ip, 15);
        devices[count].tcp_port = port;
        count++;
    }

    close(sock);
    return count;
}