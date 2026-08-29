/*
 * transfer.c — UniSync File Transfer Implementation
 *
 * Implements chunked reading from disk to socket, and chunked writing
 * from socket to disk using raw Unix file descriptors.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/socket.h>

#include "transfer.h"

/* Helper: Ensures ALL bytes are sent over the socket (handles partial sends) */
static ssize_t send_all(int sockfd, const char *buf, size_t len)
{
    size_t total_sent = 0;
    while (total_sent < len) {
        ssize_t sent = send(sockfd, buf + total_sent, len - total_sent, 0);
        if (sent <= 0) {
            perror("send() failed during file transfer");
            return -1;
        }
        total_sent += sent;
    }
    return total_sent;
}

/* Helper: Ensures ALL bytes are written to disk (handles partial writes) */
static ssize_t write_all(int file_fd, const char *buf, size_t len)
{
    size_t total_written = 0;
    while (total_written < len) {
        ssize_t written = write(file_fd, buf + total_written, len - total_written);
        if (written <= 0) {
            perror("write() failed to disk");
            return -1;
        }
        total_written += written;
    }
    return total_written;
}

/* ── Send File Data ────────────────────────────────────── */
ssize_t send_file_data(int sockfd, const char *filepath)
{
    int file_fd = open(filepath, O_RDONLY);
    if (file_fd == -1) {
        perror("open() failed for reading");
        return -1;
    }

    char buffer[CHUNK_SIZE];
    ssize_t bytes_read;
    ssize_t total_sent = 0;

    /* Loop: Read CHUNK_SIZE from disk -> send over network */
    while ((bytes_read = read(file_fd, buffer, CHUNK_SIZE)) > 0) {
        if (send_all(sockfd, buffer, bytes_read) == -1) {
            close(file_fd);
            return -1;
        }
        total_sent += bytes_read;
    }

    if (bytes_read == -1) {
        perror("read() failed from file");
        close(file_fd);
        return -1;
    }

    close(file_fd);
    printf("[transfer] Sent total %ld bytes from file: %s\n", (long)total_sent, filepath);
    return total_sent;
}

/* ── Receive File Data ─────────────────────────────────── */
ssize_t receive_file_data(int sockfd, const char *output_path)
{
    /* O_CREAT: create file if missing, O_TRUNC: overwrite if exists */
    /* 0644: permissions (rw-r--r--) */
    int file_fd = open(output_path, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (file_fd == -1) {
        perror("open() failed for writing");
        return -1;
    }

    char buffer[CHUNK_SIZE];
    ssize_t bytes_received;
    ssize_t total_written = 0;

    /* Loop: Read from network socket -> write to disk */
    while ((bytes_received = recv(sockfd, buffer, CHUNK_SIZE, 0)) > 0) {
        if (write_all(file_fd, buffer, bytes_received) == -1) {
            close(file_fd);
            return -1;
        }
        total_written += bytes_received;
    }

    if (bytes_received == -1) {
        perror("recv() failed from socket");
        close(file_fd);
        return -1;
    }

    close(file_fd);
    printf("[transfer] Saved total %ld bytes to file: %s\n", (long)total_written, output_path);
    return total_written;
}