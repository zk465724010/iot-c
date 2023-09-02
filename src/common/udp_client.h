
//////////////////////////////////////////////////////////////////////////////////////////////
// �ļ����ƣ�udp_client.h  &  udp_client.cpp												//
// ������ƣ�CUdpClient																		//
// ������ã�UDPͨ��֮�ͻ���																//
// ʹ��ƽ̨��Linux																			//
// ��    �ߣ�zk	 																			//
// ��д���ڣ�2021.4.13 22:13 �� 															//
//////////////////////////////////////////////////////////////////////////////////////////////
#ifndef __UDP_CLIENT_H__
#define __UDP_CLIENT_H__

#define USE_THREAD

#ifdef USE_THREAD
#include "Thread11.h"
#endif

#ifdef _WIN32
#define STDCALL __stdcall
#define udp_socket SOCKET
#elif __linux__
#define STDCALL __attribute__((__stdcall__))
#define udp_socket int
#endif

typedef int(STDCALL *UDP_MSG_CBK)(const void *pMsg, int nLen, const char* pIp, int nPort, void *pUserData);

class CUdpClient
{
public:
	CUdpClient();
	~CUdpClient();

	int Init();
	void Release();

	int Open(const char *pIp, int nPort);
	int Close(bool bSync=true);

	int BindAddress(const char *pIp, int nPort);
	void GetAddress(char *pIp, int *pPort);

	int Send(const void *pData, int uLen, const char *pIp=NULL, int nPort=0);
	int Recv(void *pBuf, int nSize, char *pIp = NULL, int *pPort = NULL);
	
	void SetCallback(UDP_MSG_CBK cbk, void *arg=NULL, int arg_size=0);

	//设置套接字的参数(注意只有套接字已创建该函数方可有效)
	int SetSockOption(int nOptName, const void* pValue, int nValueLen);
	//获取套接字的参数(注意只有套接字已创建该函数方可有效)
	int GetSockOption(int nOptName, void* pBuf, int nBufSize);

	void SetPeerAddress(const char *pPeerIp, int nPeerPort);

	//int SendRTP(const void *pData, int uLen, const char *pIp = NULL, int nPort = 0);


private:
	int	 m_conn = 0;

	char m_peer_ip[16] = {0};
	int m_peer_port = 0;

#ifdef USE_THREAD
public:
	void Join();
	void Detach();

private:
	CThread11 m_recv_thd;
	static void STDCALL OnRecvProc(STATE &state, PAUSE &p, void *pUserData=NULL);

	UDP_MSG_CBK m_pRecvCallback = NULL;
	void*		m_pUserData = NULL;
	int			m_UserDataSize = 0;
#endif
};


#endif

