/*
* @File:    arp.c
* @Date:    2015-11-30 16:53:29
* @Last Modified time: 2015-12-01 19:20:04
*/

#include "arp.h"

void ProcessFrame(arp_object *obj) {
    int len;
    struct sockaddr_ll from;
    bzero(&from, sizeof(struct sockaddr_ll));
    socklen_t fromlen = sizeof(struct sockaddr_ll);

    char frame[ARP_FRAME_LEN];
    bzero(&frame, sizeof(frame));

    ethhdr *eth = (ethhdr *)frame;
    arphdr *arp = (arphdr *)(frame + ETHHDR_LEN);
    arppayload *data = (arppayload *)(frame + ETHHDR_LEN + ARPHDR_LEN);

    len = recv_frame(obj->pfSockfd, &frame, (SA *)&from, &fromlen);

    // ignore the frame if identification field does not match
    if (arp->ar_id != ARP_ID_CODE)
        return;

    // build cache entry from request
    // check target IP address
    // reply if match

}

void ProcessDomainStream(arp_object *obj) {
    // maybe need to Connect() coz SOCK_STREAM?
    // find entry in cache, reply if match
    // otherwise, send out broadcast frame via pfSockfd
    // wait for reply or the connection of domain socket is closed
}

/* --------------------------------------------------------------------------
 *  create_sockets
 *
 *  Create sockets for ARP service
 *
 *  @param  : arp_object    *obj    [arp object]
 *  @return : void
 *
 *  Create a PF_PACKET socket for frame communication / ARP frame
 *  Create a Domain socket for datagram communication / request from areq
 * --------------------------------------------------------------------------
 */
void CreateSockets(arp_object *obj) {
    struct sockaddr_un arpaddr;

    // Create PF_PACKET Socket
    obj->pfSockfd = Socket(PF_PACKET, SOCK_RAW, htons(ARP_PROTOCOL_ID));

    bzero(&arpaddr, sizeof(arpaddr));
    arpaddr.sun_family = AF_LOCAL;
    strcpy(arpaddr.sun_path, ARP_PATH);

    // Create UNIX Domain Socket
    unlink(ARP_PATH);
    obj->doSockfd = Socket(AF_LOCAL, SOCK_STREAM, 0);
    Bind(obj->doSockfd, (SA *)&arpaddr, sizeof(arpaddr));
}

/* --------------------------------------------------------------------------
 *  process_sockets
 *
 *  Main loop of ARP service
 *
 *  @param  : arp_object    *obj    [arp object]
 *  @return : void
 *
 *  Wait for the message from PF_PACKET socket or Domain socket
 *  then process it
 * --------------------------------------------------------------------------
 */
void ProcessSockets(arp_object *obj) {
    int maxfdp1 = max(obj->pfSockfd, obj->doSockfd) + 1;
    int r;
    fd_set rset;

    FD_ZERO(&rset);
    while (1) {
        FD_SET(obj->pfSockfd, &rset);
        FD_SET(obj->doSockfd, &rset);

        r = Select(maxfdp1, &rset, NULL, NULL, NULL);

        if (FD_ISSET(obj->pfSockfd, &rset)) {
            // from PF_PACKET Socket
            process_frame(obj);
        }

        if (FD_ISSET(obj->doSockfd, &rset)) {
            // from Domain Socket
            process_domain_dgram(obj);
        }
    }
}

void FreeArpObject(arp_object *obj) {

}

/* --------------------------------------------------------------------------
 *  main
 *
 *  Entry function
 *
 *  @param  : int   argc
 *            char  **argv
 *  @return : int
 *
 *  ARP service entry function
 * --------------------------------------------------------------------------
 */
int main(int argc, char **argv) {
    arp_object obj;
    bzero(&obj, sizeof(odr_object));

    // Get interface information


    CreateSockets(&obj);

    ProcessSockets(&obj);

    FreeArpObject(&obj);
    exit(0);
}

