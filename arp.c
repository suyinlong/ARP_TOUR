/*
* @File:    arp.c
* @Date:    2015-11-30 16:53:29
* @Last Modified time: 2015-12-03 10:07:29
* @Description:
*     ARP service functions
*     - void PrintAddressPairs(arp_object *obj)
*         [Print all address pairs]
*     - void PrintARPFrame(char *frame)
*         [Print ARP Frame Content]
*     - arp_cache *GetCacheEntry(arp_object *obj, const uchar *ipaddr)
*         [Get cache entry by IP address]
*     - struct hwa_info *GetHwaEntry(arp_object *obj, const uchar *ipaddr)
*         [Get hwa_info entry by IP address]
*     - arp_cache *InsertOrUpdateCacheEntry(arp_object *obj, arp_cache *entry, arppayload *data, struct sockaddr_ll *from)
*         [Insert or update cache entry]
*     - void ReplyAREQ(arp_cache *entry)
*         [Reply AREQ to connected domain socket]
*     - void SendREQ(arp_object *obj, uchar *ipaddr)
*         [Send ARP REQ via broadcast]
*     - void SendREP(arp_object *obj, arp_cache *entry, struct hwa_info *localhwa)
*         [Send ARP REP via unicast]
*     - void ProcessREQ(arp_object *obj, char *frame, struct sockaddr_ll *from)
*         [Process received ARP REQ]
*     - void ProcessREP(arp_object *obj, char *frame, struct sockaddr_ll *from)
*         [Process received ARP REP]
*     - void ProcessFrame(arp_object *obj)
*         [Process received frame]
*     - void ProcessDomainStream(arp_object *obj)
*         [Process received AREQ]
*     - void CreateSockets(arp_object *obj)
*         [Create sockets for ARP service]
*     - void ProcessSockets(arp_object *obj)
*         [Main loop of ARP service]
*     + int main(int argc, char **argv)
*         [Entry function]
*/

#include "arp.h"

/* --------------------------------------------------------------------------
 *  PrintAddressPairs
 *
 *  Print all address pairs
 *
 *  @param  : arp_object    *obj    [ARP object]
 *  @return : void
 *
 *  Print all address pairs in struct hwa_info
 * --------------------------------------------------------------------------
 */
void PrintAddressPairs(arp_object *obj) {
    struct hwa_info *hwa = obj->hwa_info;
    int i;

    while (hwa) {
        printf("[ARP] Address pair found: <%s, ", UtilIpToString(hwa->ip_addr));
        for (i = 0; i < 6; i++)
            printf("%.2x%s", hwa->if_haddr[i] & 0xff, (i < 5) ? ":" : ">");
        printf(" @ interface %d\n", hwa->if_index);
        hwa = hwa->hwa_next;
    }
}

/* --------------------------------------------------------------------------
 *  PrintARPFrame
 *
 *  Print ARP Frame Content
 *
 *  @param  : char  *frame  [frame]
 *  @return : void
 *
 *  Print the content in ARP frame
 *    1. Ethernet frame header
 *    2. ARP packet header
 *    3. ARP packet payload
 * --------------------------------------------------------------------------
 */
void PrintARPFrame(char *frame) {
    int i;

    ethhdr *eth = (ethhdr *)frame;
    arphdr *arp = (arphdr *)(frame + ETHHDR_LEN);
    arppayload *data = (arppayload *)(frame + ETHHDR_LEN + ARPHDR_LEN);
    printf("      ETHHDR | dest: ");
    for (i = 0; i < 6; i++)
        printf("%.2x%s", eth->h_dest[i], (i < 5) ? ":" : ", source: ");
    for (i = 0; i < 6; i++)
        printf("%.2x%s", eth->h_source[i], (i < 5) ? ":" : ", proto: ");
    printf("%d\n", ntohs(eth->h_proto));
    printf("      ARPHDR | id: %d, hrd: 0x%.4x, pro: 0x%.4x, hln: %d, pln: %d, op: %d %s\n",
            arp->ar_id, ntohs(arp->ar_hrd), ntohs(arp->ar_pro),
            arp->ar_hln, arp->ar_pln, ntohs(arp->ar_op),
            (arp->ar_op == ntohs(ARP_REQ)) ? "REQ" : "REP");
    printf("        DATA | sender: ");
    for (i = 0; i < 6; i++)
        printf("%.2x%s", data->ar_shrd[i], (i < 5) ? ":" : " ");
    printf("%s\n", UtilIpToString(data->ar_spro));
    printf("        DATA | target: ");
    for (i = 0; i < 6; i++)
        printf("%.2x%s", data->ar_thrd[i], (i < 5) ? ":" : " ");
    printf("%s\n", UtilIpToString(data->ar_tpro));
}

/* --------------------------------------------------------------------------
 *  GetCacheEntry
 *
 *  Get cache entry by IP address
 *
 *  @param  : arp_object    *obj    [ARP object]
 *            const uchar   *ipaddr [IP address]
 *  @return : arp_cache *   [ARP cache entry, NULL if does not exist]
 *
 *  Find and return the cache entry matches IP address
 *  Return NULL if not found
 * --------------------------------------------------------------------------
 */
arp_cache *GetCacheEntry(arp_object *obj, const uchar *ipaddr) {
    arp_cache *entry = obj->cache;
    while (entry) {
        if (strncmp(ipaddr, entry->ipaddr, IP_ALEN) == 0)
            break;
        entry = entry->next;
    }
    return entry;
}

/* --------------------------------------------------------------------------
 *  GetHwaEntry
 *
 *  Get hwa_info entry by IP address
 *
 *  @param  : arp_object    *obj    [ARP object]
 *            const uchar   *ipaddr [IP address]
 *  @return : struct hwainfo *      [hwa_info entry, NULL if does not exist]
 *
 *  Find and return the hwa_info entry matches IP address
 *  Return NULL if not found
 * --------------------------------------------------------------------------
 */
struct hwa_info *GetHwaEntry(arp_object *obj, const uchar *ipaddr) {
    struct hwa_info *hwa = obj->hwa_info;
    while (hwa) {
        if (strncmp(ipaddr, hwa->ip_addr, IP_ALEN) == 0)
            break;
        hwa = hwa->hwa_next;
    }
    return hwa;
}

/* --------------------------------------------------------------------------
 *  InsertOrUpdateCacheEntry
 *
 *  Insert or update cache entry
 *
 *  @param  : arp_object            *obj    [ARP object]
 *            arp_cache             *entry  [entry]
 *            arppayload            *data   [ARP frame payload]
 *            struct sockaddr_ll    *from   [sender address structure]
 *  @return : arp_cache *   [inserted/updated ARP cache entry]
 *
 *  If entry is NULL, insert a new entry into cache
 *  Otherwise the operation would be update
 *  The entry content will be update according to data and from
 * --------------------------------------------------------------------------
 */
arp_cache *InsertOrUpdateCacheEntry(arp_object *obj, arp_cache *entry, arppayload *data, struct sockaddr_ll *from) {
    int i;

    printf("[ARP] Cache ");
    if (entry == NULL) {
        // insert a new entry, calloc memory space
        entry = Calloc(1, sizeof(arp_cache));
        entry->next = obj->cache;
        obj->cache = entry;
        printf("insert: ");
    } else
        printf("update: ");

    // fill entry content
    memcpy(entry->ipaddr, data->ar_spro, IP_ALEN);
    memcpy(entry->hwaddr, data->ar_shrd, ETH_ALEN);
    entry->ifindex = from->sll_ifindex;
    entry->hatype = from->sll_hatype;

    // print entry information
    printf("<%s, ", UtilIpToString(entry->ipaddr));
    for (i = 0; i < 6; i++)
        printf("%.2x%s", entry->hwaddr[i], (i < 5) ? ":" : ", ");
    printf("%d, %d, %d>\n", entry->ifindex, entry->hatype, entry->sockfd);

    return entry;
}

/* --------------------------------------------------------------------------
 *  ReplyAREQ
 *
 *  Reply AREQ to connected domain socket
 *
 *  @param  : arp_cache *entry  [entry]
 *  @return : void
 *
 *  When a ARP REP received and the entry is still active, send reply to
 *  connected domain socket that connected by AREQ
 *  Fill the struct hwaddr then write to the stream socket
 * --------------------------------------------------------------------------
 */
void ReplyAREQ(arp_cache *entry) {
    int i;
    struct hwaddr HWaddr;
    bzero(&HWaddr, sizeof(HWaddr));

    // fill the HWaddr
    HWaddr.sll_ifindex = entry->ifindex;
    HWaddr.sll_hatype = entry->hatype;
    HWaddr.sll_halen = ETH_ALEN;
    memcpy(HWaddr.sll_addr, entry->hwaddr, ETH_ALEN);

    // print out information
    printf("[ARP] Reply to AREQ <%s, ", UtilIpToString(entry->ipaddr));
    for (i = 0; i < 6; i++)
        printf("%.2x%s", HWaddr.sll_addr[i], (i < 5) ? ":" : ">\n");

    // write and close the socket
    Write(entry->sockfd, &HWaddr, sizeof(HWaddr));
    Close(entry->sockfd);
    entry->sockfd = 0;
}

/* --------------------------------------------------------------------------
 *  SendREQ
 *
 *  Send ARP REQ via broadcast
 *
 *  @param  : arp_object    *obj    [ARP object]
 *            uchar         *ipaddr [IP address]
 *  @return : void
 *
 *  Build a broadcast ARP frame then send it via interface eth0
 * --------------------------------------------------------------------------
 */
void SendREQ(arp_object *obj, uchar *ipaddr) {
    int i;
    char frame[ARP_FRAME_LEN];
    bzero(frame, sizeof(frame));
    // pointer
    ethhdr *eth = (ethhdr *)frame;
    arphdr *arp = (arphdr *)(frame + ETHHDR_LEN);
    arppayload *data = (arppayload *)(frame + ETHHDR_LEN + ARPHDR_LEN);
    // fill broadcast frame header
    BuildBcastFrame(eth, obj->hwa_info->if_haddr, ARP_PROTOCOL_ID);
    // fill ARP packet header
    arp->ar_id = ARP_ID_CODE;
    arp->ar_hrd = htons(ETH_P_802_3);
    arp->ar_pro = htons(ETH_P_IP);
    arp->ar_hln = 6;
    arp->ar_pln = 4;
    arp->ar_op = htons(ARP_REQ);
    // fill ARP packet payload
    memcpy(data->ar_shrd, obj->hwa_info->if_haddr, ETH_ALEN);
    memcpy(data->ar_spro, obj->hwa_info->ip_addr, IP_ALEN);
    memcpy(data->ar_tpro, ipaddr, IP_ALEN);
    // print out frame information then send the frame
    printf("[ARP] Sending ARP REQ via interface %d <broadcast>\n", obj->hwa_info->if_index);
    PrintARPFrame(frame);
    SendFrame(obj->pfSockfd, obj->hwa_info->if_index, frame, ARP_FRAME_LEN, PACKET_BROADCAST);
}

/* --------------------------------------------------------------------------
 *  SendREP
 *
 *  Send ARP REP via unicast
 *
 *  @param  : arp_object        *obj        [ARP object]
 *            arp_cache         *entry      [cache entry]
 *            struct hwa_info   *localhwa   [local hwa_info entry]
 *  @return : void
 *
 *  Build a unicast ARP frame then send it via interface eth0
 *  The sender hardware address will match the localhwa entry
 * --------------------------------------------------------------------------
 */
void SendREP(arp_object *obj, arp_cache *entry, struct hwa_info *localhwa) {
    int i;
    char frame[ARP_FRAME_LEN];
    bzero(frame, sizeof(frame));
    // pointer
    ethhdr *eth = (ethhdr *)frame;
    arphdr *arp = (arphdr *)(frame + ETHHDR_LEN);
    arppayload *data = (arppayload *)(frame + ETHHDR_LEN + ARPHDR_LEN);
    // fill unicast frame header
    BuildFrame(eth, entry->hwaddr, localhwa->if_haddr, ARP_PROTOCOL_ID);
    // fill ARP packet header
    arp->ar_id = ARP_ID_CODE;
    arp->ar_hrd = htons(ETH_P_802_3);
    arp->ar_pro = htons(ETH_P_IP);
    arp->ar_hln = 6;
    arp->ar_pln = 4;
    arp->ar_op = htons(ARP_REP);
    // fill ARP packet payload
    memcpy(data->ar_shrd, localhwa->if_haddr, ETH_ALEN);
    memcpy(data->ar_spro, localhwa->ip_addr, IP_ALEN);
    memcpy(data->ar_thrd, entry->hwaddr, ETH_ALEN);
    memcpy(data->ar_tpro, entry->ipaddr, IP_ALEN);
    // print out frame information then send the frame
    printf("[ARP] Sending out ARP REP via interface %d <unicast>\n", localhwa->if_index);
    PrintARPFrame(frame);
    SendFrame(obj->pfSockfd, localhwa->if_index, frame, ARP_FRAME_LEN, PACKET_OTHERHOST);
}

/* --------------------------------------------------------------------------
 *  ProcessREQ
 *
 *  Process received ARP REQ
 *
 *  @param  : arp_object            *obj    [ARP object]
 *            char                  *frame  [frame]
 *            struct sockaddr_ll    *from   [sender address structure]
 *  @return : void
 *
 *  Process received ARP REQ
 *  1. If the sender's entry is in cache, update the entry information
 *     Or if the target IP address matches local address, insert or update
 *     the entry information
 *  2. If the target IP address matches local address, send REP to reply
 * --------------------------------------------------------------------------
 */
void ProcessREQ(arp_object *obj, char *frame, struct sockaddr_ll *from) {
    ethhdr *eth = (ethhdr *)frame;
    arphdr *arp = (arphdr *)(frame + ETHHDR_LEN);
    arppayload *data = (arppayload *)(frame + ETHHDR_LEN + ARPHDR_LEN);

    // find sender's entry in cache
    arp_cache *entry = GetCacheEntry(obj, data->ar_spro);
    // find target's IP address in hwa_info
    struct hwa_info *localhwa = GetHwaEntry(obj, data->ar_tpro);

    if (localhwa != NULL || entry != NULL) {
        // print received frame and insert/update the cache entry
        printf("[ARP] Received ARP REQ from interface %d\n", from->sll_ifindex);
        PrintARPFrame(frame);
        entry = InsertOrUpdateCacheEntry(obj, entry, data, from);
    }

    if (localhwa != NULL) {
        // send ARP REP
        SendREP(obj, entry, localhwa);
    }
}

/* --------------------------------------------------------------------------
 *  ProcessREP
 *
 *  Process received ARP REP
 *
 *  @param  : arp_object            *obj    [ARP object]
 *            char                  *frame  [frame]
 *            struct sockaddr_ll    *from   [sender address structure]
 *  @return : void
 *
 *  Process received ARP REP
 *  If the sender's entry is in cache and target's IP matches local hwa_info,
 *  print out the information and update the entry, then send AREQ reply
 * --------------------------------------------------------------------------
 */
void ProcessREP(arp_object *obj, char *frame, struct sockaddr_ll *from) {
    ethhdr *eth = (ethhdr *)frame;
    arphdr *arp = (arphdr *)(frame + ETHHDR_LEN);
    arppayload *data = (arppayload *)(frame + ETHHDR_LEN + ARPHDR_LEN);

    // find sender's entry in cache
    arp_cache *entry = GetCacheEntry(obj, data->ar_spro);
    // find target's IP address in hwa_info
    struct hwa_info *localhwa = GetHwaEntry(obj, data->ar_tpro);

    if (entry && localhwa) {
        printf("[ARP] Received ARP REP from interface %d\n", from->sll_ifindex);
        PrintARPFrame(frame);
        entry = InsertOrUpdateCacheEntry(obj, entry, data, from);
        // reply AREQ
        ReplyAREQ(entry);
    }
}

/* --------------------------------------------------------------------------
 *  ProcessFrame
 *
 *  Process received frame
 *
 *  @param  : arp_object    *obj    [ARP object]
 *  @return : void
 *
 *  Process received frame, ignore wring ARP_ID_CODE
 *  Call ProcessREQ or ProcessREP according to the ARP operation field
 * --------------------------------------------------------------------------
 */
void ProcessFrame(arp_object *obj) {
    int len;
    struct sockaddr_ll from;
    bzero(&from, sizeof(struct sockaddr_ll));
    socklen_t fromlen = sizeof(struct sockaddr_ll);

    char frame[ARP_FRAME_LEN];
    bzero(&frame, sizeof(frame));

    len = RecvFrame(obj->pfSockfd, frame, ARP_FRAME_LEN, (SA *)&from, &fromlen);
    // pointer
    ethhdr *eth = (ethhdr *)frame;
    arphdr *arp = (arphdr *)(frame + ETHHDR_LEN);
    arppayload *data = (arppayload *)(frame + ETHHDR_LEN + ARPHDR_LEN);

    if (len < 0) {
        printf("[ARP] Frame error.\n");
        return;
    }

    // ignore the frame if identification field does not match
    if (arp->ar_id != ARP_ID_CODE)
        return;

    if (arp->ar_op == htons(ARP_REQ))
        ProcessREQ(obj, frame, &from);
    else if (arp->ar_op == htons(ARP_REP))
        ProcessREP(obj, frame, &from);
    else
        printf("[ARP] Receive undefined ARP frame.\n");
}

/* --------------------------------------------------------------------------
 *  ProcessDomainStream
 *
 *  Process received AREQ
 *
 *  @param  : arp_object    *obj    [ARP object]
 *  @return : void
 *
 *  Process received AREQ
 *  1. Accpet the connection and read the request
 *  2. If the requested address is already in cache, reply immediately
 *  3. Otherwise, create an incomplete entry then send ARP REQ
 * --------------------------------------------------------------------------
 */
void ProcessDomainStream(arp_object *obj) {
    int connSockfd;
    uchar ipaddr[IP_ALEN];
    struct sockaddr_un from;
    socklen_t addrlen = sizeof(from);
    arp_cache *entry;
    // accept and read the socket
    connSockfd = Accept(obj->doSockfd, (struct sockaddr *) &from, &addrlen);
    Read(connSockfd, ipaddr, IP_ALEN);
    printf("[ARP] Domain socket: Incoming AREQ <%s> from %s\n", UtilIpToString(ipaddr), from.sun_path);
    // try to find entry in cache
    entry = GetCacheEntry(obj, ipaddr);
    if (entry) {
        // found, send reply immediately
        printf("[ARP] AREQ <%s> found in cache, reply immediately.\n", UtilIpToString(ipaddr));
        entry->sockfd = connSockfd;
        ReplyAREQ(entry);
    } else {
        // not found, create the incomplete entry
        printf("[ARP] AREQ <%s> not found in cache, create an incomplete entry.\n", UtilIpToString(ipaddr));
        entry = Calloc(1, sizeof(arp_cache));
        memcpy(entry->ipaddr, ipaddr, IP_ALEN);
        entry->ifindex = obj->hwa_info->if_index;
        entry->hatype = ARPHRD_ETHER;
        entry->sockfd = connSockfd;
        entry->next = obj->cache;
        obj->cache = entry;
        // send ARP REQ
        SendREQ(obj, ipaddr);
    }
}

/* --------------------------------------------------------------------------
 *  CreateSockets
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
    Listen(obj->doSockfd, LISTENQ);
}

/* --------------------------------------------------------------------------
 *  ProcessSockets
 *
 *  Main loop of ARP service
 *
 *  @param  : arp_object    *obj    [arp object]
 *  @return : void
 *
 *  Wait for the message from PF_PACKET socket or Domain socket
 *  then process it
 *  Also, when there are incomplete entries in cache, also monitor those
 *  socket's readability. Clean the entries if the connection are closed.
 * --------------------------------------------------------------------------
 */
void ProcessSockets(arp_object *obj) {
    int r, maxfd;
    fd_set rset;
    arp_cache *entry, *prev;

    FD_ZERO(&rset);
    while (1) {
        // add PF_PACKET socket and domain socket
        FD_SET(obj->pfSockfd, &rset);
        FD_SET(obj->doSockfd, &rset);
        maxfd = max(obj->pfSockfd, obj->doSockfd);
        // add all incomplete entries' sockets
        entry = obj->cache;
        while (entry) {
            if (entry->sockfd > 0) {
                FD_SET(entry->sockfd, &rset);
                maxfd = max(maxfd, entry->sockfd);
            }
            entry = entry->next;
        }

        r = Select(maxfd + 1, &rset, NULL, NULL, NULL);

        if (FD_ISSET(obj->pfSockfd, &rset)) {
            // from PF_PACKET Socket
            ProcessFrame(obj);
        }

        if (FD_ISSET(obj->doSockfd, &rset)) {
            // from Domain Socket
            ProcessDomainStream(obj);
        }

checkagain:
        prev = NULL;
        entry = obj->cache;
        while (entry) {
            if (entry->sockfd > 0 && FD_ISSET(entry->sockfd, &rset)) {
                // the connection is closed, remove the entry
                if (prev) {
                    prev->next = entry->next;
                    free(entry);
                } else {
                    obj->cache = entry->next;
                    free(entry);
                }
                // print out the information and check again
                printf("[ARP] Socket connection terminated. Incomplete entry has been removed.\n");
                goto checkagain;
            }
            prev = entry;
            entry = entry->next;
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
 *  ARP service entry function
 * --------------------------------------------------------------------------
 */
int main(int argc, char **argv) {
    arp_object obj;
    bzero(&obj, sizeof(obj));

    // Get interface information
    obj.hwa_info = Get_hw_addrs();
    obj.if_index = obj.hwa_info->if_index;

    printf("[ARP] Module started.\n");
    PrintAddressPairs(&obj);
    CreateSockets(&obj);
    ProcessSockets(&obj);
    exit(0);
}

