/*
 * protocol.h — UniSync Wire Protocol Definition (Layer 9: Permissions)
 */

#ifndef PROTOCOL_H
#define PROTOCOL_H

#include <stdint.h>
#include <sys/types.h>

#define UNISYNC_MAGIC       "UNSY"
#define UNISYNC_MAGIC_LEN   4
#define UNISYNC_VERSION     1
#define MAX_FILENAME_LEN    256

typedef struct __attribute__((packed)) {
    char     magic[UNISYNC_MAGIC_LEN];
    uint32_t version;
    char     filename[MAX_FILENAME_LEN];
    uint64_t filesize;
    uint32_t mode;  /* Layer 9: POSIX file mode / permissions */
} FileHeader;

int build_file_header(FileHeader *header, const char *filepath);
int send_file_header(int sockfd, const FileHeader *header);
int receive_file_header(int sockfd, FileHeader *header);

ssize_t send_file_payload(int sockfd, const char *filepath, uint64_t filesize);

/* Updated: takes file mode to apply via chmod() after receive */
ssize_t receive_file_payload(int sockfd, const char *output_path, uint64_t filesize, uint32_t mode);

#endif /* PROTOCOL_H */