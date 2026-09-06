/*
 * handler.h — Per-client transfer handler (runs in its own thread)
 */

#ifndef HANDLER_H
#define HANDLER_H

typedef struct {
    int   client_fd;
    char  save_dir[256];
    char  client_ip[16];
} ClientContext;

/* Thread entry point — takes ClientContext*, handles one transfer, exits */
void *handle_client(void *arg);

#endif