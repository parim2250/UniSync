/*
 * file_sender.c — UniSync File Sender Client
 *
 * Usage: ./bin/UniSync-file-sender [file_to_send] [receiver_ip] [port]
 * Example: ./bin/UniSync-file-sender sample.pdf 127.0.0.1 9876
 */

#include <stdio.h>
#include <stdlib.h>

#include "network.h"
#include "transfer.h"

int main(int argc, char *argv[])
{
    if (argc < 2) {
        printf("Usage: %s <file_to_send> [ip] [port]\n", argv[0]);
        return 1;
    }

    const char *filepath = argv[1];
    const char *ip       = (argc > 2) ? argv[2] : "127.0.0.1";
    int port             = (argc > 3) ? atoi(argv[3]) : DEFAULT_PORT;

    int client_fd = connect_to_server(ip, port);
    if (client_fd == -1) {
        fprintf(stderr, "[sender] Could not connect to %s:%d\n", ip, port);
        return 1;
    }

    printf("[sender] Transmitting file: %s ...\n", filepath);

    ssize_t bytes = send_file_data(client_fd, filepath);
    if (bytes >= 0) {
        printf("[sender] File sent successfully! Total %ld bytes transmitted.\n", (long)bytes);
    } else {
        fprintf(stderr, "[sender] Failed to send file.\n");
    }

    close_socket(client_fd);
    return 0;
}