
#ifndef __MAIN__H__
#define __MAIN__H__

#include "log.h"
#include "tcp_server.h"
#include "tcp_client.h"
#include "common_func.h"

typedef struct auto_send_t {
	char filename[256] = { 0 };	// 待发送文件
	int interval = 100;			// 消息发送间隔(单位:毫秒)
	int buf_size = 10485760;	// 系统收发缓冲区大小(默认10M,即10485760)
	int pkt_size = 1024;		// 每次发送的数据大小
	bool cycle_send = false;	// 循环发送文件
	bool enable = false;		// 是否启用自动发送
}auto_send_t;

typedef struct tcp_server_cfg_t {
	char local_ip[16] = { 0 };	// 本地IP
	int local_port = 0;			// 本地TCP端口
	bool forward = false;		// 是否转发TCP消息
	bool reuse_address = false;	// 允许本地地址重用 (端口复用)
	auto_send_t s;
}tcp_server_cfg_t;

int load_config(tcp_server_cfg_t* cfg, const char* filename);
void print_config(tcp_server_cfg_t* cfg);

int STDCALL on_accept(void* pClient, void *pUserData);
int STDCALL on_message(tcp_msg_t *pMsg, void *pUserData);
void STDCALL control_proc(STATE& state, PAUSE& p, void* pUserData);
void STDCALL auto_send_proc(STATE& state, PAUSE& p, void* pUserData);

#endif