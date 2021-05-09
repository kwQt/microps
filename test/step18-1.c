#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <unistd.h>

#include "driver/ether_tap.h"
#include "driver/loopback.h"
#include "icmp.h"
#include "ip.h"
#include "net.h"
#include "test.h"
#include "udp.h"
#include "util.h"

static volatile sig_atomic_t terminate;

static void
on_signal(int s)
{
    (void)s;
    terminate = 1;
    net_interrupt();
}

static int
setup(void)
{
    struct net_device *dev;
    struct ip_iface *iface;

    if (net_init() == -1) {
        errorf("net_init() failure");
        return -1;
    }
    dev = loopback_init();
    if (!dev) {
        errorf("loopback_init() failure");
        return -1;
    }
    iface = ip_iface_alloc(LOOPBACK_IP_ADDR, LOOPBACK_NETMASK);
    if (!iface) {
        errorf("ip_iface_alloc() failure");
        return -1;
    }
    if (ip_iface_register(dev, iface) == -1) {
        errorf("ip_iface_register() failure");
        return -1;
    }
    dev = ether_tap_init(ETHER_TAP_NAME, ETHER_TAP_HW_ADDR);
    if (!dev) {
        errorf("ether_tap_init() failure");
        return -1;
    }
    iface = ip_iface_alloc(ETHER_TAP_IP_ADDR, ETHER_TAP_NETMASK);
    if (!iface) {
        errorf("ip_iface_alloc() failure");
        return -1;
    }
    if (ip_iface_register(dev, iface) == -1) {
        errorf("ip_iface_register() failure");
        return -1;
    }
    if (ip_route_set_default_gateway(iface, DEFAULT_GATEWAY) == -1) {
        errorf("ip_route_set_default_gateway() failure");
        return -1;
    }
    if (net_run() == -1) {
        errorf("net_run() failure");
        return -1;
    }
    return 0;
}

static void
cleanup(void)
{
    net_shutdown();
}

int
main(int argc, char *argv[])
{
    int soc;
    struct udp_endpoint local, foreign;
    uint8_t buf[1024];
    char ep[UDP_ENDPOINT_STR_LEN];
    ssize_t ret;

    signal(SIGINT, on_signal);
    if (setup() == -1) {
        errorf("setup() failure");
        return -1;
    }
    soc = udp_open();
    if (soc == -1) {
        errorf("udp_open() failure");
        return -1;
    }
    udp_endpoint_pton("0.0.0.0:7", &local);
    if (udp_bind(soc, &local) == -1) {
        errorf("udp_bind() failure");
        return -1;
    }
    debugf("waiting for data...");
    while (!terminate) {
        ret = udp_recvfrom(soc, buf, sizeof(buf), &foreign);
        if (ret <= 0) {
            break;
        }
        infof("%zu bytes data form %s", ret, udp_endpoint_ntop(&foreign, ep, sizeof(ep)));
        hexdump(stderr, buf, ret);
        if (udp_sendto(soc, buf, ret, &foreign) == -1) {
            errorf("udp_sendto() failure");
            break;
        }
    }
    udp_close(soc);
    cleanup();
    return 0;
}