#include <stdio.h>
#include <stdlib.h>
#include <arpa/inet.h>

#include "network.h"
#include "protocol.h"
#include "errors.h"

int main(int argc, char *argv[])
{
    if (argc < 2) {
        printf("Usage: %s <file_to_send> [ip] [port]\n", argv[0]);
        return 1;
    }

    const char *filepath = argv[1];
    const char *ip       = (argc > 2) ? argv[2] : "127.0.0.1";
    int port             = (argc > 3) ? atoi(argv[3]) : DEFAULT_PORT;

    /* Validate File */
    int err = validate_file_readable(filepath);
    if (err != UNISYNC_SUCCESS) {
        fprintf(stderr, "[sender Error] %s: %s\n", filepath, unisync_strerror(err));
        return 1;
    }

    /* Validate IP */
    struct in_addr sa;
    if (inet_pton(AF_INET, ip, &sa) <= 0) {
        fprintf(stderr, "[sender Error] %s: %s\n", ip, unisync_strerror(ERR_INVALID_IP));
        return 1;
    }

    /* Validate Port */
    if (port < 1 || port > 65535) {
        fprintf(stderr, "[sender Error] %s\n", unisync_strerror(ERR_INVALID_PORT));
        return 1;
    }

    FileHeader header;
    if (build_file_header(&header, filepath) == -1) {
        return 1;
    }

    int sockfd = connect_to_server(ip, port);
    if (sockfd == -1) {
        fprintf(stderr, "[sender Error] Could not connect to %s:%d (Is receiver running?)\n", ip, port);
        return 1;
    }

    if (send_file_header(sockfd, &header) == -1) {
        close_socket(sockfd);
        return 1;
    }

    ssize_t sent = send_file_payload(sockfd, filepath, header.filesize);
    close_socket(sockfd);

    if (sent < 0) {
        fprintf(stderr, "[sender Error] Transfer failed mid-way.\n");
        return 1;
    }

    printf("[sender] SUCCESS — %ld bytes transferred.\n", (long)sent);
    return 0;
}