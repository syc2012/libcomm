#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "comm_if.h"


#define APP_NAME "uart_send"


int main(int argc, char *argv[])
{
    tUartHandle  handle;

    char *pDevName = "/dev/ttyS0";
    int   baudRate = 115200;
    int   parity   = 0;
    int   waitTime = -1;


    if (argc < 4)
    {
        /*
        * argv[0] : uart_send
        * argv[1] : /dev/ttyS1
        * argv[2] : 115200
        * argv[3] : message string
        */
        printf("Usage: %s device baud_rate \"message...\"\n\n", APP_NAME);
        return -1;
    }

    #ifdef QUIET
    comm_setLogMask( LOG_MASK_NONE );
    #else
    comm_setLogMask( LOG_MASK_ALL );
    #endif

    pDevName = argv[1];
    handle = uart_openDev(pDevName, NULL, NULL);
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

    printf("[%s] \"%s\"\n", APP_NAME, argv[3]);

    uart_send(handle, (void *)argv[3], strlen( argv[3] ));

    uart_closeDev( handle );

    return 0;
}

