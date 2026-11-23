/*
 * transfer.h — UniSync file transfer engine
 *   - folder walking, chunked file sending, resumable receiving with SHA-256 verification
 *   - everything uses low-level Unix calls: open / read / write / lseek / ftruncate / fsync / rename / stat
 */
#ifndef TRANSFER_H
#define TRANSFER_H

#include <stdint.h>
#include <stddef.h>
#include <limits.h>
#include <sys/types.h>
#include "progress.h"

#define CHUNK_SIZE (64 * 1024)         /* bytes per network chunk */

typedef struct {
    char     abs[PATH_MAX], rel[4096];
    int      isdir, done, failed;
    uint64_t size;
    uint32_t mode;
    uint8_t  hash[32];
} Entry;

typedef struct { Entry *v; size_t n, cap; } EntryList;

/* ---- sender side ---- */
int  entries_collect(EntryList *l, const char *abs, const char *rel);   /* file or folder (recursive) */
void entries_free(EntryList *l);
const char *file_type_of(const char *name);
/* Send bytes [offset,size) of a file. 0 ok, -1 network error, -2 file problem.
 * If *drop > 0 the connection is cut after that many bytes (testing aid for the resume demo). */
int  send_file_body(int fd, const char *abs, uint64_t offset, uint64_t size, uint8_t *buf, size_t chunk,
                    ProgressTracker *pt, int *drop, uint64_t *sent_this_attempt);

/* ---- receiver side ---- */
int  safe_relpath(const char *rel);                                      /* rejects "..", absolute paths */
/* Decide where to resume: sets *already=1 if an identical file is already in place. Returns offset. */
uint64_t resume_offset(const char *path, uint64_t size, const uint8_t hash[32], int *already);
/* Receive bytes [offset,size), verify SHA-256, move into place. 1 verified, 0 corrupt, -1 network/io error. */
int  receive_file_body(int fd, const char *path, uint64_t size, uint64_t offset, uint32_t mode,
                       const uint8_t hash[32], uint8_t *buf, size_t chunk, ProgressTracker *pt);
void run_hook(const char *path);                                         /* fork + exec the --exec program */

#endif /* TRANSFER_H */
