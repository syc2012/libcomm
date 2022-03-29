#ifndef __COMM_LOG_H__
#define __COMM_LOG_H__

#include <stdio.h>
#include <stdarg.h>


#define LOG_ERROR( a... ) comm_print("ERRO", ##a)
#define LOG_WARN( a... )  comm_print("WARN", ##a)

#define LOG_1( a... ) \
    if (g_logMask & LOG_MASK_1) comm_print("INFO", ##a)

#define LOG_2( a... ) \
    if (g_logMask & LOG_MASK_2) comm_print("INFO", ##a)

#define LOG_3( a... ) \
    if (g_logMask & LOG_MASK_3) comm_print("INFO", ##a)

#define LOG_DUMP(pName, pAddr, size) \
    if ( g_dumpFlag ) comm_dump(pName, pAddr, size)


/* A @ref eLogMask enumeration */
extern int g_logMask;

/* Boolean */
extern int g_dumpFlag;


/**
*  Dump memory.
*  @param [in]  pName  Memory description.
*  @param [in]  pAddr  Memory address to dump.
*  @param [in]  size   Memory size.
*/
void comm_dump(char *pName, void *pAddr, unsigned int size);

/**
*  Print log.
*  @param [in]  pPrefix  Log prefix.
*  @param [in]  pFormat  Print format.
*/
void comm_print(
    char *pPrefix,
    char *pFormat,
    ...
) __attribute__ ((format (printf, 2, 3)));


#endif /* __COMM_LOG_H__ */
