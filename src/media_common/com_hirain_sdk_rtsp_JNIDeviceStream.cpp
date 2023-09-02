#include <stdio.h>
#include <string.h>
#include "media.h"
#include "com_hirain_sdk_rtsp_JNIDeviceStream.h"
#include "log.h"

JavaVM* gjava_vm;
jstring  gtime_unit;

extern CMinIO* g_minio;
extern char g_log_path[256];


int jstring_to_char(JNIEnv* env, jobject obj, jfieldID field_id, char* buff, int size)
{
    int nRet = -1;
    if ((NULL == env) || (NULL == obj) || (NULL == field_id) || (NULL == buff) || (size <= 0)) {
        LOG("ERROR", "Input parameter error %p %p %p %p %d\n", env, obj, field_id, buff, size);
        return nRet;
    }
    jstring jstr = (jstring)env->GetObjectField(obj, field_id);
    if (NULL != jstr) {
        const char* str = env->GetStringUTFChars(jstr, false);
        if (NULL != str) {
            strncpy(buff, str, size - 1);
            env->ReleaseStringUTFChars(jstr, str);
            nRet = 0;
        }
        else LOG("ERROR", "GetStringUTFChars function failed.\n");
    }
    else LOG("WARN", "GetObjectField function failed.\n");
    return nRet;
}

int checkExc(JNIEnv *env) {
    if(env->ExceptionCheck()) {
        env->ExceptionDescribe(); // writes to logcat
        env->ExceptionClear();
        return 1;
    }
    return -1;
}

void JNU_ThrowByName(JNIEnv *env, const char *name, const char *msg)
{
    // 查找异常类
    jclass cls = env->FindClass(name);
    /* 如果这个异常类没有找到，VM会抛出一个NowClassDefFoundError异常 */
    if (cls != NULL) {
        env->ThrowNew(cls, msg);  // 抛出指定名字的异常
    }
    /* 释放局部引用 */
    env->DeleteLocalRef(cls);
}

/*
 * Class:     com_hirain_sdk_rtsp_JNIDeviceStream
 * Method:    init
 * Signature: (Lcom/hirain/sdk/entity/rtsp/LocalConfig;)I
 */
JNIEXPORT jint JNICALL Java_com_hirain_sdk_rtsp_JNIDeviceStream_init(JNIEnv* env, jobject, jobject cfg)
{
    if ((NULL == env) || (NULL == cfg)) {
        LOG("ERROR", "Input parameter error %p %p\n", env, cfg);
        return -1;
    }
    int nRet = -1;
    do {
        jclass cfg_cls = env->GetObjectClass(cfg);
        if (NULL == cfg_cls) {
            LOG("ERROR", "GetObjectClass function failed.\n");
            break;
        }
        jfieldID jlogPath = env->GetFieldID(cfg_cls, "logPath", "Ljava/lang/String;"); // logPath
        if (NULL == jlogPath) {
            LOG("ERROR", "GetFieldID function failed 'logPath'\n");
            break;
        }
        jstring_to_char(env, cfg, jlogPath, g_log_path, sizeof(g_log_path));
        jfieldID jminio = env->GetFieldID(cfg_cls, "minioProperties", "Lcom/hirain/common/tool/minio/config/MinioProperties;"); // minioProperties
        if (NULL == jminio) {
            LOG("ERROR", "GetFieldID function failed 'minioProperties'\n");
            break;
        }
        jobject jminio_obj = env->GetObjectField(cfg, jminio);
        if (NULL == jminio_obj) {
            LOG("ERROR", "GetObjectField function failed\n");
            break;
        }
        //////////////////////////////////////////////////////////////////////
        // MinIO配置对象
        jclass cls = env->GetObjectClass(jminio_obj);
        if (NULL == cls) {
            LOG("ERROR", "GetObjectClass function failed.\n");
            break;
        }
        jfieldID jendpoint = env->GetFieldID(cls, "endpoint", "Ljava/lang/String;"); // host
        if (NULL == jendpoint) {
            LOG("ERROR", "GetFieldID function failed 'endpoint'\n");
            break;
        }
        minio_info_t minio_info;
        jstring_to_char(env, jminio_obj, jendpoint, minio_info.host, sizeof(minio_info.host));
        jfieldID jport = env->GetFieldID(cls, "port", "I");   // port
        if (NULL == jport) {
            LOG("ERROR", "GetFieldID function failed 'port'\n");
            break;
        }
        minio_info.port = env->GetIntField(jminio_obj, jport);
        jfieldID jusername = env->GetFieldID(cls, "accessKey", "Ljava/lang/String;"); // Username
        if (NULL == jusername) {
            LOG("ERROR", "GetFieldID function failed 'accessKey'\n");
            break;
        }
        jstring_to_char(env, jminio_obj, jusername, minio_info.username, sizeof(minio_info.username));
        jfieldID jpassword = env->GetFieldID(cls, "secretKey", "Ljava/lang/String;"); // Password
        if (NULL == jpassword) {
            LOG("ERROR", "GetFieldID function failed 'secretKey'\n");
            break;
        }
        jstring_to_char(env, jminio_obj, jpassword, minio_info.password, sizeof(minio_info.password));
        jfieldID jsecure = env->GetFieldID(cls, "secure", "Ljava/lang/Boolean;"); // secure (若为true,则用https,否则用http,默认值为true)
        if (NULL == jsecure) {
            LOG("ERROR", "GetFieldID function failed 'secure'\n");
            break;
        }
        //minio_info.verify_ssl = env->GetBooleanField(jminio, jsecure);
        minio_info.verify_ssl = false;
        jfieldID jbucket = env->GetFieldID(cls, "bucketDefaultName", "Ljava/lang/String;"); // Bucket
        if (NULL == jbucket) {
            LOG("ERROR", "GetFieldID function failed 'bucketDefaultName'\n");
            break;
        }
        jstring_to_char(env, jminio_obj, jbucket, minio_info.bucket, sizeof(minio_info.bucket));
        if (NULL == g_minio) {
            g_minio = new CMinIO();
            g_minio->print_info(&minio_info);
            g_minio->init(&minio_info);
        }
        //////////////////////////////////////////////////////////////////////
        nRet = 0;
    } while (false);
    return nRet;
}

/*
 * Class:     com_hirain_sdk_rtsp_JNIDeviceStream
 * Method:    rtspSlice
 * Signature: (Ljava/lang/String;Ljava/lang/Object;Ljava/lang/String;)I
 */
JNIEXPORT jint JNICALL Java_com_hirain_sdk_rtsp_JNIDeviceStream_rtspSlice
(JNIEnv * env , jobject obj, jstring rtspURl, jobject contrlCmd, jstring SlicePath)
{
    int ret;
    const char *rtsp_url=NULL;
    const char *slice_filepath=NULL;
    if(env==NULL||rtspURl==NULL||contrlCmd==NULL||SlicePath==NULL)
    {
        printf("error arg in Java_wsdd_nsmap_JNIDeviceStream_rtspSlice\n");
        return ERROR_ARG;
    }
    rtsp_url = env->GetStringUTFChars(rtspURl, 0);
    slice_filepath=env->GetStringUTFChars(SlicePath, 0);
    char * control_cmd = (char*)env->GetDirectBufferAddress(contrlCmd);
    // printf("get address char is %c %c\n",*control_cmd,*(control_cmd+1));
    ret=rtsp_slice(rtsp_url,slice_filepath,control_cmd);
    env->ReleaseStringUTFChars(rtspURl,rtsp_url);
    env->ReleaseStringUTFChars(SlicePath,slice_filepath);
    return ret;
}

/*
 * Class:     com_hirain_sdk_rtsp_JNIDeviceStream
 * Method:    rtspSliceAutoStop
 * Signature: (Ljava/lang/String;Ljava/lang/Object;Ljava/lang/String;ILjava/lang/Object;)I
 */
JNIEXPORT jint JNICALL Java_com_hirain_sdk_rtsp_JNIDeviceStream_rtspSliceAutoStop
(JNIEnv * env , jobject, jstring rtspURl, jobject contrlCmd, jstring SlicePath, jint stopSecond,jobject databaseobj)
{

    int ret,returnCode;
    const char *rtsp_url=NULL;
    const char *slice_filepath=NULL;
    char record_time_buff[10]={0};
    JNIEnv* localenv;

    if(env==NULL||rtspURl==NULL||contrlCmd==NULL||SlicePath==NULL||stopSecond<=0)
    {
        printf("error arg in Java_wsdd_nsmap_JNIDeviceStream_rtspSliceAutoStop\n");
        return ERROR_ARG;
    }
    env->GetJavaVM(&gjava_vm);
    rtsp_url = env->GetStringUTFChars(rtspURl, 0);
    slice_filepath=env->GetStringUTFChars(SlicePath, 0);
    char * control_cmd = (char*)env->GetDirectBufferAddress(contrlCmd);
    // printf("get address char is %c %c\n",*control_cmd,*(control_cmd+1));
    int video_type=3;
    int record_time=0;

    returnCode=rtsp_slice_autostop(rtsp_url,slice_filepath,control_cmd,stopSecond,&video_type,&record_time);

    //printf("end is %d %d %d\n",video_type,record_time,returnCode);
    //add to database
    ret=gjava_vm->AttachCurrentThread((void**)&localenv,NULL);
    if(ret<0)
    {
        printf("get env error in Java_wsdd_nsmap_JNIDeviceStream_rtspSliceAutoStop\n");
        return -1;
    }
    jclass database_class=localenv->GetObjectClass(databaseobj);
    if(database_class==NULL)
    {
        printf("get object class fail in Java_wsdd_nsmap_JNIDeviceStream_rtspSliceAutoStop\n");
        return -1;
    }
    jmethodID set_duration = localenv->GetMethodID(database_class, "setDuration", "(Ljava/lang/String;)V");
    jmethodID set_encode = localenv->GetMethodID(database_class, "setVideoEncode", "(I)V");
    if(set_duration==NULL||set_encode==NULL)
    {
        printf("get method id fail in Java_wsdd_nsmap_JNIDeviceStream_rtspSliceAutoStop\n");
        return -1;
    }
    int hour;
    if(record_time>3600)
    {
        hour=1;
        record_time-=3600;
    }
    else
        hour=0;
    int minute=record_time/60;
    int second=record_time%60;

    memset(record_time_buff,0,10);
    sprintf(record_time_buff,"%02d:%02d:%02d",hour,minute,second);
    //printf("get buff is %s\n",record_time_buff);

   // printf("get time string\n");
    jstring recode_time_str=localenv->NewStringUTF(record_time_buff);
   // printf("call set duration\n");
    localenv->CallVoidMethod(databaseobj,set_duration,recode_time_str);
    //printf("call set encode\n");
    localenv->CallVoidMethod(databaseobj,set_encode,video_type);

    // printf("end \n");
    localenv->ReleaseStringUTFChars(rtspURl,rtsp_url);
    localenv->ReleaseStringUTFChars(SlicePath,slice_filepath);
    return returnCode;
}

int custom_stream_output(void* opaque, unsigned char* buf, int buf_size)
{
    const char *upload_id = (char*)opaque;
    if ((NULL != buf) && (buf_size > 0) && (NULL != upload_id)) {
        g_minio->upload_part(upload_id, (char*)buf, buf_size);
    }
    return buf_size;
}
/*
 * Class:     com_hirain_sdk_rtsp_JNIDeviceStream
 * Method:    rtspSliceFrame
 * Signature: (Ljava/lang/String;Ljava/lang/Object;Ljava/lang/String;ILjava/lang/Object;I)I
 */
JNIEXPORT jint JNICALL Java_com_hirain_sdk_rtsp_JNIDeviceStream_rtspSliceFrame
  (JNIEnv * env , jobject, jstring rtspURl, jobject contrlCmd, jstring SlicePath, jint stopSecond,jobject databaseobj,jint frameInterval)
{
    char record_time_buff[10]={0};
    LOG("INFO", "------ rtspSliceFrame start ------\n");
    if(env==NULL||rtspURl==NULL||contrlCmd==NULL||SlicePath==NULL||stopSecond<=0||frameInterval<=0) {
        LOG("ERROR", "Input parameter error, %p, %p, %p, %p, %d, %d\n", env, rtspURl, contrlCmd, SlicePath, stopSecond, frameInterval);
        return ERROR_ARG;
    }
    const char* rtsp_url = NULL;
    const char* remote_name = NULL;
    JNIEnv* localenv = NULL;
    int nRet = -1;
    do {
        env->GetJavaVM(&gjava_vm);
        if ((NULL == gjava_vm) || (NULL == g_minio)) {
            LOG("ERROR", "gjava_vm=%p, g_minio=%p\n", gjava_vm, g_minio);
            break;
        }
        rtsp_url = env->GetStringUTFChars(rtspURl, 0);
        remote_name = env->GetStringUTFChars(SlicePath, 0);
        char* control_cmd = (char*)env->GetDirectBufferAddress(contrlCmd);
        if ((NULL == remote_name) || (NULL == rtsp_url) || (NULL == control_cmd)) {
            LOG("ERROR", "Input parameter error [rtsp_url:%p,remote_name:%p,control_cmd:%p]\n", rtsp_url, remote_name, control_cmd);
            break;
        }
        LOG("INFO", "JNI params [rtsp=%s,filename=%s,stopSecond=%d,frameInterval=%d]\n", rtsp_url? rtsp_url:"null", remote_name ? remote_name : "null", stopSecond, frameInterval);
        int video_type = 3, record_time = 0;
    //#define FRAME_UPLOAD

    #ifdef FRAME_UPLOAD
        Aws::String upload_id;
        nRet = g_minio->create_multipart_upload(remote_name, upload_id);
        if (nRet < 0) {
            LOG("ERROR","create_multipart function failed %d\n", nRet);
            break;
        }
        LOG("INFO", "Create multipart succeed ^-^ [id:%s,ret:%d]\n", upload_id.c_str(), nRet);
        if (frameInterval == 1)
            nRet = rtsp_slice_autostop(rtsp_url, NULL, control_cmd, stopSecond, &video_type, &record_time, custom_stream_output, (void*)upload_id.c_str());
        else
            nRet = rtsp_slice_codeautostop(rtsp_url, NULL, control_cmd, stopSecond, &video_type, &record_time, frameInterval, custom_stream_output, (void*)upload_id.c_str());
        if (NO_ERRORS != nRet) {
            LOG("ERROR", "Video record failed %d\n", nRet);
            break;
        }
        g_minio->complete_multipart_upload(upload_id.c_str());
    #else
        static int64_t num = 0;
        char output[128] = { 0 };
        sprintf(output, "./%lld.mp4", num++);
        if (frameInterval == 1)
            nRet = rtsp_slice_autostop(rtsp_url, output, control_cmd, stopSecond, &video_type, &record_time, NULL, NULL);
        else
            nRet = rtsp_slice_codeautostop(rtsp_url, output, control_cmd, stopSecond, &video_type, &record_time, frameInterval, NULL, NULL);
        if (NO_ERRORS != nRet) {
            LOG("ERROR", "Video record failed %d\n", nRet);
            break;
        }
        nRet = g_minio->upload_big(output, remote_name);
        if (nRet < 0) {
            LOG("ERROR", "Upload file failed %d [local_file:%s,remote_file:%s]\n", nRet, output,remote_name);
            break;
        }
        int state = remove(output);
        if (0 == state) {
            LOG("INFO", "Remove file succeed %d '%s'\n", state, output);
        }
        else LOG("ERROR", "Remove file failed %d '%s'\n", state, output);
    #endif

        int ret = gjava_vm->AttachCurrentThread((void**)&localenv, NULL);
        if ((ret < 0) || (NULL == localenv)) {
            LOG("ERROR", "AttachCurrentThread function failed %d\n", ret);
            break;
        }
        jclass database_class = localenv->GetObjectClass(databaseobj);
        if (database_class == NULL) {
            LOG("ERROR", "GetObjectClass function failed.\n");
            break;
        }
        jmethodID set_duration = localenv->GetMethodID(database_class, "setDuration", "(Ljava/lang/String;)V");
        jmethodID set_encode = localenv->GetMethodID(database_class, "setVideoEncode", "(I)V");
        if (set_duration == NULL || set_encode == NULL) {
            LOG("ERROR", "GetMethodID function failed %p,%p\n", set_duration, set_encode);
            break;
        }
        int hour = 0;
        if (record_time > 3600) {
            hour = 1;
            record_time -= 3600;
        }
        else
            hour = 0;
        int minute = record_time / 60;
        int second = record_time % 60;
        memset(record_time_buff, 0, 10);
        sprintf(record_time_buff, "%02d:%02d:%02d", hour, minute, second);
        jstring recode_time_str = localenv->NewStringUTF(record_time_buff);
        localenv->CallVoidMethod(databaseobj, set_duration, recode_time_str);
        localenv->CallVoidMethod(databaseobj, set_encode, video_type);
    } while (false);
    if (NULL != localenv) {
        if(NULL != rtsp_url) 
            localenv->ReleaseStringUTFChars(rtspURl, rtsp_url);
        if(NULL != remote_name)
            localenv->ReleaseStringUTFChars(SlicePath, remote_name);
        gjava_vm->DetachCurrentThread();
    }
    LOG("INFO", "------ rtspSliceFrame end %d ------\n", nRet);
    return nRet;
}

/**
 * tran a char* to jsring
 * env jinenv
 * charstring
 *
 */
jstring char_to_jstring(JNIEnv* env, const char* charstring)
{
    jclass strClass = env->FindClass("Ljava/lang/String;");
    jmethodID ctorID = env->GetMethodID(strClass, "<init>", "([BLjava/lang/String;)V");
    jbyteArray bytes = env->NewByteArray(strlen(charstring));
    env->SetByteArrayRegion(bytes, 0, strlen(charstring), (jbyte*)charstring);
    jstring encoding = env->NewStringUTF("utf-8");
    return (jstring)(env->NewObject(strClass, ctorID, bytes, encoding));
}


typedef struct _write_ignite_arg
{
    jstring   cacheName;
    jstring   data_key;
    jstring   mark_key;
    jstring   time_unit;
    char*   write_ignite_ok;
}write_ignite_arg;


/*
 * write to ignite
 *opaque arg
 *buf data buff
 * size  data buff size
*/
//int write_ignite(void *opaque, unsigned char *buf, int buf_size)
//{
//    int ret;
//    JNIEnv* env;
//    write_ignite_arg* arg=(write_ignite_arg*)opaque;
//    ret=gjava_vm->AttachCurrentThread((void**)&env,NULL);
//    if(ret<0)
//    {
//        printf("get env error in write_ignite\n");
//        return -1;
//    }
//    jclass lgnite_class = env->FindClass("com/hirain/common/utils/IgniteClientUtil");
//    //jclass lgnite_class = env->FindClass("com/hirain/common/tool/ignite/util/IgniteUtil");
//    jmethodID getInstance = env->GetStaticMethodID(lgnite_class, "getInstance","()Lcom/hirain/common/utils/IgniteClientUtil;");
//    printf("get obj\n");
//    jobject ignite_obj=env->CallStaticObjectMethod(lgnite_class,getInstance);
//    if(buf==NULL&&buf_size==0)
//    {
//        printf("write start \n");
//        //mark start put data
//        jstring start_mark=char_to_jstring(env,"start");
//        jmethodID putCacheValue=env->GetMethodID(lgnite_class,"putCacheValue", "(Ljava/lang/String;Ljava/lang/String;Ljava/lang/Object;)Z");
//        jboolean result=env->CallBooleanMethod(ignite_obj,putCacheValue,arg->cacheName,arg->mark_key,start_mark);
//        printf("write start end\n");
//        return result;
//    }
//    printf("write cache start \n");
//    jbyteArray bytes = 0;
//    bytes =env->NewByteArray( buf_size);
//
//    if (bytes != NULL) {
//        env->SetByteArrayRegion(bytes, 0, buf_size,(jbyte *)buf);
//    }
//    jmethodID putCacheValueWithExpiry=env->GetMethodID(lgnite_class,"putCacheValueWithExpiry", "(Ljava/lang/String;Ljava/lang/String;ILjava/lang/String;Ljava/lang/Object;)Z");
//    jint expiry_time=CACHE_TIME_SECOND;
//    printf("write cache start 1\n");
//    jboolean result=env->CallBooleanMethod(ignite_obj,putCacheValueWithExpiry,arg->cacheName,arg->time_unit,expiry_time,arg->data_key,bytes);
//    //jboolean result=arg->env->CallBooleanMethod(ignite_obj,putCacheValue,arg->cacheName,arg->key,buf_pointer);
//    printf("putCacheValue result is %d size is %d \n",result,buf_size);
//    arg->write_ignite_ok[1]='w';
//    env->DeleteLocalRef (bytes);
//    gjava_vm->DetachCurrentThread();
//
//    if(result==false)
//        return -1;
//    else
//        return buf_size;
//}

int write_ignite(void* opaque, unsigned char* buf, int buf_size)
{
    int ret = 0;
    JNIEnv* env = NULL;
    write_ignite_arg* arg = (write_ignite_arg*)opaque;
    do {
        ret = gjava_vm->AttachCurrentThread((void**)&env, NULL);
        if (ret < 0) {
            LOG("ERROR", "AttachCurrentThread failed %d\n", ret);
            ret = -1;
            break;
        }
        jclass lgnite_class = env->FindClass("com/hirain/common/tool/ignite/util/IgniteUtil");
        if (NULL == lgnite_class) {
            LOG("ERROR", "FindClass failed %p\n", lgnite_class);
            ret = -1;
            break;
        }
        jmethodID jsetValue = env->GetStaticMethodID(lgnite_class, "setValue", "(Ljava/lang/String;Ljava/lang/String;Ljava/lang/Object;)V");
        if (NULL == jsetValue) {
            LOG("ERROR", "GetStaticMethodID failed %p\n", jsetValue);
            ret = -1;
            break;
        }
        //jobject ignite_obj = env->CallStaticObjectMethod(lgnite_class, getInstance);
        if (buf == NULL && buf_size == 0) {
            LOG("INFO", "write start\n");
            jstring start_mark = char_to_jstring(env, "start");
            env->CallStaticVoidMethod(lgnite_class, jsetValue, arg->cacheName, arg->mark_key, start_mark);
            LOG("INFO", "write end\n");
            ret = 0;
            break;
        }
        LOG("INFO", "write cache start\n");
        jbyteArray bytes = env->NewByteArray(buf_size);
        if (NULL == bytes) {
            LOG("ERROR", "NewByteArray failed %d\n", buf_size);
            ret = -1;
            break;
        }
        env->SetByteArrayRegion(bytes, 0, buf_size, (jbyte*)buf);

        jmethodID putCacheValueWithExpiry = env->GetStaticMethodID(lgnite_class, "putCacheValueWithExpiry", "(Ljava/lang/String;Ljava/lang/String;ILjava/lang/String;Ljava/lang/Object;)Z");
        if (NULL == putCacheValueWithExpiry) {
            LOG("ERROR", "GetMethodID 'putCacheValueWithExpiry' failed.\n");
            env->DeleteLocalRef(bytes);
            ret = -1;
            break;
        }
        jint expiry_time = CACHE_TIME_SECOND;
        jboolean result = env->CallStaticBooleanMethod(lgnite_class, putCacheValueWithExpiry, arg->cacheName, arg->time_unit, expiry_time, arg->data_key, bytes);
        //jboolean result=arg->env->CallBooleanMethod(ignite_obj,putCacheValue,arg->cacheName,arg->key,buf_pointer);
        printf("putCacheValue result is %d size is %d \n", result, buf_size);
        arg->write_ignite_ok[1] = 'w';
        env->DeleteLocalRef(bytes);
        gjava_vm->DetachCurrentThread();
        ret = (false == result) ? -1 : buf_size;
    } while (false);
    return ret;
 }

/*
 * Class:     com_hirain_sdk_rtsp_JNIDeviceStream
 * Method:    rtspStream
 * Signature: (Ljava/lang/String;Ljava/lang/Object;Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;Ljava/lang/Object;)I
 */
JNIEXPORT jint JNICALL Java_com_hirain_sdk_rtsp_JNIDeviceStream_rtspStream
(JNIEnv * env, jobject obj, jstring rtspURl, jobject contrlCmd, jstring cacheName, jstring data_key,jstring mark_key,jobject writeigniteok)
{

    int ret;

    const char *rtsp_url= env->GetStringUTFChars(rtspURl, 0);

    env->GetJavaVM(&gjava_vm);
    jstring time_unit=char_to_jstring(env,"SECOND");

    write_ignite_arg arg;
    arg.cacheName=cacheName;
    arg.data_key=data_key;
    arg.mark_key=mark_key;
    arg.write_ignite_ok=(char*)env->GetDirectBufferAddress(writeigniteok);
    arg.time_unit=time_unit;
    char * control_cmd = (char*)env->GetDirectBufferAddress(contrlCmd);
    ret = rtsp_stream(rtsp_url, (void*)&arg, write_ignite, control_cmd);
    env->ReleaseStringUTFChars(rtspURl,rtsp_url);
    return ret;
}

typedef struct _write_websocket_arg
{
    jstring   channelID;
    jstring  token;
}write_websocket_arg;

/*
 * write to websocket
 *opaque arg
 *buf data buff
 * size  data buff size
*/
int write_websocket_bytebuffer_thread(void *opaque, unsigned char *buf, int buf_size)
{
    int ret;
    JNIEnv* env;
    if(buf_size<=0)
        return buf_size;

    write_websocket_arg* arg=(write_websocket_arg*)opaque;

    // printf("start write_websocket_bytebuffer_thread\n");

    ret=gjava_vm->AttachCurrentThread((void**)&env,NULL);
    if(ret<0)
    {
        printf("get env error in write_websocket_bytebuffer_thread\n");
        return -1;
    }
    // printf("get class in  write_websocket_bytebuffer_thread\n");
    jclass websocket_class_thread= env->FindClass("com/hirain/media/websocket/WebSocketServer");
    if (websocket_class_thread== NULL)
    {
        printf("get websocket_class error in write_websocket_bytebuffer_thread\n");
        return -2;
    }
    //  printf("get method in  write_websocket_bytebuffer_thread\n");
    jmethodID websocketsend_thread= env->GetStaticMethodID(websocket_class_thread, "sendByteBuffer","([BLjava/lang/String;Ljava/lang/String;)V");
    if (websocketsend_thread== NULL)
    {
        printf("get websocketsend error in write_websocket_bytebuffer_thread\n");
        return -3;
    }
    //  printf("new array in  write_websocket_bytebuffer_thread\n");
    jbyteArray bytes = 0;
    bytes =env->NewByteArray(buf_size);

    //printf("set array in  write_websocket_bytebuffer_thread\n");
    if (bytes != NULL) {
        env->SetByteArrayRegion(bytes, 0, buf_size,(jbyte *)buf);
    }
    else {
        printf("NewByteArray error in write_websocket_bytebuffer_thread\n");
        return -4;
    }

    //printf("start websocket send in write_websocket_bytebuffer_thread\n");
    env->CallStaticObjectMethod(websocket_class_thread,websocketsend_thread,bytes,arg->channelID,arg->token);
    // printf("end websocket send in write_websocket_bytebuffer_thread\n");

    env->DeleteLocalRef (bytes);
    gjava_vm->DetachCurrentThread();
    return buf_size;
}
extern "C"
{
#include  "libavutil/time.h"
};
/*
 * write to websocket
 *opaque arg
 *buf data buff
 * size  data buff size
*/
int write_websocket_bytebuffer_rtsp_thread(void *opaque, unsigned char *buf, int buf_size)
{
    //printf("websocket: data=%p,size=%d  time:%lld\n", buf, buf_size, time(NULL));
    if ((NULL == buf) || (buf_size <= 0)) {
        LOG("ERROR","buf=%p, buf_size:%d\n", buf, buf_size);
        return buf_size;
    }
    JNIEnv* env = NULL;
    write_websocket_arg* arg = (write_websocket_arg*)opaque;
    int ret = gjava_vm->AttachCurrentThread((void**)&env,NULL);
    if((ret < 0) || (!env)) {
        LOG("ERROR","AttachCurrentThread function failed [ret:%d,env:%p]\n", ret, env);
        return -1;
    }
    jclass cls = env->FindClass("com/hirain/media/websocket/WebSocketServer");
    if (NULL == cls){
        LOG("ERROR","FindClass function failed %p\n", cls);
        return -2;
    }
    jmethodID  jsend = env->GetStaticMethodID(cls, "sendDataInfo","([BLjava/lang/String;Ljava/lang/String;)V");
    if (jsend == NULL){
        LOG("ERROR","GetStaticMethodID function failed %p\n", jsend);
        return -3;
    }
    jbyteArray bytes = 0;
    bytes = env->NewByteArray(buf_size);
    if (bytes != NULL) {
        //printf("Send data %d bytes [time:%lld]\n", buf_size, time(NULL));
        //int64_t t1 = av_gettime() / 1000;
        env->SetByteArrayRegion(bytes, 0, buf_size,(jbyte*)buf);
        //int64_t t2 = av_gettime() / 1000;
        env->CallStaticObjectMethod(cls, jsend, bytes, arg->channelID, arg->token);
        //int64_t t3 = av_gettime() / 1000;
        env->DeleteLocalRef(bytes);
        //int64_t t4 = av_gettime() / 1000;
        //printf("t2-t1:%lld, t3-t2:%lld, t4-t3:%lld [size:%d, time:%lld]\n", (t2-t1), (t3-t2), (t4-t3), buf_size, time(NULL));
    }
    else {
        LOG("ERROR","NewByteArray function failed %p\n", bytes);
        return -4;
    }
    gjava_vm->DetachCurrentThread();
    return buf_size;
}
/*
 * Class:     com_hirain_sdk_rtsp_JNIDeviceStream
 * Method:    sliceStreamPlay
 * Signature: (Ljava/lang/String;ILjava/lang/Object;Ljava/lang/Object;Ljava/lang/String;Ljava/lang/String;)I
 */
JNIEXPORT jint JNICALL Java_com_hirain_sdk_rtsp_JNIDeviceStream_sliceStreamPlay
(JNIEnv * env , jobject, jstring slice_name , jint start_time, jobject contrlCmd ,jobject speed, jstring channelID,jstring token)
{
    int ret;
    if(env==NULL||slice_name==NULL||contrlCmd==NULL||channelID==NULL||token==NULL||speed==NULL)
    {
        printf("error arg in Java_wsdd_nsmap_JNIDeviceStream_sliceStreamPlay\n");
        return ERROR_ARG;
    }

    env->GetJavaVM(&gjava_vm);
    const char *slice_locate = env->GetStringUTFChars(slice_name, 0);
    char * control_cmd = (char*)env->GetDirectBufferAddress(contrlCmd);
    write_websocket_arg arg;
    memset(&arg,0,sizeof(arg));
    arg.channelID=channelID;
    arg.token =token;
    float * playspeed=(float*)env->GetDirectBufferAddress(speed);
    printf("play speed is %f\n",*playspeed);
    ret=slice_mp4(slice_locate,start_time,(void*)&arg,write_websocket_bytebuffer_thread,control_cmd,playspeed);
    env->ReleaseStringUTFChars(slice_name,slice_locate);
    return ret;
}

/*
 * Class:     com_hirain_sdk_rtsp_JNIDeviceStream
 * Method:    rtspStreamDirect
 * Signature: (Ljava/lang/String;Ljava/lang/Object;Ljava/lang/String;Ljava/lang/String;)I
 */
JNIEXPORT jint JNICALL Java_com_hirain_sdk_rtsp_JNIDeviceStream_rtspStreamDirect
(JNIEnv * env , jobject , jstring rtspURl, jobject contrlCmd, jstring channelID, jstring token)
{
    printf("[%s %d]: rtspStreamDirect start\n", __FILE__,__LINE__);
    int ret;
    if(env==NULL||rtspURl==NULL||contrlCmd==NULL||channelID==NULL||token==NULL)
    {
        printf("error arg in Java_wsdd_nsmap_JNIDeviceStream_rtspStreamDirect\n");
        return ERROR_ARG;
    }
    printf("------ 1 ------\n");
    env->GetJavaVM(&gjava_vm);
    const char *rtsp_url = env->GetStringUTFChars(rtspURl, 0);
    char * control_cmd = (char*)env->GetDirectBufferAddress(contrlCmd);

    write_websocket_arg arg;
    memset(&arg,0,sizeof(arg));
    arg.channelID=channelID;
    arg.token =token;
    ret=rtsp_stream(rtsp_url,(void*)&arg,write_websocket_bytebuffer_rtsp_thread,control_cmd);
    env->ReleaseStringUTFChars(rtspURl,rtsp_url);
    printf("[%s %d]: rtspStreamDirect end %d\n", __FILE__, __LINE__, ret);

    return ret;
}

/*
 * Class:     com_hirain_sdk_rtsp_JNIDeviceStream
 * Method:    sliceStreamH265ToH264Play
 * Signature: (Ljava/lang/String;ILjava/lang/Object;Ljava/lang/String;Ljava/lang/String;)I
 */
JNIEXPORT jint JNICALL Java_com_hirain_sdk_rtsp_JNIDeviceStream_sliceStreamH265ToH264Play
(JNIEnv * env , jobject, jstring slice_name , jint start_time, jobject contrlCmd , jstring channelID,jstring token)
{
    int ret;
    if(env==NULL||slice_name==NULL||contrlCmd==NULL||channelID==NULL||token==NULL)
    {
        printf("error arg in Java_wsdd_nsmap_JNIDeviceStream_sliceStreamPlay\n");
        return ERROR_ARG;
    }

    env->GetJavaVM(&gjava_vm);
    const char *slice_locate = env->GetStringUTFChars(slice_name, 0);
    char * control_cmd = (char*)env->GetDirectBufferAddress(contrlCmd);
    write_websocket_arg arg;
    memset(&arg,0,sizeof(arg));
    arg.channelID=channelID;
    arg.token =token;
    ret=slice_h265toh264_mp4(slice_locate,start_time,(void*)&arg,write_websocket_bytebuffer_thread,control_cmd);
    env->ReleaseStringUTFChars(slice_name,slice_locate);
    return ret;
}

/*
 * Class:     com_hirain_sdk_rtsp_JNIDeviceStream
 * Method:    tcpStreamDirect
 * Signature: (Ljava/lang/String;ILjava/lang/Object;Ljava/lang/String;Ljava/lang/String;)I
 */
JNIEXPORT jint JNICALL Java_com_hirain_sdk_rtsp_JNIDeviceStream_tcpStreamDirect
  (JNIEnv * env , jobject, jstring tcp_ip , jint tcp_port, jobject contrlCmd, jstring channelID, jstring token)
{
    int ret;
    if(env==NULL||tcp_ip==NULL||contrlCmd==NULL||channelID==NULL||token==NULL||tcp_port<=0)
    {
        printf("error arg in Java_wsdd_nsmap_JNIDeviceStream_tcpStreamDirect\n");
        return ERROR_ARG;
    }
    env->GetJavaVM(&gjava_vm);
    const char *tcp_ip_str = env->GetStringUTFChars(tcp_ip, 0);
    char * control_cmd = (char*)env->GetDirectBufferAddress(contrlCmd);
    write_websocket_arg arg;
    memset(&arg,0,sizeof(arg));
    arg.channelID=channelID;
    arg.token =token;
    ret=tcp_h264_stream(tcp_ip_str,tcp_port,(void*)&arg,write_websocket_bytebuffer_rtsp_thread,control_cmd);
    env->ReleaseStringUTFChars(tcp_ip,tcp_ip_str);
    return ret;
}

/*
 * Class:     com_hirain_sdk_rtsp_JNIDeviceStream
 * Method:    tcpSliceAutoStop
 * Signature: (Ljava/lang/String;ILjava/lang/Object;Ljava/lang/String;ILjava/lang/Object;)I
 */
JNIEXPORT jint JNICALL Java_com_hirain_sdk_rtsp_JNIDeviceStream_tcpSliceAutoStop
  (JNIEnv *env , jobject, jstring tcp_ip, jint tcp_port , jobject contrlCmd, jstring SlicePath, jint stopSecond,jobject databaseobj)
{
    int ret,returnCode;
    const char *tcp_ip_str=NULL;
    const char *slice_filepath=NULL;
    char record_time_buff[10]={0};
    JNIEnv* localenv;

    if(env==NULL||tcp_ip==NULL||contrlCmd==NULL||SlicePath==NULL||stopSecond<=0||tcp_port<=0)
    {
        printf("error arg in Java_wsdd_nsmap_JNIDeviceStream_tcpSliceAutoStop\n");
        return ERROR_ARG;
    }
    env->GetJavaVM(&gjava_vm);
    tcp_ip_str = env->GetStringUTFChars(tcp_ip, 0);
    slice_filepath=env->GetStringUTFChars(SlicePath, 0);
    char * control_cmd = (char*)env->GetDirectBufferAddress(contrlCmd);
    // printf("get address char is %c %c\n",*control_cmd,*(control_cmd+1));
    int video_type=3;
    int record_time=0;

    returnCode=tcp_slice_autostop(tcp_ip_str,tcp_port,slice_filepath,control_cmd,stopSecond,&video_type,&record_time);

    //printf("end is %d %d %d\n",video_type,record_time,returnCode);
    //add to database
    ret=gjava_vm->AttachCurrentThread((void**)&localenv,NULL);
    if(ret<0)
    {
        printf("get env error in Java_wsdd_nsmap_JNIDeviceStream_tcpSliceAutoStop\n");
        return -1;
    }
    jclass database_class=localenv->GetObjectClass(databaseobj);
    if(database_class==NULL)
    {
        printf("get object class fail in Java_wsdd_nsmap_JNIDeviceStream_tcpSliceAutoStop\n");
        return -1;
    }
    jmethodID set_duration = localenv->GetMethodID(database_class, "setDuration", "(Ljava/lang/String;)V");
    jmethodID set_encode = localenv->GetMethodID(database_class, "setVideoEncode", "(I)V");
    if(set_duration==NULL||set_encode==NULL)
    {
        printf("get method id fail in Java_wsdd_nsmap_JNIDeviceStream_tcpSliceAutoStop\n");
        return -1;
    }
    int hour;
    if(record_time>3600)
    {
        hour=1;
        record_time-=3600;
    }
    else
        hour=0;
    int minute=record_time/60;
    int second=record_time%60;

    memset(record_time_buff,0,10);
    sprintf(record_time_buff,"%02d:%02d:%02d",hour,minute,second);
    //printf("get buff is %s\n",record_time_buff);

   // printf("get time string\n");
    jstring recode_time_str=localenv->NewStringUTF(record_time_buff);
   // printf("call set duration\n");
    localenv->CallVoidMethod(databaseobj,set_duration,recode_time_str);
    //printf("call set encode\n");
    localenv->CallVoidMethod(databaseobj,set_encode,video_type);

    // printf("end \n");
    localenv->ReleaseStringUTFChars(tcp_ip,tcp_ip_str);
    localenv->ReleaseStringUTFChars(SlicePath,slice_filepath);
    return returnCode;
}

int custom_image_output(void* opaque, unsigned char* buf, int buf_size);
/*
 * Class:     com_hirain_sdk_rtsp_JNIDeviceStream
 * Method:    snapshoot
 * Signature: (Ljava/lang/String;Ljava/lang/String;)I
 */
JNIEXPORT jint JNICALL Java_com_hirain_sdk_rtsp_JNIDeviceStream_snapshoot(JNIEnv* env, jobject obj, jstring jrtsp, jstring jremote_obj_name)
{
    LOG("INFO", "------ snapshoot start ------\n");

    int nRet = -1;
    do {
        if ((NULL == env) || (NULL == obj) || (NULL == jrtsp) || (NULL == jremote_obj_name)) {
            LOG("ERROR", "Input parameter error.[env:%p,obj:%p,rtsp:%p,minio_obj_name:%p]\n", env, obj,jrtsp, jremote_obj_name);
            nRet = ERROR_ARG;
            break;
        }
        char rtsp[256] = {0};
        char remote_obj_name[256] = { 0 };
        const char* p_rtsp = env->GetStringUTFChars(jrtsp, false);
        if (NULL != p_rtsp) {
            strcpy(rtsp, p_rtsp);
            printf("[rtsp]: %s\n", p_rtsp);
            env->ReleaseStringUTFChars(jrtsp, p_rtsp);
        }
        else {
            LOG("ERROR", "GetStringUTFChars failed '%s'\n");
            break;
        }
        const char * remote_name = env->GetStringUTFChars(jremote_obj_name, false);
        if (NULL != remote_name) {
            strcpy(remote_obj_name, remote_name);
            printf("[remote_name]: %s\n", remote_name);
            env->ReleaseStringUTFChars(jremote_obj_name, remote_name);
        }
        else {
            printf("GetStringUTFChars failed '%s'\n");
            break;
        }
        nRet = video_to_image(rtsp, NULL, custom_image_output, remote_obj_name);
        if (nRet < 0) {
            LOG("ERROR", "snapshoot failed %d '%s'\n", nRet, rtsp);
            break;
        }
        //minio.release();
    } while (false);
    LOG("INFO", "------ snapshoot end ------\n");

    return nRet;
}
int custom_image_output(void* opaque, unsigned char* buf, int buf_size)
{
    const char * remote_obj_name = (char*)opaque;
    if ((buf_size > 0) && (NULL != remote_obj_name) && (NULL != g_minio)) {
        g_minio->upload((char*)buf, buf_size, remote_obj_name);
    }
    else LOG("ERROR","buf=%p,buf_size=%d,remote_name=%p,g_minio=%p\n", buf,buf_size, remote_obj_name, g_minio);
    return buf_size;
}

/*
 * Class:     com_hirain_sdk_rtsp_JNIDeviceStream
 * Method:    playStream
 * Signature: (Ljava/lang/String;Ljava/lang/Object;Ljava/lang/String;Ljava/lang/String;)I
 * @param rtspUrl   -- 点播RTSP地址或者回放文件全路径名
 * @param start_time -- 回放开始时刻(默认为0)
 * @param contrlCmd -- 控制点播开始'r'和停止's',控制回放开始'r'、暂停'p'、继续播放'r'、停止's'
 * @param jratio     -- 控制回放倍率(取值范围: 1/8,1/4,1/2,1,2,4,8)
 * @param channelID -- 当前点播通道唯一标识
 * @param token     -- 当前点播客户端唯一标识
 */
JNIEXPORT jint JNICALL Java_com_hirain_sdk_rtsp_JNIDeviceStream_playStream(JNIEnv* env, jobject, jstring rtspURl, jint start_time, jobject contrlCmd, jobject jplayspeed, jstring channelID, jstring token)
{
    printf("------ playStream start ------\n");
    int ret = -1;
    if (env == NULL || rtspURl == NULL || contrlCmd == NULL || channelID == NULL || token == NULL) {
        LOG("ERROR","Input parameter error");
        return ERROR_ARG;
    }
    char chl[64] = { 0 };
    char rtsp[256] = { 0 };
    char ctl_cmd = 0;
    do {
        env->GetJavaVM(&gjava_vm);
        const char* rtsp_url = env->GetStringUTFChars(rtspURl, 0);
        if (NULL == rtsp_url) {
            LOG("ERROR", "rtsp_url=%p\n", rtsp_url);
            break;
        }
        strcpy(rtsp, rtsp_url);
        char* control_cmd = (char*)env->GetDirectBufferAddress(contrlCmd);
        if (NULL == control_cmd) {
            LOG("ERROR", "control_cmd=%p\n", control_cmd);
            break;
        }
        // 播放倍率 (回放时有效)
        float* p_playspeed = (float*)env->GetDirectBufferAddress(jplayspeed);

        write_websocket_arg arg;
        memset(&arg, 0, sizeof(arg));
        arg.channelID = channelID;
        arg.token = token;

        const char* str = env->GetStringUTFChars(channelID, false);
        if (NULL != str) {
            strcpy(chl, str);
            env->ReleaseStringUTFChars(channelID, str);
        }
        //2022033109262500019_1回放
        //2022033109262500019_2主码流点播
        //2022033109262500019_3子码流点播
        const char* type = strchr(chl, '_');
        if (NULL == type) {
            LOG("ERROR", "type:null, chl_id:%s\n", chl);
            break;
        }
        type++;

        stream_param_t param;
        param.chl = chl;            // 测试
        float playspeed = 1.0;
        param.ratio = &playspeed;
        if ('1' == *type) { // 回放
            param.in_custom = read_packet;
            param.start_time = start_time;
            if((NULL != p_playspeed) && ((0.125 <= *p_playspeed) && (*p_playspeed <= 8)))
                param.ratio = p_playspeed;
            else {
                LOG("ERROR", "playspeed=%f\n", (NULL != p_playspeed) ? *p_playspeed : -1.0);
                break;
            }
            printf("------ [Playback] control_cmd:'%c',start_time:%lld,ratio:%0.3f,chl:'%s',url:'%s'\n", *(control_cmd + 1), start_time ,p_playspeed?*p_playspeed:-1, chl, rtsp_url);
        }
        else { // 点播
            printf("------ [Play] control_cmd:'%c',start_time:%lld,ratio:%0.3f,chl:'%s',url:'%s'\n", *(control_cmd + 1),start_time, p_playspeed ? *p_playspeed : -1, chl, rtsp_url);
        }
        param.in_url = rtsp_url;
        param.out_custom = write_websocket_bytebuffer_rtsp_thread;
        param.out_opaque = &arg;
        if ('1' == *type)  // 回放
            ret = playback_stream(control_cmd, &param);
        else
            ret = video_stream(control_cmd, &param);

        env->ReleaseStringUTFChars(rtspURl, rtsp_url);
        ctl_cmd = *(control_cmd + 1);
    } while (false);
    printf("------ playStream end. ------ [ret:%d,control_cmd:'%c',chl:'%s',url:'%s'] ------\n", ret, ctl_cmd,chl, rtsp);
    return ret;
}