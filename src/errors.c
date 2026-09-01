/*
 * errors.c — Error checks implementation
 */

#include <stdio.h>
#include <unistd.h>
#include <sys/stat.h>
#include <arpa/inet.h>
#include "errors.h"

const char* unisync_strerror(UniSyncError err)
{
    switch (err) {
        case UNISYNC_SUCCESS:       return "Success";
        case ERR_FILE_NOT_FOUND:   return "File does not exist";
        case ERR_PERMISSION_DENIED: return "Permission denied";
        case ERR_DIR_NOT_WRITABLE:  return "Directory does not exist or is not writable";
        case ERR_INVALID_IP:        return "Invalid IP address";
        case ERR_INVALID_PORT:      return "Invalid port number (must be 1024-65535)";
        default:                    return "Unknown error";
    }
}

int validate_file_readable(const char *filepath)
{
    if (access(filepath, F_OK) != 0) {
        return ERR_FILE_NOT_FOUND;
    }
    if (access(filepath, R_OK) != 0) {
        return ERR_PERMISSION_DENIED;
    }
    return UNISYNC_SUCCESS;
}

int validate_dir_writable(const char *dirpath)
{
    struct stat st;
    if (stat(dirpath, &st) != 0 || !S_ISDIR(st.st_mode)) {
        return ERR_DIR_NOT_WRITABLE;
    }
    if (access(dirpath, W_OK) != 0) {
        return ERR_PERMISSION_DENIED;
    }
    return UNISYNC_SUCCESS;
}