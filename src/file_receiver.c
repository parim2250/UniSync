#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <arpa/inet.h>
#include <sys/socket.h>

#include "logger.h"
#include "network.h"
#include "errors.h"
#include "handler.h"

int main(int argc, char *argv[])
{
    int port             = (argc > 1) ? atoi(argv[1]) : DEFAULT_PORT;
    const char *save_dir = (argc > 2) ? argv[2] : ".";

    if (port < 1 || port > 65535) {
        fprintf(stderr, "[error] bad port\n");
        return 1;
    }
    if (validate_dir_writable(save_dir) != UNISYNC_SUCCESS) {
        fprintf(stderr, "[error] directory not writable: %s\n", save_dir);
        return 1;
    }

    int server_fd = start_server(port);
    if (server_fd == -1) return 1;

    printf("[receiver] Multithreaded + Accept/Reject on port %d\n", port);
    printf("[receiver] Saving to: %s\n", save_dir);
    printf("[receiver] Ctrl+C to stop.\n");

    while (1) {
        struct sockaddr_in caddr;
        socklen_t clen = sizeof(caddr);

        int client_fd = accept(server_fd, (struct sockaddr *)&caddr, &clen);
        if (client_fd == -1) {
            perror("accept");
            continue;
        }

        ClientContext *ctx = malloc(sizeof(ClientContext));
        if (!ctx) {
            close_socket(client_fd);
            continue;
        }
        ctx->client_fd = client_fd;
        strncpy(ctx->save_dir, save_dir, sizeof(ctx->save_dir) - 1);
        ctx->save_dir[sizeof(ctx->save_dir) - 1] = '\0';
        strncpy(ctx->client_ip, inet_ntoa(caddr.sin_addr), sizeof(ctx->client_ip) - 1);
        ctx->client_ip[sizeof(ctx->client_ip) - 1] = '\0';

        pthread_t tid;
        if (pthread_create(&tid, NULL, handle_client, ctx) != 0) {
            perror("pthread_create");
            close_socket(client_fd);
            free(ctx);
            continue;
        }
        pthread_detach(tid);
    }

    close_socket(server_fd);
    return 0;
}