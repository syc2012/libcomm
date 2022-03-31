#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <ifaddrs.h>
#include <linux/if_packet.h>
#include <linux/if_ether.h>
#include <linux/if_arp.h>
#include <netinet/in.h> /* htons */
#include "comm_if.h"
#include "comm_log.h"


#define ETH_DEVICE "eth0"


typedef struct _tRawContext
{
    char           ifName[IFNAMSIZ];
    int            ifIndex;
    int            ifMtu;
    unsigned char  ifHwAddr[ETH_ALEN];
    int            promisc;
    int            fd;

    tRawRecvCb     pRecvFunc;
    void          *pArg;
    pthread_t      thread;
    int            running;

    unsigned char  recvMsg[COMM_BUF_SIZE+1];
} tRawContext;


/**
*  Initialize a raw socket.
*  @param [in]  pContext  A @ref tRawContext object.
*  @returns  Success(0) or failure(-1).
*/
static int _rawInit(tRawContext *pContext)
{
    /*
     * struct ifreq {
     *     char ifr_name[IFNAMSIZ];
     *     union {
     *         struct sockaddr ifr_addr;
     *         struct sockaddr ifr_dstaddr;
     *         struct sockaddr ifr_broadaddr;
     *         struct sockaddr ifr_netmask;
     *         struct sockaddr ifr_hwaddr;
     *         short           ifr_flags;
     *         int             ifr_ifindex;
     *         int             ifr_metric;
     *         int             ifr_mtu;
     *         struct ifmap    ifr_map;
     *         char            ifr_slave[IFNAMSIZ];
     *         char            ifr_newname[IFNAMSIZ];
     *         char           *ifr_data;
     *     };
     * };
     */
    struct ifreq ifReq;
    int fd;


    fd = socket(AF_PACKET, SOCK_RAW, htons(ETH_P_ALL));
    if (fd < 0)
    {
        perror( "socket" );
        return -1;
    }

    memset(&ifReq, 0x00, sizeof( struct ifreq ));
    strncpy(ifReq.ifr_name, pContext->ifName, IFNAMSIZ);
    LOG_2("interface name: %s\n", pContext->ifName);

    /* retrieve ethernet interface index */
    if (ioctl(fd, SIOCGIFINDEX, &ifReq) < 0)
    {
        perror( "SIOCGIFINDEX" );
        close( fd );
        return -1;
    }

    pContext->ifIndex = ifReq.ifr_ifindex;
    LOG_2("interface index: %d\n", pContext->ifIndex);

    /* retrieve ethernet interface MTU */
    if (ioctl(fd, SIOCGIFMTU, &ifReq) < 0)
    {
        perror( "SIOCGIFMTU" );
        close( fd );
        return -1;
    }

    pContext->ifMtu = ifReq.ifr_mtu;
    LOG_2("interface MTU: %d\n", pContext->ifMtu);

    /* retrieve corresponding MAC */
    if (ioctl(fd, SIOCGIFHWADDR, &ifReq) < 0)
    {
        perror( "SIOCGIFHWADDR" );
        close( fd );
        return -1;
    }

    memcpy(pContext->ifHwAddr, ifReq.ifr_hwaddr.sa_data, ETH_ALEN);
    LOG_2(
        "MAC address: %02X:%02X:%02X:%02X:%02X:%02X\n",
        pContext->ifHwAddr[0],
        pContext->ifHwAddr[1],
        pContext->ifHwAddr[2],
        pContext->ifHwAddr[3],
        pContext->ifHwAddr[4],
        pContext->ifHwAddr[5]
    );

    pContext->fd = fd;

    LOG_2("Raw socket is ready\n");
    return 0;
}

/**
*  Un-initialize a raw socket.
*  @param [in]  pContext  A @ref tRawContext object.
*/
static void _rawUninit(tRawContext *pContext)
{
    if (pContext->fd > 0)
    {
        close( pContext->fd );
        pContext->fd = -1;
    }

    LOG_2("Raw socket is closed\n");
}

/**
*  Thread function for the raw socket receiving.
*  @param [in]  pArg  A @ref tRawContext object.
*/
static void *_rawRecvTask(void *pArg)
{
    tRawContext *pContext = pArg;
    int len;


    LOG_2("start the thread: %s\n", __func__);

    while ( pContext->running )
    {
        LOG_3("Raw socket ... recvfrom\n");
        len = recvfrom(
                  pContext->fd,
                  pContext->recvMsg,
                  COMM_BUF_SIZE,
                  0,
                  NULL,
                  NULL
              );
        if (len < 0)
        {
            LOG_ERROR("fail to receive raw socket\n");
            perror( "recvfrom" );
            break;
        }

        LOG_3("<- Raw socket (%s)\n", pContext->ifName);
        LOG_DUMP("Raw recv", pContext->recvMsg, len);

        if ( pContext->pRecvFunc )
        {
            pContext->pRecvFunc(pContext->pArg, pContext->recvMsg, len);
        }
    }

    LOG_2("stop the thread: %s\n", __func__);
    pContext->running = 0;

    return NULL;
}


/**
*  Initialize raw socket.
*  @param [in]  pEthDev    Ethernet device name.
*  @param [in]  pRecvFunc  Application's receive callback function.
*  @param [in]  pArg       Application's argument.
*  @returns  Raw socket handle
*/
tRawHandle comm_rawSockInit(
    char       *pEthDev,
    tRawRecvCb  pRecvFunc,
    void       *pArg
)
{
    tRawContext *pContext = NULL;
    int error;


    pContext = malloc( sizeof( tRawContext ) );
    if (NULL == pContext)
    {
        LOG_ERROR("fail to allocate raw context\n");
        return 0;
    }

    if (NULL == pEthDev)
    {
        LOG_1("select default net device: %s\n", ETH_DEVICE);
        pEthDev = ETH_DEVICE;
    }

    memset(pContext, 0x00, sizeof( tRawContext ));
    strncpy(pContext->ifName, pEthDev, IFNAMSIZ);
    pContext->pRecvFunc = pRecvFunc;
    pContext->pArg = pArg;
    pContext->fd = -1;

    error = _rawInit( pContext );
    if (error != 0)
    {
        LOG_ERROR("failed to create raw socket\n");
        free( pContext );
        return 0;
    }

    if (NULL == pRecvFunc)
    {
        LOG_1("ignore raw socket receive function\n");
        goto _RAW_DONE;
    }

    pContext->running = 1;

    error = pthread_create(
                 &(pContext->thread),
                 NULL,
                 _rawRecvTask,
                 pContext
             );
    if (error != 0)
    {
        LOG_ERROR("failed to create raw socket receiving thread\n");
        _rawUninit( pContext );
        free( pContext );
        return 0;
    }

_RAW_DONE:
    LOG_1("Raw socket initialized\n");
    return ((tRawHandle)pContext);
}

/**
*  Un-initialize raw socket library.
*  @param [in]  handle  Raw socket handle.
*/
void comm_rawSockUninit(tRawHandle handle)
{
    tRawContext *pContext = (tRawContext *)handle;

    if ( pContext )
    {
        if ( pContext->running )
        {
            pContext->running = 0;
            pthread_cancel( pContext->thread );
            pthread_join(pContext->thread, NULL);
            usleep(1000);
        }

        if ( pContext->promisc )
        {
            comm_rawPromiscMode(handle, 0);
        }

        _rawUninit( pContext );

        free( pContext );
        LOG_1("Raw socket un-initialized\n");
    }
}

/**
*  Send data by a raw socket.
*  @param [in]  handle  Raw socket handle.
*  @param [in]  pData   A pointer of data buffer.
*  @param [in]  size    Data size.
*  @returns  Message length (-1 is failed).
*/
int comm_rawSockSend(
    tRawHandle      handle,
    unsigned char  *pData,
    unsigned short  size
)
{
    tRawContext *pContext = (tRawContext *)handle;
    unsigned char *pDestMac = pData;
    struct sockaddr_ll sockAddr;
    int sockAddrLen;
    int error;


    if (NULL == pContext)
    {
        LOG_ERROR("%s: pContext is NULL\n", __func__);
        return -1;
    }

    if (pContext->fd < 0)
    {
        LOG_ERROR("%s: Raw socket is not ready\n", __func__);
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

    LOG_3("-> Raw socket (%s)\n", pContext->ifName);
    LOG_DUMP("Raw send", pData, size);

    /* prepare sockaddr_ll */
    sockAddrLen = sizeof( struct sockaddr_ll );
    sockAddr.sll_family   = AF_PACKET;
    sockAddr.sll_protocol = htons(ETH_P_IP);
    sockAddr.sll_ifindex  = pContext->ifIndex;
    sockAddr.sll_hatype   = ARPHRD_ETHER;
    sockAddr.sll_pkttype  = PACKET_OTHERHOST;
    sockAddr.sll_halen    = ETH_ALEN;
    sockAddr.sll_addr[0]  = pDestMac[0];
    sockAddr.sll_addr[1]  = pDestMac[1];
    sockAddr.sll_addr[2]  = pDestMac[2];
    sockAddr.sll_addr[3]  = pDestMac[3];
    sockAddr.sll_addr[4]  = pDestMac[4];
    sockAddr.sll_addr[5]  = pDestMac[5];
    sockAddr.sll_addr[6]  = 0x00; 
    sockAddr.sll_addr[7]  = 0x00;

    error = sendto(
                pContext->fd,
                pData,
                size,
                0,
                (struct sockaddr *)&sockAddr,
                sockAddrLen
            );
    if (error < 0)
    {
        LOG_ERROR("fail to send raw socket\n");
        perror( "sendto" );
    }

    return error;
}

/**
*  Enable or disable the promiscuous mode.
*  @param [in]  handle  Raw socket handle.
*  @param [in]  enable  Boolean.
*  @returns  Success(0) or failure(-1).
*/
int comm_rawPromiscMode(tRawHandle handle, int enable)
{
    tRawContext *pContext = (tRawContext *)handle;
    struct ifreq ifReq;


    if (NULL == pContext)
    {
        LOG_ERROR("%s: pContext is NULL\n", __func__);
        return -1;
    }

    if (pContext->fd < 0)
    {
        LOG_ERROR("%s: Raw socket is not ready\n", __func__);
        return -1;
    }

    memset(&ifReq, 0x00, sizeof( struct ifreq ));
    strncpy(ifReq.ifr_name, pContext->ifName, IFNAMSIZ);

    if (ioctl(pContext->fd, SIOCGIFFLAGS, &ifReq) < 0)
    {
        perror( "SIOCGIFFLAGS" );
        return -1;
    }

    /* check whether the device is 'up' */
    if  ( !(ifReq.ifr_flags & IFF_UP) )
    {
        ifReq.ifr_flags |= IFF_UP;
    }

    if ( enable )
    {
        ifReq.ifr_flags |= IFF_PROMISC;
    }
    else
    {
        ifReq.ifr_flags &= ~IFF_PROMISC;
    }

    if (ioctl(pContext->fd, SIOCSIFFLAGS, &ifReq) < 0)
    {
        perror( "SIOCSIFFLAGS" );
        return -1;
    }

    LOG_3(
        "%s: promiscuous mode %s\n",
        pContext->ifName,
        (( enable ) ? "on" : "off")
    );
    pContext->promisc = enable;

    return 0;
}

/**
*  Get the device MTU size.
*  @param [in]  handle  Raw socket handle.
*  @returns  MTU size.
*/
int comm_rawGetMtu(tRawHandle handle)
{
    tRawContext *pContext = (tRawContext *)handle;


    if (NULL == pContext)
    {
        LOG_ERROR("%s: pContext is NULL\n", __func__);
        return 0;
    }

    if (pContext->fd < 0)
    {
        LOG_ERROR("%s: Raw socket is not ready\n", __func__);
        return 0;
    }

    return pContext->ifMtu;
}

/**
*  Get the device MAC address.
*  @param [in]  handle  Raw socket handle.
*  @returns  6-bytes MAC address.
*/
unsigned char *comm_rawGetHwAddr(tRawHandle handle)
{
    tRawContext *pContext = (tRawContext *)handle;


    if (NULL == pContext)
    {
        LOG_ERROR("%s: pContext is NULL\n", __func__);
        return NULL;
    }

    if (pContext->fd < 0)
    {
        LOG_ERROR("%s: Raw socket is not ready\n", __func__);
        return NULL;
    }

    return pContext->ifHwAddr;
}

