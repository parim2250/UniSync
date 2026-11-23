/*
 * protocol.h — UniSync wire protocol v2
 *
 *  S->R  "UNSY", u8 version, str senderName
 *  R->S  u8 flags (bit0 PIN required, bit1 encryption required), 16-byte nonce
 *  S->R  u8 encrypt?, 32-byte HMAC-SHA256(PIN, nonce)          (the PIN is never sent)
 *  R->S  u8 status (1 ok, 0 bad PIN, 2 encryption required), str receiverName
 *        --- if encrypting, both sides switch to ChaCha20 here ---
 *  S->R  Offer: u32 nfiles, u64 totalBytes, str label, u64 transferID, u32 chunkSize
 *  R->S  u8 accept (1) / reject (0)
 *  per item:  'D' str relpath u32 mode
 *             'F' str relpath, str fileType, u64 size, u32 mode, 32-byte SHA-256
 *                 R->S u64 offset      (bytes already on disk -> RESUME point)
 *                 S->R raw bytes [offset, size)
 *                 R->S u8 verified     (receiver re-hashes the file and compares)
 *  S->R  'E'
 */
#ifndef PROTOCOL_H
#define PROTOCOL_H

#include <stdint.h>
#include <stddef.h>

#define UNISYNC_MAGIC       "UNSY"
#define UNISYNC_MAGIC_LEN   4
#define UNISYNC_VERSION     2
#define MAX_FILENAME_LEN    4096

#define TAG_FILE 'F'
#define TAG_DIR  'D'
#define TAG_END  'E'

typedef struct {
    uint64_t transfer_id, total_bytes;
    uint32_t nfiles, chunk_size;
    char     label[256];
} Offer;

typedef struct {
    char     relpath[MAX_FILENAME_LEN];
    char     file_type[64];
    uint64_t filesize;
    uint32_t mode;                    /* POSIX permissions, restored with chmod() */
    uint8_t  sha256[32];
} FileEntryHeader;

/* Return values (client side): 0 ok, -1 network problem (can retry), -2 refused / permanent */
int proto_client_handshake(int fd, char *peername, size_t cap, int *encrypted);
/* Server side: 0 ok (sender name filled in), -1 failed/refused */
int proto_server_handshake(int fd, const char *ip, char *sender, size_t cap, int *encrypted);

int proto_send_offer(int fd, const Offer *o);
int proto_recv_offer(int fd, Offer *o);
int proto_send_decision(int fd, int accept);
int proto_recv_decision(int fd);                 /* 1 accept, 0 reject, -1 network error */

int proto_send_dir(int fd, const char *rel, uint32_t mode);
int proto_send_file_header(int fd, const FileEntryHeader *h);
int proto_send_end(int fd);
int proto_recv_tag(int fd, uint8_t *tag);
int proto_recv_dir(int fd, char *rel, size_t cap, uint32_t *mode);
int proto_recv_file_header(int fd, FileEntryHeader *h);

#endif /* PROTOCOL_H */
