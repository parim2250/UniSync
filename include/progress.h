/*
 * progress.h — Real-time visual progress bar
 */

#ifndef PROGRESS_H
#define PROGRESS_H

#include <stdint.h>
#include <time.h>

typedef struct {
    uint64_t total_bytes;
    uint64_t transferred_bytes;
    struct timespec start_time;
    struct timespec last_update;
} ProgressTracker;

/* Initialize tracker with total file size */
void progress_init(ProgressTracker *tracker, uint64_t total_bytes);

/* Call inside the transfer loop with new bytes read/written */
void progress_update(ProgressTracker *tracker, uint64_t bytes_added);

/* Finish bar and print newline when transfer completes */
void progress_finish(ProgressTracker *tracker);

#endif /* PROGRESS_H */