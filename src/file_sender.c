/*
 * file_sender.c — UniSync File Sender (Layer 3, protocol-aware)
 *
 * Sends a FileHeader first, then the file payload.
 *
 * Usage: ./bin/UniSync-file-sender <file_to_send> [ip] [port]
 */

#include <stdio.h>
#include <stdlib.h>

#include "network.h"
#include "protocol.h"

int main(int argc, char *argv[])
{
    if (argc < 2) {
        printf("Usage: %s <file_to_send> [ip] [port]\n", argv[0]);
        return 1;
    }

    const char *filepath = argv[1];
    const char *ip       = (argc > 2) ? argv[2] : "127.0.0.1";
    int port             = (argc > 3) ? atoi(argv[3]) : DEFAULT_PORT;

    /* Step 1: Build header from file metadata */
    FileHeader header;
    if (build_file_header(&header, filepath) == -1) {
        fprintf(stderr, "[sender] Failed to build header for %s\n", filepath);
        return 1;
    }

    /* Step 2: Connect to receiver */
    int sockfd = connect_to_server(ip, port);
    if (sockfd == -1) {
        fprintf(stderr, "[sender] Connection failed.\n");
        return 1;
    }

    /* Step 3: Send header */
    if (send_file_header(sockfd, &header) == -1) {
        fprintf(stderr, "[sender] Failed to send header.\n");
        close_socket(sockfd);
        return 1;
    }

    /* Step 4: Send file payload */
    ssize_t sent = send_file_payload(sockfd, filepath, header.filesize);
    if (sent < 0) {
        fprintf(stderr, "[sender] Payload transfer failed.\n");
        close_socket(sockfd);
        return 1;
    }

    printf("[sender] SUCCESS — %ld bytes of \"%s\" transferred.\n",
           (long)sent, header.filename);

    close_socket(sockfd);
    return 0;
}