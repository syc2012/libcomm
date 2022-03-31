#ifndef __COMM_IF_H__
#define __COMM_IF_H__

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/un.h>
#include <netinet/in.h>
#include <arpa/inet.h>


#define COMM_BUF_SIZE (4095)


typedef enum
{
    LOG_MASK_NONE = 0x0,
    LOG_MASK_1    = 0x1,
    LOG_MASK_2    = 0x2,
    LOG_MASK_3    = 0x4,
    LOG_MASK_ALL  = 0x7
} eLogMask;

void comm_setLogMask(int mask);
int  comm_getLogMask(void);
void comm_setDumpFlag(int flag);
int  comm_getDumpFlag(void);


/************************ Begin of UDP ************************/
typedef unsigned long  tUdpIpv4Handle;
typedef unsigned long  tUdpIpv6Handle;
/*
*  UDP IPv4:
*    pAddr ==> (struct sockaddr_in *)
*
*  UDP IPv6:
*    pAddr ==> (struct sockaddr_in6 *)
*/
typedef void (*tUdpRecvCb)(
            void            *pArg,
            unsigned char   *pData,
            unsigned short   size,
            struct sockaddr *pAddr
        );

tUdpIpv4Handle comm_udpIpv4Init(
                   unsigned short  portNum,
                   tUdpRecvCb      pRecvFunc,
                   void           *pArg
               );
void comm_udpIpv4Uninit(tUdpIpv4Handle handle);
int  comm_udpIpv4Send(
         tUdpIpv4Handle  handle,
         char           *pIpStr,
         unsigned short  portNum,
         unsigned char  *pData,
         unsigned short  size
     );
int  comm_udpIpv4GetAddr(char *pIfName, unsigned char *pIpv4Addr);

tUdpIpv6Handle comm_udpIpv6Init(
                   unsigned short  portNum,
                   tUdpRecvCb      pRecvFunc,
                   void           *pArg
               );
void comm_udpIpv6Uninit(tUdpIpv6Handle handle);
int  comm_udpIpv6Send(
         tUdpIpv6Handle  handle,
         char           *pIpStr,
         unsigned short  portNum,
         unsigned char  *pData,
         unsigned short  size
     );
int  comm_udpIpv6GetAddr(char *pIfName, unsigned char *pIpv6Addr);
/************************ End   of UDP ************************/


/************************ Begin of TCP Client ************************/
typedef unsigned long  tTcpIpv4ClientHandle;
typedef unsigned long  tTcpIpv6ClientHandle;
typedef void (*tTcpClientRecvCb)(
                 void           *pArg,
                 unsigned char  *pData,
                 unsigned short  size
             );
typedef void (*tTcpClientExitCb)(void *pArg, int code);

tTcpIpv4ClientHandle comm_tcpIpv4ClientInit(
                         unsigned short    portNum,
                         tTcpClientRecvCb  pRecvFunc,
                         tTcpClientExitCb  pExitFunc,
                         void             *pArg
                     );
void comm_tcpIpv4ClientUninit(tTcpIpv4ClientHandle handle);
int  comm_tcpIpv4ClientConnect(
         tTcpIpv4ClientHandle  handle,
         char                 *pAddr,
         unsigned short        port
     );
int  comm_tcpIpv4ClientSend(
         tTcpIpv4ClientHandle  handle,
         unsigned char        *pData,
         unsigned short        size
     );

tTcpIpv6ClientHandle comm_tcpIpv6ClientInit(
                         unsigned short    portNum,
                         tTcpClientRecvCb  pRecvFunc,
                         tTcpClientExitCb  pExitFunc,
                         void             *pArg
                     );
void comm_tcpIpv6ClientUninit(tTcpIpv6ClientHandle handle);
int  comm_tcpIpv6ClientConnect(
         tTcpIpv6ClientHandle  handle,
         char                 *pAddr,
         unsigned short        port
     );
int  comm_tcpIpv6ClientSend(
         tTcpIpv6ClientHandle  handle,
         unsigned char        *pData,
         unsigned short        size
     );
/************************ End   of TCP Client ************************/


/************************ Begin of TCP Server ************************/
typedef unsigned long  tTcpIpv4ServerHandle;
typedef unsigned long  tTcpIpv6ServerHandle;
typedef struct _tTcpUser
{
    void                *pServer;
    struct sockaddr_in   addrIpv4;
    struct sockaddr_in6  addrIpv6;
    int                  fd;
    pthread_t            thread;
    unsigned char        recvMsg[COMM_BUF_SIZE+1];
} tTcpUser;

typedef void (*tTcpServerRecvCb)(
                 void           *pArg,
                 tTcpUser       *pUser,
                 unsigned char  *pData,
                 unsigned short  size
             );
typedef void (*tTcpServerAcptCb)(void *pArg, tTcpUser *pUser);
typedef void (*tTcpServerExitCb)(void *pArg, tTcpUser *pUser);

tTcpIpv4ServerHandle comm_tcpIpv4ServerInit(
                         unsigned short    portNum,
                         int               maxUserNum,
                         tTcpServerAcptCb  pAcptFunc,
                         tTcpServerExitCb  pExitFunc,
                         tTcpServerRecvCb  pRecvFunc,
                         void             *pArg
                     );
void comm_tcpIpv4ServerUninit(tTcpIpv4ServerHandle handle);
int  comm_tcpIpv4ServerSend(
         tTcpUser       *pUser,
         unsigned char  *pData,
         unsigned short  size
     );
void comm_tcpIpv4ServerSendAllClient(
         tTcpIpv4ServerHandle  handle,
         unsigned char        *pData,
         unsigned short        size
     );
int  comm_tcpIpv4ServerGetClientNum(tTcpIpv4ServerHandle handle);

tTcpIpv6ServerHandle comm_tcpIpv6ServerInit(
                         unsigned short    portNum,
                         int               maxUserNum,
                         tTcpServerAcptCb  pAcptFunc,
                         tTcpServerExitCb  pExitFunc,
                         tTcpServerRecvCb  pRecvFunc,
                         void             *pArg
                     );
void comm_tcpIpv6ServerUninit(tTcpIpv6ServerHandle handle);
int  comm_tcpIpv6ServerSend(
         tTcpUser       *pUser,
         unsigned char  *pData,
         unsigned short  size
     );
void comm_tcpIpv6ServerSendAllClient(
         tTcpIpv6ServerHandle  handle,
         unsigned char        *pData,
         unsigned short        size
     );
int  comm_tcpIpv6ServerGetClientNum(tTcpIpv6ServerHandle handle);
/************************ End   of TCP Server ************************/


/************************ Begin of Raw ************************/
typedef unsigned long  tRawHandle;
typedef void (*tRawRecvCb)(
            void           *pArg,
            unsigned char  *pData,
            unsigned short  size
        );

tRawHandle comm_rawSockInit(
               char       *pEthDev,
               tRawRecvCb  pRecvFunc,
               void       *pArg
           );
void comm_rawSockUninit(tRawHandle handle);
int  comm_rawSockSend(
         tRawHandle      handle,
         unsigned char  *pData,
         unsigned short  size
     );
int  comm_rawPromiscMode(tRawHandle handle, int enable);
int  comm_rawGetMtu(tRawHandle handle);
unsigned char *comm_rawGetHwAddr(tRawHandle handle);
/************************ End   of Raw ************************/


/************************ Begin of FIFO ************************/
typedef unsigned long  tFifoHandle;
typedef void (*tFifoGetCb)(
                 void           *pArg,
                 unsigned char  *pData,
                 unsigned short  size
             );
typedef void (*tFifoCloseCb)(void *pArg, int code);

tFifoHandle comm_fifoReadInit(
                char         *pFileName,
                int           make,
                tFifoGetCb    pGetFunc,
                tFifoCloseCb  pCloseFunc,
                void         *pArg
            );
void comm_fifoReadUninit(tFifoHandle handle);

tFifoHandle comm_fifoWriteInit(char *pFileName, int make);
void comm_fifoWriteUninit(tFifoHandle handle);
int  comm_fifoWritePut(
         tFifoHandle     handle,
         unsigned char  *pData,
         unsigned short  size
     );
/************************ End   of FIFO ************************/


/************************ Begin of IPC Dgram ************************/
typedef unsigned long  tIpcDgramHandle;
typedef void (*tIpcDgramRecvCb)(
                 void           *pArg,
                 unsigned char  *pData,
                 unsigned short  size,
                 char           *pPath
             );

tIpcDgramHandle comm_ipcDgramInit(
                    char            *pFileName,
                    tIpcDgramRecvCb  pRecvFunc,
                    void            *pArg
                );
void comm_ipcDgramUninit(tIpcDgramHandle handle);
int  comm_ipcDgramSendTo(
         tIpcDgramHandle  handle,
         char            *pFileName,
         unsigned char   *pData,
         unsigned short   size
     );
/************************ End   of IPC Dgram ************************/


/************************ Begin of IPC Stream ************************/
typedef unsigned long  tIpcStreamClientHandle;
typedef void (*tIpcClientRecvCb)(
                 void           *pArg,
                 unsigned char  *pData,
                 unsigned short  size
             );
typedef void (*tIpcClientExitCb)(void *pArg, int code);

tIpcStreamClientHandle comm_ipcStreamClientInit(
                           char             *pFileName,
                           tIpcClientRecvCb  pRecvFunc,
                           tIpcClientExitCb  pExitFunc,
                           void             *pArg
                       );
void comm_ipcStreamClientUninit(tIpcStreamClientHandle handle);
int  comm_ipcStreamClientConnect(
         tIpcStreamClientHandle  handle,
         char                   *pFileName
     );
int  comm_ipcStreamClientSend(
         tIpcStreamClientHandle  handle,
         unsigned char          *pData,
         unsigned short          size
     );

typedef unsigned long  tIpcStreamServerHandle;
typedef struct _tIpcUser
{
    void          *pServer;
    char           fileName[256];
    int            fd;
    pthread_t      thread;
    unsigned char  recvMsg[COMM_BUF_SIZE+1];
} tIpcUser;

typedef void (*tIpcServerRecvCb)(
                 void           *pArg,
                 tIpcUser       *pUser,
                 unsigned char  *pData,
                 unsigned short  size
             );
typedef void (*tIpcServerAcptCb)(void *pArg, tIpcUser *pUser);
typedef void (*tIpcServerExitCb)(void *pArg, tIpcUser *pUser);

tIpcStreamServerHandle comm_ipcStreamInitServer(
                           char             *pFileName,
                           int               maxUserNum,
                           tIpcServerAcptCb  pAcptFunc,
                           tIpcServerExitCb  pExitFunc,
                           tIpcServerRecvCb  pRecvFunc,
                           void             *pArg
                       );
void comm_ipcStreamUninitServer(tIpcStreamServerHandle handle);
int  comm_ipcStreamServerSend(
         tIpcUser       *pUser,
         unsigned char  *pData,
         unsigned short  size
     );
void comm_ipcStreamServerSendAllClient(
         tIpcStreamServerHandle  handle,
         unsigned char          *pData,
         unsigned short          size
     );
int  comm_ipcStreamServerGetClientNum(tIpcStreamServerHandle handle);
/************************ End   of IPC Stream ************************/


/************************ Begin of Netlink ************************/
typedef unsigned long  tNetlinkHandle;
typedef void (*tNetlinkRecvCb)(
                 void           *pArg,
                 unsigned char  *pData,
                 unsigned short  size,
                 unsigned short  flags
             );
typedef void (*tNetlinkExitCb)(void *pArg, int code);

tNetlinkHandle comm_netlinkInit(
                   tNetlinkRecvCb  pRecvFunc,
                   tNetlinkExitCb  pExitFunc,
                   void           *pArg
               );
void comm_netlinkUninit(tNetlinkHandle handle);
int  comm_netlinkSendToKernel(
         tNetlinkHandle  handle,
         unsigned char  *pData,
         unsigned short  size,
         unsigned short  type,
         unsigned short  flags,
         unsigned int    seqNum
     );
/************************ End   of Netlink ************************/


/************************ Begin of UART ************************/
typedef unsigned long  tUartHandle;
typedef void (*tUartRecvCb)(
            void           *pArg,
            unsigned char  *pData,
            unsigned short  size
        );

tUartHandle uart_openDev(char *pDevName, tUartRecvCb pRecvFunc, void *pArg);
void uart_closeDev(tUartHandle handle);
int  uart_configDev(
         tUartHandle  handle,
         int          baudRate,
         int          parity,
         int          waitTime
     );
int  uart_send(tUartHandle handle, unsigned char *pData, unsigned short size);
int  uart_baudRate(int baudRate);
/************************ End   of UART ************************/



#endif /* __COMM_IF_H__ */
