#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "comm_if.h"


#define APP_NAME "ipc_send"

#if 0
# define IPC_SOUR_FILE_PATH "/var/run/ipc_send.sock"
# define IPC_DEST_FILE_PATH "/var/run/ipc_recv.sock"
#else
# define IPC_SOUR_FILE_PATH "./ipc_send.sock"
# define IPC_DEST_FILE_PATH "./ipc_recv.sock"
#endif


int main(int argc, char *argv[])
{
    tIpcDgramHandle handle;


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

    handle = comm_ipcDgramInit(IPC_SOUR_FILE_PATH, NULL, NULL);
    if (0 == handle)
    {
        printf("[%s] initial IPC failed\n\n", APP_NAME);
        return -1;
    }

    printf("[%s] \"%s\"\n", APP_NAME, argv[1]);

    comm_ipcDgramSendTo(handle, IPC_DEST_FILE_PATH, (void *)argv[1], strlen(argv[1]));

    comm_ipcDgramUninit( handle );

    return 0;
}

