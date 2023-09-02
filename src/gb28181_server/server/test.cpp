
#include "test.h"

#include <string.h>
#include <time.h>
#include "tinyxml.h"
#include "log.h"

#ifdef _WIN32
#include <conio.h> // _getch();
#elif __linux__
#include <termios.h>  // tcgetattr
#include <unistd.h>
#endif


char get_char()
{
	#ifdef _WIN32
		return _getch();		// _getch();
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
		putchar('\b'); // 删除回显
		//printf("input:  [%c]\n", c);
		tcsetattr(0, TCSANOW, &stored_settings); // 恢复终端参数
		return c;
	#endif
}
/*
time_t string_to_timestamp(const char *pTime)
{
	struct tm time;
#if 1
	if (pTime[4] == '-' || pTime[4] == '.' || pTime[4] == '/')
	{
		char szFormat[30] = { 0 };				//"%u-%u-%u","%u.%u.%u","%u/%u/%u")
		sprintf(szFormat, "%%d%c%%d%c%%dT%%d:%%d:%%d", pTime[4], pTime[4]);
		sscanf(pTime, szFormat, &time.tm_year, &time.tm_mon, &time.tm_mday, &time.tm_hour, &time.tm_min, &time.tm_sec);
		time.tm_year -= 1900;
		time.tm_mon -= 1;
		return mktime(&time);
	}
#else
	// 字符串转时间戳
	struct tm tm_time;
	strptime("2010-11-15 10:39:30", "%Y-%m-%d %H:%M:%S", &time);
	return mktime(&time);
#endif
	return -1;
}
*/

int load_gb28181server_config(const char *filename, local_cfg_t *local_cfg, CMap<string, dev_info_t> *dev_map, CMap<string, pfm_info_t> *pfm_map, CMap<string, access_info_t> *access_map)
{
	int nRet = -1;
	#if 1
		TiXmlDocument document;
		bool bRet = document.LoadFile(filename);
		if (false == bRet) {
			printf("\n\n");
			LOG("ERROR","File loading failed ('%s')\n\n", filename);
			return nRet;
		}
	#else
		TiXmlDocument document;
		document.Parse(xml_str);
	#endif
	TiXmlNode *config = document.FirstChild("config");
	if (NULL != config)
	{
		nRet = 0;
		if (NULL != dev_map) {
			TiXmlElement *device = config->FirstChildElement("device");
			if (NULL != device) {
				TiXmlElement *item = device->FirstChildElement("item");
				while (NULL != item) {
					TiXmlElement *pElem = item->FirstChildElement("enable");
					if ((NULL != pElem) && (NULL != pElem->GetText())) {
						if (0 == strcmp("0", pElem->GetText())) {
							item = item->NextSiblingElement("item");
							continue;
						}
					}

					dev_info_t dev;
					pElem = item->FirstChildElement("id");
					if ((NULL != pElem) && (NULL != pElem->GetText()))
						strncpy(dev.addr.id, pElem->GetText(), sizeof(dev.addr.id) - 1);
					pElem = item->FirstChildElement("parent_id");
					if ((NULL != pElem) && (NULL != pElem->GetText()))
						strncpy(dev.parent_id, pElem->GetText(), sizeof(dev.parent_id) - 1);
					pElem = item->FirstChildElement("ip");
					if ((NULL != pElem) && (NULL != pElem->GetText()))
						strncpy(dev.addr.ip, pElem->GetText(), sizeof(dev.addr.ip) - 1);
					pElem = item->FirstChildElement("port");
					if ((NULL != pElem) && (NULL != pElem->GetText()))
						dev.addr.port = atoi(pElem->GetText());
					pElem = item->FirstChildElement("platform");
					if ((NULL != pElem) && (NULL != pElem->GetText()))
						strncpy(dev.platform, pElem->GetText(), sizeof(dev.platform) - 1);
					pElem = item->FirstChildElement("type");
					if ((NULL != pElem) && (NULL != pElem->GetText()))
						dev.type = atoi(pElem->GetText());
					pElem = item->FirstChildElement("protocol");
					if ((NULL != pElem) && (NULL != pElem->GetText()))
						dev.protocol = atoi(pElem->GetText());
					pElem = item->FirstChildElement("username");
					if ((NULL != pElem) && (NULL != pElem->GetText()))
						strncpy(dev.username, pElem->GetText(), sizeof(dev.username) - 1);
					pElem = item->FirstChildElement("password");
					if ((NULL != pElem) && (NULL != pElem->GetText()))
						strncpy(dev.password, pElem->GetText(), sizeof(dev.password) - 1);
					pElem = item->FirstChildElement("name");
					if ((NULL != pElem) && (NULL != pElem->GetText()))
						strncpy(dev.name, pElem->GetText(), sizeof(dev.name) - 1);
					pElem = item->FirstChildElement("manufacturer");
					if ((NULL != pElem) && (NULL != pElem->GetText()))
						strncpy(dev.manufacturer, pElem->GetText(), sizeof(dev.manufacturer) - 1);
					pElem = item->FirstChildElement("firmware");
					if ((NULL != pElem) && (NULL != pElem->GetText()))
						strncpy(dev.firmware, pElem->GetText(), sizeof(dev.firmware) - 1);
					pElem = item->FirstChildElement("model");
					if ((NULL != pElem) && (NULL != pElem->GetText()))
						strncpy(dev.model, pElem->GetText(), sizeof(dev.model) - 1);
					pElem = item->FirstChildElement("expires");
					if ((NULL != pElem) && (NULL != pElem->GetText()))
						dev.expires = atoi(pElem->GetText());
					pElem = item->FirstChildElement("register_interval");
					if ((NULL != pElem) && (NULL != pElem->GetText()))
						dev.register_interval = atoi(pElem->GetText());
					pElem = item->FirstChildElement("keepalive");
					if ((NULL != pElem) && (NULL != pElem->GetText()))
						dev.keepalive = atoi(pElem->GetText());
					pElem = item->FirstChildElement("max_k_timeout");
					if ((NULL != pElem) && (NULL != pElem->GetText()))
						dev.max_k_timeout = atoi(pElem->GetText());
					pElem = item->FirstChildElement("auth");
					if ((NULL != pElem) && (NULL != pElem->GetText()))
						dev.auth = atoi(pElem->GetText());
					pElem = item->FirstChildElement("share");
					if ((NULL != pElem) && (NULL != pElem->GetText()))
						dev.share = atoi(pElem->GetText());
					TiXmlElement *channel = item->FirstChildElement("channel");
					if (NULL != channel) {
						TiXmlElement *item = channel->FirstChildElement("item");
						while (NULL != item) {
							TiXmlElement *pElem = item->FirstChildElement("enable");
							if ((NULL != pElem) && (NULL != pElem->GetText())) {
								if (0 == strcmp("0", pElem->GetText())) {
									item = item->NextSiblingElement("item");
									continue;
								}
							}
							chl_info_t chl;
							pElem = item->FirstChildElement("id");
							if ((NULL != pElem) && (NULL != pElem->GetText()))
								strncpy(chl.id, pElem->GetText(), sizeof(chl.id) - 1);
							pElem = item->FirstChildElement("parent_id");
							if ((NULL != pElem) && (NULL != pElem->GetText()))
								strncpy(chl.parent_id, pElem->GetText(), sizeof(chl.parent_id) - 1);
							pElem = item->FirstChildElement("name");
							if ((NULL != pElem) && (NULL != pElem->GetText()))
								strncpy(chl.name, pElem->GetText(), sizeof(chl.name) - 1);
							pElem = item->FirstChildElement("manufacturer");
							if ((NULL != pElem) && (NULL != pElem->GetText()))
								strncpy(chl.manufacturer, pElem->GetText(), sizeof(chl.manufacturer) - 1);
							pElem = item->FirstChildElement("model");
							if ((NULL != pElem) && (NULL != pElem->GetText()))
								strncpy(chl.model, pElem->GetText(), sizeof(chl.model) - 1);
							pElem = item->FirstChildElement("owner");
							if ((NULL != pElem) && (NULL != pElem->GetText()))
								strncpy(chl.owner, pElem->GetText(), sizeof(chl.owner) - 1);
							pElem = item->FirstChildElement("civil_code");
							if ((NULL != pElem) && (NULL != pElem->GetText()))
								strncpy(chl.civil_code, pElem->GetText(), sizeof(chl.civil_code) - 1);
							pElem = item->FirstChildElement("address");
							if ((NULL != pElem) && (NULL != pElem->GetText()))
								strncpy(chl.address, pElem->GetText(), sizeof(chl.address) - 1);
							pElem = item->FirstChildElement("ip_address");
							if ((NULL != pElem) && (NULL != pElem->GetText()))
								strncpy(chl.ip_address, pElem->GetText(), sizeof(chl.ip_address) - 1);
							pElem = item->FirstChildElement("port");
							if ((NULL != pElem) && (NULL != pElem->GetText()))
								chl.port = atoi(pElem->GetText());
							pElem = item->FirstChildElement("safety_way");
							if ((NULL != pElem) && (NULL != pElem->GetText()))
								chl.safety_way = atoi(pElem->GetText());
							pElem = item->FirstChildElement("parental");
							if ((NULL != pElem) && (NULL != pElem->GetText()))
								chl.parental = atoi(pElem->GetText());
							pElem = item->FirstChildElement("secrecy");
							if ((NULL != pElem) && (NULL != pElem->GetText()))
								chl.secrecy = atoi(pElem->GetText());
							pElem = item->FirstChildElement("status");
							if ((NULL != pElem) && (NULL != pElem->GetText())) {
								if (0 == strcmp("ON", pElem->GetText()))
									chl.status = 1;
								else chl.status = 0;
							}
							pElem = item->FirstChildElement("register_way");
							if ((NULL != pElem) && (NULL != pElem->GetText()))
								chl.register_way = atoi(pElem->GetText());
							pElem = item->FirstChildElement("longitude");
							if ((NULL != pElem) && (NULL != pElem->GetText()))
								chl.longitude = atof(pElem->GetText());
							pElem = item->FirstChildElement("latitude");
							if ((NULL != pElem) && (NULL != pElem->GetText()))
								chl.latitude = atof(pElem->GetText());
							pElem = item->FirstChildElement("share");
							if ((NULL != pElem) && (NULL != pElem->GetText()))
								chl.share = atoi(pElem->GetText());

							string key = string(chl.parent_id) + string("#") + string(chl.id);
							//string key = string(chl.id);
							dev.chl_map.insert(map<string, chl_info_t>::value_type(key, chl));
							item = item->NextSiblingElement("item");
						}
					}
					TiXmlElement *record = item->FirstChildElement("record");
					if (NULL != record) {
						TiXmlElement *item = record->FirstChildElement("item");
						while (NULL != item) {
							TiXmlElement *pElem = item->FirstChildElement("enable");
							if ((NULL != pElem) && (NULL != pElem->GetText())) {
								if (0 == strcmp("0", pElem->GetText())) {
									item = item->NextSiblingElement("item");
									continue;
								}
							}
							record_info_t rec;
							strncpy(rec.id, dev.addr.id, sizeof(rec.id) - 1);
							strncpy(rec.name, dev.name, sizeof(rec.name) - 1);
							pElem = item->FirstChildElement("record_id");
							if ((NULL != pElem) && (NULL != pElem->GetText()))
								strncpy(rec.recorder_id, pElem->GetText(), sizeof(rec.recorder_id) - 1);
							pElem = item->FirstChildElement("start_time");
							if ((NULL != pElem) && (NULL != pElem->GetText()))
								strncpy(rec.start_time, pElem->GetText(), sizeof(rec.start_time) - 1);
							pElem = item->FirstChildElement("end_time");
							if ((NULL != pElem) && (NULL != pElem->GetText()))
								strncpy(rec.end_time, pElem->GetText(), sizeof(rec.end_time) - 1);
							pElem = item->FirstChildElement("filename");
							if ((NULL != pElem) && (NULL != pElem->GetText()))
								strncpy(rec.file_path, pElem->GetText(), sizeof(rec.file_path) - 1);
							pElem = item->FirstChildElement("address");
							if ((NULL != pElem) && (NULL != pElem->GetText()))
								strncpy(rec.address, pElem->GetText(), sizeof(rec.address) - 1);
							pElem = item->FirstChildElement("secrecy");
							if ((NULL != pElem) && (NULL != pElem->GetText()))
								rec.secrecy = atoi(pElem->GetText());
							pElem = item->FirstChildElement("type");
							if ((NULL != pElem) && (NULL != pElem->GetText())) {
								if (0 == strcmp("all", pElem->GetText()))
									rec.type = 1;	// 1:all 2:manual(手动录像) 3:time 4:alarm
								else if (0 == strcmp("manual", pElem->GetText()))
									rec.type = 2;
								else if (0 == strcmp("time", pElem->GetText()))
									rec.type = 3;
								else if (0 == strcmp("alarm", pElem->GetText()))
									rec.type = 4;
								else rec.type = 1;
							}
							pElem = item->FirstChildElement("size");
							if ((NULL != pElem) && (NULL != pElem->GetText()))
								rec.size = atoi(pElem->GetText());

							string key = string(rec.start_time) + string("#") + string(rec.end_time);
							dev.rec_map.insert(map<string, record_info_t>::value_type(key, rec));
							item = item->NextSiblingElement("item");
						}
					}
					dev_map->add(dev.addr.id, &dev);
					item = item->NextSiblingElement("item");
				}
			}
		}
		if (NULL != pfm_map) {
			TiXmlElement *platform = config->FirstChildElement("platform");
			if (NULL != platform) {
				TiXmlElement *item = platform->FirstChildElement("item");
				while (NULL != item) {
					pfm_info_t pfm;
					TiXmlElement *pElem = item->FirstChildElement("id");
					if ((NULL != pElem) && (NULL != pElem->GetText()))
						strncpy(pfm.addr.id, pElem->GetText(), sizeof(pfm.addr.id) - 1);
					pElem = item->FirstChildElement("ip");
					if ((NULL != pElem) && (NULL != pElem->GetText()))
						strncpy(pfm.addr.ip, pElem->GetText(), sizeof(pfm.addr.ip) - 1);
					pElem = item->FirstChildElement("port");
					if ((NULL != pElem) && (NULL != pElem->GetText()))
						pfm.addr.port = atoi(pElem->GetText());
					pElem = item->FirstChildElement("domain");
					if ((NULL != pElem) && (NULL != pElem->GetText()))
						strncpy(pfm.domain, pElem->GetText(), sizeof(pfm.domain) - 1);
					pElem = item->FirstChildElement("platform");
					if ((NULL != pElem) && (NULL != pElem->GetText()))
						strncpy(pfm.platform, pElem->GetText(), sizeof(pfm.platform) - 1);
					pElem = item->FirstChildElement("username");
					if ((NULL != pElem) && (NULL != pElem->GetText()))
						strncpy(pfm.username, pElem->GetText(), sizeof(pfm.username) - 1);
					pElem = item->FirstChildElement("password");
					if ((NULL != pElem) && (NULL != pElem->GetText()))
						strncpy(pfm.password, pElem->GetText(), sizeof(pfm.password) - 1);
					pElem = item->FirstChildElement("expires");
					if ((NULL != pElem) && (NULL != pElem->GetText()))
						pfm.expires = atoi(pElem->GetText());
					pElem = item->FirstChildElement("register_interval");
					if ((NULL != pElem) && (NULL != pElem->GetText()))
						pfm.register_interval = atoi(pElem->GetText());
					pElem = item->FirstChildElement("keepalive");
					if ((NULL != pElem) && (NULL != pElem->GetText()))
						pfm.keepalive = atoi(pElem->GetText());
					pElem = item->FirstChildElement("cseq");
					if ((NULL != pElem) && (NULL != pElem->GetText()))
						pfm.cseq = atoi(pElem->GetText());
					pElem = item->FirstChildElement("enable");
					if ((NULL != pElem) && (NULL != pElem->GetText()))
						pfm.enable = atoi(pElem->GetText());
					pfm_map->add(pfm.addr.id, &pfm);
					item = item->NextSiblingElement("item");
				}
			}
		}
		if (NULL != local_cfg) {
			TiXmlElement *local = config->FirstChildElement("local");
			if (NULL != local) {
				TiXmlElement *pElem = local->FirstChildElement("id");
				if ((NULL != pElem) && (NULL != pElem->GetText())) {
					strncpy(local_cfg->addr.id, pElem->GetText(), sizeof(local_cfg->addr.id) - 1);
					strncpy(local_cfg->dev.addr.id, pElem->GetText(), sizeof(local_cfg->dev.addr.id) - 1);
				}
				pElem = local->FirstChildElement("ip");
				if ((NULL != pElem) && (NULL != pElem->GetText()))
					strncpy(local_cfg->addr.ip, pElem->GetText(), sizeof(local_cfg->addr.ip) - 1);
				pElem = local->FirstChildElement("port");
				if ((NULL != pElem) && (NULL != pElem->GetText()))
					local_cfg->addr.port = atoi(pElem->GetText());
				pElem = local->FirstChildElement("pfm_ip");
				if ((NULL != pElem) && (NULL != pElem->GetText()))
					strncpy(local_cfg->dev.addr.ip, pElem->GetText(), sizeof(local_cfg->dev.addr.ip) - 1);
				pElem = local->FirstChildElement("pfm_port");
				if ((NULL != pElem) && (NULL != pElem->GetText()))
					local_cfg->dev.addr.port = atoi(pElem->GetText());
				pElem = local->FirstChildElement("domain");
				if ((NULL != pElem) && (NULL != pElem->GetText()))
					strncpy(local_cfg->domain, pElem->GetText(), sizeof(local_cfg->domain) - 1);
				#if 1
					pElem = local->FirstChildElement("media_ip");
					if ((NULL != pElem) && (NULL != pElem->GetText()))
						strncpy(local_cfg->media.ip, pElem->GetText(), sizeof(local_cfg->dev.addr.ip) - 1);
					pElem = local->FirstChildElement("media_port");
					if ((NULL != pElem) && (NULL != pElem->GetText()))
						local_cfg->media.port = atoi(pElem->GetText());
				#endif
				pElem = local->FirstChildElement("password");
				if ((NULL != pElem) && (NULL != pElem->GetText()))
					strncpy(local_cfg->password, pElem->GetText(), sizeof(local_cfg->password) - 1);
			}
		}
		if (NULL != access_map) {
			TiXmlElement *access = config->FirstChildElement("access");
			if (NULL != access) {
				TiXmlElement *item = access->FirstChildElement("item");
				while (NULL != item) {
					TiXmlElement *pElem = item->FirstChildElement("enable");
					if ((NULL != pElem) && (NULL != pElem->GetText())) {
						if (0 == strcmp("0", pElem->GetText())) {
							item = item->NextSiblingElement("item");
							continue;
						}
					}
					access_info_t acc;
					pElem = item->FirstChildElement("id");
					if ((NULL != pElem) && (NULL != pElem->GetText()))
						strncpy(acc.addr.id, pElem->GetText(), sizeof(acc.addr.id) - 1);
					pElem = item->FirstChildElement("ip");
					if ((NULL != pElem) && (NULL != pElem->GetText()))
						strncpy(acc.addr.ip, pElem->GetText(), sizeof(acc.addr.ip) - 1);
					pElem = item->FirstChildElement("port");
					if ((NULL != pElem) && (NULL != pElem->GetText()))
						acc.addr.port = atoi(pElem->GetText());
					pElem = item->FirstChildElement("auth");
					if ((NULL != pElem) && (NULL != pElem->GetText()))
						acc.auth = atoi(pElem->GetText());
					pElem = item->FirstChildElement("realm");
					if ((NULL != pElem) && (NULL != pElem->GetText()))
						strncpy(acc.realm, pElem->GetText(), sizeof(acc.realm) - 1);
					pElem = item->FirstChildElement("password");
					if ((NULL != pElem) && (NULL != pElem->GetText()))
						strncpy(acc.password, pElem->GetText(), sizeof(acc.password) - 1);
					pElem = item->FirstChildElement("share");
					if ((NULL != pElem) && (NULL != pElem->GetText()))
						acc.share = atoi(pElem->GetText());

					access_map->add(acc.addr.id, &acc);
					item = item->NextSiblingElement("item");
				}
			}
		}
	}
	return nRet;
}

int load_gb28181client_config(const char *filename, local_cfg_t *cfg, CMap<string, dev_info_t> *dev_map)
{
	int nRet = -1;
	TiXmlDocument document;
	bool bRet = document.LoadFile(filename);
	if (false == bRet) {
		printf("\n\n");
		LOG("ERROR","File loading failed ('%s')\n\n", filename);
		return nRet;
	}
	//printf("INFO: Loading file successfully ^-^ '%s'\n\n", filename);
	TiXmlNode *config = document.FirstChild("config");
	if (NULL != config) {
		if (NULL != cfg) {
			TiXmlElement *media = config->FirstChildElement("media");
			if (NULL != media) {
				nRet = 0;
				TiXmlElement *pElem = media->FirstChildElement("ip");
				if ((NULL != pElem) && (NULL != pElem->GetText()))
					strncpy(cfg->dev.addr.ip, pElem->GetText(), sizeof(cfg->dev.addr.ip) - 1);
				pElem = media->FirstChildElement("port");
				if ((NULL != pElem) && (NULL != pElem->GetText()))
					cfg->dev.addr.port = atoi(pElem->GetText());
			}
			TiXmlElement *gb28181server = config->FirstChildElement("gb28181server");
			if (NULL != gb28181server) {
				nRet = 0;
				TiXmlElement *pElem = gb28181server->FirstChildElement("udp_ip");
				if ((NULL != pElem) && (NULL != pElem->GetText()))
					strncpy(cfg->addr.ip, pElem->GetText(), sizeof(cfg->addr.ip) - 1);
				pElem = gb28181server->FirstChildElement("udp_port");
				if ((NULL != pElem) && (NULL != pElem->GetText()))
					cfg->addr.port = atoi(pElem->GetText());
			}
		}
		///////////////////////////////////////////////////////////
		if (NULL != dev_map) {
			TiXmlElement *device = config->FirstChildElement("device");
			if (NULL != device) {
				nRet = 0;
				TiXmlElement *item_dev = device->FirstChildElement("item");
				while (NULL != item_dev) {
					dev_info_t dev;
					TiXmlElement *pElem = item_dev->FirstChildElement("id");
					if ((NULL != pElem) && (NULL != pElem->GetText()))
						strncpy(dev.addr.id, pElem->GetText(), sizeof(dev.addr.id) - 1);
					pElem = item_dev->FirstChildElement("ip");
					if ((NULL != pElem) && (NULL != pElem->GetText()))
						strncpy(dev.addr.ip, pElem->GetText(), sizeof(dev.addr.ip) - 1);
					pElem = item_dev->FirstChildElement("port");
					if ((NULL != pElem) && (NULL != pElem->GetText()))
						dev.addr.port = atoi(pElem->GetText());
					// 录像信息
					TiXmlElement *record = item_dev->FirstChildElement("record");
					if (NULL != record) {
						TiXmlElement *item_rec = record->FirstChildElement("item");
						while (NULL != item_rec) {
							record_info_t rec;
							TiXmlElement *pElem = item_rec->FirstChildElement("start_time");
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