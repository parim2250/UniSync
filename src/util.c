#include "common.h"
#include "util.h"

static pthread_mutex_t out_lock = PTHREAD_MUTEX_INITIALIZER;

void us_print(const char *fmt, ...)
{
    va_list ap; va_start(ap, fmt);
    pthread_mutex_lock(&out_lock);
    vprintf(fmt, ap);
    fflush(stdout);
    pthread_mutex_unlock(&out_lock);
    va_end(ap);
}

void ui_line(const char *fmt, ...)
{
    char buf[2048];
    va_list ap; va_start(ap, fmt); vsnprintf(buf, sizeof buf, fmt, ap); va_end(ap);
    pthread_mutex_lock(&out_lock);
    printf("%s%s\n", g_color ? "\r\033[K" : "", buf);
    fflush(stdout);
    pthread_mutex_unlock(&out_lock);
}

void fmt_size(uint64_t n, char *out, size_t cap)
{
    const char *u[] = { "B", "KB", "MB", "GB", "TB" };
    double v = (double)n; int i = 0;
    while (v >= 1024.0 && i < 4) { v /= 1024.0; i++; }
    if (i == 0) snprintf(out, cap, "%llu B", (unsigned long long)n);
    else        snprintf(out, cap, "%.1f %s", v, u[i]);
}

double now_sec(void)
{
    struct timespec ts; clock_gettime(CLOCK_MONOTONIC, &ts);
    return (double)ts.tv_sec + (double)ts.tv_nsec / 1e9;
}

int mkdir_p(const char *path, mode_t mode)
{
    char tmp[PATH_MAX];
    snprintf(tmp, sizeof tmp, "%s", path);
    for (char *p = tmp + 1; *p; p++) {
        if (*p == '/') {
            *p = 0;
            if (mkdir(tmp, mode) < 0 && errno != EEXIST) return -1;
            *p = '/';
        }
    }
    if (mkdir(tmp, mode) < 0 && errno != EEXIST) return -1;
    return 0;
}

int fd_write_full(int fd, const void *b, size_t n)
{
    const uint8_t *p = b;
    while (n) {
        ssize_t w = write(fd, p, n);
        if (w < 0) { if (errno == EINTR) continue; return -1; }
        p += w; n -= (size_t)w;
    }
    return 0;
}

int fd_read_full(int fd, void *b, size_t n)
{
    uint8_t *p = b;
    while (n) {
        ssize_t r = read(fd, p, n);
        if (r == 0) return -1;
        if (r < 0) { if (errno == EINTR) continue; return -1; }
        p += r; n -= (size_t)r;
    }
    return 0;
}
