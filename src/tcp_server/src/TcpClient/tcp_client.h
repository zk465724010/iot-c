
#ifndef __TCP_CLIENT_H__
#define __TCP_CLIENT_H__

#include "thread11.h"

#ifdef _WIN32
#include <Winsock2.h>
#define STDCALL __stdcall
#elif __linux__
#define STDCALL __attribute__((__stdcall__))
#endif
#include <sys/timeb.h>		//struct timeb
using namespace std;

#define TIME struct timeb

#pragma pack (1)
typedef struct tcp_header_t
{
	unsigned short	mark = 0xEFFE;
	int		size = 0;
	unsigned char  check = 0;
}tcp_header_t;
#pragma pack ()

typedef struct tcp_msg_t
{
	tcp_header_t head;
	//time_t	time;
	void*	body = NULL;
}tcp_msg_t;

typedef int(STDCALL *RECV_CBK)(tcp_msg_t *pMsg, void *pUserData);

class CTcpClient
{
public:
	CTcpClient();
	CTcpClient(int nConnSocket, bool bConnState);
	~CTcpClient();
public:
	int BindAddress(const char *pIp, int uPort);
	int Connect(const char *pSerIp, int uSerPort);
	void Close();
	bool IsConnect();
	int GetAddress(char *pIp, int *pPort);
	int GetPeerAddress(char *pIp, int *pPort);

	int GetLocalIpList(char szIpList[][16], int nRows);
	size_t GetId();
	size_t SetId(size_t uNewId);
	TIME GetTime();
	TIME SetTime(const TIME tNewTime);
	int CheckIp(const char* ip);

	int SetSockOption(int nOptName, const void* pValue, int nValueLen);
	int GetSockParam(int nOptName, void* pBuf, int nBufSize);

	int Send(const void *pData, int nLen, bool bSendHead = true);
	int Recv(void *pBuf, int nSize, bool bRecvHead = true);

	int SetReuseAddress(bool enable);
	int SetSendTimeout(long sec, long msec);
	int GetSendTimeout(long *sec, long *msec);
	int SetRecvTimeout(long sec, long msec);
	int GetRecvTimeout(long *sec, long *msec);

	int SetSendBuffer(int uNewSize);
	int GetSendBuffer();
	int SetRecvBuffer(int uNewSize);
	int GetRecvBuffer();

	int GetErrCode();
	const char* GetErrDescribe(int nErrCode);

private:
	int Init(int uConnSocket, bool bConnState);
	void Release();

private:
	int	   m_nSocket = 0;
	bool   m_bIsConnect = false;
	size_t m_id = 0;
	TIME   m_tConnectTime;

#ifdef __THREAD11_H__
public:
	void SetCallback(RECV_CBK cbk, void *arg = NULL, int arg_size = 0);
private:
	CThread11	m_RecvThd;
	static void STDCALL OnRecvProc(STATE &state, PAUSE &p, void *pUserData = NULL);

	RECV_CBK m_pCallback = NULL;
	void*	 m_pUserData = NULL;
	int		 m_UserDataSize = 0;
#endif
};

#endif	// #ifndef __TCP_CLIENT_H__