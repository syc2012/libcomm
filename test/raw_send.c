#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "comm_if.h"


#define APP_NAME "raw_send"


unsigned char g_pingReq[] = {
    /* Ethernet (14-byte) */
    0x00, 0x23, 0x24, 0x37, 0x7b, 0xfc,
    0x00, 0x26, 0x6c, 0x4a, 0xde, 0x8f,
    0x08, 0x00,
    /* IP (20-byte) */
    0x45, 0x00, 0x00, 0x54,
    0x00, 0x00, 0x40, 0x00,
    0x40, 0x01, 0xb4, 0xcb,
    0xc0, 0xa8, 0x02, 0x47,
    0xc0, 0xa8, 0x02, 0x46,
    /* ICMP (64-byte) */
    0x08, 0x00, 0x8a, 0xdd, 0xd3, 0x19, 0x00, 0x01,
    0x5f, 0xb7, 0x43, 0x62, 0x06, 0xeb, 0x05, 0x00,
    0x08, 0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f,
    0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17,
    0x18, 0x19, 0x1a, 0x1b, 0x1c, 0x1d, 0x1e, 0x1f,
    0x20, 0x21, 0x22, 0x23, 0x24, 0x25, 0x26, 0x27,
    0x28, 0x29, 0x2a, 0x2b, 0x2c, 0x2d, 0x2e, 0x2f,
    0x30, 0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37
};

int main(int argc,char *argv[])
{
    tRawHandle handle;
    unsigned char *pMac;
    int i;


    if (argc < 2)
    {
        printf("Usage: %s netdevice\n\n", APP_NAME);
        return -1;
    }

    #ifdef QUIET
    comm_setLogMask( LOG_MASK_NONE );
    #else
    comm_setLogMask( LOG_MASK_ALL );
    #endif

    handle = comm_rawSockInit(argv[1], NULL, NULL);
    if (0 == handle)
    {
        printf("[%s] initial raw socket failed\n\n", APP_NAME);
        return -1;
    }

    pMac = comm_rawGetHwAddr( handle );
    if ( pMac )
    {
        printf(
            "[%s] MAC address: %02X:%02X:%02X:%02X:%02X:%02X\n\n",
            APP_NAME,
            pMac[0],
            pMac[1],
            pMac[2],
            pMac[3],
            pMac[4],
            pMac[5]
        );
    }

    printf("[%s] %u bytes\n", APP_NAME, sizeof(g_pingReq));
    for (i=0; i<sizeof(g_pingReq); i++)
    {
        if ((i != 0) && ((i % 16) == 0))
        {
            printf("\n");
        }
        printf(" %02X", g_pingReq[i]);
    }
    printf("\n\n");

    comm_rawSockSend(handle, g_pingReq, sizeof(g_pingReq));

    comm_rawSockUninit( handle );

    return 0;
}

