
#include <stdio.h>
#include "main.h"
#include "media.h"
#include "debug.h"
//#include "common.h"

#define USE_TINY_XML
FILE *g_log_file = fopen("gb28181media.log", "w");


CMedia media(7000, 500);

int STDCALL on_original_stream(void *pData, int nSize, void *pUserData);
int STDCALL on_mp4_stream(void *pData, int nSize, void *pUserData);
int STDCALL on_record(void *pData, int nLen, void *pUserData);

typedef struct media_user_data
{
	FILE *fp = NULL;
	FILE *mp4 = NULL;
	char filename[256] = { 0 };
	char filename_mp4[256] = { 0 };
	int duration = 0;
	int size = 0;
	time_t start_time = 0;
	time_t end_time = 0;
}media_user_data_t;

int main(int argc, char *argv[])
{
	const char *cfg_file = "gb28181media_cfg.xml";
	if (argc > 1)
		cfg_file = argv[1];

	media_cfg_t cfg;
	strcpy(cfg.ip, "192.168.0.105");
	cfg.port = 25060;
	int nRet = read_config(cfg_file, &cfg);
	if (0 != nRet) {
		#ifdef _WIN32
		system("pause");
		#else
		printf("exit.\n");
		#endif
		return nRet;
	}
	cfg.port = (cfg.port > 0) ? cfg.port : media.alloc_port(); // 分配端口

	char id[64] = { 0 };
	sprintf(id, "%lld", time(NULL));

#if 1
	media_user_data_t rec;
	sprintf(rec.filename, "./playback_%lld.ps", id);
	sprintf(rec.filename_mp4, "./playback_%lld.mp4", id);

	#if 1
		// 点播
		nRet = media.create_channel(id, cfg.ip, cfg.port, on_original_stream, on_mp4_stream, &rec);
		//nRet = media.create_channel(id, cfg.ip, cfg.port, on_original_stream, on_mp4_stream, &rec);
		if (nRet >= 0) {
			//getchar();
			media.join_channel(id);
			media.destroy_channel(id);
		}
		else LOG("ERROR","Create channel feiled %d\n\n", nRet);
	#else
		// 回放
		int nRet = media.create_channel(id, cfg.url, on_original_stream, on_mp4_stream, &rec);
		if (nRet >= 0) {
			media.join_channel(id);
			media.destroy_channel(id);
		}
		else LOG("ERROR","Create channel feiled %d\n\n", nRet);
	#endif
#else
	// 录像
	media_user_data_t data;
	sprintf(data.filename, "D:/Work/video-c/gb28181_server/build/bin/%lld.ps", time(NULL));
	data.duration = 120;
	nRet = media.create_channel(id, cfg.ip, cfg.port, on_record, NULL, &data);
	if (nRet >= 0) {
		LOG("INFO","1 join_channel '%s'\n\n", id);
		media.join_channel(id);
		LOG("INFO","2 join_channel '%s'\n\n", id);
		media.destroy_channel(id);
	}
	else LOG("ERROR","Create record channel failed [%s]\n\n", id);
#endif
	media.free_port(cfg.port);	// 释放端口

	#ifdef _WIN32
	system("pause");
	#else
	printf("exit.\n");
	#endif
    return 0;
}
int STDCALL on_original_stream(void *pData, int nSize, void *pUserData)
{
	media_user_data_t *rec = (media_user_data_t*)pUserData;
#if 1
	if (NULL == rec->fp) {
		rec->fp = fopen(rec->filename, "wb");
		if (NULL == rec->fp) {
			LOG("ERROR", "Open file failed '%s'\n\n", rec->filename);
			return 0;
		}
		LOG("INFO", "Open file '%s'\n\n", rec->filename);
	}
	if ((NULL != pData) && (nSize > 0)) {
		//printf("1 data:%p, size:%d\n", pData, nSize);
		fwrite(pData, 1, nSize, rec->fp);
	}
	else {
		fclose(rec->fp);
		rec->fp = NULL;
		LOG("INFO", "Close file '%s'\n\n", rec->filename);
	}
#endif
	return 0;
}

int STDCALL on_mp4_stream(void *pData, int nSize, void *pUserData)
{
	media_user_data_t *rec = (media_user_data_t*)pUserData;
	if (NULL == rec->mp4) {
		rec->mp4 = fopen(rec->filename_mp4, "wb");
		if (NULL == rec->mp4) {
			LOG("ERROR","Open file failed '%s'\n\n", rec->filename_mp4);
			return 0;
		}
		LOG("INFO","Open file '%s'\n\n", rec->filename_mp4);
	}
	if ((NULL != pData) && (nSize > 0)) {
		printf("2 data:%p, size:%d\n", pData, nSize);
		fwrite(pData, 1, nSize, rec->mp4);
	}
	else {
		fclose(rec->mp4);
		rec->mp4 = NULL;
		LOG("INFO","Close file '%s'\n\n", rec->filename_mp4);
	}
	return 0;
}



int STDCALL on_record(void *pData, int nLen, void *pUserData)
{
	media_user_data_t *rec = (media_user_data_t*)pUserData;
	if ((NULL != pData) && (nLen > 0)) {
		if ((NULL == rec->fp) && (0 == rec->size)) {
			rec->start_time = time(NULL);
			rec->end_time = rec->start_time + rec->duration + 1;
			//DEBUG("INFO: duration=%d, time:%lld~%lld, filename=%s\n\n", rec->duration, rec->start_time, rec->end_time, rec->filename);
			rec->fp = fopen(rec->filename, "wb");
			printf("INFO: Create a file '%s' time:%lld~%lld\n\n", rec->filename, rec->start_time, rec->end_time);
			if (NULL == rec->fp) {
				LOG("ERROR","File open failure '%s'\n\n", rec->filename);
				return -1;
			}
		}
		if (NULL != rec->fp) {
			int write_bytes = fwrite(pData, 1, nLen, rec->fp);
			if (write_bytes > 0)
				rec->size += write_bytes;
		}
		if (rec->end_time <= time(NULL)) {
			if (NULL != rec->fp) {
				fclose(rec->fp);
				rec->fp = NULL;
				LOG("INFO","-------- File closed 1 '%s' --------\n\n", rec->filename);
			}
			return -1;
		}
	}
	else {
		//rec->end_time = time(NULL);
		if (NULL != rec->fp) {
			fclose(rec->fp);
			rec->fp = NULL;
			LOG("INFO","-------- File closed 2 '%s' --------\n\n", rec->filename);
			return -1;
		}
	}
	//DEBUG("INFO: ------ on_record end ------ \n\n");
	return 0;
}


#ifdef USE_TINY_XML
#include "tinyxml.h"
int read_config(const char *filename, media_cfg_t *cfg)
{
	int nRet = -1;
	#if 1
		TiXmlDocument document;
		if (false == document.LoadFile(filename)) {
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
		if (NULL != cfg)
		{
			TiXmlElement *local = config->FirstChildElement("local");
			if (NULL != local)
			{
				nRet = 0;
				TiXmlElement *pElem = local->FirstChildElement("ip");
				if ((NULL != pElem) && (NULL != pElem->GetText()))
					strncpy(cfg->ip, pElem->GetText(), sizeof(cfg->ip) - 1);
				pElem = local->FirstChildElement("port");
				if ((NULL != pElem) && (NULL != pElem->GetText()))
					cfg->port = atoi(pElem->GetText());
				pElem = local->FirstChildElement("url");
				if ((NULL != pElem) && (NULL != pElem->GetText()))
					strncpy(cfg->url, pElem->GetText(), sizeof(cfg->url) - 1);
			}
		}
	}
	return nRet;
}
#endif
