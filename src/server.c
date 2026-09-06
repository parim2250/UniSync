/*
 * server.c — UniSync TCP server (Layer 1 test)
 *
 * Usage:  ./bin/unisync-server [port]
 * Example: ./bin/unisync-server 9876
 *
 * The server loops forever, accepting one client at a time,
 * printing whatever the client sends, and replying with an ACK.
 * Press Ctrl+C to stop.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>  /* accept() */
#include <arpa/inet.h>   /* struct sockaddr_in, inet_ntoa() */

#include "network.h"

int main(int argc, char *argv[])
{
    /* Use command-line port if provided, otherwise default */
    int port = DEFAULT_PORT;
    if (argc > 1) {
        port = atoi(argv[1]);
    }

    /* Step 1: Start listening */
    int server_fd = start_server(port);
    if (server_fd == -1) {
        fprintf(stderr, "Failed to start server. Exiting.\n");
        return 1;
    }

    /* Step 2: Loop — accept connections one at a time */
    while (1) {
        printf("\n[server] Waiting for a client...\n");

        /*
         * accept() blocks here until a client calls connect().
         * It returns a NEW file descriptor for this specific client.
         * The original server_fd keeps listening for more clients.
         *
         * We also grab the client's IP address for logging.
         */
        struct sockaddr_in client_addr;
        socklen_t addr_len = sizeof(client_addr);
        int client_fd = accept(server_fd,
                               (struct sockaddr *)&client_addr,
                               &addr_len);
        if (client_fd == -1) {
            perror("accept() failed");
            continue;  /* Don't crash — try the next connection */
        }

        /* inet_ntoa() converts the binary IP back to "192.168.x.x" */
        printf("[server] Client connected from %s:%d\n",
               inet_ntoa(client_addr.sin_addr),
               ntohs(client_addr.sin_port));

        /* Step 3: Receive message from client */
        char buffer[BUFFER_SIZE];
        int bytes = receive_message(client_fd, buffer, BUFFER_SIZE);

        if (bytes > 0) {
            printf("[server] Received (%d bytes): \"%s\"\n", bytes, buffer);

            /* Step 4: Send a response back */
            const char *response = "ACK: Message received by UniSync server!";
            send_message(client_fd, response);
            printf("[server] Sent response.\n");
        } else if (bytes == 0) {
            printf("[server] Client disconnected without sending data.\n");
        }

        /* Step 5: Close this client connection */
        close_socket(client_fd);
        printf("[server] Client connection closed.\n");
    }

    /* This line is never reached (Ctrl+C kills the loop) */
    close_socket(server_fd);
    return 0;
}