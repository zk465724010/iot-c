
#include "soapStub.h"
#include "soap.h"
//#include <stdio.h>
//#include <stdlib.h>
//#include <assert.h>
//#include "wsseapi.h"
#include "stdsoap2.h"
#include "wsseapi.h"


//#include "wsdd.nsmap"
//#include <stdio.h>
//#include "soapStub.h"
//
//void onvif_soap_perror(struct soap* soap, const char* str)
//{
//    if (NULL == str)
//        printf("[soap] error: %d, %s, %s\n", soap->error, *soap_faultcode(soap), *soap_faultstring(soap));
//    else
//        printf("[soap] %s error: %d, %s, %s\n", str, soap->error, *soap_faultcode(soap), *soap_faultstring(soap));
//}

void* onvif_soap_malloc(struct soap* soap, size_t size)
{
    if ((NULL == soap) || (size <= 0)) {
        printf("Input parameter error [soap:%p,size:%d]\n", soap, size);
        return NULL;
    }
    if (size > 0) {
        void *p = soap_malloc(soap, size);
        if(NULL != p)
            memset(p, 0x00, size);
        return p;
    }
    return NULL;
}
struct soap* onvif_soap_new(int timeout)
{
    struct soap* soap = soap_new();             // soap环境变量
    // SOAP_ASSERT(NULL != soap);
    if (NULL != soap) {
        soap_set_namespaces(soap, namespaces);  // 设置soap的namespaces
        soap->recv_timeout = timeout;           // 设置超时,单位为秒(超过指定时间没有数据就退出)
        soap->send_timeout = timeout;
        soap->connect_timeout = timeout;
        #if defined(__linux__) || defined(__linux)
            soap->socket_flags = MSG_NOSIGNAL; // To prevent connection reset errors
        #endif
    // 设置为UTF-8编码,否则叠加中文OSD会乱码
        soap_set_mode(soap, SOAP_C_UTFSTRING);
        return soap;
    }
    return NULL;
}
void onvif_soap_free(struct soap* soap)
{
    if (NULL != soap) {
        soap_destroy(soap);// remove deserialized class instances (C++ only)
        soap_end(soap);    // Clean up deserialized data (except class instances) and temporary data
        soap_done(soap);   // Reset, close communications, and remove callbacks
        soap_free(soap);   // Reset and deallocate the context created with soap_new or soap_copy
    }
}

/************************************************************************
**功能: 初始化soap描述消息头
**参数: [in]soap: soap环境变量
**返回：无
************************************************************************/
void onvif_init_header(struct soap* soap)
{
    if (NULL == soap) {
        printf("Input parameter error [soap:%p]\n", soap);
        return;
    }
    #define SOAP_TO         "urn:schemas-xmlsoap-org:ws:2005:04:discovery"
    #define SOAP_ACTION     "http://schemas.xmlsoap.org/ws/2005/04/discovery/Probe"
    //struct SOAP_ENV__Header* header = (struct SOAP_ENV__Header*)onvif_soap_malloc(soap, sizeof(struct SOAP_ENV__Header));
    //soap_default_SOAP_ENV__Header(soap, header);
    //header->wsa__MessageID = (char*)soap_wsa_rand_uuid(soap);
    //header->wsa__To = (char*)ONVIF_soap_malloc(soap, strlen(SOAP_TO) + 1);
    //header->wsa__Action = (char*)ONVIF_soap_malloc(soap, strlen(SOAP_ACTION) + 1);
    //strcpy(header->wsa__To, SOAP_TO);
    //strcpy(header->wsa__Action, SOAP_ACTION);
    //soap->header = header;
}


/************************************************************************
**函数：ONVIF_GetProfiles
**功能：获取设备的音视频码流配置信息
**参数：
        [in] MediaXAddr - 媒体服务地址
        [out] profiles  - 返回的设备音视频码流配置信息列表，调用者有责任使用free释放该缓存
**返回：
        返回设备可支持的码流数量（通常是主/辅码流），即使profiles列表个数
**备注：
        1). 注意：一个码流（如主码流）可以包含视频和音频数据，也可以仅仅包含视频数据。
************************************************************************/
int ONVIF_GetProfiles(const char* MediaXAddr, struct tagProfile** profiles, const char* username, const char* password)
{
    //int i = 0;
    //int result = 0;
    //struct soap* soap = NULL;
    //_trt__GetProfiles            req;
    //_trt__GetProfilesResponse    rep;

    //SOAP_ASSERT(NULL != MediaXAddr);
    //SOAP_ASSERT(NULL != username);
    //SOAP_ASSERT(NULL != password);
    //SOAP_ASSERT(NULL != (soap = ONVIF_soap_new(SOAP_SOCK_TIMEOUT)));

    //ONVIF_SetAuthInfo(soap, username, password);

    //result = soap_call___trt__GetProfiles(soap, MediaXAddr, NULL, &req, rep);
    //printf("GetProfiles ->result = %d\n", result);

    //if (0 != result)
    //{
    //    printf("call GetProfiles failed\n");
    //    return -1;
    //}

    //if (rep.__sizeProfiles > 0) {                                               // 分配缓存
    //    (*profiles) = (struct tagProfile*)malloc(rep.__sizeProfiles * sizeof(struct tagProfile));
    //    SOAP_ASSERT(NULL != (*profiles));
    //    memset((*profiles), 0x00, rep.__sizeProfiles * sizeof(struct tagProfile));
    //}

    //for (i = 0; i < rep.__sizeProfiles; i++) {                                   // 提取所有配置文件信息（我们所关心的）
    //    tt__Profile* ttProfile = rep.Profiles[i];
    //    struct tagProfile* plst = &(*profiles)[i];

    //    if (NULL != ttProfile->Name) {                                         // 配置文件名称，区分主码流、子码流
    //        strncpy(plst->Name, ttProfile->Name, sizeof(plst->Name) - 1);
    //    }

    //    if (NULL != ttProfile->token) {                                         // 配置文件Token
    //        strncpy(plst->token, ttProfile->token, sizeof(plst->token) - 1);
    //    }

    //    if (NULL != ttProfile->VideoEncoderConfiguration) {                     // 视频编码器配置信息
    //        if (NULL != ttProfile->VideoEncoderConfiguration->token) {          // 视频编码器Token
    //            strncpy(plst->venc.token, ttProfile->VideoEncoderConfiguration->token, sizeof(plst->venc.token) - 1);
    //        }
    //        if (NULL != ttProfile->VideoEncoderConfiguration->Resolution) {     // 视频编码器分辨率
    //            plst->venc.Width = ttProfile->VideoEncoderConfiguration->Resolution->Width;
    //            plst->venc.Height = ttProfile->VideoEncoderConfiguration->Resolution->Height;
    //        }
    //    }
    //    //PTZ
    //    if (NULL != ttProfile->PTZConfiguration) {
    //        if (NULL != ttProfile->PTZConfiguration->token) {
    //            strncpy(plst->PTZtoken, ttProfile->PTZConfiguration->token, sizeof(plst->PTZtoken) - 1);
    //        }
    //    }
    //    //声音
    //    if (NULL != ttProfile->AudioEncoderConfiguration) {
    //        if (NULL != ttProfile->AudioEncoderConfiguration->token) {
    //            strncpy(plst->Audiotoken, ttProfile->AudioEncoderConfiguration->token, sizeof(plst->Audiotoken) - 1);
    //        }
    //    }
    //    //videoSource
    //    if (NULL != ttProfile->VideoSourceConfiguration) {
    //        if (NULL != ttProfile->VideoSourceConfiguration->SourceToken)
    //            strncpy(plst->videoSourceToken, ttProfile->VideoSourceConfiguration->SourceToken, sizeof(plst->videoSourceToken) - 1);
    //    }
    //}

    //if (NULL != soap) {
    //    ONVIF_soap_delete(soap);
    //}

    //return rep.__sizeProfiles;
    return 0;
}