/*
* @File:    util.c
* @Date:    2015-11-25 16:27:12
* @Last Modified time: 2015-11-26 11:29:27
* @Description:
*     Util function library, some miscellaneous helper functions
*     + uint UtilRandom(uint min, uint max)
*         [Random unsigned integer generator]
*     + void UtilTime(char *timestr)
*         [Time string generator]
*     + void UtilHostname(char *hostname)
*         [Get hostname of current node]
*     + int UtilIpToHostname(const uchar *ipaddr, char *hostname)
*         [Convert IP address to hostname]
*     + int UtilHostnameToIp(const char *hostname, uchar *ipaddr)
*         [Convert hostname to IP address]
*     + char *UtilIpToString(const uchar *ipaddr)
*         [Convert IP address to readable string]
*/

#include "tour.h"

/* --------------------------------------------------------------------------
 *  UtilRandom
 *
 *  Random unsigned integer generator
 *
 *  @param  : uint  min     [min value of random number]
 *            uint  max     [max value of random number]
 *  @return : uint          [random number between min and max]
 *
 *  Return the random unsigned integer between min and max
 * --------------------------------------------------------------------------
 */
uint UtilRandom(uint min, uint max) {
    return ((random() % (max - min + 1)) + min);
}

/* --------------------------------------------------------------------------
 *  UtilTime
 *
 *  Time string generator
 *
 *  @param  : char  *timestr    [pointer to the string]
 *  @return : void
 *
 *  Create readable format of time string
 * --------------------------------------------------------------------------
 */
void UtilTime(char *timestr) {
    time_t ticks = time(NULL);
    bzero(timestr, TIMESTR_BUFFSIZE);
    snprintf(timestr, TIMESTR_BUFFSIZE, "%.24s", ctime(&ticks));
}

/* --------------------------------------------------------------------------
 *  UtilHostname
 *
 *  Get hostname of current node
 *
 *  @param  : char  *hostname   [pointer to the string]
 *  @return : void
 *
 *  Use gethostname to get the current hostname
 * --------------------------------------------------------------------------
 */
void UtilHostname(char *hostname) {
    int i;
    if ( (i = gethostname(hostname, HOSTNAME_BUFFSIZE)) == -1 )
        err_quit("UtilHostname: gethostname error\n");
}

/* --------------------------------------------------------------------------
 *  UtilIpToHostname
 *
 *  Convert IP address to hostname
 *
 *  @param  : const uchar   *ipaddr     [IP address, network style]
 *            char          *hostname   [Hostname]
 *  @return : int           [ -1 if failed ]
 *
 *  Convert IP address (4-byte network style) to hostname
 * --------------------------------------------------------------------------
 */
int UtilIpToHostname(const uchar *ipaddr, char *hostname) {
    struct hostent *he;

    he = gethostbyaddr((struct in_addr *)ipaddr, sizeof(struct in_addr), AF_INET);
    if (he == NULL) {
        printf("UtilIpToHostname error: gethostbyaddr error for \n");
        return -1;
    }
    memcpy(hostname, he->h_name, HOSTNAME_BUFFSIZE);
    return 0;
}

/* --------------------------------------------------------------------------
 *  UtilHostnameToIp
 *
 *  Convert hostname to IP address
 *
 *  @param  : const char    *hostname   [Hostname]
 *            uchar         *ipaddr     [IP address]
 *  @return : int           [ -1 if failed ]
 *
 *  Convert hostname to IP address (4-byte network style)
 * --------------------------------------------------------------------------
 */
int UtilHostnameToIp(const char *hostname, uchar *ipaddr) {
    struct hostent      *he;
    he = gethostbyname(hostname);
    //if (he == NULL || hostname[0] != 'v' || hostname[1] != 'm') {
    if (he == NULL) {
        printf("UtilHostnameToIp error: gethostbyname error for %s\n", hostname);
        return -1;
    }
    memcpy(ipaddr, he->h_addr, IPADDR_BUFFSIZE);
    return 0;
}

/* --------------------------------------------------------------------------
 *  UtilIptoString
 *
 *  Convert IP address to readable string
 *
 *  @param  : const uchar   *ipaddr     [IP address]
 *  @return : char *        [Readable IP address string]
 *
 *  Convert IP address (4-byte network style) to readable string
 * --------------------------------------------------------------------------
 */
char *UtilIpToString(const uchar *ipaddr) {
    return inet_ntoa(*(struct in_addr*)ipaddr);
}
