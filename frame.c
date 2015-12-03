/*
* @File: frame.c
* @Date: 2015-11-10 22:45:45
* @Last Modified time: 2015-12-02 22:49:21
* @Description:
*     Frame functions, provides frame builder and frame send/recv function
*     - void build_frame_header(char *frame, uchar *dst_mac, uchar *src_mac, ushort ftype)
*         [Frame header builder]
*     + void build_frame(odr_frame *frame, uchar *dst_mac, uchar *src_mac, ushort ftype, void *data)
*         [Frame builder]
*     + void build_bcast_frame(odr_frame *frame, uchar *src_mac, ushort ftype, void *data)
*         [Broadcast frame builder]
*     + int send_frame(int sockfd, int if_index, odr_frame *frame, uchar pkttype)
*         [Frame send function]
*     + int recv_frame(int sockfd, odr_frame *frame, struct sockaddr *from, socklen_t *fromlen)
*         [Frame receive function]
*/

#include "arp.h"

/* --------------------------------------------------------------------------
 *  build_frame
 *
 *  Frame builder
 *
 *  @param  : odr_frame     *frame      [frame]
 *            uchar         *dst_mac    [Destination MAC address]
 *            uchar         *src_mac    [Source MAC address]
 *            ushort        ftype       [frame type]
 *            void          *data       [payload of frame]
 *  @return : void
 *  @see    : function#build_frame_header
 *
 *  Build the frame. Call before send_frame()
 * --------------------------------------------------------------------------
 */
void BuildFrame(ethhdr *frame, uchar *dst_mac, uchar *src_mac, ushort proto) {
    memcpy(frame->h_dest, dst_mac, ETH_ALEN);
    memcpy(frame->h_source, src_mac, ETH_ALEN);

    frame->h_proto = htons(proto);
}

/* --------------------------------------------------------------------------
 *  build_bcast_frame
 *
 *  Broadcast frame builder
 *
 *  @param  : odr_frame     *frame      [frame]
 *            uchar         *src_mac    [Source MAC address]
 *            ushort        ftype       [frame type]
 *            void          *data       [payload of frame]
 *  @return : void
 *  @see    : function#build_frame
 *
 *  Build the broadcast frame. The destination MAC address is defined as
 *  ff:ff:ff:ff:ff:ff
 * --------------------------------------------------------------------------
 */
void BuildBcastFrame(ethhdr *frame, uchar *src_mac, ushort proto) {
    uchar dst_mac[6] = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff};
    BuildFrame(frame, dst_mac, src_mac, proto);
}

/* --------------------------------------------------------------------------
 *  send_frame
 *
 *  Frame send function
 *
 *  @param  : int           sockfd      [socket file descriptor]
 *            int           if_index    [interface index]
 *            odr_frame     *frame      [frame]
 *            uchar         pkttype     [packet type]
 *  @return : int   [the number of bytes that are sent, -1 if failed]
 *
 *  Set the sockaddr_ll structure and send the frame through PF_PACKET
 *  socket
 * --------------------------------------------------------------------------
 */
int SendFrame(int sockfd, int if_index, char *frame, int framelen, uchar pkttype) {
    int i;
    struct sockaddr_ll socket_address;
    ethhdr *eth = (ethhdr *) frame;

    socket_address.sll_family   = PF_PACKET;
    socket_address.sll_protocol = eth->h_proto;
    socket_address.sll_ifindex  = if_index;
    socket_address.sll_hatype   = ARPHRD_ETHER;
    socket_address.sll_pkttype  = pkttype;
    socket_address.sll_halen    = ETH_ALEN;

    // Copy destination mac address
    for (i = 0; i < 6; i++)
        socket_address.sll_addr[i] = eth->h_dest[i];
    // unused part
    socket_address.sll_addr[6]  = 0x00;
    socket_address.sll_addr[7]  = 0x00;

    return sendto(sockfd, frame, framelen, 0,
          (struct sockaddr*)&socket_address, sizeof(socket_address));
}

/* --------------------------------------------------------------------------
 *  recv_frame
 *
 *  Frame receive function
 *
 *  @param  : int               sockfd      [socket file descriptor]
 *            odr_frame         *frame      [frame]
 *            struct sockaddr   *from       [store sender address]
 *            socklent_t        *fromlen    [length of structure]
 *  @return : int   [the number of bytes that are received, -1 if failed]
 *
 *  Receive the frame and the sender information
 * --------------------------------------------------------------------------
 */
int RecvFrame(int sockfd, char *frame, int framelen, struct sockaddr *from, socklen_t *fromlen) {
    return recvfrom(sockfd, frame, framelen, 0, from, fromlen);
}
