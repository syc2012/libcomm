#include "comm_if.h"
#include "comm_log.h"


#define IPC_LOG_SIZE 255

/* A @ref eLogMask enumeration */
int g_logMask = LOG_MASK_1;

/* Boolean */
int g_dumpFlag = 0;


/**
*  Set log mask.
*  @param [in]  mask  A @ref eLogMask enumeration.
*/
void comm_setLogMask(int mask)
{
    g_logMask = (mask & LOG_MASK_ALL);
}

/**
*  Get log mask.
*  @returns  A @ref eLogMask enumeration.
*/
int comm_getLogMask(void)
{
    return g_logMask;
}

/**
*  Set dump flag on or off.
*  @param [in]  flag  On(1) or Off(0).
*/
void comm_setDumpFlag(int flag)
{
    if ( flag )
    {
        printf("[DUMP] turn on\n");
        g_dumpFlag = 1;
    }
    else
    {
        printf("[DUMP] turn off\n");
        g_dumpFlag = 0;
    }
}

/**
*  Get dump flag.
*  @returns  On(1) or Off(0).
*/
int comm_getDumpFlag(void)
{
    return g_dumpFlag;
}

/**
*  Dump memory.
*  @param [in]  pName  Memory description.
*  @param [in]  pAddr  Memory address to dump.
*  @param [in]  size   Memory size.
*/
void comm_dump(char *pName, void *pAddr, unsigned int size)
{
    unsigned char *pByte = pAddr;
    unsigned int i;

    printf("[DUMP] %s\n", pName);

    if (NULL == pByte)
    {
        printf(" NULL\n");
    }
    else
    {
        for (i=0; i<size; i++)
        {
            #if 1
            if ((i != 0) && ((i % 16) == 0))
            {
                printf("\n");
            }
            printf(" %02X", pByte[i]);
            #else
            if ((pByte[i] > 0x1F) && (pByte[i] < 0x7F))
            {
                printf(" %c", pByte[i]);
            }
            else
            {
                printf(" .");
            }
            #endif
        }
        printf("\n");
        printf(" %u bytes\n", size);
    }

    printf("[DUMP]\n\n");
}

/**
*  Print log.
*  @param [in]  pPrefix  Log prefix.
*  @param [in]  pFormat  Print format.
*/
void comm_print(char *pPrefix, char *pFormat, ...)
{
    char  msg[IPC_LOG_SIZE+1];
    int   msgLen = 0;
    va_list args;


    /* append prefix to log message */
    msgLen = sprintf(msg, "[%s] ", pPrefix);

    va_start(args, pFormat);

    /* log message copy */
    msgLen += vsnprintf(
                  (msg + msgLen),
                  (IPC_LOG_SIZE - msgLen),
                  pFormat,
                  args
              );

    va_end(args);

    printf("%s", msg);
}


