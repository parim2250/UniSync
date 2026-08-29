/*
 * file_receiver.c — UniSync File Receiver Server
 *
 * Usage: ./bin/UniSync-file-receiver [output_file_path] [port]
 * Example: ./bin/UniSync-file-receiver received_test.zip 9876
 */

#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>

#include "network.h"
#include "transfer.h"

int main(int argc, char *argv[])
{
    const char *output_path = "received_file.dat";
    int port = DEFAULT_PORT;

    if (argc > 1) output_path = argv[1];
    if (argc > 2) port        = atoi(argv[2]);

    int server_fd = start_server(port);
    if (server_fd == -1) return 1;

    printf("[receiver] Listening for incoming file on port %d...\n", port);

    int client_fd = accept(server_fd, NULL, NULL);
    if (client_fd == -1) {
        perror("accept() failed");
        close_socket(server_fd);
        return 1;
    }

    printf("[receiver] Sender connected! Receiving file -> %s\n", output_path);

    ssize_t bytes = receive_file_data(client_fd, output_path);
    if (bytes >= 0) {
        printf("[receiver] File transfer complete! Total %ld bytes received.\n", (long)bytes);
    } else {
        fprintf(stderr, "[receiver] File transfer failed!\n");
    }

    close_socket(client_fd);
    close_socket(server_fd);
    return 0;
}