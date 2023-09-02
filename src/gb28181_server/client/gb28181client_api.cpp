
#include "gb28181client_api.h"
#include "udp_client.h"
#include "log.h"

#include "media.h"
#include <sstream>

#ifdef _WIN32
#include <io.h>				//_access() 函数
#elif __linux__
#include<unistd.h>			// access()函数
#endif

FILE *g_log_file = fopen("gb28181client.log", "w");

CUdpClient  g_udp_client;
bool g_is_init = false;
local_cfg_t g_local_cfg;
char		g_udp_ser_ip[24] = { 0 };// gb28181server.dll模块UDP的IP
int			g_udp_ser_port = 0;		// gb28181server.dll模块UDP的Port
char		g_media_ip[24] = {0};	// 本地的Media的IP
int			g_media_port = 0;		// 本地的Media的端口 [随机自动分配]
CMedia	    g_media(8500, 500);

int init()
{
	int nRet = g_udp_client.Open(NULL, 0);
	if (0 == nRet) {
		char ip[24] = { 0 };
		int port = 0;
		g_udp_client.GetAddress(ip, &port);
		LOG("INFO", "UDP client opened successfully ^-^ '%s:%d'\n\n", ip, port);
		//g_udp_client.SetCallback(on_udp_receive, NULL);
	}
	else LOG("ERROR", "UDP client opened failed.\n\n");
	return nRet;
}

void uninit()
{
	g_udp_client.Close();
	g_is_init = false;
    return ;
}

int play(const char *id, udp_msg_t *msg, const char *url, MEDIA_CBK cbk, MEDIA_CBK mp4_cbk, void *arg/*=NULL*/, int arg_size/*=0*/)
{
	int nRet = -1;
	if (NULL != id) {
		if (NULL != msg) {
			// 点播 或 录像回放(设备端)
			int port = (g_media_port > 0) ? g_media_port : g_media.alloc_port();
			if (port > 0) {
				static int msg_size = sizeof(udp_msg_t);
				strcpy(msg->media_ip, g_media_ip);
				msg->media_port = port;
				nRet = (msg_size == g_udp_client.Send(msg, msg_size, g_udp_ser_ip, g_udp_ser_port)) ? 0 : -1;
				if (0 == nRet) {
					nRet = (0 <= g_media.create_channel(id, g_media_ip, port, cbk, mp4_cbk, arg, arg_size)) ? 0 : -1;
					if (0 != nRet)
						LOG("ERROR","Create channel failed [id:%s ip:%s port:%d]\n\n", id, g_media_ip, port);
				}
				else LOG("INFO","UDP client send message to '%s:%d' failed %d ^-^\n\n", g_udp_ser_ip, g_udp_ser_port, nRet);
				if (0 != nRet) {
					g_media.free_port(port);
					LOG("ERROR","UDP client send message to '%s:%d' failed %d\n\n", g_udp_ser_ip, g_udp_ser_port, nRet);
				}
			}
			else LOG("ERROR","alloc_port failed %d\n\n", port);
		}
		else if (NULL != url) {
			// 录像回放(本地录像)
			nRet = g_media.create_channel(id, url, cbk, mp4_cbk, arg, arg_size);
			if (nRet < 0)
				LOG("ERROR","Create channel failed [url:%s]\n\n", url);
		}
		else LOG("ERROR","play error\n\n");
	}
	else LOG("ERROR","id=%p\n\n", id);
	return nRet;
}

int stop(const char *id, int cmd)
{
	int nRet = -1;
	if (NULL != id)
	{
		if (CTRL_PLAY_STOP == cmd)
		{	// 点播-停止
			LOG("INFO","Stop on demand '%s'\n\n", id);
			static int msg_size = sizeof(udp_msg_t);
			udp_msg_t msg;
			msg.cmd = CTRL_PLAY_STOP;
			strcpy(msg.call_id, id);
			int send_bytes = g_udp_client.Send(&msg, msg_size, g_udp_ser_ip, g_udp_ser_port);
			if (msg_size == send_bytes)
			{
				LOG("INFO","Send data to '%s:%d' successfully %d ^-^\n\n", g_udp_ser_ip, g_udp_ser_port, send_bytes);
				nRet = g_media.destroy_channel(id);
			}
			else LOG("ERROR","Send data to '%s:%d' failed %d/%d\n\n", g_udp_ser_ip, g_udp_ser_port, send_bytes, msg_size);
		}
		else if (CTRL_PLAYBAK_STOP == cmd)
		{	// 回放-停止
			LOG("INFO","Stop playback '%s'\n\n", id);
			nRet = g_media.destroy_channel(id);
		}
		else LOG("ERROR","Unknown cmd %d\n\n", cmd);
	}
	else LOG("ERROR","id=%p \n\n", id);
	return nRet;
}
int playbak_ctrl(const char *id, int cmd, int pos/*=0*/, float speed/*=1.0*/)
{
	const channel_t * chl = g_media.get_channel(id);
	if (NULL != chl) {
		if (CTRL_PLAYBAK_PAUSE == cmd) { // 回播-暂停
			chl->stream->pause();
		}
		else if (CTRL_PLAYBAK_CONTINUE == cmd) { // 回播-继续
			chl->stream->continu();
		}
		else if (CTRL_PLAYBAK_POS == cmd) { // 回播-拖放
			chl->stream->seek(pos, 0);
		}
		else if (CTRL_PLAYBAK_SPEED == cmd) { // 回播-速度控制(0.5, 1.0, 2.0 等)
			chl->stream->speed(speed);
		}
		else if (CTRL_PLAY_PAUSE == cmd) { // 点播-暂停
			chl->stream->pause(false);
		}
		else if (CTRL_PLAY_CONTINUE == cmd) { // 点播-继续
			chl->stream->continu(false);
		}
		else LOG("ERROR","Unknown cmd %d\n\n", cmd);
	}
	else LOG("ERROR","channel=%p\n\n", chl);
	return 0;
}
int control(addr_info_t *dev, int cmd)
{
	int nRet = -1;
	if (NULL != dev) {
		static int msg_size = sizeof(udp_msg_t);
		udp_msg_t msg;
		msg.cmd = (cmd_e)cmd;
		memcpy(&msg.dev, dev, sizeof(addr_info_t));
		int send_bytes = g_udp_client.Send(&msg, msg_size, g_udp_ser_ip, g_udp_ser_port);
		if (msg_size != send_bytes) {
			LOG("ERROR", "Send data to '%s:%d' failed %d/%d\n\n", g_udp_ser_ip, g_udp_ser_port, send_bytes, msg_size);
		}
		else nRet = 0;
	}
	else LOG("ERROR", "Parameter error (dev=%p)\n\n", dev);
	return nRet;
}
int snapshoot(addr_info_t* dev, const char* filename)
{
	int nRet = -1;
	if ((NULL == dev) || (NULL == filename)) {
		LOG("ERROR", "Parameter error %p %p\n\n", dev, filename);
		return nRet;
	}
	// 点播 或 录像回放(设备端)
	int port = (g_media_port > 0) ? g_media_port : g_media.alloc_port();
	if (port > 0) {
		static int msg_size = sizeof(udp_msg_t);
		udp_msg_t msg;
		msg.cmd = CTRL_PLAY_START;
		memcpy(&msg.dev, dev, sizeof(addr_info_t));
		strcpy(msg.call_id, dev->id);
		strcpy(msg.media_ip, g_media_ip);
		msg.media_port = port;
		// 发送点播命令
		nRet = (msg_size == g_udp_client.Send(&msg, msg_size, g_udp_ser_ip, g_udp_ser_port)) ? 0 : -1;
		if (0 == nRet) {
			nRet = g_media.snapshoot(g_media_ip, port, filename);
			if (nRet < 0)
				LOG("ERROR", "snapshoot failed %d [ip:%s port:%d]\n\n", nRet, g_media_ip, port);

			// 发送停止命令
			msg.cmd = CTRL_PLAY_STOP;
			int send_bytes = g_udp_client.Send(&msg, msg_size, g_udp_ser_ip, g_udp_ser_port);
			if (msg_size != send_bytes)
				LOG("ERROR", "Send data to '%s:%d' failed %d/%d\n\n", g_udp_ser_ip, g_udp_ser_port, send_bytes, msg_size);
		}
		else LOG("INFO", "UDP client send message to '%s:%d' failed %d ^-^\n\n", g_udp_ser_ip, g_udp_ser_port, nRet);
		g_media.free_port(port);
	}
	else LOG("ERROR", "alloc_port failed %d\n\n", port);
	return nRet;
}

int on_udp_receive(const void *pMsg, int nLen, const char* pIp, int nPort, void *pUserData)
{
	LOG("INFO","Message from the UDP client '%s:%d'\n\n", pIp, nPort);
	udp_msg_t *msg = (udp_msg_t*)pMsg;
	LOG("INFO","============= UDP Message ======================\n");
	LOG("INFO", "     dev: %s:%s:%d\n", msg->dev.id, msg->dev.ip, msg->dev.port);
	LOG("INFO", "     cmd: %d\n", msg->cmd);
	LOG("INFO", "================================================\n");
#if 0
	if (CTRL_REC_START == msg->cmd) {
		msg->cmd = CTRL_PLAY_START;
		record_t rec;
		strcpy(rec.path, msg->path);
		strcpy(rec.dev_id, msg->dev.id);
		//int nRet = play(msg->dev.id, msg, on_record_stream, &rec, sizeof(rec), false);
	}
	else if (CTRL_REC_STOP == msg->cmd) {
		stop(msg->dev.id);
	}
	else if (NOTIFY_ONLINE == msg->cmd) {
		//g_thd.Stop();
	}
#endif
	return 0;
}