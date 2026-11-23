#ifndef HANDLER_H
#define HANDLER_H

typedef struct {
    int  client_fd;
    char save_dir[4096];
    char client_ip[16];
} ClientContext;

void  handler_init(int max_concurrent);    /* limits simultaneous transfers (semaphore) */
void *handle_client(void *arg);            /* one pthread per incoming connection        */

#endif
