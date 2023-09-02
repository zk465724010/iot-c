#include "com_hirain_sdk_rtsp_JNIPTZ.h"
#include "onvifSDK_com.h"
#ifdef _WIN32
#else
#include <unistd.h>
#endif
/*
 * Class:     com_hirain_sdk_rtsp_JNIPTZ
 * Method:    ONVIFPTZControlMove
 * Signature: (Ljava/lang/String;Ljava/lang/Object;Ljava/lang/String;Ljava/lang/String;Ljava/lang/Object;)I
 */
JNIEXPORT jint JNICALL Java_com_hirain_sdk_rtsp_JNIPTZ_ONVIFPTZControlMove
(JNIEnv *env, jobject jthis, jstring jdeviceAddr, jobject jtype, jstring jusrName, jstring jpassword, jobject jspeed)
{
    printf("------ PTZControlMove start ------\n");
	return 0;

    const char* userName = jstring2CStr(env,jusrName);
    const char* passWord = jstring2CStr(env,jpassword);
    const char* deviceAddr = jstring2CStr(env,jdeviceAddr);
    int lastCmd=-1,curCmd=-1;
    float currSpeed=1.0;
    /*************************获取设备能力信息*********************************************/
    int result = 0;
    struct soap *soap = NULL;
    char *ptzXAddr = NULL; //保存云台操作地址
    char *profilesToken = NULL;//保存云台操作标志牌

    _tds__GetCapabilities            devinfo_req;
    _tds__GetCapabilitiesResponse    devinfo_resp;

    _trt__GetProfiles           dev_req;
    _trt__GetProfilesResponse   dev_resp;

    printf("2\n");

    if(NULL == deviceAddr)
    {
        printf("deviceAddr is null in Java_wsdd_nsmap_JNIPTZ_ONVIFPTZControlMove\n");
        return -2;
    }
    //    SOAP_ASSERT(NULL != (soap = ONVIF_soap_new(SOAP_SOCK_TIMEOUT)));
    if(NULL == (soap = ONVIF_soap_new(SOAP_SOCK_TIMEOUT)))
    {
        printf("soap is null in Java_wsdd_nsmap_JNIPTZ_ONVIFPTZControlMove\n");
        return -2;
    }
    printf("3\n");
    ONVIF_SetAuthInfo(soap, userName,passWord);
    result = soap_call___tds__GetCapabilities(soap, deviceAddr, NULL, &devinfo_req, devinfo_resp);
    printf("GetCapabilities ->result ====%d\n ",result);
    //    SOAP_CHECK_ERROR(result, soap, "GetCapabilities");
    printf("4\n");
    if(devinfo_resp.Capabilities->PTZ != NULL)
        ptzXAddr=devinfo_resp.Capabilities->PTZ->XAddr;

    /**********************************************************************/

    /************************获取云台操作标志牌*******************************************/
    printf("5\n");
    ONVIF_SetAuthInfo(soap, userName,passWord);
    printf("6\n");
    if(NULL == ptzXAddr)
    {
        printf("ptzXAddr is null in Java_wsdd_nsmap_JNIPTZ_ONVIFPTZControlMove\n");
        return -2;
    }
    result = soap_call___trt__GetProfiles(soap, ptzXAddr, NULL, &dev_req, dev_resp);
    printf("GetProfiles ->result ====%d\n ",result);
    //   SOAP_CHECK_ERROR(result, soap, "ONVIF_GetProfiles");
    //    SOAP_ASSERT(dev_resp.__sizeProfiles > 0);
    //strcpy(profilesToken,dev_resp.Profiles[0]->token);
    printf("get profile num is %d\n",dev_resp.__sizeProfiles);
    printf("get token is %s\n",dev_resp.Profiles[0]->token);
    profilesToken = dev_resp.Profiles[0]->token;
    /**********************************************************************/

    float *speed = (float*)env->GetDirectBufferAddress(jspeed);
    int * cmd = (int*)env->GetDirectBufferAddress(jtype);
    printf(" start cmd\n");
    while(*cmd!=0)
    {
        curCmd=*cmd;
        currSpeed=*speed;

        if(curCmd<PTZ_CMD_LEFT||curCmd>PTZ_CMD_STOP)
            continue;
        if(currSpeed<0)
            currSpeed=0.1;
        if(currSpeed>1.0)
            currSpeed=1.0;
        if(curCmd!=lastCmd)
        {
            printf("curcmd is %d\n,speed is %f",curCmd,currSpeed);
            result = ONVIF_ContinuousMove(ptzXAddr, profilesToken, userName,passWord, curCmd, currSpeed);
            printf("ContinuousMove ->result ====%d,cmd is %d\n ",result,curCmd);
        }
        lastCmd=curCmd;
#ifdef _WIN32
        Sleep(100);
#else
        usleep(100000);
#endif
    }
    //EXIT:
    if (NULL != soap) {
        ONVIF_soap_delete(soap);
    }
    return result;

}

/*
 * Class:     com_hirain_sdk_rtsp_JNIPTZ
 * Method:    ONVIFAddPresetPoint
 * Signature: (Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;)Ljava/lang/String;
 */
JNIEXPORT jstring JNICALL Java_com_hirain_sdk_rtsp_JNIPTZ_ONVIFAddPresetPoint
(JNIEnv * env, jobject, jstring jdeviceAddr, jstring jusrName, jstring jpassword)
{
    int result=0;
    struct soap* soap=NULL;

    const char* userName = jstring2CStr(env,jusrName);
    const char* passWord = jstring2CStr(env,jpassword);
    const char* deviceAddr = jstring2CStr(env,jdeviceAddr);

    struct tagCapabilities capa;

    _trt__GetProfiles           dev_req;
    _trt__GetProfilesResponse   dev_resp;

    soap = ONVIF_soap_new(SOAP_SOCK_TIMEOUT);
    ONVIF_SetAuthInfo(soap, userName, passWord);

    result = ONVIF_GetCapabilities(deviceAddr, &capa, userName, passWord);
    if(capa.PTZXAddr[0]=='\0')
    {
        printf("get capa fail in Java_wsdd_nsmap_JNIPTZ_ONVIFAddPresetPoint\n");
        return NULL;
    }
    printf("get capa is %d %s\n",result,capa.PTZXAddr);


    ONVIF_SetAuthInfo(soap, userName,passWord);
    result = soap_call___trt__GetProfiles(soap, capa.PTZXAddr, NULL, &dev_req, dev_resp);
    printf("GetProfiles ->result ====%d\n ",result);
    if(dev_resp.Profiles==NULL)
    {
        printf("get profile fail in Java_wsdd_nsmap_JNIPTZ_ONVIFAddPresetPoint\n");
        return NULL;
    }
    char* token=dev_resp.Profiles[0]->token;
    printf("token is%s\n",token);

    _tptz__SetPreset setPreset_req;
    _tptz__SetPresetResponse setPreset_resp;
    ONVIF_SetAuthInfo(soap, userName,passWord);
    setPreset_req.ProfileToken=token;
    result=soap_call___tptz__SetPreset(soap, capa.PTZXAddr, NULL, &setPreset_req, setPreset_resp);

    printf("setpreset ->result ====%d\n ",result);
    if(setPreset_resp.PresetToken==NULL)
    {
        printf("set preset fail in Java_wsdd_nsmap_JNIPTZ_ONVIFAddPresetPoint\n");
        soap_perror(soap,NULL);
        return NULL;
    }

    jstring presetToken=CStr2jstring(env,setPreset_resp.PresetToken);
    //EXIT:
    if (NULL != soap) {
        ONVIF_soap_delete(soap);
    }
    return presetToken;
}

/*
 * Class:     com_hirain_sdk_rtsp_JNIPTZ
 * Method:    ONVIFRemovePresetPoint
 * Signature: (Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;)I
 */
JNIEXPORT jint JNICALL Java_com_hirain_sdk_rtsp_JNIPTZ_ONVIFRemovePresetPoint
(JNIEnv * env, jobject, jstring jdeviceAddr, jstring jusrName, jstring jpassword, jstring jpresetToken)
{
    int result=0;
    struct soap* soap=NULL;

    const char* userName = jstring2CStr(env,jusrName);
    const char* passWord = jstring2CStr(env,jpassword);
    const char* deviceAddr = jstring2CStr(env,jdeviceAddr);
    char* presetToken=jstring2CStr(env,jpresetToken);
    struct tagCapabilities capa;

    _trt__GetProfiles           dev_req;
    _trt__GetProfilesResponse   dev_resp;

    soap = ONVIF_soap_new(SOAP_SOCK_TIMEOUT);
    ONVIF_SetAuthInfo(soap, userName, passWord);

    result = ONVIF_GetCapabilities(deviceAddr, &capa, userName, passWord);
    if(capa.PTZXAddr==NULL)
    {
        printf("get capa fail in Java_wsdd_nsmap_JNIPTZ_ONVIFAddPresetPoint\n");
        return -1;
    }
    printf("get capa is %d %s\n",result,capa.PTZXAddr);


    ONVIF_SetAuthInfo(soap, userName,passWord);
    result = soap_call___trt__GetProfiles(soap, capa.PTZXAddr, NULL, &dev_req, dev_resp);
    printf("GetProfiles ->result ====%d\n ",result);
    if(dev_resp.Profiles==NULL)
    {
        printf("get profile fail in Java_wsdd_nsmap_JNIPTZ_ONVIFAddPresetPoint\n");
        return -1;
    }
    char* token=dev_resp.Profiles[0]->token;
    printf("token is%s\n",token);

    _tptz__RemovePreset removePreset_req;
    _tptz__RemovePresetResponse removePreset_resp;
    ONVIF_SetAuthInfo(soap, userName,passWord);
    removePreset_req.PresetToken=presetToken;
    removePreset_req.ProfileToken=token;
    result=soap_call___tptz__RemovePreset(soap, capa.PTZXAddr, NULL, &removePreset_req, removePreset_resp);

    printf("removepreset ->result ====%d\n ",result);
    if(result!=0)
        soap_perror(soap,NULL);
    //EXIT:
    if (NULL != soap) {
        ONVIF_soap_delete(soap);
    }
    return result;
}

/*
 * Class:     com_hirain_sdk_rtsp_JNIPTZ
 * Method:    ONVIFControlPresetPoint
 * Signature: (Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;F)I
 */
JNIEXPORT jint JNICALL Java_com_hirain_sdk_rtsp_JNIPTZ_ONVIFControlPresetPoint
(JNIEnv * env, jobject,  jstring jdeviceAddr, jstring jusrName, jstring jpassword, jstring jpresetToken, jfloat jspeed)
{
    int result=0;
    struct soap* soap=NULL;

    const char* userName = jstring2CStr(env,jusrName);
    const char* passWord = jstring2CStr(env,jpassword);
    const char* deviceAddr = jstring2CStr(env,jdeviceAddr);
    float speed=jspeed;
    char* presetToken=jstring2CStr(env,jpresetToken);
    struct tagCapabilities capa;

    if(speed<=0)
        speed=0.1;
    if(speed>1.0)
        speed=1.0;

    _trt__GetProfiles           dev_req;
    _trt__GetProfilesResponse   dev_resp;

    soap = ONVIF_soap_new(SOAP_SOCK_TIMEOUT);
    ONVIF_SetAuthInfo(soap, userName, passWord);

    result = ONVIF_GetCapabilities(deviceAddr, &capa, userName, passWord);
    if(capa.PTZXAddr==NULL)
    {
        printf("get capa fail in Java_wsdd_nsmap_JNIPTZ_ONVIFAddPresetPoint\n");
        return -1;
    }
    printf("get capa is %d %s\n",result,capa.PTZXAddr);


    ONVIF_SetAuthInfo(soap, userName,passWord);
    result = soap_call___trt__GetProfiles(soap, capa.PTZXAddr, NULL, &dev_req, dev_resp);
    printf("GetProfiles ->result ====%d\n ",result);
    if(dev_resp.Profiles==NULL)
    {
        printf("get profile fail in Java_wsdd_nsmap_JNIPTZ_ONVIFAddPresetPoint\n");
        return -1;
    }
    char* token=dev_resp.Profiles[0]->token;
    printf("token is%s\n",token);

    _tptz__GotoPreset goPreset_req;
    _tptz__GotoPresetResponse goPreset_resp;
    ONVIF_SetAuthInfo(soap, userName,passWord);
    goPreset_req.PresetToken=presetToken;
    goPreset_req.ProfileToken=token;
    goPreset_req.Speed = soap_new_tt__PTZSpeed(soap);
    goPreset_req.Speed->PanTilt = soap_new_tt__Vector2D(soap);
    goPreset_req.Speed->Zoom = soap_new_tt__Vector1D(soap);
    goPreset_req.Speed->PanTilt->x=speed;
    goPreset_req.Speed->PanTilt->y=speed;
    goPreset_req.Speed->Zoom->x=speed;
    result=soap_call___tptz__GotoPreset(soap, capa.PTZXAddr, NULL, &goPreset_req, goPreset_resp);

    printf("gopreset ->result ====%d\n ",result);
    if(result!=0)
        soap_perror(soap,NULL);

    //EXIT:
    if (NULL != soap) {
        ONVIF_soap_delete(soap);
    }
    return result;
}

/*
 * Class:     com_hirain_sdk_rtsp_JNIPTZ
 * Method:    ONVIFGetDefaultPresetPoint
 * Signature: (Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;)Ljava/lang/Object;
 */
JNIEXPORT jobject JNICALL Java_com_hirain_sdk_rtsp_JNIPTZ_ONVIFGetDefaultPresetPoint
(JNIEnv * env, jobject, jstring jdeviceAddr, jstring jusrName, jstring jpassword)
{
    int result=0,num=0;
    struct soap* soap=NULL;

    const char* userName = jstring2CStr(env,jusrName);
    const char* passWord = jstring2CStr(env,jpassword);
    const char* deviceAddr = jstring2CStr(env,jdeviceAddr);

    struct tagCapabilities capa;

    _trt__GetProfiles           dev_req;
    _trt__GetProfilesResponse   dev_resp;

    soap = ONVIF_soap_new(SOAP_SOCK_TIMEOUT);
    ONVIF_SetAuthInfo(soap, userName, passWord);

    result = ONVIF_GetCapabilities(deviceAddr, &capa, userName, passWord);
    if(capa.PTZXAddr==NULL)
    {
        printf("get capa fail in Java_wsdd_nsmap_JNIPTZ_ONVIFAddPresetPoint\n");
        return NULL;
    }
    printf("get capa is %d %s\n",result,capa.PTZXAddr);


    ONVIF_SetAuthInfo(soap, userName,passWord);
    result = soap_call___trt__GetProfiles(soap, capa.PTZXAddr, NULL, &dev_req, dev_resp);
    printf("GetProfiles ->result ====%d\n ",result);
    if(dev_resp.Profiles==NULL)
    {
        printf("get profile fail in Java_wsdd_nsmap_JNIPTZ_ONVIFAddPresetPoint\n");
        return NULL;
    }
    char* token=dev_resp.Profiles[0]->token;
    printf("token is%s\n",token);

    _tptz__GetPresets presets_req;
    _tptz__GetPresetsResponse presets_resp;
    ONVIF_SetAuthInfo(soap, userName,passWord);
    presets_req.ProfileToken=token;
    result = soap_call___tptz__GetPresets(soap, capa.PTZXAddr, NULL,&presets_req,presets_resp);
    printf("GetPresets ->result ====%d\n ",result);

    _tptz__GetNodes nodes_req;
    _tptz__GetNodesResponse nodes_resp;
    ONVIF_SetAuthInfo(soap, userName,passWord);
    result=soap_call___tptz__GetNodes(soap,capa.PTZXAddr,NULL,&nodes_req,nodes_resp);
    printf("GetNodes ->result ====%d\n ",result);

    //set java object
    jclass ptzConfigClass=env->FindClass("com/hirain/sdk/JNIEntity/PTZConfigure");
    if(ptzConfigClass==NULL)
    {
        printf("find class fail in Java_wsdd_nsmap_JNIPTZ_ONVIFAddPresetPoint\n");
        return NULL;
    }

    jmethodID construct = env->GetMethodID(ptzConfigClass, "<init>", "()V");
    jobject ptzConfigObj=env->NewObject(ptzConfigClass,construct);
    if(ptzConfigObj==NULL)
    {
        printf("create obj fail in Java_wsdd_nsmap_JNIPTZ_ONVIFAddPresetPoint\n");
        return NULL;
    }
    jmethodID setPointMax=env->GetMethodID(ptzConfigClass,"setPointMaxUpper","(I)V");
    if(setPointMax==NULL)
    {
        printf("find setPointMax methodID fail in Java_wsdd_nsmap_JNIPTZ_ONVIFAddPresetPoint\n");
    }else
        env->CallVoidMethod(ptzConfigObj,setPointMax,nodes_resp.PTZNode[0]->MaximumNumberOfPresets);

    jfieldID getNameList=env->GetFieldID(ptzConfigClass,"pointNames","Ljava/util/List;");
    if(getNameList==NULL)
    {
        printf("find getPointNames field id fail in Java_wsdd_nsmap_JNIPTZ_ONVIFAddPresetPoint\n");
    }else
    {
        printf("get name list object\n");
        jobject namelistObject=env->GetObjectField(ptzConfigObj,getNameList);
        if(namelistObject==NULL)
        {
            printf("get namelist obj fail\n");
        }
        printf("get name list class\n");
        jclass listClass=env->GetObjectClass(namelistObject);
        printf("get name list add method\n");
        jmethodID listAddId=env->GetMethodID(listClass,"add","(Ljava/lang/Object;)Z");
        for(num=0;num<presets_resp.__sizePreset;num++)
        {

            jstring addValue=CStr2jstring(env,presets_resp.Preset[num]->token);
            printf("call add to name list \n");
            env->CallBooleanMethod(namelistObject,listAddId,addValue);
        }
    }
    jfieldID lightAndWipers=env->GetFieldID(ptzConfigClass,"lightAndWipers","Ljava/util/List;");
    if(lightAndWipers==NULL)
    {
        printf("find lightAndWipers field id fail in Java_wsdd_nsmap_JNIPTZ_ONVIFAddPresetPoint\n");
    }else
    {
        printf("get lightAndWipers list object\n");
        jobject lightAndWipersObj=env->GetObjectField(ptzConfigObj,lightAndWipers);
        if(lightAndWipersObj==NULL)
        {
            printf("get lightAndWiper obj fail\n");
        }
        printf("get lightAndWipers list class\n");
        jclass listClass=env->GetObjectClass(lightAndWipersObj);
        printf("get lightAndWipers list add method\n");
        jmethodID listAddId=env->GetMethodID(listClass,"add","(Ljava/lang/Object;)Z");
        tt__PTZNode* ptzNode=nodes_resp.PTZNode[0];
        for(num=0;num<ptzNode->__sizeAuxiliaryCommands;num++)
        {

            jstring addValue=CStr2jstring(env,ptzNode->AuxiliaryCommands[num]);
            printf("call add tolightAndWipers  list \n");
            env->CallBooleanMethod(lightAndWipersObj,listAddId,addValue);
        }
    }
    //EXIT:
    if (NULL != soap) {
        ONVIF_soap_delete(soap);
    }

    return ptzConfigObj;
}

/*
 * Class:     com_hirain_sdk_rtsp_JNIPTZ
 * Method:    ONVIFControlWiperAndLamp
 * Signature: (Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;)I
 */
JNIEXPORT jint JNICALL Java_com_hirain_sdk_rtsp_JNIPTZ_ONVIFControlWiperAndLamp
(JNIEnv *env, jobject, jstring jdeviceAddr, jstring jusrName, jstring jpassword,jstring jcommand)
{
    int result=0,num=0;
    struct soap* soap=NULL;
    struct tagCapabilities capa;
    struct tagProfile *profiles = NULL;

    const char* userName = jstring2CStr(env,jusrName);
    const char* passWord = jstring2CStr(env,jpassword);
    const char* deviceAddr = jstring2CStr(env,jdeviceAddr);
    char* command = jstring2CStr(env,jcommand);

    if (NULL != soap) {
        ONVIF_soap_delete(soap);
    }

    _trt__GetProfiles           dev_req;
    _trt__GetProfilesResponse   dev_resp;

    soap = ONVIF_soap_new(SOAP_SOCK_TIMEOUT);
    ONVIF_SetAuthInfo(soap, userName, passWord);

    result = ONVIF_GetCapabilities(deviceAddr, &capa, userName, passWord);
    if(capa.PTZXAddr[0]=='\0')
    {
        printf("get capa fail in Java_wsdd_nsmap_JNIPTZ_ONVIFControlWiperAndLamp\n");
        return -1;
    }
    printf("get capa is %d %s\n",result,capa.PTZXAddr);


    ONVIF_SetAuthInfo(soap, userName,passWord);
    result = soap_call___trt__GetProfiles(soap, capa.PTZXAddr, NULL, &dev_req, dev_resp);
    printf("GetProfiles ->result ====%d\n ",result);
    if(dev_resp.Profiles==NULL)
    {
        printf("get profile fail in Java_wsdd_nsmap_JNIPTZ_ONVIFAddPresetPoint\n");
        return -1;
    }
    char* token=dev_resp.Profiles[0]->token;
    printf("token is%s\n",token);

    _tptz__SendAuxiliaryCommand auxi_req;
    _tptz__SendAuxiliaryCommandResponse auxi_resp;

    ONVIF_SetAuthInfo(soap, userName,passWord);
    auxi_req.ProfileToken=token;
    auxi_req.AuxiliaryData=command;
    result=soap_call___tptz__SendAuxiliaryCommand(soap,capa.PTZXAddr,NULL,&auxi_req,auxi_resp);

    if(result!=0)
        soap_perror(soap,NULL);
    //EXIT:
    if (NULL != soap) {
        ONVIF_soap_delete(soap);
    }

    return result;
}

/*
 * Class:     com_hirain_sdk_rtsp_JNIPTZ
 * Method:    ONVIFFocus
 * Signature: (Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;F)I
 */
JNIEXPORT jint JNICALL Java_com_hirain_sdk_rtsp_JNIPTZ_ONVIFFocus
(JNIEnv *env , jobject, jstring jdeviceAddr, jstring jusrName, jstring jpassword, jfloat jspeed)
{
    int result=0,profileNum=0;
    struct tagCapabilities capa;
    struct tagProfile *profiles = NULL;

    const char* userName = jstring2CStr(env,jusrName);
    const char* passWord = jstring2CStr(env,jpassword);
    const char* deviceAddr = jstring2CStr(env,jdeviceAddr);
    float speed=jspeed;

    result = ONVIF_GetCapabilities(deviceAddr, &capa, userName, passWord);
    if(result!=0)
    {
        printf("get capabilites fail in Java_wsdd_nsmap_JNIPTZ_ONVIFFocus\n");
        return result;
    }
    profileNum=ONVIF_GetProfiles(capa.MediaXAddr, &profiles, userName, passWord);
    if(profileNum<=0)

    {
        printf("get profile fail in Java_wsdd_nsmap_JNIPTZ_ONVIFFocus;\n");
        return -1;
    }

    struct soap* soap=NULL;
    if(NULL == (soap = ONVIF_soap_new(SOAP_SOCK_TIMEOUT)))
    {
        printf("soap is null in Java_wsdd_nsmap_JNIPTZ_ONVIFFocus\n");
        return -2;
    }
    ONVIF_SetAuthInfo(soap, userName, passWord);
    _timg__Move movereq;
    _timg__MoveResponse moveresp;
    movereq.VideoSourceToken=profiles[0].videoSourceToken;
    movereq.Focus=soap_new_tt__FocusMove(soap);
    movereq.Focus->Continuous=soap_new_tt__ContinuousFocus(soap);
    movereq.Focus->Continuous->Speed=speed;
    result=soap_call___timg__Move(soap,capa.IMGXAddr,NULL,&movereq,moveresp);
    if(result!=0)
        soap_perror(soap,NULL);

    //EXIT:
    if (NULL != soap) {
        ONVIF_soap_delete(soap);
    }
    return result;
}

/*
 * Class:     com_hirain_sdk_rtsp_JNIPTZ
 * Method:    ONVIFStopFocus
 * Signature: (Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;)I
 */
JNIEXPORT jint JNICALL Java_com_hirain_sdk_rtsp_JNIPTZ_ONVIFStopFocus
  (JNIEnv *env , jobject, jstring jdeviceAddr, jstring jusrName, jstring jpassword)
{
    int result=0,profileNum=0;
    struct tagCapabilities capa;
    struct tagProfile *profiles = NULL;

    const char* userName = jstring2CStr(env,jusrName);
    const char* passWord = jstring2CStr(env,jpassword);
    const char* deviceAddr = jstring2CStr(env,jdeviceAddr);

    result = ONVIF_GetCapabilities(deviceAddr, &capa, userName, passWord);
    if(result!=0)
    {
        printf("get capabilites fail in Java_wsdd_nsmap_JNIPTZ_ONVIFFocus\n");
        return result;
    }
    profileNum=ONVIF_GetProfiles(capa.MediaXAddr, &profiles, userName, passWord);
    if(profileNum<=0)

    {
        printf("get profile fail in Java_wsdd_nsmap_JNIPTZ_ONVIFFocus;\n");
        return -1;
    }

    struct soap* soap=NULL;
    if(NULL == (soap = ONVIF_soap_new(SOAP_SOCK_TIMEOUT)))
    {
        printf("soap is null in Java_wsdd_nsmap_JNIPTZ_ONVIFFocus\n");
        return -2;
    }
    ONVIF_SetAuthInfo(soap, userName, passWord);
    _timg__Stop stopReq;
    _timg__StopResponse stopResp;

     stopReq.VideoSourceToken=profiles[0].videoSourceToken;
     result=soap_call___timg__Stop(soap,capa.IMGXAddr,NULL,&stopReq,stopResp);
     if(result!=0)
         soap_perror(soap,NULL);

     //EXIT:
     if (NULL != soap) {
         ONVIF_soap_delete(soap);
     }
     return result;

}
