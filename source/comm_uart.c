#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <pthread.h>
#include <fcntl.h>
#include <termios.h> /*termio.h for serial IO api*/ 
#include "comm_if.h"
#include "comm_log.h"


typedef struct _tUartContext
{
    char           devName[32];
    int            baudRate;
    int            parity;
    int            waitTime;
    int            fd;

    tUartRecvCb    pRecvFunc;
    void          *pArg;
    pthread_t      thread;
    int            running;

    unsigned char  recvMsg[COMM_BUF_SIZE+1];
} tUartContext;


/**
*  Thread function for the UART receiving.
*  @param [in]  pArg  A @ref tUartContext object.
*/
static void *_uartRecvTask(void *pArg)
{
    tUartContext *pContext = pArg;
    int len;


    pthread_setcanceltype(PTHREAD_CANCEL_DEFERRED, NULL);
    LOG_2("start the thread: %s\n", __func__);

    while ( pContext->running )
    {
        LOG_3("UART ... read\n");
        pthread_testcancel();
        len = read(
                  pContext->fd,
                  pContext->recvMsg,
                  COMM_BUF_SIZE
              );
        if (len < 0)
        {
            switch ( errno )
            {
                case EAGAIN:
                    LOG_ERROR("%s: read EAGAIN\n", __func__);
                    pthread_testcancel();
                    usleep(5 * 1000);
                    pthread_testcancel();
                    continue;
                    break;
             
                default:
                    LOG_ERROR("%s: read error(%s)\n", __func__, strerror(errno));
                    pContext->running = 0;
                    break;  
            }
        }
        else if (0 == len)
        {
            if (pContext->waitTime < 0)
            {
                LOG_WARN("%s: device is lost\n", __func__);
                pContext->running = 0;
            }
        }
        else
        {
            pContext->recvMsg[len] = 0x00;

            LOG_3("<- %s\n", pContext->devName);
            LOG_DUMP("UART read", pContext->recvMsg, len);

            if ( pContext->pRecvFunc )
            {
                pContext->pRecvFunc(
                    pContext->pArg,
                    pContext->recvMsg,
                    len
                );
            }
        }
        pthread_testcancel();
    }

    LOG_2("stop the thread: %s\n", __func__);
    pContext->running = 0;

    pthread_exit(NULL);
}

/**
*  UART open device.
*  @param [in]  pDevName   Device name.
*  @param [in]  pRecvFunc  Application's receive callback function.
*  @param [in]  pArg       Application's argument.
*  @returns  A @ref tUartHandle object.
*/
tUartHandle uart_openDev(char *pDevName, tUartRecvCb pRecvFunc, void *pArg)
{
    tUartContext *pContext = NULL;
    pthread_attr_t tattr;
    int error;
    int fd;


    pContext = malloc( sizeof( tUartContext ));
    if (NULL == pContext)
    {
        LOG_ERROR("fail to allocate UART context\n");
        return 0;
    }

    fd = open(pDevName, (O_RDWR|O_NOCTTY));
    if (fd < 0)
    {
        LOG_ERROR("fail to open device %s\n", pDevName);
        return 0;
    }

    memset(pContext, 0x00, sizeof( tUartContext ));
    strncpy(pContext->devName, pDevName, 31);
    pContext->fd = fd;
    pContext->pRecvFunc = pRecvFunc;
    pContext->pArg = pArg;

    if (NULL == pRecvFunc)
    {
        LOG_1("ignore UART receive function\n");
    }

    pContext->running = 1;

    pthread_attr_init( &tattr );
    pthread_attr_setdetachstate(&tattr, PTHREAD_CREATE_JOINABLE);

    error = pthread_create(
                &(pContext->thread),
                &tattr,
                _uartRecvTask,
                pContext
            );
    if (error != 0)
    {
        LOG_ERROR("fail to create UART receiving thread\n");
        close( fd );
        free( pContext );
        return 0;
    }

    pthread_attr_destroy( &tattr );

    LOG_1("UART device open\n");
    return ((tUartHandle)pContext);
}

/**
*  UART close device.
*  @param [in]  handle  A @ref tUartHandle object.
*/
void uart_closeDev(tUartHandle handle)
{
    tUartContext *pContext = (tUartContext *)handle;

    if ( pContext )
    {
        pthread_cancel( pContext->thread );

        pContext->running = 0;
        if (pContext->fd > 0)
        {
            close( pContext->fd );
        }

        pthread_join(pContext->thread, NULL);
        free( pContext );
        LOG_1("UART device close\n");
    }
}

/**
*  UART configure device.
*  @param [in]  handle    UART handle.
*  @param [in]  baudRate  UART baud rate.
*  @param [in]  parity    UART parity check.
*  @param [in]  waitTime  UART wait time (-1 for non-blocking mode).
*  @returns  Success(0) or fail(-1).
*/
int uart_configDev(
    tUartHandle  handle,
    int          baudRate,
    int          parity,
    int          waitTime
)
{
    tUartContext *pContext = (tUartContext *)handle;
    struct termios tty;
    int blockingMode;
    int speed;


    if (NULL == pContext)
    {
        LOG_ERROR("%s: pContext is NULL\n", __func__);
        return -1;
    }

    if (pContext->fd < 0)
    {
        LOG_ERROR("%s: UART is not ready\n", __func__);
        return -1;
    }

    speed = uart_baudRate( baudRate );
    if (0 == speed)
    {
        LOG_ERROR("%s: incorrect baud rate %d\n", __func__, baudRate);
        return -1;
    }

    if (waitTime > 255) waitTime = 255;
    blockingMode = (waitTime < 0);
   
    memset(&tty, 0, sizeof tty);
    if (tcgetattr(pContext->fd, &tty) != 0) /* save current serial port settings */
    {
        LOG_ERROR("%s: tcgetattr error(%s)\n", __func__, strerror(errno));
        return -1;
    }

    cfsetospeed(&tty, speed);
    cfsetispeed(&tty, speed);

    tty.c_cflag  = (tty.c_cflag & ~CSIZE) | CS8;  // 8-bit chars
    // disable IGNBRK for mismatched speed tests; otherwise receive break
    // as \000 chars
    tty.c_iflag &= ~IGNBRK;  // disable break processing
    tty.c_lflag  = 0;        // no signaling chars, no echo,
                             // no canonical processing
    tty.c_oflag  = 0;        // no remapping, no delays
    tty.c_cc[VMIN]  = ((1 == blockingMode) ? 1 : 0);         // read doesn't block
    tty.c_cc[VTIME] = ((1 == blockingMode) ? 0 : waitTime);  // in unit of 100 milli-sec for set timeout value
    tty.c_iflag &= ~(IXON | IXOFF | IXANY);  // shut off xon/xoff ctrl
    tty.c_cflag |= (CLOCAL | CREAD);    // ignore modem controls,
                                        // enable reading
    tty.c_cflag &= ~(PARENB | PARODD);  // shut off parity
    tty.c_cflag |= parity;
    tty.c_cflag &= ~CSTOPB;
    tty.c_cflag &= ~CRTSCTS;

    if (tcsetattr(pContext->fd, TCSANOW, &tty) != 0)
    {
        LOG_ERROR("%s: tcsetattr error(%s)\n", __func__, strerror(errno));
        return -1;
    }

    pContext->baudRate = baudRate;
    pContext->parity   = parity;
    pContext->waitTime = waitTime;

    return 0;
}

/**
*  UART send data.
*  @param [in]  handle  A @ref tUartHandle object.
*  @param [in]  pData   A pointer of data buffer.
*  @param [in]  size    Data size.
*  @returns  Message length (-1 is failed).
*/
int uart_send(tUartHandle handle, unsigned char *pData, unsigned short size)
{
    tUartContext *pContext = (tUartContext *)handle;
    int error;

    if (NULL == pContext)
    {
        LOG_ERROR("%s: pContext is NULL\n", __func__);
        return -1;
    }

    if (pContext->fd <= 0)
    {
        LOG_ERROR("%s: UART is not ready\n", __func__);
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

    LOG_3("-> %s\n", pContext->devName);
    LOG_DUMP("UART write", pData, size);

    error = write(pContext->fd, pData, size);
    if (error < 0)
    {
        LOG_ERROR("fail to write UART device\n");
        perror( "write" );
    }

    usleep(500 * 1000);

    return error;
}

/**
*  UART baud rate convert.
*  @param [in]  baudRate  UART baud rate.
*  @returns  Speed enumeration.
*/
int uart_baudRate(int baudRate)
{
    int speed = 0;

    if (9600 == baudRate)
    {
        speed = B9600;
    }
    #if 0
    else if (14400 == baudRate)
    {
        speed = B14400;
    }
    #endif
    else if (19200 == baudRate)
    {
        speed = B19200;
    }
    else if (38400 == baudRate)
    {
        speed = B38400;
    }
    else if (57600 == baudRate)
    {
        speed = B57600;
    }
    else if (115200 == baudRate)
    {
        speed = B115200;
    }
    else
    {
        LOG_ERROR("%s: wrong baud rate %d\n", __func__, baudRate);
    }

    return speed;
}

