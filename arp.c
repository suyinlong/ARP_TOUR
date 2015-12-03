/*
* @File:    arp.c
* @Date:    2015-11-30 16:53:29
* @Last Modified time: 2015-12-03 00:30:44
*/

#include "arp.h"

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

arp_cache *GetCacheEntry(arp_object *obj, const uchar *ipaddr) {
    arp_cache *entry = obj->cache;
    while (entry) {
        if (strncmp(ipaddr, entry->ipaddr, IP_ALEN) == 0)
            break;
        entry = entry->next;
    }
    return entry;
}

struct hwa_info *GetHwaEntry(arp_object *obj, const uchar *ipaddr) {
    struct hwa_info *hwa = obj->hwa_info;
    while (hwa) {
        if (strncmp(ipaddr, hwa->ip_addr, IP_ALEN) == 0)
            break;
        hwa = hwa->hwa_next;
    }
    return hwa;
}

arp_cache *InsertOrUpdateCacheEntry(arp_object *obj, arp_cache *entry, arppayload *data, struct sockaddr_ll *from) {
    int i;

    printf("[ARP] Cache ");
    if (entry == NULL) {
        entry = Calloc(1, sizeof(arp_cache));
        entry->next = obj->cache;
        obj->cache = entry;
        printf("insert: ");
    } else
        printf("update: ");

    memcpy(entry->ipaddr, data->ar_spro, IP_ALEN);
    memcpy(entry->hwaddr, data->ar_shrd, ETH_ALEN);
    entry->ifindex = from->sll_ifindex;
    entry->hatype = from->sll_hatype;

    printf("<%s, ", UtilIpToString(entry->ipaddr));
    for (i = 0; i < 6; i++)
        printf("%.2x%s", entry->hwaddr[i], (i < 5) ? ":" : ", ");
    printf("%d, %d, %d>\n", entry->ifindex, entry->hatype, entry->sockfd);

    return entry;
}

void ReplyAREQ(arp_cache *entry) {
    int i;
    struct hwaddr HWaddr;
    bzero(&HWaddr, sizeof(HWaddr));

    HWaddr.sll_ifindex = entry->ifindex;
    HWaddr.sll_hatype = entry->hatype;
    HWaddr.sll_halen = 6;
    memcpy(HWaddr.sll_addr, entry->hwaddr, ETH_ALEN);

    printf("[ARP] Reply to AREQ <%s, ", UtilIpToString(entry->ipaddr));
    for (i = 0; i < 6; i++)
        printf("%.2x%s", HWaddr.sll_addr[i], (i < 5) ? ":" : ">\n");

    Write(entry->sockfd, &HWaddr, sizeof(HWaddr));
    Close(entry->sockfd);
    entry->sockfd = 0;
}

void SendREQ(arp_object *obj, uchar *ipaddr) {
    int i;
    char frame[ARP_FRAME_LEN];
    bzero(frame, sizeof(frame));

    ethhdr *eth = (ethhdr *)frame;
    arphdr *arp = (arphdr *)(frame + ETHHDR_LEN);
    arppayload *data = (arppayload *)(frame + ETHHDR_LEN + ARPHDR_LEN);

    BuildBcastFrame(eth, obj->hwa_info->if_haddr, ARP_PROTOCOL_ID);
    arp->ar_id = ARP_ID_CODE;
    arp->ar_hrd = htons(ETH_P_802_3);
    arp->ar_pro = htons(ETH_P_IP);
    arp->ar_hln = 6;
    arp->ar_pln = 4;
    arp->ar_op = htons(ARP_REQ);
    memcpy(data->ar_shrd, obj->hwa_info->if_haddr, ETH_ALEN);
    memcpy(data->ar_spro, obj->hwa_info->ip_addr, IP_ALEN);
    memcpy(data->ar_tpro, ipaddr, IP_ALEN);

    printf("[ARP] Sending out ARP REQ via interface %d <broadcast>\n", obj->hwa_info->if_index);
    PrintARPFrame(frame);
    SendFrame(obj->pfSockfd, obj->hwa_info->if_index, frame, ARP_FRAME_LEN, PACKET_BROADCAST);
}

void SendREP(arp_object *obj, arp_cache *entry, struct hwa_info *localhwa) {
    int i;
    char frame[ARP_FRAME_LEN];
    bzero(frame, sizeof(frame));

    ethhdr *eth = (ethhdr *)frame;
    arphdr *arp = (arphdr *)(frame + ETHHDR_LEN);
    arppayload *data = (arppayload *)(frame + ETHHDR_LEN + ARPHDR_LEN);

    BuildFrame(eth, entry->hwaddr, localhwa->if_haddr, ARP_PROTOCOL_ID);

    arp->ar_id = ARP_ID_CODE;
    arp->ar_hrd = htons(ETH_P_802_3);
    arp->ar_pro = htons(ETH_P_IP);
    arp->ar_hln = 6;
    arp->ar_pln = 4;
    arp->ar_op = htons(ARP_REP);
    memcpy(data->ar_shrd, localhwa->if_haddr, ETH_ALEN);
    memcpy(data->ar_spro, localhwa->ip_addr, IP_ALEN);
    memcpy(data->ar_thrd, entry->hwaddr, ETH_ALEN);
    memcpy(data->ar_tpro, entry->ipaddr, IP_ALEN);

    printf("[ARP] Sending out ARP REP via interface %d <unicast>\n", localhwa->if_index);
    PrintARPFrame(frame);
    SendFrame(obj->pfSockfd, localhwa->if_index, frame, ARP_FRAME_LEN, PACKET_OTHERHOST);
}

void ProcessREQ(arp_object *obj, char *frame, struct sockaddr_ll *from) {
    ethhdr *eth = (ethhdr *)frame;
    arphdr *arp = (arphdr *)(frame + ETHHDR_LEN);
    arppayload *data = (arppayload *)(frame + ETHHDR_LEN + ARPHDR_LEN);

    arp_cache *entry = GetCacheEntry(obj, data->ar_spro);
    struct hwa_info *localhwa = GetHwaEntry(obj, data->ar_tpro);

    if (localhwa != NULL || entry != NULL) {
        printf("[ARP] Received ARP REQ from interface %d\n", from->sll_ifindex);
        PrintARPFrame(frame);
        entry = InsertOrUpdateCacheEntry(obj, entry, data, from);
    }

    if (localhwa != NULL) {
        //reply
        SendREP(obj, entry, localhwa);
    }
}

void ProcessREP(arp_object *obj, char *frame, struct sockaddr_ll *from) {
    ethhdr *eth = (ethhdr *)frame;
    arphdr *arp = (arphdr *)(frame + ETHHDR_LEN);
    arppayload *data = (arppayload *)(frame + ETHHDR_LEN + ARPHDR_LEN);

    arp_cache *entry = GetCacheEntry(obj, data->ar_spro);
    struct hwa_info *localhwa = GetHwaEntry(obj, data->ar_tpro);
    if (entry && localhwa) {
        printf("[ARP] Received ARP REP from interface %d\n", from->sll_ifindex);
        PrintARPFrame(frame);
        entry = InsertOrUpdateCacheEntry(obj, entry, data, from);
        ReplyAREQ(entry);
    }
}

void ProcessFrame(arp_object *obj) {
    int len;
    struct sockaddr_ll from;
    bzero(&from, sizeof(struct sockaddr_ll));
    socklen_t fromlen = sizeof(struct sockaddr_ll);

    char frame[ARP_FRAME_LEN];
    bzero(&frame, sizeof(frame));

    len = RecvFrame(obj->pfSockfd, frame, ARP_FRAME_LEN, (SA *)&from, &fromlen);

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

void ProcessDomainStream(arp_object *obj) {
    int connSockfd;
    uchar ipaddr[IP_ALEN];
    struct sockaddr_un from;
    socklen_t addrlen = sizeof(from);
    arp_cache *entry;
    // find entry in cache, reply if match
    // otherwise, send out broadcast frame via pfSockfd
    // wait for reply or the connection of domain socket is closed
    connSockfd = Accept(obj->doSockfd, (struct sockaddr *) &from, &addrlen);
    Read(connSockfd, ipaddr, IP_ALEN);
    printf("[ARP] Domain socket: Incoming AREQ <%s> from %s\n", UtilIpToString(ipaddr), from.sun_path);

    entry = GetCacheEntry(obj, ipaddr);
    if (entry) {
        printf("[ARP] AREQ <%s> found in cache, reply immediately.\n", UtilIpToString(ipaddr));
        entry->sockfd = connSockfd;
        ReplyAREQ(entry);
    } else {
        printf("[ARP] AREQ <%s> not found in cache, create an incomplete entry.\n", UtilIpToString(ipaddr));
        entry = Calloc(1, sizeof(arp_cache));
        memcpy(entry->ipaddr, ipaddr, IP_ALEN);
        entry->ifindex = obj->hwa_info->if_index;
        entry->hatype = ARPHRD_ETHER;
        entry->sockfd = connSockfd;
        entry->next = obj->cache;
        obj->cache = entry;
        SendREQ(obj, ipaddr);
    }
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
    Listen(obj->doSockfd, LISTENQ);
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
        //printf("Select return\n");
        if (FD_ISSET(obj->pfSockfd, &rset)) {
            // from PF_PACKET Socket
            ProcessFrame(obj);
        }

        if (FD_ISSET(obj->doSockfd, &rset)) {
            // from Domain Socket
            ProcessDomainStream(obj);
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
    bzero(&obj, sizeof(obj));

    // Get interface information
    obj.hwa_info = Get_hw_addrs();
    obj.if_index = obj.hwa_info->if_index;

    printf("[ARP] Module started.\n");
    PrintAddressPairs(&obj);

    CreateSockets(&obj);

    ProcessSockets(&obj);

    FreeArpObject(&obj);
    exit(0);
}

