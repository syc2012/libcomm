#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "comm_if.h"


#define APP_NAME "raw_recv"


static void _rawRecvFunc(
    void           *pArg,
    unsigned char  *pData,
    unsigned short  size
)
{
    unsigned short i;

    printf("[%s] %u bytes\n", APP_NAME, size);
    for (i=0; i<size; i++)
    {
        if ((i != 0) && ((i % 16) == 0))
        {
            printf("\n");
        }
        printf(" %02X", pData[i]);
    }
    printf("\n\n");
}

int main(int argc, char *argv[])
{
    tRawHandle handle;
    unsigned char buf[256];
    int len;


    if (argc < 3)
    {
        printf("Usage: %s netdevice promiscuous\n\n", APP_NAME);
        return -1;
    }

    #ifdef QUIET
    comm_setLogMask( LOG_MASK_NONE );
    #else
    comm_setLogMask( LOG_MASK_ALL );
    #endif

    handle = comm_rawSockInit(argv[1], _rawRecvFunc, NULL);
    if (0 == handle)
    {
        printf("[%s] initial raw socket failed\n\n", APP_NAME);
        return -1;
    }

    /* set network device to promiscuous mode */
    if ( atoi( argv[2] ) )
    {
        printf("[%s] enable device promiscuous mode\n", APP_NAME);
        comm_rawPromiscMode(handle, 1);
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

    comm_rawSockUninit( handle );

    return 0;
}

