/*
 * network.c — UniSync socket helper implementations
 *
 * All POSIX socket calls are wrapped here so the rest of
 * the project never calls socket()/bind()/listen() directly.
 */
#include "network.h"
#include <stdio.h>       /* perror(), printf() */
#include <stdlib.h>      /* exit() */
#include <string.h>      /* strlen(), memset() */
#include <unistd.h>      /* close() */
#include <arpa/inet.h>   /* inet_addr(), htons(), struct sockaddr_in */
#include <sys/socket.h>  /* socket(), bind(), listen(), accept(), send(), recv() */

/* ── Create a TCP socket ──────────────────────────────── */
int create_tcp_socket(void)
{
    /*
     * socket(domain, type, protocol)
     *   AF_INET     = IPv4 addresses
     *   SOCK_STREAM = TCP (reliable, ordered byte stream)
     *   0           = let the OS pick the right protocol for TCP
     *
     * Returns a file descriptor (small int like 3, 4, 5...)
     * or -1 if something went wrong.
     */
    int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd == -1) {
        perror("socket() failed");
    }
    return sockfd;
}

/* ── Start a TCP server ───────────────────────────────── */
int start_server(int port)
{
    int server_fd = create_tcp_socket();
    if (server_fd == -1) return -1;

    /*
     * SO_REUSEADDR lets us restart the server immediately
     * without waiting for the OS to release the port.
     * Without this, you'd get "Address already in use" errors
     * every time you kill and restart the server.
     */
    int opt = 1;
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) == -1) {
        perror("setsockopt() failed");
        close(server_fd);
        return -1;
    }

    /*
     * struct sockaddr_in holds an IPv4 address + port.
     * We fill it in and bind our socket to it.
     */
    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));     /* Zero out the struct (important!) */
    addr.sin_family      = AF_INET;     /* IPv4 */
    addr.sin_addr.s_addr = INADDR_ANY;  /* Listen on ALL network interfaces */
    addr.sin_port        = htons(port); /* Convert port to network byte order */

    /*
     * bind() attaches the socket to the address/port.
     * Think of it as "registering your phone number."
     */
    if (bind(server_fd, (struct sockaddr *)&addr, sizeof(addr)) == -1) {
        perror("bind() failed");
        close(server_fd);
        return -1;
    }

    /*
     * listen() marks the socket as "passive" — it will accept
     * incoming connections. BACKLOG = max queued connections.
     */
    if (listen(server_fd, BACKLOG) == -1) {
        perror("listen() failed");
        close(server_fd);
        return -1;
    }

    printf("[server] Listening on port %d...\n", port);
    return server_fd;
}

/* ── Connect to a TCP server ──────────────────────────── */
int connect_to_server(const char *ip, int port)
{
    int client_fd = create_tcp_socket();
    if (client_fd == -1) return -1;

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family      = AF_INET;
    addr.sin_addr.s_addr = inet_addr(ip);  /* Convert "127.0.0.1" to binary */
    addr.sin_port        = htons(port);

    /*
     * connect() initiates the TCP handshake with the server.
     * This blocks until the server accept()s or the connection fails.
     */
    if (connect(client_fd, (struct sockaddr *)&addr, sizeof(addr)) == -1) {
        perror("connect() failed");
        close(client_fd);
        return -1;
    }

    printf("[client] Connected to %s:%d\n", ip, port);
    return client_fd;
}

/* ── Send a string message ────────────────────────────── */
int send_message(int sockfd, const char *msg)
{
    /*
     * send(sockfd, data, length, flags)
     *   We send strlen(msg) + 1 to include the null terminator,
     *   so the receiver gets a proper C string.
     *   flags = 0 means default behavior.
     */
    int bytes = send(sockfd, msg, strlen(msg) + 1, 0);
    if (bytes == -1) {
        perror("send() failed");
    }
    return bytes;
}

/* ── Receive a string message ─────────────────────────── */
int receive_message(int sockfd, char *buffer, int buf_size)
{
    /*
     * recv(sockfd, buffer, max_bytes, flags)
     *   Reads up to buf_size - 1 bytes, leaving room for '\0'.
     *   Returns 0 if the other side closed the connection.
     */
    int bytes = recv(sockfd, buffer, buf_size - 1, 0);
    if (bytes == -1) {
        perror("recv() failed");
    } else if (bytes > 0) {
        buffer[bytes] = '\0';  /* Null-terminate so printf works safely */
    }
    return bytes;
}

/* ── Close a socket ───────────────────────────────────── */
void close_socket(int sockfd)
{
    if (sockfd >= 0) {
        close(sockfd);  /* close() works on sockets just like files */
    }
}