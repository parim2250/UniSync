/*
 * file_sender.c — "UniSync send": pick a device, send files/folders with progress, resume and retry
 */
#include "common.h"
#include "util.h"
#include "ui.h"
#include "app.h"
#include "logger.h"
#include "network.h"
#include "errors.h"
#include "protocol.h"
#include "transfer.h"
#include "progress.h"
#include "history.h"
#include "sha256.h"
#include "crypto.h"
#include "discovery.h"
#include <getopt.h>

static void on_signal(int s) { (void)s; g_running = 0; }

/* return: 0 ok, -1 network trouble (retry / resume), -2 permanent failure */
static int send_session(const char *ip, int port, EntryList *l, const Offer *o, ProgressTracker *pt,
                        int *drop, char *peername, size_t pcap, int attempt)
{
    int fd = tcp_connect(ip, port, 10), rc = -1, enc = 0;
    if (fd < 0) { ui_warn("Cannot connect to %s:%d (%s)", ip, port, strerror(errno)); return -1; }
    uint8_t *buf = malloc(o->chunk_size);
    if (!buf) { close(fd); return -2; }

    int hs = proto_client_handshake(fd, peername, pcap, &enc);
    if (hs < 0) { rc = hs; goto out; }
    if (proto_send_offer(fd, o) < 0) goto out;
    if (attempt == 1) ui_info("Waiting for %s to accept...", peername);
    set_timeout(fd, 300);                                    /* a human is answering y/n */
    int dec = proto_recv_decision(fd);
    if (dec < 0) goto out;
    if (!dec) { ui_err("%s rejected the transfer.", peername); rc = -2; goto out; }
    set_timeout(fd, 15);
    ui_ok("%s accepted%s - sending...", peername, enc ? " (encrypted \xf0\x9f\x94\x92)" : "");

    uint64_t sent = 0;
    for (size_t i = 0; i < l->n && g_running; i++) {
        Entry *e = &l->v[i];
        if (e->done || e->failed) continue;
        if (e->isdir) { if (proto_send_dir(fd, e->rel, e->mode) < 0) goto out; e->done = 1; continue; }

        if (access(e->abs, R_OK) != 0) {
            ui_warn("Cannot read %s (skipped)", e->abs); e->failed = 1;
            history_add("SEND", e->rel, peername, "FAILED", e->size); continue;
        }
        progress_file(pt, e->rel);
        FileEntryHeader h; memset(&h, 0, sizeof h);
        snprintf(h.relpath, sizeof h.relpath, "%s", e->rel);
        snprintf(h.file_type, sizeof h.file_type, "%s", file_type_of(e->rel));
        h.filesize = e->size; h.mode = e->mode; memcpy(h.sha256, e->hash, 32);
        if (proto_send_file_header(fd, &h) < 0) goto out;

        uint64_t offset;
        if (get_u64(fd, &offset) < 0) goto out;
        if (offset > e->size) { rc = -2; goto out; }
        progress_skip(pt, offset);
        int r = send_file_body(fd, e->abs, offset, e->size, buf, o->chunk_size, pt, drop, &sent);
        if (r == -1) { ui_line("%s! connection lost%s", C_YEL, C_RST); goto out; }
        if (r == -2) { ui_err("Could not read %s", e->abs); rc = -2; goto out; }
        uint8_t ok;
        if (get_u8(fd, &ok) < 0) goto out;
        if (!ok) { ui_err("%s was corrupted in transit - will retry", e->rel); history_add("SEND", e->rel, peername, "CORRUPTED", e->size); goto out; }
        e->done = 1;
        history_add("SEND", e->rel, peername, "SUCCESS", e->size);
    }
    if (proto_send_end(fd) < 0) goto out;
    rc = 0;
out:
    free(buf); crypt_disable(fd); close_socket(fd);
    return rc;
}

static int choose_device(char *ip, size_t ipcap, int *port)
{
    DeviceInfo devs[MAX_DEVICES];
    ui_info("Looking for UniSync devices...");
    int n = discover_devices(devs, MAX_DEVICES, 2);
    if (n <= 0) {
        ui_warn("No devices found. On the other computer run:  ./bin/UniSync receive");
        char in[64];
        if (ui_ask_line("  Or type its IP address (Enter to cancel): ", in, sizeof in) < 0 || !in[0]) return -1;
        *port = DEFAULT_PORT;
        char *c = strchr(in, ':'); if (c) { *c = 0; *port = atoi(c + 1); }
        snprintf(ip, ipcap, "%s", in);
        return 0;
    }
    if (n == 1) {
        us_print("\n  Found %s%s%s (%s:%d)%s\n", C_BOLD, devs[0].name, C_RST, devs[0].ip, devs[0].tcp_port, devs[0].needs_pin ? "  [PIN]" : "");
        snprintf(ip, ipcap, "%s", devs[0].ip); *port = devs[0].tcp_port;
        return 0;
    }
    us_print("\n%sFound %d devices:%s\n", C_BOLD, n, C_RST);
    for (int i = 0; i < n; i++)
        us_print("  %s[%d]%s %-20s %s:%d  %s%s%s\n", C_GRN, i + 1, C_RST, devs[i].name, devs[i].ip, devs[i].tcp_port,
                 C_YEL, devs[i].needs_pin ? "[PIN]" : "", C_RST);
    char in[16], prompt[64]; snprintf(prompt, sizeof prompt, "\n  Send to which device? [1-%d]: ", n);
    if (ui_ask_line(prompt, in, sizeof in) < 0) return -1;
    int c = atoi(in);
    if (c < 1 || c > n) { ui_err("Invalid choice."); return -1; }
    snprintf(ip, ipcap, "%s", devs[c - 1].ip); *port = devs[c - 1].tcp_port;
    return 0;
}

int run_send(int argc, char **argv)
{
    static struct option opts[] = {
        {"to", 1, 0, 't'}, {"pin", 1, 0, 'k'}, {"encrypt", 0, 0, 'e'}, {"name", 1, 0, 'n'},
        {"simulate-drop", 1, 0, 1000}, {0, 0, 0, 0}};
    char to[64] = ""; int drop = 0, o; optind = 1;
    while ((o = getopt_long(argc, argv, "t:k:en:", opts, NULL)) != -1) {
        switch (o) {
        case 't': snprintf(to, sizeof to, "%s", optarg); break;
        case 'k': snprintf(cfg.pin, sizeof cfg.pin, "%s", optarg); break;
        case 'e': cfg.encrypt = 1; break;
        case 'n': snprintf(cfg.name, sizeof cfg.name, "%s", optarg); break;
        case 1000: drop = atoi(optarg); break;
        default: return 1;
        }
    }
    if (optind >= argc) {
        us_print("Usage: UniSync send <file-or-folder>... [--to IP[:PORT]] [--pin PIN] [--encrypt]\n");
        return 1;
    }
    app_init();
    struct sigaction sa; memset(&sa, 0, sizeof sa);
    sa.sa_handler = on_signal; sigaction(SIGINT, &sa, NULL); sigaction(SIGTERM, &sa, NULL);
    signal(SIGPIPE, SIG_IGN);

    /* ---- gather what to send ---- */
    EntryList l = {0}; int np = argc - optind;
    for (int i = optind; i < argc; i++) {
        int err = validate_file_readable(argv[i]);
        if (err != UNISYNC_SUCCESS) { ui_err("%s: %s", argv[i], unisync_strerror(err)); entries_free(&l); return 1; }
        char abs[PATH_MAX], tmp[PATH_MAX];
        if (!realpath(argv[i], abs)) { ui_err("%s: %s", argv[i], strerror(errno)); entries_free(&l); return 1; }
        snprintf(tmp, sizeof tmp, "%s", abs);
        const char *base = strrchr(tmp, '/'); base = (base && base[1]) ? base + 1 : tmp;
        if (entries_collect(&l, abs, base) < 0) { entries_free(&l); return 1; }
    }
    uint32_t nfiles = 0; uint64_t total = 0;
    for (size_t i = 0; i < l.n; i++) if (!l.v[i].isdir) { nfiles++; total += l.v[i].size; }
    if (l.n == 0) { ui_err("Nothing to send."); entries_free(&l); return 1; }

    /* ---- pick the receiver ---- */
    char ip[64]; int port = DEFAULT_PORT;
    if (to[0]) {
        snprintf(ip, sizeof ip, "%s", to);
        char *c = strchr(ip, ':'); if (c) { *c = 0; port = atoi(c + 1); }
    } else if (choose_device(ip, sizeof ip, &port) < 0) { entries_free(&l); return 1; }
    struct in_addr ia;
    if (inet_pton(AF_INET, ip, &ia) != 1) { ui_err("%s: %s", ip, unisync_strerror(ERR_INVALID_IP)); entries_free(&l); return 1; }
    if (port < 1 || port > 65535) { ui_err("%s", unisync_strerror(ERR_INVALID_PORT)); entries_free(&l); return 1; }

    /* ---- checksums (SHA-256) ---- */
    char sz[32]; fmt_size(total, sz, sizeof sz);
    ui_info("Preparing %u file(s), %s - computing SHA-256 checksums...", nfiles, sz);
    for (size_t i = 0; i < l.n; i++) if (!l.v[i].isdir) {
        uint64_t s2;
        if (sha256_file(l.v[i].abs, l.v[i].hash, &s2) < 0) { ui_warn("cannot read %s", l.v[i].abs); l.v[i].failed = 1; }
        else l.v[i].size = s2;
    }

    Offer off; memset(&off, 0, sizeof off);
    off.nfiles = nfiles; off.total_bytes = total; off.chunk_size = CHUNK_SIZE;
    { int rf = open("/dev/urandom", O_RDONLY); if (rf < 0 || fd_read_full(rf, &off.transfer_id, 8) < 0) off.transfer_id = (uint64_t)time(NULL) ^ (uint64_t)getpid(); if (rf >= 0) close(rf); }
    if (np == 1) { const char *b = strrchr(argv[optind], '/'); b = (b && b[1]) ? b + 1 : argv[optind]; snprintf(off.label, sizeof off.label, "%s", b); }
    else snprintf(off.label, sizeof off.label, "%d items", np);

    char peername[96]; snprintf(peername, sizeof peername, "%s:%d", ip, port);
    ProgressTracker pt; progress_init(&pt, peername, 0, total);
    log_info("sending '%s' (%u files, %llu bytes) to %s:%d transfer-id %08llx", off.label, nfiles, (unsigned long long)total, ip, port, (unsigned long long)(off.transfer_id & 0xffffffffULL));

    int rc = -1;
    for (int attempt = 1; attempt <= MAX_RETRIES && g_running; attempt++) {
        uint64_t base = 0;
        for (size_t i = 0; i < l.n; i++) if (!l.v[i].isdir && l.v[i].done) base += l.v[i].size;
        progress_set_done(&pt, base);
        rc = send_session(ip, port, &l, &off, &pt, &drop, peername, sizeof peername, attempt);
        if (rc != -1) break;
        if (attempt < MAX_RETRIES) { ui_warn("Connection problem - retrying in 2s to RESUME (attempt %d/%d)...", attempt + 1, MAX_RETRIES); sleep(2); }
    }
    int skipped = 0; for (size_t i = 0; i < l.n; i++) if (!l.v[i].isdir && l.v[i].failed) skipped++;
    if (rc == 0 && !skipped) progress_finish(&pt, "completed - all files verified by the receiver", 1);
    else if (rc == 0)        progress_finish(&pt, "completed, some files were skipped", 0);
    else {
        progress_finish(&pt, rc == -2 ? "stopped" : "FAILED (gave up after retries)", 0);
        for (size_t i = 0; i < l.n; i++) if (!l.v[i].isdir && !l.v[i].done && !l.v[i].failed) { history_add("SEND", l.v[i].rel, peername, "FAILED", l.v[i].size); break; }
    }
    entries_free(&l);
    log_close();
    return (rc == 0 && !skipped) ? 0 : 1;
}
