/*
 * handler.c — receiver side of one connection (runs in its own thread)
 *   handshake -> offer -> Accept/Reject (y/n/a) -> files with resume + SHA-256 verification
 */
#include "common.h"
#include "util.h"
#include "ui.h"
#include "logger.h"
#include "network.h"
#include "protocol.h"
#include "transfer.h"
#include "progress.h"
#include "history.h"
#include "security.h"
#include "crypto.h"
#include "handler.h"

static sem_t slots;
void handler_init(int max_concurrent) { sem_init(&slots, 0, (unsigned)max_concurrent); }

void *handle_client(void *arg)
{
    ClientContext *ctx = arg;
    int fd = ctx->client_fd, enc = 0, got_slot = 0, rc = -1, failed_files = 0, have_pt = 0;
    char sender[64] = "unknown", cur[4096] = "-";
    uint64_t cur_size = 0;
    Offer o; ProgressTracker pt; uint8_t *buf = NULL;

    set_timeout(fd, 60);                                   /* sender may be typing a PIN */
    if (proto_server_handshake(fd, ctx->client_ip, sender, sizeof sender, &enc) < 0) goto end;
    if (proto_recv_offer(fd, &o) < 0) goto end;
    log_info("Incoming from %s (%s): '%s' %u files %llu bytes", sender, ctx->client_ip, o.label, o.nfiles, (unsigned long long)o.total_bytes);

    /* ---------- Accept / Reject ---------- */
    int accept = 0;
    if (is_blocked(sender)) ui_warn("Blocked device %s tried to send '%s'", sender, o.label);
    else if (cfg.max_size_mb && o.total_bytes > ((uint64_t)cfg.max_size_mb << 20)) {
        char s[32]; fmt_size(o.total_bytes, s, sizeof s);
        ui_warn("Rejected '%s' from %s: %s is over the %u MB limit", o.label, sender, s, cfg.max_size_mb);
    }
    else if (cfg.autoaccept)                        { accept = 1; ui_info("Auto-accepting '%s' from %s", o.label, sender); }
    else if (is_paired(sender))                     { accept = 1; ui_info("Paired device %s - accepting '%s'", sender, o.label); }
    else if (resume_match(sender, o.label))         { accept = 1; ui_info("%s reconnected - resuming '%s'", sender, o.label); }
    else {
        int a = ui_ask_accept(sender, ctx->client_ip, o.label, o.nfiles, o.total_bytes, cfg.pin[0] != 0, enc);
        if (a == 2) { pair_device(sender); ui_ok("Paired with %s - future transfers are accepted automatically", sender); }
        accept = a != 0;
    }
    if (accept) {
        struct timespec ts; clock_gettime(CLOCK_REALTIME, &ts); ts.tv_sec += 30;
        if (sem_timedwait(&slots, &ts) < 0) accept = 0; else got_slot = 1;
    }
    if (proto_send_decision(fd, accept) < 0) goto end;
    if (!accept) { history_add("RECV", o.label, sender, "REJECTED", o.total_bytes); ui_line("%s\xe2\x9c\x98%s Rejected '%s' from %s", C_RED, C_RST, o.label, sender); rc = 0; goto end; }

    set_timeout(fd, 20);
    ui_info("Receiving '%s' from %s%s ...", o.label, sender, enc ? " (encrypted)" : "");
    progress_init(&pt, sender, 1, o.total_bytes); have_pt = 1;
    buf = malloc(o.chunk_size);
    if (!buf) goto end;

    for (;;) {
        uint8_t t;
        if (proto_recv_tag(fd, &t) < 0) goto end;
        if (t == TAG_END) { rc = 0; break; }

        char path[PATH_MAX];
        if (t == TAG_DIR) {
            char rel[4096]; uint32_t mode;
            if (proto_recv_dir(fd, rel, sizeof rel, &mode) < 0 || !safe_relpath(rel)) goto end;
            if (snprintf(path, sizeof path, "%s/%s", ctx->save_dir, rel) >= (int)sizeof path) goto end;
            if (mkdir_p(path, 0755) < 0) goto end;
            continue;
        }
        if (t != TAG_FILE) goto end;

        FileEntryHeader h;
        if (proto_recv_file_header(fd, &h) < 0) goto end;
        if (!safe_relpath(h.relpath)) { log_error("unsafe path from %s", sender); goto end; }
        if (snprintf(path, sizeof path, "%s/%s", ctx->save_dir, h.relpath) >= (int)sizeof path) goto end;
        snprintf(cur, sizeof cur, "%s", h.relpath); cur_size = h.filesize;
        progress_file(&pt, h.relpath);
        { char dp[PATH_MAX]; snprintf(dp, sizeof dp, "%s", path); char *sl = strrchr(dp, '/'); if (sl) { *sl = 0; mkdir_p(dp, 0755); } }

        int already = 0;
        uint64_t offset = resume_offset(path, h.filesize, h.sha256, &already);
        if (offset && !already) ui_info("Resuming %s from %llu bytes", h.relpath, (unsigned long long)offset);
        if (put_u64(fd, offset) < 0) goto end;
        progress_skip(&pt, offset);

        int ok = 1;
        if (!already) {
            ok = receive_file_body(fd, path, h.filesize, offset, h.mode, h.sha256, buf, o.chunk_size, &pt);
            if (ok < 0) goto end;                              /* network lost -> partial file kept */
        }
        if (put_u8(fd, (uint8_t)ok) < 0) goto end;
        if (ok) {
            history_add("RECV", h.relpath, sender, already ? "UP-TO-DATE" : "SUCCESS", h.filesize);
            log_info("received %s [type %s] %llu bytes from %s, SHA-256 verified", h.relpath, h.file_type, (unsigned long long)h.filesize, sender);
            if (!already) run_hook(path);
        } else {
            failed_files++;
            history_add("RECV", h.relpath, sender, "CORRUPTED", h.filesize);
            ui_err("Integrity check FAILED for %s (file discarded)", h.relpath);
            log_error("hash mismatch for %s from %s", h.relpath, sender);
        }
    }

end:
    if (buf) free(buf);
    if (got_slot) sem_post(&slots);
    if (have_pt) {
        if (rc == 0 && !failed_files) { progress_finish(&pt, "all files verified (SHA-256)", 1); ui_info("Saved in %s", ctx->save_dir); }
        else if (rc == 0) progress_finish(&pt, "finished, but some files were corrupted", 0);
        else {
            progress_finish(&pt, "connection lost - partial data kept, sender can resume", 0);
            history_add("RECV", cur, sender, "INTERRUPTED", cur_size);
            remember_resume(sender, o.label);
            log_error("transfer from %s interrupted at '%s'", sender, cur);
        }
    }
    crypt_disable(fd);
    close_socket(fd);
    free(ctx);
    return NULL;
}
