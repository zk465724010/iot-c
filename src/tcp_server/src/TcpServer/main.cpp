#include <stdio.h>
#include "main.h"
#include "tinyxml.h"
#ifdef _WIN32
#include <Windows.h>
#define _sleep(n) Sleep(n)
#elif __linux__
#include <unistd.h>
#define _sleep(n) usleep((n)*1000)
#endif


CTcpServer g_server;
CThread11  g_control_thd;
CThread11* g_auto_snd_thd = NULL;
tcp_server_cfg_t g_cfg;
map<size_t, CTcpClient*> g_client_map;

int main(int argc, char *argv[])
{
    printf("Tcp Server\n\n");
	srand(time(NULL));

	int nRet = 0;
	if (argc > 1)
		nRet = load_config(&g_cfg, argv[1]);
	else nRet = load_config(&g_cfg, "TcpServer.xml");
	if (0 == nRet) {
		print_config(&g_cfg);

		g_server.SetReuseAddress(g_cfg.reuse_address);
		nRet = g_server.Open(g_cfg.local_ip, g_cfg.local_port);
		if (0 == nRet) {
			g_server.SetCallback(on_accept);

			if (g_cfg.s.enable) {
				g_auto_snd_thd = new CThread11();
				g_auto_snd_thd->Run(auto_send_proc);
			}

			g_control_thd.Run(control_proc);
			g_control_thd.Join();

			if (NULL != g_auto_snd_thd) {
				g_auto_snd_thd->Stop();
				delete g_auto_snd_thd;
				g_auto_snd_thd = NULL;
			}

			g_server.Close();
		}
		else LOG3("ERROR", "Open server '%s:%d' failed %d\n", g_cfg.local_ip, g_cfg.local_port, nRet);
	}

	#ifdef _WIN32
	system("pause");
	#endif
    return 0;
}

int STDCALL on_accept(void* pClient, void *pUserData)
{
	CTcpClient *client = (CTcpClient*)pClient;
	char peer_ip[16] = { 0 };
	int peer_port = 0;
	client->GetPeerAddress(peer_ip, &peer_port);
	client->SetCallback(on_message);
	client->SetSendBuffer(g_cfg.s.buf_size);
	client->SetRecvBuffer(g_cfg.s.buf_size);

	LOG3("INFO", "Tcp client connect '%s:%d'\n\n", peer_ip, peer_port);
	g_client_map.insert(map<size_t, CTcpClient*>::value_type(client->GetId(), client));
	return 0;
}

int STDCALL on_message(tcp_msg_t *pMsg, void *pUserData)
{
	LOG3("INFO", "Message: [%d] %s\n", pMsg->head.size, pMsg->body);
	return 0;
}
void STDCALL control_proc(STATE& state, PAUSE& p, void* pUserData)
{
	int nRet = 0, data_min_len = 32;
	char ch = 0;
	while (TS_STOP != state) {
		ch = get_char();
		switch (ch) {
		case '1': {
			std::string data = random_string(data_min_len + (rand() % data_min_len));
			auto iter = g_client_map.begin();
			for (; iter != g_client_map.end(); iter++) {
				if (NULL != iter->second) {
					nRet = iter->second->Send(data.c_str(), data.length());
					if (nRet <= 0) {
						char peer_ip[16] = { 0 };
						int peer_port = 0;
						iter->second->GetPeerAddress(peer_ip, &peer_port);
						LOG3("ERROR", "Send to '%s:%d' failed %d\n", peer_ip, peer_port, nRet);
					}
				}
			}
		}break;
		case '3': {	// 启用与禁用'转发消息'
			g_cfg.forward = !g_cfg.forward;
			LOG3("INFO", "Forward message enable=%s\n", g_cfg.forward ? "true" : "false");
		}break;
		case '4': {	// 暂停与继续'自动发送消息'
			if (NULL != g_auto_snd_thd) {
				STATE state = g_auto_snd_thd->GetState();
				if (TS_RUNING == state) {
					g_auto_snd_thd->Pause();
					LOG3("INFO", "Auto send message ['Pause']\n");
				}
				else if (TS_PAUSE == state) {
					g_auto_snd_thd->Continue();
					LOG3("INFO", "Auto send message ['Continue']\n");
				}
			}
			else LOG3("WARN", "Not enable auto send");
		}break;
		case 'e': {
			return;
		}break;
		default: LOG3("ERROR", "Invalid command '%c'\n", ch);
		}
	}
}
void STDCALL auto_send_proc(STATE& state, PAUSE& p, void* pUserData)
{
	FILE *fp = fopen(g_cfg.s.filename, "rb");
	if (NULL == fp) {
		LOG3("ERROR", "File does not exist '%s'\n\n", g_cfg.s.filename);
		return;
	}
	char *buff = NULL;
	try {
		buff = new char[g_cfg.s.pkt_size + 1];
	}
	catch (const bad_alloc& e) {
		LOG3("ERROR", "Memory allocation failed %p\n\n", buff);
		fclose(fp);
		return;
	}
	int nRet = 0;
	#ifdef _WIN32
	int interval = g_cfg.s.interval;
	#elif __linux__
	int interval = g_cfg.s.interval * 1000;
	#endif
	bool flag = true;
	while (TS_STOP != state) {
		if (TS_PAUSE == state) {
			p.wait();
		}
		if ((g_client_map.size() <= 0) && (TS_RUNING == state)) {
			if (flag) {
				flag = false;
				rewind(fp);
				LOG3("INFO", "No tcp client connection %d\n", g_client_map.size());
			}
			#ifdef _WIN32
				Sleep(2000);
			#elif __linux__
				usleep(2000000);
			#endif
			continue;
		}
		else {
			if (false == flag) {
				flag = true;
				LOG3("INFO", "Sending files ...\n\n");
			}
		}
		if (0 != feof(fp)) {	// 文件结束:返回非0值;  文件未结束:返回0值
			LOG3("INFO","Sending file completed.\n\n");
			if (g_cfg.s.cycle_send) {
				rewind(fp);
				//fseek(fp, 0, SEEK_SET);
			}
			else {
				g_client_map.clear();
				continue;
			}
		}
		nRet = fread(buff, 1, g_cfg.s.pkt_size, fp);
		if (nRet > 0) {
			auto iter = g_client_map.begin();
			for (; iter != g_client_map.end();) {
				// 发送数据
				nRet = iter->second->Send(buff, nRet, false);
				if (nRet <= 0) {
					char peer_ip[16] = { 0 };
					int peer_port = 0;
					iter->second->GetPeerAddress(peer_ip, &peer_port);
					LOG3("ERROR", "Send to '%s:%d' failed %d\n", peer_ip, peer_port, nRet);
					iter = g_client_map.erase(iter);
					continue;
				}
				iter++;
			}
		}

		#ifdef _WIN32
		Sleep(interval);
		#elif __linux__
		usleep(interval);
		#endif
	}
	fclose(fp);
	delete[] buff;
}

int load_config(tcp_server_cfg_t* cfg, const char* filename)
{
	int nRet = -1;
	TiXmlDocument document;
	#if 1
		if (false == document.LoadFile(filename)) {
			printf("\n\n");
			LOG3("ERROR", "File loading failed '%s'\n\n", filename);
			return nRet;
		}
	#else
		if (NULL == document.Parse(xml)) {
			LOG3("ERROR", "Parse xml failed '%s'\n\n", xml);
			return nRet;
		}
	#endif
	TiXmlNode* config = document.FirstChild("config");
	if (NULL != config) {
		if (NULL != cfg) {
			TiXmlElement* pElem = config->FirstChildElement("local_ip");
			if ((NULL != pElem) && (NULL != pElem->GetText()))
				strncpy(cfg->local_ip, pElem->GetText(), sizeof(cfg->local_ip) - 1);
			pElem = config->FirstChildElement("local_port");
			if ((NULL != pElem) && (NULL != pElem->GetText()))
				cfg->local_port = atoi(pElem->GetText());
			pElem = config->FirstChildElement("forward");
			if ((NULL != pElem) && (NULL != pElem->GetText()))
				cfg->forward = (0 == atoi(pElem->GetText())) ? false : true;
			pElem = config->FirstChildElement("reuse_address");
			if ((NULL != pElem) && (NULL != pElem->GetText()))
				cfg->reuse_address = (0 == atoi(pElem->GetText())) ? false : true;
			
			TiXmlElement* send = config->FirstChildElement("auto_send");
			if (NULL != send) {
				pElem = send->FirstChildElement("filename");
				if ((NULL != pElem) && (NULL != pElem->GetText()))
					strncpy(cfg->s.filename, pElem->GetText(), sizeof(cfg->s.filename) - 1);
				pElem = send->FirstChildElement("buf_size");
				if ((NULL != pElem) && (NULL != pElem->GetText()))
					cfg->s.buf_size = atoi(pElem->GetText());
				pElem = send->FirstChildElement("pkt_size");
				if ((NULL != pElem) && (NULL != pElem->GetText()))
					cfg->s.pkt_size = atoi(pElem->GetText());
				pElem = send->FirstChildElement("interval");
				if ((NULL != pElem) && (NULL != pElem->GetText()))
					cfg->s.interval = atoi(pElem->GetText());
				pElem = send->FirstChildElement("cycle_send");
				if ((NULL != pElem) && (NULL != pElem->GetText()))
					cfg->s.cycle_send = (0 == atoi(pElem->GetText())) ? false : true;
				pElem = send->FirstChildElement("enable");
				if ((NULL != pElem) && (NULL != pElem->GetText()))
					cfg->s.enable = (0 == atoi(pElem->GetText())) ? false : true;
			}
			nRet = 0;
		}
	}
	else LOG3("ERROR", "Configuration file format is error.\n");
	return nRet;
}
void print_config(tcp_server_cfg_t* cfg)
{
	if (NULL != cfg) {
		LOG3("INFO", "================================\n");
		LOG3("INFO", "local addr: %s:%d\n", cfg->local_ip, cfg->local_port);
		LOG3("INFO", "reuse_addr: %d\n", cfg->reuse_address);
		LOG3("INFO", "   forward: %d\n", cfg->forward);
		LOG3("INFO", "------------\n");
		LOG3("INFO", "    enable: %d\n", cfg->s.enable);
		LOG3("INFO", "cycle_send: %d\n", cfg->s.cycle_send);
		LOG3("INFO", "  filename: %s\n", cfg->s.filename);
		LOG3("INFO", "  buf_size: %d\n", cfg->s.buf_size);
		LOG3("INFO", "  pkt_size: %d\n", cfg->s.pkt_size);
		LOG3("INFO", "  interval: %d\n", cfg->s.interval);
		LOG3("INFO", "------------\n");
	}
}