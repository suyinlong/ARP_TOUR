/*
* @File:    areq.c
* @Date:    2015-12-02 23:29:25
* @Last Modified time: 2015-12-03 11:02:36
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

    printf("[API] AREQ \"%s\" to local ARP service...\n", UtilIpToString(ipaddr));
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
        printf("[API] AREQ timeout.\n");
        return -1;
    }

    r = Read(sockfd, HWaddr, sizeof(struct hwaddr));

    printf("[API] AREQ \"%s\" received: ", UtilIpToString(ipaddr));
    printf("<%d, %d, %d, ", HWaddr->sll_ifindex, HWaddr->sll_hatype, HWaddr->sll_halen);
    for (i = 0; i < 6; i++)
        printf("%.2x%s", HWaddr->sll_addr[i], (i < 5) ? ":" : ">\n");
}

/*

// temp test main(), will be deletetd

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

    int r = areq((struct sockaddr *) &addr, sizeof(addr), &HWaddr);

    if (r <= 0) {
        printf("AREQ timeout or error!\n");
        return -1;
    }

    printf("ifindex: %d hatype: %d halen:%d\n", HWaddr.sll_ifindex, HWaddr.sll_hatype, HWaddr.sll_halen);
    int i;
    for (i = 0; i < HWaddr.sll_halen; i++)
        printf("%.2x%s", HWaddr.sll_addr[i], (i < HWaddr.sll_halen-1) ? ":" : "\n");

    return 0;
}
*/
