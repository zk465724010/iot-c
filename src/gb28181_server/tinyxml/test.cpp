
#include <stdio.h>
#include "tinyxml.h"

int read_platform_info(const char *filename);

int main()
{


    return 0;
}


int read_platform_info(const char *filename)
{
    int nRet = -1;
	#if 1
		//TiXmlDocument *document = new TiXmlDocument(filename);
		//bool bRet = document->LoadFile();
		TiXmlDocument document;
		bool bRet = document.LoadFile(filename);
		if (!bRet) {
			printf("load file failed ('%s')\n", filename);
			return nRet;
		}
	#else
		TiXmlDocument document;
		document.Parse(xml_str);
	#endif
	// Config info
	TiXmlNode *pRootNode = document.FirstChild("config");
	if (NULL != pRootNode)
	{
		// Server info
		TiXmlElement *pElem = pRootNode->FirstChildElement("platform");
		if(NULL != pElem)
        {
			//strcpy(cfg->szIp, pElem->GetText());
			//printf("local_ip:%s\n", cfg->szIp);
			TiXmlElement *pSubElem = pElem->FirstChildElement("id");
			pSubElem = pElem->FirstChildElement("ip");
			pSubElem = pElem->FirstChildElement("port");
            nRet = 0;
		}
	}
	return nRet;
}