#ifndef LOGGER_H
#define LOGGER_H

void log_init(const char *path);
void log_info(const char *fmt, ...);
void log_error(const char *fmt, ...);
void log_close(void);

#endif