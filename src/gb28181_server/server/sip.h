

#ifndef __SIP_H__
#define __SIP_H__

#include <string>
#include <string.h>
#include "common.h"
#include "cmap.h"

#include <eXosip2/eXosip.h>
#include <osip2/osip_mt.h>

using namespace std;


#ifdef _WIN32
    #define STDCALL __stdcall
#elif __linux__
    #include <netinet/in.h>		// struct sockaddr_in
    #define STDCALL __attribute__((__stdcall__))
#endif


#define USE_GB28181_SERVER
#define USE_GB28181_CLIENT

#define USE_TEST


typedef void (*EVT_CBK)(void *event, void *arg);
typedef void (*DEV_CBK)(dev_info_t *dev, void *arg);

class CSip
{

public:
	CSip();
	~CSip();

///////////// 服务端 //////////////////
#ifdef USE_GB28181_SERVER
    int open(const char *ip, int port);
    void close();

    void set_callback(EVT_CBK pCallback, void* pUserData);

    osip_authorization_t* get_authorization(const osip_message_t *sip, int pos=0);
    int authorization(const osip_authorization_t *auth, const char *password);
    int answer_401(int tid, const char *pRealm, const char *pNonce, const char* call_id=NULL);
    int answer(int tid, int status, const char* call_id=NULL);
    int get_expires(const osip_message_t *request);

    int message_response(cmd_e cmd_type, dev_info_t *dev, addr_info_t *from, addr_info_t *to, addr_info_t *route=NULL);

	// 订阅与通知
	int subscribe_answer(int tid, int status, const char* call_id = NULL);
	int subscribe_notify(const char *body, int did, int status, int reason, const char* call_id = NULL);
	int notify_response(cmd_e cmd_type, dev_info_t *dev, int did, int status, int reason, const char* call_id = NULL);


    int call_invite(addr_info_t *from, addr_info_t *to, operation_t *opt, const char *dev_id = NULL);
    int call_ack(int did, const char* call_id=NULL);
    int call_answer(int tid, int status_code, const char *sdp, const char *call_id);
    int call_terminate(int cid, int did);


#endif

///////////// 客户端 //////////////////
#ifdef USE_GB28181_CLIENT
    int regist(addr_info_t *from, pfm_info_t *pfm, bool is_auth, int rid=0, const char *realm=NULL, const char *to_tag=NULL, size_t cseq=1);
    int unregist(addr_info_t *from, pfm_info_t *pfm, bool is_auth, int rid = 0, const char *realm = NULL, const char *to_tag = NULL);
    osip_www_authenticate_t* get_www_authenticate(const osip_message_t *sip, int pos=0);//获取'401 Unauthorized'中的字段'WWW-Authenticate'的信息
	int keepalive(addr_info_t *from, addr_info_t *to, int sn=0, const char *call_id=NULL);

	//int eXosip_register_remove(struct eXosip_t *excontext, int rid);
	int register_remove(int rid);

#endif

    int query_device(addr_info_t *from, addr_info_t *to, const operation_t *opt,const char *dev_id=NULL);
    int control_device(addr_info_t *from, addr_info_t *to, const operation_t *opt);

	int message_request(const char *body, addr_info_t *from, addr_info_t *to, addr_info_t *route = NULL, const char* call_id = NULL);
	int call_request(int did, const char *body, const char* call_id = NULL);
	int call_request(const char *dev_id, addr_info_t *from, addr_info_t *to, const char* call_id = NULL);

    int get_from(const osip_message_t *request, sip_from_t *from);
    int get_to(const osip_message_t *request, sip_to_t *to);
	int get_via(const osip_message_t *request, sip_via_t *via, int pos = 0);
	int get_contact(const osip_message_t *request, sip_contact_t *contact, int pos = 0);



    std::string get_call_id(const osip_message_t *sip);
    const char* get_body(const osip_message_t *request);


	int get_remote_sdp(int did, string &sdp_str, sdp_info_t *sdp = NULL);
	int update_remote_sdp(int did, string &sdp_str, const char *media_ip, int media_port);
	int make_sdp(const sdp_info_t *sdp, string &sdp_str);

    
    int set_call_id(osip_message_t *sip, const char *call_id);
	int set_cseq(osip_message_t *sip, unsigned int number, const char *method);

    int guess_local_ip(char *address, int size, int family=AF_INET);

    time_t string_to_timestamp(const char *time_str);
    string timestamp_to_string(time_t t, char ch='-');

	unsigned int generate_random_number();

    std::string random_string(int length);

#ifdef USE_TEST
    void print_event(osip_message_t *sip);
#endif

private:
	int init();
	void release();

private:
    struct osip_thread* m_evt_thd = NULL;
    bool m_evt_thd_flag = false;
    static void* event_proc(void *arg);

private:
	struct eXosip_t* m_pContext = NULL;
    EVT_CBK          m_pEvtCbk = NULL;
    void*            m_pUserData = NULL;

public:
    int              m_sn = 1;
};










#endif

