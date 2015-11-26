/*
* @File:    tour.c
* @Date:    2015-11-25 16:33:23
* @Last Modified time: 2015-11-26 12:14:29
* @Description:
*     Tour application basic functions
*     - void StartTour(tour_object *obj)
*         [Start route traversal]
*     - void ProcessTour(tour_object *obj)
*         [Process tour packet]
*     + void FinishTour(tour_object *obj)
*         [Exit the tour process]
*     - void ParseArguments(int argc, char **argv, tour_object *obj)
*         [Parse the tour sequence]
*     - void CreateSockets(tour_object *obj)
*         [Socket creator]
*     - void ProcessSockets(tour_object *obj)
*         [Socket process function]
*     + int main(int argc, char **argv)
*         [Entry function]
*/

#include "tour.h"

/* --------------------------------------------------------------------------
 *  StartTour
 *
 *  Start route traversal
 *
 *  @param  : tour_object   *obj    [tour object]
 *  @return : void
 *
 *  For node explicitly invoked with tour sequence, start the tour by:
 *  1. Fill the sequence data
 *  2. Create and join a multicast group
 *  3. Fill the tour header
 *  4. Fill the IP header
 *  5. Send tour packet through rtSocket
 * --------------------------------------------------------------------------
 */
void StartTour(tour_object *obj) {
    char timeString[TIMESTR_BUFFSIZE], nodeTo[HOSTNAME_BUFFSIZE];
    struct sockaddr_in sin;

    uint  length = IP4_HDRLEN + TOUR_HDRLEN + IPADDR_BUFFSIZE * obj->seqLength;
    uchar *packet = Calloc(length, 1);

    struct ip *iphdr = (struct ip *)packet;
    tourhdr *rthdr = (tourhdr *)(packet + IP4_HDRLEN);
    uchar *data = packet + IP4_HDRLEN + TOUR_HDRLEN;

    // fill the sequence data
    memcpy(data, obj->ipSeq, IPADDR_BUFFSIZE * obj->seqLength);

    // fill tour header: create multicast group, fill sequence length
    // then point to the next other node
    CreateMulticastGroup(rthdr->grp, &(rthdr->port));
    JoinMulticastGroup(obj, rthdr->grp, rthdr->port);
    rthdr->seqLength = obj->seqLength;
    rthdr->index = 1;

    // fill the IP header
    BuildIpHeader(iphdr, length, obj->ipaddr, IP_SEQ(data, rthdr->index));

    // fill the sockaddr
    bzero(&sin, sizeof(struct sockaddr_in));
    sin.sin_family = AF_INET;
    sin.sin_addr.s_addr = iphdr->ip_dst.s_addr;

    UtilTime(timeString);
    UtilIpToHostname((uchar *)&iphdr->ip_dst, nodeTo);
    printf("<%s> sending routing packet to <%s> %d of %d\n", timeString, nodeTo, rthdr->index + 1, rthdr->seqLength);
    // send to the next node
    Sendto(obj->rtSockfd, packet, length, 0, (struct sockaddr *) &sin, sizeof(struct sockaddr));

    // free the packet
    free(packet);
}

/* --------------------------------------------------------------------------
 *  ProcessTour
 *
 *  Process tour packet
 *
 *  @param  : tour_object   *obj    [tour object]
 *  @return : void
 *
 *  For received tour packet
 *  1. Check the identification field
 *  2. If it is first time visit the node, join the multicast group
 *  3. If the tour is not finished, modify the index in tour header then
 *     send to next node; Otherwise, wait for the echo replies then start
 *     multicast process
 * --------------------------------------------------------------------------
 */
void ProcessTour(tour_object *obj) {
    char timeString[TIMESTR_BUFFSIZE], nodeFrom[HOSTNAME_BUFFSIZE], nodeTo[HOSTNAME_BUFFSIZE];
    struct sockaddr_in sin;
    uchar pktBuff[PACKET_BUFFSIZE];
    struct ip *iphdr = (struct ip *) pktBuff;
    tourhdr *rthdr = (tourhdr *) (pktBuff + IP4_HDRLEN);
    uchar *data = pktBuff + IP4_HDRLEN + TOUR_HDRLEN;

    Recvfrom(obj->rtSockfd, pktBuff, PACKET_BUFFSIZE, 0, NULL, NULL);

    // ignore invalid packet (identification field != TOUR_ID_CODE)
    if (iphdr->ip_id != TOUR_ID_CODE)
        return;

    UtilTime(timeString);
    UtilIpToHostname((uchar *)&iphdr->ip_src, nodeFrom);
    printf("<%s> received routing packet from <%s>\n", timeString, nodeFrom);

    // check if already in the multicast group
    if (obj->mcastPort != rthdr->port)
        JoinMulticastGroup(obj, rthdr->grp, rthdr->port);

    rthdr->index++;
    if (rthdr->index < rthdr->seqLength) {
        // send to next
        // fill the IP header
        BuildIpHeader(iphdr, ntohs(iphdr->ip_len), obj->ipaddr, IP_SEQ(data, rthdr->index));

        // fill the sockaddr
        bzero(&sin, sizeof(struct sockaddr_in));
        sin.sin_family = AF_INET;
        sin.sin_addr.s_addr = iphdr->ip_dst.s_addr;

        UtilIpToHostname((uchar *)&iphdr->ip_dst, nodeTo);
        printf("<%s> sending routing packet to <%s> %d of %d\n", timeString, nodeTo, rthdr->index + 1, rthdr->seqLength);
        // send to the next node
        Sendto(obj->rtSockfd, pktBuff, ntohs(iphdr->ip_len), 0, (struct sockaddr *) &sin, sizeof(struct sockaddr));
    } else {
        printf("<%s> routing packet reached the last node.\n", timeString);
        // TODO: mcast start need to wait for a few seconds
        StartMulticast(obj, rthdr->grp, rthdr->port);
    }

}

/* --------------------------------------------------------------------------
 *  FinishTour
 *
 *  Exit the tour process
 *
 *  @param  : tour_object   *obj    [tour object]
 *  @return : void
 *
 *  Stop the tour process and clean tour sequence and multicast information
 * --------------------------------------------------------------------------
 */
void FinishTour(tour_object *obj) {
    printf("<%s> tour has ended.\n", obj->hostname);

    obj->seqLength = 0;
    if (obj->nodeSeq)
        free(obj->nodeSeq);
    if (obj->ipSeq)
        free(obj->ipSeq);
    obj->nodeSeq = NULL;
    obj->ipSeq = NULL;
    bzero(obj->mcastAddr, IPADDR_BUFFSIZE);
    obj->mcastPort = 0;
}

/* --------------------------------------------------------------------------
 *  ParseArguments
 *
 *  Parse the tour sequence
 *
 *  @param  : int           argc
 *            char          **argv
 *            tour_object   *obj    [tour object]
 *  @return : void
 *
 *  Parse the tour sequence, remove the same nodes appear consequentively
 *  If the final sequence is only source itself, the tour sequence is invalid
 * --------------------------------------------------------------------------
 */
void ParseArguments(int argc, char **argv, tour_object *obj) {
    int i, j;

    // return if the traversal sequence does not exist
    if (argc == 1) {
        obj->seqLength = 0;
        return;
    }

    // Calloc memory space for sequence
    obj->seqLength = argc;
    obj->nodeSeq = Calloc(obj->seqLength, HOSTNAME_BUFFSIZE);
    obj->ipSeq = Calloc(obj->seqLength, IPADDR_BUFFSIZE);

    // the source: the node itself
    strncpy(NODE_SEQ(obj->nodeSeq, 0), obj->hostname, HOSTNAME_BUFFSIZE);
    strncpy(IP_SEQ(obj->ipSeq, 0), obj->ipaddr, IPADDR_BUFFSIZE);

    j = 1;

    // copy node name and convert it into IP address
    for (i = 1; i < argc; i++) {
        // skip if previous node is the same as current node
        if (strcmp(NODE_SEQ(obj->nodeSeq, j - 1), argv[i]) == 0)
            continue;
        strncpy(NODE_SEQ(obj->nodeSeq, j), argv[i], HOSTNAME_BUFFSIZE);
        UtilHostnameToIp(NODE_SEQ(obj->nodeSeq, j), IP_SEQ(obj->ipSeq, j));
        j++;
    }

    if (j == 1) {
        printf("Tour module received an invalid node sequence after simplification (point to itself solely).\n");
        obj->seqLength = 0;
        free(obj->nodeSeq);
        free(obj->ipSeq);
        obj->nodeSeq = NULL;
        obj->ipSeq = NULL;
        return;
    } else {
        obj->seqLength = j;
        obj->nodeSeq = realloc(obj->nodeSeq, obj->seqLength * HOSTNAME_BUFFSIZE);
        obj->ipSeq = realloc(obj->ipSeq, obj->seqLength * IPADDR_BUFFSIZE);
    }

    printf("Tour module received node sequence(%d) from command line arguments:\n", obj->seqLength);
    for (i = 0; i < obj->seqLength; i++)
        printf("%*s - %-*s%s\n", HOSTNAME_BUFFSIZE, NODE_SEQ(obj->nodeSeq, i), IPSTR_BUFFSIZE, UtilIpToString(IP_SEQ(obj->ipSeq, i)), (i == 0) ? " * source" : "");
}

/* --------------------------------------------------------------------------
 *  CreateSockets
 *
 *  Sockets creator
 *
 *  @param  : tour_object   *obj    [tour object]
 *  @return : void
 *
 *  Create two IP raw sockets, one PF_PACKET socket and one UDP socket
 * --------------------------------------------------------------------------
 */
void CreateSockets(tour_object *obj) {
    const int on = 1;

    // rtSocket: IP raw socket
    //           used for sending tour packet
    //   option: IP_HDRINCL
    obj->rtSockfd = Socket(AF_INET, SOCK_RAW, TOUR_PROTOCOL_ID);
    Setsockopt(obj->rtSockfd, IPPROTO_IP, IP_HDRINCL, &on, sizeof(on));

    obj->pgSockfd = Socket(AF_INET, SOCK_RAW, IPPROTO_ICMP);

    obj->pfSockfd = Socket(PF_PACKET, SOCK_RAW, ETH_P_IP);

    // msSocket: UDP socket
    //           used for sending multicast datagram
    //   option: SO_REUSEADDR
    obj->msSockfd = Socket(AF_INET, SOCK_DGRAM, 0);
    Setsockopt(obj->msSockfd, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on));

    // mrSocket: UDP socket
    //           used for receiving multicast datagram
    //   option: SO_REUSEADDR
    obj->mrSockfd = Socket(AF_INET, SOCK_DGRAM, 0);
    Setsockopt(obj->mrSockfd, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on));

}

/* --------------------------------------------------------------------------
 *  ProcessSockets
 *
 *  Socket process function
 *
 *  @param  : tour_object   *obj    [tour object]
 *  @return : void
 *
 *  Process the incoming message from sockets
 * --------------------------------------------------------------------------
 */
void ProcessSockets(tour_object *obj) {
    int maxfdp1 = max(obj->rtSockfd, obj->mrSockfd) + 1;
    int r;
    fd_set rset;

    FD_ZERO(&rset);
    while (1) {
        FD_SET(obj->rtSockfd, &rset);
        FD_SET(obj->mrSockfd, &rset);

        r = Select(maxfdp1, &rset, NULL, NULL, NULL);

        if (FD_ISSET(obj->rtSockfd, &rset)) {
            // from rt socket
            ProcessTour(obj);
        }
        if (FD_ISSET(obj->mrSockfd, &rset)) {
            // from UDP multicast socket
            ProcessMulticast(obj);
        }

    }

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
 *  Tour application entry function
 * --------------------------------------------------------------------------
 */
int main(int argc, char **argv) {
    int i;

    // init tour_object
    tour_object obj;
    bzero(&obj, sizeof(tour_object));

    // get hostname and primary IP address of the node
    UtilHostname(obj.hostname);
    UtilHostnameToIp(obj.hostname, obj.ipaddr);

    printf("Tour module started on %s (%s).\n", obj.hostname, UtilIpToString(obj.ipaddr));

    // parse arguments to tour sequence
    ParseArguments(argc, argv, &obj);

    // create sockets
    CreateSockets(&obj);

    srandom((unsigned int)time(NULL));

    if (obj.seqLength > 0) {
        // as the source node, initial route traversal
        StartTour(&obj);
    }

    // process the incoming frame/packet from sockets
    ProcessSockets(&obj);


}
