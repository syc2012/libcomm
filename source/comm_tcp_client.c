#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <pthread.h>
#include <sys/socket.h>
#include <ifaddrs.h>
#include "comm_if.h"
#include "comm_log.h"


typedef struct _tTcpIpv4ClientContext
{
    struct sockaddr_in  remoteAddr;
    struct sockaddr_in  localAddr;
    int                 fd;

    tTcpClientRecvCb    pClientRecvFunc;
    tTcpClientExitCb    pClientExitFunc;
    void               *pClientArg;
    pthread_t           thread;
    int                 running;

    unsigned char       recvMsg[COMM_BUF_SIZE+1];
} tTcpIpv4ClientContext;


/**
*  Initialize an IPv4 TCP client socket.
*  @param [in]  pContext  A @ref tTcpIpv4ClientContext object.
*  @returns  Success(0) or failure(-1).
*/
static int _tcpIpv4InitClient(tTcpIpv4ClientContext *pContext)
{
    struct sockaddr_in bindAddr;
    socklen_t bindAddrLen;
    int fd;

    int reUseAddr = 1;
    socklen_t reUseAddrLen;


    fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0)
    {
        perror( "socket" );
        return -1;
    }

    /* enable the port number re-use */
    reUseAddrLen = sizeof( reUseAddr );
    setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &reUseAddr, reUseAddrLen);

    bindAddrLen = sizeof( struct sockaddr_in );
    bindAddr = pContext->localAddr;

    if (bind(fd, (struct sockaddr *)&bindAddr, bindAddrLen) < 0)
    {
        perror( "bind" );
        close( fd );
        return -1;
    }

    pContext->fd = fd;

    LOG_2("IPv4 TCP client socket is ready\n");
    return 0;
}

/**
*  Un-initialize an IPv4 TCP client socket.
*  @param [in]  pContext  A @ref tTcpIpv4ClientContext object.
*/
static void _tcpIpv4UninitClient(tTcpIpv4ClientContext *pContext)
{
    if (pContext->fd > 0)
    {
        close( pContext->fd );
        pContext->fd = -1;
    }

    LOG_2("IPv4 TCP client socket is closed\n");
}

/**
*  Thread function for IPv4 TCP client receiving.
*  @param [in]  pArg  A @ref tTcpIpv4ClientContext object.
*/
static void *_tcpIpv4ClientRecvTask(void *pArg)
{
    tTcpIpv4ClientContext *pContext = pArg;
    int len;


    LOG_2("start the thread: %s\n", __func__);

    while ( pContext->running )
    {
        LOG_3("IPv4 TCP client ... recv\n");
        len = recv(
                  pContext->fd,
                  pContext->recvMsg,
                  COMM_BUF_SIZE,
                  0
              );
        if (len <= 0)
        {
            LOG_ERROR("IPv4 TCP server was terminated\n");
            close( pContext->fd );
            pContext->fd = -1;
            /* notify the client that server is closed */
            if ( pContext->pClientExitFunc )
            {
                pContext->pClientExitFunc(pContext->pClientArg, len);
            }
            break;
        }

        LOG_3("<- IPv4 TCP server\n");
        LOG_DUMP("IPv4 TCP client recv", pContext->recvMsg, len);

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
*  Initialize IPv4 TCP client.
*  @param [in]  portNum    Local TCP port number.
*  @param [in]  pRecvFunc  Application's receive callback function.
*  @param [in]  pExitFunc  Application's exit callback function.
*  @param [in]  pArg       Application's argument.
*  @returns  IPv4 TCP client handle.
*/
tTcpIpv4ClientHandle comm_tcpIpv4ClientInit(
    unsigned short    portNum,
    tTcpClientRecvCb  pRecvFunc,
    tTcpClientExitCb  pExitFunc,
    void             *pArg
)
{
    tTcpIpv4ClientContext *pContext = NULL;
    int error;


    pContext = malloc( sizeof( tTcpIpv4ClientContext ) );
    if (NULL == pContext)
    {
        LOG_ERROR("fail to allocate TCP IPv4 client context\n");
        return 0;
    }

    memset(pContext, 0x00, sizeof( tTcpIpv4ClientContext ));
    pContext->localAddr.sin_family      = AF_INET;
    pContext->localAddr.sin_port        = htons( portNum );
    pContext->localAddr.sin_addr.s_addr = htonl( INADDR_ANY );
    pContext->pClientRecvFunc = pRecvFunc;
    pContext->pClientExitFunc = pExitFunc;
    pContext->pClientArg = pArg;
    pContext->fd = -1;

    error = _tcpIpv4InitClient( pContext );
    if (error != 0)
    {
        LOG_ERROR("fail to create IPv4 TCP socket\n");
        free( pContext );
        return 0;
    }

    if (NULL == pRecvFunc)
    {
        LOG_1("ignore IPv4 TCP receive function\n");
    }

    if (NULL == pExitFunc)
    {
        LOG_1("ignore IPv4 TCP exit function\n");
    }

    LOG_1("IPv4 TCP client initialized\n");
    return ((tTcpIpv4ClientHandle)pContext);
}

/**
*  Un-initialize IPv4 TCP client.
*  @param [in]  handle  IPv4 TCP client handle.
*/
void comm_tcpIpv4ClientUninit(tTcpIpv4ClientHandle handle)
{
    tTcpIpv4ClientContext *pContext = (tTcpIpv4ClientContext *)handle;

    if ( pContext )
    {
        if ( pContext->running )
        {
            pContext->running = 0;
            pthread_cancel( pContext->thread );
            pthread_join(pContext->thread, NULL);
            usleep(1000);
        }

        _tcpIpv4UninitClient( pContext );

        free( pContext );
        LOG_1("IPv4 TCP client un-initialized\n");
    }
}

/**
*  Connect to an IPv4 TCP server.
*  @param [in]  handle  IPv4 TCP client handle.
*  @param [in]  pAddr   Server address string.
*  @param [in]  port    Server port number.
*  @returns  Success(0) or failure(-1).
*/
int comm_tcpIpv4ClientConnect(
    tTcpIpv4ClientHandle  handle,
    char                 *pAddr,
    unsigned short        port
)
{
    tTcpIpv4ClientContext *pContext = (tTcpIpv4ClientContext *)handle;
    struct sockaddr_in servAddr;
    int servAddrLen;
    int error;


    if (NULL == pContext)
    {
        LOG_ERROR("%s: pContext is NULL\n", __func__);
        return -1;
    }

    if (pContext->fd < 0)
    {
        LOG_ERROR("%s: socket is not ready\n", __func__);
        return -1;
    }

    LOG_3("connect to %s:%d\n", pAddr, port);

    /*
    * Convert IPv4 address from string to 4-byte integer:
    *   in_addr_t inet_addr(const char *cp);
    */
    servAddrLen = sizeof(struct sockaddr_in);
    bzero(&servAddr, servAddrLen);
    servAddr.sin_family = AF_INET;
    servAddr.sin_port   = htons( port );
    servAddr.sin_addr.s_addr = inet_addr( pAddr );

    pContext->remoteAddr = servAddr;

    error = connect(
                pContext->fd,
                (struct sockaddr *)&servAddr,
                servAddrLen
            );
    if (error < 0)
    {
        LOG_ERROR("fail to connect IPv4 TCP server\n");
        perror( "connect" );
        return -1;
    }

    pContext->running = 1;

    error = pthread_create(
                &pContext->thread,
                NULL,
                _tcpIpv4ClientRecvTask,
                pContext
            );
    if (error != 0)
    {
        LOG_ERROR("fail to create IPv4 TCP receiving thread\n");
        perror( "pthread_create" );
        return -1;
    }

    return 0;
}

/**
*  Send message to an IPv4 TCP server.
*  @param [in]  handle  IPv4 TCP client handle.
*  @param [in]  pData   A pointer of data buffer.
*  @param [in]  size    Data size.
*  @returns  Message length (-1 is failed).
*/
int comm_tcpIpv4ClientSend(
    tTcpIpv4ClientHandle  handle,
    unsigned char        *pData,
    unsigned short        size
)
{
    tTcpIpv4ClientContext *pContext = (tTcpIpv4ClientContext *)handle;
    int error;


    if (NULL == pContext)
    {
        LOG_ERROR("%s: pContext is NULL\n", __func__);
        return -1;
    }

    if (pContext->fd < 0)
    {
        LOG_ERROR("%s: socket is not ready\n", __func__);
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

    LOG_3("-> IPv4 TCP server\n");
    LOG_DUMP("IPv4 TCP client send", pData, size);

    error = send(
                pContext->fd,
                pData,
                size,
                0
            );
    if (error < 0)
    {
        LOG_ERROR("fail to send IPv4 TCP server\n");
        perror( "send" );
    }

    return error;
}


typedef struct _tTcpIpv6ClientContext
{
    struct sockaddr_in6  remoteAddr;
    struct sockaddr_in6  localAddr;
    int                  fd;

    tTcpClientRecvCb     pClientRecvFunc;
    tTcpClientExitCb     pClientExitFunc;
    void                *pClientArg;
    pthread_t            thread;
    int                  running;

    unsigned char        recvMsg[COMM_BUF_SIZE+1];
} tTcpIpv6ClientContext;


/**
*  Initialize an IPv6 TCP client socket.
*  @param [in]  pContext  A @ref tTcpIpv6ClientContext object.
*  @returns  Success(0) or failure(-1).
*/
static int _tcpIpv6InitClient(tTcpIpv6ClientContext *pContext)
{
    struct sockaddr_in6 bindAddr;
    int bindAddrLen;
    int fd;

    int reUseAddr = 1;
    socklen_t reUseAddrLen;


    fd = socket(AF_INET6, SOCK_STREAM, 0);
    if (fd < 0)
    {
        perror( "socket" );
        return -1;
    }

    /* enable the port number re-use */
    reUseAddrLen = sizeof( reUseAddr );
    setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &reUseAddr, reUseAddrLen);

    bindAddrLen = sizeof( struct sockaddr_in6 );
    bindAddr = pContext->localAddr;

    if (bind(fd, (struct sockaddr *)&bindAddr, bindAddrLen) < 0)
    {
        perror( "bind" );
        close( fd );
        return -1;
    }

    pContext->fd = fd;

    LOG_2("IPv6 TCP client socket is ready\n");
    return 0;
}

/**
*  Un-initialize an IPv6 TCP client socket.
*  @param [in]  pContext  A @ref tTcpIpv6ClientContext object.
*/
static void _tcpIpv6UninitClient(tTcpIpv6ClientContext *pContext)
{
    if (pContext->fd > 0)
    {
        close( pContext->fd );
        pContext->fd = -1;
    }

    LOG_2("IPv6 TCP client socket is closed\n");
}

/**
*  Thread function for IPv6 TCP client receiving.
*  @param [in]  pArg  A @ref tTcpIpv6ClientContext object.
*/
static void *_tcpIpv6ClientRecvTask(void *pArg)
{
    tTcpIpv6ClientContext *pContext = pArg;
    int len;


    LOG_2("start the thread: %s\n", __func__);

    while ( pContext->running )
    {
        LOG_3("IPv6 TCP client ... recv\n");
        len = recv(
                  pContext->fd,
                  pContext->recvMsg,
                  COMM_BUF_SIZE,
                  0
              );
        if (len <= 0)
        {
            LOG_ERROR("IPv6 TCP server was terminated\n");
            close( pContext->fd );
            pContext->fd = -1;
            /* notify the client that server is closed */
            if ( pContext->pClientExitFunc )
            {
                pContext->pClientExitFunc(pContext->pClientArg, len);
            }
            break;
        }

        LOG_3("<- IPv6 TCP server\n");
        LOG_DUMP("IPv6 TCP client recv", pContext->recvMsg, len);

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
*  Initialize IPv6 TCP client.
*  @param [in]  portNum    Local TCP port number.
*  @param [in]  pRecvFunc  Application's receive callback function.
*  @param [in]  pExitFunc  Application's exit callback function.
*  @param [in]  pArg       Application's argument.
*  @returns  IPv6 TCP client handle.
*/
tTcpIpv6ClientHandle comm_tcpIpv6ClientInit(
    unsigned short    portNum,
    tTcpClientRecvCb  pRecvFunc,
    tTcpClientExitCb  pExitFunc,
    void             *pArg
)
{
    tTcpIpv6ClientContext *pContext = NULL;
    int error;


    pContext = malloc( sizeof( tTcpIpv6ClientContext ) );
    if (NULL == pContext)
    {
        LOG_ERROR("fail to allocate TCP IPv6 client context\n");
        return 0;
    }

    memset(pContext, 0x00, sizeof( tTcpIpv6ClientContext ));
    pContext->localAddr.sin6_family = AF_INET6;
    pContext->localAddr.sin6_port   = htons( portNum );
    pContext->localAddr.sin6_addr   = in6addr_any;
    pContext->pClientRecvFunc = pRecvFunc;
    pContext->pClientExitFunc = pExitFunc;
    pContext->pClientArg = pArg;
    pContext->fd = -1;

    error = _tcpIpv6InitClient( pContext );
    if (error != 0)
    {
        LOG_ERROR("fail to create an IPv6 TCP socket\n");
        free( pContext );
        return 0;
    }

    if (NULL == pRecvFunc)
    {
        LOG_1("ignore IPv6 TCP receive function\n");
    }

    if (NULL == pExitFunc)
    {
        LOG_1("ignore IPv6 TCP exit function\n");
    }

    LOG_1("IPv6 TCP client initialized\n");
    return ((tTcpIpv6ClientHandle)pContext);;
}

/**
*  Un-initialize IPv6 TCP client.
*  @param [in]  handle  IPv6 TCP client handle.
*/
void comm_tcpIpv6ClientUninit(tTcpIpv6ClientHandle handle)
{
    tTcpIpv6ClientContext *pContext = (tTcpIpv6ClientContext *)handle;

    if ( pContext )
    {
        if ( pContext->running )
        {
            pContext->running = 0;
            pthread_cancel( pContext->thread );
            pthread_join(pContext->thread, NULL);
            usleep(1000);
        }

        _tcpIpv6UninitClient( pContext );

        free( pContext );
        LOG_1("IPv6 TCP client un-initialized\n");
    }
}

/**
*  Connect to an IPv6 TCP server.
*  @param [in]  handle  IPv6 TCP client handle.
*  @param [in]  pAddr   Server address string.
*  @param [in]  port    Server port number.
*  @returns  Success(0) or failure(-1).
*/
int comm_tcpIpv6ClientConnect(
    tTcpIpv6ClientHandle  handle,
    char                 *pAddr,
    unsigned short        port
)
{
    tTcpIpv6ClientContext *pContext = (tTcpIpv6ClientContext *)handle;
    struct sockaddr_in6 servAddr;
    int servAddrLen;
    int error;


    if (NULL == pContext)
    {
        LOG_ERROR("%s: pContext is NULL\n", __func__);
        return -1;
    }

    if (pContext->fd < 0)
    {
        LOG_ERROR("%s: socket is not ready\n", __func__);
        return -1;
    }

    LOG_3("connect to %s:%d\n", pAddr, port);

    /*
    * Convert IPv6 address from string to 4-byte integer:
    *   in_addr_t inet_addr(const char *cp);
    */
    servAddrLen = sizeof(struct sockaddr_in6);
    bzero(&servAddr, servAddrLen);
    servAddr.sin6_family = AF_INET6;
    servAddr.sin6_port   = htons( port );
    inet_pton(AF_INET6, pAddr, &servAddr.sin6_addr);

    pContext->remoteAddr = servAddr;

    error = connect(
                pContext->fd,
                (struct sockaddr *)&servAddr,
                servAddrLen
            );
    if (error < 0)
    {
        LOG_ERROR("fail to connect IPv6 TCP server\n");
        perror( "connect" );
        return -1;
    }

    pContext->running = 1;

    error = pthread_create(
                &pContext->thread,
                NULL,
                _tcpIpv6ClientRecvTask,
                pContext
            );
    if (error != 0)
    {
        LOG_ERROR("fail to create IPv6 TCP receiving thread\n");
        perror( "pthread_create" );
        return -1;
    }

    return 0;
}

/**
*  Send message to an IPv6 TCP server.
*  @param [in]  handle  IPv6 TCP client handle.
*  @param [in]  pData   A pointer of data buffer.
*  @param [in]  size    Data size.
*  @returns  Message length (-1 is failed).
*/
int comm_tcpIpv6ClientSend(
    tTcpIpv6ClientHandle  handle,
    unsigned char        *pData,
    unsigned short        size
)
{
    tTcpIpv6ClientContext *pContext = (tTcpIpv6ClientContext *)handle;
    int error;


    if (NULL == pContext)
    {
        LOG_ERROR("%s: pContext is NULL\n", __func__);
        return -1;
    }

    if (pContext->fd < 0)
    {
        LOG_ERROR("%s: socket is not ready\n", __func__);
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

    LOG_3("-> IPv6 TCP server\n");
    LOG_DUMP("IPv6 TCP client send", pData, size);

    error = send(
                pContext->fd,
                pData,
                size,
                0
            );
    if (error < 0)
    {
        LOG_ERROR("fail to send IPv6 TCP server\n");
        perror( "send" );
    }

    return error;
}

