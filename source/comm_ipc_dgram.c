#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <pthread.h>
#include <sys/socket.h>
#include <sys/un.h>
#include "comm_if.h"
#include "comm_log.h"


typedef struct _tIpcDgramContext
{
    char             localPath[256];
    int              fd;

    tIpcDgramRecvCb  pRecvFunc;
    void            *pArg;
    pthread_t        thread;
    int              running;

    unsigned char    recvMsg[COMM_BUF_SIZE+1];
} tIpcDgramContext;


/**
*  Initialize a datagram UNIX domain socket.
*  @param [in]  pContext  A @ref tIpcDgramContext object.
*  @returns  Success(0) or failure(-1).
*/
static int _ipcDgramInit(tIpcDgramContext *pContext)
{
    struct sockaddr_un bindAddr;
    socklen_t bindAddrLen;
    int fd;


    unlink( pContext->localPath );

    fd = socket(AF_UNIX, SOCK_DGRAM, 0);
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
*  Un-initialize a datagram UNIX domain socket.
*  @param [in]  pContext  A @ref tIpcDgramContext object.
*/
static void _ipcDgramUninit(tIpcDgramContext *pContext)
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
*  Thread function for IPC datagram receiving.
*  @param [in]  pArg  A @ref tIpcDgramContext object.
*/
static void *_ipcDgramRecvTask(void *pArg)
{
    tIpcDgramContext *pContext = pArg;
    struct sockaddr_un recvAddr;
    socklen_t recvAddrLen;
    int len;


    pthread_setcanceltype(PTHREAD_CANCEL_DEFERRED, NULL);
    LOG_2("start the thread: %s\n", __func__);

    /* address for the source app */
    recvAddrLen = sizeof( struct sockaddr_un );
    memset(&recvAddr, 0x00, recvAddrLen);

    while ( pContext->running )
    {
        LOG_3("%s ... recvfrom\n", pContext->localPath);
        pthread_testcancel();
        len = recvfrom(
                  pContext->fd,
                  pContext->recvMsg,
                  COMM_BUF_SIZE,
                  0,
                  (struct sockaddr *)(&recvAddr),
                  &recvAddrLen
              );
        if (len < 0)
        {
            LOG_ERROR("fail to receive IPC datagram\n");
            perror( "recvfrom" );
            break;
        }
        pthread_testcancel();

        LOG_3("<- %s\n", recvAddr.sun_path);
        LOG_DUMP("IPC datagram recv", pContext->recvMsg, len);

        if ( pContext->pRecvFunc )
        {
            pContext->pRecvFunc(
                pContext->pArg,
                pContext->recvMsg,
                len,
                recvAddr.sun_path
            );
        }
    }

    LOG_2("stop the thread: %s\n", __func__);
    pContext->running = 0;

    pthread_exit(NULL);
}

/**
*  Initialize IPC datagram.
*  @param [in]  pFileName  Application's socket file name.
*  @param [in]  pRecvFunc  Application's receive callback function.
*  @param [in]  pArg       Application's argument.
*  @returns  IPC datagram handle.
*/
tIpcDgramHandle comm_ipcDgramInit(
    char            *pFileName,
    tIpcDgramRecvCb  pRecvFunc,
    void            *pArg
)
{
    tIpcDgramContext *pContext = NULL;
    pthread_attr_t tattr;
    int error;


    pContext = malloc( sizeof( tIpcDgramContext ) );
    if (NULL == pContext)
    {
        LOG_ERROR("fail to allocate IPC datagram context\n");
        return 0;
    }

    memset(pContext, 0x00, sizeof( tIpcDgramContext ));
    strncpy(pContext->localPath, pFileName, 255);
    pContext->pRecvFunc = pRecvFunc;
    pContext->pArg = pArg;
    pContext->fd = -1;

    error = _ipcDgramInit( pContext );
    if (error != 0)
    {
        LOG_ERROR("fail to create IPC datagram socket\n");
        LOG_ERROR("path: %s\n", pFileName);
        free( pContext );
        return 0;
    }

    if (NULL == pRecvFunc)
    {
        LOG_1("ignore IPC receive function\n");
        goto _DONE;
    }

    pContext->running = 1;

    pthread_attr_init( &tattr );
    pthread_attr_setdetachstate(&tattr, PTHREAD_CREATE_JOINABLE);

    error = pthread_create(
                &(pContext->thread),
                &tattr,
                _ipcDgramRecvTask,
                pContext
            );
    if (error != 0)
    {
        LOG_ERROR("fail to create IPC receiving thread\n");
        _ipcDgramUninit( pContext );
        free( pContext );
        return 0;
    }

    pthread_attr_destroy( &tattr );

_DONE:
    LOG_1("IPC datagram initialized\n");
    return ((tIpcDgramHandle)pContext);
}

/**
*  Un-initialize IPC datagram.
*  @param [in]  handle  IPC datagram handle.
*/
void comm_ipcDgramUninit(tIpcDgramHandle handle)
{
    tIpcDgramContext *pContext = (tIpcDgramContext *)handle;

    if ( pContext )
    {
        if ( pContext->pRecvFunc )
        {
            pthread_cancel( pContext->thread );
        }

        pContext->running = 0;
        _ipcDgramUninit( pContext );

        if ( pContext->pRecvFunc )
        {
            pthread_join(pContext->thread, NULL);
        }

        free( pContext );
        LOG_1("IPC datagram un-initialized\n");
    }
}

/**
*  Send message from an application to another.
*  @param [in]  handle     IPC datagram handle.
*  @param [in]  pFileName  Destination application's socket file name.
*  @param [in]  pData      A pointer of data buffer.
*  @param [in]  size       Data size.
*  @returns  Message length (-1 is failed).
*/
int comm_ipcDgramSend(
    tIpcDgramHandle  handle,
    char            *pFileName,
    unsigned char   *pData,
    unsigned short   size
)
{
    tIpcDgramContext *pContext = (tIpcDgramContext *)handle;
    struct sockaddr_un sendAddr;
    socklen_t destAddrLen;
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

    LOG_3("-> %s\n", pFileName);
    LOG_DUMP("IPC datagram send", pData, size);

    destAddrLen = sizeof( struct sockaddr_un );
    memset(&sendAddr, 0x00, destAddrLen);
    sendAddr.sun_family = AF_UNIX;
    strcpy(sendAddr.sun_path, pFileName);

    error = sendto(
                pContext->fd,
                pData,
                size,
                0,
                (struct sockaddr *)(&sendAddr),
                destAddrLen
            );
    if (error < 0)
    {
        LOG_ERROR("fail to send IPC datagram\n");
        perror( "sendto" );
    }

    return error;
}

/**
*  Receive message from an application to another.
*  @param [in]  handle  IPC datagram handle.
*  @param [in]  pData   A pointer of data buffer.
*  @param [in]  size    Data buffer size.
*  @returns  Message length (-1 is failed).
*/
int comm_ipcDgramRecv(
    tIpcDgramHandle  handle,
    unsigned char   *pData,
    unsigned short   size
)
{
    tIpcDgramContext *pContext = (tIpcDgramContext *)handle;
    struct sockaddr_un recvAddr;
    socklen_t recvAddrLen;
    int len;


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

    if ( pContext->pRecvFunc )
    {
        LOG_WARN("%s: pRecvFunc exists\n", __func__);
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

    /* address for the source app */
    recvAddrLen = sizeof( struct sockaddr_un );
    memset(&recvAddr, 0x00, recvAddrLen);

    len = recvfrom(
              pContext->fd,
              pData,
              size,
              0,
              (struct sockaddr *)(&recvAddr),
              &recvAddrLen
          );
    if (len < 0)
    {
        LOG_ERROR("fail to receive IPC datagram\n");
        perror( "recvfrom" );
        return len;
    }

    LOG_3("<- %s\n", recvAddr.sun_path);
    LOG_DUMP("IPC datagram recv", pData, len);

    return len;
}

