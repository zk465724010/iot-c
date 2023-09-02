
#include "parse_xml.h"
#include "tinyxml.h"
#include <time.h>
//#include <sstream>		// stringstream
#include "log.h"

//#ifdef _WIN32
//	#define short_file_name(x) strrchr(x,'\\') ? strrchr(x,'\\')+1 : x
//	#define DEBUG_(...) (fprintf(stderr, "[%s %s %d]: ", short_file_name(__FILE__), __FUNCTION__, __LINE__),fprintf(stderr,__VA_ARGS__))
//#elif __linux__
//	#define short_file_name(x) strrchr(x,'/') ? strrchr(x,'/')+1 : x
//	#define DEBUG_(...) (fprintf(stderr, "[%s %s %d]: ", short_file_name(__FILE__), __FUNCTION__, __LINE__),fprintf(stderr,__VA_ARGS__))
//#endif

int parse_device_info(const char *xml)
{
	#if 0
		TiXmlDocument document;
		bool bRet = document.LoadFile(filename);
		if (!bRet)
			return -1;
	#else
		TiXmlDocument document;
		document.Parse(xml);
	#endif
	TiXmlNode *pRootNode = document.FirstChild("config");
	if (NULL != pRootNode)
	{
        TiXmlElement *pElem = pRootNode->FirstChildElement("device");
    }
	return 0;
}

// 解析SIP消息的消息体
#if 1
int parse_sip_body(const char *xml,  dev_info_t *body, const dev_info_t *dev)
{
    int nRet = -1;
	if((NULL == xml) || (NULL == body))
		return nRet;

    TiXmlDocument document;
	document.Parse(xml);
	TiXmlNode *pRootNode = NULL;
    #if 0
        static char opt[4][16] = {"Query", "Control", "Response", "Notify"};
        for(int i=0; i<4; i++)
        {
            pRootNode = document.FirstChild(opt[i]);
            if(NULL != pRootNode)
                break;
        }
    #else
        pRootNode = document.FirstChild();
        if(NULL != pRootNode) {
            pRootNode = pRootNode->NextSibling();   // 忽略首行
        }
    #endif
	if(NULL != pRootNode)
	{
		//LOG("INFO","[%s]\n", pRootNode->Value());		// "Query", "Control", "Response", "Notify"
        TiXmlElement *pSubElem = pRootNode->FirstChildElement("CmdType");
        if((NULL != pSubElem) && (NULL != pSubElem->GetText())) {
			const char *cmd_type = pSubElem->GetText();
			if(0 == strcmp("DeviceInfo", cmd_type))
				body->cmd_type = QUERY_DEV_INFO;
			else if(0 == strcmp("DeviceStatus", cmd_type))
				body->cmd_type = QUERY_DEV_STATUS;
			else if(0 == strcmp("Catalog", cmd_type))
				body->cmd_type = QUERY_DEV_CATALOG;
			else if(0 == strcmp("RecordInfo", cmd_type))
				body->cmd_type = QUERY_DEV_RECORD;
			else if(0 == strcmp("ConfigDownload", cmd_type))
				body->cmd_type = QUERY_DEV_CFG;
			else if(0 == strcmp("Keepalive", cmd_type))
				body->cmd_type = NOTIFY_KEEPALIVE;
			else if(0 == strcmp("MediaStatus", cmd_type))
				body->cmd_type = NOTIFY_MEDIA_STATUS;
			else if (0 == strcmp("DeviceConfig", cmd_type))
				body->cmd_type = CTRL_DEV_CFG;
			else if (0 == strcmp("DeviceControl", cmd_type))
				body->cmd_type = CTRL_DEV_CTRL;
			//else if (0 == strcmp("Alarm", cmd_type))
			//	dev->cmd_type = 0;
			else LOG("WARN","Unknown CmdType '%s'\n\n", cmd_type ? cmd_type : "null");
		}
		else LOG("ERROR","CmdType=%p, %p\n\n", pSubElem, pSubElem->GetText());
        pSubElem = pRootNode->FirstChildElement("SN");
        if((NULL != pSubElem) && (NULL != pSubElem->GetText()))
			body->sn = atoi(pSubElem->GetText());
		TiXmlElement *pDeviceID = pRootNode->FirstChildElement("DeviceID");
		if((NULL != pDeviceID) && (NULL != pDeviceID->GetText())) {
			// 注意: 当给通道发送'Catlog'时,此处为通道的ID,而非设备ID
			strncpy(body->addr.id, pDeviceID->GetText(), sizeof(body->addr.id)-1);
			nRet = 0;
		}
		if(QUERY_DEV_INFO == body->cmd_type) {
			TiXmlElement *pElem = pRootNode->FirstChildElement("DeviceName");
			if((NULL != pElem) && (NULL != pElem->GetText()))
				strncpy(body->name, pElem->GetText(), sizeof(body->name)-1);
			pElem = pRootNode->FirstChildElement("Manufacturer");
			if((NULL != pElem) && (NULL != pElem->GetText()))
				strncpy(body->manufacturer, pElem->GetText(), sizeof(body->manufacturer)-1);
			pElem = pRootNode->FirstChildElement("Model");
			if((NULL != pElem) && (NULL != pElem->GetText()))
				strncpy(body->model, pElem->GetText(), sizeof(body->model)-1);
			pElem = pRootNode->FirstChildElement("Firmware");
			if((NULL != pElem) && (NULL != pElem->GetText()))
				strncpy(body->firmware, pElem->GetText(), sizeof(body->firmware)-1);
            pElem = pRootNode->FirstChildElement("Channel");
			if((NULL != pElem) && (NULL != pElem->GetText()))
				body->channels = atoi(pElem->GetText());
		}
		else if(QUERY_DEV_STATUS == body->cmd_type) {
			if(NULL == body->dev_status)
				body->dev_status = new status_t();
			TiXmlElement *pElem = pRootNode->FirstChildElement("Online");
			if((NULL != pElem) && (NULL != pElem->GetText())) {
				if(0 == strcmp("ONLINE", pElem->GetText()))
					body->dev_status->online = 1;	// 设备状态 (0:离线 1:在线)
				else body->dev_status->online = 0;
			}
            pElem = pRootNode->FirstChildElement("Status");
			if((NULL != pElem) && (NULL != pElem->GetText())) {
                if(0 == strcmp("OK", pElem->GetText()))
					body->dev_status->status = 0;   // 0:OK 1:ERROR
                else body->dev_status->status = 1;
            }
            pElem = pRootNode->FirstChildElement("DeviceTime");
			if((NULL != pElem) && (NULL != pElem->GetText()))
                strncpy(body->dev_status->time, pElem->GetText(), sizeof(body->dev_status->time)-1);
            
            pElem = pRootNode->FirstChildElement("Encode");
			if((NULL != pElem) && (NULL != pElem->GetText())) {
                if(0 == strcmp("ON", pElem->GetText()))
					body->dev_status->encode = 1;   // 0:OFF 1:ON
                else body->dev_status->encode = 0;
            }
            pElem = pRootNode->FirstChildElement("Record");
			if((NULL != pElem) && (NULL != pElem->GetText())) {
                if(0 == strcmp("ON", pElem->GetText()))
					body->dev_status->record = 1;   // 0:OFF 1:ON
                else body->dev_status->record = 0;
            }
		}
		else if(QUERY_DEV_CATALOG == body->cmd_type) {
			TiXmlElement *pSumNum = pRootNode->FirstChildElement("SumNum");
			if((NULL != pSumNum) && (NULL != pSumNum->GetText()))
				body->sum_num = atoi(pSumNum->GetText());
			TiXmlElement *pDevList = pRootNode->FirstChildElement("DeviceList");
			if(NULL != pDevList) {
				TiXmlAttribute* pDevLstAttr = pDevList->FirstAttribute();
				if(NULL != pDevLstAttr)
					body->num = pDevLstAttr->IntValue();
                TiXmlElement *pItem = pDevList->FirstChildElement("Item");
				while(NULL != pItem) {
					chl_info_t chl;
					TiXmlElement *pElem = pItem->FirstChildElement("DeviceID");	// 通道ID
					if((NULL != pElem) && (NULL != pElem->GetText())) 
						strncpy(chl.id, pElem->GetText(), sizeof(chl.id)-1);
                    pElem = pItem->FirstChildElement("Parental");
					if((NULL != pElem) && (NULL != pElem->GetText())) 
						chl.parental = atoi(pElem->GetText());
					else { // 过滤掉目录
						pItem = pItem->NextSiblingElement("Item");
						continue;
					}
					pElem = pItem->FirstChildElement("ParentID");		// 直连摄像头时无该字段(两级)
					if((NULL != pElem) && (NULL != pElem->GetText()))
						strncpy(chl.parent_id, pElem->GetText(), sizeof(chl.parent_id)-1);
					else strncpy(chl.parent_id, dev->addr.id, sizeof(chl.parent_id) - 1);
					pElem = pItem->FirstChildElement("Name");
					if((NULL != pElem) && (NULL != pElem->GetText()))
						strncpy(chl.name, pElem->GetText(), sizeof(chl.name)-1);
					pElem = pItem->FirstChildElement("Manufacturer");
					if((NULL != pElem) && (NULL != pElem->GetText()))
						strncpy(chl.manufacturer, pElem->GetText(), sizeof(chl.manufacturer)-1);
					pElem = pItem->FirstChildElement("Model");
					if((NULL != pElem) && (NULL != pElem->GetText()))
						strncpy(chl.model, pElem->GetText(), sizeof(chl.model)-1);
					pElem = pItem->FirstChildElement("Owner");
					if((NULL != pElem) && (NULL != pElem->GetText()))
						strncpy(chl.owner, pElem->GetText(), sizeof(chl.owner)-1);
					pElem = pItem->FirstChildElement("CivilCode");
					if((NULL != pElem) && (NULL != pElem->GetText()))
						strncpy(chl.civil_code, pElem->GetText(), sizeof(chl.civil_code)-1);
					pElem = pItem->FirstChildElement("Address");
					if ((NULL != pElem) && (NULL != pElem->GetText()))
						strncpy(chl.address, pElem->GetText(), sizeof(chl.address) - 1);
					pElem = pItem->FirstChildElement("IPAddress");
					if ((NULL != pElem) && (NULL != pElem->GetText()))
						strncpy(chl.ip_address, pElem->GetText(), sizeof(chl.ip_address) - 1);
					else strncpy(chl.ip_address, dev->addr.ip, sizeof(chl.ip_address) - 1);
					pElem = pItem->FirstChildElement("Port");
					if ((NULL != pElem) && (NULL != pElem->GetText()))
						chl.port = atoi(pElem->GetText());
					else chl.port = dev->addr.port;
					pElem = pItem->FirstChildElement("SafetyWay");
					if ((NULL != pElem) && (NULL != pElem->GetText()))
						chl.safety_way = atoi(pElem->GetText());
					pElem = pItem->FirstChildElement("RegisterWay");
					if((NULL != pElem) && (NULL != pElem->GetText())) 
						chl.register_way = atoi(pElem->GetText());
					pElem = pItem->FirstChildElement("Secrecy");
					if((NULL != pElem) && (NULL != pElem->GetText())) 
						chl.secrecy = atoi(pElem->GetText());
					pElem = pItem->FirstChildElement("Status");
					if ((NULL != pElem) && (NULL != pElem->GetText())) {
						if (0 == strcmp("ON", pElem->GetText()))
							chl.status = 1;         // 0:OFF 1:ON
						else chl.status = 0;
					}
					pElem = pItem->FirstChildElement("Longitude");// 经度
					if ((NULL != pElem) && (NULL != pElem->GetText()))
						chl.longitude = atof(pElem->GetText());
					pElem = pItem->FirstChildElement("Latitude");// 纬度
					if ((NULL != pElem) && (NULL != pElem->GetText()))
						chl.latitude = atof(pElem->GetText());
                    string key = string(chl.parent_id) + string("#") + string(chl.id);
					body->chl_map.insert(map<string, chl_info_t>::value_type(key, chl));
					//dev->chl_map.insert(map<string, chl_info_t>::value_type(chl.id, chl));
					pItem = pItem->NextSiblingElement("Item");
				}
				body->channels = body->chl_map.size();
			}
		}
		else if(QUERY_DEV_RECORD == body->cmd_type) { // 录像查询
			if (0 == strcmp("Query", pRootNode->Value())) {
				record_info_t rec;
				if ((NULL != pDeviceID) && (NULL != pDeviceID->GetText()))
					strncpy(rec.id, pDeviceID->GetText(), sizeof(rec.id) - 1);
				TiXmlElement* pElem = pRootNode->FirstChildElement("StartTime");
				if ((NULL != pElem) && (NULL != pElem->GetText()))
					strncpy(rec.start_time, pElem->GetText(), sizeof(rec.start_time)-1);
				pElem = pRootNode->FirstChildElement("EndTime");
				if ((NULL != pElem) && (NULL != pElem->GetText()))
					strncpy(rec.end_time, pElem->GetText(), sizeof(rec.end_time) - 1);
				pElem = pRootNode->FirstChildElement("FilePath");
				if ((NULL != pElem) && (NULL != pElem->GetText()))
					strncpy(rec.file_path, pElem->GetText(), sizeof(rec.file_path) - 1);
				pElem = pRootNode->FirstChildElement("Address");
				if ((NULL != pElem) && (NULL != pElem->GetText()))
					strncpy(rec.address, pElem->GetText(), sizeof(rec.address) - 1);
				pElem = pRootNode->FirstChildElement("Secrecy");
				if ((NULL != pElem) && (NULL != pElem->GetText()))
					rec.secrecy = atoi(pElem->GetText());
				pElem = pRootNode->FirstChildElement("Type");
				if ((NULL != pElem) && (NULL != pElem->GetText())) {
					// 录像类型(1:all 2:manual(手动录像) 3:time 4:alarm )
					if (0 == strcmp("all", pElem->GetText()))
						rec.type = 1;
					else if (0 == strcmp("manual", pElem->GetText()))
						rec.type = 2;
					else if (0 == strcmp("time", pElem->GetText()))
						rec.type = 3;
					else if (0 == strcmp("alarm", pElem->GetText()))
						rec.type = 4;
					else rec.type = -1;
				}
				pElem = pRootNode->FirstChildElement("RecorderID");
				if ((NULL != pElem) && (NULL != pElem->GetText()))
					strncpy(rec.recorder_id, pElem->GetText(), sizeof(rec.recorder_id) - 1);
				body->rec_map.insert(map<string, record_info_t>::value_type(rec.start_time, rec));
			}
		}
		else if((QUERY_DEV_CFG == body->cmd_type) || (CTRL_DEV_CFG == body->cmd_type)) {
			if(NULL == body->cfg)
				body->cfg = new config_t();
			TiXmlElement* pBasicParam = pRootNode->FirstChildElement("BasicParam");
			if(NULL != pBasicParam) {
				TiXmlElement* pElem = pBasicParam->FirstChildElement("Name");
				if((NULL != pElem) && (NULL != pElem->GetText()))
					strncpy(body->name, pElem->GetText(), sizeof(body->name)-1);
				pElem = pBasicParam->FirstChildElement("Expiration");
				if((NULL != pElem) && (NULL != pElem->GetText()))
					body->cfg->basic.expires = atoi(pElem->GetText());
				pElem = pBasicParam->FirstChildElement("HeartBeatInterval");
				if((NULL != pElem) && (NULL != pElem->GetText()))
					body->cfg->basic.keepalive = atoi(pElem->GetText());
				pElem = pBasicParam->FirstChildElement("HeartBeatCount");
				if((NULL != pElem) && (NULL != pElem->GetText()))
					body->cfg->basic.max_k_timeout = atoi(pElem->GetText());
				pElem = pBasicParam->FirstChildElement("PositionCapability");
				if((NULL != pElem) && (NULL != pElem->GetText()))
					body->cfg->basic.position_capability = atoi(pElem->GetText());
			}
		}
		else if(NOTIFY_KEEPALIVE == body->cmd_type) {
		}
		else if(NOTIFY_MEDIA_STATUS == body->cmd_type) {	// "MediaStatus"
		}
		//else DEBUG("INFO: 未处理的的消息 <CmdType>%s</CmdType> \n\n", sip->cmd_type);
	}
	return nRet;
}
#else
int parse_sip_body(const char *xml, const addr_info_t *dev,  sip_body_t *bady)
{
    int nRet = -1;
	if((NULL == xml) || (NULL == dev))
		return nRet;

    TiXmlDocument document;
	document.Parse(xml);
	TiXmlNode *pRootNode = NULL;
    #if 0
        static char opt[4][16] = {"Query", "Control", "Response", "Notify"};
        for(int i=0; i<4; i++) {
            pRootNode = document.FirstChild(opt[i]);
            if(NULL != pRootNode)
                break;
        }
    #else
        pRootNode = document.FirstChild();
        if(NULL != pRootNode) {
            pRootNode = pRootNode->NextSibling();   // 忽略首行
        }
    #endif

	if(NULL != pRootNode)
	{
		LOG("INFO", "opt: [%s]\n\n\n\n", pRootNode->Value());
		strncpy(bady->opt, pRootNode->Value(), sizeof(bady->opt)-1);
        TiXmlElement *pSubElem = pRootNode->FirstChildElement("CmdType");
        if((NULL != pSubElem) && (NULL != pSubElem->GetText()))
		{
			const char *cmd_type = pSubElem->GetText();
			if(0 == strcmp("DeviceInfo", cmd_type))
				bady->cmd_type = QUERY_DEV_INFO;
			else if(0 == strcmp("DeviceStatus", cmd_type))
				bady->cmd_type = QUERY_DEV_STATUS;
			else if(0 == strcmp("Catalog", cmd_type))
				bady->cmd_type = QUERY_DEV_CATALOG;
			else if(0 == strcmp("RecordInfo", cmd_type))
				bady->cmd_type = QUERY_DEV_RECORD;
			else if(0 == strcmp("ConfigDownload", cmd_type))
				bady->cmd_type = QUERY_DEV_CFG;
			else if(0 == strcmp("Keepalive", cmd_type))
				bady->cmd_type = NOTIFY_KEEPALIVE;
			else if(0 == strcmp("MediaStatus", cmd_type))
				bady->cmd_type = NOTIFY_MEDIA_STATUS;
			else if(0 == strcmp("DeviceConfig", cmd_type))
				bady->cmd_type = CTRL_DEV_CFG;
		}
        pSubElem = pRootNode->FirstChildElement("SN");
        if((NULL != pSubElem) && (NULL != pSubElem->GetText()))
            bady->sn = atoi(pSubElem->GetText());

		TiXmlElement *pDeviceID = pRootNode->FirstChildElement("DeviceID");
		if((NULL != pDeviceID) && (NULL != pDeviceID->GetText())) {
			strncpy(bady->id, pDeviceID->GetText(), sizeof(bady->id)-1);	// 注意: 当给通道发送'Catlog'时,此处为通道的ID,而非设备ID
			nRet = 0;
		}

		if(QUERY_DEV_INFO == bady->cmd_type)
		{
			if(NULL == bady->dev_info)
				bady->dev_info = new response_dev_info_t();
			TiXmlElement *pElem = pRootNode->FirstChildElement("DeviceName");
			if((NULL != pElem) && (NULL != pElem->GetText()))
				strncpy(bady->dev_info->name, pElem->GetText(), sizeof(bady->dev_info->name)-1);
			pElem = pRootNode->FirstChildElement("Manufacturer");
			if((NULL != pElem) && (NULL != pElem->GetText()))
				strncpy(bady->dev_info->manufacturer, pElem->GetText(), sizeof(bady->dev_info->manufacturer)-1);
			pElem = pRootNode->FirstChildElement("Model");
			if((NULL != pElem) && (NULL != pElem->GetText()))
				strncpy(bady->dev_info->model, pElem->GetText(), sizeof(bady->dev_info->model)-1);
			pElem = pRootNode->FirstChildElement("Firmware");
			if((NULL != pElem) && (NULL != pElem->GetText()))
				strncpy(bady->dev_info->firmware, pElem->GetText(), sizeof(bady->dev_info->firmware)-1);
            pElem = pRootNode->FirstChildElement("Channel");
			if((NULL != pElem) && (NULL != pElem->GetText()))
                bady->dev_info->channels = atoi(pElem->GetText());
		}
		else if(QUERY_DEV_STATUS == bady->cmd_type)
		{
			if(NULL == bady->dev_status)
				bady->dev_status = new response_dev_status_t();

			TiXmlElement *pElem = pRootNode->FirstChildElement("Online");
			if((NULL != pElem) && (NULL != pElem->GetText())) {
				if(0 == strcmp("ONLINE", pElem->GetText()))
					bady->dev_status->online = 1;	// 设备状态 (0:离线 1:在线)
				else bady->dev_status->online = 0;
			}
            pElem = pRootNode->FirstChildElement("Status");
			if((NULL != pElem) && (NULL != pElem->GetText())) {
                if(0 == strcmp("OK", pElem->GetText()))
                    bady->dev_status->status = 0;   // 0:OK 1:ERROR
                else bady->dev_status->status = 1;
            }
            pElem = pRootNode->FirstChildElement("DeviceTime");
			if((NULL != pElem) && (NULL != pElem->GetText()))
                strncpy(bady->dev_status->time, pElem->GetText(), sizeof(bady->dev_status->time)-1);
            
            pElem = pRootNode->FirstChildElement("Encode");
			if((NULL != pElem) && (NULL != pElem->GetText())) {
                if(0 == strcmp("ON", pElem->GetText()))
                    bady->dev_status->encode = 1;   // 0:OFF 1:ON
                else bady->dev_status->encode = 0;
            }
            pElem = pRootNode->FirstChildElement("Record");
			if((NULL != pElem) && (NULL != pElem->GetText())) {
                if(0 == strcmp("ON", pElem->GetText()))
                    bady->dev_status->record = 1;   // 0:OFF 1:ON
                else bady->dev_status->record = 0;
            }
		}
		else if(QUERY_DEV_CATALOG == bady->cmd_type)
		{
			if(NULL == bady->catalog)
				bady->catalog = new response_catalog_t();
			TiXmlElement *pSumNum = pRootNode->FirstChildElement("SumNum");
			if((NULL != pSumNum) && (NULL != pSumNum->GetText()))
				bady->catalog->sum_num = atoi(pSumNum->GetText());

			TiXmlElement *pDevList = pRootNode->FirstChildElement("DeviceList");
			if(NULL != pDevList)
			{
				TiXmlAttribute* pDevLstAttr = pDevList->FirstAttribute();
				if(NULL != pDevLstAttr)
					bady->catalog->num = pDevLstAttr->IntValue();
				
                TiXmlElement *pItem = pDevList->FirstChildElement("Item");
				while(NULL != pItem)
				{
					chl_info_t chl;
					TiXmlElement *pElem = pItem->FirstChildElement("DeviceID");	// 通道ID
					if((NULL != pElem) && (NULL != pElem->GetText())) 
						strncpy(chl.id, pElem->GetText(), sizeof(chl.id)-1);
                    pElem = pItem->FirstChildElement("Parental");
					if((NULL != pElem) && (NULL != pElem->GetText())) 
						chl.parental = atoi(pElem->GetText());
					pElem = pItem->FirstChildElement("ParentID");		// 直连摄像头时无该字段(两级)
					if(NULL != pElem)
                    {
                        if(NULL != pElem->GetText()) {
                             strncpy(chl.parent_id, pElem->GetText(), sizeof(chl.parent_id)-1);
                        }
                        #if 0
                        else {
                            if(1 == chl.parental)                               // 是否为父设备(1:是 0:否)
                                strcpy(chl.parent_id, "34020000002000000001");  // 置为GB28181服务的ID
                            else strcpy(chl.parent_id, sip->id);                // 置为通道所属设备的ID (注意:此处可能会有问题)
                        }
                        #endif     
                    }
					pElem = pItem->FirstChildElement("Name");
					if((NULL != pElem) && (NULL != pElem->GetText()))
						strncpy(chl.name, pElem->GetText(), sizeof(chl.name)-1);
					pElem = pItem->FirstChildElement("Manufacturer");
					if((NULL != pElem) && (NULL != pElem->GetText()))
						strncpy(chl.manufacturer, pElem->GetText(), sizeof(chl.manufacturer)-1);
					pElem = pItem->FirstChildElement("Model");
					if((NULL != pElem) && (NULL != pElem->GetText()))
						strncpy(chl.model, pElem->GetText(), sizeof(chl.model)-1);
					pElem = pItem->FirstChildElement("Owner");
					if((NULL != pElem) && (NULL != pElem->GetText()))
						strncpy(chl.owner, pElem->GetText(), sizeof(chl.owner)-1);
					pElem = pItem->FirstChildElement("CivilCode");
					if((NULL != pElem) && (NULL != pElem->GetText()))
						strncpy(chl.civil_code, pElem->GetText(), sizeof(chl.civil_code)-1);
					pElem = pItem->FirstChildElement("Address");
					if((NULL != pElem) && (NULL != pElem->GetText()))
						strncpy(chl.address, pElem->GetText(), sizeof(chl.address)-1);
					pElem = pItem->FirstChildElement("SafetyWay");
					if((NULL != pElem) && (NULL != pElem->GetText()))
						chl.safety_way = atoi(pElem->GetText());
					pElem = pItem->FirstChildElement("RegisterWay");
					if((NULL != pElem) && (NULL != pElem->GetText())) 
						chl.register_way = atoi(pElem->GetText());
					pElem = pItem->FirstChildElement("Secrecy");
					if((NULL != pElem) && (NULL != pElem->GetText())) 
						chl.secrecy = atoi(pElem->GetText());
					pElem = pItem->FirstChildElement("Status");
					if((NULL != pElem) && (NULL != pElem->GetText()))
                    {
                        if(0 == strcmp("ON", pElem->GetText()))
                            chl.status = 1;         // 0:OFF 1:ON
                        else chl.status = 0;
                    }
                    //string key = string(chl.parent_id) + string("#") + string(chl.id);
					//dev->chl_map.insert(map<string, chl_info_t>::value_type(chl.id, chl));
					bady->catalog->chl_map.insert(map<string, chl_info_t>::value_type(chl.id, chl));
					pItem = pItem->NextSiblingElement("Item");
				}
			}
		}
		else if(QUERY_DEV_RECORD == bady->cmd_type)
		{
			
		}
		else if((QUERY_DEV_CFG == bady->cmd_type) || (CTRL_DEV_CFG == bady->cmd_type))
		{
			#if 0
			if(NULL == dev->cfg)
                dev->cfg = new config_t();
			TiXmlElement* pBasicParam = pRootNode->FirstChildElement("BasicParam");
			if(NULL != pBasicParam)
			{
				TiXmlElement* pElem = pBasicParam->FirstChildElement("Name");
				if((NULL != pElem) && (NULL != pElem->GetText()))
					strncpy(dev->name, pElem->GetText(), sizeof(dev->name)-1);
				pElem = pBasicParam->FirstChildElement("Expiration");
				if((NULL != pElem) && (NULL != pElem->GetText()))
					dev->cfg->basic.expires = atoi(pElem->GetText());
				pElem = pBasicParam->FirstChildElement("HeartBeatInterval");
				if((NULL != pElem) && (NULL != pElem->GetText()))
					dev->cfg->basic.keepalive = atoi(pElem->GetText());
				pElem = pBasicParam->FirstChildElement("HeartBeatCount");
				if((NULL != pElem) && (NULL != pElem->GetText()))
					dev->cfg->basic.max_k_timeout = atoi(pElem->GetText());
				pElem = pBasicParam->FirstChildElement("PositionCapability");
				if((NULL != pElem) && (NULL != pElem->GetText()))
					dev->cfg->basic.position_capability = atoi(pElem->GetText());
			}
			#endif
		}
		else if(NOTIFY_KEEPALIVE == bady->cmd_type)
		{
		}
		else if(NOTIFY_MEDIA_STATUS == bady->cmd_type)	// "MediaStatus"
		{
		}
		//else DEBUG("INFO: 未处理的的消息 <CmdType>%s</CmdType> \n\n", sip->cmd_type);
	}
	return nRet;
}

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

string make_response(const char *cmd_type, sip_body_t *resp_body)
{
	stringstream xml;
	//xml << "<?xml version=\"1.0\" encoding=\"GB2312\"?>\n";
	xml << "<?xml version=\"1.0\"?>\n";
	xml << "<Response>\n";
	xml << "  <CmdType>" << cmd_type << "</CmdType>\n";
	xml << "  <SN>" << resp_body->sn << "</SN>\n";
	xml << "  <DeviceID>"<< resp_body->id << "</DeviceID>\n";
	if(0 == strcmp(cmd_type, "DeviceInfo"))         // 查询设备信息
	{
		if(NULL != resp_body->dev_info)
		{
			xml << "  <Result>OK</Result>\n";
			xml << "  <DeviceName>" << resp_body->dev_info->name << "</DeviceName>\n";
			xml << "  <Manufacturer>" << resp_body->dev_info->manufacturer << "</Manufacturer>\n";
			xml << "  <Model>" << resp_body->dev_info->model << "</Model>\n";
			xml << "  <Firmware>" << resp_body->dev_info->firmware << "</Firmware>\n";
			xml << "  <Channel>" << resp_body->dev_info->channels << "</Channel>\n";
		}
	}
	else if(0 == strcmp(cmd_type, "Catalog"))       // 查询设备目录
	{
		if(NULL != resp_body->catalog)
		{
			xml << "  <SumNum>" << resp_body->catalog->sum_num << "</SumNum>\n";
			xml << "  <DeviceList Num=\"" << resp_body->catalog->num << "\">\n";
			auto chl = resp_body->catalog->chl_map.begin();
			for(; chl != resp_body->catalog->chl_map.end(); ++chl)
			{
				xml << "    <Item>\n";
				xml << "      <DeviceID>" << chl->second.id << "</DeviceID>\n";
				xml << "      <Name>" << chl->second.name << "</Name>\n";
				xml << "      <Manufacturer>" << chl->second.manufacturer << "</Manufacturer>\n";
				xml << "      <Model>"<< chl->second.model << "</Model>\n";
				xml << "      <Owner>" << chl->second.owner << "</Owner>\n";
				xml << "      <CivilCode>" << chl->second.civil_code << "</CivilCode>\n";
				xml << "      <Address>" << chl->second.address << "</Address>\n";
				xml << "      <Parental>" << (int)chl->second.parental << "</Parental>\n";		// 是否有子设备(必选) 1 有, 0 没有 
				xml << "      <ParentID>" << chl->second.parent_id << "</ParentID>\n";
				xml << "      <SafetyWay>" << (int)chl->second.safety_way << "</SafetyWay>\n";
				xml << "      <RegisterWay>" << (int)chl->second.register_way << "</RegisterWay>\n";
				xml << "      <Secrecy>" << (int)chl->second.secrecy << "</Secrecy>\n";
				xml << "      <Status>" << ((1==chl->second.status)?"ON":"OFF") << "</Status>\n";
				xml << "      <IPAddress>" << chl->second.ip_address << "</IPAddress>\n";
				xml << "      <Port>" << chl->second.port << "</Port>\n";
				xml << "    </Item>\n";
			}
			xml << "  </DeviceList>\n";
		}
	}
	else if(0 == strcmp(cmd_type, "DeviceStatus"))   // 查询设备状态
	{
		if(NULL != resp_body->dev_status)
		{
			xml << "  <Result>OK</Result>\n";
			xml << "  <Online>" << ((1 == resp_body->dev_status->online)?"ONLINE":"OFFLINE") << "</Online>\n";
			xml << "  <Status>" << ((0 == resp_body->dev_status->status)?"OK":"ERROR") << "</Status>\n";
			xml << "  <DeviceTime>" << resp_body->dev_status->time << "</DeviceTime>\n";
			xml << "  <Alarmstatus Num=\"0\">\n";
			xml << "  </Alarmstatus>\n";
			xml << "  <Encode>" << ((1 == resp_body->dev_status->encode)?"ON":"OFF") << "</Encode>\n";
			xml << "  <Record>" << ((1 == resp_body->dev_status->record)?"ON":"OFF") << "</Record>\n";
		}
	}
	else if(0 == strcmp(cmd_type, "RecordInfo"))     // 查询录像
	{
	}
	else if(0 == strcmp(cmd_type, "ConfigDownload"))  // 查询设备配置
	{
		if(NULL != resp_body->basic_param)
		{
			
		}
	}
	xml << "</Response>\n";
	return xml.str();
}
#endif

string timestamp_to_string(time_t time, char *buf, int buf_size, char ch/*='-'*/)
{
#if 1
	//time_t tick = time(NULL);
	struct tm *pTime = localtime(&time);
	if (ch == '-')
		strftime(buf, buf_size, "%Y-%m-%dT%H:%M:%S", pTime);
	else if (ch == '/')
		strftime(buf, buf_size, "%Y/%m/%d %H:%M:%S", pTime);
	else if (ch == '.')
		strftime(buf, buf_size, "%Y.%m.%d %H:%M:%S", pTime);
	else strftime(buf, buf_size, "%Y%m%d%H%M%S", pTime);
#else
	struct tm tm1;
	localtime_r(&time, &tm1);
	sprintf(buf, "%4.4d%2.2d%2.2d%2.2d%2.2d%2.2d", tm1.tm_year + 1900, tm1.tm_mon + 1, tm1.tm_mday, tm1.tm_hour, tm1.tm_min, tm1.tm_sec);
#endif
	return string(buf);
}


/////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////
#ifdef USE_TEST_XML
void print_device_info(dev_info_t *dev)
{
	if (NULL == dev) {
		LOG("ERROR", "dev=%p\n\n", dev);
		return;
	}
    LOG("INFO","------------- device:%s -------------\n", dev->addr.ip);
    LOG("INFO", "          id: %s\n", dev->addr.id);
	LOG("INFO", "   parent_id: %s\n", dev->parent_id);
    LOG("INFO", "          ip: %s:%d\n", dev->addr.ip, dev->addr.port);
    LOG("INFO", "    platform: %s\n", dev->platform);
    LOG("INFO", "        type: %d\n", dev->type);
    LOG("INFO", "    protocol: %d\n", dev->protocol);
    LOG("INFO", "    username: %s:%s\n", dev->username, dev->password);
    LOG("INFO", "      status: %d (0:offline 1:online)\n", dev->status);
    LOG("INFO", "     expires: %d (%d)\n", dev->expires, dev->register_interval);
    LOG("INFO", "   keepalive: %d (max:%d)\n", dev->keepalive, dev->max_k_timeout);
    LOG("INFO", "        name: %s\n", dev->name);
    LOG("INFO", "manufacturer: %s (%s %s)\n", dev->manufacturer, dev->firmware, dev->model);
	LOG("INFO", "    channels: %d\n", dev->chl_map.size());
	LOG("INFO", "        auth: %d\n", dev->auth);
	LOG("INFO", "       share: %d\n", dev->share);
	int i = 1;
	auto chl = dev->chl_map.begin();
	for(; chl != dev->chl_map.end(); chl++) {
		LOG("INFO", "-------------\n");
		LOG("INFO", "    channel[%d]: %s\n", i, chl->first.c_str());
		//printf("   channel[%d]: %s\n", i++, chl->first.c_str());
		print_channel_info(&chl->second);
		i++;
	}
    LOG("INFO", "\n\n");
}

void print_device_map(CMap<string, dev_info_t> *dev_map)
{
	if(NULL != dev_map) {
		auto dev = dev_map->begin();
		for(; dev != dev_map->end(); dev++) {
			print_device_info(&dev->second);
		}
	}
	else {
		LOG("ERROR", "dev_map=%p\n\n", dev_map);
	}
}

void print_channel_info(chl_info_t *chl)
{
	if (NULL == chl) {
		LOG("ERROR", "chl=%p\n\n", chl);
		return;
	}
	LOG("INFO", "          id: %s\n", chl->id);
	LOG("INFO", "   parent_id: %s\n", chl->parent_id);
	LOG("INFO", "        name: %s\n", chl->name);
	LOG("INFO", "    parental: %d\n", chl->parental);
	LOG("INFO", "      status: %d\n", chl->status);
	LOG("INFO", "manufacturer: %s\n", chl->manufacturer);
	LOG("INFO", "       model: %s\n", chl->model);
	LOG("INFO", "       owner: %s\n", chl->owner);
	LOG("INFO", "  civil_code: %s\n", chl->civil_code);
	LOG("INFO", "     address: %s\n", chl->address);
	LOG("INFO", "  ip_address: %s\n", chl->ip_address);
	LOG("INFO", "        port: %d\n", chl->port);
	LOG("INFO", "register_way: %d\n", chl->register_way);
	LOG("INFO", "  safety_way: %d\n", chl->safety_way);
	LOG("INFO", "     secrecy: %d\n", chl->secrecy);
	LOG("INFO", "   longitude: %f %f\n", chl->longitude, chl->latitude);
	LOG("INFO", "       share: %d\n", chl->share);
	//LOG("INFO", "\n");
}

void print_channel_map(map<string, chl_info_t> *chl_map)
{
	auto iter = chl_map->begin();
	for(;iter != chl_map->end(); iter++)
	{
		LOG("INFO", "------------- channel:%s -------------\n", iter->second.id);
		print_channel_info(&iter->second);
	}
}

void print_access_map(CMap<string, access_info_t> *access_map)
{
	if (NULL != access_map) {
		LOG("INFO", "------------- access -------------\n");
		auto iter = access_map->begin();
		for (; iter != access_map->end(); iter++) {
			LOG("INFO", "          id: %s:%s:%d\n", iter->second.addr.id, iter->second.addr.ip, iter->second.addr.port);
			LOG("INFO", "       share: %d\n", iter->second.share);
			LOG("INFO", "        auth: %d\n", iter->second.auth);
			if(0 != iter->second.auth) {
				LOG("INFO", "       realm: %s\n", iter->second.realm);
				LOG("INFO", "    password: %s\n", iter->second.password);
			}
		}
		LOG("INFO", "---------------------------------\n");
	}
}

void print_platform_info(pfm_info_t *pfm)
{
	if (NULL == pfm)
		return;
	LOG("INFO", "------------- platform:%s -------------\n", pfm->addr.ip);
	LOG("INFO", "      enable: %d\n", pfm->enable);
	LOG("INFO", "          id: %s:%s:%d\n", pfm->addr.id, pfm->addr.ip, pfm->addr.port);
    //LOG("INFO","        media: %s:%d\n", pfm->media.ip, pfm->media.port);
	LOG("INFO", "      domain: %s\n", pfm->domain);
    LOG("INFO", "    platform: %s(%s)\n", pfm->platform, pfm->protocol);
    LOG("INFO", "    username: %s:%s\n", pfm->username, pfm->password);
    LOG("INFO", "     expires: %d(%d)\n", pfm->expires,pfm->register_interval);
    LOG("INFO", "   keepalive: %d\n", pfm->keepalive);
	LOG("INFO", "      status: %d\n", pfm->status);
    LOG("INFO", "\n");
}

void print_platform_map(map<string, pfm_info_t> *pfm_map)
{
	auto pfm = pfm_map->begin();
	for (; pfm != pfm_map->end(); pfm++)
	{
		LOG("INFO", "------------- platform:%s -------------\n", pfm->second.addr.id);
		print_platform_info(&pfm->second);
	}
}

void print_local_info(local_cfg_t *local_cfg)
{
	if (NULL == local_cfg)
		return;
	LOG("INFO", "------------- local_cfg -------------\n");
	LOG("INFO", "          id: %s\n", local_cfg->addr.id);
	LOG("INFO", "          ip: %s:%d\n", local_cfg->addr.ip, local_cfg->addr.port);
	LOG("INFO", "         ip2: %s:%d\n", local_cfg->dev.addr.ip, local_cfg->dev.addr.port);
	LOG("INFO", "       media: %s:%d\n", local_cfg->media.ip, local_cfg->media.port);
	LOG("INFO", "      domain: %s\n", local_cfg->domain);
	LOG("INFO", "    password: %s\n", local_cfg->password);
	LOG("INFO", "\n");
}
void print_record_map(map<string, record_info_t> *rec_map)
{
	if (NULL != rec_map)
	{
		if(rec_map->size() < 1)
			LOG("INFO", "------------- record_map=%d -------------\n", rec_map->size());
		auto rec = rec_map->begin();
		for (; rec != rec_map->end(); rec++)
		{
			LOG("INFO", "------------- record:%s -------------\n", rec->second.name);
			print_record_info(&rec->second);
		}
	}
}
void print_record_info(record_info_t *rec)
{
	if (NULL != rec)
	{
		LOG("INFO", "         id: %s\n", rec->id);
		LOG("INFO", "       name: %s\n", rec->name);
		LOG("INFO", "       path: %s\n", rec->file_path);
		LOG("INFO", "    address: %s\n", rec->address);
		LOG("INFO", " start_time: %s\n", rec->start_time);
		LOG("INFO", "   end_time: %s\n", rec->end_time);
		LOG("INFO", "    secrecy: %d\n", rec->secrecy);
		LOG("INFO", "       type: %d\n", rec->type);
		LOG("INFO", "     rec_id: %s\n", rec->recorder_id);
		LOG("INFO", "       size: %d\n", rec->size);
	}
}
#endif