#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "comm_if.h"


#define APP_NAME "tcp_send"


int main(int argc, char *argv[])
{
    tTcpIpv4ClientHandle handle;
    int error;


    if (argc < 4)
    {
        /*
        * argv[0] : tcp_send
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

    handle = comm_tcpIpv4ClientInit(0, NULL, NULL, NULL);
    if (0 == handle)
    {
        printf("[%s] initial TCP client failed\n\n", APP_NAME);
        return -1;
    }

    error = comm_tcpIpv4ClientConnect(handle, argv[1], atoi( argv[2] ));
    if (error != 0)
    {
        printf("[%s] connect to server failed (%d)\n\n", APP_NAME, error);
        comm_tcpIpv4ClientUninit( handle );
        return -1;
    }

    printf("[%s] connect to %s:%s\n\n", APP_NAME, argv[1], argv[2]);


    printf("[%s] \"%s\"\n", APP_NAME, argv[3]);
    comm_tcpIpv4ClientSend(handle, (void *)argv[3], strlen( argv[3] ));

    comm_tcpIpv4ClientUninit( handle );

    return 0;
}

