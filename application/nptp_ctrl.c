#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "comm_if.h"


#define APP_NAME "nptp_ctrl"

#if 0
# define IPC_SOUR_ADDR "/var/run/nptp_ctrl_dgram.sock"
# define IPC_DEST_ADDR "/var/run/nptp_serv_dgram.sock"
#else
# define IPC_SOUR_ADDR "./nptp_ctrl_dgram.sock"
# define IPC_DEST_ADDR "./nptp_serv_dgram.sock"
#endif


void help(void)
{
    printf("Usage: %s [OPTION...]\n", APP_NAME);
    printf("\n");
    printf("  -[0..7]   Set log mask.\n");
    printf("  -d        Turn on/off dump flag.\n");
    printf("  -t        List connection table.\n");
    printf("  -x        Terminate the server.\n");
    printf("  -h        Show the help message.\n");
    printf("\n");

}

int main(int argc,char *argv[])
{
    tIpcDgramHandle handle;
    char log[4];
    int ch;


    if (argc != 2)
    {
        help();
        return -1;
    }

    comm_setLogMask( LOG_MASK_NONE );

    handle = comm_ipcDgramInit(IPC_SOUR_ADDR, NULL, NULL);
    if (0 == handle)
    {
        printf("[%s] initial IPC failed\n\n", APP_NAME);
        return -1;
    }

    opterr = 0;
    while ((ch=getopt(argc, argv, "01234567dtxh")) != -1)
    {
        switch ( ch )
        {
            case '0': case '1': case '2': case '3':
            case '4': case '5': case '6': case '7':
                sprintf(log, "%c", ch);
                comm_ipcDgramSendTo(handle, IPC_DEST_ADDR, (void *)log, 2);
                break;
            case 'd':
                comm_ipcDgramSendTo(handle, IPC_DEST_ADDR, (void *)"dump", 5);
                break;
            case 't':
                comm_ipcDgramSendTo(handle, IPC_DEST_ADDR, (void *)"list", 5);
                break;
            case 'x':
                comm_ipcDgramSendTo(handle, IPC_DEST_ADDR, (void *)"exit", 5);
                break;
            default:
                help();
                comm_ipcDgramSendTo(handle, IPC_DEST_ADDR, (void *)"help", 5);
        }
    }

    comm_ipcDgramUninit( handle );

    return 0;
}

