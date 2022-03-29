#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "comm_if.h"


#define APP_NAME "fifo_recv"

#if 0
# define FIFO_FILE_PATH "/tmp/fifo_test.pipe"
#else
# define FIFO_FILE_PATH "./fifo_test.pipe"
#endif


static void _fifoGetFunc(void *pArg, unsigned char *pData, unsigned short size)
{
    pData[ size ] = 0x00;
    printf("[%s] \"%s\"\n", APP_NAME, (char *)pData);
}

static void _fifoCloseFunc(void *pArg, int code)
{
    printf("[%s] terminated\n\n", APP_NAME);
    exit(0);
}

int main(int argc, char *argv[])
{
    tFifoHandle handle;
    unsigned char buf[256];
    int len;


    #ifdef QUIET
    comm_setLogMask( LOG_MASK_NONE );
    #else
    comm_setLogMask( LOG_MASK_ALL );
    #endif

    handle = comm_fifoReadInit(
                 FIFO_FILE_PATH,
                 1,
                 _fifoGetFunc,
                 _fifoCloseFunc,
                 NULL
             );
    if (0 == handle)
    {
        printf("[%s] initial FIFO failed\n\n", APP_NAME);
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

    comm_fifoReadUninit( handle );

    return 0;
}

