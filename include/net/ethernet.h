
#ifndef __NET_ETHERNET_H__
#define __NET_ETHERNET_H__

#include <sys/cdefs.h>

#define ETHER_ADDR_LEN		6
#define ETHER_TYPE_LEN		2
#define ETHER_CRC_LEN		4

#define ETHER_HDR_LEN		(2*ETHER_ADDR_LEN + ETHER_TYPE_LEN)

struct ether_header {
    uint8_t ether_dhost[ETHER_ADDR_LEN];
    uint8_t ether_shost[ETHER_ADDR_LEN];
    uint16_t ether_type;
} PACKED;

struct ether_addr {
    uint8_t octet[ETHER_ADDR_LEN];
} PACKED;

#define ETHERTYPE_IP		0x0800 /* IP */
#define ETHERTYPE_ARP		0x0806 /* ARP */
#define ETHERTYPE_REVARP	0x8036 /* Reverse ARP */
#define ETHERTYPE_IPV6		0x86DD /* IPv6 */

#endif /* __NET_ETHERNET_H__ */

