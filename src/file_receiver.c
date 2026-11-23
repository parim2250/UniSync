/*
 * file_receiver.c — "UniSync receive": start the receiving device
 */
#include "common.h"
#include "util.h"
#include "ui.h"
#include "app.h"
#include "logger.h"
#include "network.h"
#include "errors.h"
#include "handler.h"
#include "discovery.h"
#include <getopt.h>
#include <ifaddrs.h>
#include <net/if.h>
#include <sys/time.h>

static void on_signal(int s) { (void)s; g_running = 0; }

static void print_my_ips(void)
{
    struct ifaddrs *ifa, *p;
    if (getifaddrs(&ifa) != 0) return;
    for (p = ifa; p; p = p->ifa_next) {
        if (!p->ifa_addr || p->ifa_addr->sa_family != AF_INET || (p->ifa_flags & IFF_LOOPBACK)) continue;
        char ip[16]; inet_ntop(AF_INET, &((struct sockaddr_in *)p->ifa_addr)->sin_addr, ip, sizeof ip);
        us_print("  %sYour IP%s      : %s\n", C_DIM, C_RST, ip);
    }
    freeifaddrs(ifa);
}

int run_receive(int argc, char **argv)
{
    static struct option opts[] = {
        {"dir", 1, 0, 'd'}, {"port", 1, 0, 'p'}, {"pin", 1, 0, 'k'}, {"encrypt", 0, 0, 'e'},
        {"auto-accept", 0, 0, 'a'}, {"name", 1, 0, 'n'}, {"max-size", 1, 0, 'm'}, {"exec", 1, 0, 1001},
        {"max-transfers", 1, 0, 1002}, {0, 0, 0, 0}};
    int o; optind = 1;
    while ((o = getopt_long(argc, argv, "d:p:k:ean:m:", opts, NULL)) != -1) {
        switch (o) {
        case 'd': snprintf(cfg.dest, sizeof cfg.dest, "%s", optarg); break;
        case 'p': cfg.tcp_port = atoi(optarg); break;
        case 'k': snprintf(cfg.pin, sizeof cfg.pin, "%s", optarg); break;
        case 'e': cfg.encrypt = 1; break;
        case 'a': cfg.autoaccept = 1; break;
        case 'n': snprintf(cfg.name, sizeof cfg.name, "%s", optarg); break;
        case 'm': cfg.max_size_mb = (unsigned)atoi(optarg); break;
        case 1001: snprintf(cfg.hook, sizeof cfg.hook, "%s", optarg); break;
        case 1002: cfg.max_transfers = atoi(optarg) > 0 ? atoi(optarg) : 4; break;
        default: return 1;
        }
    }
    if (cfg.tcp_port < 1 || cfg.tcp_port > 65535) { ui_err("%s", unisync_strerror(ERR_INVALID_PORT)); return 1; }
    if (cfg.encrypt && !cfg.pin[0]) { ui_err("Encryption needs a PIN:  UniSync receive --pin 1234 --encrypt"); return 1; }
    mkdir_p(cfg.dest, 0755);
    if (validate_dir_writable(cfg.dest) != UNISYNC_SUCCESS) { ui_err("Cannot write to %s", cfg.dest); return 1; }

    app_init();
    int server_fd = tcp_listen(cfg.tcp_port);
    if (server_fd < 0) {
        ui_err("Cannot listen on port %d: %s", cfg.tcp_port, strerror(errno));
        ui_info("Another receiver is probably running. Close it, or use:  UniSync receive --port %d", cfg.tcp_port + 1);
        return 1;
    }
    struct timeval tv = { 1, 0 }; setsockopt(server_fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof tv);
    handler_init(cfg.max_transfers);

    struct sigaction sa; memset(&sa, 0, sizeof sa);
    sa.sa_handler = on_signal; sigaction(SIGINT, &sa, NULL); sigaction(SIGTERM, &sa, NULL);
    signal(SIGPIPE, SIG_IGN);

    ui_banner();
    us_print("  %sDevice name%s  : %s%s%s\n", C_DIM, C_RST, C_BOLD, cfg.name, C_RST);
    print_my_ips();
    us_print("  %sListening on%s : TCP port %d\n", C_DIM, C_RST, cfg.tcp_port);
    us_print("  %sSaving to%s    : %s\n", C_DIM, C_RST, cfg.dest);
    us_print("  %sSecurity%s     : PIN %s, encryption %s, %s\n\n", C_DIM, C_RST, cfg.pin[0] ? "ON" : "off",
             cfg.encrypt ? "REQUIRED" : (cfg.pin[0] ? "optional" : "off"),
             cfg.autoaccept ? "auto-accept ON" : "you approve every transfer (y/n)");
    if (discovery_responder_start(cfg.tcp_port, cfg.pin[0] != 0, cfg.encrypt) < 0)
        ui_warn("Device discovery is off (UDP port %d busy). Senders can still connect by IP.", DISCOVERY_PORT);
    log_info("receiver started: name=%s port=%d dest=%s", cfg.name, cfg.tcp_port, cfg.dest);
    us_print("  %sWaiting for files...  (Ctrl+C to stop)%s\n", C_GRN, C_RST);

    while (g_running) {
        struct sockaddr_in caddr; socklen_t clen = sizeof caddr;
        int cfd = accept(server_fd, (struct sockaddr *)&caddr, &clen);
        if (cfd < 0) continue;                              /* timeout or Ctrl+C */
        ClientContext *ctx = malloc(sizeof *ctx);
        if (!ctx) { close_socket(cfd); continue; }
        ctx->client_fd = cfd;
        snprintf(ctx->save_dir, sizeof ctx->save_dir, "%s", cfg.dest);
        inet_ntop(AF_INET, &caddr.sin_addr, ctx->client_ip, sizeof ctx->client_ip);
        pthread_t tid;
        if (pthread_create(&tid, NULL, handle_client, ctx) != 0) { close_socket(cfd); free(ctx); continue; }
        pthread_detach(tid);
    }
    discovery_responder_stop();
    close_socket(server_fd);
    log_close();
    us_print("\nBye.\n");
    return 0;
}
