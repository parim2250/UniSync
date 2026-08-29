/*
 * file_receiver.c — UniSync File Receiver (Layer 3, protocol-aware)
 *
 * Reads a FileHeader first, uses its filename automatically,
 * then receives exactly `filesize` bytes.
 *
 * Usage: ./bin/UniSync-file-receiver [port] [save_directory]
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>

#include "network.h"
#include "protocol.h"

int main(int argc, char *argv[])
{
    int port                = (argc > 1) ? atoi(argv[1]) : DEFAULT_PORT;
    const char *save_dir    = (argc > 2) ? argv[2] : ".";

    int server_fd = start_server(port);
    if (server_fd == -1) return 1;

    printf("[receiver] Waiting for a sender on port %d...\n", port);

    int client_fd = accept(server_fd, NULL, NULL);
    if (client_fd == -1) {
        perror("accept() failed");
        close_socket(server_fd);
        return 1;
    }
    printf("[receiver] Sender connected.\n");

    /* Step 1: Receive header */
    FileHeader header;
    if (receive_file_header(client_fd, &header) == -1) {
        fprintf(stderr, "[receiver] Header receive failed.\n");
        close_socket(client_fd);
        close_socket(server_fd);
        return 1;
    }

    /* Step 2: Build the full output path (save_dir + filename) */
    char output_path[512];
    snprintf(output_path, sizeof(output_path), "%s/%s", save_dir, header.filename);

    printf("[receiver] Saving to: %s\n", output_path);

    /* Step 3: Receive exactly filesize bytes */
    ssize_t bytes = receive_file_payload(client_fd, output_path, header.filesize);
    if (bytes < 0) {
        fprintf(stderr, "[receiver] Payload receive failed.\n");
        close_socket(client_fd);
        close_socket(server_fd);
        return 1;
    }

    printf("[receiver] SUCCESS — %ld bytes saved to \"%s\"\n",
           (long)bytes, output_path);

    close_socket(client_fd);
    close_socket(server_fd);
    return 0;
}