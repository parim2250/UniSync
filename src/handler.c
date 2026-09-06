/*
 * handler.c — Handles one incoming file transfer in a dedicated thread
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <sys/socket.h>

#include "handler.h"
#include "network.h"
#include "protocol.h"

/* Mutex to prevent multiple threads printing over each other */
static pthread_mutex_t print_lock = PTHREAD_MUTEX_INITIALIZER;

void *handle_client(void *arg)
{
    ClientContext *ctx = (ClientContext *)arg;
    int fd = ctx->client_fd;

    pthread_mutex_lock(&print_lock);
    printf("\n[thread] New connection from %s\n", ctx->client_ip);
    pthread_mutex_unlock(&print_lock);

    /* Step 1: Receive header */
    FileHeader header;
    if (receive_file_header(fd, &header) == -1) {
        close_socket(fd);
        free(ctx);
        return NULL;
    }

    /* Step 2: Auto-accept in multithreaded mode (no interactive prompt
       because prompts from multiple threads would collide).
       Person 4 can add PIN-based auth later. */
    send(fd, "ACCEPT", 7, 0);

    pthread_mutex_lock(&print_lock);
    printf("[thread-%s] Accepting \"%s\" (%.2f MB)\n",
           ctx->client_ip, header.filename,
           header.filesize / (1024.0 * 1024.0));
    pthread_mutex_unlock(&print_lock);

    /* Step 3: Build output path */
    char output_path[512];
    snprintf(output_path, sizeof(output_path), "%s/%s",
             ctx->save_dir, header.filename);

    /* Step 4: Receive file payload */
    ssize_t bytes = receive_file_payload(fd, output_path, header.filesize);

    pthread_mutex_lock(&print_lock);
    if (bytes < 0) {
        printf("[thread-%s] Transfer FAILED\n", ctx->client_ip);
    } else {
        printf("[thread-%s] Transfer DONE — %ld bytes saved to %s\n",
               ctx->client_ip, (long)bytes, output_path);
    }
    pthread_mutex_unlock(&print_lock);

    close_socket(fd);
    free(ctx);
    return NULL;
}