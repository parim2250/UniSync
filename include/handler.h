#ifndef HANDLER_H
#define HANDLER_H

typedef struct {
    int  client_fd;
    char save_dir[256];
    char client_ip[16];
} ClientContext;

void *handle_client(void *arg);

#endif