
#ifndef __MAIN_H__
#define __MAIN_H__
#include "sip.h"

//#define USE_KEEPALIVE_LISTEN
#define USE_MAIN_TEST

// 是否使用平台级联
#define USE_PLATFORM_CASCADE

// 是否使用Sqlite数据库
// #define USE_SQLITE_DB


int init(int argc, char *argv[]);
void release();

void device_event_proc(void *event, void *arg);



///////////////////////////////////////////////////
// 消息相关
int on_register(osip_message_t *request, CSip *pSip, int tid, char *domain, char *password, const char *call_id);// 注册/注销

int on_message(osip_message_t *request, CSip *pSip, int tid, const char *call_id);// 消息
int on_notify(eXosip_event_t *p_evt);                    // 通知
int on_subscribe(eXosip_event_t *p_evt);                 // 订阅

int on_message_answered();          // 消息回答
int on_message_request_failure();   // 消息请求失败

///////////////////////////////////////////////////
// 点播相关
int on_call_invite(eXosip_event_t *p_evt, CSip *pSip, const char *call_id);
int on_call_answered();
int on_call_ack();
int on_call_message_new();
int on_call_server_failure();
int on_call_message_request_failure();
int on_call_proceeding();
int on_call_request_failure();
int on_call_closed();
int on_call_message_answered();
int on_call_released();



// 是否监听IPC设备保活
#ifdef USE_KEEPALIVE_LISTEN
static  struct osip_thread* g_keepalive_thd = NULL;
static  bool g_keepalive_thd_flag = true;
void* keepalive_proc(void *arg);
#endif


#ifdef USE_PLATFORM_CASCADE
void platform_event_proc(void *event, void *arg);
static struct osip_thread* g_platform_register_thd = NULL;
static  bool g_platform_register_thd_flag = true;
void* platform_register_proc(void *arg);

int on_platform_message(osip_message_t *request, CSip *pSip, int tid, const char *call_id);// 消息
int on_message_answered();          // 消息回答

#endif



#ifdef USE_MAIN_TEST
int main_test(int autom=0);
char get_char();
int read_config(const char *filename, local_cfg_t *local_cfg, CMap<string,dev_info_t> *dev_map, pfm_info_t *pfm);

#endif



#endif