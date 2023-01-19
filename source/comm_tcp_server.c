#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <pthread.h>
#include <sys/socket.h>
#include <netinet/tcp.h>
#include <ifaddrs.h>
#include "comm_if.h"
#include "comm_log.h"


#define TCP_USER_NUM (32)


typedef struct _tTcpIpv4ServerContext
{
    struct sockaddr_in  localAddr;
    int                 fd;

    tTcpUser           *pUser[TCP_USER_NUM];
    int                 userNum;
    int                 maxUserNum;

    tTcpServerAcptCb    pServerAcptFunc;
    tTcpServerExitCb    pServerExitFunc;
    tTcpServerRecvCb    pServerRecvFunc;
    void               *pServerArg;
    pthread_t           thread;
    int                 running;
} tTcpIpv4ServerContext;

static tTcpUser *_tcpIpv4AcceptClient(
    tTcpIpv4ServerContext *pContext,
    struct sockaddr_in    *pAddr,
    int                    fd
);
static void _tcpIpv4DisconnectClient(
    tTcpIpv4ServerContext *pContext,
    tTcpUser              *pUser
);


/**
*  Initialize an IPv4 TCP server socket.
*  @param [in]  pContext  A @ref tTcpIpv4ServerContext object.
*  @returns  Success(0) or failure(-1).
*/
static int _tcpIpv4InitServer(tTcpIpv4ServerContext *pContext)
{
    struct sockaddr_in bindAddr;
    int bindAddrLen;
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

    LOG_2("IPv4 TCP server socket is ready\n");
    return 0;
}

/**
*  Un-initialize an IPv4 TCP server socket.
*  @param [in]  pContext  A @ref tTcpIpv4ServerContext object.
*/
static void _tcpIpv4UninitServer(tTcpIpv4ServerContext *pContext)
{
    if (pContext->fd > 0)
    {
        close( pContext->fd );
        pContext->fd = -1;
    }

    LOG_2("IPv4 TCP server socket is closed\n");
}

/**
*  Thread function for the IPv4 TCP server receiving.
*  @param [in]  pArg  A @ref tTcpUser object.
*/
static void *_tcpIpv4ServerRecvTask(void *pArg)
{
    tTcpIpv4ServerContext *pContext;
    tTcpUser *pUser = pArg;
    int len;


    pthread_setcanceltype(PTHREAD_CANCEL_DEFERRED, NULL);
    LOG_2("start the thread: %s\n", __func__);

    pContext = pUser->pServer;

    while (pUser->fd > 0)
    {
        LOG_3("IPv4 TCP server ... recv\n");
        pthread_testcancel();
        len = recv(
                  pUser->fd,
                  pUser->recvMsg,
                  COMM_BUF_SIZE,
                  0
              );
        if (len <= 0)
        {
            LOG_1(
                "TCP client %s connection closed\n",
                inet_ntoa( pUser->addrIpv4.sin_addr )
            );
            close( pUser->fd );
            pUser->fd = -1;
            /* notify the client object to the server application */
            if ( pContext->pServerExitFunc )
            {
                pContext->pServerExitFunc(pContext->pServerArg, pUser);
            }
            break;
        }
        pthread_testcancel();

        LOG_3(
            "<- %s:%d\n",
            inet_ntoa( pUser->addrIpv4.sin_addr ),
            ntohs( pUser->addrIpv4.sin_port )
        );
        LOG_DUMP("IPv4 TCP server recv", pUser->recvMsg, len);

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

    _tcpIpv4DisconnectClient(pContext, pUser);

    pthread_exit(NULL);
}

/**
*  Thread function for the IPv4 TCP socket listen.
*  @param [in]  pArg  A @ref tTcpIpv4ServerContext object.
*/
static void *_tcpIpv4ServerListenTask(void *pArg)
{
    tTcpIpv4ServerContext *pContext = pArg;
    struct sockaddr_in clitAddr;
    socklen_t clitAddrLen = sizeof( struct sockaddr_in );


    LOG_2("start the thread: %s\n", __func__);
    pthread_setcanceltype(PTHREAD_CANCEL_DEFERRED, NULL);

    if (listen(pContext->fd, (TCP_USER_NUM << 1)) < 0)
    {
        perror( "listen" );
        close( pContext->fd );
        pContext->fd = -1;
        return NULL;
    }

    LOG_1("\n");
    LOG_1("Port number: %d\n", ntohs( pContext->localAddr.sin_port ));
    LOG_1("User limit : %d\n", pContext->maxUserNum);
    LOG_1("IPv4 TCP server ... listen\n");
    LOG_1("\n");

    while ( pContext->running )
    {
        tTcpUser *pUser;
        int fd;

        LOG_3("IPv4 TCP server ... accept\n");
        pthread_testcancel();
        fd = accept(
                 pContext->fd,
                 (struct sockaddr *)&clitAddr,
                 &clitAddrLen
             );
        if (fd < 0)
        {
            LOG_ERROR("fail to accept IPv4 TCP client\n");
            perror( "accept" );
            break;
        }
        pthread_testcancel();

        LOG_1("TCP client connect from %s\n", inet_ntoa(clitAddr.sin_addr));

        pUser = _tcpIpv4AcceptClient(pContext, &clitAddr, fd);
        if (NULL == pUser)
        {
            LOG_ERROR("fail to accept client\n");
            close( fd );
        }
    }

    LOG_2("stop the thread: %s\n", __func__);
    pContext->running = 0;

    pthread_exit(NULL);
}

/**
*  Initialize IPv4 TCP server.
*  @param [in]  portNum     Local TCP port number.
*  @param [in]  maxUserNum  Max. user number.
*  @param [in]  pAcptFunc   Application's accept callback function.
*  @param [in]  pExitFunc   Application's exit callback function.
*  @param [in]  pRecvFunc   Application's receive callback function.
*  @param [in]  pArg        Application's argument.
*  @returns  IPv4 TCP server handle.
*/
tTcpIpv4ServerHandle comm_tcpIpv4ServerInit(
    unsigned short    portNum,
    int               maxUserNum,
    tTcpServerAcptCb  pAcptFunc,
    tTcpServerExitCb  pExitFunc,
    tTcpServerRecvCb  pRecvFunc,
    void             *pArg
)
{
    tTcpIpv4ServerContext *pContext = NULL;
    pthread_attr_t tattr;
    int error;


    pContext = malloc( sizeof( tTcpIpv4ServerContext ) );
    if (NULL == pContext)
    {
        LOG_ERROR("fail to allocate TCP IPv4 server context\n");
        return 0;
    }

    memset(pContext, 0x00, sizeof( tTcpIpv4ServerContext ));
    pContext->localAddr.sin_family      = AF_INET;
    pContext->localAddr.sin_port        = htons( portNum );
    pContext->localAddr.sin_addr.s_addr = htonl( INADDR_ANY );
    pContext->maxUserNum = maxUserNum;
    pContext->pServerAcptFunc = pAcptFunc;
    pContext->pServerExitFunc = pExitFunc;
    pContext->pServerRecvFunc = pRecvFunc;
    pContext->pServerArg = pArg;
    pContext->fd = -1;

    if ((0 == pContext->maxUserNum) || (pContext->maxUserNum > TCP_USER_NUM))
    {
        LOG_1("set user number to the max. value %d\n", TCP_USER_NUM);
        pContext->maxUserNum = TCP_USER_NUM;
    }

    error = _tcpIpv4InitServer( pContext );
    if (error != 0)
    {
        LOG_ERROR("failed to create IPv4 TCP socket\n");
        free( pContext );
        return 0;
    }

    if (NULL == pAcptFunc)
    {
        LOG_1("ignore IPv4 TCP accept function\n");
    }

    if (NULL == pExitFunc)
    {
        LOG_1("ignore IPv4 TCP exit function\n");
    }

    if (NULL == pRecvFunc)
    {
        LOG_1("ignore IPv4 TCP receive function\n");
    }

    pContext->running = 1;

    pthread_attr_init( &tattr );
    pthread_attr_setdetachstate(&tattr, PTHREAD_CREATE_JOINABLE);

    error = pthread_create(
                &(pContext->thread),
                &tattr,
                _tcpIpv4ServerListenTask,
                pContext
            );
    if (error != 0)
    {
        LOG_ERROR("failed to create IPv4 TCP receiving thread\n");
        _tcpIpv4UninitServer( pContext );
        free( pContext );
        return 0;
    }

    pthread_attr_destroy( &tattr );

    LOG_1("IPv4 TCP server initialized\n");
    return ((tTcpIpv4ServerHandle)pContext);
}

/**
*  Un-initialize IPv4 TCP server.
*  @param [in]  handle  IPv4 TCP server handle.
*/
void comm_tcpIpv4ServerUninit(tTcpIpv4ServerHandle handle)
{
    tTcpIpv4ServerContext *pContext = (tTcpIpv4ServerContext *)handle;
    int i;

    if ( pContext )
    {
        pthread_cancel( pContext->thread );

        pContext->running = 0;
        pContext->userNum = 0;
        for (i=0; i<pContext->maxUserNum; i++)
        {
            _tcpIpv4DisconnectClient(pContext, pContext->pUser[i]);
        }
        _tcpIpv4UninitServer( pContext );
        free( pContext );

        pthread_join(pContext->thread, NULL);
        LOG_1("IPv4 TCP server un-initialized\n");
    }
}

/**
*  Create the connection to an IPv4 TCP client.
*  @param [in]  pContext  A @ref tTcpIpv4ServerContext object.
*  @param [in]  pAddr     IPv4 address string.
*  @param [in]  fd        Socket file descriptor.
*  @returns  A @ref tTcpUser object.
*/
static tTcpUser *_tcpIpv4AcceptClient(
    tTcpIpv4ServerContext *pContext,
    struct sockaddr_in    *pAddr,
    int                    fd
)
{
    tTcpUser *pUser = NULL;
    pthread_attr_t tattr;
    int bufSize = 0;
    socklen_t bufSizeLen;
    int noDelay = 1;
    socklen_t noDelayLen;
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
            pUser = malloc( sizeof( tTcpUser ) );

            if ( pUser )
            {
                LOG_3("IPv4 TCP create client fd(%d)\n", fd);

                bufSizeLen = sizeof( bufSize );
                setsockopt(fd, SOL_SOCKET, SO_SNDBUF, &bufSize, bufSizeLen);

                noDelayLen = sizeof( noDelay );
                setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, &noDelay, noDelayLen);

                memset(pUser, 0x00, sizeof( tTcpUser ));
                pUser->pServer = pContext;
                pUser->addrIpv4 = (*pAddr);
                pUser->fd = fd;

                pthread_attr_init( &tattr );
                pthread_attr_setdetachstate(&tattr, PTHREAD_CREATE_DETACHED);

                error = pthread_create(
                            &(pUser->thread),
                            &tattr,
                            _tcpIpv4ServerRecvTask,
                            pUser
                        );
                if (error != 0)
                {
                    LOG_ERROR("failed to create the client connection thread\n");
                    free( pUser );
                    return NULL;
                }

                pthread_attr_destroy( &tattr );

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
*  Remove the connection of an IPv4 TCP client.
*  @param [in]  pContext  A @ref tTcpIpv4ServerContext object.
*  @param [in]  pUser     A @ref tTcpUser object.
*/
static void _tcpIpv4DisconnectClient(
    tTcpIpv4ServerContext *pContext,
    tTcpUser              *pUser
)
{
    int i;

    if ( pUser )
    {
        LOG_3("IPv4 TCP remove client fd(%d)\n", pUser->fd);

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
            pthread_cancel( pUser->thread );
            close( pUser->fd );
            pUser->fd = -1;
        }

        free( pUser );
    }
}

/**
*  Send message to an IPv4 TCP client.
*  @param [in]  pUser  A @ref tTcpUser object.
*  @param [in]  pData  A pointer of data buffer.
*  @param [in]  size   Data size.
*  @returns  Message length (-1 is failed).
*/
int comm_tcpIpv4ServerSend(
    tTcpUser       *pUser,
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
        LOG_ERROR("%s: client socket is not ready\n", __func__);
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

    LOG_3("-> %s\n", inet_ntoa( pUser->addrIpv4.sin_addr ));
    LOG_DUMP("IPv4 TCP server send", pData, size);

    error = send(
                pUser->fd,
                pData,
                size,
                0
            );
    if (error < 0)
    {
        LOG_ERROR("fail to send IPv4 TCP client\n");
        perror( "send" );
    }

    return error;
}

/**
*  Send message to all IPv4 TCP clients.
*  @param [in]  handle  IPv4 TCP server handle.
*  @param [in]  pData   A pointer of data buffer.
*  @param [in]  size    Data size.
*/
void comm_tcpIpv4ServerSendAllClient(
    tTcpIpv4ServerHandle  handle,
    unsigned char        *pData,
    unsigned short        size
)
{
    tTcpIpv4ServerContext *pContext = (tTcpIpv4ServerContext *)handle;
    tTcpUser *pUser;
    int error;
    int i;


    if (0 == pContext->userNum)
    {
        LOG_WARN("%s: user number is 0\n", __func__);
        return;
    }

    if (NULL == pData)
    {
        LOG_ERROR("%s: pData is NULL\n", __func__);
        return;
    }

    if (0 == size)
    {
        LOG_WARN("%s: size is 0\n", __func__);
        return;
    }

    LOG_DUMP("IPv4 TCP send to all clients", pData, size);

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
                LOG_ERROR("fail to send IPv4 TCP to fd(%d)\n", pUser->fd);
                perror( "send" );
            }
        }
    }
}

/**
*  Get the number of connected IPv4 clients.
*  @param [in]  handle  IPv4 TCP server handle.
*  @returns  Number of clients.
*/
int comm_tcpIpv4ServerGetClientNum(tTcpIpv4ServerHandle handle)
{
    tTcpIpv4ServerContext *pContext = (tTcpIpv4ServerContext *)handle;
    return pContext->userNum;
}


typedef struct _tTcpIpv6ServerContext
{
    struct sockaddr_in6  localAddr;
    int                  fd;

    tTcpUser            *pUser[TCP_USER_NUM];
    int                  userNum;
    int                  maxUserNum;

    tTcpServerAcptCb     pServerAcptFunc;
    tTcpServerExitCb     pServerExitFunc;
    tTcpServerRecvCb     pServerRecvFunc;
    void                *pServerArg;
    pthread_t            thread;
    int                  running;
} tTcpIpv6ServerContext;

static tTcpUser *_tcpIpv6AcceptClient(
    tTcpIpv6ServerContext *pContext,
    struct sockaddr_in6   *pAddr,
    int                    fd
);
static void _tcpIpv6DisconnectClient(
    tTcpIpv6ServerContext *pContext,
    tTcpUser              *pUser
);


/**
*  Initialize an IPv6 TCP socket.
*  @param [in]  pContext  A @ref tTcpIpv6ServerContext object.
*  @returns  Success(0) or failure(-1).
*/
static int _tcpIpv6InitServer(tTcpIpv6ServerContext *pContext)
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

    LOG_2("IPv6 TCP server socket is ready\n");
    return 0;
}

/**
*  Un-initialize an IPv6 TCP socket.
*  @param [in]  pContext  A @ref tTcpIpv6ServerContext object.
*/
static void _tcpIpv6UninitServer(tTcpIpv6ServerContext *pContext)
{
    if (pContext->fd > 0)
    {
        close( pContext->fd );
        pContext->fd = -1;
    }

    LOG_2("IPv6 TCP server socket is closed\n");
}

/**
*  Thread function for the IPv6 TCP server receiving.
*  @param [in]  pArg  A @ref tTcpUser object.
*/
static void *_tcpIpv6ServerRecvTask(void *pArg)
{
    tTcpIpv6ServerContext *pContext;
    tTcpUser *pUser = pArg;
    char ipv6Str[INET6_ADDRSTRLEN];
    int len;


    pthread_setcanceltype(PTHREAD_CANCEL_DEFERRED, NULL);
    LOG_2("start the thread: %s\n", __func__);

    pContext = pUser->pServer;

    while (pUser->fd > 0)
    {
        LOG_3("IPv6 TCP server ... recv\n");
        pthread_testcancel();
        len = recv(
                  pUser->fd,
                  pUser->recvMsg,
                  COMM_BUF_SIZE,
                  0
              );
        if (len <= 0)
        {
            inet_ntop(
                AF_INET6,
                &(pUser->addrIpv6.sin6_addr),
                ipv6Str,
                INET6_ADDRSTRLEN
            );
            LOG_1("TCP client %s connection closed\n", ipv6Str);
            close( pUser->fd );
            pUser->fd = -1;
            /* notify the client object to the server application */
            if ( pContext->pServerExitFunc )
            {
                pContext->pServerExitFunc(pContext->pServerArg, pUser);
            }
            break;
        }
        pthread_testcancel();

        inet_ntop(
            AF_INET6,
            &(pUser->addrIpv6.sin6_addr),
            ipv6Str,
            INET6_ADDRSTRLEN
        );
        LOG_3(
           "<- %s:%d\n",
            ipv6Str,
            ntohs( pUser->addrIpv6.sin6_port )
        );
        LOG_DUMP("IPv6 TCP server recv", pUser->recvMsg, len);

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

    _tcpIpv6DisconnectClient(pContext, pUser);

    pthread_exit(NULL);
}

/**
*  Thread function for the IPv6 TCP socket listen.
*  @param [in]  pArg  A @ref tTcpIpv6ServerContext object.
*/
static void *_tcpIpv6ServerListenTask(void *pArg)
{
    tTcpIpv6ServerContext *pContext = pArg;
    char ipv6Str[INET6_ADDRSTRLEN];
    struct sockaddr_in6 clitAddr;
    socklen_t clitAddrLen = sizeof( struct sockaddr_in6 );


    pthread_setcanceltype(PTHREAD_CANCEL_DEFERRED, NULL);
    LOG_2("start the thread: %s\n", __func__);

    if (listen(pContext->fd, (TCP_USER_NUM << 1)) < 0)
    {
        perror( "listen" );
        close( pContext->fd );
        pContext->fd = -1;
        return NULL;
    }

    LOG_1("\n");
    LOG_1("Port number: %d\n", ntohs( pContext->localAddr.sin6_port ));
    LOG_1("User limit : %d\n", pContext->maxUserNum);
    LOG_1("IPv6 TCP server ... listen\n");
    LOG_1("\n");

    while ( pContext->running )
    {
        tTcpUser *pUser;
        int fd;

        LOG_3("IPv6 TCP server ... accept\n");
        pthread_testcancel();
        fd = accept(
                 pContext->fd,
                 (struct sockaddr *)&clitAddr,
                 &clitAddrLen
             );
        if (fd < 0)
        {
            LOG_ERROR("fail to accept IPv6 TCP client\n");
            perror( "accept" );
            break;
        }
        pthread_testcancel();

        inet_ntop(
            AF_INET6,
            &(clitAddr.sin6_addr),
            ipv6Str,
            INET6_ADDRSTRLEN
        );
        LOG_1("TCP client connect from %s\n", ipv6Str);

        pUser = _tcpIpv6AcceptClient(pContext, &clitAddr, fd);
        if (NULL == pUser)
        {
            LOG_ERROR("fail to accept client\n");
            close( fd );
        }
    }

    LOG_2("stop the thread: %s\n", __func__);
    pContext->running = 0;

    pthread_exit(NULL);
}

/**
*  Initialize IPv6 TCP server.
*  @param [in]  portNum     Local TCP port number.
*  @param [in]  maxUserNum  Max. user number.
*  @param [in]  pAcptFunc   Application's accept callback function.
*  @param [in]  pExitFunc   Application's exit callback function.
*  @param [in]  pRecvFunc   Application's receive callback function.
*  @param [in]  pArg        Application's argument.
*  @returns  IPv6 TCP server handle.
*/
tTcpIpv6ServerHandle comm_tcpIpv6ServerInit(
    unsigned short    portNum,
    int               maxUserNum,
    tTcpServerAcptCb  pAcptFunc,
    tTcpServerExitCb  pExitFunc,
    tTcpServerRecvCb  pRecvFunc,
    void             *pArg
)
{
    tTcpIpv6ServerContext *pContext = NULL;
    pthread_attr_t tattr;
    int error;


    pContext = malloc( sizeof( tTcpIpv6ServerContext ) );
    if (NULL == pContext)
    {
        LOG_ERROR("fail to allocate TCP IPv6 server context\n");
        return 0;
    }

    memset(pContext, 0x00, sizeof( tTcpIpv6ServerContext ));
    pContext->localAddr.sin6_family = AF_INET6;
    pContext->localAddr.sin6_port   = htons( portNum );
    pContext->localAddr.sin6_addr   = in6addr_any;
    pContext->maxUserNum = maxUserNum;
    pContext->pServerAcptFunc = pAcptFunc;
    pContext->pServerExitFunc = pExitFunc;
    pContext->pServerRecvFunc = pRecvFunc;
    pContext->pServerArg = pArg;
    pContext->fd = -1;

    if ((0 == pContext->maxUserNum) || (pContext->maxUserNum > TCP_USER_NUM))
    {
        LOG_1("set user number to the max. value %d\n", TCP_USER_NUM);
        pContext->maxUserNum = TCP_USER_NUM;
    }

    error = _tcpIpv6InitServer( pContext );
    if (error != 0)
    {
        LOG_ERROR("failed to create IPv6 TCP socket\n");
        free( pContext );
        return 0;
    }

    if (NULL == pAcptFunc)
    {
        LOG_1("ignore IPv6 TCP accept function\n");
    }

    if (NULL == pExitFunc)
    {
        LOG_1("ignore IPv6 TCP exit function\n");
    }

    if (NULL == pRecvFunc)
    {
        LOG_1("ignore IPv6 TCP receive function\n");
    }

    pContext->running = 1;

    pthread_attr_init( &tattr );
    pthread_attr_setdetachstate(&tattr, PTHREAD_CREATE_JOINABLE);

    error = pthread_create(
                &(pContext->thread),
                &tattr,
                _tcpIpv6ServerListenTask,
                pContext
            );
    if (error != 0)
    {
        LOG_ERROR("failed to create IPv6 TCP receiving thread\n");
        _tcpIpv6UninitServer( pContext );
        free( pContext );
        return 0;
    }

    pthread_attr_destroy( &tattr );

    LOG_1("IPv6 TCP server initialized\n");
    return ((tTcpIpv6ServerHandle)pContext);
}

/**
*  Un-initialize IPv6 TCP server.
*  @param [in]  handle  IPv6 TCP server handle.
*/
void comm_tcpIpv6ServerUninit(tTcpIpv6ServerHandle handle)
{
    tTcpIpv6ServerContext *pContext = (tTcpIpv6ServerContext *)handle;
    int i;

    if ( pContext )
    {
        pthread_cancel( pContext->thread );

        pContext->running = 0;
        pContext->userNum = 0;
        for (i=0; i<pContext->maxUserNum; i++)
        {
            _tcpIpv6DisconnectClient(pContext, pContext->pUser[i]);
        }
        _tcpIpv6UninitServer( pContext );
        free( pContext );

        pthread_join(pContext->thread, NULL);
        LOG_1("IPv6 TCP server un-initialized\n");
    }
}

/**
*  Create the connection to an IPv6 TCP client.
*  @param [in]  pContext  A @ref tTcpIpv6ServerContext object.
*  @param [in]  pAddr     IPv6 address string.
*  @param [in]  fd        Socket file descriptor.
*  @returns  A @ref tTcpUser object.
*/
static tTcpUser *_tcpIpv6AcceptClient(
    tTcpIpv6ServerContext *pContext,
    struct sockaddr_in6   *pAddr,
    int                    fd
)
{
    tTcpUser *pUser = NULL;
    pthread_attr_t tattr;
    int  bufSize = 0;
    socklen_t bufSizeLen;
    int  noDelay = 1;
    socklen_t noDelayLen;
    int  error;
    int  i;


    if (pContext->userNum >= pContext->maxUserNum)
    {
        LOG_ERROR("user number was exceeded (%d)\n", pContext->maxUserNum);
        return NULL;
    }

    for (i=0; i<pContext->maxUserNum; i++)
    {
        if (NULL == pContext->pUser[i])
        {
            pUser = malloc( sizeof( tTcpUser ) );

            if ( pUser )
            {
                LOG_3("IPv6 TCP create client fd(%d)\n", fd);

                bufSizeLen = sizeof( bufSize );
                setsockopt(fd, SOL_SOCKET, SO_SNDBUF, &bufSize, bufSizeLen);

                noDelayLen = sizeof( noDelay );
                setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, &noDelay, noDelayLen);

                memset(pUser, 0x00, sizeof( tTcpUser ));
                pUser->pServer = pContext;
                pUser->addrIpv6 = (*pAddr);
                pUser->fd = fd;

                pthread_attr_init( &tattr );
                pthread_attr_setdetachstate(&tattr, PTHREAD_CREATE_DETACHED);

                error = pthread_create(
                            &(pUser->thread),
                            &tattr,
                            _tcpIpv6ServerRecvTask,
                            pUser
                        );
                if (error != 0)
                {
                    LOG_ERROR("failed to create the client connection thread\n");
                    free( pUser );
                    return NULL;
                }

                pthread_attr_destroy( &tattr );

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
*  Remove the connection of an IPv6 TCP client.
*  @param [in]  pContext  A @ref tTcpIpv6ServerContext object.
*  @param [in]  pUser     A @ref tTcpUser object.
*/
static void _tcpIpv6DisconnectClient(
    tTcpIpv6ServerContext *pContext,
    tTcpUser              *pUser
)
{
    int i;

    if ( pUser )
    {
        LOG_3("IPv6 TCP remove client fd(%d)\n", pUser->fd);

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
            pthread_cancel( pUser->thread );
            close( pUser->fd );
            pUser->fd = -1;
        }

        free( pUser );
    }
}

/**
*  Send message to an IPv6 TCP client.
*  @param [in]  pUser  A @ref tTcpUser object.
*  @param [in]  pData  A pointer of data buffer.
*  @param [in]  size   Data size.
*  @returns  Message length (-1 is failed).
*/
int comm_tcpIpv6ServerSend(
    tTcpUser       *pUser,
    unsigned char  *pData,
    unsigned short  size
)
{
    char ipv6Str[INET6_ADDRSTRLEN];
    int error;


    if (NULL == pUser)
    {
        LOG_ERROR("%s: pUser is NULL\n", __func__);
        return -1;
    }

    if (pUser->fd < 0)
    {
        LOG_ERROR("%s: client socket is not ready\n", __func__);
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

    inet_ntop(
        AF_INET6,
        &(pUser->addrIpv6.sin6_addr),
        ipv6Str,
        INET6_ADDRSTRLEN
    );
    LOG_3("-> %s\n", ipv6Str);
    LOG_DUMP("IPv6 TCP server send", pData, size);

    error = send(
                pUser->fd,
                pData,
                size,
                0
            );
    if (error < 0)
    {
        LOG_ERROR("fail to send IPv6 TCP client\n");
        perror( "send" );
    }

    return error;
}

/**
*  Send message to all IPv6 TCP clients.
*  @param [in]  handle  IPv6 TCP server handle.
*  @param [in]  pData   A pointer of data buffer.
*  @param [in]  size    Data size.
*/
void comm_tcpIpv6ServerSendAllClient(
    tTcpIpv6ServerHandle  handle,
    unsigned char        *pData,
    unsigned short        size
)
{
    tTcpIpv6ServerContext *pContext = (tTcpIpv6ServerContext *)handle;
    tTcpUser *pUser;
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

    LOG_DUMP("IPv6 TCP send to all clients", pData, size);

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
                LOG_ERROR("fail to send IPv6 TCP to fd(%d)\n", pUser->fd);
                perror( "send" );
            }
        }
    }
}

/**
*  Get the number of connected IPv6 clients.
*  @param [in]  handle  IPv6 TCP server handle.
*  @returns  Number of clients.
*/
int comm_tcpIpv6ServerGetClientNum(tTcpIpv6ServerHandle handle)
{
    tTcpIpv6ServerContext *pContext = (tTcpIpv6ServerContext *)handle;
    return pContext->userNum;
}

