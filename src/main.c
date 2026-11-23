/*
 * main.c — the UniSync command
 *
 *   UniSync receive            start a receiving device (the "server")
 *   UniSync send <files...>    send files/folders to a device
 *   UniSync devices            list devices on the network
 *   UniSync history            show past transfers
 *   UniSync paired / pair <name> / unpair <name> / block <name> / unblock <name>
 *
 * The old command names (UniSync-file-receiver, UniSync-file-sender, UniSync-discover)
 * are symlinks to this program and still work.
 */
#include "common.h"
#include "util.h"
#include "ui.h"
#include "app.h"
#include "logger.h"
#include "history.h"
#include "security.h"
#include "sha256.h"
#include "crypto.h"
#include "network.h"
#include <libgen.h>

config_t cfg;
volatile sig_atomic_t g_running = 1;
int g_color = 0;

void app_init(void)
{
    mkdir_p(cfg.datadir, 0755);
    char lp[PATH_MAX + 16]; snprintf(lp, sizeof lp, "%s/unisync.log", cfg.datadir);
    log_init(lp);
    history_init();
    security_init();
}

static void usage(void)
{
    ui_banner();
    us_print("%sQuick start%s\n", C_BOLD, C_RST);
    us_print("  %s1.%s On the receiving computer:   %s./bin/UniSync receive%s\n", C_GRN, C_RST, C_CYN, C_RST);
    us_print("  %s2.%s On the sending computer:     %s./bin/UniSync send notes.pdf%s\n", C_GRN, C_RST, C_CYN, C_RST);
    us_print("  %s3.%s Receiver answers:            %sy%s (accept)  /  %sn%s (reject)  /  %sa%s (always trust)\n\n", C_GRN, C_RST, C_GRN, C_RST, C_RED, C_RST, C_CYN, C_RST);
    us_print("%sCommands%s\n", C_BOLD, C_RST);
    const char *rows[][2] = {
        {"receive",            "start receiving   [--dir FOLDER] [--pin 1234] [--encrypt] [--auto-accept] [--port N]"},
        {"send <path>...",     "send files/folders [--to IP[:PORT]] [--pin 1234] [--encrypt]"},
        {"devices",            "list UniSync devices on the network"},
        {"history [n]",        "show the last n transfers (default 20)"},
        {"paired",             "show paired (trusted) and blocked devices"},
        {"pair|unpair <name>", "trust / forget a device (skips the y/n question)"},
        {"block|unblock <name>", "always refuse / allow a device"},
        {"selftest",           "check SHA-256 and ChaCha20 against official test vectors"},
    };
    for (size_t i = 0; i < sizeof rows / sizeof rows[0]; i++) us_print("  %s%-22s%s %s\n", C_GRN, rows[i][0], C_RST, rows[i][1]);
    us_print("\n%sExamples%s\n  ./bin/UniSync receive --dir ~/Downloads --pin 4321\n  ./bin/UniSync send report.pdf photos/ --pin 4321 --encrypt\n  ./bin/UniSync send big.iso --to 192.168.1.20\n", C_BOLD, C_RST);
}

int main(int argc, char **argv)
{
    const char *home = getenv("HOME"); if (!home) home = "/tmp";
    memset(&cfg, 0, sizeof cfg);
    gethostname(cfg.name, sizeof cfg.name - 1);
    if (!cfg.name[0]) strcpy(cfg.name, "unisync-device");
    snprintf(cfg.dest, sizeof cfg.dest, "%s/UniSync_Received", home);
    snprintf(cfg.datadir, sizeof cfg.datadir, "%s/.unisync", home);
    cfg.tcp_port = DEFAULT_PORT; cfg.max_transfers = 4;
    { int rf = open("/dev/urandom", O_RDONLY); if (rf < 0 || fd_read_full(rf, &cfg.id, sizeof cfg.id) < 0) cfg.id = (uint32_t)getpid(); if (rf >= 0) close(rf); }
    g_color = isatty(STDOUT_FILENO) && !getenv("NO_COLOR");

    /* old command names -> new commands (symlinks to this binary) */
    char self[PATH_MAX]; snprintf(self, sizeof self, "%s", argv[0]);
    const char *me = basename(self);
    char *args[64]; int n = 0;
    if (!strcmp(me, "UniSync-file-receiver")) {          /* [port] [dir] */
        args[n++] = "receive";
        if (argc > 1) { args[n++] = "--port"; args[n++] = argv[1]; }
        if (argc > 2) { args[n++] = "--dir";  args[n++] = argv[2]; }
    } else if (!strcmp(me, "UniSync-file-sender")) {     /* <file> [ip [port]] */
        char to[64] = "";
        args[n++] = "send";
        if (argc > 1) args[n++] = argv[1];
        if (argc > 2) { snprintf(to, sizeof to, "%s%s%s", argv[2], argc > 3 ? ":" : "", argc > 3 ? argv[3] : ""); args[n++] = "--to"; args[n++] = strdup(to); }
    } else if (!strcmp(me, "UniSync-discover")) {
        args[n++] = "devices";
    } else {
        for (int i = 1; i < argc && n < 63; i++) args[n++] = argv[i];
    }
    args[n] = NULL;

    if (n == 0 || !strcmp(args[0], "help") || !strcmp(args[0], "--help") || !strcmp(args[0], "-h")) { usage(); return 0; }
    const char *cmd = args[0];

    if (!strcmp(cmd, "receive") || !strcmp(cmd, "server") || !strcmp(cmd, "start")) return run_receive(n, args);
    if (!strcmp(cmd, "send"))     return run_send(n, args);
    if (!strcmp(cmd, "devices") || !strcmp(cmd, "discover") || !strcmp(cmd, "scan")) return run_discover();
    if (!strcmp(cmd, "version") || !strcmp(cmd, "--version")) { printf("UniSync %s\n", UNISYNC_VERSION_STR); return 0; }

    if (!strcmp(cmd, "selftest")) {
        uint8_t h[32]; char hx[65]; sha256_ctx c; sha256_init(&c); sha256_update(&c, (const uint8_t *)"abc", 3); sha256_final(&c, h); hex_encode(h, 32, hx);
        int a = !strcmp(hx, "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad"), b = crypt_selftest();
        printf("SHA-256 (FIPS 180-4 'abc'):     %s\nChaCha20 (RFC 8439 sec 2.4.2): %s\n", a ? "PASS" : "FAIL", b ? "PASS" : "FAIL");
        return !(a && b);
    }
    if (!strcmp(cmd, "hash") && n > 1) {
        uint8_t h[32]; char hx[65];
        if (sha256_file(args[1], h, NULL) < 0) { perror(args[1]); return 1; }
        hex_encode(h, 32, hx); printf("%s  %s\n", hx, args[1]); return 0;
    }

    app_init();
    if (!strcmp(cmd, "history")) { history_print(n > 1 ? atoi(args[1]) : 20); return 0; }
    if (!strcmp(cmd, "paired") || !strcmp(cmd, "acl")) { security_print(); return 0; }
    if (n > 1 && !strcmp(cmd, "pair"))    { pair_device(args[1]);    ui_ok("Paired with %s - their transfers are accepted automatically.", args[1]); return 0; }
    if (n > 1 && !strcmp(cmd, "unpair"))  { unpair_device(args[1]);  ui_ok("Unpaired %s.", args[1]); return 0; }
    if (n > 1 && !strcmp(cmd, "block"))   { block_device(args[1]);   ui_ok("Blocked %s.", args[1]); return 0; }
    if (n > 1 && !strcmp(cmd, "unblock")) { unblock_device(args[1]); ui_ok("Unblocked %s.", args[1]); return 0; }

    ui_err("Unknown command '%s'.", cmd);
    usage();
    return 1;
}
