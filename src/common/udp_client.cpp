
#include "udp_client.h"
//#include "common.h"
#include <sstream>		// stringstream

#ifdef _WIN32
//#include <ws2tcpip.h>		// //inet_ntop(),inet_pton()
//#include <windows.h>
#include <Winsock2.h>
#include <WS2tcpip.h>		// inet_ntop

#pragma comment (lib,"Ws2_32.lib")
#elif __linux__
#include <sys/socket.h>
//#include <cstdlib>
#include <string.h>
#include <arpa/inet.h>
#include<unistd.h>		// close
//#include<netinet/in.h>
//#include<sys/types.h>
//#include<sys/select.h>
#include <sys/select.h>
#endif
#include "log.h"

#define USE_UDP_DEBUG_OUTPUT
#define RECV_BUFF_SIZE  10240

#ifdef _WIN32
#define close_socket closesocket
#define socklen int
#elif __linux__
#define close_socket close
#define socklen socklen_t
#endif

CUdpClient::CUdpClient()
{
	Init();
}

CUdpClient::~CUdpClient()
{
	Release();
}

int CUdpClient::Init()
{
	int nRet = 0;
	#ifdef _WIN32
		WSADATA wsaData;
		WSAStartup(0x202, &wsaData);
		//#ifdef USE_UDP_DEBUG_OUTPUT
		#if 0
			printf("---------------------------------------------------------\n");
			printf(" 数据包最大大小: %d\n", wsaData.iMaxUdpDg);	//发送或接收的最大的用户数据包协议（UDP）的数据包大小
			//printf("	 VendorInfo: %s\n",wsaData.lpVendorInfo);
			printf("    Sockets描述: %s\n", wsaData.szDescription);
			printf("    Sockets状态: %s\n", wsaData.szSystemStatus);
			printf("单个进程能够打开Socket的最大数目: %d\n", wsaData.iMaxSockets);
			printf("    Sockets版本: 副版本号:%d / 主版本号:%d\n", (wsaData.wVersion >> 8), (wsaData.wVersion & 0x00FF));
			printf("Sockets最高版本: %d(通常与Sockets版本相同)\n", wsaData.wHighVersion);
			printf("---------------------------------------------------------\n");
		#endif
	#endif
	return nRet;
}

void CUdpClient::Release()
{
	#ifdef _WIN32
		WSACleanup();
	#endif
}

int CUdpClient::Open(const char *pIp, int nPort)
{
	int nRet = -1;
	if (m_conn <= 0)
	{
		m_conn = socket(AF_INET, SOCK_DGRAM, IPPROTO_IP);
		if (m_conn > 0)
		{
			nRet = BindAddress(pIp, nPort);
			#ifdef USE_THREAD
			if (0 == nRet)
			{
				#ifdef USE_UDP_DEBUG_OUTPUT
				//char ip[24] = {0};
				//int port = 0;
				//GetAddress(ip, &port);
				//DEBUG("UDP client opened successfully, is listening'%s:%d'...\n\n", ip, port);
				#endif
			}
			#endif
		}
		#ifdef USE_UDP_DEBUG_OUTPUT
		else LOG("ERROR","Failed to create the socket %d\n", m_conn);
		#endif
	}
	return nRet;
}

int CUdpClient::Close(bool bSync/*=true*/)
{
	#ifdef USE_THREAD
		m_recv_thd.Stop(bSync);
	#endif
	if (m_conn > 0)
	{
		int nRet = close_socket(m_conn);
		if (0 == nRet) {
			m_conn = 0;
			//DEBUG("INFO: Close udp socket.\n\n");
		}
		#ifdef USE_UDP_DEBUG_OUTPUT
		else LOG("ERROR","Failed to close the socket ('%s')\n\n", strerror(errno));
		#endif
		return nRet;
	}
	return 0;
}

// 成功返回0,否则返回-1
int CUdpClient::BindAddress(const char *pIp, int nPort)
{
	int nRet = -1;
	//if((NULL != pIp) && (nPort > 0))
	{
		if(m_conn <= 0)
			m_conn = socket(AF_INET, SOCK_DGRAM, 0);

		if(m_conn > 0)
		{
			struct sockaddr_in addr;
			socklen len = sizeof(addr);

			memset(&addr, 0, len);
			addr.sin_family = AF_INET;       // IPV4
			if(nPort > 0)
			addr.sin_port = htons(nPort);
			//addr.sin_addr.s_addr = htonl(INADDR_ANY);
			addr.sin_addr.s_addr = (NULL != pIp) ? inet_addr(pIp) : htonl(INADDR_ANY);

		
			#ifdef _WIN32
				nRet = bind((SOCKET)m_conn, (const sockaddr*)&addr, len);
			#elif __linux__
				nRet = bind(m_conn, (const sockaddr*)&addr, len);
			#endif
			if (nRet < 0)
			{
				#ifdef USE_UDP_DEBUG_OUTPUT
				LOG("ERROR","Failed to bind address '%s:%d' ('%s')\n\n", pIp, nPort, strerror(errno));
				#endif
				close_socket(m_conn);
				m_conn = 0;
			}
			//else DEBUG("ERROR: Binding address successfully '%s:%d' ret:%d\n\n", pIp, nPort, nRet);
			// Time out
			//struct timeval tv;
			//tv.tv_sec = 0;
			//tv.tv_usec = 200000;  // 200 ms
			//setsockopt(m_conn, SOL_SOCKET, SO_RCVTIMEO, (const char*)&tv, sizeof(struct timeval));
		}
		#ifdef USE_UDP_DEBUG_OUTPUT
		else LOG("ERROR","Socket error ('%s')\n\n", strerror(errno));
		#endif
	}
	#ifdef USE_UDP_DEBUG_OUTPUT
	//else DEBUG("ERROR: ip=%p, port=%d\n\n", pIp, nPort);
	#endif
	return nRet;
}

void CUdpClient::GetAddress(char *pIp, int *pPort)
{
	if(m_conn > 0)
	{
		sockaddr_in  addr;
		static int iLen = sizeof(sockaddr_in);
		//getpeername(*m_pConnSocket,(SOCKADDR*)&addr,&iLen);
		int nRet = getsockname(m_conn, (sockaddr*)&addr, (socklen*)&iLen);
		if(0 == nRet)
		{
			if(NULL != pIp)
				strcpy(pIp, inet_ntoa(addr.sin_addr));
			if(NULL != pPort)
				*pPort = ntohs(addr.sin_port);
		}
		#ifdef USE_UDP_DEBUG_OUTPUT
		else LOG("ERROR","getsockname failed ('%s')\n\n", strerror(errno));
		#endif
	}
}

int CUdpClient::Send(const void *pData, int uLen, const char *pIp/*=NULL*/, int nPort/*=0*/)
{
	int nRet = -1;
	if ((m_conn > 0) && (NULL != pData))
	{
		if (NULL == pIp)
			pIp = m_peer_ip;
		if (nPort <= 0)
			nPort = m_peer_port;
		struct sockaddr_in addr;
		static socklen len = sizeof(addr);
		addr.sin_family = AF_INET;
		addr.sin_port = htons(nPort);
		addr.sin_addr.s_addr = inet_addr(pIp);
		nRet = sendto(m_conn, (const char*)pData, uLen, 0, (sockaddr*)&addr, len);
		if(nRet <= 0)
			LOG("ERROR","UDP send data(%p,%d) to '%s:%d' failed %d \n\n", pData, uLen,pIp ? pIp : "null", nPort, nRet);
	}
	else LOG("ERROR","m_conn=%d,pData=%p,pIp=%p,nPort=%d\n\n", m_conn, pData, pIp, nPort);
	return nRet;
}

int CUdpClient::Recv(void *pBuf, int nSize, char *pIp/*=NULL*/, int *pPort/*=NULL*/)
{
	int nRet = -1;
	if (m_conn <= 0)
	{
		LOG("WARN","UDP client is not open %d\n\n", m_conn);
		return nRet;
	}
	struct sockaddr_in peer_addr;
	static socklen len = sizeof(peer_addr);
#if 0
	if ((m_conn > 0) && (NULL != pBuf) && (nSize > 0))
	{
	#if 0
		int failed_count = 0;
		do {
			nRet = recvfrom(m_conn, (char*)pBuf, nSize, 0, (sockaddr*)&peer_addr, &len);
			if (nRet <= 0) {
				failed_count++;
				Sleep(50);
			}
		} while ((nRet <= 0) && (failed_count <= 25));
	#else
		nRet = recvfrom(m_conn, (char*)pBuf, nSize, 0, (sockaddr*)&peer_addr, &len);
	#endif
		if (nRet >= 0)
		{
			if (NULL != pIp)
				inet_ntop(AF_INET, &peer_addr.sin_addr, pIp, 16);//C++11 (新版)
			if (NULL != pPort)
				*pPort = ntohs(peer_addr.sin_port);
		}
	}
#else
	#ifdef _WIN32
		static struct timeval tv = { 0,600000 };	// 成员1:秒, 成员2:微秒
	#elif __linux__
		static struct timespec ts = { 0,600000000 };		// 成员1:秒, 成员2:纳秒
	#endif
	fd_set	recvfd;						// 套接字集合
	FD_ZERO(&recvfd);					// 清空套接字集合
	FD_SET(m_conn, &recvfd);			// 加入感兴趣的套接字到集合中,
	#ifdef _WIN32
		int ret = select(0, &recvfd, NULL, NULL, &tv);	//select会更新这个集合,将其中不可读的套接字清除掉,只保留符合条件的套接字在里面
	#elif __linux__
		int ret = pselect(m_conn + 1, &recvfd, NULL, NULL, &ts, NULL);
	#endif
	if (ret > 0)		// 小于0: select函数出错；等于0: 等待超时；大于0：某些文件可读写或出错
	{
		if (FD_ISSET(m_conn, &recvfd))// 检查加入的套接字是否在这个集合里面
		{
			nRet = recvfrom(m_conn, (char*)pBuf, nSize, 0, (sockaddr*)&peer_addr, &len);
			if (nRet > 0)
			{
				if (NULL != pIp)
					inet_ntop(AF_INET, &peer_addr.sin_addr, pIp, 16);
				if (NULL != pPort)
					*pPort = ntohs(peer_addr.sin_port);
			}
			else LOG("ERROR","recvfrom=%d\n\n", nRet);
		}
		else LOG("ERROR","FD_ISSET not \n\n");
	}
	//else if(0 == ret)
	//	DEBUG("Timeout %d\n", nRet);
	else if (ret < 0)
		LOG("ERROR","select function error %d\n", ret);
#endif
	return nRet;
}

int CUdpClient::SetSockOption(int nOptName, const void* pValue, int nValueLen)
{
	//int setsockopt(int sockfd, int level, int optname,const void *optval, socklen_t optlen);
	#ifdef _WIN32
	int nRet = setsockopt(m_conn, SOL_SOCKET, nOptName, (const char*)pValue, nValueLen);
	#elif __linux__
	int nRet = setsockopt(m_conn, SOL_SOCKET, nOptName, pValue, (socklen_t)nValueLen);
	#endif
	if (0 != nRet)
		LOG("ERROR","setsockopt failed [ret:%d value:%d]\n\n", nRet, *((int*)pValue));
	return nRet;
}

int CUdpClient::GetSockOption(int nOptName, void* pBuf, int nBufSize)
{
	//int getsockopt(int sockfd, int level, int optname,void *optval, socklen_t *optlen);
	#ifdef _WIN32
	int nRet = getsockopt(m_conn, SOL_SOCKET, nOptName, (char*)pBuf, &nBufSize);
	#elif __linux__
	int nRet = getsockopt(m_conn, SOL_SOCKET, nOptName, pBuf, (socklen_t*)&nBufSize);
	#endif
	if (0 != nRet)
		LOG("ERROR","getsockopt failed [ret:%d]\n\n", nRet);
	return nRet;
}

void CUdpClient::SetPeerAddress(const char *pPeerIp, int nPeerPort)
{
	if ((NULL != pPeerIp) && (nPeerPort > 0))
	{
		strncpy(m_peer_ip, pPeerIp, sizeof(m_peer_ip) - 1);
		m_peer_port = nPeerPort;
	}
}


#ifdef USE_THREAD
void CUdpClient::SetCallback(UDP_MSG_CBK cbk, void *arg/*=NULL*/, int arg_size/*=0*/)
{
	if ((NULL != m_pUserData) && (m_UserDataSize > 0))
	{
		free(m_pUserData);
		m_pUserData = NULL;
		m_UserDataSize = 0;
	}
	m_pRecvCallback = cbk;
	m_pUserData = arg;
	if ((NULL != arg) && (arg_size > 0))
	{
		m_pUserData = malloc(arg_size + 1);
		if(NULL != m_pUserData)
		{
			memset(m_pUserData, 0, arg_size + 1);
			memcpy(m_pUserData, arg, arg_size);
			m_UserDataSize = arg_size;
		}
	}
	if (NULL != m_pRecvCallback)
	{
		m_recv_thd.Run(OnRecvProc, this);
	}
}

void CUdpClient::Join()
{
	m_recv_thd.Join();
}

void CUdpClient::Detach()
{
	m_recv_thd.Detach();
}

void STDCALL CUdpClient::OnRecvProc(STATE &state, PAUSE &p, void *pUserData/*=NULL*/)
{
	//LOG("INFO","---------- UDP receiving thread is running ... ----------\n\n");
	CUdpClient *pClient = (CUdpClient*)pUserData;
	#ifdef _WIN32
		struct timeval tv = { 1,0 };					// 成员1:秒, 成员2:微秒
	#elif __linux__
		struct timespec ts = { 1,0 };
	#endif
	char *buff = new char[RECV_BUFF_SIZE + 1];
	fd_set	recvfd;									//套接字集合
	char peer_ip[24] = {0};
	int peer_port = 0;
	int nRet = 0;
	while (TS_STOP != state)
	{
		if (TS_PAUSE == state)
		{
			p.wait();
		}
		FD_ZERO(&recvfd);					// 清空套接字集合
		FD_SET(pClient->m_conn, &recvfd);	// 加入感兴趣的套接字到集合中,
		// select会更新这个集合,将其中不可读的套接字清除掉,只保留符合条件的套接字在里面
		#ifdef _WIN32
			nRet = select(0, &recvfd, NULL, NULL, &tv);	//select会更新这个集合,将其中不可读的套接字清除掉,只保留符合条件的套接字在里面
		#elif __linux__
			nRet = pselect(pClient->m_conn + 1, &recvfd, NULL, NULL, &ts, NULL);
		#endif
		if (nRet > 0)		// 小于0: select函数出错；等于0: 等待超时；大于0：某些文件可读写或出错
		{
			if (FD_ISSET(pClient->m_conn, &recvfd))// 检查加入的套接字是否在这个集合里面
			{
				nRet = pClient->Recv(buff, RECV_BUFF_SIZE, peer_ip, &peer_port);
				if ((NULL != pClient->m_pRecvCallback) && (nRet >= 0)) {
					buff[nRet] = 0;
					nRet = pClient->m_pRecvCallback(buff, nRet, peer_ip, peer_port, pClient->m_pUserData);
					if (nRet < 0)
						break;
				}
			}
		}
		//else if(0 == nRet)
		//	DEBUG("Timeout %d\n", nRet);
		#ifdef USE_UDP_DEBUG_OUTPUT
		else if(nRet < 0)
			LOG("ERROR","select function error %d\n", nRet);
		#endif
	}

	if (NULL != pClient->m_pRecvCallback) {
		pClient->m_pRecvCallback(NULL, 0, peer_ip, peer_port, pClient->m_pUserData);
	}

	if(NULL != buff)
		delete []buff;
	//LOG("INFO","---------- UDP receiving thread has exited ----------\n\n");
}
#endif