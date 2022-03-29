#ifndef __NPTP_PROXY_H__
#define __NPTP_PROXY_H__


void proxy_launchTcp(tMapping *pMapping);
void proxy_connectIpc(tMapping *pMapping);
int  proxy_init(void);
void proxy_uninit(void);


#endif
