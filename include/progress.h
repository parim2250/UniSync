/*
 * progress.h — live progress bar (percent, bytes, speed, ETA)
 * One tracker per transfer. With a single active transfer the bar updates in place;
 * with several concurrent transfers each prints a line once per second.
 */
#ifndef PROGRESS_H
#define PROGRESS_H

#include <stdint.h>
#include <time.h>

typedef struct {
    int      id, receiving;           /* receiving: 1 = download, 0 = upload */
    char     peer[64], file[256];
    uint64_t total_bytes;             /* whole transfer                */
    uint64_t transferred_bytes;       /* done so far (incl. skipped/resumed bytes) */
    uint64_t moved_bytes;             /* bytes that actually crossed the wire */
    double   start, last_print;
} ProgressTracker;

/* Start tracking a transfer of total_bytes with `peer` */
void progress_init(ProgressTracker *t, const char *peer, int receiving, uint64_t total_bytes);
void progress_file(ProgressTracker *t, const char *file);        /* current file name   */
void progress_update(ProgressTracker *t, uint64_t bytes_added);  /* call in the loop    */
void progress_skip(ProgressTracker *t, uint64_t bytes);          /* resumed/already-there bytes */
void progress_set_done(ProgressTracker *t, uint64_t done);
/* Finish: prints a ✔/✘ summary line. `ok` = 1 on success. */
void progress_finish(ProgressTracker *t, const char *status, int ok);

#endif /* PROGRESS_H */
