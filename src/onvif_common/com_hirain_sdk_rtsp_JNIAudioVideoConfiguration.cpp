#include "com_hirain_sdk_rtsp_JNIAudioVideoConfiguration.h"
#include "onvifSDK_com.h"

/*
 * Class:     com_hirain_sdk_rtsp_JNIAudioVideoConfiguration
 * Method:    ONVIFGetVideoEncoderConfiguration
 * Signature: (Ljava/lang/String;ILjava/lang/String;Ljava/lang/String;)Lcom/hirain/sdk/entity/rtsp/AudioVideoConfiguration;
 */
JNIEXPORT jobject JNICALL Java_com_hirain_sdk_rtsp_JNIAudioVideoConfiguration_ONVIFGetVideoEncoderConfiguration
(JNIEnv *env, jobject jthis, jstring jdeviceAddr, jint jType, jstring jusrName, jstring jpassword)
{
    const char* userName = jstring2CStr(env,jusrName);
    const char* passWord = jstring2CStr(env,jpassword);
    const char* deviceAddr = jstring2CStr(env,jdeviceAddr);

    //jni初始化
    jclass jClass = env->FindClass("com/hirain/sdk/JNIEntity/AudioVideoConfiguration");
    jmethodID jMethod = env->GetMethodID(jClass, "<init>", "()V");
    jobject jObject = env->NewObject(jClass, jMethod);


    int profile_cnt = 0;                                                        // 设备配置文件个数
    struct tagProfile *profiles = NULL;                                         // 设备配置文件列表
    struct tagCapabilities capa;                                                // 设备能力信息

    ONVIF_GetCapabilities(deviceAddr, &capa,userName,passWord);                // 获取设备能力信息（获取媒体服务地址）
    profile_cnt = ONVIF_GetProfiles(capa.MediaXAddr, &profiles,jstring2CStr(env,jusrName),jstring2CStr(env,jpassword));

    // 获取媒体配置信息（主/辅码流配置信息）
    if(profile_cnt <= 0)
    {
        printf("GetProfiles failed === %d\n",profile_cnt);
    }
    struct tagVideoEncoderConfiguration venc;
    char *vencToken = NULL;
    char *audioToken = NULL;
    printf("profile_cnt ==== %d\n",profile_cnt);

    if(jType<0||jType>1)
        jType=0;
    for(int i = 0; i < profile_cnt; i++)
    {
        vencToken = NULL;
        audioToken = NULL;
        //查找传递进来的码流类型，是否匹配
        if(i==jType)
        {
            vencToken = profiles[i].venc.token;
            audioToken = profiles[i].Audiotoken;
            break;
        }
    }

    int result = 0;
    struct soap *soap = NULL;
    _trt__GetVideoEncoderConfiguration          req;
    _trt__GetVideoEncoderConfigurationResponse  rep;

    SOAP_ASSERT(NULL != (soap = ONVIF_soap_new(SOAP_SOCK_TIMEOUT)));

    req.ConfigurationToken = vencToken;
    printf("req.ConfigurationToken == %s\n",req.ConfigurationToken);

    ONVIF_SetAuthInfo(soap, userName,passWord);
    result = soap_call___trt__GetVideoEncoderConfiguration(soap, deviceAddr, NULL, &req, rep);

    if(0 != result)
    {
        printf("GetVideoEncoderConfiguration failed\n");
        return NULL;
    }

    if(NULL != vencToken)
    {
        // jfieldID streamTypeID = env->GetFieldID(jClass,"streamType","Ljava/lang/String;");
        // env->SetObjectField(jObject,streamTypeID,CStr2jstring(env,streamType));
    }
    if(NULL != rep.Configuration)
    {
        jfieldID EncodingID = env->GetFieldID(jClass,"videoEncode","Ljava/lang/String;");
        env->SetObjectField(jObject,EncodingID,CStr2jstring(env,dump_enum2str_VideoEncoding(rep.Configuration->Encoding)));

        if(NULL != rep.Configuration->Resolution){
            jfieldID widthID = env->GetFieldID(jClass,"width","I");
            env->SetIntField(jObject,widthID,rep.Configuration->Resolution->Width);

            jfieldID heightID = env->GetFieldID(jClass,"height","I");
            env->SetIntField(jObject,heightID,rep.Configuration->Resolution->Height);
        }

        jfieldID qualityID = env->GetFieldID(jClass,"imageQuality","F");
        env->SetFloatField(jObject,qualityID,rep.Configuration->Quality);

        if(NULL != rep.Configuration->RateControl){
            jfieldID FrameRateLimitID = env->GetFieldID(jClass,"FrameRateLimit","I");
            env->SetIntField(jObject,FrameRateLimitID,rep.Configuration->RateControl->FrameRateLimit);

            jfieldID BitrateLimitID = env->GetFieldID(jClass,"BitrateLimit","I");
            env->SetIntField(jObject,BitrateLimitID,rep.Configuration->RateControl->BitrateLimit);
        }
        if(NULL != rep.Configuration->H264){
            jfieldID frameSpaceID = env->GetFieldID(jClass,"GovLength","I");
            env->SetIntField(jObject,frameSpaceID,rep.Configuration->H264->GovLength);
        }
    }

    //获取音频编码
    _trt__GetAudioEncoderConfiguration reQ;
    _trt__GetAudioEncoderConfigurationResponse reP;

    reQ.ConfigurationToken = audioToken;
    printf("reQ.ConfigurationToken = %s,=============%s\n",reQ.ConfigurationToken,audioToken);

    ONVIF_SetAuthInfo(soap, userName,passWord);
    result = soap_call___trt__GetAudioEncoderConfiguration(soap, deviceAddr, NULL, &reQ, reP);

    if(0 != result)
    {
        soap_perror(soap,NULL);
        if (NULL != soap) {
            ONVIF_soap_delete(soap);
        }
        return jObject;
    }
    printf("Encoding ==== %d\n", reP.Configuration->Encoding);
    if(NULL != reP.Configuration){
        jfieldID adudioEncodID = env->GetFieldID(jClass,"audioEncode","Ljava/lang/String;");
        env->SetObjectField(jObject,adudioEncodID,CStr2jstring(env,dump_enum2str_AudioEncoding(reP.Configuration->Encoding)));
    }

    if (NULL != soap) {
        ONVIF_soap_delete(soap);
    }
    return jObject;
}

/*
 * Class:     com_hirain_sdk_rtsp_JNIAudioVideoConfiguration
 * Method:    ONVIFSetVideoEncoderConfiguration
 * Signature: (Lcom/hirain/sdk/entity/rtsp/AudioVideoConfiguration;Ljava/util/List;)Ljava/util/List;
 */
JNIEXPORT jobject JNICALL Java_com_hirain_sdk_rtsp_JNIAudioVideoConfiguration_ONVIFSetVideoEncoderConfiguration
(JNIEnv *env ,jobject jthis, jobject jobj, jobject jarraylist)
{
    /*************************获取 jobj对象中的参数*************************************************/
    //printf("1\n");
    //jni初始化
    jclass jClass = env->GetObjectClass(jobj);
    if (NULL == jClass)
    {
        if (env->ExceptionOccurred())
        {
            env->ExceptionDescribe();
            printf("get AudioVideoConfiguration failed \n");
            return NULL;
        }
    }
    AUDIOVIDEOCONF conf;

    //获取视屏类型
    jfieldID VideoTypeID = env->GetFieldID(jClass, "videoType", "Ljava/lang/String;");
    if (NULL == VideoTypeID)
    {
        if (env->ExceptionOccurred())
        {
            env->ExceptionDescribe();
            printf("get videoType failed \n");
            return NULL;
        }
    }
    jstring VideoType = (jstring)env->GetObjectField(jobj, VideoTypeID);
    strcpy(conf.videoType, (char*)env->GetStringUTFChars(VideoType, 0));
    //printf("3\n");
    //视屏格式
    jfieldID EncodingID = env->GetFieldID(jClass, "videoEncode", "Ljava/lang/String;");
    if (NULL == EncodingID)
    {
        if (env->ExceptionOccurred())
        {
            env->ExceptionDescribe();
            printf("get videoEncode failed \n");
            return NULL;
        }
    }
    jstring vidoEncod	= (jstring)env->GetObjectField(jobj, EncodingID);
    strcpy(conf.videoEncode,(char*)env->GetStringUTFChars(vidoEncod, 0));

    //图像质量
    jfieldID qualityID = env->GetFieldID(jClass, "imageQuality", "F");
    if (NULL == qualityID)
    {
        if (env->ExceptionOccurred())
        {
            env->ExceptionDescribe();
            printf("get imageQuality failed \n");
            return NULL;
        }
    }
    jfloat quality = env->GetFloatField(jobj, qualityID);
    conf.imageQuality = quality;
    //printf("7\n");
    //码率上限
    jfieldID FrameRateLimitID = env->GetFieldID(jClass, "FrameRateLimit", "I");
    if (NULL == FrameRateLimitID)
    {
        if (env->ExceptionOccurred())
        {
            env->ExceptionDescribe();
            printf("get FrameRateLimit failed \n");
            return NULL;
        }
    }
    jint FrameRateLimit = env->GetIntField(jobj, FrameRateLimitID);
    conf.FrameRateLimit = FrameRateLimit;
    //视频帧率
    jfieldID BitrateLimitID = env->GetFieldID(jClass, "BitrateLimit", "I");
    if (NULL == BitrateLimitID)
    {
        if (env->ExceptionOccurred())
        {
            env->ExceptionDescribe();
            printf("get BitrateLimit failed \n");
            return NULL;
        }
    }
    jint BitrateLimit = env->GetIntField(jobj, BitrateLimitID);
    conf.BitrateLimit = BitrateLimit;
    //printf("9\n");
    //I帧间隔
    jfieldID GovLengthID = env->GetFieldID(jClass, "GovLength", "I");
    if (NULL == GovLengthID)
    {
        if (env->ExceptionOccurred())
        {
            env->ExceptionDescribe();
            printf("get GovLength failed \n");
            return NULL;
        }
    }
    jint GovLength = env->GetIntField(jobj, GovLengthID);
    conf.GovLength= GovLength;
    //printf("10\n");
    //音频编码
    jfieldID adudioEncodID = env->GetFieldID(jClass, "audioEncode", "Ljava/lang/String;");
    if (NULL == adudioEncodID)
    {
        if (env->ExceptionOccurred())
        {
            env->ExceptionDescribe();
            printf("get audioEncode failed \n");
            return NULL;
        }
    }
    jstring adudioEncod = (jstring)env->GetObjectField(jobj, adudioEncodID);
    strcpy(conf.audioEncode, (char*)env->GetStringUTFChars(adudioEncod, 0));

    //主码流还是辅码流
    jfieldID typeID = env->GetFieldID(jClass, "type", "I");
    if (NULL == typeID)
    {
        if (env->ExceptionOccurred())
        {
            env->ExceptionDescribe();
            printf("get type failed \n");
            return NULL;
        }
    }
    jint type = env->GetIntField(jobj, typeID);
    //0 main 1 minor others not case
    if(type<0||type>1)
        type=0;
    //printf("11\n");
    /***********************************************************************************************/

    /*************************获取ArrayList对象****************************************************/
    jclass jcs_alist = env->GetObjectClass(jarraylist);
    if (NULL == jcs_alist)
    {
        if (env->ExceptionOccurred())
        {
            env->ExceptionDescribe();
            printf("get arraylist failed \n");
            return NULL;
        }
    }
    //获取Arraylist的methodid
    jmethodID alist_get = env->GetMethodID(jcs_alist, "get", "(I)Ljava/lang/Object;");
    if (NULL == alist_get)
    {
        if (env->ExceptionOccurred())
        {
            env->ExceptionDescribe();
            printf("get method failed \n");
            return NULL;
        }
    }
    jmethodID alist_size = env->GetMethodID(jcs_alist, "size", "()I");//I 前面必须保留（）。
    if (NULL == alist_size)
    {
        if (env->ExceptionOccurred())
        {
            env->ExceptionDescribe();
            printf("arraylist size method error\n");
            return NULL;
        }
    }
    jint len = env->CallIntMethod(jarraylist, alist_size);
    printf("len =====%d\n",len);

    /************************************************************************************************/
    /*保存视频参数修改的结果（IP和是否成功），返回一个java的list*/
    //获取ArrayList类引用
    jclass cls_ArrayList = env->FindClass("java/util/ArrayList");
    if (NULL == cls_ArrayList)
    {
        if (env->ExceptionOccurred())
        {
            env->ExceptionDescribe();
            printf("cls_ArrayList method error\n");
            return NULL;
        }
    }
    //获取ArrayList构造函数id
    jmethodID construct = env->GetMethodID(cls_ArrayList, "<init>", "()V");
    if (NULL == construct)
    {
        if (env->ExceptionOccurred())
        {
            env->ExceptionDescribe();
            printf("construct method error\n");
            return NULL;
        }
    }
    //创建一个ArrayList对象
    jobject obj_ArrayList = env->NewObject(cls_ArrayList, construct, "");
    if (NULL == obj_ArrayList)
    {
        if (env->ExceptionOccurred())
        {
            env->ExceptionDescribe();
            printf("obj_ArrayList method error\n");
            return NULL;
        }
    }
    //获取ArrayList对象的add()的methodID
    jmethodID arrayList_add = env->GetMethodID(cls_ArrayList, "add", "(Ljava/lang/Object;)Z");
    if (NULL == arrayList_add)
    {
        if (env->ExceptionOccurred())
        {
            env->ExceptionDescribe();
            printf("arrayList_add method error\n");
            return NULL;
        }
    }
    //获取DeviceConfigResult类 //com.hirain.sdk.JNIEntity.DeviceConfigResult
    jclass cls_user = env->FindClass("com/hirain/sdk/JNIEntity/DeviceConfigResult");
    if (NULL == cls_user)
    {
        if (env->ExceptionOccurred())
        {
            env->ExceptionDescribe();
            printf("DeviceConfigResult method error\n");
            return NULL;
        }
    }
    //获取DeviceConfigResult的构造函数
    jmethodID construct_user = env->GetMethodID(cls_user, "<init>", "()V");
    if (NULL == construct_user)
    {
        if (env->ExceptionOccurred())
        {
            env->ExceptionDescribe();
            printf("construct_user method error\n");
            return NULL;
        }
    }
    /**********************************************************************************************/
    for (int j = 0; j < len; j++)
    {
        //printf("12 ==========%d\n",j);
        //获取DeviceParam对象
        jobject device_obj = env->CallObjectMethod(jarraylist, alist_get, j);
        if (NULL == device_obj)
        {
            if (env->ExceptionOccurred())
            {
                env->ExceptionDescribe();
                printf("获取DeviceParam对象 failed \n");
                return NULL;
            }
        }
        //printf("13 ==========\n");
        //获取DeviceParam类
        jclass Device_cls = env->GetObjectClass(device_obj);
        if (NULL == Device_cls)
        {
            if (env->ExceptionOccurred())
            {
                env->ExceptionDescribe();
                printf("获取DeviceParam类 failed \n");
                return NULL;
            }

        }

        jmethodID streamTypeId = env->GetMethodID(Device_cls, "getStreamType", "()Ljava/lang/String;");
        if (NULL == streamTypeId)
        {
            if (env->ExceptionOccurred())
            {
                env->ExceptionDescribe();
                printf("获取getStreamType failed \n");
                return NULL;
            }

        }
        jmethodID factoryId = env->GetMethodID(Device_cls, "getFactory", "()Ljava/lang/String;");
        if (NULL == factoryId)
        {
            if (env->ExceptionOccurred())
            {
                env->ExceptionDescribe();
                printf("获取getFactory failed \n");
                return NULL;
            }

        }
        jmethodID onvifUrlId = env->GetMethodID(Device_cls, "getOnvifUrl", "()Ljava/lang/String;");
        if (NULL == onvifUrlId)
        {
            if (env->ExceptionOccurred())
            {
                env->ExceptionDescribe();
                printf("获取getOnvifUrl failed \n");
                return NULL;
            }
        }
        jmethodID userNameId = env->GetMethodID(Device_cls, "getUserName", "()Ljava/lang/String;");
        if (NULL == userNameId)
        {
            if (env->ExceptionOccurred())
            {
                env->ExceptionDescribe();
                printf("获取getUserName failed \n");
                return NULL;
            }
        }
        jmethodID passWordId = env->GetMethodID(Device_cls, "getPassWord", "()Ljava/lang/String;");
        if (NULL == passWordId)
        {
            if (env->ExceptionOccurred())
            {
                env->ExceptionDescribe();
                printf("获取getPassWord failed \n");
                return NULL;
            }
        }
        jmethodID widthID = env->GetMethodID(Device_cls, "getWidth", "()I");
        if (NULL == widthID)
        {
            if (env->ExceptionOccurred())
            {
                env->ExceptionDescribe();
                printf("获取getWidth failed \n");
                return NULL;
            }
        }
        int width = (jint)env->CallIntMethod(device_obj, widthID);
        printf("width === %d\n", width);
        jmethodID heightID = env->GetMethodID(Device_cls, "getHeight", "()I");
        if (NULL == heightID)
        {
            if (env->ExceptionOccurred())
            {
                env->ExceptionDescribe();
                printf("获取getHeight failed \n");
                return NULL;
            }
        }
        int height = (jint)env->CallIntMethod(device_obj, heightID);
        printf("height === %d\n", height);

        jstring streamType = (jstring)env->CallObjectMethod(device_obj, streamTypeId);
        jstring factory = (jstring)env->CallObjectMethod(device_obj, factoryId);
        jstring onvifUr = (jstring)env->CallObjectMethod(device_obj, onvifUrlId);
        jstring UserName = (jstring)env->CallObjectMethod(device_obj, userNameId);
        jstring PassWord = (jstring)env->CallObjectMethod(device_obj, passWordId);
        if(NULL == streamType || NULL == factory || NULL == onvifUr ||
                NULL == UserName || NULL == PassWord)
        {
            printf("streamType factory onvifUr UserName PassWord \n");
            continue;
        }

        const char *streamtype = jstring2CStr(env,streamType);
        //		printf("streamtype=====%s\n", streamtype);
        const char* Factory = jstring2CStr(env,factory);
        //		printf("Factory=====%s\n", Factory);
        const char* DeviceXAddr = jstring2CStr(env,onvifUr);
        //		printf("DeviceXAddr=====%s\n", DeviceXAddr);
        const char* userName = jstring2CStr(env,UserName);
        //		printf("userName=====%s\n", userName);
        const char* passWord = jstring2CStr(env,PassWord);
        //		printf("passWord=====%s\n", passWord);

        /*************************获取设备能力**********************************************************/
        int profile_cnt = 0, result = 0;
        struct tagProfile *profiles = NULL;                                         // 设备配置文件列表
        struct tagCapabilities capa;                                                // 设备能力信息

        result = ONVIF_GetCapabilities(DeviceXAddr, &capa, userName, passWord);					// 获取设备能力信息（获取媒体服务地址）
        if (0 != result)
        {
            printf(" 获取设备能力信息 failed \n");
            return NULL;
        }
        profile_cnt = ONVIF_GetProfiles(capa.MediaXAddr, &profiles, userName, passWord);// 获取媒体配置信息（主/辅码流配置信息）
        printf("profile_cnt =====%d\n", profile_cnt);

        for (int i = 0; i < profile_cnt; i++)
        {
            //if (!strcmp(profiles[i].Name, streamtype))//判断码流
            // if(i==0)//设置主码流
            if(i==type)
            {
                struct tagVideoEncoderConfiguration venc;
                char *vencToken = profiles[i].venc.token;
                venc = profiles[i].venc;
                venc.Height = height;
                venc.Width = width;
                int videoResult = -2, audioResult = -2;

                result = ONVIF_GetVideoEncoderConfigurationOptions(capa.MediaXAddr, vencToken, userName, passWord);  // 获取该码流支持的视频编码器参数选项集
                printf("result ====%d\n", result);
                result = ONVIF_GetVideoEncoderConfiguration(capa.MediaXAddr, vencToken, userName, passWord);         // 获取该码流当前的视频编码器参数
                printf("result1 ====%d\n", result);
                videoResult = ONVIF_SetVideoEncoderConfiguration(capa.MediaXAddr, &venc, conf, userName, passWord);             // 设置该码流当前的视频编码器参数
                printf("result2 ====%d\n", result);
                if (!strcmp(Factory, "HIKVISION"))
                {
                    result = ONVIF_GetAudioEncoderConfigurationOptions(capa.MediaXAddr, profiles[i].Audiotoken, userName, passWord);	// 获取该码流支持的音频编码器参数选项集
                    printf("result3 ====%d\n", result);
                    result = ONVIF_GetAudioEncoderConfiguration(capa.MediaXAddr, profiles[i].Audiotoken, userName, passWord);		// 获取该码流当前的音频编码器参数
                    printf("result4 ====%d\n", result);
                    audioResult = ONVIF_SetAudioEncoderConfiguration(capa.MediaXAddr, profiles[i].Audiotoken, conf, userName, passWord);		// 设置该码流当前的音频编码器参数
                    printf("result5 ====%d\n", result);
                }
                //保存ONVIF 接口执行结果
                jobject obj_user = env->NewObject(cls_user, construct_user, "");
                jfieldID addressID = env->GetFieldID(cls_user, "deviceAddr", "Ljava/lang/String;");
                env->SetObjectField(obj_user, addressID, onvifUr);

                jfieldID videoResultID = env->GetFieldID(cls_user, "videoResult", "I");
                env->SetIntField(obj_user, videoResultID, videoResult);

                jfieldID audioResultID = env->GetFieldID(cls_user, "audioResult", "I");
                env->SetIntField(obj_user, audioResultID, audioResult);

                env->CallObjectMethod(obj_ArrayList, arrayList_add, obj_user);
            }
        }
        /***********************************************************************************************/
        if (NULL != profiles) {
            free(profiles);
            profiles = NULL;
        }
    }
    /***********************************************************************************************/

    return obj_ArrayList;
}

/*
 * Class:     com_hirain_sdk_rtsp_JNIAudioVideoConfiguration
 * Method:    ONVIFGetVideoChannelParam
 * Signature: (Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;)Ljava/lang/Object;
 */
JNIEXPORT jobject JNICALL Java_com_hirain_sdk_rtsp_JNIAudioVideoConfiguration_ONVIFGetVideoChannelParam
(JNIEnv * env, jobject, jstring jdeviceAddr, jstring jusrName, jstring jpassword)
{
    int ret=0,profileNum=0;
    struct tagCapabilities capa;
    struct tagProfile *profiles = NULL;
    struct imgConfigure conf;
    if(jdeviceAddr==NULL||jusrName==NULL||jpassword==NULL) {
        printf("[ERROR]: jdeviceAddr=%p,jusrName=%p,jpassword=%p\n\n", jdeviceAddr, jusrName, jpassword);
        return NULL;
    }

    const char* userName = jstring2CStr(env,jusrName);
    const char* passWord = jstring2CStr(env,jpassword);
    const char* deviceAddr = jstring2CStr(env,jdeviceAddr);

    const char* className = "com/hirain/domain/entity/operation/VideoChannelParam";
    jclass VideoSourceConfiguration = env->FindClass(className);
    if(VideoSourceConfiguration==NULL) {
        printf("[ERROR]: FindClass function failed '%s'\n\n", className);
        return NULL;
    }
    jobject obj=env->AllocObject(VideoSourceConfiguration);
    if(obj==NULL) {
        printf("[ERROR]: AllocObject function failed. \n\n");
        return NULL;
    }
    ret = ONVIF_GetCapabilities(deviceAddr, &capa, userName, passWord);
    if(ret!=0){
        printf("[ERROR]: ONVIF_GetCapabilities function failed [ret=%d] \n\n", ret);
        return NULL;
    }
    profileNum=ONVIF_GetProfiles(capa.MediaXAddr, &profiles, userName, passWord);
    if(profileNum<=0||capa.IMGXAddr==NULL) {
        printf("[ERROR]: ONVIF_GetProfiles function failed [ret=%d,IMGXAddr=%p] \n\n", profileNum, capa.IMGXAddr);
        return NULL;
    }

    ret=ONVIF_GetPictureConfiguretion(capa.IMGXAddr,profiles[0].videoSourceToken,&conf,userName,passWord);
    if(ret!=0) {
        printf("[ERROR]: ONVIF_GetPictureConfiguretion function failed [ret=%d] \n\n", profileNum);
        return NULL;
    }
    // printf("get field 1\n");
    jfieldID brightnessID=env->GetFieldID(VideoSourceConfiguration,"brightness","F");
    // printf("set field 1\n");
    env->SetFloatField(obj,brightnessID,conf.brightness);

    jfieldID contrastID=env->GetFieldID(VideoSourceConfiguration,"contrast","F");
    //printf("set field 2\n");
    env->SetFloatField(obj,contrastID,conf.contrast);

    jfieldID  coloraturationID=env->GetFieldID(VideoSourceConfiguration,"colorSaturation","F");
    // printf("set field 3\n");
    env->SetFloatField(obj,coloraturationID,conf.colorsaturation);

    // printf("get field 4\n");
    jfieldID brightnessMINID=env->GetFieldID(VideoSourceConfiguration,"brightnessMin","F");
    // printf("set field 4\n");
    env->SetFloatField(obj,brightnessMINID,conf.brightnessMin);

    jfieldID contrastMINID=env->GetFieldID(VideoSourceConfiguration,"contrastMin","F");
    // printf("set field 5\n");
    env->SetFloatField(obj,contrastMINID,conf.contrastMin);

    jfieldID  coloraturationMINID=env->GetFieldID(VideoSourceConfiguration,"colorSaturationMin","F");
    // printf("set field 6\n");
    env->SetFloatField(obj,coloraturationMINID,conf.colorsaturationMin);

    // printf("get field 7\n");
    jfieldID brightnessMaxID=env->GetFieldID(VideoSourceConfiguration,"brightnessMax","F");
    //printf("set field 7\n");
    env->SetFloatField(obj,brightnessMaxID,conf.brightnessMax);

    jfieldID contrastMaxID=env->GetFieldID(VideoSourceConfiguration,"contrastMax","F");
    // printf("set field 8\n");
    env->SetFloatField(obj,contrastMaxID,conf.contrastMax);

    jfieldID  coloraturationMaxID=env->GetFieldID(VideoSourceConfiguration,"colorSaturationMax","F");
    // printf("set field 9\n");
    env->SetFloatField(obj,coloraturationMaxID,conf.colorsaturationMax);

    return obj;
}

/*
 * Class:     com_hirain_sdk_rtsp_JNIAudioVideoConfiguration
 * Method:    ONVFISetVideoChannelParam
 * Signature: (Ljava/lang/Object;Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;)I
 */
JNIEXPORT jint JNICALL Java_com_hirain_sdk_rtsp_JNIAudioVideoConfiguration_ONVFISetVideoChannelParam
(JNIEnv * env , jobject, jobject VideoSourceConfigurationObj,jstring jdeviceAddr, jstring jusrName, jstring jpassword)
{
    int ret=0,profileNum=0;
    struct tagCapabilities capa;
    struct tagProfile *profiles = NULL;
    struct imgConfigure conf;

    if((NULL==VideoSourceConfigurationObj) || (NULL==jdeviceAddr) || (NULL==jusrName) || (NULL== jpassword)) {
        printf("[ERROR]: VideoSourceConfigurationObj=%p,jdeviceAddr=%p,jusrName=%p,jpassword=%p\n\n", VideoSourceConfigurationObj, jdeviceAddr, jusrName, jpassword);
        return -1;
    }

    const char* userName = jstring2CStr(env,jusrName);
    const char* passWord = jstring2CStr(env,jpassword);
    const char* deviceAddr = jstring2CStr(env,jdeviceAddr);
    if ((NULL == deviceAddr) || (NULL == userName) || (NULL == passWord)) {
        printf("[ERROR]: deviceAddr=%p,userName=%p,passWord=%p\n\n", deviceAddr, userName, passWord);
        return -1;
    }
    ret = ONVIF_GetCapabilities(deviceAddr, &capa, userName, passWord);
    if(ret!=0) {
        printf("[ERROR]: ONVIF_GetCapabilities function failed [ret=%d]\n\n", ret);
        return ret;
    }
    profileNum=ONVIF_GetProfiles(capa.MediaXAddr, &profiles, userName, passWord);
    if(profileNum<=0){
        printf("[ERROR]: ONVIF_GetProfiles function failed [ret=%d]\n\n", profileNum);
        return -1;
    }

    jclass VideoSourceConfiguration = env->GetObjectClass(VideoSourceConfigurationObj);
    // printf("get field name 1\n");
    jfieldID brightnessID=env->GetFieldID(VideoSourceConfiguration,"brightness","F");
    // printf("get field 1\n");
    conf.brightness=env->GetFloatField(VideoSourceConfigurationObj,brightnessID);

    jfieldID contrastID=env->GetFieldID(VideoSourceConfiguration,"contrast","F");
    // printf("get field 2\n");
    conf.contrast=env->GetFloatField(VideoSourceConfigurationObj,contrastID);

    jfieldID  coloraturationID=env->GetFieldID(VideoSourceConfiguration,"colorSaturation","F");
    // printf("set field 3\n");
    conf.colorsaturation=env->GetFloatField(VideoSourceConfigurationObj,coloraturationID);

    ret=ONVIF_SetPictureConfiguretion(capa.IMGXAddr,profiles[0].videoSourceToken,&conf,userName,passWord);

    return ret;
}

/*
 * Class:     com_hirain_sdk_rtsp_JNIAudioVideoConfiguration
 * Method:    ONVIFSetWaterMark
 * Signature: (Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;)I
 */
JNIEXPORT jint JNICALL Java_com_hirain_sdk_rtsp_JNIAudioVideoConfiguration_ONVIFSetWaterMark
(JNIEnv *env , jobject,  jstring jdeviceAddr, jstring jusrName, jstring jpassword, jstring jshowText)
{
    int result=0,profileNum=0,pos=0;
    struct tagCapabilities capa;
    struct tagProfile *profiles = NULL;
    struct soap* soap=NULL;
    bool isAdd=false;

    const char* userName = jstring2CStr(env,jusrName);
    const char* passWord = jstring2CStr(env,jpassword);
    const char* deviceAddr = jstring2CStr(env,jdeviceAddr);
    char* showText = jstring2CStr(env,jshowText);

    result = ONVIF_GetCapabilities(deviceAddr, &capa, userName, passWord);
    if(result!=0)
    {
        printf("get capabilites fail in Java_wsdd_nsmap_JNIAudioVideoConfiguration_ONVIFSetWaterMark\n");
        return result;
    }
    profileNum=ONVIF_GetProfiles(capa.MediaXAddr, &profiles, userName, passWord);
    if(profileNum<=0)

    {
        printf("get profile fail in Java_wsdd_nsmap_JNIAudioVideoConfiguration_ONVIFSetWaterMark;\n");
        return -1;
    }

    if(NULL == (soap = ONVIF_soap_new(SOAP_SOCK_TIMEOUT)))
    {
        printf("soap is null in Java_wsdd_nsmap_JNIAudioVideoConfiguration_ONVIFSetWaterMark\n");
        return -2;
    }
    ONVIF_SetAuthInfo(soap, userName, passWord);

    _trt__GetOSDs getOsdreq;
    _trt__GetOSDsResponse getOsdResp;
    result=soap_call___trt__GetOSDs(soap,capa.MediaXAddr,NULL,&getOsdreq,getOsdResp);
    if(result!=0)
    {
        printf("get osds fail in Java_wsdd_nsmap_JNIAudioVideoConfiguration_ONVIFSetWaterMark\n");
        soap_perror(soap,NULL);
        return result;
    }

    //replace text
    printf("osd num is %d\n",getOsdResp.__sizeOSDs);
    for(pos=0;pos<getOsdResp.__sizeOSDs;pos++)
    {
        if(getOsdResp.OSDs[pos]->TextString->PlainText!=NULL)
        {
            _trt__SetOSD  setOsdReq;
            setOsdReq.OSD=getOsdResp.OSDs[pos];
            getOsdResp.OSDs[pos]->TextString->PlainText=showText;
            _trt__SetOSDResponse setOsdResp;
            ONVIF_SetAuthInfo(soap, userName, passWord);
            result=soap_call___trt__SetOSD(soap,capa.MediaXAddr,NULL,&setOsdReq,setOsdResp);
            if(result!=0)
            {
                printf("set osd fail in Java_wsdd_nsmap_JNIAudioVideoConfiguration_ONVIFSetWaterMark\n");
                soap_perror(soap,NULL);
                return result;
            }
            isAdd=true;
            break;
        }
    }

    //add new
    if(isAdd==false)
    {
        ONVIF_SetAuthInfo(soap, userName, passWord);
        _trt__CreateOSD createReq;
        _trt__CreateOSDResponse createResp;
        //osd conf
        tt__OSDConfiguration osd;
        createReq.OSD=&osd;
        //set token
        tt__OSDReference videoToken;
        videoToken.__item=profiles[0].videoSourceToken;
        osd.VideoSourceConfigurationToken=&videoToken;
        //set type
        osd.Type=tt__OSDType__Text;

        //set text conf
        tt__OSDTextConfiguration textOsd;
        char* PositonType="Custom";
        char* textType="Plain";

        textOsd.Type=textType;
        textOsd.PlainText=showText;
        osd.TextString=&textOsd;

        //set postion
        tt__OSDPosConfiguration postion;
        postion.Type=PositonType;
        tt__Vector pos;
        //right down
        float x=0.298;
        float y=-0.818;
        pos.x=&x;
        pos.y=&y;
        postion.Pos=&pos;
        osd.Position=&postion;

        result=soap_call___trt__CreateOSD(soap,capa.MediaXAddr,NULL,&createReq,createResp);
        if(result!=0)
            soap_perror(soap,NULL);

    }

    //EXIT:
    if (NULL != soap) {
        ONVIF_soap_delete(soap);
    }
    return result;

}

/*
 * Class:     com_hirain_sdk_rtsp_JNIAudioVideoConfiguration
 * Method:    ONVIFBatchSetWaterMark
 * Signature: (Ljava/util/List;)Ljava/util/List;
 */
JNIEXPORT jobject JNICALL Java_com_hirain_sdk_rtsp_JNIAudioVideoConfiguration_ONVIFBatchSetWaterMark
(JNIEnv * env, jobject, jobject arglist)
{
    int result,pos,profileNum;
    jclass list_class=env->GetObjectClass(arglist);
    jmethodID list_get = env->GetMethodID(list_class, "get", "(I)Ljava/lang/Object;");
    jfieldID size_Id = env->GetFieldID(list_class, "size", "I");
    jint size= env->GetIntField(arglist,size_Id);

    printf("get list size %d\n",size);

    jclass device_param_class=env->FindClass("com/hirain/sdk/JNIEntity/DeviceConfigParam");

    if(device_param_class==NULL)
    {
        printf("find class fail in Java_wsdd_nsmap_JNIAudioVideoConfiguration_ONVIFBatchSetWaterMark\n");
        return arglist;
    }
    jfieldID address_ID=env->GetFieldID(device_param_class, "deviceAddr", "Ljava/lang/String;");
    if(address_ID==NULL)
    {
        printf("find address id fail in Java_wsdd_nsmap_JNIAudioVideoConfiguration_ONVIFBatchSetWaterMark\n");
        return arglist;
    }
    jfieldID username_ID=env->GetFieldID(device_param_class, "userName", "Ljava/lang/String;");
    if(username_ID==NULL)
    {
        printf("find user name id fail in Java_wsdd_nsmap_JNIAudioVideoConfiguration_ONVIFBatchSetWaterMark\n");
        return arglist;
    }

    jfieldID password_ID=env->GetFieldID(device_param_class, "passWord", "Ljava/lang/String;");
    if(password_ID==NULL)
    {
        printf("find password id fail in Java_wsdd_nsmap_JNIAudioVideoConfiguration_ONVIFBatchSetWaterMark\n");
        return arglist;
    }

    jfieldID mark_ID=env->GetFieldID(device_param_class, "mark", "Ljava/lang/String;");
    if(mark_ID==NULL)
    {
        printf("find mark id fail in Java_wsdd_nsmap_JNIAudioVideoConfiguration_ONVIFBatchSetWaterMark\n");
        return arglist;
    }

    jfieldID result_ID=env->GetFieldID(device_param_class, "waterResult", "I");
    if(result_ID==NULL)
    {
        printf("find result id fail in Java_wsdd_nsmap_JNIAudioVideoConfiguration_ONVIFBatchSetWaterMark\n");
        return arglist;
    }
    for(pos=0;pos<size;pos++)
    {
        jobject item = env->CallObjectMethod(arglist,list_get,pos);
        jstring addressStr=(jstring)env->GetObjectField(item,address_ID);
        jstring userNameStr=(jstring)env->GetObjectField(item,username_ID);
        jstring passWdStr=(jstring)env->GetObjectField(item,password_ID);
        jstring markStr=(jstring)env->GetObjectField(item,mark_ID);

        //set fault first
        env->SetIntField(item,result_ID,1);
        char* userName = jstring2CStr(env,userNameStr);
        char* passWord = jstring2CStr(env,passWdStr);
        char* deviceAddr = jstring2CStr(env,addressStr);
        char* showText = jstring2CStr(env,markStr);

        struct tagCapabilities capa;
        struct tagProfile *profiles = NULL;
        struct soap* soap=NULL;
        bool isAdd=false;
        int  osdPos;
        result = ONVIF_GetCapabilities(deviceAddr, &capa, userName, passWord);
        if(result!=0)
        {
            printf("get capabilites fail in %s at Java_wsdd_nsmap_JNIAudioVideoConfiguration_ONVIFBatchSetWaterMark\n",deviceAddr);
            continue;
        }
        profileNum=ONVIF_GetProfiles(capa.MediaXAddr, &profiles, userName, passWord);
        if(profileNum<=0)
        {
            printf("get profile fail in %s at Java_wsdd_nsmap_JNIAudioVideoConfiguration_ONVIFBatchSetWaterMark\n",deviceAddr);
            continue;
        }

        if(NULL == (soap = ONVIF_soap_new(SOAP_SOCK_TIMEOUT)))
        {
            printf("soap is null in %s at Java_wsdd_nsmap_JNIAudioVideoConfiguration_ONVIFBatchSetWaterMark\n",deviceAddr);
            continue;
        }
        ONVIF_SetAuthInfo(soap, userName, passWord);
        _trt__GetOSDs getOsdreq;
        _trt__GetOSDsResponse getOsdResp;
        result=soap_call___trt__GetOSDs(soap,capa.MediaXAddr,NULL,&getOsdreq,getOsdResp);
        if(result!=0)
        {
            printf("get osds fail in %s at Java_wsdd_nsmap_JNIAudioVideoConfiguration_ONVIFBatchSetWaterMark\n",deviceAddr);
            soap_perror(soap,NULL);
            ONVIF_soap_delete(soap);
            free(profiles);
            continue;
        }

        //replace text
        printf("osd num is %d\n",getOsdResp.__sizeOSDs);
        for(osdPos=0;osdPos<getOsdResp.__sizeOSDs;osdPos++)
        {
            if(getOsdResp.OSDs[osdPos]->TextString->PlainText!=NULL)
            {
                _trt__SetOSD  setOsdReq;
                setOsdReq.OSD=getOsdResp.OSDs[osdPos];
                getOsdResp.OSDs[osdPos]->TextString->PlainText=showText;
                _trt__SetOSDResponse setOsdResp;
                ONVIF_SetAuthInfo(soap, userName, passWord);
                result=soap_call___trt__SetOSD(soap,capa.MediaXAddr,NULL,&setOsdReq,setOsdResp);
                if(result!=0)
                {
                    printf("set osd fail in %s at Java_wsdd_nsmap_JNIAudioVideoConfiguration_ONVIFBatchSetWaterMark\n",deviceAddr);
                    soap_perror(soap,NULL);
                    ONVIF_soap_delete(soap);
                    free(profiles);
                    continue;
                }
                isAdd=true;
                break;
            }
        }

        //add new
        if(isAdd==false)
        {
            ONVIF_SetAuthInfo(soap, userName, passWord);
            _trt__CreateOSD createReq;
            _trt__CreateOSDResponse createResp;
            //osd conf
            tt__OSDConfiguration osd;
            createReq.OSD=&osd;
            //set token
            tt__OSDReference videoToken;
            videoToken.__item=profiles[0].videoSourceToken;
            osd.VideoSourceConfigurationToken=&videoToken;
            //set type
            osd.Type=tt__OSDType__Text;

            //set text conf
            tt__OSDTextConfiguration textOsd;
            char* PositonType="Custom";
            char* textType="Plain";

            textOsd.Type=textType;
            textOsd.PlainText=showText;
            osd.TextString=&textOsd;

            //set postion
            tt__OSDPosConfiguration postion;
            postion.Type=PositonType;
            tt__Vector location;
            //right down
            float x=0.298;
            float y=-0.818;
            location.x=&x;
            location.y=&y;
            postion.Pos=&location;
            osd.Position=&postion;

            result=soap_call___trt__CreateOSD(soap,capa.MediaXAddr,NULL,&createReq,createResp);
            if(result!=0)
            {
                soap_perror(soap,NULL);
                ONVIF_soap_delete(soap);
                free(profiles);
                continue;
            }
        }
        //next
        env->SetIntField(item,result_ID,0);
        if (NULL != soap) {
            ONVIF_soap_delete(soap);
        }
        free(profiles);

    }

    return arglist;
}

/*
 * Class:     com_hirain_sdk_rtsp_JNIAudioVideoConfiguration
 * Method:    ONVIFGetDeviceDefaultConfigure
 * Signature: (Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;I)Lcom/hirain/sdk/entity/rtsp/DeviceDefaultConfigure;
 */
JNIEXPORT jobject JNICALL Java_com_hirain_sdk_rtsp_JNIAudioVideoConfiguration_ONVIFGetDeviceDefaultConfigure
(JNIEnv *env , jobject,  jstring jdeviceAddr, jstring jusrName, jstring jpassword,jint type)
{
    int result=0,profileNum=0,pos=0;
    struct tagCapabilities capa;
    struct tagProfile *profiles = NULL;
    struct soap* soap=NULL;

    if(jdeviceAddr==NULL||jusrName==NULL||jpassword==NULL||type<0||type>1)
    {
        printf("arg error in Java_wsdd_nsmap_JNIAudioVideoConfiguration_ONVIFGetDeviceDefaultConfigure\n");
        return NULL;
    }
    jclass devicereSolution = env->FindClass("com/hirain/sdk/JNIEntity/DeviceResolution");
    if(devicereSolution==NULL)
    {
        printf("get class fail in Java_wsdd_nsmap_JNIAudioVideoConfiguration_ONVIFGetDeviceDefaultConfigure\n");
        return NULL;
    }

    jclass DeviceDefaultConfigure   = env->FindClass("com/hirain/sdk/JNIEntity/DeviceDefaultConfigure");
    if(DeviceDefaultConfigure  ==NULL)
    {
        printf("get class fail in Java_wsdd_nsmap_JNIAudioVideoConfiguration_ONVIFGetDeviceDefaultConfigure\n");
        return NULL;
    }
    jfieldID widthID= env->GetFieldID(devicereSolution, "width", "I");
    if(widthID==NULL)
    {
        printf("get width field fail in Java_wsdd_nsmap_JNIAudioVideoConfiguration_ONVIFGetDeviceDefaultConfigure\n");
        return NULL;
    }

    jfieldID heigthID= env->GetFieldID(devicereSolution, "heigth", "I");
    if(heigthID==NULL)
    {
        printf("get heigth field fail in Java_wsdd_nsmap_JNIAudioVideoConfiguration_ONVIFGetDeviceDefaultConfigure\n");
        return NULL;
    }

    jfieldID imageQualityMinID= env->GetFieldID(DeviceDefaultConfigure, "imageQualityMin", "I");
    if(imageQualityMinID==NULL)
    {
        printf("get imageQualityMinID field fail in Java_wsdd_nsmap_JNIAudioVideoConfiguration_ONVIFGetDeviceDefaultConfigure\n");
        return NULL;
    }

    jfieldID imageQualityMaxID= env->GetFieldID(DeviceDefaultConfigure, "imageQualityMax", "I");
    if(imageQualityMaxID==NULL)
    {
        printf("get imageQualityMaxID field fail in Java_wsdd_nsmap_JNIAudioVideoConfiguration_ONVIFGetDeviceDefaultConfigure\n");
        return NULL;
    }

    jfieldID videoFrameMinID= env->GetFieldID(DeviceDefaultConfigure, "videoFrameMin", "I");
    if(videoFrameMinID==NULL)
    {
        printf("get videoFrameMinID field fail in Java_wsdd_nsmap_JNIAudioVideoConfiguration_ONVIFGetDeviceDefaultConfigure\n");
        return NULL;
    }

    jfieldID videoFrameMaxID= env->GetFieldID(DeviceDefaultConfigure, "videoFrameMax", "I");
    if(videoFrameMaxID==NULL)
    {
        printf("get videoFrameMaxID field fail in Java_wsdd_nsmap_JNIAudioVideoConfiguration_ONVIFGetDeviceDefaultConfigure\n");
        return NULL;
    }

    jfieldID frameSpaceMinID= env->GetFieldID(DeviceDefaultConfigure, "frameSpaceMin", "I");
    if(frameSpaceMinID==NULL)
    {
        printf("get frameSpaceMinID field fail in Java_wsdd_nsmap_JNIAudioVideoConfiguration_ONVIFGetDeviceDefaultConfigure\n");
        return NULL;
    }

    jfieldID frameSpaceMaxID= env->GetFieldID(DeviceDefaultConfigure, "frameSpaceMax", "I");
    if(frameSpaceMaxID==NULL)
    {
        printf("get frameSpaceMaxID field fail in Java_wsdd_nsmap_JNIAudioVideoConfiguration_ONVIFGetDeviceDefaultConfigure\n");
        return NULL;
    }

    jfieldID deviceResolutionListID= env->GetFieldID(DeviceDefaultConfigure, "deviceResolutionList", "Ljava/util/List;");
    if(deviceResolutionListID==NULL)
    {
        printf("get deviceResolutionListID field fail in Java_wsdd_nsmap_JNIAudioVideoConfiguration_ONVIFGetDeviceDefaultConfigure\n");
        return NULL;
    }

    jmethodID construct_resolution = env->GetMethodID(devicereSolution, "<init>", "()V");
    jmethodID construct = env->GetMethodID(DeviceDefaultConfigure, "<init>", "()V");

    jobject objDeviceDefaultConfigure=env->NewObject(DeviceDefaultConfigure,construct);
    if(objDeviceDefaultConfigure==NULL)
    {
        printf("get objDeviceDefaultConfigure fail in Java_wsdd_nsmap_JNIAudioVideoConfiguration_ONVIFGetDeviceDefaultConfigure\n");
        return NULL;
    }

    jobject obj_ArrayList=env->GetObjectField(objDeviceDefaultConfigure,deviceResolutionListID);
    if(obj_ArrayList==NULL)//java new first
    {
        printf("get obj_ArrayList fail in Java_wsdd_nsmap_JNIAudioVideoConfiguration_ONVIFGetDeviceDefaultConfigure\n");
        return NULL;
    }

    jclass cls_ArrayList = env->GetObjectClass(obj_ArrayList);

    jmethodID arrayList_add = env->GetMethodID(cls_ArrayList, "add", "(Ljava/lang/Object;)Z");


    const char* userName = jstring2CStr(env,jusrName);
    const char* passWord = jstring2CStr(env,jpassword);
    const char* deviceAddr = jstring2CStr(env,jdeviceAddr);

    result = ONVIF_GetCapabilities(deviceAddr, &capa, userName, passWord);
    if(result!=0)
    {
        printf("get capabilites fail in Java_wsdd_nsmap_JNIAudioVideoConfiguration_ONVIFGetDeviceDefaultConfigure\n");
        return NULL;
    }
    profileNum=ONVIF_GetProfiles(capa.MediaXAddr, &profiles, userName, passWord);
    if(profileNum<=0||capa.IMGXAddr==NULL)

    {
        printf("get profile fail in Java_wsdd_nsmap_JNIAudioVideoConfiguration_ONVIFGetDeviceDefaultConfigure\n");
        return NULL;
    }
    soap = ONVIF_soap_new(SOAP_SOCK_TIMEOUT);
    ONVIF_SetAuthInfo(soap, userName, passWord);

    _trt__GetVideoEncoderConfigurationOptions req;
    _trt__GetVideoEncoderConfigurationOptionsResponse resp;

    req.ProfileToken=profiles[type].token;
    printf("%s\n",profiles[type].token);
    result=soap_call___trt__GetVideoEncoderConfigurationOptions(soap,capa.MediaXAddr,NULL,&req,resp);
    if(result!=0)
    {
        soap_perror(soap,NULL);
        if (NULL != soap) {
            ONVIF_soap_delete(soap);
        }
        return NULL;
    }
    else
    {
        printf("que %d %d\n",resp.Options->QualityRange->Min,resp.Options->QualityRange->Max);
        printf("framerate %d %d\n",resp.Options->H264->FrameRateRange->Min,resp.Options->H264->FrameRateRange->Max);
        printf("gop %d %d\n",resp.Options->H264->GovLengthRange->Min,resp.Options->H264->GovLengthRange->Max);
        printf("get available resolutions %d\n",resp.Options->H264->__sizeResolutionsAvailable);

        env->SetIntField(objDeviceDefaultConfigure,imageQualityMinID,resp.Options->QualityRange->Min);
        env->SetIntField(objDeviceDefaultConfigure,imageQualityMaxID,resp.Options->QualityRange->Max);
        env->SetIntField(objDeviceDefaultConfigure,videoFrameMinID,resp.Options->H264->FrameRateRange->Min);
        env->SetIntField(objDeviceDefaultConfigure,videoFrameMaxID,resp.Options->H264->FrameRateRange->Max);
        env->SetIntField(objDeviceDefaultConfigure,frameSpaceMinID,resp.Options->H264->GovLengthRange->Min);
        env->SetIntField(objDeviceDefaultConfigure,frameSpaceMaxID,resp.Options->H264->GovLengthRange->Max);
        for(pos=0;pos<resp.Options->H264->__sizeResolutionsAvailable;pos++)
        {
            printf("%d %d \n",resp.Options->H264->ResolutionsAvailable[pos]->Height,resp.Options->H264->ResolutionsAvailable[pos]->Width);
            jobject obj_resoluton = env->NewObject(devicereSolution, construct_resolution, "");
            env->SetIntField(obj_resoluton,widthID,resp.Options->H264->ResolutionsAvailable[pos]->Width);
            env->SetIntField(obj_resoluton,heigthID,resp.Options->H264->ResolutionsAvailable[pos]->Height);
            env->CallBooleanMethod(obj_ArrayList, arrayList_add, obj_resoluton);
        }
    }

    if (NULL != soap) {
        ONVIF_soap_delete(soap);
    }

    return objDeviceDefaultConfigure;
}

/*
 * Class:     com_hirain_sdk_rtsp_JNIAudioVideoConfiguration
 * Method:    ONVIFSetVideoAndAudioEncoderConfigure
 * Signature: (Lcom/hirain/sdk/entity/rtsp/AudioVideoConfiguration;Ljava/lang/String;Ljava/lang/String;)I
 */
JNIEXPORT jint JNICALL Java_com_hirain_sdk_rtsp_JNIAudioVideoConfiguration_ONVIFSetVideoAndAudioEncoderConfigure
(JNIEnv * env, jobject, jobject setConf, jstring jusrName, jstring jpassword)
{
    if(setConf==NULL||jusrName==NULL||jpassword==NULL)
    {
        printf("arg error in Java_wsdd_nsmap_JNIAudioVideoConfiguration_ONVIFSetVideoAndAudioEncoderConfigure\n");
        return -1;
    }
     AUDIOVIDEOCONF conf;
     memset(&conf,0,sizeof(AUDIOVIDEOCONF));

    jclass videoaudioconfig = env->GetObjectClass(setConf);
    if(videoaudioconfig==NULL)
    {
        printf("get class fail in Java_wsdd_nsmap_JNIAudioVideoConfiguration_ONVIFSetVideoAndAudioEncoderConfigure\n");
        return -1;
    }


    jfieldID typeID = env->GetFieldID(videoaudioconfig, "type", "I");
    if(typeID==NULL)
    {
        printf("get typeID fail in Java_wsdd_nsmap_JNIAudioVideoConfiguration_ONVIFSetVideoAndAudioEncoderConfigure\n");
        return -1;
    }
    int type=env->GetIntField(setConf,typeID);
    if(type<0||type>1)
        type=0;
    printf("set type is %d\n",type);

    jfieldID EncodingID = env->GetFieldID(videoaudioconfig, "videoEncode", "Ljava/lang/String;");
    if(EncodingID==NULL)
    {
        printf("get EncodingID fail in Java_wsdd_nsmap_JNIAudioVideoConfiguration_ONVIFSetVideoAndAudioEncoderConfigure\n");
        return -1;
    }
    jstring vidoEncod	= (jstring)env->GetObjectField(setConf, EncodingID);
    strcpy(conf.videoEncode,(char*)env->GetStringUTFChars(vidoEncod, 0));
    printf("set encode is %s\n",conf.videoEncode);

    jfieldID widthID = env->GetFieldID(videoaudioconfig, "width", "I");
    if(widthID==NULL)
    {
        printf("get widthID fail in Java_wsdd_nsmap_JNIAudioVideoConfiguration_ONVIFSetVideoAndAudioEncoderConfigure\n");
        return -1;
    }
    conf.width=env->GetIntField(setConf,widthID);
    printf("set width is %d\n",conf.width);

    jfieldID heightID = env->GetFieldID(videoaudioconfig, "height", "I");
    if(heightID==NULL)
    {
        printf("get heightID fail in Java_wsdd_nsmap_JNIAudioVideoConfiguration_ONVIFSetVideoAndAudioEncoderConfigure\n");
        return -1;
    }
    conf.height=env->GetIntField(setConf,heightID);
    printf("set height is %d\n",conf.height);

    jfieldID BitrateLimitID = env->GetFieldID(videoaudioconfig, "BitrateLimit", "I");
    if(BitrateLimitID==NULL)
    {
        printf("get BitrateLimitID fail in Java_wsdd_nsmap_JNIAudioVideoConfiguration_ONVIFSetVideoAndAudioEncoderConfigure\n");
        return -1;
    }
    conf.BitrateLimit=env->GetIntField(setConf,BitrateLimitID);
    printf("set BitrateLimit is %d\n",conf.BitrateLimit);

    jfieldID qualityID = env->GetFieldID(videoaudioconfig, "imageQuality", "F");
    if(qualityID==NULL)
    {
        printf("get qualityID fail in Java_wsdd_nsmap_JNIAudioVideoConfiguration_ONVIFSetVideoAndAudioEncoderConfigure\n");
        return -1;
    }
    conf.imageQuality=env->GetFloatField(setConf,qualityID);
    printf("set BitrateLimit is %f\n",conf.imageQuality);

    jfieldID FrameRateLimitID = env->GetFieldID(videoaudioconfig, "FrameRateLimit", "I");
    if(FrameRateLimitID==NULL)
    {
        printf("get FrameRateLimitID fail in Java_wsdd_nsmap_JNIAudioVideoConfiguration_ONVIFSetVideoAndAudioEncoderConfigure\n");
        return -1;
    }
    conf.FrameRateLimit=env->GetIntField(setConf,FrameRateLimitID);
    printf("set FrameRateLimit is %d\n",conf.FrameRateLimit);

    jfieldID GovLengthID = env->GetFieldID(videoaudioconfig, "GovLength", "I");
    if(GovLengthID==NULL)
    {
        printf("get GovLengthID fail in Java_wsdd_nsmap_JNIAudioVideoConfiguration_ONVIFSetVideoAndAudioEncoderConfigure\n");
        return -1;
    }
    conf.GovLength=env->GetIntField(setConf,GovLengthID);
    printf("set GovLength is %d\n",conf.GovLength);

    jfieldID adudioEncodID = env->GetFieldID(videoaudioconfig, "audioEncode", "Ljava/lang/String;");
    if(adudioEncodID==NULL)
    {
        printf("get adudioEncodID fail in Java_wsdd_nsmap_JNIAudioVideoConfiguration_ONVIFSetVideoAndAudioEncoderConfigure\n");
        return -1;
    }
    jstring audioCode	= (jstring)env->GetObjectField(setConf, adudioEncodID);
    strcpy(conf.audioEncode,(char*)env->GetStringUTFChars(audioCode, 0));
    printf("set encode is %s\n",conf.audioEncode);

    jfieldID OnvifUrlID = env->GetFieldID(videoaudioconfig, "onvifUrl", "Ljava/lang/String;");
    if(OnvifUrlID==NULL)
    {
        printf("get OnvifUrlID fail in Java_wsdd_nsmap_JNIAudioVideoConfiguration_ONVIFSetVideoAndAudioEncoderConfigure\n");
        return -1;
    }
    jstring addressStr	= (jstring)env->GetObjectField(setConf, OnvifUrlID);
    char* address=(char*)env->GetStringUTFChars(addressStr, 0);
    printf("set address is %s\n",address);

    jfieldID factoryID = env->GetFieldID(videoaudioconfig, "factory", "Ljava/lang/String;");
    if(factoryID==NULL)
    {
        printf("get factoryID fail in Java_wsdd_nsmap_JNIAudioVideoConfiguration_ONVIFSetVideoAndAudioEncoderConfigure\n");
        return -1;
    }
    jstring factoryStr= (jstring)env->GetObjectField(setConf, factoryID);
    char* factory=(char*)env->GetStringUTFChars(factoryStr, 0);
    printf("set factory is %s\n",factory);

    const char* userName = jstring2CStr(env,jusrName);
    const char* passWord = jstring2CStr(env,jpassword);

    int profile_cnt = 0, result = 0;
    struct tagProfile *profiles = NULL;                                         // 设备配置文件列表
    struct tagCapabilities capa;                                                // 设备能力信息

    result = ONVIF_GetCapabilities(address, &capa, userName, passWord);					// 获取设备能力信息（获取媒体服务地址）
    if (0 != result)
    {
        printf(" 获取设备能力信息 failed \n");
        return -1;
    }

    profile_cnt = ONVIF_GetProfiles(capa.MediaXAddr, &profiles, userName, passWord);// 获取媒体配置信息（主/辅码流配置信息）
    printf("profile_cnt =====%d\n", profile_cnt);
    struct tagVideoEncoderConfiguration venc;
    venc = profiles[type].venc;
    result=ONVIF_SetVideoEncoderConfiguration(capa.MediaXAddr, &venc, conf, userName, passWord);

    if(result!=0)
        return result;

    if (!strcmp(factory, "HIKVISION"))
    {
        result=ONVIF_SetAudioEncoderConfiguration(capa.MediaXAddr, profiles[type].Audiotoken, conf, userName, passWord);
    }
    return result;
}
