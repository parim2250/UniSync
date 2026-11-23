#include "common.h"
#include "util.h"
#include "security.h"
#include <strings.h>

#define MAX_ACL 64
static char paired[MAX_ACL][64], blocked[MAX_ACL][64];
static struct { char sender[64], label[256]; time_t t; } resume_ok[MAX_ACL];
static pthread_mutex_t sl = PTHREAD_MUTEX_INITIALIZER;

static int in_list(char l[][64], const char *n)
{ for (int i = 0; i < MAX_ACL; i++) if (l[i][0] && !strcasecmp(l[i], n)) return 1; return 0; }

static void save_list(char l[][64], const char *file)
{
    char p[PATH_MAX + 32]; snprintf(p, sizeof p, "%s/%s", cfg.datadir, file);
    mkdir_p(cfg.datadir, 0755);
    FILE *f = fopen(p, "w"); if (!f) return;
    for (int i = 0; i < MAX_ACL; i++) if (l[i][0]) fprintf(f, "%s\n", l[i]);
    fclose(f);
}

static void load_list(char l[][64], const char *file)
{
    char p[PATH_MAX + 32], line[128]; snprintf(p, sizeof p, "%s/%s", cfg.datadir, file);
    FILE *f = fopen(p, "r"); if (!f) return;
    int n = 0;
    while (n < MAX_ACL && fgets(line, sizeof line, f)) { line[strcspn(line, "\r\n")] = 0; if (line[0]) snprintf(l[n++], 64, "%.63s", line); }
    fclose(f);
}

static void add(char l[][64], const char *file, const char *n)
{
    pthread_mutex_lock(&sl);
    if (!in_list(l, n)) for (int i = 0; i < MAX_ACL; i++) if (!l[i][0]) { snprintf(l[i], 64, "%s", n); break; }
    save_list(l, file);
    pthread_mutex_unlock(&sl);
}
static void del(char l[][64], const char *file, const char *n)
{
    pthread_mutex_lock(&sl);
    for (int i = 0; i < MAX_ACL; i++) if (!strcasecmp(l[i], n)) l[i][0] = 0;
    save_list(l, file);
    pthread_mutex_unlock(&sl);
}

void security_init(void) { load_list(paired, "paired.txt"); load_list(blocked, "blocked.txt"); }
int  is_paired(const char *n)  { pthread_mutex_lock(&sl); int r = in_list(paired, n);  pthread_mutex_unlock(&sl); return r; }
int  is_blocked(const char *n) { pthread_mutex_lock(&sl); int r = in_list(blocked, n); pthread_mutex_unlock(&sl); return r; }
void pair_device(const char *n)    { del(blocked, "blocked.txt", n); add(paired, "paired.txt", n); }
void unpair_device(const char *n)  { del(paired, "paired.txt", n); }
void block_device(const char *n)   { del(paired, "paired.txt", n); add(blocked, "blocked.txt", n); }
void unblock_device(const char *n) { del(blocked, "blocked.txt", n); }

void security_print(void)
{
    int a = 0, b = 0;
    us_print("%sPaired devices%s (accepted automatically):\n", C_BOLD, C_RST);
    for (int i = 0; i < MAX_ACL; i++) if (paired[i][0]) { us_print("  %s\xe2\x9c\x94%s %s\n", C_GRN, C_RST, paired[i]); a++; }
    if (!a) us_print("  %s(none - answer 'a' at an Accept? prompt, or run: UniSync pair <name>)%s\n", C_DIM, C_RST);
    us_print("%sBlocked devices%s:\n", C_BOLD, C_RST);
    for (int i = 0; i < MAX_ACL; i++) if (blocked[i][0]) { us_print("  %s\xe2\x9c\x98%s %s\n", C_RED, C_RST, blocked[i]); b++; }
    if (!b) us_print("  %s(none)%s\n", C_DIM, C_RST);
}

void remember_resume(const char *sender, const char *label)
{
    pthread_mutex_lock(&sl);
    int s = 0;
    for (int i = 0; i < MAX_ACL; i++) { if (!resume_ok[i].t) { s = i; break; } if (resume_ok[i].t < resume_ok[s].t) s = i; }
    snprintf(resume_ok[s].sender, 64, "%s", sender); snprintf(resume_ok[s].label, 256, "%s", label);
    resume_ok[s].t = time(NULL);
    pthread_mutex_unlock(&sl);
}

int resume_match(const char *sender, const char *label)
{
    int ok = 0; time_t now = time(NULL);
    pthread_mutex_lock(&sl);
    for (int i = 0; i < MAX_ACL; i++)
        if (resume_ok[i].t && now - resume_ok[i].t < 600 && !strcmp(resume_ok[i].sender, sender) && !strcmp(resume_ok[i].label, label)) ok = 1;
    pthread_mutex_unlock(&sl);
    return ok;
}
