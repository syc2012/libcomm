#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <semaphore.h>
#include "nptp_serv.h"
#include "nptp_event.h"
#include "nptp_xml.h"
#include "nptp_proxy.h"


tMapping *g_pMapping[MAX_MAPPING_NUM];
int g_mappingNum = 0;

tIpcDgramHandle g_ipcDgramhandle;


static void ipcRecvFunc(
    void           *pArg,
    unsigned char  *pData,
    unsigned short  size,
    char           *pPath
)
{
    int flag;

    pData[ size ] = 0x00;
    if (0 == strcmp("help", (char *)pData))
    {
        PRINT("service ready\n");
    }
    else if (0 == strcmp("list", (char *)pData))
    {
        serv_list();
    }
    else if (0 == strcmp("exit", (char *)pData))
    {
        event_uninit();
    }
    else if (0 == strcmp("dump", (char *)pData))
    {
        flag = comm_getDumpFlag();
        comm_setDumpFlag(flag ^ 0x1);
    }
    else if (0 == strcmp("0", (char *)pData))
    {
        comm_setLogMask( LOG_MASK_NONE );
    }
    else if (0 == strcmp("1", (char *)pData))
    {
        comm_setLogMask( LOG_MASK_1 );
    }
    else if (0 == strcmp("2", (char *)pData))
    {
        comm_setLogMask( LOG_MASK_2 );
    }
    else if (0 == strcmp("3", (char *)pData))
    {
        comm_setLogMask(LOG_MASK_1 | LOG_MASK_2);
    }
    else if (0 == strcmp("4", (char *)pData))
    {
        comm_setLogMask( LOG_MASK_3 );
    }
    else if (0 == strcmp("5", (char *)pData))
    {
        comm_setLogMask(LOG_MASK_1 | LOG_MASK_3);
    }
    else if (0 == strcmp("6", (char *)pData))
    {
        comm_setLogMask(LOG_MASK_2 | LOG_MASK_3);
    }
    else if (0 == strcmp("7", (char *)pData))
    {
        comm_setLogMask( LOG_MASK_ALL );
    }
}

void serv_list(void)
{
    int i;


    PRINT("\n");
    for (i=0; i<g_mappingNum; i++)
    {
        if ( g_pMapping[i] )
        {
            PRINT(
                "%u:[1;31m%c[0m %s/%s:[1;31m%c[0m \"%s\"\n",
                g_pMapping[i]->tcpPort,
                (g_pMapping[i]->tcpConnect ? '*' : '-'),
                g_pMapping[i]->pipePath,
                g_pMapping[i]->pipeName,
                (g_pMapping[i]->pipeConnect ? '*' : '-'),
                g_pMapping[i]->descript
            );
        }
    }
    PRINT("\n");
}

int serv_init(void)
{
    int i;


    comm_setLogMask( LOG_MASK_NONE );

    for (i=0; i<MAX_MAPPING_NUM; i++)
    {
        g_pMapping[i] = NULL;
    }
    g_mappingNum = 0;

    if (xml_init() != 0)
    {
        PRINT("ERR: xml_init ... failed\n");
        return -1;
    }

    if (0 == g_mappingNum)
    {
        PRINT("ERR: mapping is empty\n");
        xml_uninit();
        return -1;
    }

    if (proxy_init() != 0)
    {
        PRINT("ERR: proxy_init ... failed\n");
        xml_uninit();
        return -1;
    }

    serv_list();
    PRINT("service ready\n");

    if (event_init() != 0)
    {
        PRINT("ERR: event_init ... failed\n");
        proxy_uninit();
        xml_uninit();
        return -1;
    }

    g_ipcDgramhandle = comm_ipcDgramInit(IPC_DGRAM_FILE_PATH, ipcRecvFunc, NULL);
    if (0 == g_ipcDgramhandle)
    {
        PRINT("ERR: comm_ipcDgramInit ... failed\n");
        event_uninit();
        proxy_uninit();
        xml_uninit();
        return -1;
    }

    return 0;
}

void serv_uninit(void)
{
    int i;


    comm_ipcDgramUninit( g_ipcDgramhandle );

    event_uninit();

    proxy_uninit();

    xml_uninit();

    for (i=0; i<MAX_MAPPING_NUM; i++)
    {
        if ( g_pMapping[i] )
        {
            free( g_pMapping[i] );
            g_pMapping[i] = NULL;
        }
    }
    g_mappingNum = 0;

    PRINT("service terminate\n");
}

/**
*  Main entry of Named Pipe TCP Proxy.
*  @param [in]  argc  Nuumber of arguments.
*  @param [in]  argv  Array of arguments.
*  @returns  success(0) or fail(-1).
*/
int main(int argc, char *argv[])
{
    int background = 1;
    int ch;


    opterr = 0;
    while ((ch=getopt(argc, argv, "f")) != -1)
    {
        switch ( ch )
        {
            case 'f':
                background = 0;
                break;
            default:
                ;
        }
    }

    if ( background )
    {
        pid_t pid = fork();
        if (pid > 0) 
        {
            /* If we are the parent, die. */
            exit(0);
        }
    }

    if (serv_init() != 0)
    {
        PRINT("ERR: fail to initial service\n");
        return -1;
    }

    /* infinity loop to monitor named pipes */
    event_monitor();

    serv_uninit();

    return 0;
}

