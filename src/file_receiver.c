#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>

#include "network.h"
#include "protocol.h"
#include "errors.h"

int main(int argc, char *argv[])
{
    int port             = (argc > 1) ? atoi(argv[1]) : DEFAULT_PORT;
    const char *save_dir = (argc > 2) ? argv[2] : ".";

    if (port < 1 || port > 65535) {
        fprintf(stderr, "[error] %s\n", unisync_strerror(ERR_INVALID_PORT));
        return 1;
    }

    int err = validate_dir_writable(save_dir);
    if (err != UNISYNC_SUCCESS) {
        fprintf(stderr, "[error] %s: %s\n", save_dir, unisync_strerror(err));
        return 1;
    }

    int server_fd = start_server(port);
    if (server_fd == -1) return 1;

    printf("[receiver] Waiting on port %d ...\n", port);

    int client_fd = accept(server_fd, NULL, NULL);
    if (client_fd == -1) {
        perror("accept");
        close_socket(server_fd);
        return 1;
    }

    FileHeader header;
    if (receive_file_header(client_fd, &header) == -1) {
        close_socket(client_fd);
        close_socket(server_fd);
        return 1;
    }

    /* Accept / Reject prompt */
    double mb = header.filesize / (1024.0 * 1024.0);
    printf("\nIncoming file: \"%s\" (%.2f MB)\n", header.filename, mb);
    printf("Accept? (y/n): ");

    char answer[8];
    if (!fgets(answer, sizeof(answer), stdin) || (answer[0] != 'y' && answer[0] != 'Y')) {
        send(client_fd, "REJECT", 7, 0);
        printf("[receiver] Rejected.\n");
        close_socket(client_fd);
        close_socket(server_fd);
        return 0;
    }

    send(client_fd, "ACCEPT", 7, 0);
    printf("[receiver] Accepted. Receiving...\n");

    char output_path[512];
    snprintf(output_path, sizeof(output_path), "%s/%s", save_dir, header.filename);

    ssize_t bytes = receive_file_payload(client_fd, output_path, header.filesize);

    close_socket(client_fd);
    close_socket(server_fd);

    if (bytes < 0) {
        fprintf(stderr, "[error] Transfer failed.\n");
        return 1;
    }

    printf("[receiver] SUCCESS — saved to \"%s\"\n", output_path);
    return 0;
}