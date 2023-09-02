
#include "tcp_server.h"
#include "tcp_client.h"
#include "log.h"

#define USE_DEBUG_OUTPUT

#ifdef _WIN32
	#include <ws2tcpip.h>
	#define close_socket closesocket
	#define socklen int
#elif __linux__
	#include <sys/types.h>
	#include <sys/socket.h>
	#include <netdb.h>
	#include<unistd.h>		// close
	#include <arpa/inet.h>	// inet_pton()
	#include<netinet/in.h>
	#define close_socket close
	#define socklen socklen_t
#endif

#ifdef _WIN32
	#define close_socket closesocket
	#define socklen int
#elif __linux__
	#define close_socket close
	#define socklen socklen_t
#endif

size_t CTcpServer::m_client_count = 100;

CTcpServer::CTcpServer()
{
	#ifdef _WIN32
		WSADATA wsaData;
		WSAStartup(0x202, &wsaData);
	#endif
	Init();
}

CTcpServer::~CTcpServer()
{
	Release();
	#ifdef _WIN32
		WSACleanup();
	#endif
}
int CTcpServer::Init()
{
	int nRet = 0;
	m_socket = socket(AF_INET, SOCK_STREAM, IPPROTO_IP);
	if(m_socket <= 0)
		LOG3("ERROR", "Create socket failed %d\n", m_socket);
	nRet = (m_socket > 0) ? 0 : -1;
	return nRet;
}
void CTcpServer::Release()
{
	auto iter = m_client_map.begin();
	for (; iter != m_client_map.end(); )
	{
		CTcpClient *client = (CTcpClient*)iter->second;
		client->Close();
		delete client;
		iter = m_client_map.erase(iter);
	}
	if(m_socket > 0)
	{
		close_socket(m_socket);
		m_socket = 0;
	}
}
int CTcpServer::Open(const char *pIp, int nPort, int nListenQueue/*=5*/)
{
	int nRet = -1;
	if (m_socket > 0) {
		nRet = BindAddress(pIp, nPort);
		if (0 == nRet) {
			char ip[24] = { 0 };
			int port = 0;
			GetAddress(ip, &port);
			nRet = listen(m_socket, nListenQueue);
			if (0 == nRet) {
				LOG3("INFO", "TCP server listening '%s:%d' ...\n\n", ip, port);
			}
			else {
				LOG3("ERROR", "TCP server listening failed '%s:%d'('%s').\n\n", ip, port, strerror(errno));
				close_socket(m_socket);
				m_socket = 0;
			}
		}
	}
	else LOG3("ERROR", "Socket not create %d\n", m_socket);
	return nRet;
}
void CTcpServer::Close(bool bSync/*=true*/)
{
	#ifdef USE_THREAD
	m_AcceptThd.Stop();
	#endif
	if (m_socket > 0) {
		close_socket(m_socket);
		m_socket = 0;
	}
}
void* CTcpServer::Accept()
{
	#ifdef _WIN32
	struct timeval tv = { 0,500000 };
	#elif __linux__
	struct timespec ts = { 0, 500000000 };
	#endif
	fd_set readfd;
	int nRet = 0;
	FD_ZERO(&readfd);
	FD_SET(m_socket, &readfd);
	#ifdef _WIN32
		nRet = select(0, &readfd, NULL, NULL, &tv);
	#elif __linux__
		nRet = pselect(m_socket + 1, &readfd, NULL, NULL, &ts, NULL);
	#endif
	if (nRet >= 0)
	{
		if (FD_ISSET(m_socket, &readfd))
		{
			sockaddr_in  client_addr;
			static socklen iLen = sizeof(sockaddr_in);
			int conn = accept(m_socket, (sockaddr*)&client_addr, &iLen);
			#if 0
				char szIp[16] = { 0 };
				#if 1
					inet_ntop(AF_INET, &client_addr.sin_addr, szIp, 16);
				#else
					strcpy(szIp, inet_ntoa(client_addr.sin_addr));
				#endif
				int uPort = ntohs(client_addr.sin_port);
				printf("来自客户端[%s:%u]\n", szIp, uPort);
			#endif
			CTcpClient* pClient = new CTcpClient(conn, true);
			if (NULL != pClient)
			{
				TIME current_time = { 0 };
				ftime(&current_time);
				pClient->SetTime(current_time);
				pClient->SetId(m_client_count);
				m_client_map.insert(map<size_t, void*>::value_type(m_client_count, pClient));
				m_client_count++;
			}
			else close_socket(conn);
			return pClient;
		}
	}
	else LOG3("ERROR","select function failed %d ('%s')\n", nRet, strerror(errno));
	return NULL;
}
int CTcpServer::BindAddress(const char *pIp, int nPort)
{
	int nRet = -1;
	if ((NULL != pIp) && (0 != CheckIp(pIp))) {
		LOG3("ERROR", "IP address format error '%s'\n\n", pIp);
		return nRet;
	}
	if (m_socket <= 0)
		m_socket = socket(AF_INET, SOCK_DGRAM, 0);
	if (m_socket > 0)
	{
		struct sockaddr_in addr;
		socklen len = sizeof(addr);
		memset(&addr, 0, len);
		addr.sin_family = AF_INET;
		addr.sin_addr.s_addr = (0 != pIp) ? inet_addr(pIp) : htonl(INADDR_ANY);
		addr.sin_port = (nPort > 0) ? htons(nPort) : 0;
		#ifdef _WIN32
			nRet = bind((SOCKET)m_socket, (const sockaddr*)&addr, len);
		#elif __linux__
			nRet = bind(m_socket, (const sockaddr*)&addr, len);
		#endif
		if (nRet < 0) {
			const char *str = GetErrDescribe(GetErrCode());
			LOG3("ERROR", " Bind address failed '%s:%d' ('%d %s')\n\n", pIp ? pIp : "null", nPort, GetErrCode(), str);
			close_socket(m_socket);
			m_socket = 0;
		}
	}
	else LOG3("ERROR", "Socket error %d.\n\n", m_socket);
	return nRet;
}
int CTcpServer::GetAddress(char *pIp, int *pPort)
{
	sockaddr_in addr;
	static socklen iLen = sizeof(sockaddr_in);
	int nRet = getsockname(m_socket, (sockaddr*)&addr, &iLen);
	if (0 == nRet)
	{
		if (NULL != pIp)
		{
			#if 1
			inet_ntop(AF_INET, &addr.sin_addr, pIp, 16);
			#else
			strcpy(pIp, inet_ntoa(skAddr.sin_addr));
			#endif
		}
		if (NULL != pPort)
			*pPort = ntohs(addr.sin_port);
	}
	else LOG3("ERROR", "getsockname function failure %d ('%s')\n\n", nRet, strerror(errno));
	return nRet;
}
int CTcpServer::GetLocalIpList(char szIpList[][16], int nRows)
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
int CTcpServer::SetReuseAddress(bool enable)
{
	if (m_socket <= 0)
		m_socket = socket(AF_INET, SOCK_DGRAM, 0);
	int nRet = -1;
	if (m_socket > 0)
	{
		int nEnable = enable ? 1 : 0;
		nRet = setsockopt(m_socket, SOL_SOCKET, SO_REUSEADDR, (char*)&nEnable, sizeof(nEnable));
		if(0 != nRet)
			LOG3("INFO","setsockopt function failed %d ('%s')\n", nRet, strerror(errno));
	}
	else LOG3("ERROR", "Service did not open successfully\n\n");
	return nRet;
}
int CTcpServer::CheckIp(const char* ip)
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
int CTcpServer::GetErrCode()
{
#if _WIN32
	int nErrCode = 0;
	#if 1
		nErrCode = WSAGetLastError();
	#else
		int nRet = 0, iLen = sizeof(nErrCode);
		nRet = getsockopt(m_nSocket, SOL_SOCKET, SO_ERROR, (char*)&nErrCode, &iLen);
		nErrCode = (0 == nRet) ? nErrCode : nRet;
	#endif
	return nErrCode;
#elif __linux__
	return errno;
#endif
}
const char* CTcpServer::GetErrDescribe(int nErrCode)
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
size_t CTcpServer::GetClientCount()
{
	return m_client_map.size();
}

#ifdef __THREAD11_H__
void CTcpServer::SetCallback(ACCEPT_CBK cbk, void* pUserData/*=NULL*/, int nUserDataSize/*=0*/)
{
	if ((NULL != m_user_data) && (m_user_data_size > 0))
	{
		free(m_user_data);
		m_user_data = NULL;
		m_user_data_size = 0;
	}
	m_user_data = pUserData;
	if ((NULL != pUserData) && (nUserDataSize > 0))
	{
		m_user_data = malloc(nUserDataSize + 1);
		if (NULL != m_user_data)
		{
			memset(m_user_data, 0, nUserDataSize + 1);
			memcpy(m_user_data, pUserData, nUserDataSize);
			m_user_data_size = nUserDataSize;
		}
	}
	m_accept_callback = cbk;
	if (NULL != m_accept_callback)
	{
		int nRet = m_AcceptThd.Run(AcceptProc, this);
		if (0 != nRet)
			LOG3("ERROR", "Thread running failure %d\n\n", nRet);
	}
}
void STDCALL CTcpServer::AcceptProc(STATE &state, PAUSE &p, void* pUserData/*=NULL*/)
{
	CTcpServer *pServer = (CTcpServer*)pUserData;
	#ifdef _WIN32
		struct timeval tv = { 1,0 };
	#elif __linux__
		struct timespec ts = { 1,0 };
	#endif
	fd_set readfd;
	int nRet = 0;
	while (TS_STOP != state)
	{
		if (TS_PAUSE == state) {
			p.wait();
		}
		FD_ZERO(&readfd);
		FD_SET(pServer->m_socket, &readfd);
		#ifdef _WIN32
			nRet = select(0, &readfd, NULL, NULL, &tv);
		#elif __linux__
			nRet = pselect(pServer->m_socket + 1, &readfd, NULL, NULL, &ts, NULL);
		#endif
		if ((nRet >= 0) && (state == TS_RUNING)) {
			if (FD_ISSET(pServer->m_socket, &readfd)) {
				sockaddr_in  client_addr;
				static socklen iLen = sizeof(sockaddr_in);
				int conn = accept(pServer->m_socket, (sockaddr*)&client_addr, &iLen);
				#if 0
					char szIp[16] = { 0 };
					#if 1
						inet_ntop(AF_INET, &client_addr.sin_addr, szIp, 16);
					#else
						strcpy(szIp, inet_ntoa(client_addr.sin_addr));
					#endif
					int uPort = ntohs(client_addr.sin_port);
					printf("来自客户端[%s:%u]\n", szIp, uPort);
				#endif
				CTcpClient *pClient = new CTcpClient(conn, true);
				if (NULL != pClient) {
					TIME current_time = {0};
					ftime(&current_time);
					pClient->SetTime(current_time);
					pClient->SetId(m_client_count);
					pServer->m_client_map.insert(map<size_t, void*>::value_type(m_client_count, pClient));
					m_client_count++;
					pServer->m_accept_callback(pClient, pServer->m_user_data);
				}
				else close_socket(conn);
			}
		}
		else {
			const char *err = pServer->GetErrDescribe(pServer->GetErrCode());
			LOG3("ERROR", "select function failed %d ('%s')\n", nRet, err);
			break;
		}
	}
}
#endif  // #ifdef __THREAD11_H__
