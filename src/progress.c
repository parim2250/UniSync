/*
 * progress.c — progress bar with percent, MB, speed and ETA
 */
#include "common.h"
#include "util.h"
#include "ui.h"
#include "progress.h"

#define BAR_WIDTH 28

static pthread_mutex_t plock = PTHREAD_MUTEX_INITIALIZER;
static int active = 0, next_id = 1;

void progress_init(ProgressTracker *t, const char *peer, int receiving, uint64_t total_bytes)
{
    memset(t, 0, sizeof *t);
    t->receiving = receiving; t->total_bytes = total_bytes;
    snprintf(t->peer, sizeof t->peer, "%s", peer);
    t->start = now_sec();
    pthread_mutex_lock(&plock); t->id = next_id++; active++; pthread_mutex_unlock(&plock);
}

void progress_file(ProgressTracker *t, const char *file) { snprintf(t->file, sizeof t->file, "%s", file); }
void progress_skip(ProgressTracker *t, uint64_t b)       { t->transferred_bytes += b; }
void progress_set_done(ProgressTracker *t, uint64_t d)   { t->transferred_bytes = d; }

static void render(const ProgressTracker *t, char *out, size_t cap)
{
    double el = now_sec() - t->start; if (el < 0.001) el = 0.001;
    double speed = (double)t->moved_bytes / el;
    int pct = t->total_bytes ? (int)(t->transferred_bytes * 100 / t->total_bytes) : 100;
    if (pct > 100) pct = 100;
    int filled = pct * BAR_WIDTH / 100;
    char bar[256] = "";
    for (int i = 0; i < BAR_WIDTH; i++) strcat(bar, i < filled ? "\xe2\x96\x88" : "\xe2\x96\x91");
    char d[32], tot[32], sp[32], eta[24] = "";
    fmt_size(t->transferred_bytes, d, sizeof d); fmt_size(t->total_bytes, tot, sizeof tot);
    fmt_size((uint64_t)speed, sp, sizeof sp);
    if (speed > 1 && t->total_bytes > t->transferred_bytes)
        snprintf(eta, sizeof eta, "ETA %ds", (int)((double)(t->total_bytes - t->transferred_bytes) / speed) + 1);
    snprintf(out, cap, "%s%s%s %s%s%s %3d%%  %s / %s  %s%s/s%s  %-8s %s%.24s%s",
             t->receiving ? C_MAG : C_CYN, t->receiving ? "\xe2\x86\x93" : "\xe2\x86\x91", C_RST,
             t->receiving ? C_MAG : C_CYN, bar, C_RST, pct, d, tot, C_GRN, sp, C_RST, eta, C_DIM, t->file, C_RST);
}

void progress_update(ProgressTracker *t, uint64_t n)
{
    t->transferred_bytes += n; t->moved_bytes += n;
    double now = now_sec();
    int inplace = g_color && active == 1;
    if (now - t->last_print < (inplace ? 0.1 : 1.0) && t->transferred_bytes < t->total_bytes) return;
    t->last_print = now;
    char l[700]; render(t, l, sizeof l);
    if (inplace) us_print("\r\033[K  %s", l); else ui_line("  [T%d] %s", t->id, l);
}

void progress_finish(ProgressTracker *t, const char *status, int ok)
{
    double el = now_sec() - t->start; if (el < 0.001) el = 0.001;
    char sz[32], sp[32];
    fmt_size(t->transferred_bytes, sz, sizeof sz);
    fmt_size((uint64_t)((double)t->moved_bytes / el), sp, sizeof sp);
    pthread_mutex_lock(&plock); active--; pthread_mutex_unlock(&plock);
    if (ok) ui_ok("%s %s - %s (%s in %.1fs, avg %s/s)", t->receiving ? "From" : "To", t->peer, status, sz, el, sp);
    else    ui_err("%s %s - %s", t->receiving ? "From" : "To", t->peer, status);
}
