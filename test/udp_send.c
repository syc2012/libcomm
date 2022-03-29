#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "comm_if.h"


#define APP_NAME "udp_send"


int main(int argc, char *argv[])
{
    tUdpIpv4Handle handle;


    if (argc < 4)
    {
        /*
        * argv[0] : udp_send
        * argv[1] : IPv4 address string
        * argv[2] : port number
        * argv[3] : message string
        */
        printf("Usage: %s ip_addr port_num \"message...\"\n\n", APP_NAME);
        return -1;
    }

    #ifdef QUIET
    comm_setLogMask( LOG_MASK_NONE );
    #else
    comm_setLogMask( LOG_MASK_ALL );
    #endif

    handle = comm_udpIpv4Init(0, NULL, NULL);
    if (0 == handle)
    {
        printf("[%s] initial UDP failed\n\n", APP_NAME);
        return -1;
    }

    printf("[%s] \"%s\"\n", APP_NAME, argv[3]);

    comm_udpIpv4Send(
        handle,
        argv[1],
        (unsigned short)atoi( argv[2] ),
        (unsigned char *)argv[3],
        strlen( argv[3] )
    );

    comm_udpIpv4Uninit( handle );

    return 0;
}

