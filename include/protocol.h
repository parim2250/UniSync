/*
 * protocol.h — UniSync Wire Protocol Definition
 *
 * Defines the binary structure exchanged between sender and receiver
 * before any file data is transmitted.
 *
 *   Handshake order:
 *     1. Sender  -> Receiver : FileHeader (fixed size)
 *     2. Sender  -> Receiver : File payload (filesize bytes)
 */

#ifndef PROTOCOL_H
#define PROTOCOL_H

#include <stdint.h>
#include <sys/types.h>

/* ── Protocol Constants ────────────────────────────────── */

#define UNISYNC_MAGIC       "UNSY"   /* 4-byte magic identifier */
#define UNISYNC_MAGIC_LEN   4
#define UNISYNC_VERSION     1        /* Protocol version */
#define MAX_FILENAME_LEN    256      /* Max filename length in header */

/* ── File Header Structure ─────────────────────────────── */

/*
 * FileHeader — sent BEFORE file payload
 *
 *   magic[4]     : Protocol identifier "UNSY"
 *   version      : Protocol version number (currently 1)
 *   filename[256]: Original filename (null-terminated)
 *   filesize     : Size of the file in bytes (network byte order on wire)
 *
 * NOTE: The packed attribute forces the compiler to NOT add
 *       padding bytes — critical for network transmission.
 */
typedef struct __attribute__((packed)) {
    char     magic[UNISYNC_MAGIC_LEN];
    uint32_t version;
    char     filename[MAX_FILENAME_LEN];
    uint64_t filesize;
} FileHeader;

/* ── Function Declarations ─────────────────────────────── */

/*
 * build_file_header(header, filepath)
 *   Fills the given header struct with metadata from filepath.
 *   Returns 0 on success, -1 on error.
 */
int build_file_header(FileHeader *header, const char *filepath);

/*
 * send_file_header(sockfd, header)
 *   Serializes header (converts to network byte order) and sends it.
 *   Returns 0 on success, -1 on error.
 */
int send_file_header(int sockfd, const FileHeader *header);

/*
 * receive_file_header(sockfd, header)
 *   Reads a header from the socket and deserializes it (host byte order).
 *   Validates the magic number.
 *   Returns 0 on success, -1 on error.
 */
int receive_file_header(int sockfd, FileHeader *header);

/*
 * send_file_payload(sockfd, filepath, filesize)
 *   Sends exactly `filesize` bytes of `filepath` over the socket.
 *   Returns total bytes sent, or -1 on error.
 */
ssize_t send_file_payload(int sockfd, const char *filepath, uint64_t filesize);

/*
 * receive_file_payload(sockfd, output_path, filesize)
 *   Receives exactly `filesize` bytes and writes them to output_path.
 *   Returns total bytes written, or -1 on error.
 */
ssize_t receive_file_payload(int sockfd, const char *output_path, uint64_t filesize);

#endif /* PROTOCOL_H */