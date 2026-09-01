#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>

#include "network.h"
#include "protocol.h"
#include "errors.h"

int main(int argc, char *argv[])
{
    int port             = (argc > 1) ? atoi(argv[1]) : DEFAULT_PORT;
    const char *save_dir = (argc > 2) ? argv[2] : ".";

    /* Validate Port */
    if (port < 1 || port > 65535) {
        fprintf(stderr, "[receiver Error] %s\n", unisync_strerror(ERR_INVALID_PORT));
        return 1;
    }

    /* Validate Save Directory */
    int err = validate_dir_writable(save_dir);
    if (err != UNISYNC_SUCCESS) {
        fprintf(stderr, "[receiver Error] Directory '%s': %s\n", save_dir, unisync_strerror(err));
        return 1;
    }

    int server_fd = start_server(port);
    if (server_fd == -1) return 1;

    printf("[receiver] Waiting for sender on port %d...\n", port);

    int client_fd = accept(server_fd, NULL, NULL);
    if (client_fd == -1) {
        perror("accept() failed");
        close_socket(server_fd);
        return 1;
    }

    FileHeader header;
    if (receive_file_header(client_fd, &header) == -1) {
        close_socket(client_fd);
        close_socket(server_fd);
        return 1;
    }

    char output_path[512];
    snprintf(output_path, sizeof(output_path), "%s/%s", save_dir, header.filename);

    ssize_t bytes = receive_file_payload(client_fd, output_path, header.filesize);

    close_socket(client_fd);
    close_socket(server_fd);

    if (bytes < 0) {
        fprintf(stderr, "[receiver Error] Transfer failed.\n");
        return 1;
    }

    printf("[receiver] SUCCESS — Saved to \"%s\"\n", output_path);
    return 0;
}