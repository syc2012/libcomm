#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "comm_if.h"


#define APP_NAME "tcp_recv"


static void _tcpAcptFunc(void *pArg, tTcpUser *pUser)
{
    ;
}

static void _tcpExitFunc(void *pArg, tTcpUser *pUser)
{
    ;
}

static void _tcpRecvFunc(
    void           *pArg,
    tTcpUser       *pUser,
    unsigned char  *pData,
    unsigned short  size
)
{
    pData[ size ] = 0x00;
    printf("[%s] \"%s\"\n", APP_NAME, (char *)pData);
}

int main(int argc, char *argv[])
{
    tTcpIpv4ServerHandle handle;
    unsigned char buf[256];
    int len;


    if (argc < 2)
    {
        /*
        * argv[0] : tcp_recv
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

    handle = comm_tcpIpv4ServerInit(
                 atoi( argv[1] ),
                 0,
                 _tcpAcptFunc,
                 _tcpExitFunc,
                 _tcpRecvFunc,
                 NULL
             );
    if (0 == handle)
    {
        printf("[%s] initial TCP server failed\n\n", APP_NAME);
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

    comm_tcpIpv4ServerUninit( handle );

    return 0;
}

