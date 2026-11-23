/*
 * common.h — shared configuration, constants and terminal colours
 */
#ifndef COMMON_H
#define COMMON_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdarg.h>
#include <errno.h>
#include <time.h>
#include <limits.h>
#include <signal.h>
#include <unistd.h>
#include <fcntl.h>
#include <pthread.h>
#include <semaphore.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#define UNISYNC_VERSION_STR "2.0"
#define MAX_RETRIES         6     /* automatic reconnect attempts while sending */
#define ACCEPT_WAIT         60    /* seconds the receiver waits for a y/n answer */

/* Runtime settings (filled from the command line) */
typedef struct {
    char     name[64];            /* device name shown to others            */
    char     dest[PATH_MAX];      /* where received files are saved         */
    char     datadir[PATH_MAX];   /* ~/.unisync : history, log, paired list */
    char     pin[64];             /* empty = no PIN                         */
    char     hook[256];           /* program run after every received file  */
    int      tcp_port;
    int      encrypt;             /* 1 = encrypted transport (needs a PIN)  */
    int      autoaccept;
    int      max_transfers;       /* concurrent transfers per direction     */
    unsigned max_size_mb;         /* 0 = unlimited                          */
    uint32_t id;
} config_t;

extern config_t cfg;
extern volatile sig_atomic_t g_running;
extern int g_color;               /* 1 when stdout is a terminal */

#define CL(code) (g_color ? code : "")
#define C_RST  CL("\033[0m")
#define C_BOLD CL("\033[1m")
#define C_DIM  CL("\033[2m")
#define C_RED  CL("\033[31m")
#define C_GRN  CL("\033[32m")
#define C_YEL  CL("\033[33m")
#define C_BLU  CL("\033[34m")
#define C_MAG  CL("\033[35m")
#define C_CYN  CL("\033[36m")

#endif /* COMMON_H */
