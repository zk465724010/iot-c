#include "main.h"
#include <iostream>
using namespace std;
#include "onvifSDK_com.h"
#include "onvifSDK_structDefine.h"
#include "soap.h"

const char* g_device_service = "http://192.168.1.67/onvif/device_service";

int main(int argc, char* argv[])
{
    //int nRet = ptz_test1(g_device_service);
    //int nRet = image_test(g_device_service);
    ONVIF_DetectDevice();

    return 0;
}

int image_test(const char* device_service)
{
    int result, profileNum;
    struct tagCapabilities capa;
    struct tagProfile* profiles = NULL;
    struct imgConfigure conf;

    memset(&capa, 0, sizeof(struct tagCapabilities));
    char* userName = "Skwl@2020";
    char* passWord = "Skwl@2021";
    result = ONVIF_GetCapabilities(device_service, &capa, userName, passWord);

    printf("get capa is %d %s\n%s\n", result, capa.IMGXAddr, capa.MediaXAddr);

    profileNum = ONVIF_GetProfiles(capa.MediaXAddr, &profiles, userName, passWord);

    printf("get profile num is %d\n", profileNum);

    printf("get token is %s %s\n", profiles[0].videoSourceToken, profiles[1].videoSourceToken);

    if (capa.IMGXAddr[0] != '\0')
    {
        result = ONVIF_GetPictureConfiguretion(capa.IMGXAddr, profiles[0].videoSourceToken, &conf, userName, passWord);
        conf.brightness = 50;
        conf.contrast = 50;
        conf.colorsaturation = 50;
        result = ONVIF_SetPictureConfiguretion(capa.IMGXAddr, profiles[0].videoSourceToken, &conf, userName, passWord);

        struct soap* soap = NULL;
        if (nullptr == (soap = ONVIF_soap_new(SOAP_SOCK_TIMEOUT)))
        {
            printf("soap is null\n");
            return -2;
        }
        char* userName = "Skwl@2020";
        char* passWord = "Skwl@2021";
        ONVIF_SetAuthInfo(soap, userName, passWord);
        _timg__GetMoveOptions moveoptionreq;
        _timg__GetMoveOptionsResponse moveoptionresp;
        moveoptionreq.VideoSourceToken = profiles[0].videoSourceToken;
        result = soap_call___timg__GetMoveOptions(soap, capa.IMGXAddr, NULL, &moveoptionreq, moveoptionresp);
        printf("result is %d,%f,%f\n", result, moveoptionresp.MoveOptions->Continuous->Speed->Min, moveoptionresp.MoveOptions->Continuous->Speed->Max);
        ONVIF_SetAuthInfo(soap, userName, passWord);
        _timg__Move movereq;
        _timg__MoveResponse moveresp;
        movereq.VideoSourceToken = profiles[0].videoSourceToken;
        movereq.Focus = soap_new_tt__FocusMove(soap);
        movereq.Focus->Continuous = soap_new_tt__ContinuousFocus(soap);
        movereq.Focus->Continuous->Speed = -0.5;
        result = soap_call___timg__Move(soap, capa.IMGXAddr, NULL, &movereq, moveresp);
        Sleep(3000);
        movereq.VideoSourceToken = profiles[0].videoSourceToken;

        movereq.Focus->Continuous->Speed = 0.5;
        result = soap_call___timg__Move(soap, capa.IMGXAddr, NULL, &movereq, moveresp);
        Sleep(3000);
        _timg__Stop moveStopreq;
        _timg__StopResponse moveStopresp;
        moveStopreq.VideoSourceToken = profiles[0].videoSourceToken;
        result = soap_call___timg__Stop(soap, capa.IMGXAddr, NULL, &moveStopreq, moveStopresp);
    }
    return 0;
}

int ptz_test1(const char* device_service)
{
    int result, profileNum;
    struct tagCapabilities capa;
    struct ptzProfile* profiles = NULL;
    struct soap* soap = NULL;

    _trt__GetProfiles           dev_req;
    _trt__GetProfilesResponse   dev_resp;

    _tptz__GetConfigurationOptions  conf_req;
    _tptz__GetConfigurationOptionsResponse conf_resp;

    _tptz__GetNodes nodes_req;
    _tptz__GetNodesResponse nodes_resp;

    if (nullptr == (soap = ONVIF_soap_new(SOAP_SOCK_TIMEOUT)))
    {
        printf("soap is null\n");
        return -2;
    }

    memset(&capa, 0, sizeof(struct tagCapabilities));
    char* userName = "Skwl@2020";
    char* passWord = "Skwl@2021";
    result = ONVIF_GetCapabilities(device_service, &capa, userName, passWord);
    printf("get capa is %d %s\n", result, capa.PTZXAddr);
    if (capa.PTZXAddr != NULL)
    {
        ONVIF_SetAuthInfo(soap, userName, passWord);
        result = soap_call___trt__GetProfiles(soap, capa.PTZXAddr, nullptr, &dev_req, dev_resp);
        printf("GetProfiles ->result ====%d\n ", result);
        char* token = dev_resp.Profiles[0]->PTZConfiguration->token;
        char* token2 = dev_resp.Profiles[0]->token;
        printf("token is%s, %s\n", dev_resp.Profiles[0]->token, token);

        ONVIF_SetAuthInfo(soap, userName, passWord);
        conf_req.ConfigurationToken = token;
        result = soap_call___tptz__GetConfigurationOptions(soap, capa.PTZXAddr, NULL, &conf_req, conf_resp);
        printf("Getconf ->result ====%d\n ", result);
        if (conf_resp.PTZConfigurationOptions != NULL)
        {
            printf("continue speed is %d\n", conf_resp.PTZConfigurationOptions->Spaces->__sizePanTiltSpeedSpace);
            printf("%f %f\n", conf_resp.PTZConfigurationOptions->Spaces->PanTiltSpeedSpace[0]->XRange->Min, conf_resp.PTZConfigurationOptions->Spaces->PanTiltSpeedSpace[0]->XRange->Max);
        }
        ONVIF_SetAuthInfo(soap, userName, passWord);

        result = soap_call___tptz__GetNodes(soap, capa.PTZXAddr, NULL, &nodes_req, nodes_resp);
        printf("get nodes result is %d\n", result);
        printf("get node num is %d,name %s max %f\n", nodes_resp.__sizePTZNode, nodes_resp.PTZNode[0]->Name, nodes_resp.PTZNode[0]->SupportedPTZSpaces->PanTiltSpeedSpace[0]->XRange->Max);

        result = ONVIF_ContinuousMove(capa.PTZXAddr, token2, userName, passWord, PTZ_CMD_LEFT, 1.2);
        printf("left move result is %d", result);
        Sleep(3000);
        result = ONVIF_ContinuousMove(capa.PTZXAddr, token2, userName, passWord, PTZ_CMD_RIGHT, 1.2);
        Sleep(3000);
        result = ONVIF_ContinuousMove(capa.PTZXAddr, token2, userName, passWord, PTZ_CMD_STOP, 0.8);

        printf("stop result is %d", result);
        result = ONVIF_ContinuousMove(capa.PTZXAddr, token2, userName, passWord, PTZ_CMD_LEFTUP, 0.2);

        printf("left move result is %d", result);

        Sleep(3000);
        result = ONVIF_ContinuousMove(capa.PTZXAddr, token2, userName, passWord, PTZ_CMD_RIGHTDOWN, 0.2);
        Sleep(3000);
        result = ONVIF_ContinuousMove(capa.PTZXAddr, token2, userName, passWord, PTZ_CMD_STOP, 0.8);

        printf("stop result is %d", result);
    }
    if (nullptr != soap) {
        ONVIF_soap_delete(soap);
    }
    return 0;
}

int ptz_test2(const char* device_service)
{
    int result;
    struct soap* soap = NULL;
    char* userName = "Skwl@2020";
    char* passWord = "Skwl@2021";
    soap = ONVIF_soap_new(SOAP_SOCK_TIMEOUT);
    ONVIF_SetAuthInfo(soap, userName, passWord);
    struct tagCapabilities capa;

    _trt__GetProfiles           dev_req;
    _trt__GetProfilesResponse   dev_resp;

    _tptz__GetPresets presets_req;
    _tptz__GetPresetsResponse presets_resp;

    _tptz__GetNodes nodes_req;
    _tptz__GetNodesResponse nodes_resp;

    _tptz__SendAuxiliaryCommand auxi_req;
    _tptz__SendAuxiliaryCommandResponse auxi_resp;

    _tptz__GotoPreset gopreset_req;
    _tptz__GotoPresetResponse gopreset_resp;

    result = ONVIF_GetCapabilities(device_service, &capa, userName, passWord);

    printf("get capa is %d %s\n", result, capa.PTZXAddr);

    ONVIF_SetAuthInfo(soap, userName, passWord);
    result = soap_call___trt__GetProfiles(soap, capa.PTZXAddr, nullptr, &dev_req, dev_resp);
    printf("GetProfiles ->result ====%d\n ", result);
    char* token = dev_resp.Profiles[0]->token;
    printf("token is%s\n", token);


    ONVIF_SetAuthInfo(soap, userName, passWord);
    presets_req.ProfileToken = token;
    result = soap_call___tptz__GetPresets(soap, capa.PTZXAddr, nullptr, &presets_req, presets_resp);
    printf("GetPresets ->result ====%d\n ", result);
    //SOAP_CHECK_ERROR(result, soap, "soap_call___tptz__GetPresets");

    int i;
    gopreset_req.ProfileToken = token;
    gopreset_req.Speed = soap_new_tt__PTZSpeed(soap);
    gopreset_req.Speed->PanTilt = soap_new_tt__Vector2D(soap);
    gopreset_req.Speed->Zoom = soap_new_tt__Vector1D(soap);
    gopreset_req.Speed->PanTilt->x = 0.5;
    gopreset_req.Speed->PanTilt->y = 0.5;
    gopreset_req.Speed->Zoom->x = 0.5;
    for (i = 0; i < presets_resp.__sizePreset; i++)
    {
        printf("%s %s\n", presets_resp.Preset[i]->Name, presets_resp.Preset[i]->token);
        ONVIF_SetAuthInfo(soap, userName, passWord);
        gopreset_req.PresetToken = presets_resp.Preset[i]->token;
        result = soap_call___tptz__GotoPreset(soap, capa.PTZXAddr, nullptr, &gopreset_req, gopreset_resp);
        printf("GotoPresets ->result ====%d\n ", result);
        Sleep(10000);
    }

    ONVIF_SetAuthInfo(soap, userName, passWord);

    result = soap_call___tptz__GetNodes(soap, capa.PTZXAddr, NULL, &nodes_req, nodes_resp);
    printf("get nodes result is %d\n", result);

    printf("max %d\n ", nodes_resp.PTZNode[0]->MaximumNumberOfPresets);
    tt__PTZNode* p = nodes_resp.PTZNode[0];
    for (i = 0; i < p->__sizeAuxiliaryCommands; i++)
    {
        printf("cmd %s\n", nodes_resp.PTZNode[0]->AuxiliaryCommands[i]);
        ONVIF_SetAuthInfo(soap, userName, passWord);
        auxi_req.ProfileToken = token;
        auxi_req.AuxiliaryData = nodes_resp.PTZNode[0]->AuxiliaryCommands[i];
        result = soap_call___tptz__SendAuxiliaryCommand(soap, capa.PTZXAddr, NULL, &auxi_req, auxi_resp);
        Sleep(2000);
    }
    if (nullptr != soap) {
        ONVIF_soap_delete(soap);
    }
    return 0;
}
int osd_test(const char* device_service)
{
    int result, profileNum, i;
    struct soap* soap = NULL;
    char* userName = "Skwl@2020";
    char* passWord = "Skwl@2021";
    bool  isadd = false;

    soap = ONVIF_soap_new(SOAP_SOCK_TIMEOUT);
    ONVIF_SetAuthInfo(soap, userName, passWord);
    struct tagCapabilities capa;


    result = ONVIF_GetCapabilities(device_service, &capa, userName, passWord);
    ONVIF_SetAuthInfo(soap, userName, passWord);
    _trt__GetProfiles            req;
    _trt__GetProfilesResponse    rep;
    result = soap_call___trt__GetProfiles(soap, capa.MediaXAddr, NULL, &req, rep);


    printf("call 1\n");
    ONVIF_SetAuthInfo(soap, userName, passWord);

    _trt__GetOSDs getOsdreq;
    _trt__GetOSDsResponse getOsdResp;
    result = soap_call___trt__GetOSDs(soap, capa.MediaXAddr, NULL, &getOsdreq, getOsdResp);
    if (result != 0)
        soap_perror(soap, NULL);

    printf("osd num is %d\n", getOsdResp.__sizeOSDs);
    for (i = 0; i < getOsdResp.__sizeOSDs; i++)
    {
        printf("type %d,position %s,text %s %s\n ", getOsdResp.OSDs[i]->Type, getOsdResp.OSDs[i]->Position->Type, getOsdResp.OSDs[i]->TextString->Type, getOsdResp.OSDs[i]->TextString->PlainText);
        printf("position  %f %f\n", *(getOsdResp.OSDs[i]->Position->Pos->x), *(getOsdResp.OSDs[i]->Position->Pos->y));
        if (getOsdResp.OSDs[i]->TextString->PlainText != NULL)
        {
            _trt__SetOSD  setOsdReq;
            setOsdReq.OSD = getOsdResp.OSDs[i];
            char* test = "测试海康";
            getOsdResp.OSDs[i]->TextString->PlainText = test;
            _trt__SetOSDResponse setOsdResp;
            ONVIF_SetAuthInfo(soap, userName, passWord);
            result = soap_call___trt__SetOSD(soap, capa.MediaXAddr, NULL, &setOsdReq, setOsdResp);
            if (result != 0)
                soap_perror(soap, NULL);
            isadd = true;
        }
    }
    if (isadd == false)
    {
        ONVIF_SetAuthInfo(soap, userName, passWord);
        _trt__CreateOSD createReq;
        _trt__CreateOSDResponse createResp;
        tt__OSDConfiguration osd;
        createReq.OSD = &osd;
        tt__OSDReference videoToken;
        videoToken.__item = rep.Profiles[0]->VideoSourceConfiguration->SourceToken;
        osd.VideoSourceConfigurationToken = &videoToken;
        osd.Type = tt__OSDType__Text;
        tt__OSDTextConfiguration textOsd;
        char* PositonType = "Custom";
        char* textType = "Plain";
        char* planText = "addsucess";
        textOsd.Type = textType;
        textOsd.PlainText = planText;
        osd.TextString = &textOsd;
        tt__OSDPosConfiguration postion;
        postion.Type = PositonType;
        tt__Vector pos;
        float x = 0.298;
        float y = -0.818;
        pos.x = &x;
        pos.y = &y;
        postion.Pos = &pos;
        osd.Position = &postion;
        result = soap_call___trt__CreateOSD(soap, capa.MediaXAddr, NULL, &createReq, createResp);
        if (result != 0)
            soap_perror(soap, NULL);
    }
    ONVIF_SetAuthInfo(soap, userName, passWord);
    _trt__GetOSDOptions osdOptionsReq;
    _trt__GetOSDOptionsResponse osdOptionsResp;
    //memset(&osdOptionsReq,0,sizeof(_trt__GetOSDOptions));
    osdOptionsReq.ConfigurationToken = rep.Profiles[0]->VideoSourceConfiguration->token;
    printf("call\n");
    result = soap_call___trt__GetOSDOptions(soap, capa.MediaXAddr, NULL, &osdOptionsReq, osdOptionsResp);
    if (result != 0)
        soap_perror(soap, NULL);
    tt__OSDConfigurationOptions* osdOptions = osdOptionsResp.OSDOptions;
    printf("type\n");
    for (i = 0; i < osdOptions->__sizeType; i++)
        printf("%d ", osdOptions->Type[i]);

    printf("\npositon\n");

    for (i = 0; i < osdOptions->__sizePositionOption; i++)
        printf("%s ", osdOptions->PositionOption[i]);

    printf("\nnum\n");
    if (osdOptions->MaximumNumberOfOSDs->PlainText != NULL)
        printf("total %d text %d\n", osdOptions->MaximumNumberOfOSDs->Total, *(osdOptions->MaximumNumberOfOSDs->PlainText));
    else
        printf("total %d\n", osdOptions->MaximumNumberOfOSDs->Total);

    if (osdOptions->TextOption != NULL)
    {
        for (i = 0; i < osdOptions->TextOption->__sizeType; i++)
            printf("%s ", osdOptions->TextOption->Type[i]);
        printf("\n");
        if (osdOptions->TextOption->FontSizeRange != NULL)
            printf("size %d %d\n", osdOptions->TextOption->FontSizeRange->Min, osdOptions->TextOption->FontSizeRange->Max);
        if (osdOptions->TextOption->FontColor != NULL && osdOptions->TextOption->FontColor->Color != NULL)
        {
            for (i = 0; i < osdOptions->TextOption->FontColor->Color->__sizeColorList; i++)
                printf("%f %f %f\n", osdOptions->TextOption->FontColor->Color->ColorList[i]->X, osdOptions->TextOption->FontColor->Color->ColorList[i]->Y, osdOptions->TextOption->FontColor->Color->ColorList[i]->Z);
        }
    }
    if (NULL != soap) {
        ONVIF_soap_delete(soap);
    }
    return 0;
}
int video_get_range1(const char* device_service)
{
    int result;
    struct soap* soap = NULL;
    //char ipaddress[256];
    char* userName = "Skwl@2020";
    char* passWord = "Skwl@2021";
    bool	isadd = false;

    soap = ONVIF_soap_new(SOAP_SOCK_TIMEOUT);
    ONVIF_SetAuthInfo(soap, userName, passWord);
    struct tagCapabilities capa;

    struct tagProfile* profiles = NULL;
    result = ONVIF_GetCapabilities(device_service, &capa, userName, passWord);
    ONVIF_SetAuthInfo(soap, userName, passWord);
    int profile_count = ONVIF_GetProfiles(capa.MediaXAddr, &profiles, userName, passWord);

    printf("profile_count: %d\n", profile_count);
    char* ventoken = NULL;
    if(profile_count > 0)
        ventoken = profiles[0].token;

    printf("%s\n", ventoken);
    _trt__GetVideoEncoderConfigurationOptions req;
    _trt__GetVideoEncoderConfigurationOptionsResponse resp;

    req.ProfileToken = ventoken;

    ONVIF_SetAuthInfo(soap, userName, passWord);
    result = soap_call___trt__GetVideoEncoderConfigurationOptions(soap, capa.MediaXAddr, NULL, &req, resp);
    if (result != 0)
        soap_perror(soap, NULL);
    else
    {
        int i;
        printf("con %d\n", resp.Options->H264->__sizeResolutionsAvailable);
        printf("que %d %d\n", resp.Options->QualityRange->Min, resp.Options->QualityRange->Max);
        printf("framerate %d %d\n", resp.Options->H264->FrameRateRange->Min, resp.Options->H264->FrameRateRange->Max);
        printf("gop %d %d\n", resp.Options->H264->GovLengthRange->Min, resp.Options->H264->GovLengthRange->Max);
        printf("encoder %d %d\n", resp.Options->H264->EncodingIntervalRange->Min, resp.Options->H264->EncodingIntervalRange->Max);
        for (i = 0; i < resp.Options->H264->__sizeResolutionsAvailable; i++)
        {
            printf("%d %d \n", resp.Options->H264->ResolutionsAvailable[i]->Height, resp.Options->H264->ResolutionsAvailable[i]->Width);
        }
    }

    if (NULL != soap) {
        ONVIF_soap_delete(soap);
    }
    return 0;
}

int video_get_range2(const char* device_service)
{
    int result;
    struct soap* soap = NULL;
    //char ipaddress[256];
    char* userName = "Skwl@2020";
    char* passWord = "Skwl@2021";
    bool	isadd = false;

    soap = ONVIF_soap_new(SOAP_SOCK_TIMEOUT);
    ONVIF_SetAuthInfo(soap, userName, passWord);
    struct tagCapabilities capa;

    struct tagProfile* profiles = NULL;

    result = ONVIF_GetCapabilities(device_service, &capa, userName, passWord);
    ONVIF_SetAuthInfo(soap, userName, passWord);
    int profile_count = ONVIF_GetProfiles(capa.MediaXAddr, &profiles, userName, passWord);

    printf("profile_count: %d\n", profile_count);
    char* ventoken = NULL;
    if (profile_count > 0)
        ventoken = profiles[0].venc.token;


    printf("%s\n", ventoken);
    _trt__GetVideoEncoderConfiguration          req;
    _trt__GetVideoEncoderConfigurationResponse  rep;

    req.ConfigurationToken = ventoken;

    ONVIF_SetAuthInfo(soap, userName, passWord);
    result = soap_call___trt__GetVideoEncoderConfiguration(soap, capa.MediaXAddr, NULL, &req, rep);
    if (result != 0)
        soap_perror(soap, NULL);
    else
    {
    }
    char* audiotoken = profiles[0].Audiotoken;
    printf("%s\n", audiotoken);
    ONVIF_SetAuthInfo(soap, userName, passWord);
    _trt__GetAudioEncoderConfiguration reQ;
    _trt__GetAudioEncoderConfigurationResponse reP;
    reQ.ConfigurationToken = audiotoken;
    result = soap_call___trt__GetAudioEncoderConfiguration(soap, capa.MediaXAddr, NULL, &reQ, reP);
    if (result != 0)
        soap_perror(soap, NULL);

    ONVIF_SetAuthInfo(soap, userName, passWord);
    _trt__SetAudioEncoderConfiguration reQS;
    _trt__SetAudioEncoderConfigurationResponse rePS;
    //reQS.ForcePersistence = true;
   //reQS.Configuration;
    result = soap_call___trt__SetAudioEncoderConfiguration(soap, capa.MediaXAddr, NULL, &reQS, rePS);

    if (result != 0)
        soap_perror(soap, NULL);
    if (NULL != soap) {
        ONVIF_soap_delete(soap);
    }
    return 0;
}

void ONVIF_DetectDevice()
{
    int i;
    int result = 0;
    unsigned int count = 0;                                                     // 搜索到的设备个数
    struct soap* soap = NULL;                                                   // soap环境变量
    struct wsdd__ProbeType      req;                                            // 用于发送Probe消息
    struct __wsdd__ProbeMatches rep;                                            // 用于接收Probe应答
    struct wsdd__ProbeMatchType* probeMatch;

    soap = ONVIF_soap_new(SOAP_SOCK_TIMEOUT);

    ONVIF_init_header(soap);      // 设置消息头描述
    ONVIF_init_ProbeType(soap, &req);   // 设置寻找的设备的范围和类型
    result = soap_send___wsdd__Probe(soap, SOAP_MCAST_ADDR, NULL, &req);// 向组播地址广播Probe消息
    while (SOAP_OK == result)  // 开始循环接收设备发送过来的消息
    {
        memset(&rep, 0x00, sizeof(rep));
        result = soap_recv___wsdd__ProbeMatches(soap, &rep);
        if (SOAP_OK == result) {
            if (soap->error) {
                soap_perror(soap, "ProbeMatches");
            }
            else {           // 成功接收到设备的应答消息
            // dump__wsdd__ProbeMatches(&rep);
                if (NULL != rep.wsdd__ProbeMatches) {
                    count += rep.wsdd__ProbeMatches->__sizeProbeMatch;
                    for (i = 0; i < rep.wsdd__ProbeMatches->__sizeProbeMatch; i++) {
                        probeMatch = rep.wsdd__ProbeMatches->ProbeMatch + i;

                        printf(">: %s\n", probeMatch->XAddrs);
                    }
                }
            }
        }
        else if (soap->error) {
            break;
        }
    }
    SOAP_DBGLOG("\ndetect end! It has detected %d devices!\n", count);
    if (NULL != soap) {
        ONVIF_soap_delete(soap);
    }
    return;
}
