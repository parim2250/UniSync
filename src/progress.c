/*
 * progress.c — Renders progress bar with speed (MB/s) and %
 */

 #define _POSIX_C_SOURCE 199309L

#include <stdio.h>
#include <time.h>
#include <stdint.h>
#include "progress.h"

#define BAR_WIDTH 30

static double get_elapsed_sec(struct timespec *start, struct timespec *end)
{
    return (end->tv_sec - start->tv_sec) + (end->tv_nsec - start->tv_nsec) / 1e9;
}

void progress_init(ProgressTracker *tracker, uint64_t total_bytes)
{
    tracker->total_bytes = total_bytes;
    tracker->transferred_bytes = 0;
    clock_gettime(CLOCK_MONOTONIC, &tracker->start_time);
    tracker->last_update = tracker->start_time;
}

void progress_update(ProgressTracker *tracker, uint64_t bytes_added)
{
    tracker->transferred_bytes += bytes_added;

    struct timespec now;
    clock_gettime(CLOCK_MONOTONIC, &now);

    /* Limit terminal updates to every 50ms to avoid flicker */
    if (get_elapsed_sec(&tracker->last_update, &now) < 0.05 && 
        tracker->transferred_bytes < tracker->total_bytes) {
        return;
    }
    tracker->last_update = now;

    double percent = 0.0;
    if (tracker->total_bytes > 0) {
        percent = (double)tracker->transferred_bytes / tracker->total_bytes * 100.0;
    }

    int filled = (int)(percent / 100.0 * BAR_WIDTH);
    if (filled > BAR_WIDTH) filled = BAR_WIDTH;

    double elapsed = get_elapsed_sec(&tracker->start_time, &now);
    double speed_mb = 0.0;
    if (elapsed > 0.001) {
        speed_mb = (tracker->transferred_bytes / (1024.0 * 1024.0)) / elapsed;
    }

    double transferred_mb = tracker->transferred_bytes / (1024.0 * 1024.0);
    double total_mb       = tracker->total_bytes / (1024.0 * 1024.0);

    /* Render bar: [=========>          ] 45.2% (5.1/10.0 MB) @ 12.3 MB/s */
    printf("\r[");
    for (int i = 0; i < BAR_WIDTH; i++) {
        if (i < filled) printf("=");
        else if (i == filled) printf(">");
        else printf(" ");
    }
    printf("] %5.1f%% (%6.1f/%6.1f MB) @ %5.1f MB/s",
           percent, transferred_mb, total_mb, speed_mb);

    fflush(stdout);
}

void progress_finish(ProgressTracker *tracker)
{
    progress_update(tracker, 0);
    printf("\n");
}