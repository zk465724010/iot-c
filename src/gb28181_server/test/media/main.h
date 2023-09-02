
#ifndef __MY_MAIN_H__
#define __MY_MAIN_H__

#include <string.h>		// memset()函数
#ifdef _WIN32
#define STDCALL __stdcall
#elif __linux__
#define STDCALL __attribute__((__stdcall__))
#endif

typedef struct media_cfg
{
	char url[256] = { 0 };
	char ip[24] = { 0 };
	int port = 0;

}media_cfg_t;

//#pragma pack(1)	//�����ֽڶ���Ϊ1�ֽ�
//#pragma pack(push,1)	//�����ֽڶ���Ϊ1�ֽ�
//#pragma pack(pop)		//�ָ�������ֽڶ��뷽ʽΪĬ��

void STDCALL on_media_stream(const void *pData, int nLen, const char* pIp, int nPort, void *pUserData);
int read_config(const char *filename, media_cfg_t *cfg);


#endif