#ifndef LWIPOPTS_H
#define LWIPOPTS_H

// Pico W with cyw43 + lwIP in NO_SYS=1 mode (threadsafe background arch).
#define NO_SYS                      1
#define LWIP_RAW                    1
#define LWIP_TCP                    1
#define LWIP_DNS                    1
#define LWIP_DHCP                   1
#define LWIP_IPV4                   1
#define LWIP_IPV6                   0
#define LWIP_NETIF_HOSTNAME         1
#define LWIP_NETIF_STATUS_CALLBACK  1

// Sockets/netconn are disabled in NO_SYS mode for this firmware.
#define LWIP_SOCKET                 0
#define LWIP_NETCONN                0

#endif