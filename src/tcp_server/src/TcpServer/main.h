
#ifndef __MAIN__H__
#define __MAIN__H__

#include "log.h"
#include "tcp_server.h"
#include "tcp_client.h"
#include "common_func.h"

typedef struct auto_send_t {
	char filename[256] = { 0 };	// �������ļ�
	int interval = 100;			// ��Ϣ���ͼ��(��λ:����)
	int buf_size = 10485760;	// ϵͳ�շ���������С(Ĭ��10M,��10485760)
	int pkt_size = 1024;		// ÿ�η��͵����ݴ�С
	bool cycle_send = false;	// ѭ�������ļ�
	bool enable = false;		// �Ƿ������Զ�����
}auto_send_t;

typedef struct tcp_server_cfg_t {
	char local_ip[16] = { 0 };	// ����IP
	int local_port = 0;			// ����TCP�˿�
	bool forward = false;		// �Ƿ�ת��TCP��Ϣ
	bool reuse_address = false;	// �����ص�ַ���� (�˿ڸ���)
	auto_send_t s;
}tcp_server_cfg_t;

int load_config(tcp_server_cfg_t* cfg, const char* filename);
void print_config(tcp_server_cfg_t* cfg);

int STDCALL on_accept(void* pClient, void *pUserData);
int STDCALL on_message(tcp_msg_t *pMsg, void *pUserData);
void STDCALL control_proc(STATE& state, PAUSE& p, void* pUserData);
void STDCALL auto_send_proc(STATE& state, PAUSE& p, void* pUserData);

#endif