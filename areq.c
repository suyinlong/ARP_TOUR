/*
* @File:    areq.c
* @Date:    2015-12-02 23:29:25
* @Last Modified time: 2015-12-02 23:51:20
*/

#include "arp.h"

int areq(struct sockaddr *IPaddr, socklen_t sockaddrlen, struct hwaddr *HWaddr) {
    int sockfd;
    uchar *ipaddr = ((uchar *)IPaddr) + 4;

    struct sockaddr_un areqaddr, arpaddr;

    bzero(&areqaddr, sizeof(areqaddr));
    areqaddr.sun_family = AF_LOCAL;
    strcpy(areqaddr.sun_path, TMP_PATH);

    bzero(&arpaddr, sizeof(arpaddr));
    arpaddr.sun_family = AF_LOCAL;
    strcpy(arpaddr.sun_path, ARP_PATH);

    sockfd = mkstemp(areqaddr.sun_path);
    // Call unlink so that whenever the file is closed or the program exits
    // the temporary file is deleted
    unlink(areqaddr.sun_path);

    // Create UNIX Domain Socket, Bind and Connect
    sockfd = Socket(AF_LOCAL, SOCK_STREAM, 0);
    Bind(sockfd, (SA *)&areqaddr, sizeof(areqaddr));
    Connect(sockfd, (SA *)&arpaddr, sizeof(arpaddr));

    Write(sockfd, ipaddr, IP_ALEN);

    Read(sockfd, HWaddr, sizeof(struct hwaddr));
}

int main(int argc, char **argv) {
    struct sockaddr_in addr;
    bzero(&addr, sizeof(addr));
    addr.sin_family = AF_INET;
    uchar *ipaddr = ((uchar *)&addr) + 4;
    ipaddr[0] = 130;
    ipaddr[1] = 245;
    ipaddr[2] = 156;
    ipaddr[3] = 21;

    struct hwaddr HWaddr;

    areq((struct sockaddr *) &addr, sizeof(addr), &HWaddr);

    printf("ifindex: %d hatype: %d halen:%d\n", HWaddr.sll_ifindex, HWaddr.sll_hatype, HWaddr.sll_halen);
    int i;
    for (i = 0; i < HWaddr.sll_halen; i++)
        printf("%.2x%s", HWaddr.sll_addr[i], (i < HWaddr.sll_halen-1) ? ":" : "\n");

    return 0;
}
