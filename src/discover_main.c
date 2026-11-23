/*
 * discover_main.c — "UniSync devices": list UniSync receivers on the network
 */
#include "common.h"
#include "util.h"
#include "ui.h"
#include "app.h"
#include "discovery.h"

int run_discover(void)
{
    DeviceInfo devices[MAX_DEVICES];
    ui_info("Scanning the network for UniSync devices (3 seconds)...");
    int n = discover_devices(devices, MAX_DEVICES, 3);
    if (n <= 0) {
        ui_warn("No devices found. On the other computer run:  ./bin/UniSync receive");
        return 0;
    }
    us_print("\n%sFound %d device(s):%s\n", C_BOLD, n, C_RST);
    for (int i = 0; i < n; i++)
        us_print("  %s[%d]%s %-20s %s:%d  %s%s%s\n", C_GRN, i + 1, C_RST, devices[i].name, devices[i].ip, devices[i].tcp_port,
                 C_YEL, devices[i].needs_pin ? (devices[i].encrypted ? "[PIN + encrypted]" : "[PIN]") : "", C_RST);
    return 0;
}
