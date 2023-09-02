

#ifndef __MAIN__H__
#define __MAIN__H__

#include "debug.h"
#include "Thread11.h"

#include <map>
//#include <string.h>
using namespace std;

typedef struct multicast {
	char addr[16] = { 0 };		// 组播IP(如:"224.0.0.100")
	char local_ip[16] = { 0 };	// 网络接口(当机器有多个网卡时,需指定一个网卡,如:"192.168.1.105")
	bool enable = true;			// 是否启用
}multicast_t;

int timestamp_to_string(time_t tick, char *buf, int buf_size, char ch = '-');

typedef struct file {
	FILE *fp = NULL;
	char path[256] = { 0 };		// UDP消息保存路径
	char name[128] = {0};		// 文件名称
	char suffix[8] = {0};		// 文件后缀名
	char mode[8] = {0};			// 文件打开模式(如:"w","r","wb","a"等)
	bool enable = true;			// 是否启用
	file() {
		strcpy(path, ".");
		strcpy(mode, "w");
	}
	~file() {
		if (NULL != fp) {
			fclose(fp);
			fp = NULL;
			#ifdef _WIN32
				LOG("INFO", "File closed '%s\\%s'\n\n", path, name);
			#elif __linux__
				LOG("INFO", "File closed '%s/%s'\n\n", path, name);
			#endif	
		}
	}
	int open() {
		if (NULL == fp) {
			char filename[256] = { 0 };
			if (strlen(path) <= 0)
				strcpy(path, ".");
			if (strlen(name) <= 0) {
				timestamp_to_string(time(NULL), name, sizeof(name));
				if (strlen(suffix) > 0) {
					if('.' != suffix[0])
						strcat(name, ".");
					strcat(name, suffix);
				}
			}
			if (strlen(mode) <= 0)
				strcpy(mode, "w");
			#ifdef _WIN32
				sprintf(filename, "%s\\%s", path, name);
			#elif __linux__
				sprintf(filename, "%s/%s", path, name);
			#endif
			fp = fopen(filename, mode);
			if (NULL == fp) {
				LOG("INFO", "File open failed '%s' [mode:'%s']\n\n", filename, mode);
				return -1;
			}
			return 0;
		}
		return 0;
	}
	void close() {
		if (NULL != fp) {
			fclose(fp);
			fp = NULL;
		}
	}
}file_t;

typedef struct send_t {
	char text[512] = {0};		// 发送文本
	int interval = 5000;		// 发送间隔时间(单位:毫秒)
	bool enable = false;		// 是否启用
}send_t;

typedef struct udp_cfg {
	char local_ip[16] = { 0 };	// 本地IP
	int local_port = 0;			// 本地端口
	char peer_ip[16] = { 0 };	// 对方IP
	int peer_port = 0;			// 对方端口
	bool forward = false;		// 是否转发消息
	send_t s;					// 发送配置
	file_t f;					// 文件配置
	map<string, multicast_t> multicast;// 组播列表
}udp_cfg_t;

int init(int argc, char *argv[]);
int load_config(udp_cfg_t *cfg, const char *filename);
void print_config(udp_cfg_t *cfg);
int STDCALL on_message(const void *pMsg, int nLen, const char* pIp, int nPort, void *pUserData);
void STDCALL control_proc(STATE &state, PAUSE &p, void *pUserData);
void STDCALL send_proc(STATE &state, PAUSE &p, void *pUserData);
char get_char();
std::string random_string(int length);

#endif
