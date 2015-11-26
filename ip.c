/*
* @File:    ip.c
* @Date:    2015-11-25 21:30:36
* @Last Modified time: 2015-11-26 11:29:12
* @Description:
*     IP function library
*     + void BuildIpHeader(struct ip *iphdr, uint length, uchar *src, uchar *dst)
*         [IP header builder]
*/

#include "tour.h"

/* --------------------------------------------------------------------------
 *  BuildIpHeader
 *
 *  IP header builder
 *
 *  @param  : struct ip *iphdr  [IP header]
 *            uint      length  [IP packet length]
 *            uchar     *src    [source IP address]
 *            uchar     *dst    [destination IP address]
 *  @return : void
 *
 *  Fill the IP header
 * --------------------------------------------------------------------------
 */
void BuildIpHeader(struct ip *iphdr, uint length, uchar *src, uchar *dst) {
    // IPv4 header
    iphdr->ip_hl = 5;
    iphdr->ip_v = 4;
    iphdr->ip_tos = 0;
    iphdr->ip_len = htons(length);
    iphdr->ip_id = TOUR_ID_CODE;
    iphdr->ip_off = htons(0);
    iphdr->ip_ttl = 1;
    iphdr->ip_p = TOUR_PROTOCOL_ID;
    iphdr->ip_sum = 0;
    memcpy((void *)&iphdr->ip_src, src, IPADDR_BUFFSIZE);
    memcpy((void *)&iphdr->ip_dst, dst, IPADDR_BUFFSIZE);
/*
    printf("IP_HL: %d %d\n", iphdr->ip_hl, ntohs(iphdr->ip_hl));
    printf("IP_V: %d %d\n", iphdr->ip_v, ntohs(iphdr->ip_v));
    printf("IP_TOS: %d %d\n", iphdr->ip_tos, ntohs(iphdr->ip_tos));
    printf("IP_LEN: %d %d\n", iphdr->ip_len, ntohs(iphdr->ip_len));
    printf("IP_ID: %d %d\n", iphdr->ip_id, ntohs(iphdr->ip_id));
    printf("IP_OFF: %d %d\n", iphdr->ip_off, ntohs(iphdr->ip_off));
    printf("IP_TTL: %d %d\n", iphdr->ip_ttl, ntohs(iphdr->ip_ttl));
    printf("IP_P: %d %d\n", iphdr->ip_p, ntohs(iphdr->ip_p));
    printf("IP_SUM: %d %d\n", iphdr->ip_sum, ntohs(iphdr->ip_sum));


    printf("iphdr (%d) src: %d.%d.%d.%d dst:%d.%d.%d.%d\n", length, src[0], src[1], src[2], src[3], dst[0], dst[1], dst[2], dst[3]);
    */
}
