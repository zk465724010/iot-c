
#include "main.h"

#include "udp_client.h"
#include "tinyxml.h"
#include <sstream>
#include <stdio.h>


#ifdef _WIN32
#include <Windows.h>
#include <conio.h>
#elif __linux__
#include <termios.h>	// tcgetattr
#include <unistd.h>
#include <unistd.h>		// sleep(),usleep()
#include <iconv.h>		// iconv_open(),iconv(),iconv_close()
#endif

FILE *g_log_file = fopen("udp_client.log", "w");

CUdpClient g_udp_client;
CThread11 g_thd;
CThread11 *g_auto_snd_thd = NULL;	// 自动发送线程
udp_cfg_t g_cfg;

int main(int argc, char *argv[])
{
	int nRet = init(argc, argv);
	if (0 == nRet) {
		nRet = g_udp_client.Open(g_cfg.local_ip, g_cfg.local_port);
		if (0 == nRet) {
			LOG("INFO", "UDP client open succeed,listening '%s:%d'\n\n", g_cfg.local_ip, g_cfg.local_port);
			g_udp_client.SetCallback(on_message);
			g_thd.Run(control_proc);
			if (g_cfg.s.enable) {
				g_auto_snd_thd = new CThread11();
				g_auto_snd_thd->Run(send_proc);
			}

			g_thd.Join();

			if (NULL != g_auto_snd_thd) {
				g_auto_snd_thd->Stop();
				delete g_auto_snd_thd;
				g_auto_snd_thd = NULL;
			}
			g_udp_client.Close();

			if (NULL != g_cfg.f.fp) {
				fclose(g_cfg.f.fp);
				g_cfg.f.fp = NULL;
			}
		}
		else LOG("ERROR", "UDP client opened failed '%s:%d'\n\n", g_cfg.local_ip, g_cfg.local_port);
	}

	#ifdef _WIN32
	system("pause");
	#else
	LOG("INFO","\n Program exit.\n");
	#endif
    return nRet;
}
int init(int argc, char *argv[])
{
	srand(time(NULL));
	int nRet = 0;
	if (2 == argc) {
		nRet = load_config(&g_cfg, argv[1]);
	}
	else if (3 == argc) {
		strcpy(g_cfg.local_ip, argv[1]);
		g_cfg.local_port = atoi(argv[2]);
	}
	else if (4 == argc) {
		strcpy(g_cfg.local_ip, argv[1]);
		g_cfg.local_port = atoi(argv[2]);
		strcpy(g_cfg.peer_ip, argv[3]);

	}
	else if (5 == argc) {
		strcpy(g_cfg.local_ip, argv[1]);
		g_cfg.local_port = atoi(argv[2]);
		strcpy(g_cfg.peer_ip, argv[3]);
		g_cfg.peer_port = atoi(argv[4]);
	}
	else if (6 == argc) {
		strcpy(g_cfg.local_ip, argv[1]);
		g_cfg.local_port = atoi(argv[2]);
		strcpy(g_cfg.peer_ip, argv[3]);
		g_cfg.peer_port = atoi(argv[4]);
		g_cfg.forward = atoi(argv[6]);
	}
	else if (7 == argc) {
		strcpy(g_cfg.local_ip, argv[1]);
		g_cfg.local_port = atoi(argv[2]);
		strcpy(g_cfg.peer_ip, argv[3]);
		g_cfg.peer_port = atoi(argv[4]);
		g_cfg.forward = atoi(argv[6]);
		g_cfg.f.enable = atoi(argv[7]);
	}
	else {
		nRet = load_config(&g_cfg, "udp_cfg.xml");
	}
	print_config(&g_cfg);
	return nRet;
}

int STDCALL on_message(const void *pMsg, int nLen, const char* pIp, int nPort, void *pUserData)
{
	if ((NULL != pMsg) && (nLen > 0)) {
		if(NULL != strchr(g_cfg.f.mode, 'b'))
			printf("From '%s:%d': %p [%d]\n", pIp, nPort, (char*)pMsg, nLen);
		else printf("From '%s:%d': %s [%d]\n", pIp, nPort, (char*)pMsg, nLen);
		if (g_cfg.f.enable) {
			if (NULL == g_cfg.f.fp)
				g_cfg.f.open();
			if(NULL != g_cfg.f.fp)
				fwrite(pMsg, 1, nLen, g_cfg.f.fp);
			else LOG("ERROR","File open failed.\n\n");
		}
		if (g_cfg.forward) {
			int nRet = g_udp_client.Send(pMsg, nLen, g_cfg.peer_ip, g_cfg.peer_port);
			if (nRet != nLen){
				LOG("ERROR", "Send data to '%s:%d' failed %d\n\n", g_cfg.peer_ip, g_cfg.peer_port, nRet);
			}
			//#ifdef _WIN32
			//Sleep(200);
			//#elif __linux__
			//usleep(200000);
			//#endif
		}
	}
	return 0;
}
void STDCALL control_proc(STATE &state, PAUSE &p, void *pUserData)
{
	int nRet = 0;
	char ch = 0;
	while (TS_STOP != state) {
		ch = get_char();
		switch (ch) {
			case '1': {	// 手动发送消息
				int len = 32;
				std::string data = random_string(len + (rand() % len));
				nRet = g_udp_client.Send(data.c_str(), data.length(), g_cfg.peer_ip, g_cfg.peer_port);
				if (nRet != data.length()) {
					LOG("ERROR", "Send data to '%s:%d' failed %d\n\n", g_cfg.peer_ip, g_cfg.peer_port, nRet);
				}
			}break;
			case '2': {	// 启用与禁用'消息保存'
				g_cfg.f.enable = !g_cfg.f.enable;
				if (g_cfg.f.enable && (NULL == g_cfg.f.fp)) {
					nRet = g_cfg.f.open();
				}
				LOG("INFO", "Save message enable=%s\n", g_cfg.f.enable ? "true" : "false");
			}break;
			case '3': {	// 启用与禁用'转发消息'
				if ((strlen(g_cfg.peer_ip) > 0) && (g_cfg.peer_port > 0))
					g_cfg.forward = !g_cfg.forward;
				else g_cfg.forward = false;
				LOG("INFO", "Forward message enable=%s\n", g_cfg.forward ? "true" : "false");
			}break;
			case '4': {	// 暂停与继续'自动发送消息'
				if (NULL != g_auto_snd_thd) {
					STATE state = g_auto_snd_thd->GetState();
					if (TS_RUNING == state) {
						g_auto_snd_thd->Pause();
						LOG("INFO", "Auto send message ['Pause']\n");
					}
					else if (TS_PAUSE == state) {
						g_auto_snd_thd->Continue();
						LOG("INFO", "Auto send message ['Continue']\n");
					}
				}
				else LOG("WARN", "Not enable auto send");
			}break;
			case '5': {
			}break;
			case '6': {
			}break;
			case '7': {
			}break;
			case '8': {
			}break;
			case '9': {
				print_config(&g_cfg);
			}break;
			case 'e': {
				return;
			}break;
		}
	}
}
void STDCALL send_proc(STATE &state, PAUSE &p, void *pUserData)
{
	if ((strlen(g_cfg.s.text) <= 0) || (strlen(g_cfg.peer_ip) <= 0) || (g_cfg.peer_port <= 0))
		return;

	int nRet = 0, nLen = strlen(g_cfg.s.text);
	#ifdef _WIN32
	size_t interval = g_cfg.s.interval;
	#elif __linux__
	int nInterval = g_cfg.s.interval * 1000;
	size_t interval = nInterval;
	#endif
	while (TS_STOP != state)
	{
		if (TS_PAUSE == state) {
			p.wait();
		}
		nRet = g_udp_client.Send(g_cfg.s.text, nLen, g_cfg.peer_ip, g_cfg.peer_port);
		if(nRet == nLen) {
			printf("Send '%s:%d': %s [%d]\n", g_cfg.peer_ip, g_cfg.peer_port, g_cfg.s.text, nLen);
			#ifdef _WIN32
				interval = g_cfg.s.interval;
				Sleep(g_cfg.s.interval);
			#elif __linux__
				interval = nInterval;
				usleep(nInterval);
			#endif
		}
		else {
			LOG("ERROR", "Send data to '%s:%d' failed %d\n\n", g_cfg.peer_ip, g_cfg.peer_port, nRet);
			#ifdef _WIN32
				Sleep(interval);
				interval += 200;
			#elif __linux__
				usleep(interval);
				interval += 200000;
			#endif
		}
	}
}


std::string random_string(int length)
{
	static char str[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-=[],.~@#$%^&*)(_+><?/|\{}";
	static int len = strlen(str);
	//srand(time(NULL));				// 初始化随机数种子
	std::stringstream str_random;		// #include <sstream>
	for (int i = 0; i < length; i++)
	{
		str_random << str[rand() % len];
	}
	//static int min = 1000000;
	//static int max = 9999999999;
	//srand(time(NULL));
	//unsigned int nRand = rand() % (max - min + 1) + min;
	return str_random.str();
}

char get_char()
{
#ifdef _WIN32
	return _getch();	// #include <conio.h> // _getch();
#elif __linux__
	//#include <termios.h>  // tcgetattr
	//#include <unistd.h>
	struct termios stored_settings;
	struct termios new_settings;
	tcgetattr(0, &stored_settings);
	new_settings = stored_settings;
	new_settings.c_lflag &= (~ICANON);
	new_settings.c_cc[VTIME] = 0;
	new_settings.c_cc[VMIN] = 1;
	tcsetattr(0, TCSANOW, &new_settings);
	char c = getchar();
	putchar('\b'); // 删除回显
	//printf("input:  [%c]\n", c);
	tcsetattr(0, TCSANOW, &stored_settings); // 恢复终端参数
	return c;
#endif
}


int load_config(udp_cfg_t *cfg, const char *filename)
{
	int nRet = -1;
	TiXmlDocument document;
#if 1
	if (false == document.LoadFile(filename)) {
		printf("\n\n");
		LOG("ERROR", "File loading failed '%s'\n\n", filename);
		return nRet;
	}
#else
	if (NULL == document.Parse(xml)) {
		LOG("ERROR", "Parse xml failed '%s'\n\n", xml);
		return nRet;
	}
#endif

	TiXmlNode *config = document.FirstChild("config");
	if (NULL != config) {
		if (NULL != cfg) {
			TiXmlElement *pElem = config->FirstChildElement("local_ip");
			if ((NULL != pElem) && (NULL != pElem->GetText()))
				strncpy(cfg->local_ip, pElem->GetText(), sizeof(cfg->local_ip) - 1);
			pElem = config->FirstChildElement("local_port");
			if ((NULL != pElem) && (NULL != pElem->GetText()))
				cfg->local_port = atoi(pElem->GetText());
			pElem = config->FirstChildElement("peer_ip");
			if ((NULL != pElem) && (NULL != pElem->GetText()))
				strncpy(cfg->peer_ip, pElem->GetText(), sizeof(cfg->peer_ip) - 1);
			pElem = config->FirstChildElement("peer_port");
			if ((NULL != pElem) && (NULL != pElem->GetText()))
				cfg->peer_port = atoi(pElem->GetText());
			pElem = config->FirstChildElement("forward");
			if ((NULL != pElem) && (NULL != pElem->GetText()))
				cfg->forward = (0 == atoi(pElem->GetText())) ? false : true;
			TiXmlElement *send = config->FirstChildElement("auto_send");
			if (NULL != send) {
				pElem = send->FirstChildElement("text");
				if ((NULL != pElem) && (NULL != pElem->GetText()))
					strncpy(cfg->s.text, pElem->GetText(), sizeof(cfg->s.text) - 1);
				pElem = send->FirstChildElement("interval");
				if ((NULL != pElem) && (NULL != pElem->GetText()))
					cfg->s.interval = atoi(pElem->GetText());
				pElem = send->FirstChildElement("enable");
				if ((NULL != pElem) && (NULL != pElem->GetText()))
					cfg->s.enable = (0 == atoi(pElem->GetText())) ? false : true;
			}
			TiXmlElement *file = config->FirstChildElement("msg_sava");
			if (NULL != file) {
				pElem = file->FirstChildElement("path");
				if ((NULL != pElem) && (NULL != pElem->GetText()))
					strncpy(cfg->f.path, pElem->GetText(), sizeof(cfg->f.path) - 1);
				else strcpy(cfg->f.path, ".");
				pElem = file->FirstChildElement("name");
				if ((NULL != pElem) && (NULL != pElem->GetText()))
					strncpy(cfg->f.name, pElem->GetText(), sizeof(cfg->f.name) - 1);
				pElem = file->FirstChildElement("suffix");
				if ((NULL != pElem) && (NULL != pElem->GetText()))
					strncpy(cfg->f.suffix, pElem->GetText(), sizeof(cfg->f.suffix) - 1);
				pElem = file->FirstChildElement("mode");
				if ((NULL != pElem) && (NULL != pElem->GetText()))
					strncpy(cfg->f.mode, pElem->GetText(), sizeof(cfg->f.mode) - 1);
				else strcpy(cfg->f.mode, "wb");
				pElem = file->FirstChildElement("enable");
				if ((NULL != pElem) && (NULL != pElem->GetText()))
					cfg->f.enable = (0 == atoi(pElem->GetText())) ? false : true;
			}
			TiXmlElement *multicast = config->FirstChildElement("multicast");
			if (NULL != multicast) {
				TiXmlElement *item = multicast->FirstChildElement("item");
				while (NULL != item) {
					multicast_t m;
					TiXmlElement *pElem = item->FirstChildElement("addr");
					if ((NULL != pElem) && (NULL != pElem->GetText()))
						strncpy(m.addr, pElem->GetText(), sizeof(m.addr) - 1);
					pElem = item->FirstChildElement("interface");
					if ((NULL != pElem) && (NULL != pElem->GetText()))
						strncpy(m.local_ip, pElem->GetText(), sizeof(m.local_ip) - 1);
					pElem = item->FirstChildElement("enable");
					if ((NULL != pElem) && (NULL != pElem->GetText()))
						m.enable = (0 == atoi(pElem->GetText())) ? false : true;

					cfg->multicast.insert(map<string, multicast_t>::value_type(m.addr, m));
					item = item->NextSiblingElement("item");
				}
			}
			nRet = 0;
		}
	}
	else LOG("ERROR","Configuration file format is error.\n");
	return nRet;
}
void print_config(udp_cfg_t *cfg)
{
	if (NULL != cfg) {
		LOG("INFO", " local_ip: %s:%d\n", cfg->local_ip, cfg->local_port);
		LOG("INFO", " local_ip: %s:%d\n", cfg->peer_ip, cfg->peer_port);
		LOG("INFO", "  forward: %d\n", cfg->forward);
		LOG("INFO","----------\n");
		LOG("INFO", "     text: %s\n", cfg->s.text);
		LOG("INFO", " interval: %d\n", cfg->s.interval);
		LOG("INFO", "   enable: %d\n", cfg->s.enable);
		LOG("INFO", "----------\n");
		LOG("INFO", "     path: %s\n", cfg->f.path);
		LOG("INFO", "     name: %s\n", cfg->f.name);
		LOG("INFO", "   suffix: %s\n", cfg->f.suffix);
		LOG("INFO", "     mode: %s\n", cfg->f.mode);
		LOG("INFO", "   enable: %d\n", cfg->f.enable);
		LOG("INFO", "----------\n");
		auto iter = cfg->multicast.begin();
		for (; iter != cfg->multicast.end(); iter++) {
			LOG("INFO", "     addr: %s\n", iter->second.addr);
			LOG("INFO", "interface: %s\n", iter->second.local_ip);
			LOG("INFO", "   enable: %d\n", iter->second.enable);
			LOG("INFO", "\n");
		}
		LOG("INFO", "--------------------------\n\n");
	}
}

int timestamp_to_string(time_t tick, char *buf, int buf_size, char ch/*='-'*/)
{
	#define buff_size 64
	char buff[buff_size] = { 0 };
	//time_t tick = time(NULL);
#if 0
	// localtime获取系统时间并不是线程安全的(函数内部申请空间,返回时间指针)
	struct tm *pTime = localtime(&tick);
	if (ch == '-')
		strftime(buff, buff_size, "%Y-%m-%dT%H:%M:%S", pTime);
	else if (ch == '/')
		strftime(buff, buff_size, "%Y/%m/%d %H:%M:%S", pTime);
	else if (ch == '.')
		strftime(buff, buff_size, "%Y.%m.%d_%H.%M.%S", pTime);
	else strftime(buff, buff_size, "%Y%m%d%H%M%S", pTime);
#else
	#ifdef _WIN32
		// localtime_s也是用来获取系统时间,运行于windows平台下,与localtime_r只有参数顺序不一样
		struct tm t;
		localtime_s(&t, &tick);
		sprintf(buff, "%4.4d%c%2.2d%c%2.2d_%2.2d%c%2.2d%c%2.2d", t.tm_year + 1900, ch, t.tm_mon + 1, ch, t.tm_mday, t.tm_hour, ch, t.tm_min, ch, t.tm_sec);
	#elif __linux__
		// localtime_r也是用来获取系统时间,运行于linux平台下,支持线程安全
		struct tm t;
		localtime_r(&tick, &t);
		sprintf(buff, "%4.4d%c%2.2d%c%2.2d_%2.2d%c%2.2d%c%2.2d", t.tm_year + 1900, ch, t.tm_mon + 1, ch, t.tm_mday, t.tm_hour, ch, t.tm_min, ch, t.tm_sec);
	#endif
#endif
	int len = strlen(buff);
	int min = (buf_size > len) ? len : buf_size;
	if (NULL != buf)
		strncpy(buf, buff, min);
	return len;
}