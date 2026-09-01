/*
 * protocol.c — UniSync Wire Protocol (With Progress Bar & SIGPIPE Fix)
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <endian.h>
#include <libgen.h>

#include "protocol.h"
#include "progress.h"

#define CHUNK_SIZE 65536  /* 64KB chunk size for fast transfers */

static ssize_t send_all(int sockfd, const char *buf, size_t len)
{
    size_t total = 0;
    while (total < len) {
        /* MSG_NOSIGNAL stops Linux from killing sender on network hiccups */
        ssize_t s = send(sockfd, buf + total, len - total, MSG_NOSIGNAL);
        if (s <= 0) { 
            perror("\nsend() failed"); 
            return -1; 
        }
        total += s;
    }
    return total;
}

static ssize_t recv_all(int sockfd, char *buf, size_t len)
{
    size_t total = 0;
    while (total < len) {
        ssize_t r = recv(sockfd, buf + total, len - total, 0);
        if (r <= 0) { 
            if (r < 0) perror("\nrecv() failed");
            return -1; 
        }
        total += r;
    }
    return total;
}

static ssize_t write_all(int fd, const char *buf, size_t len)
{
    size_t total = 0;
    while (total < len) {
        ssize_t w = write(fd, buf + total, len - total);
        if (w <= 0) { 
            perror("\nwrite() failed"); 
            return -1; 
        }
        total += w;
    }
    return total;
}

int build_file_header(FileHeader *header, const char *filepath)
{
    struct stat st;
    if (stat(filepath, &st) == -1) {
        perror("stat() failed");
        return -1;
    }

    char path_copy[512];
    strncpy(path_copy, filepath, sizeof(path_copy) - 1);
    path_copy[sizeof(path_copy) - 1] = '\0';
    char *fname = basename(path_copy);

    memset(header, 0, sizeof(FileHeader));
    memcpy(header->magic, UNISYNC_MAGIC, UNISYNC_MAGIC_LEN);
    header->version  = UNISYNC_VERSION;
    header->filesize = (uint64_t)st.st_size;
    strncpy(header->filename, fname, MAX_FILENAME_LEN - 1);

    return 0;
}

int send_file_header(int sockfd, const FileHeader *header)
{
    FileHeader net_header = *header;
    net_header.version  = htonl(header->version);
    net_header.filesize = htobe64(header->filesize);

    if (send_all(sockfd, (const char *)&net_header, sizeof(FileHeader)) == -1) {
        return -1;
    }

    printf("[protocol] Sent header: file=\"%s\", size=%lu bytes\n",
           header->filename, (unsigned long)header->filesize);
    return 0;
}

int receive_file_header(int sockfd, FileHeader *header)
{
    if (recv_all(sockfd, (char *)header, sizeof(FileHeader)) == -1) {
        return -1;
    }

    if (memcmp(header->magic, UNISYNC_MAGIC, UNISYNC_MAGIC_LEN) != 0) {
        fprintf(stderr, "[protocol] Invalid magic number! Not a UniSync stream.\n");
        return -1;
    }

    header->version  = ntohl(header->version);
    header->filesize = be64toh(header->filesize);
    header->filename[MAX_FILENAME_LEN - 1] = '\0';

    printf("[protocol] Received header: file=\"%s\", size=%lu bytes, version=%u\n",
           header->filename, (unsigned long)header->filesize, header->version);
    return 0;
}

ssize_t send_file_payload(int sockfd, const char *filepath, uint64_t filesize)
{
    int file_fd = open(filepath, O_RDONLY);
    if (file_fd == -1) { perror("open()"); return -1; }

    char buffer[CHUNK_SIZE];
    uint64_t total_sent = 0;

    ProgressTracker tracker;
    progress_init(&tracker, filesize);

    while (total_sent < filesize) {
        uint64_t remaining = filesize - total_sent;
        size_t to_read = (remaining < CHUNK_SIZE) ? remaining : CHUNK_SIZE;

        ssize_t r = read(file_fd, buffer, to_read);
        if (r < 0) {
            perror("\nread() failed");
            close(file_fd);
            return -1;
        }
        if (r == 0) {
            fprintf(stderr, "\n[protocol] Reached EOF before full size sent.\n");
            close(file_fd);
            return -1;
        }

        if (send_all(sockfd, buffer, r) == -1) {
            close(file_fd);
            return -1;
        }

        total_sent += r;
        progress_update(&tracker, r);
    }

    progress_finish(&tracker);
    close(file_fd);
    return (ssize_t)total_sent;
}

ssize_t receive_file_payload(int sockfd, const char *output_path, uint64_t filesize)
{
    int file_fd = open(output_path, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (file_fd == -1) { perror("open()"); return -1; }

    char buffer[CHUNK_SIZE];
    uint64_t total_written = 0;

    ProgressTracker tracker;
    progress_init(&tracker, filesize);

    while (total_written < filesize) {
        uint64_t remaining = filesize - total_written;
        size_t to_read = (remaining < CHUNK_SIZE) ? remaining : CHUNK_SIZE;

        ssize_t r = recv(sockfd, buffer, to_read, 0);
        if (r < 0) {
            perror("\nrecv() failed");
            close(file_fd);
            return -1;
        }
        if (r == 0) {
            fprintf(stderr, "\n[protocol] Connection closed by sender prematurely.\n");
            close(file_fd);
            return -1;
        }

        if (write_all(file_fd, buffer, r) == -1) {
            close(file_fd);
            return -1;
        }

        total_written += r;
        progress_update(&tracker, r);
    }

    progress_finish(&tracker);
    close(file_fd);
    return (ssize_t)total_written;
}