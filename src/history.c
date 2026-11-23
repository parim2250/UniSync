/* Persistent transfer history (tab-separated file, appended with O_APPEND). */
#include "common.h"
#include "util.h"
#include "history.h"

static char hpath[PATH_MAX + 32];
static pthread_mutex_t hl = PTHREAD_MUTEX_INITIALIZER;

void history_init(void) { snprintf(hpath, sizeof hpath, "%s/history.log", cfg.datadir); }

void history_add(const char *dir, const char *file, const char *peer, const char *status, uint64_t bytes)
{
    char ts[32], l[PATH_MAX + 256];
    time_t t = time(NULL); struct tm tmv; localtime_r(&t, &tmv);
    strftime(ts, sizeof ts, "%Y-%m-%d %H:%M:%S", &tmv);
    int n = snprintf(l, sizeof l, "%s\t%s\t%s\t%s\t%s\t%llu\n", ts, dir, file, peer, status, (unsigned long long)bytes);
    if (n >= (int)sizeof l) n = (int)sizeof l - 1;
    pthread_mutex_lock(&hl);
    int fd = open(hpath, O_WRONLY | O_CREAT | O_APPEND, 0644);
    if (fd >= 0) { if (fd_write_full(fd, l, (size_t)n) < 0) { /* ignore */ } close(fd); }
    pthread_mutex_unlock(&hl);
}

void history_print(int last_n)
{
    pthread_mutex_lock(&hl);
    FILE *f = fopen(hpath, "r");
    if (!f) { pthread_mutex_unlock(&hl); us_print("No transfer history yet.\n"); return; }
    char **lines = NULL; size_t cnt = 0, cap = 0; char *ln = NULL; size_t lc = 0;
    while (getline(&ln, &lc, f) > 0) {
        if (cnt == cap) { cap = cap ? cap * 2 : 64; lines = realloc(lines, cap * sizeof *lines); }
        lines[cnt++] = strdup(ln);
    }
    free(ln); fclose(f);
    pthread_mutex_unlock(&hl);

    us_print("%s%-19s  %-4s  %-28s  %-16s  %-12s  %s%s\n", C_BOLD, "Time", "Dir", "File", "Device", "Status", "Size", C_RST);
    us_print("%s--------------------------------------------------------------------------------------------%s\n", C_DIM, C_RST);
    size_t from = (last_n > 0 && (size_t)last_n < cnt) ? cnt - (size_t)last_n : 0;
    for (size_t i = from; i < cnt; i++) {
        char *f6[6] = {0}; char *sp, *s = lines[i]; int k = 0;
        for (char *t = strtok_r(s, "\t\n", &sp); t && k < 6; t = strtok_r(NULL, "\t\n", &sp)) f6[k++] = t;
        if (k < 6) continue;
        char sz[32]; fmt_size(strtoull(f6[5], NULL, 10), sz, sizeof sz);
        const char *sc = (!strcmp(f6[4], "SUCCESS") || !strcmp(f6[4], "UP-TO-DATE")) ? C_GRN :
                         (!strcmp(f6[4], "INTERRUPTED") || !strcmp(f6[4], "REJECTED")) ? C_YEL : C_RED;
        us_print("%-19s  %s%-4s%s  %-28.28s  %-16.16s  %s%-12s%s  %s\n", f6[0], !strcmp(f6[1], "SEND") ? C_CYN : C_MAG, f6[1], C_RST,
                 f6[2], f6[3], sc, f6[4], C_RST, sz);
    }
    for (size_t i = 0; i < cnt; i++) free(lines[i]);
    free(lines);
}
