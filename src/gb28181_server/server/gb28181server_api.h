
#ifndef __MAIN_H__
#define __MAIN_H__
#include "sip.h"
#include "Thread11.h"

#define USE_KEEPALIVE_LISTEN

// 是否使用平台级联
#define USE_PLATFORM_CASCADE

// 使用合众思壮平台
#define USE_PLATFORM_UNIS
#define UDP_CLIENT

// 使用设备准入功能
//#define USE_DEVICE_ACCESS
/////////////////////////////////////////////////////////////////////////
#ifdef USE_PLATFORM_UNIS
typedef struct callback_
{
	void(*registration)(dev_info_t *dev, void *arg) = NULL;		// 注册回调(注册与注销)
	void(*registration2)(dev_info_t *dev, void *arg) = NULL;	// 注册回调(注册与注销)
	void(*query)(dev_info_t *dev, void *arg) = NULL;			// 查询回调
	void(*query_catalog)(dev_info_t *dev, void *arg) = NULL;	// 查询回调
	void(*keepalive)(dev_info_t *dev, void *arg) = NULL;		// 保活回调(上线与下线)
	void(*control)(const char *id, int status, void *arg) = NULL;	// 控制回调
	int(*query_record)(const char *dev_id, map<string, record_info_t> *rec_map) = NULL;// 查询录像回调
}callback_t;
int unis_init(local_cfg_t *cfg, pfm_info_t *pfm);
void unis_uninit();
int unis_open(const char *ip, int port);
int unis_close();
int unis_query(addr_info_t *dev,  operation_t *opt);// 设备查询
int unis_control(addr_info_t *dev, operation_t *opt);// 设备控制
int unis_play(addr_info_t *dev, operation_t *opt);	// 点播&回播
int unis_stop(const char *call_id);					// 停止点播&回播
#endif

////////////// 设备端消息事件 /////////////////////////////////////////
void device_event_proc(void *event, void *arg);
int on_device_register(osip_message_t *request, CSip *pSip, int tid, char *domain, char *password, const char *call_id);// 注册/注销
int on_device_message(osip_message_t *request, CSip *pSip, int tid, const char *call_id);// 消息
//int on_device_notify(eXosip_event_t *p_evt);		// 通知
//int on_device_subscribe(eXosip_event_t *p_evt);	// 订阅
int on_device_call_message(osip_message_t *request, CSip *pSip);

////////////// 平台端消息事件 //////////////////////////////////////////
#ifdef USE_PLATFORM_CASCADE
void platform_event_proc(void *event, void *arg);

int on_platform_register(eXosip_event_t *p_evt, CSip *pSip);
int on_platform_message(osip_message_t *request, const char *call_id, CSip *pSip, pfm_info_t *pfm);
int on_platform_subscribe_notify(eXosip_event_t *p_evt, const char *call_id, CSip *pSip, pfm_info_t *pfm);
int on_platform_call_invite(eXosip_event_t *p_evt, CSip *pSip, const char *call_id);
// [Bye][回播控制&暂停&快进&进退]
int on_platform_call_message(osip_message_t *request, CSip *pSip);

int add_platform(pfm_info_t *pfm);		// 添加平台
int del_platform(const char *id);		// 删除平台
void del_platforms();					// 删除所有平台
int update_platform(pfm_info_t *pfm);	// 更新平台信息

void* platform_register_proc(void *arg);

int STDCALL on_record_playbak(void *pData, int nLen, void *pUserData);
#endif
/////////////////////////////////////////////////////////////////////////

typedef struct thd_handle
{
	struct osip_thread* hd = NULL;
	bool flag = false;
	void* arg = NULL;
	int arg_size = 0;
}thd_handle_t;

// 是否监听IPC设备保活
#ifdef USE_KEEPALIVE_LISTEN
void* keepalive_proc(void *arg);
#endif

#ifdef UDP_CLIENT
int STDCALL on_udp_receive(const void *pMsg, int nLen, const char* pIp, int nPort, void *pUserData);
string get_cmd(cmd_e cmd);
#endif

#ifdef USE_COMPILE_EXE
int main(int argc, char *argv[]);
int do_main(int autom=0);
int init(int argc, char *argv[]);
void release();
#endif


#ifdef __cplusplus
extern "C" {
#endif
// ...
#ifdef __cplusplus
}
#endif

#endif