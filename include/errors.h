/*
 * errors.h — Central error handling & system validations
 */

#ifndef ERRORS_H
#define ERRORS_H

typedef enum {
    UNISYNC_SUCCESS = 0,
    ERR_FILE_NOT_FOUND,
    ERR_PERMISSION_DENIED,
    ERR_DIR_NOT_WRITABLE,
    ERR_INVALID_IP,
    ERR_INVALID_PORT
} UniSyncError;

/* Returns human-readable error message */
const char* unisync_strerror(UniSyncError err);

/* Validates file exists and is readable */
int validate_file_readable(const char *filepath);

/* Validates output directory exists and is writable */
int validate_dir_writable(const char *dirpath);

#endif /* ERRORS_H */