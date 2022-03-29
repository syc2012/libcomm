#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "comm_if.h"


#define APP_NAME "netlink_send"


int main(int argc, char *argv[])
{
    tNetlinkHandle handle;


    if (argc < 5)
    {
        printf("Usage: %s type flag seq \"message...\"\n\n", APP_NAME);
        return -1;
    }

    #ifdef QUIET
    comm_setLogMask( LOG_MASK_NONE );
    #else
    comm_setLogMask( LOG_MASK_ALL );
    #endif

    handle = comm_netlinkInit(NULL, NULL, NULL);
    if (0 == handle)
    {
        printf("[%s] initial netlink failed\n\n", APP_NAME);
        return -1;
    }

    printf("[%s] \"%s\"\n", APP_NAME, argv[1]);

    comm_netlinkSendToKernel(
        handle,
        (unsigned char *)argv[4],
        strlen( argv[4] ),
        (unsigned short)atoi( argv[1] ),
        (unsigned short)atoi( argv[2] ),
        (unsigned int)atoi( argv[3] )
    );

    comm_netlinkUninit( handle );

    return 0;
}

