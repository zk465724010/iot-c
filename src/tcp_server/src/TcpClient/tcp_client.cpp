
#include "tcp_client.h"
#include "log.h"

#ifdef _WIN32
#include <Winsock2.h>
#include <WS2tcpip.h>		// inet_ntop
#pragma comment (lib,"Ws2_32.lib")
#define close_socket closesocket
#define socklen int
#elif __linux__
#include <sys/socket.h>
#include <string.h>
#include <arpa/inet.h>
#include<unistd.h>		// close
#include <sys/select.h>
#include <netdb.h>		// gethostbyname()
#define close_socket close
#define socklen socklen_t
#endif

#define	MAX_BUFF_SIZE 10240			// 最大缓冲区大小(单位:字节)
#define min(a,b)	(a) > (b) ? (b) : (a)
#define max(a,b)	(a) > (b) ? (a) : (b)

CTcpClient::CTcpClient()
{
	#ifdef _WIN32
	WSADATA wsaData;
	WSAStartup( 0x202, &wsaData );
	#endif
	int nRet = Init(0, false);
	if (0 != nRet) {
		LOG3("ERROR", "TCP client initialize failed %d\n", nRet);
	}
}

CTcpClient::CTcpClient(int nConnSocket, bool bConnState)
{
	#ifdef _WIN32
	WSADATA wsaData;
	WSAStartup( 0x202, &wsaData );
	#endif
	int nRet = Init(nConnSocket, bConnState);
	if (0 != nRet) {
		LOG3("ERROR", "TCP client initialize failed %d\n", nRet);
	}
}

CTcpClient::~CTcpClient(void)
{
	Release();
	#ifdef _WIN32
	WSACleanup();
	#endif
}
int CTcpClient::Init(int conn, bool bConnState)
{
	static size_t id = 0;
	if (conn > 0) {
		m_nSocket = conn;
		m_bIsConnect = bConnState;
	}
	else {
		m_nSocket = socket(AF_INET, SOCK_STREAM, 0);
		m_bIsConnect = false;
		m_id = ++id;
	}
	memset(&m_tConnectTime, 0, sizeof(m_tConnectTime));
	return 0;
}
void CTcpClient::Release()
{
	Close();
}
int CTcpClient::Connect(const char *pSerIp, int nSerPort)
{
	int nRet = 0;
	if (m_bIsConnect == false) {
		if (m_nSocket <= 0)
			m_nSocket = socket(AF_INET, SOCK_STREAM, 0);

		sockaddr_in addrSer;
		addrSer.sin_family = AF_INET;
		addrSer.sin_port = htons(nSerPort);
		#if 1
			inet_pton(AF_INET, pSerIp, &addrSer.sin_addr);
		#else
			addrSer.sin_addr.s_addr = inet_addr(pSerIp);
		#endif
		nRet = connect(m_nSocket, (sockaddr*)&addrSer, sizeof(addrSer));
		if (0 == nRet) {
			ftime(&m_tConnectTime);
			m_bIsConnect = true;
			LOG3("INFO","Connect to server '%s:%d' success ^-^ \n\n", pSerIp, nSerPort);
		}
		else {
			int err_code = GetErrCode();
			const char *err_str = GetErrDescribe(err_code);
			LOG3("ERROR","Connect to server '%s:%d' failed %d %d '%s'\n\n", pSerIp, nSerPort, nRet, err_code, err_str);
		}
	}
	return nRet;
}
bool CTcpClient::IsConnect()
{
	return m_bIsConnect;
}
void CTcpClient::Close()
{
	m_bIsConnect = false;
	#ifdef __THREAD11_H__
	m_RecvThd.Stop();
	#endif
	if (m_nSocket > 0) {
		close_socket(m_nSocket);
		m_nSocket = 0;
	}
}
int CTcpClient::BindAddress(const char *pIp, int nPort)
{
	int nRet = -1;
	if ((NULL != pIp) && (0 != CheckIp(pIp))) {
		LOG3("ERROR", "IP address format error '%s'\n\n", pIp);
		return nRet;
	}
	if (m_nSocket <= 0)
		m_nSocket = socket(AF_INET, SOCK_STREAM, 0);
	if (m_nSocket > 0) {
		struct sockaddr_in addr;
		socklen len = sizeof(addr);
		memset(&addr, 0, len);
		addr.sin_family = AF_INET;
		#if 1
			inet_pton(AF_INET, (NULL != pIp) ? pIp : INADDR_ANY, &addr.sin_addr);
		#else
			addr.sin_addr.s_addr = (NULL != pIp) ? inet_addr(pIp) : htonl(INADDR_ANY);
		#endif
		addr.sin_port = (nPort > 0) ? htons(nPort) : 0;
		#ifdef _WIN32
			nRet = bind((SOCKET)m_nSocket, (const sockaddr*)&addr, len);
		#elif __linux__
			nRet = bind(m_nSocket, (const sockaddr*)&addr, len);
		#endif
		if (nRet < 0) {
			const char *str = GetErrDescribe(GetErrCode());
			LOG3("ERROR", "Bind address '%s:%d' failed ('%d %s')\n\n", pIp ? pIp : "null", nPort, GetErrCode(), str);
			close_socket(m_nSocket);
			m_nSocket = 0;
		}
	}
	else LOG3("ERROR", "Socket error %d.\n\n", m_nSocket);
	return nRet;
}
int CTcpClient::CheckIp(const char* ip)
{
	int nRet = -1;
	if (NULL != ip) {
		int n1 = -1, n2 = -1, n3 = -1, n4 = -1;
		sscanf(ip, "%d.%d.%d.%d", &n1, &n2, &n3, &n4);
		if (((0 <= n1) && (n1 <= 255))
			&& ((0 <= n2) && (n2 <= 255))
			&& ((0 <= n3) && (n3 <= 255))
			&& ((0 <= n4) && (n4 <= 255))
			)
			nRet = 0;
	}
	return nRet;
}
int CTcpClient::GetAddress(char *pIp, int *pPort)
{
	sockaddr_in  addr;
	static socklen iLen = sizeof(addr);
	int nRet = getsockname(m_nSocket, (sockaddr*)&addr, &iLen);
	if (0 == nRet) {
		if (NULL != pIp) {
			#if 1
			inet_ntop(AF_INET, &addr.sin_addr, pIp, 16);
			#else
			strcpy(pIp, inet_ntoa(addr.sin_addr));
			#endif
		}
		if (NULL != pPort)
			*pPort = ntohs(addr.sin_port);
	}
	return nRet;
}
int CTcpClient::GetPeerAddress(char *pIp, int *pPort)
{
	sockaddr_in  peer_addr;
	static socklen iLen = sizeof(sockaddr_in);
	int nRet = getpeername(m_nSocket, (sockaddr*)&peer_addr, &iLen);
	if (0 == nRet) {
		if (NULL != pIp) {
			#if 1
			inet_ntop(AF_INET, &peer_addr.sin_addr, pIp, 16);
			#else
			strcpy(pIp, inet_ntoa(skAddr.sin_addr));
			#endif
		}
		if (NULL != pPort)
			*pPort = ntohs(peer_addr.sin_port);
	}
	return nRet;
}
int CTcpClient::GetLocalIpList(char szIpList[][16], int nRows)
{
	int nRet = 0;
	char szHostName[128] = { 0 };
	gethostname(szHostName, sizeof(szHostName));
#ifdef _WIN32
	struct addrinfo hints;
	memset(&hints, 0, sizeof(hints));
	hints.ai_family = AF_INET;
	hints.ai_flags = AI_PASSIVE;
	hints.ai_protocol = IPPROTO_IP;
	hints.ai_socktype = SOCK_DGRAM;
	struct addrinfo *pResult = NULL;
	int n = getaddrinfo(szHostName, NULL, &hints, &pResult);
	if ((0 == n) && (NULL != pResult))
	{
		int index = 0;
		for (struct addrinfo *p = pResult; NULL != p; p = p->ai_next)
		{
			struct sockaddr_in *addr = (struct sockaddr_in*)p->ai_addr;
			if ((NULL != addr) && (index < nRows))
			{
				sprintf(szIpList[index++], "%d.%d.%d.%d\0",
					(*addr).sin_addr.S_un.S_un_b.s_b1,
					(*addr).sin_addr.S_un.S_un_b.s_b2,
					(*addr).sin_addr.S_un.S_un_b.s_b3,
					(*addr).sin_addr.S_un.S_un_b.s_b4);
				//printf("[%s]\n", szIpList[index-1]);
			}
		}
		freeaddrinfo(pResult);
		nRet = index;
	}
#elif __linux__
	struct hostent *pInfo = gethostbyname(szHostName);
	int index = 0;
	for (int i = 0; NULL != pInfo && NULL != pInfo->h_addr_list[i]; ++i)
	{
		char *ip = inet_ntoa(*(struct in_addr*)pInfo->h_addr_list[i]);
		if ((NULL != ip) && (index < nRows))
			strncpy(szIpList[index++], ip, 15);
	}
	nRet = index;
#endif
	return nRet;
}
size_t CTcpClient::GetId()
{
	return m_id;
}
size_t CTcpClient::SetId(size_t uNewId)
{
	size_t uOldId = m_id;
	m_id = uNewId;
	return uOldId;
}
TIME CTcpClient::GetTime()
{
	return m_tConnectTime;
}
TIME CTcpClient::SetTime(const TIME tNewTime)
{
	TIME tOldTime = m_tConnectTime;
	m_tConnectTime = tNewTime;
	return tOldTime;
}
int CTcpClient::SetSockOption(int nOptName, const void* pValue, int nValueLen)
{	
	#ifdef _WIN32
	int nRet = setsockopt(m_nSocket, SOL_SOCKET, nOptName, (const char*)pValue, nValueLen);
	#elif __linux__
	int nRet = setsockopt(m_nSocket, SOL_SOCKET, nOptName, pValue, (socklen_t)nValueLen);
	#endif
	if (0 != nRet)
		LOG3("ERROR", "setsockopt failed %d '%s'\n\n", nRet, strerror(errno));
	return nRet;
}
int CTcpClient::GetSockParam(int nOptName, void* pBuf, int nBufSize)
{
	return getsockopt(m_nSocket, SOL_SOCKET, nOptName, (char*)pBuf, (socklen*)&nBufSize);
	#ifdef _WIN32
		int nRet = getsockopt(m_nSocket, SOL_SOCKET, nOptName, (char*)pBuf, &nBufSize);
	#elif __linux__
		int nRet = getsockopt(m_nSocket, SOL_SOCKET, nOptName, pBuf, (socklen_t*)&nBufSize);
	#endif
	if (0 != nRet)
		LOG3("ERROR", "getsockopt failed %d '%s'\n\n", nRet, strerror(errno));
	return nRet;
}
int CTcpClient::Send(const void *pData, int nLen, bool bSendHead/*=true*/)
{
	int nSndBytes = 0;
	if ((NULL == pData) || (nLen <= 0)) {
		LOG3("ERROR", "pData=%p, nLen=%d\n", pData, nLen);
		return -1;
	}
	if (bSendHead) {
		tcp_header_t head;
		int nRet = 0;
		while (nLen > 0) {
			head.size = min(nLen, MAX_BUFF_SIZE);
			//head.check = calc_crc8((unsigned char*)pData, head.size);
			nRet = send(m_nSocket, (char*)&head, sizeof(head), 0);
			if (nRet > 0) {
				nSndBytes += nRet;
				nRet = send(m_nSocket, (char*)pData, head.size, 0);
				if (nRet > 0) {
					nSndBytes += nRet;
					pData = (char*)pData + nRet;
					nLen -= nRet;
				}
				else {
					LOG3("ERROR", "send function error %d ('%s')\n", nRet, strerror(errno));
					break;
				}
			}
			else {
				LOG3("ERROR", "send function error %d ('%s')\n", nRet, strerror(errno));
				break;
			}
		}
	}
	else {
		int nRet = 0, nSend = 0;
		while (nLen > 0) {
			nSend = min(nLen, MAX_BUFF_SIZE);
			nRet = send(m_nSocket, (const char*)pData, nSend, 0);
			if (nRet > 0) {
				nSndBytes += nRet;
				pData = (char*)pData + nRet;
				nLen -= nRet;
			}
			else {
				int err_code = GetErrCode();
				const char *err_str = GetErrDescribe(err_code);
				LOG3("ERROR", "send function error %d %d '%s'\n", nRet, err_code, err_str);
				nSndBytes = -err_code;
				break;
			}
		}
	}
	return nSndBytes;
}
int CTcpClient::Recv(void *pBuf, int nSize, bool bRecvHead/*=true*/)
{
	int nRecvBytes = 0;
	if ((NULL == pBuf) || (nSize <= 0)) {
		LOG3("ERROR", "pData=%p, nLen=%d\n", pBuf, nSize);
		return nRecvBytes;
	}
	#ifdef _WIN32
		static struct timeval tv = { 0,600000 };
	#elif __linux__
		static struct timespec ts = { 0,600000000 };
	#endif
	fd_set	recvfd;
	FD_ZERO(&recvfd);
	FD_SET(m_nSocket, &recvfd);
	#ifdef _WIN32
		int nRet = select(0, &recvfd, NULL, NULL, &tv);
	#elif __linux__
		int nRet = pselect(m_nSocket + 1, &recvfd, NULL, NULL, &ts, NULL);
	#endif
	if (nRet > 0) {
		if (FD_ISSET(m_nSocket, &recvfd)) {
			do {
				if (bRecvHead) {
					tcp_header_t head;
					nRet = recv(m_nSocket, (char*)&head, sizeof(head), 0);
					if (nRet > 0) {
						nRecvBytes += nRet;
						nSize = min(nSize, head.size);
					}
					else {
						LOG3("ERROR", "recv function error %d ('%s')\n", nRet, strerror(errno));
						break;
					}
				}
				nRet = recv(m_nSocket, (char*)pBuf, nSize, 0);
				if (nRet > 0) {
					nRecvBytes += nRet;
				}
				else LOG3("ERROR", "recv function error %d ('%s')\n", nRet, strerror(errno));
			} while (false);
		}
		else LOG3("ERROR", "FD_ISSET not \n\n");
	}
	else if (nRet < 0)
		LOG3("ERROR", "select function error %d ('%s')\n", nRet, strerror(errno));
	return nRecvBytes;
}
int CTcpClient::SetReuseAddress(bool enable)
{
	if (m_nSocket > 0)
	{
		int nEnable = enable ? 1 : 0;
		int nRet = setsockopt(m_nSocket, SOL_SOCKET, SO_REUSEADDR, (char*)&nEnable, sizeof(nEnable));
		if (0 != nRet) {
			int err_code = GetErrCode();
			const char *err_str = GetErrDescribe(err_code);
			LOG3("ERROR", "setsockopt function failed %d %d '%s'\n", nRet, err_code, err_str);
		}
		return nRet;
	}
	else LOG3("ERROR", "Service did not open successfully\n\n");
	return -1;
}
int CTcpClient::SetSendTimeout(long sec, long msec)
{
	struct timeval timeout;
	timeout.tv_sec = sec;
	timeout.tv_usec = msec * 1000;
	int nRet = setsockopt(m_nSocket, SOL_SOCKET, SO_SNDTIMEO, (const char*)&timeout, sizeof(timeout));
	if (0 != nRet) {
		int err_code = GetErrCode();
		const char *err_str = GetErrDescribe(err_code);
		LOG3("ERROR", "setsockopt function failed %d %d '%s'\n", nRet, err_code, err_str);
	}
	return nRet;
}
int CTcpClient::GetSendTimeout(long *sec, long *msec)
{
	static socklen size = sizeof(struct timeval);
	struct timeval tv_out;
	int nRet = getsockopt(m_nSocket, SOL_SOCKET, SO_SNDTIMEO, (char*)&tv_out, &size);
	if (0 == nRet)
	{
		if (NULL != sec)
			*sec = tv_out.tv_sec;
		if (NULL != msec)
			*msec = tv_out.tv_usec / 1000;
	}
	else
	{
		int err_code = GetErrCode();
		const char *err_str = GetErrDescribe(err_code);
		LOG3("ERROR", "getsockopt function failed %d %d '%s'\n", nRet, err_code, err_str);
	}
	return nRet;
}
int CTcpClient::SetRecvTimeout(long sec, long msec)
{
	struct timeval timeout;
	timeout.tv_sec = sec;
	timeout.tv_usec = msec * 1000;
	int nRet = setsockopt(m_nSocket, SOL_SOCKET, SO_RCVTIMEO, (const char*)&timeout, sizeof(timeout));
	if (0 != nRet) {
		int err_code = GetErrCode();
		const char *err_str = GetErrDescribe(err_code);
		LOG3("ERROR", "setsockopt function failed %d %d '%s'\n", nRet, err_code, err_str);
	}
	return nRet;
}
int CTcpClient::GetRecvTimeout(long *sec, long *msec)
{
	static socklen size = sizeof(struct timeval);
	struct timeval tv_out;
	int nRet = getsockopt(m_nSocket, SOL_SOCKET, SO_RCVTIMEO, (char*)&tv_out, &size);
	if (0 == nRet)
	{
		if (NULL != sec)
			*sec = tv_out.tv_sec;
		if (NULL != msec)
			*msec = tv_out.tv_usec / 1000;
	}
	else
	{
		int err_code = GetErrCode();
		const char *err_str = GetErrDescribe(err_code);
		LOG3("ERROR", "getsockopt function failed %d %d '%s'\n", nRet, err_code, err_str);
	}
	return nRet;
}
int CTcpClient::SetSendBuffer(int uNewSize)
{
	int nRet = setsockopt(m_nSocket, SOL_SOCKET, SO_SNDBUF, (const char*)&uNewSize, sizeof(uNewSize));
	if (0 != nRet) {
		int err_code = GetErrCode();
		const char *err_str = GetErrDescribe(err_code);
		LOG3("ERROR","setsockopt function failed %d %d '%s'\n", nRet, err_code, err_str);
	}
	return nRet;
}
int CTcpClient::GetSendBuffer()
{
	static socklen size = sizeof(int);
	int snd_buf_size = -1;
	int nRet = getsockopt(m_nSocket, SOL_SOCKET, SO_SNDBUF, (char*)&snd_buf_size, &size);
	if (0 != nRet) {
		int err_code = GetErrCode();
		const char *err_str = GetErrDescribe(err_code);
		LOG3("ERROR", "getsockopt function failed %d %d '%s'\n", nRet, err_code, err_str);
	}
	return snd_buf_size;
}
int CTcpClient::SetRecvBuffer(int uNewSize)
{
	int nRet = setsockopt(m_nSocket, SOL_SOCKET, SO_RCVBUF, (const char*)&uNewSize, sizeof(uNewSize));
	if (0 != nRet) {
		int err_code = GetErrCode();
		const char *err_str = GetErrDescribe(err_code);
		LOG3("ERROR", "setsockopt function failed %d %d '%s'\n", nRet, err_code, err_str);
	}
	return nRet;
}
int CTcpClient::GetRecvBuffer()
{
	static socklen size = sizeof(int);
	int rcv_buf_size = -1;
	int nRet = getsockopt(m_nSocket, SOL_SOCKET, SO_RCVBUF, (char*)&rcv_buf_size, &size);
	if (0 != nRet) {
		int err_code = GetErrCode();
		const char *err_str = GetErrDescribe(err_code);
		LOG3("ERROR", "getsockopt function failed %d %d '%s'\n", nRet, err_code, err_str);
	}
	return rcv_buf_size;
}
int CTcpClient::GetErrCode()
{
#if _WIN32
	int nErrCode = 0;
	#if 1
		nErrCode = WSAGetLastError();
	#else
		int nRet = 0, iLen = sizeof(nErrCode);
		nRet = getsockopt(m_nSocket, SOL_SOCKET, SO_ERROR, (char*)&nErrCode, &iLen);	//获取错误码
		nErrCode = (0 == nRet) ? nErrCode : nRet;
	#endif
	return nErrCode;
#elif __linux__
	return errno;
#endif
}
const char* CTcpClient::GetErrDescribe(int nErrCode)
{
#if _WIN32
	static char buff[256] = { 0 };
	memset(buff, 0, sizeof(buff));
	int nRet = FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
		NULL,
		nErrCode,
		MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
		buff,
		sizeof(buff),
		NULL);
	if (0 == nRet)
		strcpy(buff, "Unknown error");
	return buff;
#elif __linux__
	return strerror(nErrCode);
#endif
}


#ifdef __THREAD11_H__
void CTcpClient::SetCallback(RECV_CBK cbk, void *arg/*=NULL*/, int arg_size/*=0*/)
{
	if ((NULL != m_pUserData) && (m_UserDataSize > 0))
	{
		free(m_pUserData);
		m_pUserData = NULL;
		m_UserDataSize = 0;
	}
	m_pCallback = cbk;
	m_pUserData = arg;
	if ((NULL != arg) && (arg_size > 0)) {
		m_pUserData = malloc(arg_size + 1);
		if (NULL != m_pUserData) {
			memset(m_pUserData, 0, arg_size + 1);
			memcpy(m_pUserData, arg, arg_size);
			m_UserDataSize = arg_size;
		}
	}
	if (NULL != m_pCallback)
		m_RecvThd.Run(OnRecvProc, this);
}
void STDCALL CTcpClient::OnRecvProc(STATE &state, PAUSE &p, void *pUserData/*=NULL*/)
{
	CTcpClient *pClient = (CTcpClient*)pUserData;
	#ifdef _WIN32
	struct timeval tv = { 1,0 };
	#elif __linux__
	struct timespec ts = { 1,0 };
	#endif

	tcp_msg_t msg;
	memset(&msg, 0, sizeof(msg));
	msg.body = (char*)malloc(MAX_BUFF_SIZE+1);
	if (NULL != msg.body) {
		memset(msg.body, 0, MAX_BUFF_SIZE + 1);
		msg.head.size = MAX_BUFF_SIZE;	
	}
	else {
		LOG3("ERROR", " Allocate memory failed %p\n", msg.body);
		return;
	}
	bool bRecvHead = false;
	fd_set	readfd;
	int nRet = 0;
	while (TS_STOP != state) {
		if (TS_PAUSE == state) {
			p.wait();
		}
		FD_ZERO(&readfd);
		FD_SET(pClient->m_nSocket, &readfd);
		#ifdef _WIN32
			nRet = select(0, &readfd, NULL, NULL, &tv);
		#elif __linux__
			nRet = pselect(pClient->m_nSocket + 1, &readfd, NULL, NULL, &ts, NULL);
		#endif
		if (nRet > 0) {
			if (FD_ISSET(pClient->m_nSocket, &readfd)) {
				if (bRecvHead) {
					nRet = recv(pClient->m_nSocket, (char*)&msg.head, sizeof(msg.head), 0);
					if (nRet > 0) {
						msg.head.size = min(MAX_BUFF_SIZE, msg.head.size);
					}
					else {
						int err_code = pClient->GetErrCode();
						const char *err_str = pClient->GetErrDescribe(err_code);
						LOG3("ERROR", "recv function error %d %d '%s'\n", nRet, err_code, err_str);
						break;
					}
				}
				else msg.head.size = MAX_BUFF_SIZE;
				nRet = recv(pClient->m_nSocket, (char*)msg.body, msg.head.size, 0);
				if (nRet > 0) {
					msg.head.size = nRet;
					((char*)msg.body)[nRet] = '\0';
					pClient->m_pCallback(&msg, pClient->m_pUserData);
				}
				else {
					int err_code = pClient->GetErrCode();
					const char *err_str = pClient->GetErrDescribe(err_code);
					LOG3("ERROR", "recv function error %d %d '%s'\n", nRet, err_code, err_str);
					break;
				}
			}
		}
		else if (nRet < 0) {
			LOG3("ERROR", "select function error '%s'\n", strerror(errno));
			break;
		}
	}
	if (NULL != msg.body)
		free(msg.body);
}
#endif
