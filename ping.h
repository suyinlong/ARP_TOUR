/**
* @file         :  ping.h
* @author       :  Jiewen Zheng
* @date         :  2015-12-6
* @brief        :  ping implementation
* @changelog    :
**/

#ifndef __PING_H_
#define __PING_H_

#include "tour.h"

/**
* @brief Start ping the node with dstIp and dstHw
* @param[in] obj    : tour object
* @param[in] dstIp  : destination ip address
* @param[in] dstHw  : destination mac address
* @return NULL
**/
void Ping(tour_object *obj, const struct sockaddr_in *dstIp, const struct hwaddr *dstHw);

#endif // __PING_H_

