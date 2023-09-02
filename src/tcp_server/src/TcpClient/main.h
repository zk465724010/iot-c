
#ifndef __MAIN__H__

//#include <stdio.h>
#include <string.h>
#include "log.h"
#include "common_func.h"
#include "tcp_client.h"

typedef struct save_t {
	FILE* fp = NULL;
	char filename[256] = { 0 };	// TCP������·��
	char mode[8] = { 0 };		// �ļ�д��ģʽ(��:"w","r","wb","a"��)
	bool enable = false;			// �Ƿ�������Ϣ����
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
	bool enable = false;		// �Ƿ������Զ�����
	bool cycle_send = false;	// ѭ�������ļ�
	char filename[256] = { 0 };	// �������ļ�
	int interval = 100;			// ��Ϣ���ͼ��(��λ:����)
	int buf_size = 10485760;	// ϵͳ�շ���������С(Ĭ��10M,��10485760)
	int pkt_size = 1024;		// ÿ�η��͵����ݴ�С
}auto_send_t;

typedef struct tcp_client_cfg_t {
	char local_ip[16] = { 0 };	// ����IP
	int local_port = 0;			// ����TCP�˿�
	char peer_ip[16] = { 0 };	// �Է�IP
	int peer_port = 0;			// �Է�TCP�˿�
	bool forward = false;		// �Ƿ�ת��TCP��Ϣ
	bool reuse_address = false;	// �����ص�ַ���� (�˿ڸ���)
	auto_send_t s;
	save_t sa;
}tcp_client_cfg_t;

int load_config(tcp_client_cfg_t* cfg, const char* filename);
void print_config(tcp_client_cfg_t* cfg);

int STDCALL on_message(tcp_msg_t* pMsg, void* pUserData);
void STDCALL control_proc(STATE& state, PAUSE& p, void* pUserData);
void STDCALL auto_send_proc(STATE& state, PAUSE& p, void* pUserData);


#endif