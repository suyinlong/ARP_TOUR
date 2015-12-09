/*
* @File:    areq.c
* @Date:    2015-12-02 23:29:25
* @Last Modified time: 2015-12-08 22:50:54
* @Description:
*     ARP API function
*     + int areq(struct sockaddr *IPaddr, socklen_t sockaddrlen, struct hwaddr *HWaddr)
*         [ARP API function]
*/

#include "arp.h"

/* --------------------------------------------------------------------------
 *  areq
 *
 *  ARP API function
 *
 *  @param  : struct sockaddr   *IPaddr         [IP address structure]
 *            socklen_t         sockaddrlen     [address structure length]
 *            struct hwaddr     *HWaddr         [Hardware address structure]
 *  @return : int               [The number of bytes read, -1 if failed]
 *
 *  Create a domain socket to write IP address request to ARP service
 *  Wait for the response (3 seconds)
 *  Write the response to HWaddr
 * --------------------------------------------------------------------------
 */
int areq(struct sockaddr *IPaddr, socklen_t sockaddrlen, struct hwaddr *HWaddr) {
    int sockfd, r, i;
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

    printf("[AREQ] AREQ \"%s\" to local ARP service...\n", UtilIpToString(ipaddr));
    // Write the IP address to ARP service
    Write(sockfd, ipaddr, IP_ALEN);

    fd_set rset;
    FD_ZERO(&rset);
    FD_SET(sockfd, &rset);
    // Timeout = 3s
    struct timeval timeout;
    timeout.tv_sec  = AREQ_TIMEOUT;
    timeout.tv_usec = 0;

    // monitor the socket, timeout = 3s
    r = select(sockfd+1, &rset, NULL, NULL, &timeout);

    // error or timeout, return -1
    if (r <= 0) {
        printf("[AREQ] AREQ timeout.\n");
        return -1;
    }

    r = Read(sockfd, HWaddr, sizeof(struct hwaddr));

    printf("[AREQ] AREQ \"%s\" received: ", UtilIpToString(ipaddr));
    printf("<%d, %d, %d, ", HWaddr->sll_ifindex, HWaddr->sll_hatype, HWaddr->sll_halen);
    for (i = 0; i < 6; i++)
        printf("%.2x%s", HWaddr->sll_addr[i], (i < 5) ? ":" : ">\n");
}
