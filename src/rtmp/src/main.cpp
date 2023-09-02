
#include "main.h"
#include "tinyxml.h"
#include "log.h"
#include "rtmp_media.h"
#include <stdio.h>
//#if (defined (COMPILE_EXE) && defined (__MY_LOG_H__) && defined (_WIN32))
//#include <Windows.h>		// 日志模块
//#endif

#ifdef _WIN32
	#include <conio.h>
	#include "thread11.h"
	#if (defined (COMPILE_EXE) && defined (__MY_LOG_H__))
	#include <Windows.h>		// 日志模块
	#endif
#elif __linux__
	#include <termios.h>  // tcgetattr
	#include <unistd.h>
#endif

CRtmpMedia g_rtmp_media;

#ifdef COMPILE_EXE
typedef struct tag_chl_param {
	stream_param_t stream;
	int status = 0;		// 通道状态(0:未运行,1:运行)
}chl_param_t;

void STDCALL on_control_proc(STATE& state, PAUSE& p, void* pUserData);
char get_char();
int load_config(map<string, chl_param_t>* chl_map, const char* filename);
void print_config(map<string, chl_param_t>* chl_map);

map<string, chl_param_t> g_param_map;
CThread11 g_control_thd;

int main(int argc, char *argv[])
{
	printf("rtmp ... \n\n");

	const char *filename = "./rtmp.xml";
	if (2 == argc) {
		filename = argv[1];
	}
	int nRet = load_config(&g_param_map, filename);
	if (0 == nRet) {
		print_config(&g_param_map);

		int nRet = g_control_thd.Run(on_control_proc);
		if (0 == nRet) {
			g_control_thd.Join();

			g_control_thd.Stop();
		}
	}
	#ifdef _WIN32
	system("pause");
	#else
	printf("exit.\n");
	#endif
	return 0;
}
void STDCALL on_control_proc(STATE& state, PAUSE& p, void* pUserData)
{
	printf("1 create  |  0 destroy\n\n");
	char ch = 0;
	while (TS_STOP != state) {
		if (TS_PAUSE == state)
			p.wait();
		printf(">: ");
		ch = get_char();
		if ('0' == ch) {  // 销毁通道  ('0'==48)
			auto iter = g_param_map.begin();
			for (; iter != g_param_map.end(); iter++) {
				if (0 != iter->second.status) {
					//g_rtmp_media.join_channel(iter->first.c_str());
					int ret = g_rtmp_media.destroy_channel(iter->first.c_str());
					if (ret >= 0) {
						iter->second.status = 0;
						break;
					}
				}
			}
		}
		else if ('1' == ch) {  // 创建通道
			auto iter = g_param_map.begin();
			for (; iter != g_param_map.end(); iter++) {
				if (0 == iter->second.status) {
					int ret = g_rtmp_media.create_channel(iter->first.c_str(), &iter->second.stream);
					if (ret >= 0)
						iter->second.status = 1;
				}
			}
		}
		else if ('e' == ch) {
			break;
		}
		printf("\n");
	}
}
char get_char()
{
#ifdef _WIN32
	return _getch();
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
int load_config(map<string, chl_param_t> *chl_map, const char *filename)
{
	int nRet = -1;
	if ((NULL == chl_map) || (NULL == filename)) {
		LOG("ERROR", "Parameter error, chl_map=%p,filename=%p\n", chl_map, filename);
		return nRet;
	}
	TiXmlDocument document;
	#if 1
	if (false == document.LoadFile(filename)) {
		LOG("ERROR", "Loading file failed '%s'\n", filename);
		return nRet;
	}
	#else
	if (NULL == document.Parse(xml)) {
		LOG("ERROR", "Parse xml failed '%s'\n\n", xml);
		return nRet;
	}
	#endif
	TiXmlNode* config = document.FirstChild("config");
	if (NULL != config) {
		TiXmlElement* channel = config->FirstChildElement("channel");
		if (NULL != channel) {
			TiXmlElement* item = channel->FirstChildElement("item");
			while (NULL != item) {
				bool enable = false;
				TiXmlElement* pElem = item->FirstChildElement("enable");
				if ((NULL != pElem) && (NULL != pElem->GetText()))
					enable = (0 == atoi(pElem->GetText())) ? false : true;
				if (enable) {
					chl_param_t chl;
					chl.status = 0;
					pElem = item->FirstChildElement("id");
					if ((NULL != pElem) && (NULL != pElem->GetText()))
						strncpy(chl.stream.id, pElem->GetText(), sizeof(chl.stream.id) - 1);
					TiXmlElement* input = item->FirstChildElement("input");
					if (NULL != input) {
						pElem = input->FirstChildElement("url");
						if ((NULL != pElem) && (NULL != pElem->GetText()))
							strncpy(chl.stream.input, pElem->GetText(), sizeof(chl.stream.input) - 1);
						pElem = input->FirstChildElement("device_name");
						if ((NULL != pElem) && (NULL != pElem->GetText()))
							strncpy(chl.stream.device_name, pElem->GetText(), sizeof(chl.stream.device_name) - 1);
						pElem = input->FirstChildElement("rtsp_transport");
						if ((NULL != pElem) && (NULL != pElem->GetText()))
							strncpy(chl.stream.rtsp_transport, pElem->GetText(), sizeof(chl.stream.rtsp_transport) - 1);
						pElem = input->FirstChildElement("stimeout");
						if ((NULL != pElem) && (NULL != pElem->GetText()))
							chl.stream.stimeout = atoi(pElem->GetText());
						pElem = input->FirstChildElement("buffer_size");
						if ((NULL != pElem) && (NULL != pElem->GetText()))
							chl.stream.buffer_size = atoi(pElem->GetText());
						///////////////////////////////////////////////////////////
						pElem = input->FirstChildElement("time_base");
						if ((NULL != pElem) && (NULL != pElem->GetText()))
							chl.stream.time_base = atoi(pElem->GetText());
						pElem = input->FirstChildElement("framerate");
						if ((NULL != pElem) && (NULL != pElem->GetText()))
							chl.stream.framerate = atoi(pElem->GetText());
						pElem = input->FirstChildElement("r_frame_rate");
						if ((NULL != pElem) && (NULL != pElem->GetText()))
							chl.stream.r_frame_rate = atoi(pElem->GetText());
						pElem = input->FirstChildElement("bit_rate");
						if ((NULL != pElem) && (NULL != pElem->GetText()))
							chl.stream.bit_rate = atoi(pElem->GetText());
						pElem = input->FirstChildElement("gop_size");
						if ((NULL != pElem) && (NULL != pElem->GetText()))
							chl.stream.gop_size = atoi(pElem->GetText());
						pElem = input->FirstChildElement("qmax");
						if ((NULL != pElem) && (NULL != pElem->GetText()))
							chl.stream.qmax = atoi(pElem->GetText());
						pElem = input->FirstChildElement("qmin");
						if ((NULL != pElem) && (NULL != pElem->GetText()))
							chl.stream.qmin = atoi(pElem->GetText());
						pElem = input->FirstChildElement("keyint_min");
						if ((NULL != pElem) && (NULL != pElem->GetText()))
							chl.stream.keyint_min = atoi(pElem->GetText());
						pElem = input->FirstChildElement("codec_tag");
						if ((NULL != pElem) && (NULL != pElem->GetText()))
							chl.stream.codec_tag = atoi(pElem->GetText());
						pElem = input->FirstChildElement("me_range");
						if ((NULL != pElem) && (NULL != pElem->GetText()))
							chl.stream.me_range = atoi(pElem->GetText());
						pElem = input->FirstChildElement("max_qdiff");
						if ((NULL != pElem) && (NULL != pElem->GetText()))
							chl.stream.max_qdiff = atoi(pElem->GetText());
						pElem = input->FirstChildElement("max_b_frames");
						if ((NULL != pElem) && (NULL != pElem->GetText()))
							chl.stream.max_b_frames = atoi(pElem->GetText());
						pElem = input->FirstChildElement("qcompress");
						if ((NULL != pElem) && (NULL != pElem->GetText()))
							chl.stream.qcompress = atof(pElem->GetText());
						pElem = input->FirstChildElement("qblur");
						if ((NULL != pElem) && (NULL != pElem->GetText()))
							chl.stream.qblur = atof(pElem->GetText());
						pElem = input->FirstChildElement("thread_count");
						if ((NULL != pElem) && (NULL != pElem->GetText()))
							chl.stream.thread_count = atof(pElem->GetText());
					}
					TiXmlElement *output = item->FirstChildElement("output");
					if (NULL != output) {
						pElem = output->FirstChildElement("output_format");
						if ((NULL != pElem) && (NULL != pElem->GetText()))
							strncpy(chl.stream.output_format, pElem->GetText(), sizeof(chl.stream.output_format) - 1);
						pElem = output->FirstChildElement("url");
						if ((NULL != pElem) && (NULL != pElem->GetText())) {
							// strncpy(param.output, pElem->GetText(), sizeof(param.output) - 1);
							if (0 == strcmp("flv", chl.stream.output_format))
								sprintf(chl.stream.output, "%s/%s", pElem->GetText(), chl.stream.id);
							else strncpy(chl.stream.output, pElem->GetText(), sizeof(chl.stream.output) - 1);
						}
					}
					chl_map->insert(map<string, chl_param_t>::value_type(chl.stream.id, chl));
				}
				item = item->NextSiblingElement("item");
			}
		}
		nRet = 0;
	}
	else LOG("ERROR", "Configuration file format is error %p\n", config);
	return nRet;
}
void print_config(map<string, chl_param_t> *chl_map)
{
	if (NULL != chl_map) {
		printf("---------------------------------------------------\n");
		if (chl_map->size() > 0) {
			//printf("|---------------------------\n");
			auto iter = chl_map->begin();
			for (; iter != chl_map->end(); iter++) {
				//printf("|\n");
				printf("|          id: %s\n", iter->second.stream.id);
				printf("|   input url: %s\n", iter->second.stream.input);
				printf("| device name: %s\n", iter->second.stream.device_name);
				printf("|  rtsp_trans: %s\n", iter->second.stream.rtsp_transport);
				printf("|    stimeout: %d\n", iter->second.stream.stimeout);
				printf("| buffer_size: %d\n", iter->second.stream.buffer_size);
				printf("|  output url: %s\n", iter->second.stream.output);
				printf("|  out format: %s\n", iter->second.stream.output_format);
				printf("|------------\n");
			}
		}
		else printf("| channel size: %d\n", chl_map->size());
		printf("---------------------------------------------------\n\n");
	}
}

#else  // COMPILE_EXE


#endif  // COMPILE_EXE