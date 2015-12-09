#ifndef __tour_h
#define __tour_h

#include <stdio.h>
#include <stdbool.h>
#include <sys/socket.h>
#include <net/ethernet.h>
#include <netinet/ip.h>
#include <linux/if_packet.h>
#include <linux/if_ether.h>
#include <linux/if_arp.h>
#include "unp.h"

#define TOUR_PROTOCOL_ID    222
#define TOUR_ID_CODE        14508
#define ARP_PROTOCOL_ID     14508
#define ARP_ID_CODE         61375

#define MCAST_ADDRESS       {0xee, 0x5c, 0x53, 0x12}
#define MCAST_PORT          7518

#define IPADDR_BUFFSIZE     4
#define HWADDR_BUFFSIZE     6
#define HOSTNAME_BUFFSIZE   10
#define PATHNAME_BUFFSIZE   108
#define IPSTR_BUFFSIZE      16
#define TIMESTR_BUFFSIZE    60
#define PACKET_BUFFSIZE     1024
#define MCAST_BUFFSIZE      100
#define PING_BUFFSIZE       10

#define IP4_HDRLEN          20  // IPv4 header length
#define TOUR_HDRLEN         8   // TOUR header length, excludes data

#define NODE_SEQ(__node_seq, __index) ((__node_seq) + (HOSTNAME_BUFFSIZE * (__index)))
#define IP_SEQ(__ip_seq, __index) ((__ip_seq) + (IPADDR_BUFFSIZE * (__index)))

typedef unsigned char   BITFIELD8;
typedef unsigned char   uchar;
typedef unsigned short  ushort;
typedef unsigned int    uint;
typedef unsigned long   ulong;

struct hwaddr {
    int     sll_ifindex;    /* Interface number         */
    ushort  sll_hatype;     /* Hardware type            */
    uchar   sll_halen;      /* Length of address        */
    uchar   sll_addr[8];    /* Physical layer address   */
};

// TOUR IP packet payload
typedef struct tourhdr_t {
    uchar   grp[4];     /* Multicast group address  */
    ushort  port;       /* Multicast port number    */
    uchar   seqLength;  /* Tour sequence length     */
    uchar   index;      /* Current node index       */
  //uchar   seq[seqLength*4]; This is payload
} tourhdr;

// Main TOUR information object
typedef struct tour_object_t {
    uchar   ipaddr[IPADDR_BUFFSIZE];        /* IP address           */
    char    hostname[HOSTNAME_BUFFSIZE];    /* Host name            */
    int     rtSockfd;                       /* tour IP raw socket   */
    int     pgSockfd;                       /* ping IP raw socket   */
    int     pfSockfd;                       /* PF_PACKET socket     */
    int     msSockfd;                       /* Mcast send socket    */
    int     mrSockfd;                       /* Mcast recv socket    */
    int     seqLength;                      /* Sequence length      */
    char    *nodeSeq;                       /* pointer to node seq  */
    char    *ipSeq;                         /* pointer to ip seq    */
    uchar   mcastAddr[IPADDR_BUFFSIZE];     /* Multicast group addr */
    int     mcastPort;                      /* Multicast port #     */
} tour_object;


char *UtilIpToString(const uchar *);

#endif
