#ifndef __arp_h
#define __arp_h

#include <stdio.h>
#include <stdbool.h>
#include <sys/socket.h>
#include <net/ethernet.h>
#include <netinet/ip.h>
#include <linux/if_packet.h>
#include <linux/if_ether.h>
#include <linux/if_arp.h>
#include "unp.h"

#define ARP_PROTOCOL_ID     61173
#define ARP_ID_CODE         14508
#define ARP_REQ             1
#define ARP_REP             2

#define AREQ_TIMEOUT        3

#define IP_ALEN     4
#define ETH_ALEN    6

#define ETHHDR_LEN  14
#define ARPHDR_LEN  10

#define ARP_FRAME_LEN   44

#define ARP_PATH    "/tmp/14508-61173-arpService"
#define TMP_PATH    "/tmp/14508-61173-tourApplication-XXXXXX"

#define IF_NAME             16
#define IF_HADDR            6
#define IP_ALIAS            1

typedef unsigned char   BITFIELD8;
typedef unsigned char   uchar;
typedef unsigned short  ushort;
typedef unsigned int    uint;
typedef unsigned long   ulong;

// Interface table entry
// Modified hardware address information
//   * interface: eth0
struct hwa_info {
    char    if_name[IF_NAME];       /* interface name, null terminated      */
    uchar   if_haddr[IF_HADDR];     /* hardware address                     */
    int     if_index;               /* interface index                      */
    short   ip_alias;               /* 1 if hwa_addr is an alias IP address */
    //struct  sockaddr  *ip_addr;     /* IP address                           */
    uchar   ip_addr[IP_ALEN];       /* IP address                           */
    struct  hwa_info  *hwa_next;    /* next of these structures             */
};

typedef struct ethhdr_t {
    uchar   h_dest[ETH_ALEN];       /* destination eth addr */
    uchar   h_source[ETH_ALEN];     /* source ether addr    */
    ushort  h_proto;                /* packet type ID field */
}__attribute__((packed)) ethhdr;

typedef struct arphdr_t {
    ushort  ar_id;          /* our own ARP identification # */
    ushort  ar_hrd;         /* format of hardware address   */
    ushort  ar_pro;         /* format of protocol address   */
    uchar   ar_hln;         /* length of hardware address   */
    uchar   ar_pln;         /* length of protocol address   */
    ushort  ar_op;          /* ARP opcode (command)         */
}__attribute__((packed)) arphdr;

typedef struct arppayload_t {
    uchar   ar_shrd[ETH_ALEN];  /* sender hardware address */
    uchar   ar_spro[IP_ALEN];   /* sender protocol address */
    uchar   ar_thrd[ETH_ALEN];  /* target hardware address */
    uchar   ar_tpro[IP_ALEN];   /* target protocol address */
} arppayload;

typedef struct arp_cache_t {
    uchar   ipaddr[IP_ALEN];    /* IP address       */
    uchar   hwaddr[ETH_ALEN];   /* Hardware address */
    int     ifindex;            /* Interface number */
    ushort  hatype;             /* Hardware type    */
    int     sockfd;             /* Connected client */
    struct arp_cache_t *next;   /* next pointer     */
} arp_cache;

typedef struct arp_object_t {
    struct hwa_info *hwa_info;
    int         pfSockfd;   /* PF_PACKET socket     */
    int         doSockfd;   /* UNIX Domain socket   */
    int         if_index;   /* Main interface idnex */
    arp_cache   *cache;     /* ARP Cache            */
} arp_object;

struct hwaddr {
    int     sll_ifindex;    /* Interface number         */
    ushort  sll_hatype;     /* Hardware type            */
    uchar   sll_halen;      /* Length of address        */
    uchar   sll_addr[8];    /* Physical layer address   */
};

struct hwa_info *Get_hw_addrs();
char *UtilIpToString(const uchar *);

#endif
