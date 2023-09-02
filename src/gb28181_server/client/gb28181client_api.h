

#ifndef __GB28181CLIENT_H__
#define __GB28181CLIENT_H__

#include "common.h"
#include "Thread11.h"

#ifdef _WIN32
#define STDCALL __stdcall
#elif __linux__
#include <netinet/in.h>		// struct sockaddr_in
#define STDCALL __attribute__((__stdcall__))
#endif

//#define USE_COMPILE_EXE

typedef int(*MEDIA_CBK)(void *data, int size, void *arg);
int init();
void uninit();

int play(const char *id, udp_msg_t *msg, const char *url, MEDIA_CBK cbk, MEDIA_CBK mp4_cbk, void *arg = NULL, int arg_size = 0);
int stop(const char *id, int cmd);
int playbak_ctrl(const char *id, int cmd, int pos=0, float speed=1.0);
int control(addr_info_t *dev, int cmd);
int snapshoot(addr_info_t *dev, const char *filename);
int on_udp_receive(const void *pMsg, int nLen, const char* pIp, int nPort, void *pUserData);

void STDCALL on_online_notice(STATE &state, void *pUserData=NULL);
int STDCALL on_gb_record(void *pData, int nLen, void *pUserData);

#endif