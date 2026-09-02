#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <arpa/inet.h>

#include "network.h"
#include "protocol.h"
#include "errors.h"
#include "discovery.h"

int main(int argc, char *argv[])
{
    if (argc < 2) {
        printf("Usage: %s <file_to_send>\n", argv[0]);
        printf("       %s <file_to_send> <ip> [port]   (manual mode)\n", argv[0]);
        return 1;
    }

    const char *filepath = argv[1];

    int err = validate_file_readable(filepath);
    if (err != UNISYNC_SUCCESS) {
        fprintf(stderr, "[error] %s: %s\n", filepath, unisync_strerror(err));
        return 1;
    }

    char ip[16] = "127.0.0.1";
    int port = DEFAULT_PORT;

    /* Manual mode if IP given */
    if (argc >= 3) {
        strncpy(ip, argv[2], 15);
        if (argc >= 4) port = atoi(argv[3]);
    } else {
        /* Discovery mode */
        DeviceInfo devices[MAX_DEVICES];
        printf("Scanning for UniSync devices...\n");
        int n = discover_devices(devices, MAX_DEVICES, 3);

        if (n <= 0) {
            fprintf(stderr, "[error] No devices found. Is a receiver running?\n");
            return 1;
        }

        printf("\nFound %d device(s):\n", n);
        for (int i = 0; i < n; i++) {
            printf("  [%d] %s  (%s:%d)\n", i + 1,
                   devices[i].name, devices[i].ip, devices[i].tcp_port);
        }

        printf("\nSelect device (1-%d): ", n);
        int choice = 0;
        if (scanf("%d", &choice) != 1 || choice < 1 || choice > n) {
            fprintf(stderr, "[error] Invalid selection.\n");
            return 1;
        }

        strncpy(ip, devices[choice - 1].ip, 15);
        port = devices[choice - 1].tcp_port;
        printf("Sending to %s (%s:%d)...\n", devices[choice - 1].name, ip, port);
    }

    FileHeader header;
    if (build_file_header(&header, filepath) == -1) return 1;

    int sockfd = connect_to_server(ip, port);
    if (sockfd == -1) {
        fprintf(stderr, "[error] Could not connect to %s:%d\n", ip, port);
        return 1;
    }

    if (send_file_header(sockfd, &header) == -1) {
        close_socket(sockfd);
        return 1;
    }

    /* Wait for Accept/Reject */
    char reply[8] = {0};
    int r = recv(sockfd, reply, sizeof(reply) - 1, 0);
    if (r <= 0 || strncmp(reply, "ACCEPT", 6) != 0) {
        printf("[sender] Transfer rejected by peer.\n");
        close_socket(sockfd);
        return 1;
    }
    printf("[sender] Accepted! Starting transfer...\n");

    ssize_t sent = send_file_payload(sockfd, filepath, header.filesize);
    close_socket(sockfd);

    if (sent < 0) {
        fprintf(stderr, "[error] Transfer failed.\n");
        return 1;
    }

    printf("[sender] SUCCESS — %ld bytes sent.\n", (long)sent);
    return 0;
}