#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "comm_if.h"


#define APP_NAME "udp_recv"


static void _udpRecvFunc(
    void            *pArg,
    unsigned char   *pData,
    unsigned short   size,
    struct sockaddr *pAddr
)
{
    pData[ size ] = 0x00;
    printf("[%s] \"%s\"\n", APP_NAME, (char *)pData);
}

int main(int argc, char *argv[])
{
    tUdpIpv4Handle handle;
    unsigned char buf[256];
    int len;


    if (argc < 2)
    {
        /*
        * argv[0] : udp_recv
        * argv[1] : port number
        */
        printf("Usage: %s port_num\n\n", APP_NAME);
        return -1;
    }

    #ifdef QUIET
    comm_setLogMask( LOG_MASK_NONE );
    #else
    comm_setLogMask( LOG_MASK_ALL );
    #endif

    handle = comm_udpIpv4Init(atoi( argv[1] ), _udpRecvFunc, NULL);
    if (0 == handle)
    {
        printf("[%s] initial UDP failed\n\n", APP_NAME);
        return -1;
    }

    while ( 1 )
    {
        memset(buf, 0x00, 256);
        len = read(STDIN_FILENO, buf, 255);

        if (0x0A == buf[len-1])
        {
            buf[len-1] = 0x00;
            len--;
        }

        if ((0 == strcmp("exit", (char *)buf)) ||
            (0 == strcmp("quit", (char *)buf)))
        {
            printf("\n[%s] terminated\n\n", APP_NAME);
            break;
        }
    }

    comm_udpIpv4Uninit( handle );

    return 0;
}

