

#ifndef __MAIN__H__
#define __MAIN__H__

#include "debug.h"
#include "Thread11.h"

#include <map>
//#include <string.h>
using namespace std;

typedef struct multicast {
	char addr[16] = { 0 };		// �鲥IP(��:"224.0.0.100")
	char local_ip[16] = { 0 };	// ����ӿ�(�������ж������ʱ,��ָ��һ������,��:"192.168.1.105")
	bool enable = true;			// �Ƿ�����
}multicast_t;

int timestamp_to_string(time_t tick, char *buf, int buf_size, char ch = '-');

typedef struct file {
	FILE *fp = NULL;
	char path[256] = { 0 };		// UDP��Ϣ����·��
	char name[128] = {0};		// �ļ�����
	char suffix[8] = {0};		// �ļ���׺��
	char mode[8] = {0};			// �ļ���ģʽ(��:"w","r","wb","a"��)
	bool enable = true;			// �Ƿ�����
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
	char text[512] = {0};		// �����ı�
	int interval = 5000;		// ���ͼ��ʱ��(��λ:����)
	bool enable = false;		// �Ƿ�����
}send_t;

typedef struct udp_cfg {
	char local_ip[16] = { 0 };	// ����IP
	int local_port = 0;			// ���ض˿�
	char peer_ip[16] = { 0 };	// �Է�IP
	int peer_port = 0;			// �Է��˿�
	bool forward = false;		// �Ƿ�ת����Ϣ
	send_t s;					// ��������
	file_t f;					// �ļ�����
	map<string, multicast_t> multicast;// �鲥�б�
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
