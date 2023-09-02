
#include "main.h"
#include "log.h"
#include "tinyxml.h"
#include "gb28181client_api.h"
#include "media.h"

#include "media.h"
#include <sstream>

#ifdef _WIN32
	#include <io.h>			//_access() 函数
	#include <conio.h>		// _getch();
#elif __linux__
	#include<unistd.h>		// access()函数
	#include <termios.h>	// tcgetattr
	#include <unistd.h>
#endif


extern CUdpClient  g_udp_client;
extern bool g_is_init;
extern local_cfg_t g_local_cfg;
extern char		g_udp_ser_ip[24];// gb28181server.dll模块UDP的IP
extern int		g_udp_ser_port;		// gb28181server.dll模块UDP的Port
extern char		g_media_ip[24];	// 本地的Media的IP
extern int		g_media_port;		// 本地的Media的端口 [随机自动分配]
extern CMedia	g_media;

CMap<string, dev_info_t> g_dev_map;
std::mutex g_record_mutex;

int main(int argc, char* argv[])
{
	printf("%s example ...\n\n", PROJECT_NAME);

	srand(time(NULL));
	const char* filename = "gb28181client_cfg.xml";
	if (argc > 1)
		filename = argv[1];

	local_cfg_t cfg;
	int nRet = load_config(filename, &g_local_cfg, &g_dev_map);
	if (0 == nRet) {
		g_media_port = g_local_cfg.dev.addr.port;
		nRet = init();
		if (0 == nRet) {
			do_main();
		}
	}
#ifdef _WIN32
	system("pause");
#endif
	return 0;
}

int record(cmd_e cmd, addr_info_t* dev, unis_record_t* rec)
{
	if ((NULL == dev) || (NULL == rec)) {
		LOG("ERROR", "dev=%p, rec=%p\n\n", dev, rec);
		return -1;
	}
	static int msg_size = sizeof(udp_msg_t);
	int nRet = -1;
	udp_msg_t msg;
	memcpy(&msg.dev, dev, sizeof(addr_info_t));
	if (CTRL_REC_START == cmd) {	// 录像
		if (strlen(rec->filename) > 0) { // 本地录像
			msg.cmd = CTRL_PLAY_START;
			sprintf(msg.call_id, "%s", g_media.generate_random_string(31).c_str());
			LOG("INFO", "Local record (dev:%s:%s:%d,path:%s,duration:%d)\n\n", msg.dev.id, msg.dev.ip, msg.dev.port, rec->filename, rec->duration);
			int port = g_media.alloc_port();
			if (port > 0) {
				strcpy(msg.media_ip, g_media_ip);
				msg.media_port = port;
				nRet = (msg_size == g_udp_client.Send(&msg, msg_size, (const char*)g_udp_ser_ip, g_udp_ser_port)) ? 0 : -1;
				if (0 == nRet) {
					//LOG("INFO","UDP client send message to '%s:%d' successfully %d ^-^\n\n", g_udp_ser_ip, *g_udp_ser_port, nRet);
					char create_time[32] = { 0 };		// 测试
					time_t start = time(NULL);			// 测试
					g_media.timestamp_to_string(start, create_time, sizeof(create_time));// 测试
					LOG("INFO", "%s Create channel '%s'\n\n", create_time, msg.call_id);

					nRet = g_media.create_channel(msg.call_id, msg.media_ip, port, on_gb_record, NULL, rec);
					if (nRet >= 0) {
						//LOG("INFO","1 join_channel '%s'\n\n", msg.call_id);
						g_media.join_channel(msg.call_id);	// Join channel
						//LOG("INFO","2 join_channel '%s'\n\n", msg.call_id);
						msg.cmd = CTRL_PLAY_STOP;
						nRet = (msg_size == g_udp_client.Send(&msg, msg_size, (const char*)g_udp_ser_ip, g_udp_ser_port)) ? 0 : -1;// Terminate call invite
						if (0 == nRet) {
							time_t end = time(NULL);			// 测试
							char destroy_time[32] = { 0 };		// 测试
							g_media.timestamp_to_string(end, destroy_time, sizeof(destroy_time));// 测试
							LOG("INFO", "%s Destroy channel '%s'[%ds]\n\n", destroy_time, msg.call_id, (end - start));
							g_media.destroy_channel(msg.call_id);	// Destroy channel
							// ...
						}
						else LOG("ERROR", "UDP client send message to '%s:%d' failed %d\n\n", g_udp_ser_ip, g_udp_ser_port, nRet);
					}
					else LOG("ERROR", "Create record channel '%s' failed [dev:%s:%s:%d]\n\n", msg.call_id, msg.dev.id, msg.dev.ip, msg.dev.port);
				}
				else LOG("ERROR", "UDP client send message to '%s:%d' failed %d\n\n", g_udp_ser_ip, g_udp_ser_port, nRet);
				g_media.free_port(port);
			}
			else LOG("ERROR", "alloc_port failed %d.\n\n", port);
		}
		else {		// 设备端录像
			msg.cmd = CTRL_REC_START;
			msg.start_time = rec->start_time;
			msg.end_time = rec->end_time;
			LOG("INFO", "Device record (dev:%s:%s:%d,time:%ld~%ld)\n\n", msg.dev.id, msg.dev.ip, msg.dev.port, msg.start_time, msg.end_time);
			if ((msg.start_time > 0) && (msg.end_time > msg.start_time)) {
				nRet = (msg_size == g_udp_client.Send(&msg, msg_size, (const char*)g_udp_ser_ip, g_udp_ser_port)) ? 0 : -1;
				if (0 != nRet)
					LOG("ERROR", "UDP client send message to '%s:%d' failed %d\n\n", g_udp_ser_ip, g_udp_ser_port, nRet);
			}
			else LOG("ERROR", "start_time:%ld, end_time:%ld\n\n", msg.start_time, msg.end_time);
		}
	}
	else if (CTRL_REC_STOP == cmd) {// 录像停止
		msg.cmd = cmd;
		LOG("INFO", "Device record stop (dev:%s:%s:%d)\n\n", msg.dev.id, msg.dev.ip, msg.dev.port);
		nRet = (msg_size == g_udp_client.Send(&msg, msg_size, (const char*)g_udp_ser_ip, g_udp_ser_port)) ? 0 : -1;
		if (0 != nRet)
			LOG("ERROR", "UDP client send message to '%s:%d' failed %d\n\n", g_udp_ser_ip, g_udp_ser_port, nRet);
	}
	else LOG("ERROR", "Unknown command cmd=%d\n\n", cmd);
	return nRet;
}

int on_gb_record(void* data, int size, void* arg)
{
	user_data_t* user_data = (user_data_t*)arg;
	if (NULL == user_data->fp) {
		user_data->fp = fopen(user_data->filename, "wb");
		if (NULL == user_data->fp) {
			LOG("ERROR", "file open failed '%s'\n\n", user_data->filename);
			return 0;
		}
		LOG("INFO", "Create file '%s' [token:%s]\n\n", user_data->filename, user_data->token);
	}
	if (NULL != user_data->fp) {
		if ((NULL != data) && (size > 0)) {
			if (user_data->n > 0) {
				user_data->n--;
				LOG("INFO", "FrameSize=%d, token=%s\n\n", size, user_data->token);
			}
			if (size > 30) {
				printf(" data:%p size:%d\n", data, size);
				fwrite(data, 1, size, user_data->fp);
			}
			else {
				LOG("INFO", "Discard mp4 header %d bytes\n\n", size);
			}
		}
		else {
			LOG("INFO", "Close file '%s'\n\n", user_data->filename);
			fclose(user_data->fp);
			user_data->fp = NULL;
		}
	}
	return size;
}

int on_play_media(void* data, int size, void* arg)
{
	user_data_t* user_data = (user_data_t*)arg;
	//LOG("INFO","stream data=%p, size=%d token=%s gid=%s\n\n", data, size, user_data->token, user_data->gid);
	if (NULL == user_data->fp_mp4) {
		user_data->fp_mp4 = fopen(user_data->filename_mp4, "wb");
		if (NULL == user_data->fp_mp4) {
			LOG("ERROR", "file open failed '%s'\n\n", user_data->filename_mp4);
			return 0;
		}
		LOG("INFO", "Create file '%s' [token:%s]\n\n", user_data->filename_mp4, user_data->token);
	}
	if (NULL != user_data->fp_mp4) {
		if ((NULL != data) && (size > 0)) {
			if (user_data->n > 0) {
				user_data->n--;
				LOG("INFO", "FrameSize=%d, token=%s\n\n", size, user_data->token);
			}
			if (size > 30) {
				printf(" data:%p size:%d\n", data, size);
				fwrite(data, 1, size, user_data->fp_mp4);
			}
			else {
				LOG("INFO", "Discard mp4 header %d bytes\n\n", size);
			}
		}
		else {
			LOG("INFO", "Close file '%s'\n\n", user_data->filename_mp4);
			fclose(user_data->fp_mp4);
			user_data->fp_mp4 = NULL;
		}
	}
	return size;
}

int do_main(int autom/*=0*/)
{
	int nRet = 0;
	CMap<string, addr_info_t> play_map;	// 点播和回播集合
	char ch = 0;
	while (ch != 'e') {
		printf("Please input command:>\n");
		if (0 == autom)
			ch = get_char();
		else {				// 自动化运行程序
			ch = 97 + (rand() % 26);
			while ('e' == ch)
				ch = 97 + (rand() % 26);
		}
		printf("Selected command: '%c'\n", ch);
		switch (ch) {
		case 'a': {// 点播
			LOG("INFO", "------ play ------\n\n");
			udp_msg_t msg;
			msg.cmd = CTRL_PLAY_START;
			auto dev = g_dev_map.begin();
			for (; dev != g_dev_map.end(); dev++) {
				memcpy(&msg.dev, &dev->second.addr, sizeof(addr_info_t));
				string token = g_media.generate_random_string(30);
				strcpy(msg.call_id, token.c_str());
				user_data_t user_data;
				strcpy(user_data.token, token.c_str());
				sprintf(user_data.gid, "play '%s:%s:%d'", dev->second.addr.id, dev->second.addr.ip, dev->second.addr.port);
				char current_time[32] = { 0 };
				g_media.timestamp_to_string(time(NULL), current_time, sizeof(current_time), '.');
				#ifdef _WIN32
				sprintf(user_data.filename, "./play_%s.ps", current_time);
				sprintf(user_data.filename_mp4, "./play_%s.mp4", current_time);
				#elif __linux__
				sprintf(user_data.filename, "./play_%s.ps", current_time);
				sprintf(user_data.filename_mp4, "./play_%s.mp4", current_time);
				#endif
				nRet = play(token.c_str(), &msg, NULL, on_gb_record, on_play_media, &user_data, sizeof(user_data_t));
				if (0 == nRet)
					play_map.add(token.c_str(), &dev->second.addr);
			}
		}break;
		case 'b': {// 点播-停止
			LOG("INFO", "------ play stop ------\n\n");
			auto iter = play_map.begin();
			if (iter != play_map.end()) {
				nRet = stop(iter->first.c_str(), CTRL_PLAY_STOP);
				if (0 == nRet) {
					LOG("INFO", "------ stop '%s' [size:%d] ------\n\n", iter->first.c_str(), play_map.size() - 1);
					play_map.erase(iter);
				}
				else LOG("ERROR", "------ stop '%s' failed [size:%d] ------\n\n", iter->first.c_str(), play_map.size());
			}
		}break;
		case 'c': {// 回播(设备)
			LOG("INFO", "------ playback device ------\n\n");
			udp_msg_t msg;
			msg.cmd = CTRL_PLAYBAK_START;
			auto dev = g_dev_map.begin();
			for (; dev != g_dev_map.end(); dev++) {
				memcpy(&msg.dev, &dev->second.addr, sizeof(addr_info_t));
				auto rec = dev->second.rec_map.begin();
				for (; rec != dev->second.rec_map.end(); rec++) {
					string token = g_media.generate_random_string(31);
					strcpy(msg.call_id, token.c_str());
					msg.start_time = g_media.string_to_timestamp(rec->second.start_time);
					msg.end_time = g_media.string_to_timestamp(rec->second.end_time);
					user_data_t user_data;
					strcpy(user_data.token, token.c_str());
					sprintf(user_data.gid, "playback '%s:%s' %s~%s", dev->second.addr.id, dev->second.addr.ip, rec->second.start_time, rec->second.end_time);
					char current_time[32] = { 0 };
					g_media.timestamp_to_string(time(NULL), current_time, sizeof(current_time), '.');// 测试
					sprintf(user_data.filename, "D:\\Work\\video-c\\gb28181_server\\build\\bin\\playback_%s.mp4", current_time);
					nRet = play(token.c_str(), &msg, NULL, NULL, on_play_media, &user_data, sizeof(user_data_t));
					if (0 == nRet)
						play_map.add(token.c_str(), &dev->second.addr);
				}
			}
		}break;
		case 'd': {// 回播(本地)
			LOG("INFO", "------ playback local ------\n\n");
			auto dev = g_dev_map.begin();
			for (; dev != g_dev_map.end(); dev++) {
				auto rec = dev->second.rec_map.begin();
				for (; rec != dev->second.rec_map.end(); rec++) {
					string token = g_media.generate_random_string(31);
					user_data_t user_data;
					strcpy(user_data.token, token.c_str());
					sprintf(user_data.gid, "playback '%s'", rec->second.file_path);
					char current_time[32] = { 0 };
					g_media.timestamp_to_string(time(NULL), current_time, sizeof(current_time), '.');
					sprintf(user_data.filename, "D:\\Work\\video-c\\gb28181_server\\build\\bin\\playback_%s.mp4", current_time);
					nRet = play(token.c_str(), NULL, rec->second.file_path, NULL, on_play_media, &user_data, sizeof(user_data_t));
					if (0 == nRet)
						play_map.add(token.c_str(), &dev->second.addr);
				}
			}
		}break;
		case 'f': {// 回播-停止
			LOG("INFO", "------ playbak stop ------\n\n");
			auto iter = play_map.begin();
			if (iter != play_map.end()) {
				nRet = stop(iter->first.c_str(), CTRL_PLAYBAK_STOP);
				if (0 == nRet) {
					LOG("INFO", "------ playbak '%s' [size:%d] ------\n\n", iter->first.c_str(), play_map.size() - 1);
					play_map.erase(iter);
				}
				else LOG("ERROR", "------ stop '%s' failed %d [size:%d] ------\n\n", iter->first.c_str(), nRet, play_map.size());
			}
		}break;
		case 'e': {
			LOG("INFO", "------ program exit ------\n\n");
		}break;
		case 'g': {// 录像(设备)
			LOG("INFO", "------ record device ------\n\n");
			auto dev = g_dev_map.begin();
			for (; dev != g_dev_map.end(); dev++) {
				unis_record_t rec_info;
				auto rec = dev->second.rec_map.begin();
				for (; rec != dev->second.rec_map.end(); rec++) {
					rec_info.start_time = g_media.string_to_timestamp(rec->second.start_time);
					rec_info.end_time = g_media.string_to_timestamp(rec->second.end_time);
					break;
				}
				nRet = record(CTRL_REC_START, &dev->second.addr, &rec_info);
			}
		}break;
		case 'h': {// 录像(本地)
			LOG("INFO", "------ record local start ------\n\n");
			CMap<int, CThread11*> thd_map;
			int i = 0, count = 1;
			for (i = 0; i < 10; i++) {
				auto dev = g_dev_map.begin();
				for (; dev != g_dev_map.end(); dev++) {
					char current_time[32] = { 0 };
					g_media.timestamp_to_string(time(NULL), current_time, sizeof(current_time), '.');// 测试
					unis_record_t rec_info;
					sprintf(rec_info.filename, "D:/Work/video-c/gb28181_server/build/bin/record_%s.ps", current_time);
					rec_info.duration = 120; // 录像时长
					#if 0
					// 同步录像(不创建线程)
					nRet = record(CTRL_REC_START, &dev->second, &rec_info);
					if (0 != nRet) {
						LOG("INFO", "ERROR: Record failed %d \n\n", nRet);
						break;
					}
					#else
					// 异步录像(创建线程)
					user_data2_t data;
					memcpy(&data.dev, &dev->second.addr, sizeof(addr_info_t));
					memcpy(&data.rec, &rec_info, sizeof(unis_record_t));
					CThread11* thd = new CThread11();
					thd->Run(on_record_proc, &data, sizeof(user_data2_t));
					thd_map.add(count++, &thd);
					#ifdef _WIN32
					Sleep(1000);
					#else
					sleep(1);
					#endif
					#endif
				}
			}
			auto iter = thd_map.begin();
			for (iter; iter != thd_map.end(); iter++) {
				if (NULL != iter->second) {
					iter->second->Join();
					delete iter->second;
				}
			}
			LOG("INFO", "------ record local end %d ------\n\n", i);
		}break;
		case 'i': {// 回播-控制
			LOG("INFO", " 1.Pause 2.Continue 3.Position\n\n");
			ch = get_char();
			auto iter = play_map.begin();
			if (iter != play_map.end()) {
				if ('1' == ch) { // 回播-暂停
					nRet = playbak_ctrl(iter->first.c_str(), CTRL_PLAYBAK_PAUSE);
				}
				else if ('2' == ch) { // 回播-继续
					nRet = playbak_ctrl(iter->first.c_str(), CTRL_PLAYBAK_CONTINUE);
				}
				else if ('3' == ch) { // 回播-拖放
					nRet = playbak_ctrl(iter->first.c_str(), CTRL_PLAYBAK_POS, 30);
				}
			}
		}break;
		default: {
			nRet = -1;
			LOG("ERROR", "Invalid command '%c'\n\n", ch);
		}
		}
		//if(0 != nRet)
		//	LOG("INFO","error code: %d\n", nRet);
	}
	return nRet;
}
void STDCALL on_record_proc(STATE& state, PAUSE& p, void* pUserData)
{
	user_data2_t* data = (user_data2_t*)pUserData;
	int nRet = record(CTRL_REC_START, &data->dev, &data->rec);
	if (0 != nRet) {
		LOG("ERROR", "Record failed %d \n\n", nRet);
	}
}

int load_config(const char* filename, local_cfg_t* cfg, CMap<string, dev_info_t>* dev_map)
{
	int nRet = -1;
	TiXmlDocument document;
	bool bRet = document.LoadFile(filename);
	if (false == bRet) {
		printf("\n\n");
		LOG("ERROR", "File loading failed ('%s')\n\n", filename);
		return nRet;
	}
	//printf("INFO: Loading file successfully ^-^ '%s'\n\n", filename);
	TiXmlNode* config = document.FirstChild("config");
	if (NULL != config) {
		if (NULL != cfg) {
			TiXmlElement* media = config->FirstChildElement("media");
			if (NULL != media) {
				nRet = 0;
				TiXmlElement* pElem = media->FirstChildElement("ip");
				if ((NULL != pElem) && (NULL != pElem->GetText()))
					strncpy(cfg->dev.addr.ip, pElem->GetText(), sizeof(cfg->dev.addr.ip) - 1);
				pElem = media->FirstChildElement("port");
				if ((NULL != pElem) && (NULL != pElem->GetText()))
					cfg->dev.addr.port = atoi(pElem->GetText());
			}
			TiXmlElement* gb28181server = config->FirstChildElement("gb28181server");
			if (NULL != gb28181server) {
				nRet = 0;
				TiXmlElement* pElem = gb28181server->FirstChildElement("udp_ip");
				if ((NULL != pElem) && (NULL != pElem->GetText()))
					strncpy(cfg->addr.ip, pElem->GetText(), sizeof(cfg->addr.ip) - 1);
				pElem = gb28181server->FirstChildElement("udp_port");
				if ((NULL != pElem) && (NULL != pElem->GetText()))
					cfg->addr.port = atoi(pElem->GetText());
			}
		}
		///////////////////////////////////////////////////////////
		if (NULL != dev_map) {
			TiXmlElement* device = config->FirstChildElement("device");
			if (NULL != device) {
				nRet = 0;
				TiXmlElement* item_dev = device->FirstChildElement("item");
				while (NULL != item_dev) {
					dev_info_t dev;
					TiXmlElement* pElem = item_dev->FirstChildElement("id");
					if ((NULL != pElem) && (NULL != pElem->GetText()))
						strncpy(dev.addr.id, pElem->GetText(), sizeof(dev.addr.id) - 1);
					pElem = item_dev->FirstChildElement("ip");
					if ((NULL != pElem) && (NULL != pElem->GetText()))
						strncpy(dev.addr.ip, pElem->GetText(), sizeof(dev.addr.ip) - 1);
					pElem = item_dev->FirstChildElement("port");
					if ((NULL != pElem) && (NULL != pElem->GetText()))
						dev.addr.port = atoi(pElem->GetText());
					// 录像信息
					TiXmlElement* record = item_dev->FirstChildElement("record");
					if (NULL != record) {
						TiXmlElement* item_rec = record->FirstChildElement("item");
						while (NULL != item_rec) {
							record_info_t rec;
							TiXmlElement* pElem = item_rec->FirstChildElement("start_time");
							if ((NULL != pElem) && (NULL != pElem->GetText()))
								strncpy(rec.start_time, pElem->GetText(), sizeof(rec.start_time) - 1);
							pElem = item_rec->FirstChildElement("end_time");
							if ((NULL != pElem) && (NULL != pElem->GetText()))
								strncpy(rec.end_time, pElem->GetText(), sizeof(rec.end_time) - 1);
							pElem = item_rec->FirstChildElement("filename");
							if ((NULL != pElem) && (NULL != pElem->GetText()))
								strncpy(rec.file_path, pElem->GetText(), sizeof(rec.file_path) - 1);
							//printf("rec %s %s~%s\n\n", rec.file_path, rec.start_time, rec.end_time);
							dev.rec_map[rec.start_time] = rec;
							item_rec = item_rec->NextSiblingElement("item");
						}
					}
					dev_map->add(dev.addr.id, &dev);
					item_dev = item_dev->NextSiblingElement("item");
				}
			}
		}
	}
	return nRet;
}

char get_char()
{
#ifdef _WIN32
	return _getch();		// #include <conio.h>
#elif __linux__
	//#include <termios.h>  // tcgetattr
	//#include <unistd.h>
	struct termios stored_settings;
	struct termios new_settings;
	tcgetattr(0, &stored_settings);
	new_settings = stored_settings;
	new_settings.c_lflag &= (~ICANON);
	new_settings.c_cc[VTIME] = 0;
	new_settings.c_cc[VMIN] = 1;
	tcsetattr(0, TCSANOW, &new_settings);
	char c = getchar();
	putchar('\b');
	//printf("input:  [%c]\n", c);
	tcsetattr(0, TCSANOW, &stored_settings);
	return c;
#endif
}