/*
 * network.h — UniSync networking foundation
 *
 * This header declares all core socket helper functions.
 * Other modules (discovery, transfer, security) will
 * #include this file to use the networking layer.
 */

#ifndef NETWORK_H
#define NETWORK_H

/* ── Constants ─────────────────────────────────────────── */

#define DEFAULT_PORT   9876   /* TCP port for UniSync connections       */
#define BUFFER_SIZE    1024   /* Max bytes per send/recv call           */
#define BACKLOG        5      /* Max pending connections in listen queue */

/* ── Function Declarations ─────────────────────────────── */

/*
 * create_tcp_socket()
 *   Creates a new TCP socket and returns its file descriptor.
 *   Returns -1 on failure.
 */
int create_tcp_socket(void);

/*
 * start_server(port)
 *   Creates a socket, binds it to the given port, and starts listening.
 *   Returns the listening socket file descriptor.
 *   Returns -1 on failure.
 */
int start_server(int port);

/*
 * connect_to_server(ip, port)
 *   Creates a socket and connects to the server at the given IP and port.
 *   Returns the connected socket file descriptor.
 *   Returns -1 on failure.
 */
int connect_to_server(const char *ip, int port);

/*
 * send_message(sockfd, msg)
 *   Sends a null-terminated string through the socket.
 *   Returns number of bytes sent, or -1 on failure.
 */
int send_message(int sockfd, const char *msg);

/*
 * receive_message(sockfd, buffer, buf_size)
 *   Reads data from the socket into buffer (up to buf_size - 1 bytes).
 *   Null-terminates the result so you can print it safely.
 *   Returns number of bytes received, 0 if connection closed, -1 on error.
 */
int receive_message(int sockfd, char *buffer, int buf_size);

/*
 * close_socket(sockfd)
 *   Closes the socket and releases the file descriptor.
 */
void close_socket(int sockfd);

#endif /* NETWORK_H */