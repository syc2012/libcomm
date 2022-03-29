#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "comm_if.h"


#define APP_NAME "uart_recv"


static void _uartRecvFunc(
    void           *pArg,
    unsigned char  *pData,
    unsigned short  size
)
{
#if 0
    pData[ size ] = 0x00;
    printf("[%s] \"%s\"\n", APP_NAME, (char *)pData);
#else
    unsigned short i;

    printf("[%s]\n", APP_NAME);
    for (i=0; i<size; i++)
    {
        if ((i != 0) && ((i % 16) == 0))
        {
            printf("\n");
        }
        if ((pData[i] > 0x1F) && (pData[i] < 0x7F))
        {
            printf("  %c", pData[i]);
        }
        else
        {
            printf(" %02X", pData[i]);
        }
    }
    printf("\n\n");
#endif
}

int main(int argc, char *argv[])
{
    tUartHandle  handle;
    unsigned char buf[256];
    int len;

    char *pDevName = "/dev/ttyS0";
    int   baudRate = 115200;
    int   parity   = 0;
    int   waitTime = -1;


    if (argc < 3)
    {
        /*
        * argv[0] : uart_send
        * argv[1] : /dev/ttyS1
        * argv[2] : 115200
        */
        printf("Usage: %s device baud_rate\n\n", APP_NAME);
        return -1;
    }

    #ifdef QUIET
    comm_setLogMask( LOG_MASK_NONE );
    #else
    comm_setLogMask( LOG_MASK_ALL );
    #endif

    pDevName = argv[1];
    handle = uart_openDev(pDevName, _uartRecvFunc, NULL);
    if (0 == handle)
    {
        printf("[%s] open UART failed\n\n", APP_NAME);
        return -1;
    }

    baudRate = atoi( argv[2] );
    if (uart_configDev(handle, baudRate, parity, waitTime) < 0)
    {
        printf("[%s] configure UART failed\n\n", APP_NAME);
        uart_closeDev( handle );
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

    uart_closeDev( handle );

    return 0;
}

