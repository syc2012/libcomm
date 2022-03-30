#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "nptp_serv.h"
#include "nptp_proxy.h"


static void tcpIpv4AcptFunc(void *pArg, tTcpUser *pUser)
{
    tMapping *pMapping = pArg;

    /* TCP client socket connected */
    pMapping->tcpConnect = 1;
    pMapping->pTcpUser = pUser;
}

static void tcpIpv4ExitFunc(void *pArg, tTcpUser *pUser)
{
    tMapping *pMapping = pArg;

    /* TCP client socket closed */
    pMapping->tcpConnect = 0;
    pMapping->pTcpUser = NULL;
}

static void tcpIpv4RecvFunc(
    void          *pArg,
    tTcpUser      *pUser,
    unsigned char *pData,
    unsigned short size
)
{
    tMapping *pMapping = pArg;

    /* forward TCP client data to IPC server */
    comm_ipcStreamClientSend(pMapping->ipcStreamHandle, pData, size);
}

void proxy_launchTcp(tMapping *pMapping)
{
    tTcpIpv4ServerHandle handle;

    handle = comm_tcpIpv4ServerInit(
                 pMapping->tcpPort,
                 1,
                 tcpIpv4AcptFunc,
                 tcpIpv4ExitFunc,
                 tcpIpv4RecvFunc,
                 pMapping
             );
    if (0 == handle)
    {
        PRINT("ERR: comm_tcpIpv4ServerInit\n");
    }

    pMapping->tcpServerHandle = handle;
}

void proxy_ceaseTcp(tMapping *pMapping)
{
    comm_tcpIpv4ServerUninit( pMapping->tcpServerHandle );
    pMapping->tcpServerHandle = 0;
    pMapping->tcpConnect = 0;
    pMapping->pTcpUser = NULL;
}

static void ipcStreamExitFunc(void *pArg, int code)
{
    tMapping *pMapping = pArg;

    /* IPC stream server closed */
    pMapping->pipeConnect = 0;
}

static void ipcStreamRecvFunc(
    void           *pArg,
    unsigned char  *pData,
    unsigned short  size
)
{
    tMapping *pMapping = pArg;
    tTcpUser *pUser = pMapping->pTcpUser;

    if ( pUser )
    {
        /* forward IPC client data to TCP server */
        comm_tcpIpv4ServerSend(pUser, pData, size);
    }
}

void proxy_connectIpc(tMapping *pMapping)
{
    tIpcStreamClientHandle handle;
    char clientPath[256];
    char serverPath[256];
    int error;

    if ( pMapping->ipcStreamHandle )
    {
        comm_ipcStreamClientUninit( pMapping->ipcStreamHandle );
    }

    sprintf(
        clientPath,
        "%s%d",
        IPC_STREAM_FILE_PATH,
        pMapping->index
    );

    handle = comm_ipcStreamClientInit(
                          clientPath,
                          ipcStreamRecvFunc,
                          ipcStreamExitFunc,
                          pMapping
                      );
    if (0 == handle)
    {
        PRINT("ERR: comm_ipcStreamClientInit\n");
    }

    pMapping->ipcStreamHandle = handle;

    sprintf(
        serverPath,
        "%s/%s",
        pMapping->pipePath,
        pMapping->pipeName
    );

    if (0 == access(serverPath, F_OK))
    {
        error = comm_ipcStreamClientConnect(
                    pMapping->ipcStreamHandle,
                    serverPath
                );
        if (error != 0)
        {
            PRINT("ERR: comm_ipcStreamClientConnect\n");
        }
        else
        {
            /* IPC stream server connected */
            pMapping->pipeConnect = 1;
        }
    }
}

void proxy_disconnectIpc(tMapping *pMapping)
{
    comm_ipcStreamClientUninit( pMapping->ipcStreamHandle );
    pMapping->ipcStreamHandle = 0;
    pMapping->pipeConnect = 0;
}

int proxy_init(void)
{
    int i;

    for (i=0; i<g_mappingNum; i++)
    {
        if ( g_pMapping[i] )
        {
            proxy_launchTcp( g_pMapping[i] );
            proxy_connectIpc( g_pMapping[i] );
        }
    }

    return 0;
}

void proxy_uninit(void)
{
    int i;

    for (i=0; i<g_mappingNum; i++)
    {
        if ( g_pMapping[i] )
        {
            proxy_disconnectIpc( g_pMapping[i] );
            proxy_ceaseTcp( g_pMapping[i] );
        }
    }
}

