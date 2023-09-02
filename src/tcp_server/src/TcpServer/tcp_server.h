
#ifndef	__TCP_SERVER_H__
#define	__TCP_SERVER_H__

#include "thread11.h"

#include <map>
using namespace std;

#ifdef _WIN32
#define STDCALL __stdcall
#elif __linux__
#define STDCALL __attribute__((__stdcall__))
#endif

typedef int(STDCALL *ACCEPT_CBK)(void* pClient, void *pUserData);

class CTcpServer
{
public:
	CTcpServer();
	~CTcpServer();

public:
	int Open(const char *pIp, int nPort, int nListenQueue = 5);
	void Close(bool bSync = true);
	void* Accept();
	int BindAddress(const char *pIp, int nPort);
	int GetAddress(char *pIp, int *pPort);
	int SetReuseAddress(bool enable);
	int GetLocalIpList(char szIpList[][16], int nRows);
	int CheckIp(const char* ip);

	int GetErrCode();
	const char* GetErrDescribe(int nErrCode);
	size_t GetClientCount();

private:
	int Init();
	void Release();

private:
	int			m_socket = 0;
	static size_t m_client_count;
	map<size_t, void*>	m_client_map;

#ifdef __THREAD11_H__
public:
	void SetCallback(ACCEPT_CBK cbk, void *pUserData = NULL, int nUserDataSize = 0);
private:
	CThread11	m_AcceptThd;
	static void STDCALL AcceptProc(STATE& state, PAUSE& p, void* pUserData = NULL);

	ACCEPT_CBK	m_accept_callback = NULL;
	void*		m_user_data = NULL;
	int			m_user_data_size = 0;
#endif
};

#endif