#include <stdio.h>
#include "discovery.h"

int main(void)
{
    DeviceInfo devices[MAX_DEVICES];

    printf("Scanning LAN for UniSync devices (3 seconds)...\n\n");
    int n = discover_devices(devices, MAX_DEVICES, 3);

    if (n <= 0) {
        printf("No devices found.\n");
        return 0;
    }

    printf("Found %d device(s):\n", n);
    printf("----------------------------------------\n");
    for (int i = 0; i < n; i++) {
        printf("  [%d] %-20s  %s:%d\n", i+1,
               devices[i].name, devices[i].ip, devices[i].tcp_port);
    }
    printf("----------------------------------------\n");
    return 0;
}