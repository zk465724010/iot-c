

#include "gb28181server_api.h"
#include <stdio.h>
#include "parse_xml.h"
//#include "unis_map.h"
#include "log.h"
#include "udp_client.h"
#include "media.h"

#ifdef USE_COMPILE_EXE
	#include "test.h"
	#include <time.h>
	#include <sstream>		// stringstream
#endif


#ifdef _WIN32
	#include <windows.h>
#elif __linux__
	#include <unistd.h>			// sleep(),usleep()
	#include <signal.h>			// signal()
#else
#endif

#define USE_DEBUG_OUTPUT

#ifdef USE_PLATFORM_UNIS
//#include "media.h"
//#include "gb28181_JNIGB28181.h"
#endif

//////////// 日志 ///////////
FILE *g_log_file = fopen("gb28181server.log", "w");
const char *g_version = "2021-12-9 10:11";

CSip 	g_gb28181_server;
local_cfg_t g_local_cfg;

#ifdef USE_PLATFORM_CASCADE
CSip 	g_sip_client;
CMap<string, thd_handle_t*> g_pfm_thd_map;
#endif


// 是否监听IPC设备保活
#ifdef USE_KEEPALIVE_LISTEN
thd_handle_t g_keepalive_thd;
#endif

#ifdef USE_PLATFORM_UNIS
callback_t g_callback;
#endif

#ifdef USE_DEVICE_ACCESS
CMap<string, access_info_t> g_access_map;	// 准入设备集合
#endif
CMap<string, dev_info_t> g_dev_map;			// 设备集合 (key为设备ID)
CMap<string,session_t>	 g_ses_map;			// 回话集合 (key为设备Call ID)

// 本地录像回放(级联)
CMedia g_media(8000, 500);
CUdpClient  g_playbak_client;

#ifdef UDP_CLIENT
CUdpClient g_udp_client;
int g_udp_local_port = 9000;
char g_udp_peer_ip[24] = {0};
int g_udp_peer_port = 0;
#endif



#ifdef USE_COMPILE_EXE
void on_sigint_proc(int num)
{
	printf("recvive the signal is %d\n", num);
}
int main(int argc, char *argv[])
{
	printf("\n\n");
	LOG("INFO"," [ver=%s]\n\n", g_version);
	//signal(SIGINT, on_sigint_proc);
	int nRet = init(argc, argv);
	if (0 == nRet) {
		#if 1
			while(true) {
				nRet = do_main(0);
				if(-10 == nRet)
					break;
			}
		#else
			while (true) {
				#ifdef _WIN32
				Sleep(2000);
				#elif __linux__
				sleep(2);
				#endif
			}
		#endif
	}
	else LOG("ERROR", "Initialization failure [ret:%d]\n\n", nRet);
	release();

	#ifdef _WIN32
	system("pause");
	#endif
	if (NULL != g_log_file) {
		fclose(g_log_file);
		g_log_file = NULL;
	}
	return 0;
}
int init(int argc, char *argv[])
{
	int nRet = 0;
	const char *cfg_file = "gb28181server_cfg.xml";
	if (argc > 1)
		cfg_file = argv[1];

	#ifdef USE_PLATFORM_CASCADE
		CMap<string, pfm_info_t> pfm_map;
		#ifdef USE_DEVICE_ACCESS
			nRet = load_gb28181server_config(cfg_file, &g_local_cfg, &g_dev_map, &pfm_map, &g_access_map);
		#else
			nRet = load_gb28181server_config(cfg_file, &g_local_cfg, &g_dev_map, &pfm_map, NULL);
		#endif
	#else
		#ifdef USE_DEVICE_ACCESS
			nRet = load_gb28181server_config(cfg_file, &g_local_cfg, &g_dev_map, NULL, &g_access_map);
		#else
			nRet = load_gb28181server_config(cfg_file, &g_local_cfg, &g_dev_map, NULL, NULL);
		#endif
	#endif
	if (0 != nRet) {
		return nRet;
	}
	//auto dev = g_dev_map.begin();
	//for (; dev != g_dev_map.end(); dev++) {
	//	access_info_t acc;
	//	memcpy(&acc.addr, &dev->second.addr, sizeof(acc.addr));
	//	acc.auth = dev->second.auth;
	//	strcpy(acc.realm, g_local_cfg.domain);
	//	strcpy(acc.password, dev->second.password);
	//	acc.share = dev->second.share;
	//	g_access_map.add(acc.addr.id, &acc);
	//}
	print_local_info(&g_local_cfg);
	print_device_map(&g_dev_map);

	nRet = g_gb28181_server.open(g_local_cfg.addr.ip, g_local_cfg.addr.port);
	if (0 == nRet) {
		LOG("INFO","GB28181 service listening '%s:%d' ^-^\n", g_local_cfg.addr.ip, g_local_cfg.addr.port);

		g_gb28181_server.set_callback(device_event_proc, &g_gb28181_server);
	}
	else LOG("ERROR","GB28181 service started failed '%s:%d'\n", g_local_cfg.addr.ip, g_local_cfg.addr.port);

	#ifdef USE_PLATFORM_CASCADE
	nRet |= g_sip_client.open(g_local_cfg.dev.addr.ip, g_local_cfg.dev.addr.port);
	if (0 == nRet) {
		LOG("INFO","GB28181 client listening '%s:%d' ^-^\n\n", g_local_cfg.dev.addr.ip, g_local_cfg.dev.addr.port);
		g_sip_client.set_callback(platform_event_proc, &g_sip_client);
		auto pfm = pfm_map.begin();
		for (; pfm != pfm_map.end(); pfm++) {
			print_platform_info(&pfm->second);
			add_platform(&pfm->second);
			#ifdef _WIN32
			Sleep(100);
			#elif __linux__
			usleep(100000);
			#endif
		}
	}
	else LOG("ERROR","GB28181 client started failed '%s:%d'\n", g_local_cfg.dev.addr.ip, g_local_cfg.dev.addr.port);
	#endif

	#ifdef USE_KEEPALIVE_LISTEN
	g_keepalive_thd.flag = true;
	g_keepalive_thd.hd = osip_thread_create(20000, keepalive_proc, NULL);// 设备心跳保活线程
	#endif

	#ifdef UDP_CLIENT
		nRet |= g_udp_client.Open(g_local_cfg.addr.ip, g_udp_local_port);
		if (0 == nRet) {
			LOG("INFO", "UDP client listening '%s:%d' ^_^\n\n", g_local_cfg.addr.ip, g_udp_local_port);
			g_udp_client.SetCallback(on_udp_receive, NULL);
		}
		else LOG("ERROR", "UDP client opened failed '%s:%d'\n\n", g_local_cfg.addr.ip, g_udp_local_port);
	#endif
	return nRet;
}
void release()
{
	#ifdef USE_PLATFORM_CASCADE
		del_platforms();
		g_sip_client.close();
	#endif
	#ifdef USE_KEEPALIVE_LISTEN
		if (NULL != g_keepalive_thd.hd) {
			g_keepalive_thd.flag = false;
			osip_thread_join(g_keepalive_thd.hd);
			osip_free(g_keepalive_thd.hd);
			g_keepalive_thd.hd = NULL;
		}
	#endif
	g_gb28181_server.close();
}
#endif


#ifdef USE_PLATFORM_UNIS
// -1:参数有误 -2: -3: 
int unis_init(local_cfg_t *cfg, pfm_info_t *pfm)
{
    return 0;
}

void unis_uninit()
{
	#ifdef USE_PLATFORM_CASCADE
		del_platforms();
	#endif
	memset(&g_callback, 0, sizeof(g_callback));
}

int unis_open(const char *ip, int port)
{
	print_local_info(&g_local_cfg);
	int nRet = g_gb28181_server.open(ip, port);
	if(0 == nRet) {
		srand(time(NULL));		// 初始化随机数种子
		LOG("INFO", "GB28181 server started successfully,Is listening[%s:%d]\n\n", ip, port);
		g_gb28181_server.set_callback(device_event_proc, &g_gb28181_server);

		#ifdef USE_KEEPALIVE_LISTEN
			g_keepalive_thd.flag = true;
			g_keepalive_thd.hd = osip_thread_create(20000, keepalive_proc, NULL);// 设备心跳保活线程
		#endif
	}
	else LOG("ERROR", "GB28181 server started failed [%s:%d]\n\n", ip, port);

	#ifdef USE_PLATFORM_CASCADE
		//ip = g_local_cfg.dev.addr.ip;
		//port = g_local_cfg.dev.addr.port;
		port = port + 1;
		nRet |= g_sip_client.open(ip, port);
		if (0 == nRet) {
			LOG("INFO","GB28181 client started successfully,Is listening[%s:%d] ^-^\n\n", ip, port);
			g_sip_client.set_callback(platform_event_proc, &g_sip_client);
		}
		else LOG("ERROR", "GB28181 client started failed[%s:%d]\n\n", ip, port);
	#endif

	#ifdef UDP_CLIENT
		port = g_udp_local_port;	//  port + 1;
		nRet |= g_udp_client.Open(ip, port);
		if (0 == nRet) {
			g_udp_client.SetCallback(on_udp_receive, NULL);
			LOG("INFO","UDP client opened successfully ^_^ '%s:%d'\n\n", ip, port);
		}
		else LOG("ERROR","UDP client opened failed '%s:%d'\n\n", ip, port);
	#endif

	return nRet;
}

int unis_close()
{
	#ifdef UDP_CLIENT
		g_udp_client.Close();
	#endif

	g_gb28181_server.close();
	#ifdef USE_KEEPALIVE_LISTEN
		if (NULL != g_keepalive_thd.hd) {
			g_keepalive_thd.flag = false;
			osip_thread_join(g_keepalive_thd.hd);
			osip_free(g_keepalive_thd.hd);
			g_keepalive_thd.hd = NULL;
		}
	#endif

	#ifdef USE_PLATFORM_CASCADE
		g_sip_client.close();
	#endif
	return 0;
}

int unis_query(addr_info_t *dev,  operation_t *opt)
{
	if(QUERY_DEV_CATALOG == opt->cmd) {
		auto iter = g_dev_map.find(dev->id);
		if (iter != g_dev_map.end()) {
			iter->second.sum_num = 0;
			iter->second.num = 0;
		}
	}
	return g_gb28181_server.query_device(&g_local_cfg.addr, dev, opt);
}

int unis_control(addr_info_t *dev, operation_t *opt)
{
	return g_gb28181_server.control_device(&g_local_cfg.addr, dev, opt);
}

int unis_play(addr_info_t *dev, operation_t *opt)
{
	int nRet = -1;
	if ((NULL != dev) && (NULL != opt))
	{
		if (NULL != opt->call_id)
		{
			nRet = g_gb28181_server.call_invite(&g_local_cfg.addr, dev, opt);
			if (0 == nRet) {
				session_t ses;
				sprintf(ses.call_id, "%s", opt->call_id);
				g_ses_map.add(ses.call_id, &ses);
				LOG("INFO","------ Invite successful ^_^ [ses_id:%s]\n\n", ses.call_id);
			}
			else LOG("ERROR","call_invite failed '%s:%s:%d'[call_id:%s  ret:%d]\n\n", dev->id, dev->ip, dev->port, opt->call_id, nRet);
		}
		else LOG("ERROR","The call id is empty.\n\n");
	}
	else LOG("ERROR","dev=%p, opt=%p\n\n", dev , opt);
	return nRet;
}

int unis_stop(const char *call_id)
{
	int nRet = -1;
	if (NULL != call_id)
	{
		auto ses = g_ses_map.find(call_id);
		if (ses != g_ses_map.end())
		{
			nRet = g_gb28181_server.call_terminate(ses->second.d_cid, ses->second.d_did);
			if(0 == nRet)
				LOG("INFO","Call stop successful ^-^ [ret:%d call_id:%s cid:%d did:%d ses_size:%d]\n\n", nRet, call_id, ses->second.d_cid, ses->second.d_did, g_ses_map.size()-1);
			//else
			//	LOG("ERROR","Call stop failed [ret:%d call_id:%s cid:%d did:%d ses_size:%d]\n\n", nRet, call_id, ses->second.cid, ses->second.did, g_ses_map.size() - 1);
			g_ses_map.erase(ses);
		}
		else LOG("WARN","Session does not exist '%s'\n\n", call_id);
	}
	else LOG("ERROR","call_id=%p\n\n", call_id);
	return nRet;
}

#ifdef UDP_CLIENT
int STDCALL on_udp_receive(const void *pMsg, int nLen, const char* pIp, int nPort, void *pUserData)
{
	strcpy(g_udp_peer_ip, pIp);
	g_udp_peer_port = nPort;
	char current_time[64] = { 0 };
	timestamp_to_string(time(NULL), current_time, sizeof(current_time));
	udp_msg_t *msg = (udp_msg_t*)pMsg;
	static time_t start = time(NULL);// msg->start_time;
	LOG("INFO","======= UDP Message '%s:%d' ========\n", g_udp_peer_ip, g_udp_peer_port);
	LOG("INFO", "          dev: [%s:%s:%d]\n", msg->dev.id, msg->dev.ip, msg->dev.port);
	LOG("INFO", "        media: [%s:%d]\n", msg->media_ip, msg->media_port);
	LOG("INFO", "          cmd: %s\n", get_cmd(msg->cmd).c_str());
	LOG("INFO", "      call_id: [%s]\n", msg->call_id);
	LOG("INFO", " current_time: [%s][%ds]\n", current_time, (time(NULL)- start));
	LOG("INFO", "================================================\n");
	start = time(NULL);
	operation_t opt;
	opt.cmd = msg->cmd;
	if (CTRL_PLAY_START == msg->cmd) {
		opt.call_id = (strlen(msg->call_id) > 0) ? msg->call_id : NULL;
		strcpy(opt.media.ip, msg->media_ip);
		opt.media.port = msg->media_port;
		unis_play(&msg->dev, &opt);
	}
	else if (CTRL_PLAYBAK_START == msg->cmd) {
		opt.call_id = msg->call_id;
		strcpy(opt.media.ip, msg->media_ip);
		opt.media.port = msg->media_port;
		opt.record = new record_info_t();
		timestamp_to_string(msg->start_time, opt.record->start_time, sizeof(opt.record->start_time));
		timestamp_to_string(msg->end_time, opt.record->end_time, sizeof(opt.record->end_time));
		unis_play(&msg->dev, &opt);
		delete opt.record;
	}
	else if (CTRL_PLAY_STOP == msg->cmd) {
		int nRet = unis_stop(msg->call_id);
		if (0 == nRet){
			LOG("INFO", "Stop play successfully ^_^ [call_id:%s]\n\n", msg->call_id);
		}
		else LOG("ERROR","Stop play failed [call_id:%s]\n\n", msg->call_id);
	}
	else if ((CTRL_REC_START == msg->cmd) || (CTRL_REC_STOP == msg->cmd)) { // 录像与停止
		if (CTRL_REC_START == opt.cmd) {
			opt.record = new record_info_t();
			timestamp_to_string(msg->start_time, opt.record->start_time, sizeof(opt.record->start_time));
			timestamp_to_string(msg->end_time, opt.record->end_time, sizeof(opt.record->end_time));
		}
		int nRet = unis_control(&msg->dev, &opt);
		if (0 != nRet)
			LOG("ERROR","Control failed (cmd:%d dev:%s:%s:%d ret:%d)\n\n", msg->cmd, msg->dev.id, msg->dev.ip, msg->dev.port,nRet);
		if(NULL != opt.record)
			delete opt.record;
	}
	else if ((CTRL_DEV_CTRL < msg->cmd) && (msg->cmd <= CTRL_PTZ_STOP)) { // 云台控制
		unis_control(&msg->dev, &opt);
	}
	else LOG("WARN","Unknown command %d\n\n", msg->cmd);
	return 0;
}
string get_cmd(cmd_e cmd)
{
	string str;
	switch (cmd) {
	case UNKNOWN_CMD: str = "[Unknown_CMD]"; break;
	case QUERY_DEV_INFO: str = "[DeviceInfo]"; break;
	case QUERY_DEV_STATUS: str = "[DeviceStatus]"; break;
	case QUERY_DEV_CATALOG: str = "[Catalog]"; break;
	case QUERY_DEV_RECORD: str = "[QueryRecord]"; break;
	case QUERY_DEV_CFG: str = "[QueryConfig]"; break;
	case QUERY_DEV_ALARM: str = "[QueryAlarm]"; break;
	case QUERY_DEV_PRESET: str = "[QueryPreset]"; break;
	case QUERY_DEV_MP: str = "[QueryMP]"; break;
	case CTRL_DEV_CTRL: str = "[CtrlDevice]"; break;
	case CTRL_PTZ_LEFT: str = "[PTZ_Left]"; break;
	case CTRL_PTZ_RIGHT: str = "[PTZ_Right]"; break;
	case CTRL_PTZ_UP: str = "[PTZ_Up]"; break;
	case CTRL_PTZ_DOWN: str = "[PTZ_Down]"; break;
	case CTRL_PTZ_LARGE: str = "[PTZ_Large]"; break;
	case CTRL_PTZ_SMALL: str = "[PTZ_Small]"; break;
	case CTRL_PTZ_STOP: str = "[PTZ_Stop]"; break;
	case CTRL_REC_START: str = "[Rec_Start]"; break;
	case CTRL_REC_STOP: str = "[Rec_Stop]"; break;
	case CTRL_GUD_START: str = "[GUD_Start]"; break;
	case CTRL_GUD_STOP: str = "[GUD_Stop]"; break;
	case CTRL_ALM_RESET: str = "[ALM_Reset]"; break;
	case CTRL_TEL_BOOT: str = "[Tel_Bool]"; break;
	case CTRL_IFR_SEND: str = "[IFrame_Send]"; break;
	case CTRL_DEV_CFG: str = "[Ctrl_Config]"; break;
	case CTRL_PLAY_START: str = "[Play_Start]"; break;
	case CTRL_PLAY_STOP: str = "[Play_Stop]"; break;
	case CTRL_PLAYBAK_START: str = "[Playbak_Start]"; break;
	case CTRL_PLAYBAK_STOP: str = "[Playbak_Stop]"; break;
	case CTRL_DOWNLOAD_START: str = "[Download_Start]"; break;
	case CTRL_DOWNLOAD_STOP: str = "[Download_Stop]"; break;
	case CTRL_TALK_START: str = "[Talk_Start]"; break;
	case CTRL_TALK_STOP: str = "[Talk_Stop]"; break;
	case NOTIFY_KEEPALIVE: str = "[Keepalive]"; break;
	case NOTIFY_MEDIA_STATUS: str = "[Media_Status]"; break;
	case NOTIFY_ONLINE: str = "[Notify_Online]"; break;
	default: str = "[Unknown]";
	}
	return str;
}
#endif	// #ifdef UDP_CLIENT

////////////////////////////////////////////////////////////////////////////////////
#endif	// #ifndef USE_PLATFORM_UNIS

void device_event_proc(void *event, void *arg)
{
	//DEBUG("INFO: ------ event start ------\n\n");
    eXosip_event_t *p_evt = (eXosip_event_t*)event;
	CSip *pSip = (CSip*)arg;
    //DEBUG("event type %d [EXOSIP_CALL_ACK=%d]\n", p_evt->type);
    int nRet = 0;
    switch(p_evt->type)
    {
        case EXOSIP_REGISTRATION_SUCCESS:{	// 注册成功
        }break;
        case EXOSIP_REGISTRATION_FAILURE:{	// 注册失败
        }break;
        case EXOSIP_MESSAGE_NEW: {
			pSip->print_event(p_evt->request);
            std::string call_id = pSip->get_call_id(p_evt->request);
            if(MSG_IS_REGISTER(p_evt->request)) {
                nRet = on_device_register(p_evt->request, pSip, p_evt->tid, g_local_cfg.domain, g_local_cfg.password, call_id.c_str());
            }
            else if (MSG_IS_MESSAGE(p_evt->request)) {
				LOG("INFO","Device [New message][call_id:%s]\n\n", call_id.c_str());
				on_device_message(p_evt->request, pSip, p_evt->tid, call_id.c_str());
            }
            else if (MSG_IS_NOTIFY(p_evt->request)) {		// 通知
			}
			else if (MSG_IS_SUBSCRIBE(p_evt->request)) {	// 订阅
			}
			else if (MSG_IS_BYE(p_evt->request)) {	// Bye
				LOG("INFO","Device [New message][%s][call_id:%s]\n\n", p_evt->request->sip_method, call_id.c_str());
			}
			else LOG("WARN","Device [Message new][method:%s]\n\n", p_evt->request->sip_method);
        }break;
        case EXOSIP_MESSAGE_ANSWERED: {
			LOG("INFO","Device [Message answered]\n\n");
            pSip->print_event(p_evt->response);
        }break;
        case EXOSIP_MESSAGE_REQUESTFAILURE: {
			LOG("ERROR","Device [Message request failure]\n\n");
			pSip->print_event(p_evt->response);
		}break;
		case EXOSIP_CALL_INVITE: {
			LOG("ERROR","Device [Call invite]\n\n");
			pSip->print_event(p_evt->request);
		}break;
		case EXOSIP_CALL_ANSWERED: {	// 设备 -> Server
			string call_id = pSip->get_call_id(p_evt->response);
			LOG("INFO","Device [Call answered] [call_id:%s cid:%d did:%d ses_size:%d]\n", call_id.c_str(), p_evt->cid, p_evt->did, g_ses_map.size());
			pSip->print_event(p_evt->response);
			auto ses = g_ses_map.find(call_id);
			if(ses != g_ses_map.end()) {
				ses->second.d_cid = p_evt->cid;
				ses->second.d_did = p_evt->did;
				if (ses->second.p_tid > 0) {
					#ifdef USE_PLATFORM_CASCADE
					string sdp;
					pSip->get_remote_sdp(p_evt->did, sdp);
					LOG("INFO","Forward platform [Call answered]\n\n");
					g_sip_client.call_answer(ses->second.p_tid, 200, sdp.c_str(), call_id.c_str());
					#endif
				}
				else {
					pSip->call_ack(p_evt->did, call_id.c_str());
				}
			}
			else LOG("ERROR","Session does not exist '%s'\n\n", call_id.c_str());
        }break;
		case EXOSIP_CALL_MESSAGE_NEW: {	// 回播(设备 -> 网关) [Bye]
			LOG("INFO","Device [Call message new]\n\n");
			//pSip->print_event(p_evt->request);
			//string call_id = pSip->get_call_id(p_evt->request);
			//auto sec = g_ses_map.find(call_id.c_str());
			//if (sec != g_ses_map.end()) {
			//	g_ses_map.erase(sec);
			//	LOG("INFO", "ses_id:%s ses_size:%d\n\n", call_id.c_str(), g_ses_map.size());
			//}
			nRet = on_device_call_message(p_evt->request, pSip);
		}break;
		case EXOSIP_CALL_PROCEEDING: {
			LOG("INFO","Device [Call proceeding]\n\n");
			pSip->print_event(p_evt->response);
		}break; // announce processing by a remote app
		case EXOSIP_CALL_RINGING: {
			LOG("INFO","Device [Call ringing]\n\n");
			pSip->print_event(p_evt->response);
		}break; // announce ringback
		case EXOSIP_CALL_REQUESTFAILURE: {
			LOG("ERROR","Device [Call request failure]\n\n");
			pSip->print_event(p_evt->response);
			string call_id = pSip->get_call_id(p_evt->response);
			auto ses = g_ses_map.find(call_id);
			if (ses != g_ses_map.end()) {
				if (ses->second.p_tid > 0) {
					#ifdef USE_PLATFORM_CASCADE
					LOG("INFO", "Forward platform [Call request failure]\n\n");
					g_sip_client.call_answer(ses->second.p_tid, p_evt->response->status_code, NULL, call_id.c_str());
					#endif
				}
				g_ses_map.erase(ses);
			}
			else LOG("WARN", "Session does not exist '%s'\n\n", call_id.c_str());
		}break;
		case EXOSIP_CALL_MESSAGE_REQUESTFAILURE: {
			LOG("ERROR","Device [Call message request failure]\n\n");
			pSip->print_event(p_evt->response);
		}break;
		case EXOSIP_CALL_MESSAGE_ANSWERED: {
			LOG("INFO","Device [Call message answered]\n\n");
			pSip->print_event(p_evt->response);
		}break;
		case EXOSIP_CALL_CLOSED: {
			LOG("INFO", "Device [Call closed]\n\n");
			pSip->print_event(p_evt->response);
		}break;
		case EXOSIP_CALL_RELEASED: {
			LOG("INFO","Device [Call released]\n\n");
			pSip->print_event(p_evt->response);
			LOG("INFO","TextInfo:%s\n\n", p_evt->textinfo);
			LOG("INFO","ss_status:%d tid:%d\n\n", p_evt->ss_status, p_evt->tid);
		}break;
		default : {
			LOG("WARN","Device Unknown event %d\n\n", p_evt->type);
		}break;

    }
	//DEBUG("INFO: ------ event end ------\n\n");
}
int on_device_call_message(osip_message_t *request, CSip *pSip)
{
	int nRet = -1;
	if ((NULL != request) && (NULL != pSip)) {
		pSip->print_event(request);
		std::string call_id = pSip->get_call_id(request);
		auto ses = g_ses_map.find(call_id);
		if (ses != g_ses_map.end()) {
			if (MSG_IS_BYE(request)) {			// 停播
				LOG("INFO", "Forward to platform [Bye]: [call_id:%s ses_size:%d]\n\n", call_id.c_str(), g_ses_map.size() - 1);
				if ((ses->second.p_cid > 0) && (ses->second.p_did > 0))
					nRet = g_sip_client.call_terminate(ses->second.p_cid, ses->second.p_did);
				g_ses_map.erase(ses);
			}
			else LOG("WRAN", "Unknown call message [method:%s]\n\n", request->sip_method ? request->sip_method:"null");
		}
		else LOG("ERROR", "Session does not exist '%s' [ses_size:%d]\n\n", call_id.c_str(), g_ses_map.size());
	}
	else LOG("ERROR", "request=%p, pSip=%p\n\n", request, pSip);
	return nRet;
}
//  注册与注销
int on_device_register(osip_message_t *request, CSip *pSip, int tid, char *domain, char *password, const char *call_id)
{
    int nRet = -1;
	sip_contact_t contact;
	nRet = pSip->get_contact(request, &contact);
	int expires = pSip->get_expires(request);
	#ifdef USE_DEVICE_ACCESS
	auto access = g_access_map.find(contact.username);
	if (access == g_access_map.end()) {
		if(expires > 0)
			pSip->answer(tid, 403, call_id);
		else {
			pSip->answer(tid, 200, call_id);
			auto dev = g_dev_map.find(contact.username);
			if (dev != g_dev_map.end()) {
				#ifdef USE_PLATFORM_UNIS
				if (NULL != g_callback.registration2) {
					dev->second.expires = expires;
					g_callback.registration2(&dev->second, NULL);
				}
				#endif
				g_dev_map.erase(dev);
			}
		}
		LOG("INFO", "Unauthorized device '%s' %s [size:%d]\n\n", contact.username, ((expires>0)?"register":"unregister"), g_dev_map.size());
		return 0;
	}
	#endif	// #ifdef USE_DEVICE_ACCESS
	#ifdef USE_DEVICE_ACCESS
	if (0 != access->second.auth) { // 鉴权
	#else
	if (true) {		// 鉴权
	#endif
		auto auth = pSip->get_authorization(request);
		if (NULL == auth) {
			char nonce[24] = { 0 };
			eXosip_generate_random(nonce, 16);
			#ifdef USE_DEVICE_ACCESS
			pSip->answer_401(tid, access->second.realm, nonce, call_id);
			#else
			pSip->answer_401(tid, domain, nonce, call_id);
			#endif
			LOG("INFO", "Device Authentication information not found, answer 401\n\n");
			return 0;
		}
		#ifdef USE_DEVICE_ACCESS
		else if(0 != pSip->authorization(auth, access->second.password)){ // 鉴权未通过
		#else
		else if (0 != pSip->authorization(auth, password)) { // 鉴权未通过
		#endif

			char nonce[24] = { 0 };
			eXosip_generate_random(nonce, 16);
			#ifdef USE_DEVICE_ACCESS
			pSip->answer_401(tid, access->second.realm, nonce, call_id);
			#else
			pSip->answer_401(tid, domain, nonce, call_id);
			#endif
			LOG("INFO", "Device Authentication failed, answer 401\n\n");
			return 0;
		}
	}

    pSip->answer(tid, 200, call_id);

    if(expires > 0) {     // 注册
		auto iter = g_dev_map.find(contact.username);
		if (iter == g_dev_map.end()) {
			dev_info_t dev;
			strncpy(dev.parent_id, g_local_cfg.addr.id, sizeof(dev.parent_id) - 1);
			strcpy(dev.password, g_local_cfg.password);
			strcpy(dev.addr.id, contact.username);
			strcpy(dev.username, contact.username);
			strcpy(dev.addr.ip, contact.ip);
			dev.addr.port = contact.port;
			dev.status = 1;   // 状态(0:离线 1:在线)
			#ifdef USE_DEVICE_ACCESS
			dev.share = access->second.share;// 是否共享给上级(0:不共享 1:共享)
			dev.auth = access->second.auth;	// 是否开启鉴权
			#endif
			dev.keepalive_flag = dev.keepalive * dev.max_k_timeout;
			g_dev_map.add(dev.addr.id, &dev);
			//print_device_map(&g_dev_map);
			LOG("INFO","Device '%s' register,insert map [size:%d] ^-^\n\n", contact.username, g_dev_map.size());
			iter = g_dev_map.find(contact.username);
		}
		else {
			iter->second.status = 1;
			#ifdef USE_DEVICE_ACCESS
			iter->second.share = access->second.share;// 是否共享给上级（0:不共享 1:共享）
			iter->second.auth = access->second.auth;	// 是否开启鉴权
			#endif
			auto chl = iter->second.chl_map.begin();
			for (; chl != iter->second.chl_map.end(); chl++)
				chl->second.status = 1;
			iter->second.keepalive_flag = iter->second.keepalive * iter->second.max_k_timeout;
			LOG("INFO","Device '%s' refresh register [size:%d] ^-^\n\n", contact.username, g_dev_map.size());
		}

		#ifdef USE_PLATFORM_UNIS
		if(NULL != g_callback.registration2)
			g_callback.registration2(&iter->second, NULL);
		#endif
		operation_t opt;
		opt.cmd = QUERY_DEV_INFO;
		pSip->query_device(&g_local_cfg.addr, &iter->second.addr, &opt);
		//opt.cmd = QUERY_DEV_STATUS;
		//ret |= pSip->query_device(&g_local_cfg.addr, &dev.addr, &opt);
		opt.cmd = QUERY_DEV_CFG;
		pSip->query_device(&g_local_cfg.addr, &iter->second.addr, &opt);
		opt.cmd = QUERY_DEV_CATALOG;
		iter->second.sum_num = 0;
		iter->second.num = 0;
		pSip->query_device(&g_local_cfg.addr, &iter->second.addr, &opt);
	}
    else {               // 注销
		auto dev = g_dev_map.find(contact.username);
		if (dev != g_dev_map.end()) {
			#ifdef USE_PLATFORM_UNIS
			if (NULL != g_callback.registration2) {
				dev->second.expires = expires;
				g_callback.registration2(&dev->second, NULL);
			}
			#endif
			g_dev_map.erase(dev);
		}
		else LOG("ERROR","Device '%s' does not exist \n\n", contact.username);
		LOG("INFO","Device '%s' unregister succeed ^-^ [size:%d]\n\n", contact.username, g_dev_map.size());
    }
    return nRet;
}

int on_device_message(osip_message_t *request, CSip *pSip, int tid, const char *call_id)
{
	int nRet = -1;
    sip_from_t from;
    pSip->get_from(request, &from);
    auto dev = g_dev_map.find(from.username);
	if (dev != g_dev_map.end()) {
		nRet = pSip->answer(tid, 200, call_id);// 回答
	}
	else {
		LOG("ERROR","Device '%s' is not in the map\n\n", from.username);
		return nRet;
	}
    const char *body = pSip->get_body(request);
    if(NULL != body) {
		dev_info_t dev_info;
		parse_sip_body(body, &dev_info, &dev->second);

		//if (0 != strcmp(dev_info.addr.id, dev->second.addr.id)) {// 若'DeviceID'为子平台下面的某通道ID时
		//	string key = string(dev->second.addr.id) + string("#") + string(dev_info.addr.id);
		//	auto chl = dev->second.chl_map.find(key);
		//}
        if(QUERY_DEV_INFO == dev_info.cmd_type)         // 查询设备信息
        {
			LOG("INFO","Device [DeviceInfo] '%s:%s'\n\n", dev->second.addr.id, dev->second.addr.ip);
            strcpy(dev->second.name, dev_info.name);
            strcpy(dev->second.manufacturer, dev_info.manufacturer);
            strcpy(dev->second.model, dev_info.model);
            strcpy(dev->second.firmware, dev_info.firmware);
            dev->second.channels = dev_info.chl_map.size();
			//print_device_map(&g_dev_map);
        }
		else if(QUERY_DEV_STATUS == dev_info.cmd_type)      // 查询设备状态
        {
			LOG("INFO","Device [DeviceStatus] '%s:%s'\n\n", dev->second.addr.id, dev->second.addr.ip);
            dev->second.status = dev_info.dev_status->online;
			//print_device_map(&g_dev_map);
        }
		else if (QUERY_DEV_CATALOG == dev_info.cmd_type)    // 查询设备目录
		{
			#ifdef USE_PLATFORM_UNIS
			if(NULL == dev->second.pfm)
				g_callback.query = g_callback.query_catalog;
			#endif
			dev->second.sum_num = dev_info.sum_num;
			dev->second.num += dev_info.num;
			auto chl = dev_info.chl_map.begin();
			for (; chl != dev_info.chl_map.end(); ++chl) {
				auto old_chl = dev->second.chl_map.find(chl->first);
				if (old_chl == dev->second.chl_map.end())
					dev->second.chl_map.insert(map<string, chl_info_t>::value_type(chl->first, chl->second));
				else {
					chl->second.share = old_chl->second.share;// 共享状态不更新
					old_chl->second = chl->second;
				}
			}
			dev->second.channels = dev->second.chl_map.size();

			#ifdef USE_PLATFORM_UNIS			
			if((dev->second.sum_num > 0) && (dev->second.num >= dev->second.sum_num)) {
				if (NULL != g_callback.query) {
					g_callback.query(&dev->second, NULL);
					g_callback.query = NULL;
				}
				else LOG("WARN", "g_callback.query=%p\n\n", g_callback.query);
				dev->second.sum_num = 0;
				dev->second.num = 0;
				//print_channel_info();
			}
			else LOG("WARN","sum_num=%d,num=%d\n\n", dev->second.sum_num, dev->second.num);
			#endif			
        }
        else if(QUERY_DEV_RECORD == dev_info.cmd_type)  // 查询录像
        {
			LOG("INFO","Device [RecordInfo] '%s:%s'\n\n", dev->second.addr.id, dev->second.addr.ip);
        }
        else if(QUERY_DEV_CFG == dev_info.cmd_type)    // 查询设备配置
        {
			LOG("INFO","Device [ConfigDownload] '%s:%s'\n\n", dev->second.addr.id, dev->second.addr.ip);
            dev->second.expires = dev_info.cfg->basic.expires;
            dev->second.keepalive = dev_info.cfg->basic.keepalive;
            dev->second.max_k_timeout = dev_info.cfg->basic.max_k_timeout;
			dev->second.keepalive_flag = dev->second.keepalive * dev->second.max_k_timeout;
			//print_device_map(&g_dev_map);
        }
		else if(NOTIFY_KEEPALIVE == dev_info.cmd_type)    	// 心跳保活
		{
			dev->second.keepalive_flag = dev->second.keepalive * dev->second.max_k_timeout;
			LOG("INFO","Device [Keepalive] '%s:%s' [keepalive:%d(%d) flag=%d]\n\n", dev->second.addr.id, dev->second.addr.ip, dev->second.keepalive,dev->second.max_k_timeout,dev->second.keepalive_flag);
		}
		else if(NOTIFY_MEDIA_STATUS == dev_info.cmd_type)  // 媒体状态
		{
			LOG("INFO","Device [MediaStatus]\n\n");
		}
		else LOG("WARN","Device unknown cmd_type %d\n\n", dev_info.cmd_type);
		dev->second.keepalive_flag = dev->second.keepalive * dev->second.max_k_timeout;

		#ifdef USE_PLATFORM_UNIS
		if ((QUERY_DEV_INFO <= dev_info.cmd_type) 
			&& (dev_info.cmd_type < CTRL_DEV_CTRL) 
			&& (QUERY_DEV_CATALOG != dev_info.cmd_type) 
			&& (NULL != g_callback.query)) {
			g_callback.query(&dev->second, NULL);
			g_callback.query = NULL;
		}
		#endif
		#ifdef USE_PLATFORM_CASCADE
		//if ((NULL != dev->second.pfm) && ((0 < dev_info.cmd_type) && (dev_info.cmd_type < 200)))	// 查询类响应
		if ((NULL != dev->second.pfm) && (NOTIFY_KEEPALIVE != dev_info.cmd_type))
		{	// 转发至平台
			LOG("INFO","Forward platform '%s'\n\n", dev->second.pfm->id);
			g_sip_client.message_request(body, &g_local_cfg.dev.addr, dev->second.pfm, NULL, call_id);
			dev->second.pfm = NULL;
		}
		#endif
    }
    else LOG("WARN","message body=%p\n\n", body);
    return nRet;
}



#ifdef USE_KEEPALIVE_LISTEN
// 设备心跳保活监视线程函数
void* keepalive_proc(void *arg)
{
	LOG("INFO","---------- Device heartbeat thread is running... ----------\n\n");
	while(g_keepalive_thd.flag)
	{
		auto dev = g_dev_map.begin();
		for(; dev != g_dev_map.end(); ++dev)
		{
			//LOG("INFO", "Device '%s' [flag:%d status:%d]\n\n", dev->second.addr.id, dev->second.keepalive_flag, dev->second.status);
			if(dev->second.keepalive_flag > 0) {
				dev->second.keepalive_flag--;
				if(1 != dev->second.status) {
					dev->second.status = 1;	// 状态(0:离线 1:在线)
					auto chl = dev->second.chl_map.begin();
					for (; chl != dev->second.chl_map.end(); chl++) {
						chl->second.status = 1;// 通道状态(必选) (0:OFF 1:ON)
					}
					LOG("INFO","Device '%s:%d' [Online] [status:%d flag:%d]\n\n", dev->second.addr.ip, dev->second.addr.port, dev->second.status,dev->second.keepalive_flag);
					#ifdef USE_PLATFORM_UNIS
					if (NULL != g_callback.keepalive)
						g_callback.keepalive(&dev->second, NULL);
					#endif
				}
			}
			//else if(0 == dev->second.keepalive_flag) {
			else {
				if(1 == dev->second.status) {
					dev->second.status = 0;	// 状态(0:离线 1:在线)
					auto chl = dev->second.chl_map.begin();
					for (; chl != dev->second.chl_map.end(); chl++) {
						chl->second.status = 0;// 设备状态(必选) (0:OFF 1:ON)
					}
					LOG("INFO","Device '%s:%d' [Offline] [status:%d flag:%d]\n\n", dev->second.addr.ip, dev->second.addr.port, dev->second.status,dev->second.keepalive_flag);
					#ifdef USE_PLATFORM_UNIS
					if (NULL != g_callback.keepalive)
						g_callback.keepalive(&dev->second, NULL);
					#endif
				}
			}
		}	
		#ifdef _WIN32
			Sleep(1000);
		#elif __linux__
			sleep(1);
		#endif
	}
	LOG("INFO","---------- Device heartbeat thread exited ----------\n\n");
	osip_thread_exit();
	return NULL;
}
#endif // #ifdef USE_KEEPALIVE_LISTEN


#ifdef USE_PLATFORM_CASCADE
void platform_event_proc(void *event, void *arg)
{
    eXosip_event_t *p_evt = (eXosip_event_t*)event;
	CSip *pSip = (CSip*)arg;
    int nRet = -1;
    switch(p_evt->type)
    {
	#if 0
			case EXOSIP_REGISTRATION_SUCCESS:{	// 注册成功
				g_pfm_cfg.status = 1;	// 注册状态(0:未注册 1:注册成功 2:注册失败 3:注册中)
				LOG("INFO","--------- 注册成功 ---------\n\n");
				pSip->print_event(p_evt->response);
			}break;
			case EXOSIP_REGISTRATION_FAILURE:{	// 注册失败

				if((NULL != p_evt->response) && (401 == p_evt->response->status_code)) {
					LOG("INFO"," --------- 注册401 ---------\n\n");
					pSip->print_event(p_evt->response);
					g_pfm_cfg.status = 3;
					auto pAuth = pSip->get_www_authenticate(p_evt->response, 0);
					nRet = pSip->regist(&g_local_cfg.dev.addr , &g_pfm_cfg, true, p_evt->rid, (NULL!=pAuth)?pAuth->realm:g_pfm_cfg.domain);
				}
				else {
					g_pfm_cfg.status = 2;
					LOG("INFO","--------- 注册失败 [response:%p status_code:%d] ---------\n\n", p_evt->response, p_evt->response?p_evt->response->status_code:-1);
				}
			}break;
	#else
		case EXOSIP_REGISTRATION_SUCCESS:
		case EXOSIP_REGISTRATION_FAILURE: {
			nRet = on_platform_register(p_evt, pSip);
		}break;
	#endif
        case EXOSIP_MESSAGE_NEW: {
			sip_to_t from;
			pSip->get_from(p_evt->request, &from);
			std::string call_id = pSip->get_call_id(p_evt->request);

			LOG("INFO","Platfrom [New message] '%s' [call_id:%s tid:%d]:\n", from.username, call_id.c_str(), p_evt->tid);
			pSip->print_event(p_evt->request);
			if ((NULL != p_evt->request) && (MSG_IS_MESSAGE(p_evt->request))) {
				nRet = pSip->answer(p_evt->tid, 200, call_id.c_str());
				auto pfm = g_pfm_thd_map.find(from.username);
				if (pfm != g_pfm_thd_map.end()) {
					on_platform_message(p_evt->request, call_id.c_str(), pSip, (pfm_info_t*)pfm->second->arg);
				}
				else LOG("WARN","Unknown platform '%s' response.\n\n", from.username);
			}
		}break;
        case EXOSIP_MESSAGE_ANSWERED: {
			sip_to_t to;
    		pSip->get_to(p_evt->request, &to);
			//std::string call_id = pSip->get_call_id(p_evt->response);
			LOG("INFO","Platform '%s' [Message answered] \n\n", to.username);
			pSip->print_event(p_evt->response);
			auto thds = g_pfm_thd_map.find(to.username);
			if (thds != g_pfm_thd_map.end()) {
				if (NULL != thds->second) {
					if (NULL != thds->second->arg) {
						LOG("INFO", "Platform '%s' keepalive_flag=%d\n", to.username, ((pfm_info_t*)thds->second->arg)->keepalive_flag);
						if(((pfm_info_t*)thds->second->arg)->keepalive_flag > 0)
							((pfm_info_t*)thds->second->arg)->keepalive_flag--;
					}
					else LOG("ERROR", "thds->second->arg=%p\n\n", thds->second->arg);
				}
				else LOG("ERROR","thds->second=%p\n\n", thds->second);
			}
			else LOG("ERROR", "Unknown platform '%s'\n\n", to.username);
        }break;
        case EXOSIP_MESSAGE_REQUESTFAILURE: {
			LOG("ERROR","Platform [Message Request Failure] [%d].\n\n", ((NULL != p_evt->response)?p_evt->response->status_code : -1));
			pSip->print_event(p_evt->response);
		}break;
		case EXOSIP_CALL_INVITE: {
			LOG("INFO","Platfrom [Invite]\n\n");
			std::string call_id = pSip->get_call_id(p_evt->request);
			pSip->print_event(p_evt->request);
			on_platform_call_invite(p_evt, pSip, call_id.c_str());
		}break;
		case EXOSIP_CALL_ANSWERED: {	// 设备 -> Server
			LOG("INFO","Platfrom [Call answered]\n\n");
			pSip->print_event(p_evt->response);
        }break;
		case EXOSIP_CALL_ACK: {
			LOG("INFO","Platfrom [Ack]: [ses_size:%d]\n\n", g_ses_map.size());
			pSip->print_event(p_evt->ack);
			std::string call_id = pSip->get_call_id(p_evt->request);
			auto ses = g_ses_map.find(call_id);
			if(ses != g_ses_map.end()) {
				if (ses->second.d_did > 0) {
					LOG("INFO","Forward to device [Ack]\n\n");
					g_gb28181_server.call_ack(ses->second.d_did, call_id.c_str());
				}
			}
			else LOG("WARN","Session does not exist '%s'\n\n", call_id.c_str());
		}break;
		case EXOSIP_CALL_MESSAGE_NEW: {	// 平台 -> 网关 [Bye][回播控制&暂停&快进&进退]
			LOG("INFO","Platfrom [Call message]\n\n");
			nRet = on_platform_call_message(p_evt->request, pSip);
		}break;
		case EXOSIP_CALL_MESSAGE_ANSWERED: {
			LOG("INFO","Platfrom [Call message answered]\n\n");
			pSip->print_event(p_evt->response);
		}break;
		case EXOSIP_CALL_CLOSED: {
			LOG("INFO","Platfrom [Call closed]\n\n");
			pSip->print_event(p_evt->response);
		}break;
		case EXOSIP_CALL_RELEASED: {
			LOG("INFO","Platfrom [Call released]\n\n");
			pSip->print_event(p_evt->response);
		}break;
		case EXOSIP_IN_SUBSCRIPTION_NEW: {
			sip_to_t from;
			pSip->get_from(p_evt->request, &from);
			std::string call_id = pSip->get_call_id(p_evt->request);

			LOG("INFO", "Platfrom [New subscribe] '%s' [call_id:%s tid:%d]:\n", from.username, call_id.c_str(), p_evt->tid);
			pSip->print_event(p_evt->request);
			if ((NULL != p_evt->request) && (MSG_IS_SUBSCRIBE(p_evt->request))) {
				nRet = pSip->subscribe_answer(p_evt->tid, 200, call_id.c_str());
				auto pfm = g_pfm_thd_map.find(from.username);
				if (pfm != g_pfm_thd_map.end()) {
					on_platform_subscribe_notify(p_evt, call_id.c_str(), pSip, (pfm_info_t*)pfm->second->arg);
				}
				else LOG("WARN", "Unknown platform '%s' response.\n\n", from.username);
			}
		}break;
		default: {
			LOG("WARN", "--------- Unknown event [%d] ---------\n", p_evt->type);
			pSip->print_event(p_evt->request);
			pSip->print_event(p_evt->response);
		}break;
    }
}
// [Bye][回播控制&暂停&快进&进退]
int on_platform_call_message(osip_message_t *request, CSip *pSip)
{
	int nRet = -1;
	if ((NULL != request) && (NULL != pSip)) {
		pSip->print_event(request);
		std::string call_id = pSip->get_call_id(request);
		auto ses = g_ses_map.find(call_id);
		if (ses != g_ses_map.end()) {
			if (MSG_IS_BYE(request)) {			// 停播
				if((ses->second.d_cid > 0) && (ses->second.d_did > 0)) {
					LOG("INFO", "Forward to device [Bye]: [call_id:%s ses_size:%d]\n\n", call_id.c_str(), g_ses_map.size() - 1);
					nRet = g_gb28181_server.call_terminate(ses->second.d_cid, ses->second.d_did);
				}
				else {
					g_media.destroy_channel(call_id.c_str());
				}
				g_ses_map.erase(ses);
			}
			else if (MSG_IS_INFO(request)) {	// 回播控制消息
				const char *body_str = pSip->get_body(request);
				if (NULL != body_str) {
					LOG("INFO","Forward device [call_id:%s ses_size:%d]\n\n", call_id.c_str(), g_ses_map.size());
					nRet = g_gb28181_server.call_request(ses->second.d_did, body_str, call_id.c_str());
				}
			}
			else LOG("WRAN", "Unknown call message [method:%s]\n\n", request->sip_method ? request->sip_method : "null");
		}
		else LOG("ERROR","Session does not exist '%s' [ses_size:%d]\n\n", call_id.c_str(), g_ses_map.size());
	}
	else LOG("ERROR","request=%p, pSip=%p\n\n", request, pSip);
	return nRet;
}

int on_platform_register(eXosip_event_t *p_evt, CSip *pSip)
{
	int nRet = -1;
	if ((NULL == p_evt->request) || (NULL == p_evt->request->req_uri) 
		|| (NULL == p_evt->request->req_uri->username)) {
		LOG("ERROR","request:%p,req_uri:%p,username:%p\n\n", p_evt->request, p_evt->request->req_uri, p_evt->request->req_uri->username);
		return nRet;
	}
	pSip->print_event(p_evt->response);

	const char *username = p_evt->request->req_uri->username;
	auto thd = g_pfm_thd_map.find(username);
	if (thd != g_pfm_thd_map.end()) {
		pfm_info_t *pfm = (pfm_info_t*)thd->second->arg;
		int expires = pSip->get_expires(p_evt->request);
		if (EXOSIP_REGISTRATION_SUCCESS == p_evt->type) {
			LOG("INFO","from platform '%s:%s:%s' register response [expires:%d rid:%d]\n\n", username, p_evt->request->req_uri->host, p_evt->request->req_uri->port, expires,p_evt->rid);
			pfm->status = (expires > 0) ? 1 : 3;	// 注册状态(0:未注册 1:注册成功 2:注册失败 3:注销成功 4:注册中)
			nRet = 0;
		}
		else if (EXOSIP_REGISTRATION_FAILURE == p_evt->type) {
			LOG("INFO","from platform '%s:%s:%s' register failed response [expires:%d, status_code:%d]\n", username, p_evt->request->req_uri->host, p_evt->request->req_uri->port, expires, (NULL!=p_evt->response)?p_evt->response->status_code:-1);
			if (NULL != p_evt->response) {
				if (401 == p_evt->response->status_code) {
					pfm->status = 4;
					auto auth = pSip->get_www_authenticate(p_evt->response, 0);
					const char *realm = (NULL != auth) ? auth->realm : pfm->domain;
					if (expires > 0)
						nRet = pSip->regist(&g_local_cfg.dev.addr, pfm, true, p_evt->rid, realm);
					else nRet = pSip->unregist(&g_local_cfg.dev.addr, pfm, true, p_evt->rid, realm);
				}
				else {
					pfm->status = 2;
					nRet = 0;
					LOG("ERROR","register failed %d\n\n", p_evt->response->status_code);
					pSip->register_remove(p_evt->rid);
				}
			}
			else {
				LOG("WARN","Register response=%p\n\n", p_evt->response);
				pSip->register_remove(p_evt->rid);
			}
		}
	}
	else LOG("WARN","Unknown platform '%s' response.\n\n", username);
	if (0 != nRet)
		LOG("ERROR","ret=%d\n\n", nRet);
	return nRet;
}

int on_platform_message(osip_message_t *request, const char *call_id, CSip *pSip, pfm_info_t *pfm)
{
	int nRet = -1;
	if (NULL == request) {
		LOG("ERROR","request=%p\n\n", request);
		return nRet;
	}
	const char *body_xml = pSip->get_body(request);
    if(NULL != body_xml)
    {
		dev_info_t body;
		parse_sip_body(body_xml, &body, NULL);
		pSip->m_sn = body.sn;
		if(0 == strcmp(body.addr.id, g_local_cfg.dev.addr.id)) { // 平台 -> SIP服务
			#if 1
			if (QUERY_DEV_CATALOG == body.cmd_type) {
				auto dev = g_dev_map.begin();
				for (; dev != g_dev_map.end(); dev++) {
					// 是否共享给上级（0:不共享 1:共享） 设备状态(0:离线 1:在线 2:离开[注销])
					if (0 == dev->second.share) {
						LOG("INFO", "Device '%s' share=%d status=%d\n\n", dev->second.addr.id, dev->second.share, dev->second.status);
						continue;
					}
					string key = string(dev->second.parent_id) + string("#") + string(dev->second.addr.id);
					auto iter = g_local_cfg.dev.chl_map.find(key);
					if (iter == g_local_cfg.dev.chl_map.end()) {
						// 将设备信息构造成一个通道
						chl_info_t channel;
						strcpy(channel.id, dev->second.addr.id);
						strcpy(channel.parent_id, dev->second.parent_id);
						strcpy(channel.ip_address, dev->second.addr.ip);
						strcpy(channel.address, dev->second.addr.ip);
						strcpy(channel.manufacturer, dev->second.manufacturer);
						strcpy(channel.model, dev->second.model);
						strcpy(channel.name, dev->second.name);
						channel.port = dev->second.addr.port;
						channel.parental = (dev->second.chl_map.size() > 0);
						channel.share = dev->second.share;
						g_local_cfg.dev.chl_map.insert(map<string, chl_info_t>::value_type(key, channel));
					}
					// 添加设备的通道
					auto chl = dev->second.chl_map.begin();
					for (; chl != dev->second.chl_map.end(); chl++) {
						if(0 == chl->second.share) { // 是否共享给上级（0:不共享 1:共享）
							continue;
						}
						string key = string(chl->second.parent_id) + string("#") + string(chl->second.id);
						auto iter = g_local_cfg.dev.chl_map.find(key);
						if (iter == g_local_cfg.dev.chl_map.end())
							g_local_cfg.dev.chl_map.insert(map<string, chl_info_t>::value_type(key, chl->second));
					}
				}
			}
			else LOG("WARN", "Unknown event type %d\n\n", body.cmd_type);
			#endif
			//print_device_info(&g_local_cfg.dev);
			pSip->message_response(body.cmd_type, &g_local_cfg.dev, &g_local_cfg.dev.addr, &pfm->addr);
			g_local_cfg.dev.chl_map.clear();
		}
		else {	// 平台 -> 设备 (需要转发)
			auto dev = g_dev_map.find(body.addr.id);
			if(dev == g_dev_map.end()) {
				//auto chl = g_local_cfg.dev.chl_map.begin();
				//for(; chl != g_local_cfg.dev.chl_map.end(); ++chl) {
				//	if(0 == strcmp(chl->second.id, body.addr.id)) {
				//		DEBUG("INFO: Channel '%s' -> ParentID '%s'\n\n", chl->second.id, chl->second.parent_id);
				//		dev = g_dev_map.find(chl->second.parent_id);
				//		break;
				//	}
				//}
				bool flag = true;
				dev = g_dev_map.begin();
				for (; (dev != g_dev_map.end()) && flag; dev++) {
					auto chl = dev->second.chl_map.begin();
					for (; (chl != dev->second.chl_map.end()) && flag; chl++) {
						if (0 == strcmp(chl->second.id, body.addr.id)) {
							flag = false;
							break;
						}
					}
					if (false == flag)
						break;
				}
				if(dev == g_dev_map.end()) {
					LOG("INFO","Device does not exist '%s'\n\n", body.addr.id);
					return -1;
				}
			}
			if (QUERY_DEV_RECORD == body.cmd_type) { // 录像查询
				auto rec = body.rec_map.begin();
				if (rec != body.rec_map.end()) {
					//DEBUG("INFO: Query database record '%s'\n\n", body.addr.id);
					const char *dev_id = dev->second.addr.id;
					int nRet = -1;
					if (NULL != g_callback.query_record) {
						nRet = g_callback.query_record(dev_id, &body.rec_map);
						if (nRet < 0)
							body.rec_map.clear();
						print_record_map(&body.rec_map);
					}
					else {
						body.rec_map = dev->second.rec_map;
						//print_record_map(&dev->second.rec_map);
					}
					pSip->message_response(body.cmd_type, &body, &g_local_cfg.dev.addr, &pfm->addr);
				}
			}
			else {
				dev->second.pfm = &pfm->addr;
				// 转发至设备
				LOG("INFO","Forward device '%s'\n\n", body.addr.id);
				g_gb28181_server.message_request(body_xml, &g_local_cfg.addr, &dev->second.addr, NULL, call_id);
			}
		}
		nRet = 0;
    }
	else LOG("INFO","body_xml=%p\n\n", body_xml);
	return nRet;
}

int on_platform_subscribe_notify(eXosip_event_t *p_evt, const char *call_id, CSip *pSip, pfm_info_t *pfm)
{
	if (NULL == p_evt) {
		LOG("ERROR", "p_evt=%p\n\n", p_evt);
		return -1;
	}
	int nRet = 0;
	const char *body_xml = pSip->get_body(p_evt->request);
	if (NULL != body_xml) {
		dev_info_t body;
		parse_sip_body(body_xml, &body, NULL);
		pSip->m_sn = body.sn;
		if (QUERY_DEV_CATALOG == body.cmd_type) {
			auto dev = g_dev_map.begin();
			for (; dev != g_dev_map.end(); dev++) {
				// 是否共享给上级（0:不共享 1:共享） 设备状态(0:离线 1:在线 2:离开[注销])
				if (0 == dev->second.share) {
					LOG("INFO", "Device '%s' share=%d status=%d\n\n", dev->second.addr.id, dev->second.share, dev->second.status);
					continue;
				}
				string key = string(dev->second.parent_id) + string("#") + string(dev->second.addr.id);
				auto iter = g_local_cfg.dev.chl_map.find(key);
				if (iter == g_local_cfg.dev.chl_map.end()) {
					// 将设备信息构造成一个通道
					chl_info_t channel;
					strcpy(channel.id, dev->second.addr.id);
					strcpy(channel.parent_id, dev->second.parent_id);
					strcpy(channel.ip_address, dev->second.addr.ip);
					strcpy(channel.address, dev->second.addr.ip);
					strcpy(channel.manufacturer, dev->second.manufacturer);
					strcpy(channel.model, dev->second.model);
					strcpy(channel.name, dev->second.name);
					channel.port = dev->second.addr.port;
					channel.parental = (dev->second.chl_map.size() > 0);
					channel.share = dev->second.share;
					g_local_cfg.dev.chl_map.insert(map<string, chl_info_t>::value_type(key, channel));
				}
				// 添加设备的通道
				auto chl = dev->second.chl_map.begin();
				for (; chl != dev->second.chl_map.end(); chl++) {
					if (0 == chl->second.share) { // 是否共享给上级（0:不共享 1:共享）
						continue;
					}
					string key = string(chl->second.parent_id) + string("#") + string(chl->second.id);
					auto iter = g_local_cfg.dev.chl_map.find(key);
					if (iter == g_local_cfg.dev.chl_map.end())
						g_local_cfg.dev.chl_map.insert(map<string, chl_info_t>::value_type(key, chl->second));
				}
			}
		}
		else LOG("WARN", "Unknown event type %d\n\n", body.cmd_type);
		pSip->notify_response(body.cmd_type,&g_local_cfg.dev, p_evt->did, 200, 0, call_id);
		g_local_cfg.dev.chl_map.clear();
	}
	else LOG("INFO", "body_xml=%p\n\n", body_xml);
	return nRet;
}
int on_platform_call_invite(eXosip_event_t *p_evt, CSip *pSip, const char *call_id)
{
	int nRet = -1;
	addr_info_t to;
	auto dev = g_dev_map.find(p_evt->request->req_uri->username);
	if(dev != g_dev_map.end())
		memcpy(&to, &dev->second.addr, sizeof(to));
	else {
		bool flag = true;
		dev = g_dev_map.begin();
		for (; (dev != g_dev_map.end())&&flag; dev++) {
			auto chl = dev->second.chl_map.begin();
			for (; (chl != dev->second.chl_map.end()) && flag; chl++) {
				if (0 == strcmp(chl->second.id, p_evt->request->req_uri->username)) {
					memcpy(&to, &dev->second.addr, sizeof(to));
					flag = false;
					break;
				}
			}
			if (false == flag)
				break;
		}
	}
	if((strlen(to.id) > 0) && (strlen(to.ip) > 0)) {
		session_t ses;
		strcpy(ses.call_id, call_id);
		ses.p_tid = p_evt->tid;
		ses.p_cid = p_evt->cid;
		ses.p_did = p_evt->did;
		string sdp_str;
		sdp_info_t sdp;
		pSip->get_remote_sdp(p_evt->did, sdp_str, &sdp);
		if (0 == strcmp("Play", sdp.s)) {
			operation_t opt;
			opt.cmd = CTRL_PLAY_START;
			opt.body = sdp_str.c_str();
			opt.call_id = call_id;
			LOG("INFO","Forward device '%s' [ses_size:%d]\n\n", to.id, g_ses_map.size());
			nRet = g_gb28181_server.call_invite(&g_local_cfg.addr, &to, &opt);
			if (0 == nRet)
				g_ses_map.add(ses.call_id, &ses);
			else LOG("ERROR","call_invite failed %s:%s\n", to.id, to.ip);
		}
		else if (0 == strcmp("Playback", sdp.s)) {
			record_info_t rec;
			strcpy(rec.id, to.id); // 通道ID
			string start_time = pSip->timestamp_to_string(sdp.t_start_time);
			string end_time = pSip->timestamp_to_string(sdp.t_end_time);
			strcpy(rec.start_time, start_time.c_str());
			strcpy(rec.end_time, end_time.c_str());

			map<string, record_info_t> rec_map;
			rec_map.insert(map<string, record_info_t>::value_type(rec.start_time, rec));
			int nRet = 0;
			if (NULL != g_callback.query_record) {
				nRet = g_callback.query_record(dev->second.addr.id, &rec_map);
				if (nRet > 0) {
					auto iter = rec_map.begin();
					if (iter != rec_map.end()) {
						memcpy(&rec, &iter->second, sizeof(record_info_t));
					}
				}
				else LOG("WARN","Query device '%s' record size %d\n\n", dev->second.addr.id, nRet);
			}
			else {
				// 测试
				auto iter = dev->second.rec_map.begin();
				for (; iter != dev->second.rec_map.end(); iter++) {
					time_t start_time = pSip->string_to_timestamp(iter->second.start_time);
					time_t end_time = pSip->string_to_timestamp(iter->second.end_time);
					if ((sdp.t_start_time == start_time) && (sdp.t_end_time == end_time)) {
						memcpy(&rec, &iter->second, sizeof(record_info_t));
						break;
					}
				}
				nRet = dev->second.rec_map.size();
				if(nRet <= 0)
					LOG("WARN","Device '%s' record size %d\n\n", dev->first.c_str(), dev->second.rec_map.size());
			}
			if (nRet > 0) {
				// 拿到指定录像文件后,创建线程发送视频流
				LOG("INFO","-------- Playback %s '%s' %s~%s --------\n\n", rec.id, rec.file_path, rec.start_time, rec.end_time);
				// ..................................
				udp_addr_t addr;
				int port = g_media.alloc_port();
				if (port > 0) {
					auto m = sdp.m.find("video");
					if (m != sdp.m.end()) {
						strcpy(addr.ip, g_local_cfg.addr.ip);
						addr.port = port;
						strcpy(addr.peer_ip, m->second.c.addr);
						addr.peer_port = m->second.port;
						//DEBUG("INFO: ----------- Platform media %s:%d\n\n", m->second.c.addr, m->second.port);
					}
				}
				g_ses_map.add(ses.call_id, &ses);

				//////////////////////////////////////////////////////////
				// 生成新的SDP
				string new_sdp = "";
				memset(sdp.o_username, 0, sizeof(sdp.o_username));
				strncpy(sdp.o_username, dev->second.parent_id, sizeof(sdp.o_username)-1);
				memset(sdp.o_addr, 0, sizeof(sdp.o_addr));
				strncpy(sdp.o_addr, g_local_cfg.addr.ip, sizeof(sdp.o_addr)-1);
				auto m = sdp.m.find("video");
				if (m != sdp.m.end()) {
					m->second.port = port;
					strncpy(m->second.c.addr, g_local_cfg.addr.ip, sizeof(m->second.c.addr)-1);
					auto payloads = m->second.payloads.begin();
					for (; payloads != m->second.payloads.end();) {
						if (96 != *payloads)
							payloads = m->second.payloads.erase(payloads);
						else payloads++;
					}
					auto a = m->second.a.begin();
					for (; a != m->second.a.end(); ) {
						if ((0 == strcmp(a->field, "rtpmap")) && (0 != strcmp(a->value, "96 PS/90000")))
							a = m->second.a.erase(a);
						else a++;
					}
				}
				pSip->make_sdp(&sdp, new_sdp);
				//////////////////////////////////////////////////////////

				g_sip_client.call_answer(p_evt->tid, 200, new_sdp.c_str(), call_id);
				sip_from_t from;
				pSip->get_from(p_evt->request, &from);
				memset(from.tag, 0, sizeof(from.tag));
				strcpy(from.tag, dev->first.c_str());
				nRet = g_media.create_channel(ses.call_id, rec.file_path, &addr, on_record_playbak, &from, sizeof(from));
			}
		}
		else LOG("ERROR","session name '%s'\n\n", sdp.s);
	}
	else LOG("INFO","Device does not exist '%s'\n\n", p_evt->request->req_uri->username);
	return nRet;
}

int STDCALL on_record_playbak(void *pData, int nLen, void *pUserData)
{
	sip_from_t *pfm = (sip_from_t*)pUserData;
	addr_info_t to;
	strcpy(to.id, pfm->username);
	strcpy(to.ip, pfm->ip);
	to.port = pfm->port;
	g_sip_client.call_request(pfm->tag, &g_local_cfg.dev.addr, &to);
	return 0;
}

// 平台级联线程函数
void* platform_register_proc(void *arg)
{
	thd_handle_t *thd = (thd_handle_t*)arg;
	pfm_info_t *pfm = (pfm_info_t*)thd->arg;
	LOG("INFO","Platform cascade thread is running '%s:%s:%d'\n\n", pfm->addr.id, pfm->addr.ip, pfm->addr.port);
	//print_platform_info(pfm);
	bool is_unregister = false;
	int nRet = 0;
	int expires = pfm->expires, keepalive = pfm->keepalive;
	pfm->status = 0;
	time_t register_time = 0;
	while(thd->flag) {
		//LOG("INFO","enable=%d, status=%d, expires=%d(%d), keepalive=%d(%d)\n\n", pfm->enable, pfm->status, expires, pfm->expires, keepalive,pfm->keepalive);
		if(0 != pfm->enable) {	// 注册
			if(expires < pfm->expires) {
				expires++;
				if(expires == pfm->expires)
					pfm->status = 0;	// 注册过期
			}
			else {
				// 注册状态(0:未注册 1:注册成功 2:注册失败 3:注销成功 4:注册中)
				if((0 == pfm->status) || (3 == pfm->status)) {
					pfm->status = 4;
					register_time = time(NULL);
					addr_info_t from;
					strcpy(from.id, pfm->username);
					strcpy(from.ip, g_local_cfg.dev.addr.ip);
					from.port = g_local_cfg.dev.addr.port;
					nRet = g_sip_client.regist(&from, pfm, false,0, g_local_cfg.domain,NULL, pfm->cseq);
					if (0 != nRet)
						LOG("ERROR","Register failed %d\n\n", nRet);
				}
				else if(1 == pfm->status) {
					expires = 0;
					keepalive = 0;
				}
				else if (2 == pfm->status) {
					expires = (expires > pfm->register_interval) ? (expires - pfm->register_interval) : 0;
				}
				else if(4 == pfm->status) {
					//DEBUG("INFO: enable=%d, status=%d, expires=%d(%d), keepalive=%d(%d)\n\n", pfm->enable, pfm->status, expires, pfm->expires, keepalive, pfm->keepalive);
					if ((time(NULL) - register_time) >= pfm->register_interval) {
						LOG("INFO","register_time:%d\n\n", time(NULL) - register_time);
						pfm->status = 0;
					}
				}
				//else DEBUG("register status %d\n\n", g_pfm_cfg.status);
			}
			if (1 == pfm->status) {	// 心跳保活
				if (keepalive < pfm->keepalive)
					keepalive++;
				if (keepalive >= pfm->keepalive) {
					keepalive = 0;
					nRet = g_sip_client.keepalive(&g_local_cfg.dev.addr, &pfm->addr);
					if (0 == nRet)
						pfm->keepalive_flag++;
					else
						LOG("ERROR","Send heartbeat packets failed %d\n\n", nRet);
				}	
			}
		}
		else {	// 注销
			expires = pfm->expires;
			if (1 == pfm->status) {		// 注册状态(0:未注册 1:注册成功 2:注册失败 3:注销成功 4:注册中)
				pfm->status = 4;
				nRet = g_sip_client.unregist(&g_local_cfg.dev.addr, pfm, false);
			}
			else if (2 == pfm->status) {
				pfm->status = 3;
			}
			else if (3 == pfm->status) {
			}
		}
		//DEBUG("enable:%d expires:%d status:%d \n", g_pfm_cfg.enable, expires, g_pfm_cfg.status);
		#ifdef _WIN32
		Sleep(1000);
		#elif __linux__
		sleep(1);
		#endif
	}	// while()
	LOG("INFO","flag=%d,enable=%d,status=%d,expires=%d(%d),keepalive=%d(%d)\n\n", thd->flag,pfm->enable,pfm->status,expires,pfm->expires,keepalive,pfm->keepalive);
	if (1 == pfm->status) {	// 注册状态(0:未注册 1:注册成功 2:注册失败 3:注销成功 4:注册中)
		while (3 != pfm->status) {
			if (1 == pfm->status) {
				pfm->status = 4;
				nRet = g_sip_client.unregist(&g_local_cfg.dev.addr, pfm, false);
				if (0 != nRet)
					LOG("ERROR","Unregister failed %d\n\n", nRet);
			}
			else if (2 == pfm->status) {
				pfm->status = 1;
			}
			#ifdef _WIN32
			Sleep(500);
			#elif __linux__
			usleep(500000);
			#endif
		}
	}
	LOG("INFO","Platform cascade thread has exited '%s'\n\n", pfm->addr.id);
	osip_thread_exit();		// 此函数执行后 线程函数立即退出
	return NULL;
}

int add_platform(pfm_info_t *pfm)
{
	int nRet = -1;
	if (NULL != pfm) {
		auto iter = g_pfm_thd_map.find(pfm->addr.id);
		if (iter == g_pfm_thd_map.end()) {
			thd_handle_t *thd = new thd_handle_t();
			thd->arg = malloc(sizeof(pfm_info_t));
			memcpy(thd->arg, pfm, sizeof(pfm_info_t));
			thd->flag = true;
			thd->hd = osip_thread_create(20000, platform_register_proc, thd);
			g_pfm_thd_map.add(pfm->addr.id, &thd);
			nRet = 0;
			//LOG("INFO","Platform '%s' add successfully ^-^ [size:%d]\n\n", pfm->addr.id, g_pfm_thd_map.size());
		}
		else LOG("ERROR","Platform '%s' exists, could not add\n\n", pfm->addr.id);
	}
	else LOG("ERROR","pfm = %p\n\n", pfm);
	return nRet;
}

int del_platform(const char *id)
{
	int nRet = -1;
	if (NULL != id) {
		auto iter = g_pfm_thd_map.find(id);
		if (iter != g_pfm_thd_map.end()) {
			if (NULL != iter->second) {
				iter->second->flag = false;
				if (NULL != iter->second->hd ) {
					osip_thread_join(iter->second->hd);
					osip_free(iter->second->hd);
					iter->second->hd = NULL;
				}
				if (NULL != iter->second->arg) {
					free(iter->second->arg);
					iter->second->arg = NULL;
				}
				delete iter->second;
			}
			g_pfm_thd_map.erase(iter);
			nRet = 0;
			LOG("INFO","Platform '%s' deleted successfully ^-^ [size:%d]\n\n", id, g_pfm_thd_map.size());
		}
		else LOG("ERROR","Platform '%s' does not exist,cannot be deleted.\n\n", id);
	}
	else LOG("ERROR","id = %p\n\n", id);
	return nRet;
}

void del_platforms()
{
	auto pfm = g_pfm_thd_map.begin();
	for (; pfm != g_pfm_thd_map.end(); )
	{
		if (NULL != pfm->second) {
			pfm->second->flag = false;
			if (NULL != pfm->second->hd) {
				osip_thread_join(pfm->second->hd);
				osip_free(pfm->second->hd);
				pfm->second->hd = NULL;
			}
			if (NULL != pfm->second->arg) {
				free(pfm->second->arg);
				pfm->second->arg = NULL;
			}
			delete pfm->second;
		}
		LOG("INFO","Platform '%s' deleted successfully ^-^ [size:%d]\n\n", pfm->first.c_str(), g_pfm_thd_map.size() - 1);
		pfm = g_pfm_thd_map.erase(pfm);
	}
	g_pfm_thd_map.clear();
}

int update_platform(pfm_info_t *pfm)
{
	int nRet = -1;
	if (NULL != pfm) {
		auto iter = g_pfm_thd_map.find(pfm->addr.id);
		if (iter != g_pfm_thd_map.end()) {
			if (NULL != iter->second) {
				LOG("INFO","Before the update:\n");
				print_platform_info((pfm_info_t*)iter->second->arg);
				char status = ((pfm_info_t*)iter->second->arg)->status;
				memcpy(iter->second->arg, pfm, sizeof(pfm_info_t));
				((pfm_info_t*)iter->second->arg)->status = status;
				LOG("INFO","After the update:\n");
				print_platform_info((pfm_info_t*)iter->second->arg);
				nRet = 0;
			}
		}
		else LOG("ERROR","Platform '%s' does not exist,cannot be update.\n\n", pfm->addr.id);
	}
	else LOG("ERROR","pfm = %p\n\n", pfm);
	return nRet;
}
#endif // #ifdef USE_PLATFORM_CASCADE



























////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////



#ifdef USE_COMPILE_EXE
int do_main(int autom/*=0*/)
{
	int nRet = 0;
	srand(time(NULL));
	char call_id[64] = { "1626563477" };
	//const char *dev_id = "34020000001320000003";
	//const char *dev_id = "41020200001310051971";
	//const char *dev_id = "41020200001310144975";
	//const char *dev_id = "41020200001310605876";
	//const char *dev_id = "41020200001310627184";
	//const char *dev_id = "41020200001310722368";
	char dev_id[64] = {0};
	dev_info_t dev;
	static int n = 0;
	auto iter = g_dev_map.begin();
	if (iter != g_dev_map.end()) {
		#if 0
			strcpy(dev.addr.id, iter->second.addr.id);
			strcpy(dev.addr.ip, iter->second.addr.ip);
			dev.addr.port = iter->second.addr.port;
			//dev_id = dev.addr.id;
		#else
			#ifdef _WIN32
				Sleep(2000);
			#elif __linux__
				sleep(2);
			#endif
			auto chl = iter->second.chl_map.begin();
			if (chl != iter->second.chl_map.end()) {
				strcpy(dev.addr.id, chl->second.id);
				strcpy(dev.addr.ip, chl->second.ip_address);
				dev.addr.port = chl->second.port;
			}
		#endif
	}
	else {
		if(n++ < 5)
			LOG("WARN","no device\n");
		#ifdef _WIN32
		Sleep(1000);
		#elif __linux__
		sleep(1);
		#endif
		return -1;
	}
	n = 0;

#if 0
	printf("Enter the device id >:");
	scanf("%s", dev_id);
	if (strlen(dev_id) <= 0)
		strcpy(dev_id, "41020200001310722368");
	strcpy(dev.addr.id, dev_id);
#else
	strcpy(dev_id, dev.addr.id);
#endif

	#ifdef USE_PLATFORM_CASCADE
	pfm_info_t pfm;
	auto iter_pfm = g_pfm_thd_map.begin();
	if(iter_pfm != g_pfm_thd_map.end()) {
		memcpy(&pfm, iter_pfm->second->arg, sizeof(pfm_info_t));
	}
	else {
		strcpy(pfm.addr.id, "34020000002000000008");
		strcpy(pfm.addr.ip, "192.168.1.100");
		pfm.addr.port = 5070;
		strcpy(pfm.domain, "3402000000");
		strcpy(pfm.platform, "GB/T28181-2016");
		strcpy(pfm.username, g_local_cfg.addr.id);
		strcpy(pfm.password, "123456");
		pfm.expires = 30;
		pfm.register_interval = 10;
		pfm.keepalive = 5;
		pfm.enable = 0;
	}
	#endif

	char ch = 0;
	while(ch != 'e') {
		LOG("INFO", "Please input command >: ");
		if(0 == autom) 	
			ch = get_char();
		else {				// 自动化运行程序
			ch = 97 + (rand() % 26);
			while('e' == ch)
				ch = 97 + (rand() % 26);
		}
		LOG("INFO", "\n");
		if (g_dev_map.size() == 0) {
			memset(&dev.addr, 0, sizeof(dev.addr));
			return -1;
		}
		//printf("Selected command: '%c'\n", ch);
		operation_t opt;
		switch(ch){
			case 'a': {// 查询设备信息
				opt.cmd = QUERY_DEV_INFO;
				nRet = g_gb28181_server.query_device(&g_local_cfg.addr, &dev.addr, &opt, dev_id);
			}break;
			case 'b': {// 查询设备目录信息
				opt.cmd = QUERY_DEV_CATALOG;
				nRet = g_gb28181_server.query_device(&g_local_cfg.addr, &dev.addr, &opt, dev_id);
			}break;
			case 'c': {// 查询设备状态
				opt.cmd = QUERY_DEV_STATUS;
				nRet = g_gb28181_server.query_device(&g_local_cfg.addr, &dev.addr, &opt, dev_id);
			}break;
			case 'd': {// 查询录像
				opt.cmd = QUERY_DEV_RECORD;
				record_info_t rec;
				rec.type = 1;   // 录像类型(1:all 2:manual(手动录像) 3:time 4:alarm )
				strcpy(rec.start_time, "2021-06-21T14:50:12");
				strcpy(rec.end_time, "2021-06-22T14:50:12");
				opt.record = &rec;
				nRet = g_gb28181_server.query_device(&g_local_cfg.addr, &dev.addr, &opt);
			}break;
			case 'e': {
				return -10;
			}break;
			case 'f': {// 查询配置
				opt.cmd = QUERY_DEV_CFG;
				nRet = g_gb28181_server.query_device(&g_local_cfg.addr, &dev.addr, &opt);
			}break;
			case 'g': {// 点播
				opt.cmd = CTRL_PLAY_START;
				strcpy(opt.media.ip, g_local_cfg.media.ip);
				opt.media.port = g_local_cfg.media.port;
				//sprintf(call_id, "%lld", time(NULL));
				char call_id[64] = {0};
				sprintf(call_id, "%s", g_gb28181_server.random_string(36).c_str());
				opt.call_id = call_id;
				nRet = g_gb28181_server.call_invite(&g_local_cfg.addr, &dev.addr, &opt, dev_id);
				if (0 == nRet) {
					session_t ses;
					sprintf(ses.call_id, "%s", opt.call_id);
					g_ses_map.add(ses.call_id, &ses);
					LOG("INFO","Invite successful ^_^ [id:%s size:%d]\n\n", ses.call_id, g_ses_map.size());
				}
				else LOG("ERROR","Invite failed %s:%s\n\n\n\n", dev.addr.id, dev.addr.ip);
            }break;
			case 'h': {// 停点
				//auto ses = g_ses_map.find(call_id);
				auto ses = g_ses_map.begin();
				if (ses != g_ses_map.end()) {
					char call_id[32] = {0};
					sprintf(call_id, "%s", ses->first.c_str());
					nRet = g_gb28181_server.call_terminate(ses->second.d_cid, ses->second.d_did);
					if(0 == nRet) {
						g_ses_map.erase(ses);
						LOG("INFO","Terminate successful ^_^ [id:%s size:%d]\n\n", call_id, g_ses_map.size());
					}
					else LOG("ERROR","Terminate failed [id:%s size:%d]\n\n\n\n", call_id, g_ses_map.size());
				}
				else LOG("WARN","no call invite [size:%d]\n\n\n\n", g_ses_map.size());
				
            }break;
			case 'i': {	// 回放
				//opt.cmd = CTRL_PLAY_START; // CTRL_PLAYBAK_START;
				record_info_t rec;
				strcpy(rec.start_time, "2021-06-21T14:50:12");
				strcpy(rec.end_time, "2021-06-22T14:50:12");
				opt.record = &rec;
				addr_info_t ivt;
				strcpy(ivt.id, dev.addr.id);
				strcpy(ivt.ip, dev.addr.ip);
				ivt.port = dev.addr.port;				
				//nRet = g_gb28181_server.Invite(&ivt, &opt);
			}break;
			case 'j': {	// 停止回放
				//nRet = g_gb28181_server.Terminate(0);
			}break;
			case 'k': {// 手动录像
				opt.cmd = CTRL_REC_START;
				nRet = g_gb28181_server.control_device(&g_local_cfg.addr, &dev.addr, &opt);
			}break;
			case 'l': {// 停止录像
				opt.cmd = CTRL_REC_STOP;
				nRet = g_gb28181_server.control_device(&g_local_cfg.addr, &dev.addr, &opt);
			}break;
			case 'm': {
				LOG("INFO","PTZ Control: 0.Stop 1.Up 2.Down 3.Left 4.Right 5.Large 6.Small e.exit\n");
				LOG("INFO",">:");
				if (0 == autom)
					ch = get_char();
				else {				// 自动化运行程序
					ch = '0' + (rand() % 10);
					while ('e' == ch)
						ch = 97 + (rand() % 26);
				}
				LOG("INFO", "\n");
				switch (ch) 
				{
					case '0': opt.cmd = CTRL_PTZ_STOP; break;
					case '1': opt.cmd = CTRL_PTZ_UP; break;
					case '2': opt.cmd = CTRL_PTZ_DOWN; break;
					case '3': opt.cmd = CTRL_PTZ_LEFT; break;
					case '4': opt.cmd = CTRL_PTZ_RIGHT; break;
					case '5': opt.cmd = CTRL_PTZ_LARGE; break;
					case '6': opt.cmd = CTRL_PTZ_SMALL; break;
					case 'e': {
						return -10;
					}break;
					default : LOG("ERROR","Invalid command '%c'\n\n", ch);
				}
				if (UNKNOWN_CMD != opt.cmd)
					nRet = g_gb28181_server.control_device(&g_local_cfg.addr, &dev.addr, &opt);
			}break;
			#ifdef USE_PLATFORM_CASCADE
			case 'n': {// 平台级联
				LOG("INFO","[Platform]: 1.add platform  2.del platform  3.enable platform  4.disable platform  e.exit\n");
				if (0 == autom)
					ch = get_char();
				else {				// 自动化运行程序
					ch = '0' + (rand() % 10);
					while ('e' == ch)
						ch = 97 + (rand() % 26);
				}
				LOG("INFO", "\n");
				switch (ch) {
					case '1': {
						pfm.enable = 0;
						nRet = add_platform(&pfm);
					} break;
					case '2': {
						const char *id = pfm.addr.id;
						nRet = del_platform(id);
					} break;
					case '3': {
						pfm.enable = 1;
						nRet = update_platform(&pfm);
					} break;
					case '4': {
						pfm.enable = 0;
						nRet = update_platform(&pfm);
					} break;
					case 'e': {
						return -10;
					}break;
					default: LOG("ERROR","Invalid command '%c'\n\n", ch);
				}
			}break;
			#endif
			case 'o': {
				LOG("INFO","Enter the device id >:");
				scanf("%s", dev_id);
				strcpy(dev.addr.id, dev_id);
				LOG("INFO", "\n");
			}break;
			#ifdef USE_DEVICE_ACCESS
			case 'p': {
				LOG("INFO","1.add access  2.delete access  3.print access 4.exit\n\n");
				ch = get_char();
				if (('1' <= ch) && (ch <= '4')) {
					access_info_t acc;
					LOG("INFO", "Enter the access device id >:");
					scanf("%s", acc.addr.id);
					LOG("INFO", "\n");
					if ('1' == ch) {
						LOG("INFO", "*1.share device *2.not share device  *e.exit\n\n");
						LOG("INFO", ">:");
						ch = get_char();
						LOG("INFO", "\n");
						switch (ch) {
						case '1': acc.share = 1; break;
						case '2': acc.share = 0; break;
						case 'e': {
							return -10;
						};
						}
						strcpy(acc.realm, g_local_cfg.domain);
						strcpy(acc.password, "123456");
						g_access_map.add(acc.addr.id, &acc);
						LOG("INFO","access map size %d\n\n", g_access_map.size());
					}
					else if ('2' == ch) {
						g_access_map.del(acc.addr.id);
						LOG("INFO", "access map size %d\n\n", g_access_map.size());
					}
					else if ('3' == ch) {
						print_access_map(&g_access_map);
					}
					else if ('4' == ch) {
						return -10;
					}
				}
				else LOG("ERROR", "Invalid command '%c'\n\n", ch);
			}break;
			#endif
			case 'q': { LOG("ERROR","Invalid command '%c'\n\n", ch);
			}break;
			case 'r': { LOG("ERROR","Invalid command '%c'\n\n", ch);
			}break;
			case 's': {
			}break;
			case 't': {// 强制关键帧
				opt.cmd = CTRL_IFR_SEND;
				nRet = g_gb28181_server.control_device(&g_local_cfg.addr, &dev.addr, &opt);
            }break;
			case 'u': {// 布防
				opt.cmd = CTRL_GUD_START;
				nRet = g_gb28181_server.control_device(&g_local_cfg.addr, &dev.addr, &opt);
            }break;
			case 'v': {// 撤防
				opt.cmd = CTRL_GUD_STOP;
				nRet = g_gb28181_server.control_device(&g_local_cfg.addr, &dev.addr, &opt);
            }break;
			case 'w': {// 报警复位
				opt.cmd = CTRL_ALM_RESET;
				nRet = g_gb28181_server.control_device(&g_local_cfg.addr, &dev.addr, &opt);
            }break;
			case 'x': {// 设备配置
				opt.cmd = CTRL_DEV_CFG;
				basic_param_t param;
				strcpy(param.name, "Camera Test");
				param.expires = 120;
				param.keepalive = 60;
				param.max_k_timeout = 5;
				opt.basic_param = &param;
				nRet = g_gb28181_server.control_device(&g_local_cfg.addr, &dev.addr, &opt);
			}break;
			case 'y': {// 设备重启
				opt.cmd = CTRL_TEL_BOOT;
				if(0 == autom)
				nRet = g_gb28181_server.control_device(&g_local_cfg.addr, &dev.addr, &opt);
			}break;
			case 'z': {	// 回播控制
				//opt.cmd = CTRL_PLAYBAK_CTRL;
				LOG("INFO", "回播控制: 1.暂停 2.播放 3.快进 4.慢进 5.停止\n");
                #if 0
				playback_ctrl_t pctrl;
				char ctrl_code = 0;
				if(0 == autom) 
					ctrl_code = get_char();
				else ctrl_code = 49 + (rand() % 5);
				if('1' == ctrl_code) {
					printf("已选择: 1.暂停\n");
					pctrl.opt = playback_ctrl_t::CTRL_PAUSE;
					strcpy(pctrl.cmd, "Pause");			// 控制命令 (取值 "Play", "Pause", "Teardown")
					strcpy(pctrl.pause_time, "now");	// 录像暂停在当前位置
				}
				else if('2' == ctrl_code) {
					printf("已选择: 2.播放\n");
					pctrl.opt = playback_ctrl_t::CTRL_PLAY;
					strcpy(pctrl.cmd, "Play");
					pctrl.range = -1;					// 表示表示从暂停位置以原倍速恢复播放
				}
				else if('3' == ctrl_code) {
					printf("已选择: 3.快进\n");
					pctrl.opt = playback_ctrl::CTRL_FF;
					strcpy(pctrl.cmd, "Play");
					pctrl.scale = 2.0;					// 快进
				}
				else if('4' == ctrl_code) {
					printf("已选择: 4.慢进\n");
					strcpy(pctrl.cmd, "Play");
					pctrl.opt = playback_ctrl::CTRL_SLOW;
					pctrl.scale = 0.5;					// 慢进
				}
				else if('5' == ctrl_code) {
					printf("已选择: 5.停止\n");
					strcpy(pctrl.cmd, "Teardown");
					pctrl.opt = playback_ctrl::CTRL_TEAR;
				}
				opt.pctrl = &pctrl;
				nRet = g_gb28181_server.control_device(&g_local_cfg.addr, &dev.addr, &opt);
                #endif
			}break;
			case '1': {
				print_device_map(&g_dev_map);
			}break;
			case '2': {
				print_channel_map(&g_local_cfg.dev.chl_map);
			}break;
			case '3': {
				auto thd = g_pfm_thd_map.begin();
				for (; thd != g_pfm_thd_map.end(); thd++) {
					print_platform_info((pfm_info_t*)thd->second->arg);
				}
			}break;
			case '4': {
				pfm.enable = 1;
				nRet = add_platform(&pfm);
			} break;
			case '5': {
				const char *id = pfm.addr.id;
				nRet = del_platform(id);
			} break;
			case '6': {
				pfm.enable = 1;
				nRet = update_platform(&pfm);
			} break;
			case '7': {
				pfm.enable = 0;
				nRet = update_platform(&pfm);
			} break;
			default : {
				nRet = -1;
				//LOG("ERROR","Invalid command '%c'\n\n", ch);
			}
		}
		//if(0 != nRet)
		//	LOG("ERROR","error code: %d\n", nRet);
	}
	return nRet;
}
#endif