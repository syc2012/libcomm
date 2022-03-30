#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <pthread.h>
#include <sys/socket.h>
#include <ifaddrs.h>
#include "comm_if.h"
#include "comm_log.h"


typedef struct _tUdpIpv4Context
{
    struct sockaddr_in  localAddr;
    int                 fd;

    tUdpRecvCb          pRecvFunc;
    void               *pArg;
    pthread_t           thread;
    int                 running;

    unsigned char       recvMsg[COMM_BUF_SIZE+1];
} tUdpIpv4Context;


/**
*  Initialize an IPv4 UDP socket.
*  @param [in]  pContext  A @ref tUdpIpv4Context object.
*  @returns  Success(0) or failure(-1).
*/
static int _udpIpv4InitSocket(tUdpIpv4Context *pContext)
{
    struct sockaddr_in bindAddr;
    int bindAddrLen;
    int fd;


    fd = socket(AF_INET, SOCK_DGRAM, 0);
    if (fd < 0)
    {
        perror( "socket" );
        return -1;
    }

    /* local host address */
    bindAddrLen = sizeof( struct sockaddr_in );
    bindAddr = pContext->localAddr;

    if (bind(fd, (struct sockaddr*)&bindAddr, bindAddrLen) < 0)
    {
        perror( "bind" );
        close( fd );
        return -1;
    }

    pContext->fd = fd;

    LOG_2("IPv4 UDP socket is ready\n");
    return 0;
}

/**
*  Un-initialize an IPv4 UDP socket.
*  @param [in]  pContext  A @ref tUdpIpv4Context object.
*/
static void _udpIpv4UninitSocket(tUdpIpv4Context *pContext)
{
    if (pContext->fd > 0)
    {
        close( pContext->fd );
        pContext->fd = -1;
    }

    LOG_2("IPv4 UDP socket is closed\n");
}

/**
*  Thread function for the IPv4 UDP socket receiving.
*  @param [in]  pArg  A @ref tUdpIpv4Context object.
*/
static void *_udpIpv4RecvTask(void *pArg)
{
    tUdpIpv4Context *pContext = pArg;
    struct sockaddr_in recvAddr;
    socklen_t recvAddrLen;
    int len;


    LOG_2("start the thread: %s\n", __func__);

    /* source address */
    recvAddrLen = sizeof( struct sockaddr_in );
    bzero(&recvAddr, recvAddrLen);

    while ( pContext->running )
    {
        LOG_3("IPv4 UDP ... recvfrom\n");
        len = recvfrom(
                  pContext->fd,
                  pContext->recvMsg,
                  COMM_BUF_SIZE,
                  0,
                  (struct sockaddr *)(&recvAddr),
                  &recvAddrLen
              );
        if (len <= 0)
        {
            LOG_ERROR("fail to receive IPv4 UDP socket\n");
            perror( "recvfrom" );
            break;
        }

        /*
        * Convert IPv4 address from byte array to string:
        *   char *inet_ntoa(struct in_addr in);
        */

        LOG_3(
            "<- %s:%d\n",
            inet_ntoa( recvAddr.sin_addr ),
            ntohs( recvAddr.sin_port )
        );
        LOG_DUMP("IPv4 UDP recv", pContext->recvMsg, len);

        if ( pContext->pRecvFunc )
        {
            pContext->pRecvFunc(
                pContext->pArg,
                pContext->recvMsg,
                len,
                (struct sockaddr *)&recvAddr
            );
        }
    }

    LOG_2("stop the thread: %s\n", __func__);
    pContext->running = 0;

    return NULL;
}

/**
*  Initialize IPv4 UDP socket.
*  @param [in]  portNum    Local UDP port number.
*  @param [in]  pRecvFunc  Application's receive callback function.
*  @param [in]  pArg       Application's argument.
*  @returns  IPv4 UDP handle.
*/
tUdpIpv4Handle comm_udpIpv4Init(
    unsigned short  portNum,
    tUdpRecvCb      pRecvFunc,
    void           *pArg
)
{
    tUdpIpv4Context *pContext = NULL;
    int error;


    pContext = malloc( sizeof( tUdpIpv4Context ) );
    if (NULL == pContext)
    {
        LOG_ERROR("fail to allocate IPv4 UDP context\n");
        return 0;
    }

    memset(pContext, 0x00, sizeof( tUdpIpv4Context ));
    pContext->localAddr.sin_family      = AF_INET;
    pContext->localAddr.sin_port        = htons( portNum );
    pContext->localAddr.sin_addr.s_addr = htonl( INADDR_ANY );
    pContext->pRecvFunc = pRecvFunc;
    pContext->pArg = pArg;
    pContext->fd = -1;

    error = _udpIpv4InitSocket( pContext );
    if (error != 0)
    {
        LOG_ERROR("fail to create IPv4 UDP socket\n");
        free( pContext );
        return 0;
    }

    if (NULL == pRecvFunc)
    {
        LOG_1("ignore IPv4 UDP receive function\n");
        goto _IPV4_DONE;
    }

    pContext->running = 1;

    error = pthread_create(
                &(pContext->thread),
                NULL,
                _udpIpv4RecvTask,
                pContext
            );
    if (error != 0)
    {
        LOG_ERROR("fail to create IPv4 UDP receiving thread\n");
        _udpIpv4UninitSocket( pContext );
        free( pContext );
        return 0;
    }

_IPV4_DONE:
    LOG_1("IPv4 UDP initialized\n");
    return ((tUdpIpv4Handle)pContext);
}

/**
*  Un-initialize IPv4 UDP socket.
*  @param [in]  handle  IPv4 UDP handle.
*/
void comm_udpIpv4Uninit(tUdpIpv4Handle handle)
{
    tUdpIpv4Context *pContext = (tUdpIpv4Context *)handle;

    if ( pContext )
    {
        if ( pContext->running )
        {
            pContext->running = 0;
            pthread_cancel( pContext->thread );
            pthread_join(pContext->thread, NULL);
            usleep(1000);
        }
     
        _udpIpv4UninitSocket( pContext );

        free( pContext );
        LOG_1("IPv4 UDP un-initialized\n");
    }
}

/**
*  Send message by the IPv4 UDP socket.
*  @param [in]  handle   IPv4 UDP handle.
*  @param [in]  pIpStr   A string of an IPv4 address.
*  @param [in]  portNum  UDP port number.
*  @param [in]  pData    A pointer of data buffer.
*  @param [in]  size     Data size.
*  @returns  Message length (-1 is failed).
*/
int comm_udpIpv4Send(
    tUdpIpv4Handle  handle,
    char           *pIpStr,
    unsigned short  portNum,
    unsigned char  *pData,
    unsigned short  size
)
{
    tUdpIpv4Context *pContext = (tUdpIpv4Context *)handle;
    struct sockaddr_in sendAddr;
    int sendAddrLen;
    int error;


    if (NULL == pContext)
    {
        LOG_ERROR("%s: pContext is NULL\n", __func__);
        return -1;
    }

    if (pContext->fd < 0)
    {
        LOG_ERROR("%s: UDP socket is not ready\n", __func__);
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

    LOG_3("-> %s:%d\n", pIpStr, portNum);
    LOG_DUMP("IPv4 UDP send", pData, size);

    /*
    * Convert IPv4 address from string to 4-byte integer:
    *   in_addr_t inet_addr(const char *cp);
    */

    sendAddrLen = sizeof( struct sockaddr_in );
    bzero(&sendAddr, sendAddrLen);
    sendAddr.sin_family      = AF_INET;
    sendAddr.sin_port        = htons( portNum );
    sendAddr.sin_addr.s_addr = inet_addr( pIpStr );

    error = sendto(
                pContext->fd,
                pData,
                size,
                0,
                (struct sockaddr *)(&sendAddr),
                sendAddrLen
            );
    if (error < 0)
    {
        LOG_ERROR("fail to send IPv4 UDP socket\n");
        perror( "sendto" );
    }

    return error;
}

/**
*  Get the IPv4 address of local host.
*  @param [in]      pIfName    A string of network interface name.
*  @param [in,out]  pIpv4Addr  A pointer of IPv4 address buffer.
*  @returns  Success(0) or failure(-1).
*/
int comm_udpIpv4GetAddr(char *pIfName, unsigned char *pIpv4Addr)
{
    struct ifaddrs *pIfAddrHdr = NULL;
    struct ifaddrs *pIf = NULL;
    unsigned int  sinAddr;
    int found = 0;


    memset(pIpv4Addr, 0x00, 4);

    if (getifaddrs( &pIfAddrHdr ) < 0)
    {
        perror( "getifaddrs" );
        return -1;
    }

    /*
    *  Walk through linked list, maintaining head pointer so we
    *  can free list later
    */
    for (pIf=pIfAddrHdr; pIf!=NULL; pIf=pIf->ifa_next)
    {
        if ((AF_INET == pIf->ifa_addr->sa_family) &&
            (0 == strcmp(pIf->ifa_name, pIfName)))
        {
            sinAddr = ntohl( ((struct sockaddr_in *)pIf->ifa_addr)->sin_addr.s_addr );
            pIpv4Addr[0] = ((sinAddr >> 24) & 0xFF);
            pIpv4Addr[1] = ((sinAddr >> 16) & 0xFF);
            pIpv4Addr[2] = ((sinAddr >>  8) & 0xFF);
            pIpv4Addr[3] = ((sinAddr      ) & 0xFF);

            LOG_2(
                "IPv4 address of %s is %u.%u.%u.%u\n",
                pIfName,
                pIpv4Addr[0],
                pIpv4Addr[1],
                pIpv4Addr[2],
                pIpv4Addr[3]
            );

            found = 1;
            break;
        }
    }

    freeifaddrs( pIfAddrHdr );

    return ( found ) ? 0 : -1;
}


typedef struct _tUdpIpv6Context
{
    struct sockaddr_in6  localAddr;
    int                  fd;

    tUdpRecvCb           pRecvFunc;
    void                *pArg;
    pthread_t            thread;
    int                  running;

    unsigned char        recvMsg[COMM_BUF_SIZE+1];
} tUdpIpv6Context;


/**
*  Initialize an IPv6 UDP socket.
*  @param [in]  pContext  A @ref tUdpIpv6Context object.
*  @returns  Success(0) or failure(-1).
*/
static int _udpIpv6InitSocket(tUdpIpv6Context *pContext)
{
    struct sockaddr_in6 bindAddr;
    int bindAddrLen;
    int fd;


    fd = socket(AF_INET6, SOCK_DGRAM, 0);
    if (fd < 0)
    {
        perror( "socket" );
        return -1;
    }

    /* local host address */
    bindAddrLen = sizeof( struct sockaddr_in6 );
    bindAddr = pContext->localAddr;

    if (bind(fd, (struct sockaddr *)&bindAddr, bindAddrLen) < 0)
    {
        perror( "bind" );
        close( fd );
        return -1;
    }

    pContext->fd = fd;

    LOG_2("IPv6 UDP socket is ready\n");
    return 0;
}

/**
*  Un-initialize an IPv6 UDP socket.
*  @param [in]  pContext  A @ref tUdpIpv6Context object.
*/
static void _udpIpv6UninitSocket(tUdpIpv6Context *pContext)
{
    if (pContext->fd > 0)
    {
        close( pContext->fd );
        pContext->fd = -1;
    }

    LOG_2("IPv6 UDP socket is closed\n");
}

/**
*  Thread function for the IPv6 UDP socket receiving.
*  @param [in]  pArg  A @ref tUdpIpv6Context object.
*/
static void *_udpIpv6RecvTask(void *pArg)
{
    tUdpIpv6Context *pContext = pArg;
    char ipv6Str[INET6_ADDRSTRLEN];
    struct sockaddr_in6 recvAddr;
    socklen_t recvAddrLen;
    int len;


    LOG_2("start the thread: %s\n", __func__);

    /* source address */
    recvAddrLen = sizeof( struct sockaddr_in6 );
    bzero(&recvAddr, recvAddrLen);

    while ( pContext->running )
    {
        LOG_3("IPv6 UDP ... recvfrom\n");
        len = recvfrom(
                  pContext->fd,
                  pContext->recvMsg,
                  COMM_BUF_SIZE,
                  0,
                  (struct sockaddr *)(&recvAddr),
                  &recvAddrLen
              );
        if (len <= 0)
        {
            LOG_ERROR("fail to receive IPv6 UDP socket\n");
            perror( "recvfrom" );
            break;
        }

        /*
        * Convert IPv6 address from byte array to string:
        *   const char *inet_ntop(
        *                   int af,
        *                   const void *src,
        *                   char *dst,
        *                   socklen_t size
        *               );
        */
        inet_ntop(
            AF_INET6,
            &(recvAddr.sin6_addr),
            ipv6Str,
            INET6_ADDRSTRLEN
        );

        LOG_3(
            "<- %s:%d\n",
            ipv6Str,
            ntohs( recvAddr.sin6_port )
        );
        LOG_DUMP("IPv6 UDP recv", pContext->recvMsg, len);

        if ( pContext->pRecvFunc )
        {
            pContext->pRecvFunc(
                pContext->pArg,
                pContext->recvMsg,
                len,
                (struct sockaddr *)&recvAddr
            );
        }
    }

    LOG_2("stop the thread: %s\n", __func__);
    pContext->running = 0;

    return NULL;
}

/**
*  Initialize IPv6 UDP socket.
*  @param [in]  portNum    Local UDP port number.
*  @param [in]  pRecvFunc  Application's receive callback function.
*  @param [in]  pArg       Application's argument.
*  @returns  IPv6 UDP handle.
*/
tUdpIpv6Handle comm_udpIpv6Init(
    unsigned short  portNum,
    tUdpRecvCb      pRecvFunc,
    void           *pArg
)
{
    tUdpIpv6Context *pContext = NULL;
    int error;


    pContext = malloc( sizeof( tUdpIpv6Context ) );
    if (NULL == pContext)
    {
        LOG_ERROR("fail to allocate IPv6 UDP context\n");
        return 0;
    }

    memset(pContext, 0x00, sizeof( tUdpIpv6Context ));
    pContext->localAddr.sin6_family = AF_INET6;
    pContext->localAddr.sin6_port   = htons( portNum );
    pContext->localAddr.sin6_addr   = in6addr_any;
    pContext->pRecvFunc = pRecvFunc;
    pContext->pArg = pArg;
    pContext->fd = -1;

    error = _udpIpv6InitSocket( pContext );
    if (error != 0)
    {
        LOG_ERROR("fail to create IPv6 UDP socket\n");
        free( pContext );
        return 0;
    }

    if (NULL == pRecvFunc)
    {
        LOG_1("ignore IPv6 UDP receive function\n");
        goto _IPV6_DONE;
    }

    pContext->running = 1;

    error = pthread_create(
                &(pContext->thread),
                NULL,
                _udpIpv6RecvTask,
                pContext
            );
    if (error != 0)
    {
        LOG_ERROR("fail to create IPv6 UDP receiving thread\n");
        _udpIpv6UninitSocket( pContext );
        free( pContext );
        return 0;
    }

_IPV6_DONE:
    LOG_1("IPv6 UDP initialized\n");
    return ((tUdpIpv6Handle)pContext);
}

/**
*  Un-initialize IPv6 UDP library.
*  @param [in]  handle  IPv6 UDP handle.
*/
void comm_udpIpv6Uninit(tUdpIpv6Handle handle)
{
    tUdpIpv6Context *pContext = (tUdpIpv6Context *)handle;

    if ( pContext )
    {
        if ( pContext->running )
        {
            pContext->running = 0;
            pthread_cancel( pContext->thread );
            pthread_join(pContext->thread, NULL);
            usleep(1000);
        }

        _udpIpv6UninitSocket( pContext );

        free( pContext );
        LOG_1("IPv6 UDP un-initialized\n");
    }
}

/**
*  Send message by the IPv6 UDP socket.
*  @param [in]  handle   IPv6 UDP handle.
*  @param [in]  pIpStr   A string of an IPv6 address.
*  @param [in]  portNum  UDP port number.
*  @param [in]  pData    A pointer of data buffer.
*  @param [in]  size     Data size.
*  @returns  Message length (-1 is failed).
*/
int comm_udpIpv6Send(
    tUdpIpv4Handle  handle,
    char           *pIpStr,
    unsigned short  portNum,
    unsigned char  *pData,
    unsigned short  size
)
{
    tUdpIpv4Context *pContext = (tUdpIpv4Context *)handle;
    struct sockaddr_in6 sendAddr;
    int sendAddrLen;
    int error;


    if (NULL == pContext)
    {
        LOG_ERROR("%s: pContext is NULL\n", __func__);
        return -1;
    }

    if (pContext->fd < 0)
    {
        LOG_ERROR("%s: UDP socket is not ready\n", __func__);
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

    LOG_3("-> %s:%d\n", pIpStr, portNum);
    LOG_DUMP("IPv6 UDP send", pData, size);

    /*
    * Convert IPv6 address from string to byte array:
    *   int inet_pton(int af, const char *src, void *dst);
    */

    sendAddrLen = sizeof( struct sockaddr_in6 );
    bzero(&sendAddr, sendAddrLen);
    sendAddr.sin6_family = AF_INET6;
    sendAddr.sin6_port   = htons( portNum );
    inet_pton(AF_INET6, pIpStr, &sendAddr.sin6_addr);

    error = sendto(
                pContext->fd,
                pData,
                size,
                0,
                (struct sockaddr *)(&sendAddr),
                sendAddrLen
            );
    if (error < 0)
    {
        LOG_ERROR("fail to send IPv6 UDP socket\n");
        perror( "sendto" );
    }

    return error;
}


/**
*  Get the IPv6 address of local host.
*  @param [in]      pIfName    A string of network interface name.
*  @param [in,out]  pIpv6Addr  A pointer of IPv6 address buffer.
*  @returns  Success(0) or failure(-1).
*/
int comm_udpIpv6GetAddr(char *pIfName, unsigned char *pIpv6Addr)
{
    struct ifaddrs *pIfAddrHdr = NULL;
    struct ifaddrs *pIf = NULL;
    unsigned char *pSin6Addr;
    int found = 0;


    memset(pIpv6Addr, 0x00, 16);

    if (getifaddrs( &pIfAddrHdr ) < 0)
    {
        perror( "getifaddrs" );
        return -1;
    }

    /*
    *  Walk through linked list, maintaining head pointer so we
    *  can free list later
    */
    for (pIf=pIfAddrHdr; pIf!=NULL; pIf=pIf->ifa_next)
    {
        if ((AF_INET6 == pIf->ifa_addr->sa_family) &&
            (0 == strcmp(pIf->ifa_name, pIfName)))
        {
            pSin6Addr = (unsigned char *)&(((struct sockaddr_in6 *)pIf->ifa_addr)->sin6_addr);
            memcpy(pIpv6Addr, pSin6Addr, 16);

            LOG_2(
                "IPv6 address of %s is %02X%02X:%02X%02X:%02X%02X:%02X%02X:%02X%02X:%02X%02X:%02X%02X:%02X%02X\n",
                pIfName,
                pIpv6Addr[0], pIpv6Addr[1],
                pIpv6Addr[2], pIpv6Addr[3],
                pIpv6Addr[4], pIpv6Addr[5],
                pIpv6Addr[6], pIpv6Addr[7],
                pIpv6Addr[8], pIpv6Addr[9],
                pIpv6Addr[10], pIpv6Addr[11],
                pIpv6Addr[12], pIpv6Addr[13],
                pIpv6Addr[14], pIpv6Addr[15]
            );

            found = 1;
            break;
        }
    }

    freeifaddrs( pIfAddrHdr );

    return ( found ) ? 0 : -1;
}

