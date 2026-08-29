#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <arpa/inet.h>

#include "network.h"

int main(int argc, char *argv[])
{
    int port = DEFAULT_PORT;
    if (argc > 1) {
        port = atoi(argv[1]);
    }

    int server_fd = start_server(port);
    if (server_fd == -1) {
        fprintf(stderr, "Failed to start server. Exiting.\n");
        return 1;
    }

    while (1) {
        printf("\n[server] Waiting for a client...\n");

        struct sockaddr_in client_addr;
        socklen_t addr_len = sizeof(client_addr);
        int client_fd = accept(server_fd,
                               (struct sockaddr *)&client_addr,
                               &addr_len);
        if (client_fd == -1) {
            perror("accept() failed");
            continue;
        }

        printf("[server] Client connected from %s:%d\n",
               inet_ntoa(client_addr.sin_addr),
               ntohs(client_addr.sin_port));

        char buffer[BUFFER_SIZE];
        int bytes = receive_message(client_fd, buffer, BUFFER_SIZE);

        if (bytes > 0) {
            printf("[server] Received (%d bytes): \"%s\"\n", bytes, buffer);

            const char *response = "ACK: Message received by UniSync server!";
            send_message(client_fd, response);
            printf("[server] Sent response.\n");
        } else if (bytes == 0) {
            printf("[server] Client disconnected without sending data.\n");
        }

        close_socket(client_fd);
        printf("[server] Client connection closed.\n");
    }

    close_socket(server_fd);
    return 0;
}
