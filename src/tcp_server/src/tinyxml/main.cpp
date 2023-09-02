
#include <stdio.h>
#include "tinyxml.h"
#include <map>

using namespace std;

typedef struct multicast {
	char addr[16] = { 0 };		// 组播IP(如:"224.0.0.100")
	char interface[16] = { 0 };	// 网络接口(当机器有多个网卡时,需指定一个网卡,如:"192.168.1.105")
	bool enable = true;			// 是否启用
}multicast_t;

typedef struct config_t{
	char ip[16] = {0};
	int port = 0;
	map<string, multicast_t> multicast;// 组播列表
}config_t;

int load_config(config_t *cfg, const char *filename);
void print_config(config_t *cfg);


int main(int argc, char *argv[])
{
	const char *filename = "cfg.xml";
	if (2 == argc)
		filename = argv[1];

	config_t cfg;
	int nRet = load_config(&cfg, filename);
	if (0 == nRet)
		print_config(&cfg);
	else 
		printf("load config file failed %d\n\n", nRet);

	printf("Press any key to exit ...");
	getchar();
	return nRet;
}




/*
配置文件如下:
<?xml version="1.0" encoding="UTF-8" standalone="yes" ?>
<config>
	<ip>192.168.0.105</ip>
	<port>25060</port>
	<multicast num="1">
		<item>
			<addr>224.0.0.100</addr>
			<interface>192.168.0.105</interface>
			<enable>0</enable>
		</item>
	</multicast>
</config>
*/
int load_config(config_t *cfg, const char *filename)
{
	int nRet = -1;
	TiXmlDocument document;
#if 1
	if (false == document.LoadFile(filename)) {
		printf("File loading failed '%s'\n\n", filename);
		return nRet;
	}
#else
	if (NULL == document.Parse(xml)) {
		printf("Parse xml failed '%s'\n\n", xml);
		return nRet;
	}
#endif

	TiXmlNode *config = document.FirstChild("config");
	if (NULL != config) {
		if (NULL != cfg) {
			TiXmlElement *pElem = config->FirstChildElement("ip");
			if ((NULL != pElem) && (NULL != pElem->GetText()))
				strncpy(cfg->ip, pElem->GetText(), sizeof(cfg->ip) - 1);
			pElem = config->FirstChildElement("port");
			if ((NULL != pElem) && (NULL != pElem->GetText()))
				cfg->port = atoi(pElem->GetText());
			
			TiXmlElement *multicast = config->FirstChildElement("multicast");
			if (NULL != multicast) {
				TiXmlAttribute* attrib = multicast->FirstAttribute();
				if (NULL != attrib) {
					int num = attrib->IntValue();
				}
				TiXmlElement *item = multicast->FirstChildElement("item");
				while (NULL != item) {
					multicast_t m;
					TiXmlElement *pElem = item->FirstChildElement("addr");
					if ((NULL != pElem) && (NULL != pElem->GetText()))
						strncpy(m.addr, pElem->GetText(), sizeof(m.addr) - 1);
					pElem = item->FirstChildElement("interface");
					if ((NULL != pElem) && (NULL != pElem->GetText()))
						strncpy(m.interface, pElem->GetText(), sizeof(m.interface) - 1);
					pElem = item->FirstChildElement("enable");
					if ((NULL != pElem) && (NULL != pElem->GetText()))
						m.enable = (0 == atoi(pElem->GetText())) ? false : true;

					cfg->multicast.insert(map<string, multicast_t>::value_type(m.addr, m));
					item = item->NextSiblingElement("item");
				}
			}
			nRet = 0;
		}
	}
	else printf("Configuration file format is error.\n");
	return nRet;
}

void print_config(config_t *cfg)
{
	if (NULL != cfg) {
		printf("       ip: %s:%d\n", cfg->ip, cfg->port);
		auto iter = cfg->multicast.begin();
		for (; iter != cfg->multicast.end(); iter++) {
			printf("     addr: %s\n", iter->second.addr);
			printf("interface: %s\n", iter->second.interface);
			printf("   enable: %d\n", iter->second.enable);
			printf("\n");
		}
	}
}