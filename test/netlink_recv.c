#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "comm_if.h"


#define APP_NAME "netlink_recv"


static void _netlinkRecvFunc(
    void           *pArg,
    unsigned char  *pData,
    unsigned short  size,
    unsigned short  flags
)
{
    pData[ size ] = 0x00;
    printf("[%s] \"%s\"\n", APP_NAME, (char *)pData);
}

static void _netlinkExitFunc(void *pArg, int code)
{
    ;
}

int main(int argc, char *argv[])
{
    tNetlinkHandle handle;
    unsigned char buf[256];
    int len;


    #ifdef QUIET
    comm_setLogMask( LOG_MASK_NONE );
    #else
    comm_setLogMask( LOG_MASK_ALL );
    #endif


    handle = comm_netlinkInit(_netlinkRecvFunc, _netlinkExitFunc, NULL);
    if (0 == handle)
    {
        printf("[%s] initial netlink failed\n\n", APP_NAME);
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

    comm_netlinkUninit( handle );

    return 0;
}

