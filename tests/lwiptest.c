
#include <unistd.h>

#include <lwip/init.h>
#include <lwip/ip.h>
#include <lwip/sockets.h>
#include <netif/etharp.h>

static struct netif netif;

extern err_t ethernetif_init(struct netif *netif);

int
main(int argc, const char *argv[])
{
    static ip_addr_t addr, mask, gw;

    IP4_ADDR(&addr, 192, 168, 1, 2);
    IP4_ADDR(&mask, 255, 255, 255, 0);
    IP4_ADDR(&gw, 0, 0, 0, 0);

    lwip_init();

    netif_add(&netif, &addr, &mask, &gw, NULL, ethernetif_init, ethernet_input);
    netif_set_default(&netif);
    netif_set_up(&netif);

    lwip_socket(AF_INET, SOCK_STREAM, IP_PROTO_TCP);

    while (1) { sleep(0); }

    return 0;
}

