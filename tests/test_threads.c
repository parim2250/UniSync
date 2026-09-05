/*
 * test_threads.c — Standalone pthread demo
 * 
 * This demonstrates the core Unix threading concepts:
 *   - pthread_create()  → spawn a new thread
 *   - pthread_join()    → wait for a thread to finish
 *   - pthread_mutex_t   → protect shared data
 * 
 * Compile: gcc -o test_threads tests/test_threads.c -pthread
 * Run:     ./test_threads
 */

#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>

/* ─── Shared data ──────────────────────────────────────── */
/* Multiple threads will access this counter.
   Without a mutex, threads would corrupt each other's writes. */
static int shared_counter = 0;
static pthread_mutex_t counter_mutex = PTHREAD_MUTEX_INITIALIZER;

/* ─── What each thread does ────────────────────────────── */
/* This function runs in a SEPARATE thread.
   The argument is passed from pthread_create(). */
void *worker_thread(void *arg) {
    int thread_id = *(int *)arg;

    printf("  [Thread %d] Started (PID: %d, TID: %lu)\n",
           thread_id, getpid(), pthread_self());

    /* Simulate work: increment shared counter 5 times */
    for (int i = 0; i < 5; i++) {
        /* LOCK before touching shared data */
        pthread_mutex_lock(&counter_mutex);

        shared_counter++;
        printf("  [Thread %d] Counter is now: %d\n",
               thread_id, shared_counter);

        /* UNLOCK so other threads can access */
        pthread_mutex_unlock(&counter_mutex);

        sleep(1);  /* Simulate slow work */
    }

    printf("  [Thread %d] Finished!\n", thread_id);
    return NULL;
}

/* ─── Main (runs in the original thread) ───────────────── */
int main() {
    printf("=== UniSync Threading Demo ===\n\n");

    #define NUM_THREADS 3
    pthread_t threads[NUM_THREADS];
    int thread_ids[NUM_THREADS];

    /* Spawn threads */
    printf("Spawning %d threads...\n\n", NUM_THREADS);

    for (int i = 0; i < NUM_THREADS; i++) {
        thread_ids[i] = i + 1;

        /*
         * pthread_create() breakdown:
         *   &threads[i]    → where to store the thread handle
         *   NULL           → default thread attributes
         *   worker_thread  → function the thread will run
         *   &thread_ids[i] → argument passed to that function
         */
        int result = pthread_create(&threads[i], NULL,
                                     worker_thread, &thread_ids[i]);

        if (result != 0) {
            printf("  ERROR: Failed to create thread %d\n", i + 1);
            return 1;
        }
    }

    /* Wait for ALL threads to finish */
    printf("\nMain thread waiting for workers...\n\n");

    for (int i = 0; i < NUM_THREADS; i++) {
        /*
         * pthread_join() blocks until thread[i] finishes.
         * This is like wait() for processes, but for threads.
         */
        pthread_join(threads[i], NULL);
    }

    printf("\nAll threads done. Final counter: %d\n", shared_counter);
    printf("Expected: %d (3 threads × 5 increments)\n", NUM_THREADS * 5);

    pthread_mutex_destroy(&counter_mutex);
    return 0;
}