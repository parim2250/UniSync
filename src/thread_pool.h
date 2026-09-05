/*
 * thread_pool.h — Threaded transfer management for UniSync
 *
 * This module manages concurrent file transfers using POSIX threads.
 * Each incoming connection gets its own handler thread, so the server
 * can accept new connections while transfers are in progress.
 *
 * Unix concepts demonstrated:
 *   - pthread_create / pthread_join / pthread_detach
 *   - pthread_mutex_t for thread-safe data access
 *   - Concurrent server architecture
 *   - Thread lifecycle management
 *
 * Integration note (for Person 3/4/5):
 *   This module calls transfer_handle_client() from transfer.c
 *   You do NOT need to modify this file. If you need per-transfer
 *   hooks (e.g., for resume or security), add them in transfer.c.
 */

#ifndef THREAD_POOL_H
#define THREAD_POOL_H

#include <pthread.h>
#include <stdint.h>
#include <time.h>

/* Maximum simultaneous transfers */
#define MAX_CONCURRENT_TRANSFERS 5

/* ─── Transfer state (thread-safe, read with mutex) ────── */
typedef enum {
    XFER_IDLE,
    XFER_IN_PROGRESS,
    XFER_COMPLETE,
    XFER_FAILED,
    XFER_CANCELLED
} TransferStatus;

/* ─── Per-transfer context (one per thread) ────────────── */
/*
 * Each handler thread gets its own TransferContext.
 * This struct holds everything the thread needs to know
 * about its specific transfer.
 */
typedef struct {
    int             client_fd;          /* TCP socket to peer */
    int             slot;               /* Index in active array */
    char            filename[256];      /* File being transferred */
    char            peer_name[64];      /* Peer hostname */
    char            peer_ip[16];        /* Peer IP address */
    uint64_t        file_size;          /* Total bytes */
    uint64_t        bytes_done;         /* Bytes transferred so far */
    TransferStatus  status;             /* Current state */
    double          speed_mbps;         /* Transfer speed */
    time_t          start_time;         /* When transfer began */
    pthread_t       thread;             /* Thread handle */
} TransferContext;

/* ─── Thread pool state ────────────────────────────────── */
/*
 * The server maintains an array of active transfers.
 * The mutex protects this array from concurrent access
 * (multiple threads reading/writing simultaneously).
 */
typedef struct {
    TransferContext     active[MAX_CONCURRENT_TRANSFERS];
    int                 count;              /* How many active */
    pthread_mutex_t     mutex;              /* Protects this struct */
    int                 running;            /* Server alive flag */
    int                 server_fd;          /* Listening socket */
    pthread_t           accept_thread;      /* Thread that accepts connections */
} ThreadPool;

/* ─── Public API ───────────────────────────────────────── */

/* Initialize the thread pool (call once at startup) */
int threadpool_init(ThreadPool *pool, int server_fd);

/* Start the accept thread (begins accepting connections) */
int threadpool_start(ThreadPool *pool);

/* Stop all threads and clean up (call on shutdown) */
void threadpool_stop(ThreadPool *pool);

/* Get a snapshot of active transfers (thread-safe) */
int threadpool_get_status(ThreadPool *pool,
                          TransferContext *snapshot,
                          int max_entries);

/* Cancel a specific transfer by slot number */
int threadpool_cancel_transfer(ThreadPool *pool, int slot);

#endif