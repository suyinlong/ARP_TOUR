/**
* @file         :  ping.c
* @author       :  Jiewen Zheng
* @date         :  2015-12-6
* @brief        :  ping implementation
* @changelog    :
**/

#include "ping.h"

#include <pthread.h>
#include <netinet/ip_icmp.h>
#include <linux/if.h>   // struct ifreq

#define ETHHDR_LEN          14
#define ICMP_HDRLEN         8
#define ICMP_DATALEN        (sizeof(struct timeval))
#define ICMP_FRAME_LEN      (ETHHDR_LEN + IP4_HDRLEN + ICMP_HDRLEN + ICMP_DATALEN)

// Computing the internet checksum (RFC 1071).
// Note that the internet checksum does not preclude collisions.
uint16_t CheckSum(uint16_t *addr, int len)
{
    int count = len;
    register uint32_t sum = 0;
    uint16_t answer = 0;

    // Sum up 2-byte values until none or only one byte left.
    while (count > 1) {
        sum += *(addr++);
        count -= 2;
    }

    // Add left-over byte, if any.
    if (count > 0) {
        sum += *(uint8_t *)addr;
    }

    // Fold 32-bit sum into 16 bits; we lose information by doing this,
    // increasing the chances of a collision.
    // sum = (lower 16 bits) + (upper 16 bits shifted right 16 bits)
    while (sum >> 16) {
        sum = (sum & 0xffff) + (sum >> 16);
    }

    // Checksum is one's compliment of sum.
    answer = ~sum;

    return (answer);
}

// Build IPv4 ICMP pseudo-header and call checksum function.
uint16_t Icmp4CheckSum(struct icmp icmphdr, uint8_t *payload, int payloadlen)
{
    char buf[IP_MAXPACKET];
    char *ptr;
    int chksumlen = 0;
    int i;

    ptr = &buf[0];  // ptr points to beginning of buffer buf

    // Copy Message Type to buf (8 bits)
    memcpy(ptr, &icmphdr.icmp_type, sizeof(icmphdr.icmp_type));
    ptr += sizeof(icmphdr.icmp_type);
    chksumlen += sizeof(icmphdr.icmp_type);

    // Copy Message Code to buf (8 bits)
    memcpy(ptr, &icmphdr.icmp_code, sizeof(icmphdr.icmp_code));
    ptr += sizeof(icmphdr.icmp_code);
    chksumlen += sizeof(icmphdr.icmp_code);

    // Copy ICMP checksum to buf (16 bits)
    // Zero, since we don't know it yet
    *ptr = 0; ptr++;
    *ptr = 0; ptr++;
    chksumlen += 2;

    // Copy Identifier to buf (16 bits)
    memcpy(ptr, &icmphdr.icmp_id, sizeof(icmphdr.icmp_id));
    ptr += sizeof(icmphdr.icmp_id);
    chksumlen += sizeof(icmphdr.icmp_id);

    // Copy Sequence Number to buf (16 bits)
    memcpy(ptr, &icmphdr.icmp_seq, sizeof(icmphdr.icmp_seq));
    ptr += sizeof(icmphdr.icmp_seq);
    chksumlen += sizeof(icmphdr.icmp_seq);

    // Copy payload to buf
    memcpy(ptr, payload, payloadlen);
    ptr += payloadlen;
    chksumlen += payloadlen;

    // Pad to the next 16-bit boundary
    for (i = 0; i < payloadlen % 2; i++, ptr++) {
        *ptr = 0;
        ptr++;
        chksumlen++;
    }

    return CheckSum((uint16_t *)buf, chksumlen);
}

// get local mac address
int GetLocalMacAddr(int fd, struct hwaddr *hw)
{
    memset(hw, 0, sizeof(*hw));
    struct ifreq ifr;
    char interface[32] = { 0 };
    strcpy(interface, "eth0");
    // Use ioctl() to look up interface name and get its MAC address.
    memset(&ifr, 0, sizeof(ifr));
    snprintf(ifr.ifr_name, sizeof(ifr.ifr_name), "%s", interface);
    if (ioctl(fd, SIOCGIFHWADDR, &ifr) < 0)
    {
        perror("ioctl() failed to get source MAC address ");
        return -1;
    }

    // Copy source MAC address.
    memcpy(hw->sll_addr, ifr.ifr_hwaddr.sa_data, 6);

    if ((hw->sll_ifindex = if_nametoindex(interface)) == 0)
    {
        perror("if_nametoindex() failed to obtain interface index ");
        return -1;
    }

    return 0;
}

// build eth header
void BuildEthHdr(uchar *eth, const uchar *srcMac, const uchar *dstMac, ushort proto)
{
    // Destination and Source MAC addresses
    memcpy(eth, dstMac, 6);
    memcpy(eth + 6, srcMac, 6);
    proto = htons(proto);
    memcpy(eth + 12, &proto, sizeof(proto));
}

// build ip header
void BuildIpHdr(struct ip *iphdr, const uchar *src, const uchar *dst)
{
    memset(iphdr, 0, sizeof(*iphdr));
    int iplen = IPADDR_BUFFSIZE;
    // IPv4 header
    iphdr->ip_hl = 5;
    iphdr->ip_v = 4;
    iphdr->ip_tos = 0;
    iphdr->ip_len = htons(IP4_HDRLEN + ICMP_HDRLEN + ICMP_DATALEN);
    iphdr->ip_id = htons(TOUR_ID_CODE);
    iphdr->ip_off = htons(0);
    iphdr->ip_ttl = 255;
    iphdr->ip_p = IPPROTO_ICMP;
    iphdr->ip_sum = 0;
    memcpy(&iphdr->ip_src, src, IPADDR_BUFFSIZE);
    memcpy(&iphdr->ip_dst, dst, IPADDR_BUFFSIZE);
    iphdr->ip_sum = CheckSum((uint16_t *)iphdr, IP4_HDRLEN);
}

// build ICMP frame
void BuildIcmpFrame(struct icmp *icmp, int seq)
{
    pid_t pid = getpid() & 0xffff;
    memset(icmp, 0, sizeof(*icmp));
    icmp->icmp_type = ICMP_ECHO;
    icmp->icmp_code = 0;
    icmp->icmp_id = pid;
    icmp->icmp_seq = htons(seq);
    memset(icmp->icmp_data, 0, ICMP_DATALEN);
    //strcpy(icmp->icmp_data, "hello");
    Gettimeofday((struct timeval *)icmp->icmp_data, NULL);

    int len = 8 + ICMP_DATALEN; // checksum ICMP header and data
    icmp->icmp_cksum = 0;
    icmp->icmp_cksum = in_cksum((uint16_t *)icmp, len);
    //icmp->icmp_cksum = icmp4_checksum(*icmp, data, ICMP_DATALEN);
}

// Send ICMP data frame
int SendIcmpFrame(int sockfd, int if_index, const uchar *src, void *frame, int framelen)
{
    struct sockaddr_ll sll;
    memset(&sll, 0, sizeof(sll));

    sll.sll_family = AF_PACKET;
    sll.sll_protocol = htons(ETH_P_IP);
    sll.sll_ifindex = if_index;
    sll.sll_hatype = 0;
    sll.sll_halen = ETH_ALEN;
    memcpy(sll.sll_addr, src, 6);

    return sendto(sockfd, frame, framelen, 0, (struct sockaddr *)&sll, sizeof(sll));
}

// Send ICMP echo request
int SendIcmpRequestMsg(tour_object *obj, const uchar *dstIpAddr, const uchar *dstMacAddr, int seq)
{
    uchar frame[ICMP_FRAME_LEN] = { 0 };

    // get local mac address
    struct hwaddr hwAddr;
    GetLocalMacAddr(obj->pfSockfd, &hwAddr);

    // build eth header
    BuildEthHdr(frame, hwAddr.sll_addr, dstMacAddr, ETH_P_IP);

    // build ip header
    struct ip *ipHdr = (struct ip *)(frame + ETHHDR_LEN);
    BuildIpHdr(ipHdr, obj->ipaddr, dstIpAddr);

    // build ICMP frame
    struct icmp *icmpHdr = (struct icmp *)(frame + ETHHDR_LEN + IP4_HDRLEN);
    BuildIcmpFrame(icmpHdr, seq);

    int n = SendIcmpFrame(obj->pfSockfd, hwAddr.sll_ifindex, dstMacAddr, frame, ICMP_FRAME_LEN);
    if (n < 0)
    {
        printf("[PING] Send frame error(%d) %s\n", errno, strerror(errno));
        return -1;
    }

    /*
    struct in_addr dstIn;
    memcpy(&dstIn, dstIpAddr, IPADDR_BUFFSIZE);
    char *ip = inet_ntoa(dstIn);
    char mac[32] = { 0 };
    sprintf(mac, "%.2x:%.2x:%.2x:%.2x:%.2x:%.2x",
        dstMacAddr[0], dstMacAddr[1], dstMacAddr[2], dstMacAddr[3], dstMacAddr[4], dstMacAddr[5]);
    printf("[PING] Send ICMP request to vm node ok, ip=%s mac=%s interface=%d bytes=%d\n",
        ip, mac, hwAddr.sll_ifindex, n);
    */

    return 0;
}

// Receive ICMP echo reply
int RecvIcmpReplyMsg(tour_object *obj)
{
    char buffer[PACKET_BUFFSIZE] = {0};
    struct sockaddr_in fromAddr;
    int fromLen = sizeof(fromAddr);
    int n = Recvfrom(obj->pgSockfd, buffer, PACKET_BUFFSIZE, 0, (struct sockaddr *)&fromAddr, &fromLen);
    if (n == 0)
        //return -1;
        return RecvIcmpReplyMsg(obj);

    // get peer ip address
    char *fromIp = inet_ntoa(fromAddr.sin_addr);

    char *p = buffer;
    // get ip frame
    struct ip *iphdr = (struct ip *)p;
    if (iphdr->ip_p != IPPROTO_ICMP)
        //return -2;  // not ICMP
        return RecvIcmpReplyMsg(obj);

    // get icmp frame
    p += IP4_HDRLEN;
    struct icmp *icmp = (struct icmp *)p;
    int icmpLen = n - IP4_HDRLEN;
    if (icmpLen < 8)
        //return -3;  // malformed packet
        return RecvIcmpReplyMsg(obj);

    if (icmp->icmp_type == ICMP_ECHOREPLY)
    {
        pid_t pid = getpid() & 0xffff;
        if (icmp->icmp_id != pid)
            //return -4;  // not a response to our ECHO_REQUEST
            return RecvIcmpReplyMsg(obj);

        if (icmpLen < 16)
            //return -5;  // not enough data to use
            return RecvIcmpReplyMsg(obj);

        struct timeval	*tvsend = NULL;
        struct timeval tvrecv;
        Gettimeofday(&tvrecv, NULL);
        tvsend = (struct timeval *)icmp->icmp_data;
        tv_sub(&tvrecv, tvsend);
        double rtt = tvrecv.tv_sec * 1000.0 + tvrecv.tv_usec / 1000.0;

        printf("[PING] %d bytes from %s: seq=%u, ttl=%d, rtt=%.3f ms\n",
            icmpLen, fromIp, ntohs(icmp->icmp_seq), iphdr->ip_ttl, rtt);
        return 0;
    }
    else {
        // If receive REQ, need to discard and recv again!
        return RecvIcmpReplyMsg(obj);
    }

}

// Send work thread
void *SendThread(void *arg)
{
    pthread_t tid = pthread_self();
    Pthread_detach(tid);

    char *p = (char *)arg;
    tour_object obj;
    uchar dstIpAddr[IPADDR_BUFFSIZE] = { 0 };
    uchar dstMacAddr[8] = { 0 };

    memcpy(&obj, p, sizeof(obj));
    p += sizeof(obj);
    memcpy(dstIpAddr, p, IPADDR_BUFFSIZE);
    p += IPADDR_BUFFSIZE;
    memcpy(dstMacAddr, p, 6);

    char ip[INET_ADDRSTRLEN] = { 0 };
    inet_ntop(AF_INET, dstIpAddr, ip, INET_ADDRSTRLEN);
    int dataLen = ICMP_FRAME_LEN;
    printf("[PING] %s (%.2x:%.2x:%.2x:%.2x:%.2x:%.2x): %d data bytes\n",
        ip, dstMacAddr[0], dstMacAddr[1], dstMacAddr[2], dstMacAddr[3], dstMacAddr[4], dstMacAddr[5], dataLen);
    int ret, count = 0;
    while (1)
    {
        if (count >= 4)
            break;
        // send a icmp echo request
        if (SendIcmpRequestMsg(&obj, dstIpAddr, dstMacAddr, count) < 0)
        {
            printf("[PING] Send ICMP echo request error: %s\n", strerror(errno));
            break;
        }
        count++;
        // receive the icmp echo reply
        ret = RecvIcmpReplyMsg(&obj);
        if (ret == -1)
        {
            printf("[PING] Receive ICMP echo reply error: %s\n", strerror(errno));
            break;
        }
        sleep(1);
    }

    free(arg);
}

// Create a sending thread
int CreateSendThread(void *context, void * (*func)(void *))
{
    int ret = 0;
    pthread_t tid = 0;
    pthread_attr_t att;

    ret = pthread_create(&tid, NULL, func, context);
    if (ret != 0)
    {
        printf("[PING] Create thread error, %s.\n", strerror(errno));
        return -1;
    }

    //printf("Create thread ok, fd:%d tid:%d.\n", fd, tid);

    return 0;
}

void Ping(tour_object *obj, const struct sockaddr_in *dstIp, const struct hwaddr *dstHw)
{
    uchar dstIpAddr[IPADDR_BUFFSIZE] = { 0 };
    memcpy(dstIpAddr, &dstIp->sin_addr, IPADDR_BUFFSIZE);

    int len = sizeof(*obj) + IPADDR_BUFFSIZE + 6;
    char *context = (char *)malloc(len);
    char *p = context;
    memcpy(p, obj, sizeof(*obj));
    p += sizeof(*obj);
    memcpy(p, dstIpAddr, IPADDR_BUFFSIZE);
    p += IPADDR_BUFFSIZE;
    memcpy(p, dstHw->sll_addr, 6);

    // create a thread for sending ICMP echo
    CreateSendThread(context, SendThread);
}




