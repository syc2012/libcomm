#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/socket.h>
#include <linux/netlink.h> /* struct nlmsghdr */
#include "comm_if.h"
#include "comm_log.h"


typedef struct _tNetlinkContext
{
    struct sockaddr_nl  localAddr;
    int                 fd;

    tNetlinkRecvCb      pRecvFunc;
    tNetlinkExitCb      pExitFunc;
    void               *pArg;
    pthread_t           thread;
    int                 running;

    struct nlmsghdr    *pSendNlHdr;
    struct nlmsghdr    *pRecvNlHdr;
    struct iovec        sendIov;
    struct iovec        recvIov;
    struct msghdr       sendMsg;
    struct msghdr       recvMsg;
    unsigned char      *pSendBuf;
    unsigned char      *pRecvBuf;
} tNetlinkContext;


/**
*  Initialize netlink socket.
*  @param [in]  pContext  A @ref tNetlinkContext object.
*  @returns  Success(0) or failure(-1).
*/
static int _netlinkInit(tNetlinkContext *pContext)
{
    struct nlmsghdr *pSendNlHdr = NULL;
    struct nlmsghdr *pRecvNlHdr = NULL;
    struct sockaddr_nl sourAddr;
    struct sockaddr_nl destAddr;
    socklen_t sourAddrLen;
    socklen_t destAddrLen;
    int fd;


    fd = socket(PF_NETLINK, SOCK_RAW, NETLINK_USERSOCK);
    if (fd < 0)
    {
        perror( "socket" );
        return -1;
    }

    sourAddrLen = sizeof( struct sockaddr_nl );
    sourAddr = pContext->localAddr;

    if (bind(fd, (struct sockaddr *)&sourAddr, sourAddrLen) < 0)
    {
        perror( "bind" );
        close( fd );
        return -1;
    }


    /* [1] netlink message buffer for send */
    pSendNlHdr = (struct nlmsghdr *)malloc(
                     NLMSG_SPACE(COMM_BUF_SIZE + 1)
                 );
    if (NULL == pSendNlHdr)
    {
        LOG_ERROR("fail to allocate send buffer\n");
        close( fd );
        return -1;
    }

    destAddrLen = sizeof( struct sockaddr_nl );
    memset(&destAddr, 0x00, destAddrLen);
    destAddr.nl_family = AF_NETLINK;
    destAddr.nl_pid    = 0; /* for Linux Kernel */
    destAddr.nl_groups = 0; /* unicast */

    /* Fill the netlink message header */
    pSendNlHdr->nlmsg_type  = 0;
    pSendNlHdr->nlmsg_len   = NLMSG_SPACE(COMM_BUF_SIZE);
    pSendNlHdr->nlmsg_pid   = getpid(); /* self pid */
    pSendNlHdr->nlmsg_seq   = 0;
    pSendNlHdr->nlmsg_flags = 0;

    pContext->sendIov.iov_base = (void *)pSendNlHdr;
    pContext->sendIov.iov_len  = pSendNlHdr->nlmsg_len;

    pContext->sendMsg.msg_name       = (void *)&destAddr;
    pContext->sendMsg.msg_namelen    = destAddrLen;
    pContext->sendMsg.msg_iov        = &(pContext->sendIov);
    pContext->sendMsg.msg_iovlen     = 1;
    pContext->sendMsg.msg_control    = NULL;
    pContext->sendMsg.msg_controllen = 0;
    pContext->sendMsg.msg_flags      = 0;

    /* Fill in the netlink message payload */
    pContext->pSendBuf = NLMSG_DATA(pSendNlHdr);
    pContext->pSendBuf[0] = 0x00;


    /* [2] netlink message buffer for receive */
    pRecvNlHdr = (struct nlmsghdr *)malloc(
                     NLMSG_SPACE(COMM_BUF_SIZE + 1)
                 );
    if (NULL == pRecvNlHdr)
    {
        LOG_ERROR("fail to allocate receive buffer\n");
        free( pSendNlHdr );
        pSendNlHdr = NULL;
        close( fd );
        return -1;
    }

    pRecvNlHdr->nlmsg_type  = 0;
    pRecvNlHdr->nlmsg_len   = NLMSG_SPACE(COMM_BUF_SIZE);
    pRecvNlHdr->nlmsg_pid   = 0;
    pRecvNlHdr->nlmsg_seq   = 0;
    pRecvNlHdr->nlmsg_flags = 0;

    pContext->recvIov.iov_base = (void *)pRecvNlHdr;
    pContext->recvIov.iov_len  = pRecvNlHdr->nlmsg_len;

    pContext->recvMsg.msg_name       = (void *)&destAddr;
    pContext->recvMsg.msg_namelen    = destAddrLen;
    pContext->recvMsg.msg_iov        = &(pContext->recvIov);
    pContext->recvMsg.msg_iovlen     = 1;
    pContext->recvMsg.msg_control    = NULL;
    pContext->recvMsg.msg_controllen = 0;
    pContext->recvMsg.msg_flags      = 0;

    pContext->pRecvBuf = NLMSG_DATA(pRecvNlHdr);


    pContext->pSendNlHdr = pSendNlHdr;
    pContext->pRecvNlHdr = pRecvNlHdr;
    pContext->fd = fd;

    LOG_2("netlink socket is ready\n");
    return 0;
}

/**
*  Un-initialize netlink socket.
*  @param [in]  pContext  A @ref tNetlinkContext object.
*/
static void _netlinkUninit(tNetlinkContext *pContext)
{
    if (pContext->fd > 0)
    {
        close( pContext->fd );
        pContext->fd = -1;
    }

    if ( pContext->pSendNlHdr )
    {
        free( pContext->pSendNlHdr );
        pContext->pSendNlHdr = NULL;
    }

    if ( pContext->pRecvNlHdr )
    {
        free( pContext->pRecvNlHdr );
        pContext->pRecvNlHdr = NULL;
    }

    LOG_2("netlink socket is closed\n");
}

/**
*  Thread function for netlink socket receiving.
*  @param [in]  pArg  A @ref tNetlinkContext object.
*/
static void *_netlinkRecvTask(void *pArg)
{
    tNetlinkContext *pContext = pArg;
    int len;


    pthread_setcanceltype(PTHREAD_CANCEL_DEFERRED, NULL);
    LOG_2("start the thread: %s\n", __func__);

    while ( pContext->running )
    {
        LOG_3("pid(%d) ... recvmsg\n", pContext->localAddr.nl_pid);
        pthread_testcancel();
        len = recvmsg(pContext->fd, &(pContext->recvMsg), 0);
        if (len <= 0)
        {
            LOG_ERROR("IPv4 TCP server was terminated\n");
            close( pContext->fd );
            pContext->fd = -1;
            /* notify that peer netlink shutdown */
            if ( pContext->pExitFunc )
            {
                pContext->pExitFunc(pContext->pArg, len);
            }
            break;
        }
        pthread_testcancel();

        LOG_3("<- kernel space\n");
        LOG_DUMP(
            "netlink recvmsg",
            pContext->pRecvBuf,
            pContext->pRecvNlHdr->nlmsg_len
        );

        if ( pContext->pRecvFunc )
        {
            pContext->pRecvFunc(
                pContext->pArg,
                pContext->pRecvBuf,
                pContext->pRecvNlHdr->nlmsg_len,
                pContext->pRecvNlHdr->nlmsg_flags
            );
        }
    }

    LOG_2("stop the thread: %s\n", __func__);
    pContext->running = 0;

    pthread_exit(NULL);
}

/**
*  Initialize netlink.
*  @param [in]  pFileName  Application's socket file name.
*  @param [in]  pRecvFunc  Application's receive callback function.
*  @param [in]  pExitFunc  Application's exit callback function.
*  @param [in]  pArg       Application's argument.
*  @returns  Netlink handle.
*/
tNetlinkHandle comm_netlinkInit(
    tNetlinkRecvCb  pRecvFunc,
    tNetlinkExitCb  pExitFunc,
    void           *pArg
)
{
    tNetlinkContext *pContext = NULL;
    pthread_attr_t tattr;
    int error;

    pContext = malloc( sizeof( tNetlinkContext ) );
    if (NULL == pContext)
    {
        LOG_ERROR("fail to allocate netlink context\n");
        return 0;
    }

    memset(pContext, 0x00, sizeof( tNetlinkContext ));
    pContext->localAddr.nl_family  = AF_NETLINK;
    pContext->localAddr.nl_pid     = getpid(); /* self pid */
    pContext->localAddr.nl_groups  = 0;        /* not in mcast groups */
    pContext->pRecvFunc = pRecvFunc;
    pContext->pExitFunc = pExitFunc;
    pContext->pArg = pArg;
    pContext->fd = -1;

    error = _netlinkInit( pContext );
    if (error != 0)
    {
        LOG_ERROR("failed to create a netlink socket\n");
        free( pContext );
        return 0;
    }

    if (NULL == pRecvFunc)
    {
        LOG_1("ignore netlink receive function\n");
    }

    if (NULL == pExitFunc)
    {
        LOG_1("ignore netlink exit function\n");
    }

    pContext->running = 1;

    pthread_attr_init( &tattr );
    pthread_attr_setdetachstate(&tattr, PTHREAD_CREATE_JOINABLE);

    error = pthread_create(
                 &(pContext->thread),
                 &tattr,
                 _netlinkRecvTask,
                 pContext
             );
    if (error != 0)
    {
        LOG_ERROR("failed to create netlink receiving thread\n");
        _netlinkUninit( pContext );
        free( pContext );
        return 0;
    }

    pthread_attr_destroy( &tattr );

    LOG_1("netlink initialized\n");
    return ((tNetlinkHandle)pContext);
}

/**
*  Un-initialize netlink.
*  @param [in]  handle  Netlink handle.
*/
void comm_netlinkUninit(tNetlinkHandle handle)
{
    tNetlinkContext *pContext = (tNetlinkContext *)handle;

    if ( pContext )
    {
        pthread_cancel( pContext->thread );

        pContext->running = 0;
        _netlinkUninit( pContext );
        free( pContext );

        pthread_join(pContext->thread, NULL);
        LOG_1("netlink un-initialized\n");
    }
}

/**
*  Send netlink message to kernel space.
*  @param [in]  handle  Netlink handle.
*  @param [in]  pData   A pointer of data buffer.
*  @param [in]  size    Data size.
*  @param [in]  type    Message type.
*  @param [in]  flags   Message flags.
*  @param [in]  seqNum  Message sequence number.
*  @returns  Message length (-1 is failed).
*/
int comm_netlinkSendToKernel(
    tNetlinkHandle  handle,
    unsigned char  *pData,
    unsigned short  size,
    unsigned short  type,
    unsigned short  flags,
    unsigned int    seqNum
)
{
    tNetlinkContext *pContext = (tNetlinkContext *)handle;
    int error;


    if (NULL == pContext)
    {
        LOG_ERROR("%s: pContext is NULL\n", __func__);
        return -1;
    }

    if (pContext->fd < 0)
    {
        LOG_ERROR("%s: netlink socket is not ready\n", __func__);
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

    LOG_3("-> kernel space\n");
    LOG_DUMP("netlink sendmsg", pData, size);

    pContext->pSendNlHdr->nlmsg_type  = type;
    pContext->pSendNlHdr->nlmsg_flags = flags;
    pContext->pSendNlHdr->nlmsg_seq   = seqNum;
    pContext->pSendNlHdr->nlmsg_len   = size;
    memcpy(pContext->pSendBuf, pData, size);

    error = sendmsg(pContext->fd, &(pContext->sendMsg), 0);
    if (error < 0)
    {
        LOG_ERROR("fail to send netlink message\n");
        perror( "sendmsg" );
    }

    return error;
}

