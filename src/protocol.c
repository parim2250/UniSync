/*
 * protocol.c — handshake (PIN challenge/response + optional encryption) and message framing
 */
#include "common.h"
#include "util.h"
#include "ui.h"
#include "network.h"
#include "protocol.h"
#include "sha256.h"
#include "crypto.h"
#include "logger.h"
#include "history.h"

int proto_client_handshake(int fd, char *peername, size_t cap, int *encrypted)
{
    uint8_t rflags, nonce[16], mac[32], st;
    if (net_send(fd, UNISYNC_MAGIC, UNISYNC_MAGIC_LEN) < 0 || put_u8(fd, UNISYNC_VERSION) < 0 || put_str(fd, cfg.name) < 0) return -1;
    if (get_u8(fd, &rflags) < 0 || net_recv(fd, nonce, 16) < 0) return -1;

    int need_pin = rflags & 1;
    if (need_pin && !cfg.pin[0] && ui_ask_pin(cfg.pin, sizeof cfg.pin) < 0) { ui_err("A PIN is required but none was entered."); return -2; }
    int enc = cfg.encrypt || (rflags & 2);
    if (enc && !need_pin) { ui_err("Encryption needs a PIN. Start the receiver with:  UniSync receive --pin 1234"); return -2; }
    if (enc && !cfg.pin[0]) { ui_err("Encryption needs a PIN (use --pin)."); return -2; }

    const char *pin = need_pin ? cfg.pin : "";
    hmac_sha256((const uint8_t *)pin, strlen(pin), nonce, 16, mac);
    if (put_u8(fd, enc ? 1 : 0) < 0 || net_send(fd, mac, 32) < 0 || get_u8(fd, &st) < 0) return -1;
    if (st == 0) { ui_err("Access denied - wrong PIN."); return -2; }
    if (st == 2) { ui_err("This device only accepts encrypted transfers."); return -2; }
    if (get_str(fd, peername, cap) < 0) return -1;
    if (enc) {
        uint8_t key[32]; crypt_derive_key(cfg.pin, nonce, key);
        if (crypt_enable(fd, key, 1) < 0) return -1;
    }
    *encrypted = enc;
    return 0;
}

int proto_server_handshake(int fd, const char *ip, char *sender, size_t cap, int *encrypted)
{
    char magic[UNISYNC_MAGIC_LEN]; uint8_t ver;
    if (net_recv(fd, magic, UNISYNC_MAGIC_LEN) < 0 || memcmp(magic, UNISYNC_MAGIC, UNISYNC_MAGIC_LEN) != 0) return -1;
    if (get_u8(fd, &ver) < 0 || ver != UNISYNC_VERSION) return -1;
    if (get_str(fd, sender, cap) < 0) return -1;

    uint8_t nonce[16], mac[32], exp[32], sflags;
    int rf = open("/dev/urandom", O_RDONLY);
    if (rf < 0 || fd_read_full(rf, nonce, 16) < 0) { if (rf >= 0) close(rf); return -1; }
    close(rf);
    uint8_t rflags = (uint8_t)((cfg.pin[0] ? 1 : 0) | (cfg.encrypt ? 2 : 0));
    if (put_u8(fd, rflags) < 0 || net_send(fd, nonce, 16) < 0) return -1;
    if (get_u8(fd, &sflags) < 0 || net_recv(fd, mac, 32) < 0) return -1;

    hmac_sha256((const uint8_t *)cfg.pin, strlen(cfg.pin), nonce, 16, exp);
    uint8_t diff = 0; for (int i = 0; i < 32; i++) diff |= (uint8_t)(mac[i] ^ exp[i]);   /* constant time */
    if (diff || ((sflags & 1) && !cfg.pin[0])) {
        put_u8(fd, 0);
        ui_warn("Refused %s (%s): wrong PIN", sender, ip);
        log_error("auth failed from %s (%s)", sender, ip);
        history_add("RECV", "-", sender, "AUTH-FAILED", 0);
        return -1;
    }
    if (cfg.encrypt && !(sflags & 1)) {
        put_u8(fd, 2);
        ui_warn("Refused %s (%s): plaintext not allowed (receiver requires encryption)", sender, ip);
        log_error("plaintext transfer refused from %s", sender);
        return -1;
    }
    if (put_u8(fd, 1) < 0 || put_str(fd, cfg.name) < 0) return -1;
    if (sflags & 1) {
        uint8_t key[32]; crypt_derive_key(cfg.pin, nonce, key);
        if (crypt_enable(fd, key, 0) < 0) return -1;
    }
    *encrypted = sflags & 1;
    return 0;
}

int proto_send_offer(int fd, const Offer *o)
{
    return (put_u32(fd, o->nfiles) < 0 || put_u64(fd, o->total_bytes) < 0 || put_str(fd, o->label) < 0 ||
            put_u64(fd, o->transfer_id) < 0 || put_u32(fd, o->chunk_size) < 0) ? -1 : 0;
}
int proto_recv_offer(int fd, Offer *o)
{
    if (get_u32(fd, &o->nfiles) < 0 || get_u64(fd, &o->total_bytes) < 0 || get_str(fd, o->label, sizeof o->label) < 0 ||
        get_u64(fd, &o->transfer_id) < 0 || get_u32(fd, &o->chunk_size) < 0) return -1;
    return (o->chunk_size >= 4096 && o->chunk_size <= 4u * 1024 * 1024) ? 0 : -1;
}
int proto_send_decision(int fd, int accept) { return put_u8(fd, accept ? 1 : 0); }
int proto_recv_decision(int fd) { uint8_t v; return get_u8(fd, &v) < 0 ? -1 : (v ? 1 : 0); }

int proto_send_dir(int fd, const char *rel, uint32_t mode)
{ return (put_u8(fd, TAG_DIR) < 0 || put_str(fd, rel) < 0 || put_u32(fd, mode) < 0) ? -1 : 0; }
int proto_send_file_header(int fd, const FileEntryHeader *h)
{
    return (put_u8(fd, TAG_FILE) < 0 || put_str(fd, h->relpath) < 0 || put_str(fd, h->file_type) < 0 ||
            put_u64(fd, h->filesize) < 0 || put_u32(fd, h->mode) < 0 || net_send(fd, h->sha256, 32) < 0) ? -1 : 0;
}
int proto_send_end(int fd) { return put_u8(fd, TAG_END); }
int proto_recv_tag(int fd, uint8_t *tag) { return get_u8(fd, tag); }
int proto_recv_dir(int fd, char *rel, size_t cap, uint32_t *mode)
{ return (get_str(fd, rel, cap) < 0 || get_u32(fd, mode) < 0) ? -1 : 0; }
int proto_recv_file_header(int fd, FileEntryHeader *h)
{
    return (get_str(fd, h->relpath, sizeof h->relpath) < 0 || get_str(fd, h->file_type, sizeof h->file_type) < 0 ||
            get_u64(fd, &h->filesize) < 0 || get_u32(fd, &h->mode) < 0 || net_recv(fd, h->sha256, 32) < 0) ? -1 : 0;
}
