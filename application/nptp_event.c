#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sys/inotify.h>
#include "nptp_serv.h"
#include "nptp_event.h"
#include "nptp_proxy.h"


#define EVENT_SIZE   sizeof(struct inotify_event)
#define EVENT_BUFFER ((EVENT_SIZE + 16) * 1024)

int g_inotifyFd = -1;


void event_monitor(void)
{
    static char buf[EVENT_BUFFER];
    struct inotify_event *event;
    int len;
    int i;
    int j;


    for (;;)
    {
        memset(buf, 0x00, EVENT_BUFFER);

        len = read(g_inotifyFd, buf, EVENT_BUFFER);
        if (len <= 0)
        {
            if (-1 == g_inotifyFd) break;
            perror( "read" );
            continue;
        }

        for (j=0; j<len; )
        {
            event = (struct inotify_event *)&(buf[j]);

            for (i=0; i<MAX_MAPPING_NUM; i++)
            {
                if (( g_pMapping[i] ) && ( !g_pMapping[i]->pipeConnect ))
                {
                    if (0 == strcmp(g_pMapping[i]->pipeName, event->name))
                    {
                        proxy_connectIpc( g_pMapping[i] );
                        break;
                    }
                }
            }

            j += (EVENT_SIZE + event->len);
        }
    }
}

int event_init(void)
{
    int wd;
    int i;

    g_inotifyFd = inotify_init();
    if (g_inotifyFd < 0)
    {
        perror( "inotify_init" );
        return -1;
    }

    for (i=0; i<g_mappingNum; i++)
    {
        if ( g_pMapping[i] )
        {
            wd = inotify_add_watch(
                     g_inotifyFd,
                     g_pMapping[i]->pipePath,
                     IN_CREATE
                 );
            if (wd < 0)
            {
                perror( "inotify_add_watch" );
                continue;
            }

            g_pMapping[i]->wd = wd;
        }
    }

    return 0;
}

void event_uninit(void)
{
    int i;

    for (i=0; i<g_mappingNum; i++)
    {
        if (( g_pMapping[i] ) && (g_pMapping[i]->wd >= 0))
        {
            inotify_rm_watch(g_inotifyFd, g_pMapping[i]->wd);
            g_pMapping[i]->wd = -1;
        }
    }
    if (g_inotifyFd >= 0)
    {
        close( g_inotifyFd );
        g_inotifyFd = -1;
    }
}

