#include "common.h"
#include "util.h"
#include "network.h"
#include "sha256.h"
#include "logger.h"
#include "transfer.h"
#include <dirent.h>
#include <sys/wait.h>

/* ====================== sender side ====================== */
static Entry *entry_new(EntryList *l)
{
    if (l->n == l->cap) {
        size_t nc = l->cap ? l->cap * 2 : 32;
        Entry *nv = realloc(l->v, nc * sizeof(Entry));
        if (!nv) return NULL;
        l->v = nv; l->cap = nc;
    }
    Entry *e = &l->v[l->n++]; memset(e, 0, sizeof *e); return e;
}

void entries_free(EntryList *l) { free(l->v); l->v = NULL; l->n = l->cap = 0; }

const char *file_type_of(const char *name)
{
    const char *b = strrchr(name, '/'); b = b ? b + 1 : name;
    const char *d = strrchr(b, '.');
    return (d && d[1] && strlen(d + 1) < 16) ? d + 1 : "file";
}

int entries_collect(EntryList *l, const char *abs, const char *rel)
{
    struct stat st;
    if (lstat(abs, &st) < 0) { us_print("  skipping %s: %s\n", abs, strerror(errno)); return 0; }
    if (S_ISLNK(st.st_mode)) { us_print("  skipping symlink %s\n", abs); return 0; }
    if (S_ISDIR(st.st_mode)) {
        Entry *e = entry_new(l); if (!e) return -1;
        snprintf(e->abs, sizeof e->abs, "%s", abs); snprintf(e->rel, sizeof e->rel, "%s", rel);
        e->isdir = 1; e->mode = st.st_mode & 0777;
        DIR *d = opendir(abs);
        if (!d) { us_print("  cannot open folder %s: %s\n", abs, strerror(errno)); return 0; }
        struct dirent *de;
        while ((de = readdir(d))) {
            if (!strcmp(de->d_name, ".") || !strcmp(de->d_name, "..")) continue;
            size_t al = strlen(de->d_name);
            if (al >= 13 && (!strcmp(de->d_name + al - 13, ".unisync.part") || !strcmp(de->d_name + al - 13, ".unisync.meta"))) continue;
            char a2[PATH_MAX], r2[4096];
            if (snprintf(a2, sizeof a2, "%s/%s", abs, de->d_name) >= (int)sizeof a2) continue;
            if (snprintf(r2, sizeof r2, "%s/%s", rel, de->d_name) >= (int)sizeof r2) continue;
            if (entries_collect(l, a2, r2) < 0) { closedir(d); return -1; }
        }
        closedir(d);
        return 0;
    }
    if (S_ISREG(st.st_mode)) {
        Entry *e = entry_new(l); if (!e) return -1;
        snprintf(e->abs, sizeof e->abs, "%s", abs); snprintf(e->rel, sizeof e->rel, "%s", rel);
        e->size = (uint64_t)st.st_size; e->mode = st.st_mode & 0777;
        return 0;
    }
    us_print("  skipping special file %s\n", abs);
    return 0;
}

int send_file_body(int fd, const char *abs, uint64_t offset, uint64_t size, uint8_t *buf, size_t chunk,
                   ProgressTracker *pt, int *drop, uint64_t *sent)
{
    int ff = open(abs, O_RDONLY);
    if (ff < 0) return -2;
    if (lseek(ff, (off_t)offset, SEEK_SET) < 0) { close(ff); return -2; }
    uint64_t remaining = size - offset;
    while (remaining) {
        size_t n = remaining > chunk ? chunk : (size_t)remaining;
        if (fd_read_full(ff, buf, n) < 0) { close(ff); return -2; }           /* file shrank while sending */
        if (net_send(fd, buf, n) < 0) { close(ff); return -1; }
        remaining -= n; *sent += n; progress_update(pt, n);
        if (*drop > 0 && *sent >= (uint64_t)*drop) { *drop = 0; close(ff); return -1; }
    }
    close(ff);
    return 0;
}

/* ====================== receiver side ====================== */
int safe_relpath(const char *r)
{
    if (!*r || r[0] == '/') return 0;
    const char *p = r;
    while (*p) {
        const char *e = strchr(p, '/'); size_t n = e ? (size_t)(e - p) : strlen(p);
        if (n == 0 || (n == 2 && !strncmp(p, "..", 2)) || (n == 1 && p[0] == '.')) return 0;
        if (!e) break;
        p = e + 1;
    }
    return 1;
}

static int meta_matches(const char *meta, uint64_t size, const char *hex)
{
    FILE *f = fopen(meta, "r"); if (!f) return 0;
    unsigned long long s; char h[80];
    int ok = (fscanf(f, "%llu %64s", &s, h) == 2 && s == size && !strcmp(h, hex));
    fclose(f); return ok;
}

uint64_t resume_offset(const char *path, uint64_t size, const uint8_t hash[32], int *already)
{
    char part[PATH_MAX + 16], meta[PATH_MAX + 16], hex[65];
    snprintf(part, sizeof part, "%s.unisync.part", path);
    snprintf(meta, sizeof meta, "%s.unisync.meta", path);
    hex_encode(hash, 32, hex);
    *already = 0;
    struct stat st; uint8_t h2[32]; uint64_t sz2;
    if (stat(path, &st) == 0 && S_ISREG(st.st_mode) && (uint64_t)st.st_size == size &&
        sha256_file(path, h2, &sz2) == 0 && !memcmp(h2, hash, 32)) { *already = 1; return size; }   /* identical file already here */
    if (meta_matches(meta, size, hex) && stat(part, &st) == 0 && (uint64_t)st.st_size <= size)
        return (uint64_t)st.st_size;                                                                /* partial data on disk */
    FILE *mf = fopen(meta, "w");
    if (mf) { fprintf(mf, "%llu %s\n", (unsigned long long)size, hex); fclose(mf); }
    return 0;
}

int receive_file_body(int fd, const char *path, uint64_t size, uint64_t offset, uint32_t mode,
                      const uint8_t hash[32], uint8_t *buf, size_t chunk, ProgressTracker *pt)
{
    char part[PATH_MAX + 16], meta[PATH_MAX + 16];
    snprintf(part, sizeof part, "%s.unisync.part", path);
    snprintf(meta, sizeof meta, "%s.unisync.meta", path);
    int fo = open(part, O_WRONLY | O_CREAT, 0644);
    if (fo < 0 || ftruncate(fo, (off_t)offset) < 0 || lseek(fo, (off_t)offset, SEEK_SET) < 0) {
        log_error("open %s: %s", part, strerror(errno)); if (fo >= 0) close(fo); return -1;
    }
    uint64_t remaining = size - offset;
    while (remaining) {
        size_t n = remaining > chunk ? chunk : (size_t)remaining;
        if (net_recv(fd, buf, n) < 0) { close(fo); return -1; }               /* .part stays -> resumable */
        if (fd_write_full(fo, buf, n) < 0) { log_error("write %s: %s", part, strerror(errno)); close(fo); return -1; }
        remaining -= n; progress_update(pt, n);
    }
    fsync(fo); close(fo);

    uint8_t h2[32]; uint64_t sz2;                                              /* integrity check */
    int ok = (sha256_file(part, h2, &sz2) == 0 && sz2 == size && !memcmp(h2, hash, 32));
    if (ok) {
        if (rename(part, path) < 0) ok = 0; else chmod(path, (mode & 0777) | 0600);
        unlink(meta);
    } else { unlink(part); unlink(meta); }
    return ok;
}

/* Process management: fork + exec a user program with the new file as argument, then waitpid. */
void run_hook(const char *path)
{
    if (!cfg.hook[0]) return;
    pid_t p = fork();
    if (p == 0) { execlp(cfg.hook, cfg.hook, path, (char *)NULL); _exit(127); }
    if (p > 0) { int st = 0; waitpid(p, &st, 0); log_info("hook '%s' finished (exit %d) for %s", cfg.hook, WIFEXITED(st) ? WEXITSTATUS(st) : -1, path); }
}
