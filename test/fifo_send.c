#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "comm_if.h"


#define APP_NAME "fifo_send"

#if 0
# define FIFO_FILE_PATH "/tmp/fifo_test.pipe"
#else
# define FIFO_FILE_PATH "./fifo_test.pipe"
#endif


int main(int argc, char *argv[])
{
    tFifoHandle handle;


    if (argc < 2)
    {
        printf("Usage: %s \"message...\"\n\n", APP_NAME);
        return -1;
    }

    #ifdef QUIET
    comm_setLogMask( LOG_MASK_NONE );
    #else
    comm_setLogMask( LOG_MASK_ALL );
    #endif

    handle = comm_fifoWriteInit(FIFO_FILE_PATH, 1);
    if (0 == handle)
    {
        printf("[%s] initial FIFO failed\n\n", APP_NAME);
        return -1;
    }

    printf("[%s] \"%s\"\n", APP_NAME, argv[1]);

    comm_fifoWritePut(handle, (void *)argv[1], strlen(argv[1]));

    comm_fifoWriteUninit( handle );

    return 0;
}

