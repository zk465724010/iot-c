
#ifndef __MAIN__H__

//#include <stdio.h>
#include <string.h>
#include "log.h"
#include "common_func.h"
#include "tcp_client.h"

typedef struct save_t {
	FILE* fp = NULL;
	char filename[256] = { 0 };	// TCP消保存路径
	char mode[8] = { 0 };		// 文件写入模式(如:"w","r","wb","a"等)
	bool enable = false;			// 是否启用消息保存
	save_t() {
		strcpy(mode, "w");
	}
	~save_t() {
		if (NULL != fp) {
			fclose(fp);
			fp = NULL;
			#ifdef _WIN32
				LOG3("INFO", "File closed '%s'\n\n", filename);
			#elif __linux__
				LOG3("INFO", "File closed '%s'\n\n", filename);
			#endif	
		}
	}
	int open() {
		if (NULL == fp) {
			if (strlen(filename) <= 0) {
				timestamp_to_string(time(NULL), filename, sizeof(filename), '_');
			}
			fp = fopen(filename, mode);
			if (NULL == fp) {
				LOG3("INFO", "File open failed '%s' [mode:'%s']\n\n", filename, mode);
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
}save_t;

typedef struct auto_send_t {
	bool enable = false;		// 是否启用自动发送
	bool cycle_send = false;	// 循环发送文件
	char filename[256] = { 0 };	// 待发送文件
	int interval = 100;			// 消息发送间隔(单位:毫秒)
	int buf_size = 10485760;	// 系统收发缓冲区大小(默认10M,即10485760)
	int pkt_size = 1024;		// 每次发送的数据大小
}auto_send_t;

typedef struct tcp_client_cfg_t {
	char local_ip[16] = { 0 };	// 本地IP
	int local_port = 0;			// 本地TCP端口
	char peer_ip[16] = { 0 };	// 对方IP
	int peer_port = 0;			// 对方TCP端口
	bool forward = false;		// 是否转发TCP消息
	bool reuse_address = false;	// 允许本地地址重用 (端口复用)
	auto_send_t s;
	save_t sa;
}tcp_client_cfg_t;

int load_config(tcp_client_cfg_t* cfg, const char* filename);
void print_config(tcp_client_cfg_t* cfg);

int STDCALL on_message(tcp_msg_t* pMsg, void* pUserData);
void STDCALL control_proc(STATE& state, PAUSE& p, void* pUserData);
void STDCALL auto_send_proc(STATE& state, PAUSE& p, void* pUserData);


#endif