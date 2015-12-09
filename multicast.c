/*
* @File:    multicast.c
* @Date:    2015-11-25 19:29:30
* @Last Modified time: 2015-12-08 23:23:17
* @Description:
*     Multicast function library
*     + void CreateMulticastGroup(uchar *grp, int *port)
*         [Multicast group generator]
*     + void JoinMulticastGroup(tour_object *obj, uchar *grp, int port)
*         [Join the multicast group]
*     + void LeaveMulticastGroup(tour_object *obj, uchar *grp, int port)
*         [Leave the multicast group]
*     - void SendMulticast(tour_object *obj, char *msg)
*         [Send out multicast message]
*     + void StartMulticast(tour_object *obj)
*         [Start multicast]
*     + void ProcessMulticast(tour_object *obj)
*         [Process received multicast message]
*/

#include "tour.h"

/* --------------------------------------------------------------------------
 *  CreateMulticastGroup
 *
 *  Multicast group generator
 *
 *  @param  : uchar     *grp    [multicast group address]
 *            int       *port   [multicast port number]
 *  @return : void
 *
 *  Create multicast group address and port number
 * --------------------------------------------------------------------------
 */
void CreateMulticastGroup(uchar *grp, int *port) {
    // group ID should not be 1, 2
    // we also do not use 0
    uchar mcast_addr[4] = MCAST_ADDRESS;
    memcpy(grp, mcast_addr, IPADDR_BUFFSIZE);
    *port = MCAST_PORT;
}

/* --------------------------------------------------------------------------
 *  JoinMulticastGroup
 *
 *  Join the multicast group
 *
 *  @param  : tour_object   *obj    [tour object]
 *            uchar         *grp    [multicast group address]
 *            int           port    [multicast port number]
 *  @return : void
 *
 *  Bind the UDP socket to multicast address and join the multicast group
 *  Save multicast group information in tour object
 * --------------------------------------------------------------------------
 */
void JoinMulticastGroup(tour_object *obj, uchar *grp, int port) {
    struct ip_mreq mreq;

    // Join the multicast group
    memcpy(&mreq.imr_multiaddr, grp, IPADDR_BUFFSIZE);
    mreq.imr_interface.s_addr = htonl(INADDR_ANY);

    Setsockopt(obj->mrSockfd, IPPROTO_IP, IP_ADD_MEMBERSHIP, &mreq, sizeof(mreq));

    memcpy(obj->mcastAddr, grp, IPADDR_BUFFSIZE);
    obj->mcastPort = port;

    printf("[TOUR] Join multicast address: %d.%d.%d.%d:%d\n", grp[0], grp[1], grp[2], grp[3], port);
}

/* --------------------------------------------------------------------------
 *  LeaveMulticastGroup
 *
 *  Leave the multicast group
 *
 *  @param  : tour_object   *obj    [tour object]
 *            uchar         *grp    [multicast group address]
 *            int           port    [multicast port number]
 *  @return : void
 *
 *  Leave multicast group and clear the information in tour object
 * --------------------------------------------------------------------------
 */
void LeaveMulticastGroup(tour_object *obj, uchar *grp, int port) {
    struct ip_mreq mreq;

    memcpy(&mreq.imr_multiaddr, grp, IPADDR_BUFFSIZE);
    mreq.imr_interface.s_addr = htonl(INADDR_ANY);

    Setsockopt(obj->mrSockfd, IPPROTO_IP, IP_DROP_MEMBERSHIP, &mreq, sizeof(mreq));

    printf("[TOUR] Leave multicast address: %d.%d.%d.%d:%d\n",grp[0], grp[1], grp[2], grp[3], port);

    bzero(obj->mcastAddr, IPADDR_BUFFSIZE);
    obj->mcastPort = 0;
}

/* --------------------------------------------------------------------------
 *  SendMulticast
 *
 *  Send out multicast message
 *
 *  @param  : tour_object   *obj    [tour object]
 *            char          *msg    [multicast message]
 *  @return : void
 *
 *  Use the multicast group information stored in tour object to send
 *  multicast message
 * --------------------------------------------------------------------------
 */
void SendMulticast(tour_object *obj, char *msg) {
    struct sockaddr_in mcastaddr;

    bzero(&mcastaddr, sizeof(mcastaddr));
    mcastaddr.sin_family = AF_INET;
    memcpy(&mcastaddr.sin_addr, obj->mcastAddr, IPADDR_BUFFSIZE);
    mcastaddr.sin_port = htons(obj->mcastPort);

    printf("[TOUR] Node %s. Sending : %s.\n", obj->hostname, msg);
    Sendto(obj->msSockfd, msg, strlen(msg) + 1, 0, (struct sockaddr *)&mcastaddr, sizeof(mcastaddr));
}

/* --------------------------------------------------------------------------
 *  StartMulticast
 *
 *  Start multicast
 *
 *  @param  : tour_object   *obj    [tour object]
 *  @return : void
 *
 *  When the last node on tour sequence reached and received a few echo
 *  replies, the node start the multicast indentification process
 * --------------------------------------------------------------------------
 */
void StartMulticast(tour_object *obj) {
    char msg[MCAST_BUFFSIZE];

    bzero(msg, MCAST_BUFFSIZE);
    snprintf(msg, MCAST_BUFFSIZE, "<<<<<This is node %s. Tour has ended. Group members please identify yourselves.>>>>>", obj->hostname);
    SendMulticast(obj, msg);
}

/* --------------------------------------------------------------------------
 *  ProcessMulticast
 *
 *  Process received multicast message
 *
 *  @param  : tour_object   *obj    [tour object]
 *  @return : void
 *
 *  Process received multicast message
 *  If an identification process request has been received, start another
 *  select() to receive the multicast messages with 5 seconds timeout
 *  When timeout expires, exit the tour process
 * --------------------------------------------------------------------------
 */
void ProcessMulticast(tour_object *obj) {
    char msg[MCAST_BUFFSIZE];

    int r;
    r = Recvfrom(obj->mrSockfd, msg, MCAST_BUFFSIZE, 0, NULL, NULL);

    printf("[TOUR] Node %s. Received: %s.\n", obj->hostname, msg);


    // identify request, send out multicast message
    if (strstr(msg, "identify") != NULL) {
        // TODO: stop pinging
        snprintf(msg, MCAST_BUFFSIZE, "<<<<<Node %s. I am a member of the group.>>>>>", obj->hostname);
        SendMulticast(obj, msg);

        fd_set  rset;
        FD_ZERO(&rset);

        // Timeout = 5s
        struct timeval timeout;
        timeout.tv_sec  = 5;
        timeout.tv_usec = 0;

        // use select to receive multicast messages
        while (1) {
            FD_SET(obj->mrSockfd, &rset);
            r = select(obj->mrSockfd + 1, &rset, NULL, NULL, &timeout);
            if (r == -1 && errno == EINTR)
                continue;

            if (r == 0) {
                // timeout, exit mcast and tour process
                FinishTour(obj);
                // exit mcast group // unbind?
                break;
            }
            if (FD_ISSET(obj->mrSockfd, &rset))
                ProcessMulticast(obj);
        }
    }
}
