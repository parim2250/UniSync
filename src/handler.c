#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <sys/socket.h>

#include "logger.h"
#include "handler.h"
#include "network.h"
#include "protocol.h"

static pthread_mutex_t io_lock = PTHREAD_MUTEX_INITIALIZER;

void *handle_client(void *arg)
{
    ClientContext *ctx = (ClientContext *)arg;
    int fd = ctx->client_fd;

    FileHeader header;
    if (receive_file_header(fd, &header) == -1) {
        close_socket(fd);
        free(ctx);
        return NULL;
    }

    log_info("Incoming from %s file=%s size=%lu mode=0%o",
             ctx->client_ip, header.filename,
             (unsigned long)header.filesize, header.mode & 0777);

    double mb = header.filesize / (1024.0 * 1024.0);
    char answer[8];

    /* Lock so only ONE thread asks y/n at a time */
    pthread_mutex_lock(&io_lock);
    printf("\n========================================\n");
    printf("Incoming from %s\n", ctx->client_ip);
    printf("File: \"%s\" (%.2f MB)\n", header.filename, mb);
    printf("Accept? (y/n): ");
    fflush(stdout);

    if (!fgets(answer, sizeof(answer), stdin)) {
        answer[0] = 'n';
    }
    pthread_mutex_unlock(&io_lock);

    if (answer[0] != 'y' && answer[0] != 'Y') {
        send(fd, "REJECT", 7, MSG_NOSIGNAL);
        log_info("Rejected %s from %s", header.filename, ctx->client_ip);
        pthread_mutex_lock(&io_lock);
        printf("[thread] Rejected \"%s\" from %s\n", header.filename, ctx->client_ip);
        pthread_mutex_unlock(&io_lock);
        close_socket(fd);
        free(ctx);
        return NULL;
    }

    send(fd, "ACCEPT", 7, MSG_NOSIGNAL);
    log_info("Accepted %s from %s", header.filename, ctx->client_ip);

    pthread_mutex_lock(&io_lock);
    printf("[thread] Accepted \"%s\" from %s — receiving...\n",
           header.filename, ctx->client_ip);
    pthread_mutex_unlock(&io_lock);

    char output_path[512];
    snprintf(output_path, sizeof(output_path), "%s/%s", ctx->save_dir, header.filename);

    ssize_t bytes = receive_file_payload(fd, output_path, header.filesize, header.mode);

    if (bytes < 0)
        log_error("Transfer failed: %s from %s", header.filename, ctx->client_ip);
    else
        log_info("Transfer complete: %s (%ld bytes)", header.filename, (long)bytes);

    pthread_mutex_lock(&io_lock);
    if (bytes < 0)
        printf("[thread] FAILED \"%s\"\n", header.filename);
    else
        printf("[thread] DONE \"%s\" → %s (%ld bytes)\n",
               header.filename, output_path, (long)bytes);
    pthread_mutex_unlock(&io_lock);

    close_socket(fd);
    free(ctx);
    return NULL;
}