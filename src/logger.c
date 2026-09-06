#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <time.h>
#include <pthread.h>
#include <sys/stat.h>

#include "logger.h"

static FILE *log_fp = NULL;
static pthread_mutex_t log_lock = PTHREAD_MUTEX_INITIALIZER;

void log_init(const char *path)
{
    mkdir("logs", 0755);
    log_fp = fopen(path, "a");
    if (!log_fp) perror("log_init");
}

static void log_write(const char *level, const char *fmt, va_list ap)
{
    if (!log_fp) return;

    time_t now = time(NULL);
    struct tm t;
    localtime_r(&now, &t);
    char ts[32];
    strftime(ts, sizeof(ts), "%Y-%m-%d %H:%M:%S", &t);

    pthread_mutex_lock(&log_lock);
    fprintf(log_fp, "[%s] %s: ", ts, level);
    vfprintf(log_fp, fmt, ap);
    fprintf(log_fp, "\n");
    fflush(log_fp);
    pthread_mutex_unlock(&log_lock);
}

void log_info(const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    log_write("INFO", fmt, ap);
    va_end(ap);
}

void log_error(const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    log_write("ERROR", fmt, ap);
    va_end(ap);
}

void log_close(void)
{
    if (log_fp) {
        fclose(log_fp);
        log_fp = NULL;
    }
}