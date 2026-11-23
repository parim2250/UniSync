/* history.h — persistent transfer history (~/.unisync/history.log) */
#ifndef HISTORY_H
#define HISTORY_H
#include <stdint.h>
void history_init(void);
/* dir = "SEND"/"RECV", status = SUCCESS / FAILED / INTERRUPTED / REJECTED / ... */
void history_add(const char *dir, const char *file, const char *peer, const char *status, uint64_t bytes);
void history_print(int last_n);
#endif
