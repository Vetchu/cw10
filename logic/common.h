//
// Created by vetch on 02.06.19.
//

#ifndef CW10_COMMON_H
#define CW10_COMMON_H

#include "../utils/hashmap.h"

#define MSG_SIZE 500
#define PORT 5000
#define PLACETAKEN ""
int bufferSize=60000;
#ifdef DGRAM
#define CONN_MODE SOCK_DGRAM
#else
#define CONN_MODE SOCK_STREAM
#endif
enum connectType {
    NETWORK,
    UNIX
};





#endif //CW10_COMMON_H
