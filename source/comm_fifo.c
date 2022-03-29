#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <pthread.h>
#include <fcntl.h>
#include <sys/stat.h>
#include "comm_if.h"
#include "comm_log.h"


typedef struct _tFifoContext
{
    char           fileName[256];
    int            make;
    int            fd;

    tFifoGetCb     pGetFunc;
    tFifoCloseCb   pCloseFunc;
    void          *pArg;
    pthread_t      thread;
    int            running;

    unsigned char  recvMsg[COMM_BUF_SIZE+1];
} tFifoContext;


/**
*  Create and open read only FIFO.
*  @param [in]  pContext  A @ref tFifoContext object.
*  @returns  Success(0) or failure(-1).
*/
static int _openFifoRead(tFifoContext *pContext)
{
    int fd;


    if ( pContext->make )
    {
        if (0 == access(pContext->fileName, F_OK))
        {
            LOG_1("FIFO %s exists\n", pContext->fileName);
            goto _OPEN;
        }

        if (mkfifo(pContext->fileName, (S_IFIFO|0644)) < 0)
        {
            perror( "mkfifo" );
            return -1;
        }

        LOG_2("FIFO %s is maked\n", pContext->fileName);
    }

_OPEN:
    fd = open(pContext->fileName, O_RDONLY);
    if (fd < 0)
    {
        perror( "open" );
        return -1;
    }

    pContext->fd = fd;

    LOG_2("FIFO %s is ready\n", pContext->fileName);
    return 0;
}

/**
*  Close read only FIFO.
*  @param [in]  pContext  A @ref tFifoContext object.
*/
static void _closeFifoRead(tFifoContext *pContext)
{
    if (pContext->fd > 0)
    {
        close( pContext->fd );
        pContext->fd = -1;
    }

    if ( pContext->make )
    {
        unlink( pContext->fileName );
    }

    LOG_2("FIFO %s is closed\n", pContext->fileName);
}

/**
*  Thread function for FIFO read.
*  @param [in]  pArg  A @ref tFifoContext object.
*/
static void *_fifoGetTask(void *pArg)
{
    tFifoContext *pContext = pArg;
    int len;


    LOG_2("start the thread: %s\n", __func__);

    while ( pContext->running )
    {
        LOG_3("fd(%d) ... read\n", pContext->fd);
        len = read(
                  pContext->fd,
                  pContext->recvMsg,
                  COMM_BUF_SIZE
              );
        if (len <= 0)
        {
            LOG_ERROR("FIFO is closed\n");
            close( pContext->fd );
            pContext->fd = -1;
            /* notify FIFO is closed */
            if ( pContext->pCloseFunc )
            {
                pContext->pCloseFunc(pContext->pArg, len);
            }
            break;
        }

        LOG_3("<- %s\n", pContext->fileName);
        LOG_DUMP("FIFO: read", pContext->recvMsg, len);

        if ( pContext->pGetFunc )
        {
            pContext->pGetFunc(
                          pContext->pArg,
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
*  Initial read only FIFO.
*  @param [in]  pFileName   FIFO file name.
*  @param [in]  make        Make FIFO or not.
*  @param [in]  pGetFunc    Application's get callback function.
*  @param [in]  pCloseFunc  Application's close callback function.
*  @param [in]  pArg        Application's argument.
*  @returns  FIFO handle.
*/
tFifoHandle comm_fifoReadInit(
    char         *pFileName,
    int           make,
    tFifoGetCb    pGetFunc,
    tFifoCloseCb  pCloseFunc,
    void         *pArg
)
{
    tFifoContext *pContext = NULL;
    int error;


    pContext = malloc( sizeof( tFifoContext ) );
    if (NULL == pContext)
    {
        LOG_ERROR("fail to allocate FIFO context\n");
        return 0;
    }

    memset(pContext, 0x00, sizeof( tFifoContext ));
    strncpy(pContext->fileName, pFileName, 255);
    pContext->make = make;
    pContext->pGetFunc = pGetFunc;
    pContext->pCloseFunc = pCloseFunc;
    pContext->pArg = pArg;
    pContext->fd = -1;

    error = _openFifoRead( pContext );
    if (error != 0)
    {
        LOG_ERROR("fail to open FIFO %s\n", pFileName);
        free( pContext );
        return 0;
    }

    if (NULL == pGetFunc)
    {
        LOG_1("ignore FIFO get function\n");
    }

    if (NULL == pCloseFunc)
    {
        LOG_1("ignore FIFO close function\n");
    }

    pContext->running = 1;

    error = pthread_create(
                &(pContext->thread),
                NULL,
                _fifoGetTask,
                pContext
            );
    if (error != 0)
    {
        LOG_ERROR("fail to create FIFO get thread\n");
        _closeFifoRead( pContext );
        free( pContext );
        return 0;
    }


    LOG_1("FIFO read only initialized\n");
    return ((tFifoHandle)pContext);
}

/**
*  Un-initial read only FIFO.
*  @param [in]  handle  FIFO handle.
*/
void comm_fifoReadUninit(tFifoHandle handle)
{
    tFifoContext *pContext = (tFifoContext *)handle;

    if ( pContext )
    {
        if ( pContext->running )
        {
            pContext->running = 0;
            pthread_cancel( pContext->thread );
            pthread_join(pContext->thread, NULL);
            usleep(1000);
        }

        _closeFifoRead( pContext );

        free( pContext );
        LOG_1("FIFO read only un-initialized\n");
    }
}


/**
*  Create and open write only FIFO.
*  @param [in]  pContext  A @ref tFifoContext object.
*  @returns  Success(0) or failure(-1).
*/
static int _openFifoWrite(tFifoContext *pContext)
{
    int fd;


    if ( pContext->make )
    {
        if (0 == access(pContext->fileName, F_OK))
        {
            LOG_1("FIFO %s exists\n", pContext->fileName);
            goto _OPEN;
        }

        if (mkfifo(pContext->fileName, (S_IFIFO|0644)) < 0)
        {
            perror( "mkfifo" );
            return -1;
        }

        LOG_2("FIFO %s is maked\n", pContext->fileName);
    }

_OPEN:
    fd = open(pContext->fileName, O_WRONLY);
    if (fd < 0)
    {
        perror( "open" );
        return -1;
    }

    pContext->fd = fd;

    LOG_2("FIFO %s is ready\n", pContext->fileName);
    return 0;
}

/**
*  Close write only FIFO.
*  @param [in]  pContext  A @ref tFifoContext object.
*/
static void _closeFifoWrite(tFifoContext *pContext)
{
    if (pContext->fd > 0)
    {
        close( pContext->fd );
        pContext->fd = -1;
    }

    if ( pContext->make )
    {
        unlink( pContext->fileName );
    }

    LOG_2("FIFO %s is closed\n", pContext->fileName);
}

/**
*  Initial write only FIFO.
*  @param [in]  pFileName  FIFO file name.
*  @param [in]  make       Make FIFO or not.
*  @returns  FIFO handle.
*/
tFifoHandle comm_fifoWriteInit(char *pFileName, int make)
{
    tFifoContext *pContext = NULL;
    int error;


    pContext = malloc( sizeof( tFifoContext ) );
    if (NULL == pContext)
    {
        LOG_ERROR("fail to allocate FIFO context\n");
        return 0;
    }

    memset(pContext, 0x00, sizeof( tFifoContext ));
    strncpy(pContext->fileName, pFileName, 255);
    pContext->make = make;
    pContext->fd = -1;

    error = _openFifoWrite( pContext );
    if (error != 0)
    {
        LOG_ERROR("fail to open FIFO %s\n", pFileName);
        free( pContext );
        return 0;
    }

    LOG_1("FIFO write only initialized\n");
    return ((tFifoHandle)pContext);
}

/**
*  Un-initial write only FIFO.
*  @param [in]  handle  FIFO handle.
*/
void comm_fifoWriteUninit(tFifoHandle handle)
{
    tFifoContext *pContext = (tFifoContext *)handle;

    if ( pContext )
    {
        _closeFifoWrite( pContext );

        free( pContext );
        LOG_1("FIFO write only un-initialized\n");
    }
}

/**
*  Put data into write only FIFO.
*  @param [in]  handle  FIFO handle.
*  @param [in]  pData  A pointer of data buffer.
*  @param [in]  size   Data size.
*  @returns  Message length (-1 is failed).
*/
int comm_fifoWritePut(
    tFifoHandle     handle,
    unsigned char  *pData,
    unsigned short  size
)
{
    tFifoContext *pContext = (tFifoContext *)handle;
    int error;


    if (NULL == pContext)
    {
        LOG_ERROR("%s: pContext is NULL\n", __func__);
        return -1;
    }

    if (pContext->fd < 0)
    {
        LOG_ERROR("%s: %s is not ready\n", __func__, pContext->fileName);
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

    LOG_3("-> %s\n", pContext->fileName);
    LOG_DUMP("FIFO: write", pData, size);

    error = write(pContext->fd, pData, size);
    if (error < 0)
    {
        LOG_ERROR("fail to write FIFO\n");
        perror( "write" );
    }

    return error;
}

