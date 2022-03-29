#ifndef __NPTP_SERV_H__
#define __NPTP_SERV_H__

#include "comm_if.h"


#if 0
# define IPC_DGRAM_FILE_PATH  "/var/run/nptp_serv_dgram.sock"
# define IPC_STREAM_FILE_PATH "/var/run/nptp_serv_stream.sock"
#else
# define IPC_DGRAM_FILE_PATH  "./nptp_serv_dgram.sock"
# define IPC_STREAM_FILE_PATH "./nptp_serv_stream.sock"
#endif

#define PRINT( a... ) printf("[nptp] " a)

#define MAX_MAPPING_NUM 64

typedef struct _tMapping
{
    tTcpIpv4ServerHandle   tcpServerHandle;
    tIpcStreamClientHandle ipcStreamHandle;
    unsigned char   tcpConnect;
    unsigned char   pipeConnect;
    unsigned short  tcpPort;
    char            pipePath[160];
    char            pipeName[80];
    char            descript[80];
    int             wd;
    tTcpUser       *pTcpUser;
    int             index;
} tMapping;

extern tMapping *g_pMapping[MAX_MAPPING_NUM];
extern int g_mappingNum;


void serv_list(void);


#endif
