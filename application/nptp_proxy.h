#ifndef __NPTP_PROXY_H__
#define __NPTP_PROXY_H__


int  proxy_launchTcp(tMapping *pMapping);
void proxy_ceaseTcp(tMapping *pMapping);
int  proxy_connectIpc(tMapping *pMapping);
void proxy_disconnectIpc(tMapping *pMapping);
int  proxy_init(void);
void proxy_uninit(void);


#endif
