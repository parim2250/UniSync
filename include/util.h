/* util.h — small helpers: thread-safe printing, sizes, time, mkdir -p, fd I/O */
#ifndef UTIL_H
#define UTIL_H
#include <stddef.h>
#include <stdint.h>
#include <sys/types.h>

void   us_print(const char *fmt, ...);          /* thread-safe printf              */
void   ui_line(const char *fmt, ...);           /* clears a live progress line first, adds \n */
void   fmt_size(uint64_t n, char *out, size_t cap);
double now_sec(void);
int    mkdir_p(const char *path, mode_t mode);
int    fd_write_full(int fd, const void *b, size_t n);
int    fd_read_full(int fd, void *b, size_t n);
#endif
