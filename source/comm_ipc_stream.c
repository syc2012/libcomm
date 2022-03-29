#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <pthread.h>
#include <sys/socket.h>
#include <sys/un.h>
#include "comm_if.h"
#include "comm_log.h"


typedef struct _tIpcStreamClientContext
{
    char              remotePath[256];
    char              localPath[256];
    int               fd;

    tIpcClientRecvCb  pClientRecvFunc;
    tIpcClientExitCb  pClientExitFunc;
    void             *pClientArg;
    pthread_t         thread;
    int               running;

    unsigned char     recvMsg[COMM_BUF_SIZE+1];
} tIpcStreamClientContext;


/**
*  Initialize a stream UNIX domain socket.
*  @param [in]  pContext  A @ref tIpcStreamClientContext object.
*  @returns  Success(0) or failure(-1).
*/
static int _ipcStreamInitClient(tIpcStreamClientContext *pContext)
{
    struct sockaddr_un bindAddr;
    socklen_t bindAddrLen;
    int fd;


    unlink( pContext->localPath );

    fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (fd < 0)
    {
        perror( "socket" );
        return -1;
    }

    bindAddrLen = sizeof( struct sockaddr_un );
    memset(&bindAddr, 0x00, bindAddrLen);
    bindAddr.sun_family = AF_UNIX;
    strcpy(bindAddr.sun_path, pContext->localPath);

    if (bind(fd, (struct sockaddr *)&bindAddr, bindAddrLen) < 0)
    {
        perror( "bind" );
        close( fd );
        return -1;
    }

    pContext->fd = fd;

    LOG_2("IPC %s is ready\n", pContext->localPath);
    return 0;
}

/**
*  Un-initialize a stream UNIX domain socket.
*  @param [in]  pContext  A @ref tIpcStreamClientContext object.
*/
static void _ipcStreamUninitClient(tIpcStreamClientContext *pContext)
{
    if (pContext->fd > 0)
    {
        close( pContext->fd );
        pContext->fd = -1;
    }

    unlink( pContext->localPath );

    LOG_2("IPC %s is closed\n", pContext->localPath);
}

/**
*  Thread function for IPC stream client receiving.
*  @param [in]  pArg  A @ref tIpcStreamClientContext object.
*/
static void *_ipcStreamClientRecvTask(void *pArg)
{
    tIpcStreamClientContext *pContext = pArg;
    int len;


    LOG_2("start the thread: %s\n", __func__);

    while ( pContext->running )
    {
        LOG_3("%s ... recv\n", pContext->localPath);
        len = recv(
                  pContext->fd,
                  pContext->recvMsg,
                  COMM_BUF_SIZE,
                  0
              );
        if (len <= 0)
        {
            LOG_1("IPC stream server was terminated\n");
            close( pContext->fd );
            pContext->fd = -1;
            /* notify the client that server is closed */
            if ( pContext->pClientExitFunc )
            {
                pContext->pClientExitFunc(pContext->pClientArg, len);
            }
            break;
        }

        LOG_3("<- %s\n", pContext->remotePath);
        LOG_DUMP("IPC stream client recv", pContext->recvMsg, len);

        if ( pContext->pClientRecvFunc )
        {
            pContext->pClientRecvFunc(
                          pContext->pClientArg,
                          pContext->recvMsg,
                          len
                      );
        }
    }

    LOG_2("stop the thread: %s\n", __func__);
    pContext->running = 0;

    return NULL;
}

/**
*  Initialize IPC stream client.
*  @param [in]  pFileName  Application's socket file name.
*  @param [in]  pRecvFunc  Application's receive callback function.
*  @param [in]  pExitFunc  Application's exit callback function.
*  @param [in]  pArg       Application's argument.
*  @returns  IPC stream client handle.
*/
tIpcStreamClientHandle comm_ipcStreamClientInit(
    char             *pFileName,
    tIpcClientRecvCb  pRecvFunc,
    tIpcClientExitCb  pExitFunc,
    void             *pArg
)
{
    tIpcStreamClientContext *pContext = NULL;
    int error;


    pContext = malloc( sizeof( tIpcStreamClientContext ) );
    if (NULL == pContext)
    {
        LOG_ERROR("fail to allocate IPC stream client context\n");
        return 0;
    }

    memset(pContext, 0x00, sizeof( tIpcStreamClientContext ));
    strncpy(pContext->localPath, pFileName, 255);
    pContext->pClientRecvFunc = pRecvFunc;
    pContext->pClientExitFunc = pExitFunc;
    pContext->pClientArg = pArg;
    pContext->fd = -1;

    error = _ipcStreamInitClient( pContext );
    if (error != 0)
    {
        LOG_ERROR("fail to create IPC stream client\n");
        LOG_ERROR("path: %s\n", pFileName);
        free( pContext );
        return 0;
    }

    if (NULL == pRecvFunc)
    {
        LOG_1("ignore IPC stream receive function\n");
    }

    if (NULL == pExitFunc)
    {
        LOG_1("ignore IPC stream exit function\n");
    }

    LOG_1("IPC stream client initialized\n");
    return ((tIpcStreamClientHandle)pContext);
}

/**
*  Un-initialize IPC stream client.
*  @param [in]  handle  IPC stream client handle.
*/
void comm_ipcStreamClientUninit(tIpcStreamClientHandle handle)
{
    tIpcStreamClientContext *pContext = (tIpcStreamClientContext *)handle;

    if ( pContext )
    {
        if ( pContext->running )
        {
            pContext->running = 0;
            pthread_cancel( pContext->thread );
            pthread_join(pContext->thread, NULL);
            usleep(1000);
        }

        _ipcStreamUninitClient( pContext );

        free( pContext );
        LOG_1("IPC stream client un-initialized\n");
    }
}

/**
*  Connect to an IPC stream server.
*  @param [in]  handle     IPC stream client handle.
*  @param [in]  pFileName  Destination application's socket file name.
*  @returns  Success(0) or failure(-1).
*/
int comm_ipcStreamClientConnect(
    tIpcStreamClientHandle  handle,
    char                   *pFileName
)
{
    tIpcStreamClientContext *pContext = (tIpcStreamClientContext *)handle;
    struct sockaddr_un servAddr;
    int servAddrLen;
    int error;


    if (NULL == pContext)
    {
        LOG_ERROR("%s: pContext is NULL\n", __func__);
        return -1;
    }

    if (pContext->fd < 0)
    {
        LOG_ERROR("%s: %s is not ready\n", __func__, pContext->localPath);
        return -1;
    }

    LOG_3("connect to %s\n", pFileName);
    strcpy(pContext->remotePath, pFileName);

    servAddrLen = sizeof(struct sockaddr_un);
    memset(&servAddr, 0x00, servAddrLen);
    servAddr.sun_family = AF_UNIX;
    strcpy(servAddr.sun_path, pFileName);

    error = connect(
                pContext->fd,
                (struct sockaddr *)&servAddr,
                servAddrLen
            );
    if (error < 0)
    {
        LOG_ERROR("fail to connect IPC stream server\n");
        perror( "connect" );
        return -1;
    }

    pContext->running = 1;

    error = pthread_create(
                &(pContext->thread),
                NULL,
                _ipcStreamClientRecvTask,
                pContext
            );
    if (error != 0)
    {
        LOG_ERROR("fail to create IPC stream receiving thread\n");
        perror( "pthread_create" );
        return -1;
    }

    return 0;
}

/**
*  Send message to an IPC stream server.
*  @param [in]  handle  IPC stream client handle.
*  @param [in]  pData   A pointer of data buffer.
*  @param [in]  size    Data size.
*  @returns  Message length (-1 is failed).
*/
int comm_ipcStreamClientSend(
    tIpcStreamClientHandle  handle,
    unsigned char          *pData,
    unsigned short          size
)
{
    tIpcStreamClientContext *pContext = (tIpcStreamClientContext *)handle;
    int error;


    if (NULL == pContext)
    {
        LOG_ERROR("%s: pContext is NULL\n", __func__);
        return -1;
    }

    if (pContext->fd < 0)
    {
        LOG_ERROR("%s: %s is not ready\n", __func__, pContext->localPath);
        return -1;
    }

    if (NULL == pData)
    {
        LOG_WARN("%s: pData is NULL\n", __func__);
        return -1;
    }

    if (0 == size)
    {
        LOG_WARN("%s: size is 0\n", __func__);
        return -1;
    }

    LOG_3("-> %s\n", pContext->remotePath);
    LOG_DUMP("IPC stream client send", pData, size);

    error = send(
                pContext->fd,
                pData,
                size,
                0
            );
    if (error < 0)
    {
        LOG_ERROR("fail to send IPC stream\n");
        perror( "send" );
    }

    return error;
}



#define IPC_USER_NUM (32)

typedef struct _tIpcStreamServerContext
{
    char              localPath[256];
    int               fd;

    tIpcUser         *pUser[IPC_USER_NUM];
    int               userNum;
    int               maxUserNum;

    tIpcServerAcptCb  pServerAcptFunc;
    tIpcServerExitCb  pServerExitFunc;
    tIpcServerRecvCb  pServerRecvFunc;
    void             *pServerArg;
    pthread_t         thread;
    int               running;

    unsigned char     recvMsg[COMM_BUF_SIZE+1];
} tIpcStreamServerContext;

static tIpcUser *_ipcStreamAcceptClient(
    tIpcStreamServerContext *pContext,
    char                    *pFileName,
    int                      fd
);
static void _ipcStreamDisconnectClient(
    tIpcStreamServerContext *pContext,
    tIpcUser                *pUser
);


/**
*  Initialize a stream UNIX domain socket.
*  @param [in]  pContext  A @ref tIpcStreamServerContext object.
*  @returns  Success(0) or failure(-1).
*/
static int _ipcStreamInitServer(tIpcStreamServerContext *pContext)
{
    struct sockaddr_un bindAddr;
    socklen_t bindAddrLen;
    int fd;


    unlink( pContext->localPath );

    fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (fd < 0)
    {
        perror( "socket" );
        return -1;
    }

    bindAddrLen = sizeof( struct sockaddr_un );
    memset(&bindAddr, 0x00, bindAddrLen);
    bindAddr.sun_family = AF_UNIX;
    strcpy(bindAddr.sun_path, pContext->localPath);

    if (bind(fd, (struct sockaddr *)&bindAddr, bindAddrLen) < 0)
    {
        perror( "bind" );
        close( fd );
        return -1;
    }

    pContext->fd = fd;

    LOG_2("IPC %s is ready\n", pContext->localPath);
    return 0;
}

/**
*  Un-initialize a stream UNIX domain socket.
*  @param [in]  pContext  A @ref tIpcStreamServerContext object.
*/
static void _ipcStreamUninitServer(tIpcStreamServerContext *pContext)
{
    if (pContext->fd > 0)
    {
        close( pContext->fd );
        pContext->fd = -1;
    }

    unlink( pContext->localPath );

    LOG_2("IPC %s is closed\n", pContext->localPath);
}

/**
*  Thread function for the IPC stream server receiving.
*  @param [in]  pArg  A @ref tIpcUser object.
*/
static void *_ipcStreamServerRecvTask(void *pArg)
{
    tIpcStreamServerContext *pContext;
    tIpcUser *pUser = pArg;
    int len;


    LOG_2("start the thread: %s\n", __func__);

    pContext = pUser->pServer;

    while (pUser->fd > 0)
    {
        LOG_3("%s ... recv\n", pUser->fileName);
        len = recv(
                  pUser->fd,
                  pUser->recvMsg,
                  COMM_BUF_SIZE,
                  0
              );
        if (len < 0)
        {
            LOG_1(
                "IPC client %s connection closed\n",
                pUser->fileName
            );
            close( pUser->fd );
            pUser->fd = -1;
            /* notify the server that client is closed */
            if ( pContext->pServerExitFunc )
            {
                pContext->pServerExitFunc(pContext->pServerArg, pUser);
            }
            break;
        }

        LOG_3("<- %s\n", pUser->fileName);
        LOG_DUMP("IPC stream server recv", pUser->recvMsg, len);

        if ( pContext->pServerRecvFunc )
        {
            pContext->pServerRecvFunc(
                          pContext->pServerArg,
                          pUser,
                          pUser->recvMsg,
                          len
                      );
        }
    }

    LOG_2("stop the thread: %s\n", __func__);

    _ipcStreamDisconnectClient(pContext, pUser);

    return NULL;
}

/**
*  Thread function for the IPC stream server listen.
*  @param [in]  pArg  A @ref tIpcStreamServerContext object.
*/
static void *_ipcStreamServerListenTask(void *pArg)
{
    tIpcStreamServerContext *pContext = pArg;
    struct sockaddr_un clitAddr;
    socklen_t clitAddrLen = sizeof( struct sockaddr_un );


    LOG_2("start the thread: %s\n", __func__);

    if (listen(pContext->fd, pContext->maxUserNum) < 0)
    {
        perror( "listen" );
        close( pContext->fd );
        pContext->fd = -1;
        return NULL;
    }

    LOG_1("\n");
    LOG_1("File name : %s\n", pContext->localPath);
    LOG_1("User limit: %d\n", pContext->maxUserNum);
    LOG_1("IPC stream server ... listen\n");
    LOG_1("\n");

    while ( pContext->running )
    {
        tIpcUser *pUser;
        int fd;

        LOG_3("%s ... accept\n", pContext->localPath);
        fd = accept(
                 pContext->fd,
                 (struct sockaddr *)&clitAddr,
                 &clitAddrLen
             );
        if (fd < 0)
        {
            LOG_ERROR("fail to receive IPC stream client\n");
            perror( "accept" );
            break;
        }

        LOG_1("IPC stream client connect from %s\n", clitAddr.sun_path);

        pUser = _ipcStreamAcceptClient(pContext, clitAddr.sun_path, fd);
        if (NULL == pUser)
        {
            LOG_ERROR("fail to accept client\n");
            close( fd );
        }
    }

    LOG_2("stop the thread: %s\n", __func__);
    pContext->running = 0;

    return NULL;
}

/**
*  Initialize IPC stream server.
*  @param [in]  pFileName   Application's socket file name.
*  @param [in]  maxUserNum  Max. user number.
*  @param [in]  pAcptFunc   Application's accept callback function.
*  @param [in]  pExitFunc   Application's exit callback function.
*  @param [in]  pRecvFunc   Application's receive callback function.
*  @param [in]  pArg        Application's argument.
*  @returns  IPC stream server handle.
*/
tIpcStreamServerHandle comm_ipcStreamInitServer(
    char             *pFileName,
    int               maxUserNum,
    tIpcServerAcptCb  pAcptFunc,
    tIpcServerExitCb  pExitFunc,
    tIpcServerRecvCb  pRecvFunc,
    void             *pArg
)
{
    tIpcStreamServerContext *pContext = NULL;
    int error;


    pContext = malloc( sizeof( tIpcStreamServerContext ) );
    if (NULL == pContext)
    {
        LOG_ERROR("fail to allocate IPC stream server context\n");
        return 0;
    }

    memset(pContext, 0x00, sizeof( tIpcStreamServerContext ));
    strncpy(pContext->localPath, pFileName, 255);
    pContext->maxUserNum = maxUserNum;
    pContext->pServerAcptFunc = pAcptFunc;
    pContext->pServerExitFunc = pExitFunc;
    pContext->pServerRecvFunc = pRecvFunc;
    pContext->pServerArg = pArg;
    pContext->fd = -1;

    if ((0 == pContext->maxUserNum) || (pContext->maxUserNum > IPC_USER_NUM))
    {
        LOG_1("set user number to the max. value %d\n", IPC_USER_NUM);
        pContext->maxUserNum = IPC_USER_NUM;
    }

    error = _ipcStreamInitServer( pContext );
    if (error != 0)
    {
        LOG_ERROR("fail to create IPC stream server\n");
        LOG_ERROR("path: %s\n", pFileName);
        free( pContext );
        return 0;
    }

    if (NULL == pAcptFunc)
    {
        LOG_1("ignore IPC stream accept function\n");
    }

    if (NULL == pExitFunc)
    {
        LOG_1("ignore IPC stream exit function\n");
    }

    if (NULL == pRecvFunc)
    {
        LOG_1("ignore IPC stream receive function\n");
    }

    pContext->running = 1;

    error = pthread_create(
                &(pContext->thread),
                NULL,
                _ipcStreamServerListenTask,
                pContext
            );
    if (error != 0)
    {
        LOG_ERROR("fail to create IPC stream receiving thread\n");
        _ipcStreamUninitServer( pContext );
        free( pContext );
        return 0;
    }

    LOG_1("IPC stream server initialized\n");
    return ((tIpcStreamServerHandle)pContext);
}

/**
*  Un-initialize IPC stream server.
*  @param [in]  handle  IPC stream server handle.
*/
void comm_ipcStreamUninitServer(tIpcStreamServerHandle handle)
{
    tIpcStreamServerContext *pContext = (tIpcStreamServerContext *)handle;
    int i;

    if ( pContext )
    {
        if ( pContext->running )
        {
            pContext->running = 0;
            pthread_cancel( pContext->thread );
            pthread_join(pContext->thread, NULL);
            usleep(1000);
        }

        pContext->userNum = 0;
        for (i=0; i<pContext->maxUserNum; i++)
        {
            _ipcStreamDisconnectClient(pContext, pContext->pUser[i]);
        }

        _ipcStreamUninitServer( pContext );

        free( pContext );
        LOG_1("IPC stream server un-initialized\n");
    }
}

/**
*  Create the connection to IPC stream client.
*  @param [in]  pContext   A @ref tIpcStreamServerContext object.
*  @param [in]  pFileName  Client's socket file name.
*  @param [in]  fd         Socket file descriptor.
*  @returns  A @ref tIpcUser object.
*/
static tIpcUser *_ipcStreamAcceptClient(
    tIpcStreamServerContext *pContext,
    char                    *pFileName,
    int                      fd
)
{
    tIpcUser *pUser = NULL;
    int bufSize = 0;
    socklen_t bufSizeLen;
    int error;
    int i;


    if (pContext->userNum >= pContext->maxUserNum)
    {
        LOG_ERROR("user number was exceeded (%d)\n", pContext->maxUserNum);
        return NULL;
    }

    for (i=0; i<pContext->maxUserNum; i++)
    {
        if (NULL == pContext->pUser[i])
        {
            pUser = malloc( sizeof( tIpcUser ) );

            if ( pUser )
            {
                LOG_3("IPC stream create client %s\n", pFileName);

                bufSizeLen = sizeof( bufSize );
                setsockopt(fd, SOL_SOCKET, SO_SNDBUF, &bufSize, bufSizeLen);

                memset(pUser, 0x00, sizeof( tIpcUser ));
                pUser->pServer = pContext;
                strcpy(pUser->fileName, pFileName);
                pUser->fd = fd;

                error = pthread_create(
                            &(pUser->thread),
                            NULL,
                            _ipcStreamServerRecvTask,
                            pUser
                        );
                if (error != 0)
                {
                    LOG_ERROR("failed to create the client connection thread\n");
                    free( pUser );
                    return NULL;
                }

                /* notify the client object to the server application */
                if ( pContext->pServerAcptFunc )
                {
                    pContext->pServerAcptFunc(pContext->pServerArg, pUser);
                }

                pContext->pUser[i] = pUser;
                pContext->userNum++;
            }

            return pUser;
        }
    }

    LOG_ERROR("user memory was exhausted\n");
    return NULL;
}

/**
*  Remove the connection of IPC stream client.
*  @param [in]  pContext  A @ref tIpcStreamServerContext object.
*  @param [in]  pUser     A @ref tIpcUser object.
*/
static void _ipcStreamDisconnectClient(
    tIpcStreamServerContext *pContext,
    tIpcUser                *pUser
)
{
    int i;

    if ( pUser )
    {
        LOG_3("IPC stream remove client %s\n", pUser->fileName);

        for (i=0; i<pContext->maxUserNum; i++)
        {
            if (pContext->pUser[i] == pUser)
            {
                pContext->pUser[i] = NULL;
                if (pContext->userNum > 0)
                {
                    pContext->userNum--;
                }
                break;
            }
        }

        if (pUser->fd > 0)
        {
            close( pUser->fd );
            pUser->fd = -1;

            pthread_cancel( pUser->thread );
            pthread_join(pUser->thread, NULL);
        }

        free( pUser );
    }
}

/**
*  Send message to IPC stream client.
*  @param [in]  pUser  A @ref tIpcUser object.
*  @param [in]  pData  A pointer of data buffer.
*  @param [in]  size   Data size.
*  @returns  Message length (-1 is failed).
*/
int comm_ipcStreamServerSend(
    tIpcUser       *pUser,
    unsigned char  *pData,
    unsigned short  size
)
{
    int error;


    if (NULL == pUser)
    {
        LOG_ERROR("%s: pUser is NULL\n", __func__);
        return -1;
    }

    if (pUser->fd < 0)
    {
        LOG_ERROR("%s: %s is not ready\n", __func__, pUser->fileName);
        return -1;
    }

    if (NULL == pData)
    {
        LOG_WARN("%s: pData is NULL\n", __func__);
        return -1;
    }

    if (0 == size)
    {
        LOG_WARN("%s: size is 0\n", __func__);
        return -1;
    }

    LOG_3("-> %s\n", pUser->fileName);
    LOG_DUMP("IPC stream server send", pData, size);

    error = send(
                pUser->fd,
                pData,
                size,
                0
            );
    if (error < 0)
    {
        LOG_ERROR("fail to send IPC stream\n");
        perror( "send" );
    }

    return error;
}

/**
*  Send message to all IPC stream clients.
*  @param [in]  handle  IPC stream server handle.
*  @param [in]  pData   A pointer of data buffer.
*  @param [in]  size    Data size.
*/
void comm_ipcStreamServerSendAllClient(
    tIpcStreamServerHandle  handle,
    unsigned char          *pData,
    unsigned short          size
)
{
    tIpcStreamServerContext *pContext = (tIpcStreamServerContext *)handle;
    tIpcUser *pUser;
    int error;
    int i;


    if (0 == pContext->userNum)
    {
        LOG_WARN("%s: user number is 0\n", __func__);
        return;
    }

    if (NULL == pData)
    {
        LOG_WARN("%s: pData is NULL\n", __func__);
        return;
    }

    if (0 == size)
    {
        LOG_WARN("%s: size is 0\n", __func__);
        return;
    }

    LOG_DUMP("IPC stream send to all clients", pData, size);

    for (i=0; i<pContext->maxUserNum; i++)
    {
        pUser = pContext->pUser[i];

        if (( pUser ) && (pUser->fd > 0))
        {
            error = send(
                        pUser->fd,
                        pData,
                        size,
                        0
                    );
            if (error < 0)
            {
                LOG_ERROR("fail to send IPC stream to fd(%d)\n", pUser->fd);
                perror( "send" );
            }
        }
    }
}

/**
*  Get the number of connected IPC stream clients.
*  @param [in]  handle  IPC stream server handle.
*  @returns  Number of clients.
*/
int comm_ipcStreamServerGetClientNum(tIpcStreamServerHandle handle)
{
    tIpcStreamServerContext *pContext = (tIpcStreamServerContext *)handle;
    return pContext->userNum;
}

