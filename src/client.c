#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "network.h"

int main(int argc, char *argv[])
{
    const char *ip = "127.0.0.1";
    int port = DEFAULT_PORT;
    const char *message = "Hello from UniSync client!";

    if (argc > 1) ip      = argv[1];
    if (argc > 2) port     = atoi(argv[2]);
    if (argc > 3) message  = argv[3];

    int client_fd = connect_to_server(ip, port);
    if (client_fd == -1) {
        fprintf(stderr, "Could not connect to %s:%d. Is the server running?\n",
                ip, port);
        return 1;
    }

    printf("[client] Sending: \"%s\"\n", message);
    int sent = send_message(client_fd, message);
    if (sent == -1) {
        fprintf(stderr, "Failed to send message.\n");
        close_socket(client_fd);
        return 1;
    }
    printf("[client] Sent %d bytes.\n", sent);

    char buffer[BUFFER_SIZE];
    int bytes = receive_message(client_fd, buffer, BUFFER_SIZE);

    if (bytes > 0) {
        printf("[client] Server replied: \"%s\"\n", buffer);
    } else if (bytes == 0) {
        printf("[client] Server closed the connection.\n");
    }

    close_socket(client_fd);
    printf("[client] Disconnected.\n");
    return 0;
}
