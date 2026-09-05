/*
 * thread_pool.c — Threaded transfer management
 *
 * Architecture:
 *
 *   Main Thread
 *       │
 *       ├── Accept Thread (listens for new connections)
 *       │       │
 *       │       ├── Handler Thread 1 (transfer to Aarav)
 *       │       ├── Handler Thread 2 (transfer to Rahul)
 *       │       └── Handler Thread 3 (transfer to Lab-PC)
 *       │
 *       └── UI Thread (displays progress, handles input)
 *
 * The accept thread runs accept() in a loop. Each time a new
 * peer connects, it spawns a handler thread and goes back to
 * listening. This means the server never blocks on a single
 * transfer.
 */

#include "thread_pool.h"
#include "logger.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <arpa/inet.h>
#include <sys/socket.h>

/* ─── External function from transfer.c ────────────────── */
/*
 * This is the key integration point. The thread pool calls
 * this function for each client. transfer.c handles the
 * actual protocol, file I/O, and checksums.
 *
 * Person 3 (resume) and Person 4 (security) will add their
 * logic INSIDE this function in transfer.c, not here.
 */
extern int transfer_handle_client(int client_fd,
                                   const char *peer_ip,
                                   const char *peer_name,
                                   TransferContext *ctx);

/* ─── Find a free slot in the active transfers array ───── */
/* MUST be called with pool->mutex already locked */
static int find_free_slot(ThreadPool *pool) {
    for (int i = 0; i < MAX_CONCURRENT_TRANSFERS; i++) {
        if (pool->active[i].status == XFER_IDLE ||
            pool->active[i].status == XFER_COMPLETE ||
            pool->active[i].status == XFER_FAILED) {
            return i;
        }
    }
    return -1;  /* All slots full */
}

/* ─── Handler thread function ──────────────────────────── */
/*
 * This is what each transfer thread runs.
 * One thread = one file transfer.
 *
 * The thread:
 *   1. Calls transfer_handle_client() to do the actual work
 *   2. Updates its status when done
 *   3. Closes the client socket
 *   4. Exits (pthread_detach means no join needed)
 */
static void *transfer_handler_thread(void *arg) {
    TransferContext *ctx = (TransferContext *)arg;

    log_info("[Thread %lu] Handling transfer from %s (%s)",
             pthread_self(), ctx->peer_name, ctx->peer_ip);

    ctx->status = XFER_IN_PROGRESS;
    ctx->start_time = time(NULL);

    /*
     * This is where the real work happens.
     * transfer_handle_client() will:
     *   - Receive the file offer
     *   - Accept/reject
     *   - Receive file chunks
     *   - Verify checksum
     *   - Update ctx->bytes_done as it goes
     */
    int result = transfer_handle_client(ctx->client_fd,
                                         ctx->peer_ip,
                                         ctx->peer_name,
                                         ctx);

    /* Update final status */
    if (result == 0) {
        ctx->status = XFER_COMPLETE;
        log_info("[Thread %lu] Transfer complete: %s",
                 pthread_self(), ctx->filename);
    } else {
        ctx->status = XFER_FAILED;
        log_error("[Thread %lu] Transfer failed: %s",
                  pthread_self(), ctx->filename);
    }

    /* Clean up the socket */
    close(ctx->client_fd);
    ctx->client_fd = -1;

    log_info("[Thread %lu] Exiting", pthread_self());
    return NULL;
}

/* ─── Accept thread function ───────────────────────────── */
/*
 * This thread runs accept() in a loop. It's the "front door"
 * of the server. Every incoming TCP connection arrives here.
 *
 * Flow:
 *   1. accept() blocks until a peer connects
 *   2. Lock the mutex, find a free slot
 *   3. Fill in the TransferContext for that slot
 *   4. Spawn a handler thread for this connection
 *   5. Go back to step 1
 */
static void *accept_thread_func(void *arg) {
    ThreadPool *pool = (ThreadPool *)arg;

    log_info("Accept thread started, listening for connections...");

    while (pool->running) {
        /* Set a timeout so we can check pool->running periodically */
        struct timeval tv = { .tv_sec = 1, .tv_usec = 0 };
        setsockopt(pool->server_fd, SOL_SOCKET, SO_RCVTIMEO,
                   &tv, sizeof(tv));

        struct sockaddr_in client_addr;
        socklen_t addr_len = sizeof(client_addr);

        int client_fd = accept(pool->server_fd,
                                (struct sockaddr *)&client_addr,
                                &addr_len);

        if (client_fd < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                continue;  /* Timeout, check if still running */
            }
            if (pool->running) {
                log_error("accept() failed: %s", strerror(errno));
            }
            continue;
        }

        /* Extract peer info */
        char peer_ip[16];
        inet_ntop(AF_INET, &client_addr.sin_addr,
                  peer_ip, sizeof(peer_ip));

        log_info("Incoming connection from %s:%d",
                 peer_ip, ntohs(client_addr.sin_port));

        /* Lock the pool to safely find a slot */
        pthread_mutex_lock(&pool->mutex);

        int slot = find_free_slot(pool);

        if (slot < 0) {
            /* All slots full — reject the connection */
            log_warn("Max concurrent transfers reached, rejecting %s",
                     peer_ip);
            pthread_mutex_unlock(&pool->mutex);
            close(client_fd);
            continue;
        }

        /* Initialize the transfer context for this slot */
        TransferContext *ctx = &pool->active[slot];
        memset(ctx, 0, sizeof(*ctx));
        ctx->client_fd = client_fd;
        ctx->slot = slot;
        ctx->status = XFER_IDLE;
        strncpy(ctx->peer_ip, peer_ip, sizeof(ctx->peer_ip) - 1);
        snprintf(ctx->peer_name, sizeof(ctx->peer_name),
                 "Peer-%d", slot + 1);

        pool->count++;

        pthread_mutex_unlock(&pool->mutex);

        /*
         * Spawn the handler thread.
         *
         * We use PTHREAD_CREATE_DETACHED so the thread cleans
         * up after itself when it exits. We don't need to call
         * pthread_join() for detached threads.
         *
         * Why detached? Because transfers finish at unpredictable
         * times. If we used join(), the accept thread would block
         * waiting for a transfer to finish instead of accepting
         * new connections.
         */
        pthread_attr_t attr;
        pthread_attr_init(&attr);
        pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);

        int err = pthread_create(&ctx->thread, &attr,
                                  transfer_handler_thread, ctx);

        pthread_attr_destroy(&attr);

        if (err != 0) {
            log_error("Failed to create handler thread: %s",
                      strerror(err));
            close(client_fd);
            pthread_mutex_lock(&pool->mutex);
            ctx->status = XFER_FAILED;
            pool->count--;
            pthread_mutex_unlock(&pool->mutex);
        } else {
            log_info("Handler thread %lu spawned for slot %d",
                     ctx->thread, slot);
        }
    }

    log_info("Accept thread exiting");
    return NULL;
}

/* ─── Public API ───────────────────────────────────────── */

int threadpool_init(ThreadPool *pool, int server_fd) {
    memset(pool, 0, sizeof(*pool));
    pool->server_fd = server_fd;
    pool->running = 0;
    pool->count = 0;

    /* Initialize all slots to idle */
    for (int i = 0; i < MAX_CONCURRENT_TRANSFERS; i++) {
        pool->active[i].status = XFER_IDLE;
        pool->active[i].client_fd = -1;
    }

    /* Initialize the mutex */
    if (pthread_mutex_init(&pool->mutex, NULL) != 0) {
        log_error("Failed to initialize thread pool mutex");
        return -1;
    }

    log_info("Thread pool initialized (max %d concurrent transfers)",
             MAX_CONCURRENT_TRANSFERS);
    return 0;
}

int threadpool_start(ThreadPool *pool) {
    pool->running = 1;

    int err = pthread_create(&pool->accept_thread, NULL,
                              accept_thread_func, pool);
    if (err != 0) {
        log_error("Failed to start accept thread: %s", strerror(err));
        pool->running = 0;
        return -1;
    }

    log_info("Thread pool started (accept thread: %lu)",
             pool->accept_thread);
    return 0;
}

void threadpool_stop(ThreadPool *pool) {
    log_info("Stopping thread pool...");
    pool->running = 0;

    /* Wait for accept thread to notice and exit */
    pthread_join(pool->accept_thread, NULL);

    /*
     * Wait a moment for handler threads to finish.
     * Since they're detached, we can't join them.
     * In production, you'd use a condition variable
     * or a shutdown flag that handlers check.
     */
    sleep(2);

    /* Close any remaining client sockets */
    pthread_mutex_lock(&pool->mutex);
    for (int i = 0; i < MAX_CONCURRENT_TRANSFERS; i++) {
        if (pool->active[i].client_fd >= 0) {
            close(pool->active[i].client_fd);
            pool->active[i].client_fd = -1;
        }
    }
    pthread_mutex_unlock(&pool->mutex);

    pthread_mutex_destroy(&pool->mutex);
    log_info("Thread pool stopped");
}

int threadpool_get_status(ThreadPool *pool,
                           TransferContext *snapshot,
                           int max_entries) {
    pthread_mutex_lock(&pool->mutex);

    int count = 0;
    for (int i = 0; i < MAX_CONCURRENT_TRANSFERS && count < max_entries; i++) {
        if (pool->active[i].status == XFER_IN_PROGRESS) {
            /* Copy the context (snapshot, not pointer) */
            snapshot[count] = pool->active[i];

            /* Calculate live speed */
            time_t elapsed = time(NULL) - pool->active[i].start_time;
            if (elapsed > 0) {
                snapshot[count].speed_mbps =
                    (double)pool->active[i].bytes_done /
                    (1024.0 * 1024.0 * elapsed);
            }

            count++;
        }
    }

    pthread_mutex_unlock(&pool->mutex);
    return count;
}

int threadpool_cancel_transfer(ThreadPool *pool, int slot) {
    if (slot < 0 || slot >= MAX_CONCURRENT_TRANSFERS) {
        return -1;
    }

    pthread_mutex_lock(&pool->mutex);

    if (pool->active[slot].status != XFER_IN_PROGRESS) {
        pthread_mutex_unlock(&pool->mutex);
        return -1;
    }

    /*
     * Signal the handler thread to stop by closing its socket.
     * The thread's read()/write() calls will fail with EBADF,
     * and it will exit cleanly.
     */
    log_info("Cancelling transfer in slot %d", slot);
    close(pool->active[slot].client_fd);
    pool->active[slot].client_fd = -1;
    pool->active[slot].status = XFER_CANCELLED;

    pthread_mutex_unlock(&pool->mutex);
    return 0;
}