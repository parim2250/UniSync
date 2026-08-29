/*
 * transfer.h — UniSync File Transfer Engine
 *
 * Declarations for sending and receiving files in fixed-size chunks
 * over POSIX TCP sockets.
 */

#ifndef TRANSFER_H
#define TRANSFER_H

#include <sys/types.h>

/* ── Constants ─────────────────────────────────────────── */

#define CHUNK_SIZE 4096   /* Read/Write 4KB chunks at a time */

/* ── Function Declarations ─────────────────────────────── */

/*
 * send_file_data(sockfd, filepath)
 *   Reads `filepath` in CHUNK_SIZE blocks and transmits over `sockfd`.
 *   Returns total bytes sent, or -1 on error.
 */
ssize_t send_file_data(int sockfd, const char *filepath);

/*
 * receive_file_data(sockfd, output_path)
 *   Receives chunks from `sockfd` and writes them to `output_path`.
 *   Reads until the sender closes the connection.
 *   Returns total bytes written, or -1 on error.
 */
ssize_t receive_file_data(int sockfd, const char *output_path);

#endif /* TRANSFER_H */