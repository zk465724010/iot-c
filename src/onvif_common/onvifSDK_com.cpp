#include "onvifSDK_com.h"
#include <cstring>
#include "wsdd.nsmap"

#include <iostream>
using namespace std;

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "log.h"

//#include <unistd.h>
//#include <sys/socket.h>
//#include <netinet/in.h>
//#include <arpa/inet.h>
//#include <netdb.h>

//使用 ifconf结构体和ioctl函数时需要用到该头文件
//#include <net/if.h>
//#include <sys/ioctl.h>

//使用ifaddrs结构体时需要用到该头文件
//#include <ifaddrs.h>

#define PJ_MAX_HOSTNAME  (128)

#define RUN_SUCCESS 0
#define RUN_FAIL -1

int get_netway_ip_using_res(char *str_ip)
{
    int status = RUN_FAIL;
    char do_comment[] = "ifconfig | grep 'inet addr' | awk '{print $2}' | sed 's/.*://g'";
    //该命令是从ifconfig中提取相应的IP
#if defined(__linux__) || defined(__linux)
    FILE *fp = NULL;
    fp = popen(do_comment, "r");
    if(fp != NULL)
    {
        status = RUN_SUCCESS;
        while( !feof(fp) )
        {
            fgets(str_ip, 1024, fp);
            status = RUN_SUCCESS;
            if(strcmp("127.0.0.1", str_ip))
            {
                break;
            }
        }
    }
    fclose(fp);
#endif
    return status;

}

int get_local_ip_using_create_socket(char *str_ip)
{
    int status = RUN_FAIL;
    int af = AF_INET;
    int sock_fd = socket(AF_INET, SOCK_DGRAM, 0);
    struct sockaddr_in remote_addr;
    struct sockaddr_in local_addr;
    char *local_ip = NULL;
    socklen_t len = 0;

    remote_addr.sin_family = AF_INET;
    remote_addr.sin_port = htons(53);
    remote_addr.sin_addr.s_addr = inet_addr("1.1.1.1");

    len =  sizeof(struct sockaddr_in);
    status = connect(sock_fd, (struct sockaddr*)&remote_addr, len);
    if(status != 0 ){
        printf("connect err \n");
    }

    len =  sizeof(struct sockaddr_in);
    getsockname(sock_fd, (struct sockaddr*)&local_addr, &len);

    local_ip = inet_ntoa(local_addr.sin_addr);
    if(local_ip)
    {
        strcpy(str_ip, local_ip);
        status = RUN_SUCCESS;
    }
    return status;
}

int UDPsendMessage(sockaddr_in multi_addr, SOAP_SOCKET s, const char *probe)
{
    int ret;
    int optval = 1;
    ret = setsockopt(s, SOL_SOCKET, SO_BROADCAST, (const char*)&optval, sizeof(int));
    if (ret < 0) {
        printf("set broadcast error\n");
    }

    ret = sendto(s, probe, strlen(probe), 0, (sockaddr*)&multi_addr, sizeof(multi_addr));
    if (ret < 0) {
        printf("sendto error\n");
    }
    return ret;
}

void removeChar(const char *buffer,char *GetBuff)
{
    string str;
    str = buffer;

    string strWsaF("<wsadis:Address>");
    int iWsaF = str.find(strWsaF,0);
    if(iWsaF > -1)//-1为未找到
    {
        str.erase(iWsaF,strWsaF.length());
    }
    string strWsaB("</wsadis:Address>");
    int iWsaB = str.find(strWsaB,0);
    if(iWsaB > -1)//-1为未找到
    {
        str.erase(iWsaB,strWsaB.length());
    }

    string strF("<a:Address>");
    int intF = str.find(strF,0);
    if(intF > -1)//-1为未找到
    {
        str.erase(intF,strF.length());
    }

    string strB("</a:Address>");
    int intB = str.find(strB,0);
    if(intB > -1)//-1为未找到
    {
        str.erase(intB,strB.length());
    }

    string strAddrF("<d:XAddrs>");
    int intAddrF = str.find(strAddrF,0);
    if(intAddrF > -1)//-1为未找到
    {
        str.erase(intAddrF,strAddrF.length());
    }

    string strAddrB("</d:XAddrs>");
    int intAddrB = str.find(strAddrB,0);
    if(intAddrB > -1)//-1为未找到
    {
        str.erase(intAddrB,strAddrB.length());
    }else
    {

    }

    strcpy(GetBuff,(char*)str.data());
}

void splitChar(char* buff,int index,char *backBuff)
{
    if(index == 1)
    {
        //        printf("1 \n");
        char* pAddrF = NULL;
        pAddrF =strstr(buff, "<d:ProbeMatch>");
        char* pAddrB = NULL;
        pAddrB =strstr(buff, "</d:ProbeMatch>");
        int partLen = 0;
        partLen = pAddrB - pAddrF + 15; // len of "EB EB EB" is 8

        //        if(pAddrF == NULL && pAddrB == NULL)
        //        {
        //            pAddrF =strstr(buff, "<a:Address>");
        //            pAddrB =strstr(buff, "</a:Address>");
        //            partLen = pAddrB - pAddrF + 12;
        //        }
        strncpy(backBuff, pAddrF, partLen);
        backBuff[partLen] = 0;
        puts(backBuff);
    }else if(index == 2)//<d:XAddrs>
    {
        //        printf("2 \n");
        char* pAddrF = NULL;
        pAddrF =strstr(buff, "<d:XAddrs>");
        char* pAddrB = NULL;
        pAddrB =strstr(buff, "</d:XAddrs>");
        int partLen = 0;
        partLen = pAddrB - pAddrF + 11; // len of "EB EB EB" is 8

        strncpy(backBuff, pAddrF, partLen);
        backBuff[partLen] = 0;
        puts(backBuff);
    }else
    {
        backBuff = NULL;
        printf("backBuff is emtipy! \n");
    }
}

char* jstring2CStr(JNIEnv *env, jstring jstr)
{
    if (env->ExceptionCheck() == JNI_TRUE || jstr == NULL)
    {
        if(env->ExceptionOccurred())
        {
            env->ExceptionDescribe();
            printf("jstringToNative函数转换时,传入的参数str为空\n");
            return NULL;
        }
    }

    jbyteArray bytes = 0;
    jthrowable exc;
    char *result = 0;
    if (env->EnsureLocalCapacity(2) < 0) {
        return 0; /* out of memory error */
    }
    jclass jcls_str = env->FindClass("java/lang/String");
    jmethodID MID_String_getBytes = env->GetMethodID(jcls_str, "getBytes", "()[B");

    bytes = (jbyteArray)env->CallObjectMethod(jstr, MID_String_getBytes);
    exc = env->ExceptionOccurred();
    if (!exc) {
        jint len = env->GetArrayLength( bytes);
        result = (char *)malloc(len + 1);
        if (result == 0) {
            //JNU_ThrowByName(env, "java/lang/OutOfMemoryError", 	0);
            env->DeleteLocalRef(bytes);
            return 0;
        }
        env->GetByteArrayRegion(bytes, 0, len, (jbyte *)result);
        result[len] = 0; /* NULL-terminate */
    } else {
        env->DeleteLocalRef(exc);
    }
    env->DeleteLocalRef(bytes);
    return (char*)result;
}

jstring CStr2jstring( JNIEnv* env,const char* str )
{
    jclass strClass = env->FindClass( "Ljava/lang/String;");
    jmethodID ctorID = env->GetMethodID( strClass, "<init>",
                                         "([BLjava/lang/String;)V");

    if (env->ExceptionCheck() == JNI_TRUE || str == NULL)
    {
        env->ExceptionDescribe();
        env->ExceptionClear();
        printf("nativeTojstring函数转换时,str为空\n");
        return NULL;
    }

    jbyteArray bytes = env->NewByteArray( strlen(str));
    //如果str为空则抛出异常给jvm

    env->SetByteArrayRegion( bytes, 0,  strlen(str), (jbyte*)str);
    //jstring encoding = env->NewStringUTF( "GBK");
    jstring encoding = env->NewStringUTF( "UTF8");
    jstring strRtn = (jstring)env->NewObject( strClass, ctorID, bytes,
                                              encoding);
    //释放str内存
    // free(str);
    return strRtn;
}
jstring char_to_jstring(JNIEnv* env, const char* str)
{
    if ((NULL != env) && (NULL != str)) {
        jclass cls = env->FindClass("Ljava/lang/String;");
        if (NULL != cls) {
            jmethodID construct = env->GetMethodID(cls, "<init>", "([BLjava/lang/String;)V");
            if (NULL != construct) {
                int nLen = strlen(str);
                jbyteArray bytes = env->NewByteArray(nLen);
                if (NULL != bytes) {
                    env->SetByteArrayRegion(bytes, 0, nLen, (jbyte*)str);
                    jstring encoding = env->NewStringUTF("utf-8");
                    if (NULL != encoding) {
                        jstring jstr = (jstring)env->NewObject(cls, construct, bytes, encoding);
                        if (NULL == jstr)
                            LOG("ERROR", "NewObject error\n\n");
                        return jstr;
                    }
                    else LOG("ERROR", "NewStringUTF error\n\n");
                }
                else LOG("ERROR", "NewByteArray error\n\n");
            }
            else LOG("ERROR", "GetMethodID error\n\n");
        }
        else LOG("ERROR", "FindClass error\n\n");
    }
    else LOG("ERROR", "env=%p, str=%p\n\n", env, str);
    return NULL;
}

int jstringCompare(JNIEnv *env, jstring String1, const char* String2)
{
    const char *nativeString1 = jstring2CStr(env,String1);

    /* Now you can compare nativeString1 with nativeString2*/
    // 比较字符串str1和str2的大小,如果str1小于str2,返回值就0,反之如果str1大于str2,返回值就0,
    //如果str1等于str2,返回值就=0,len指的是str1与str2的比较的字符数

    return strncmp(nativeString1, String2, strlen(nativeString1));
}

jstring jstringSplicing(JNIEnv *env,char* bstr,char* estr)
{
    strcat(bstr," + ");
    strcat(bstr,estr);
    return env->NewStringUTF(bstr);
}

const char *dump_enum2str_VideoEncoding(enum tt__VideoEncoding e)
{
    switch(e) {
    case tt__VideoEncoding__JPEG:  return "JPEG";
    case tt__VideoEncoding__MPEG4: return "MPEG4";
    case tt__VideoEncoding__H264:  return "H264";
    }
    return "unknown";
}

const char *dump_enum2str_AudioEncoding(enum tt__AudioEncoding e)
{
    switch (e) {
    case tt__AudioEncoding__G711: return "G711";
    case tt__AudioEncoding__G726: return "G726";
    case tt__AudioEncoding__AAC:  return "AAC";
    }
    return "unknown";
}

void soap_perror(struct soap *soap, const char *str)
{
    if (NULL == str) {
        SOAP_DBGERR("[soap] error: %d, %s, %s\n", soap->error, *soap_faultcode(soap), *soap_faultstring(soap));
    } else {
        SOAP_DBGERR("[soap] %s error: %d, %s, %s\n", str, soap->error, *soap_faultcode(soap), *soap_faultstring(soap));
    }
    return;
}

void* ONVIF_soap_malloc(struct soap *soap, unsigned int n)
{
    void *p = NULL;

    if (n > 0) {
        p = soap_malloc(soap, n);
        SOAP_ASSERT(NULL != p);
        memset(p, 0x00 ,n);
    }
    return p;
}

struct soap *ONVIF_soap_new(int timeout)
{
    struct soap *soap = NULL;                                                   // soap环境变量

    SOAP_ASSERT(NULL != (soap = soap_new()));

    soap_set_namespaces(soap, namespaces);                                      // 设置soap的namespaces
    soap->recv_timeout    = timeout;                                            // 设置超时（超过指定时间没有数据就退出）
    soap->send_timeout    = timeout;
    soap->connect_timeout = timeout;

#if defined(__linux__) || defined(__linux)                                      // 参考https://www.genivia.com/dev.html#client-c的修改：
    soap->socket_flags = MSG_NOSIGNAL;                                          // To prevent connection reset errors
#endif

    soap_set_mode(soap, SOAP_C_UTFSTRING);                                      // 设置为UTF-8编码，否则叠加中文OSD会乱码

    return soap;
}

void ONVIF_soap_delete(struct soap *soap)
{
    soap_destroy(soap);                                                         // remove deserialized class instances (C++ only)
    soap_end(soap);                                                             // Clean up deserialized data (except class instances) and temporary data
    soap_done(soap);                                                            // Reset, close communications, and remove callbacks
    soap_free(soap);                                                            // Reset and deallocate the context created with soap_new or soap_copy
}

/************************************************************************
**函数：ONVIF_SetAuthInfo
**功能：设置认证信息
**参数：
        [in] soap     - soap环境变量
        [in] username - 用户名
        [in] password - 密码
**返回：
        0表明成功，非0表明失败
**备注：
************************************************************************/
int ONVIF_SetAuthInfo(struct soap *soap, const char *username, const char *password)
{
    int result = 0;

    SOAP_ASSERT(NULL != username);
    SOAP_ASSERT(NULL != password);
    try
    {
        result = soap_wsse_add_UsernameTokenDigest(soap, NULL, username, password);
    }
    catch (const std::exception&)
    {
        printf("username or password error\n");
    }

    if(0 != result)
    {
        printf("setAuthInfo failed\n");
        return -1;
    }
    return result;
}

/************************************************************************
**函数：ONVIF_init_header
**功能：初始化soap描述消息头
**参数：
        [in] soap - soap环境变量
**返回：无
**备注：
    1). 在本函数内部通过ONVIF_soap_malloc分配的内存，将在ONVIF_soap_delete中被释放
************************************************************************/
void ONVIF_init_header(struct soap *soap)
{
    struct SOAP_ENV__Header *header = NULL;

    SOAP_ASSERT(NULL != soap);

    header = (struct SOAP_ENV__Header *)ONVIF_soap_malloc(soap, sizeof(struct SOAP_ENV__Header));
    soap_default_SOAP_ENV__Header(soap, header);
    header->wsa__MessageID = (char*)soap_wsa_rand_uuid(soap);
    header->wsa__To        = (char*)ONVIF_soap_malloc(soap, strlen(SOAP_TO) + 1);
    header->wsa__Action    = (char*)ONVIF_soap_malloc(soap, strlen(SOAP_ACTION) + 1);
    strcpy(header->wsa__To, SOAP_TO);
    strcpy(header->wsa__Action, SOAP_ACTION);
    soap->header = header;

    return;
}

/************************************************************************
**函数：ONVIF_init_ProbeType
**功能：初始化探测设备的范围和类型
**参数：
        [in]  soap  - soap环境变量
        [out] probe - 填充要探测的设备范围和类型
**返回：
        0表明探测到，非0表明未探测到
**备注：
    1). 在本函数内部通过ONVIF_soap_malloc分配的内存，将在ONVIF_soap_delete中被释放
************************************************************************/
void ONVIF_init_ProbeType(struct soap *soap, struct wsdd__ProbeType *probe)
{
    struct wsdd__ScopesType *scope = NULL;                                      // 用于描述查找哪类的Web服务

    SOAP_ASSERT(NULL != soap);
    SOAP_ASSERT(NULL != probe);

    scope = (struct wsdd__ScopesType *)ONVIF_soap_malloc(soap, sizeof(struct wsdd__ScopesType));
    soap_default_wsdd__ScopesType(soap, scope);                                 // 设置寻找设备的范围
    scope->__item = (char*)ONVIF_soap_malloc(soap, strlen(SOAP_ITEM) + 1);
    strcpy(scope->__item, SOAP_ITEM);

    memset(probe, 0x00, sizeof(struct wsdd__ProbeType));
    soap_default_wsdd__ProbeType(soap, probe);
    probe->Scopes = scope;
    probe->Types  = (char*)ONVIF_soap_malloc(soap, strlen(SOAP_TYPES) + 1);     // 设置寻找设备的类型
    strcpy(probe->Types, SOAP_TYPES);

    return;
}

/************************************************************************
**函数：ONVIF_GetCapabilities
**功能：获取设备能力信息
**参数：
        [in] DeviceXAddr - 设备服务地址
        [out] capa       - 返回设备能力信息信息
**返回：
        0表明成功，非0表明失败
**备注：
    1). 其中最主要的参数之一是媒体服务地址
************************************************************************/
int ONVIF_GetCapabilities(const char *DeviceXAddr, struct tagCapabilities *capa,const char *username, const char *password)
{
    int result = -1;
    struct soap *soap = NULL;
    _tds__GetCapabilities            req;
    _tds__GetCapabilitiesResponse    rep;
    soap = ONVIF_soap_new(SOAP_SOCK_TIMEOUT);
    do {
        if (NULL == DeviceXAddr) {
            printf("[ERROR]: DeviceXAddr=%p\n\n", DeviceXAddr);
            break;
        }
        if (NULL == capa) {
            printf("[ERROR]: capa=%p\n\n", capa);
            break;
        }
        if (NULL == username) {
            printf("[ERROR]: username=%p\n\n", username);
            break;
        }
        if (NULL == password) {
            printf("[ERROR]: password=%p\n\n", password);
            break;
        }
        if (NULL == soap) {
            printf("[ERROR]: soap=%p\n\n", soap);
            break;
        }
        result = 0;
    } while (false);
    if (result < 0)
        return result;


    ONVIF_SetAuthInfo(soap, username, password);
    result = soap_call___tds__GetCapabilities(soap, DeviceXAddr, NULL, &req, rep);
    if(0 != result){
        printf("[ERROR]: GetCapabilities function failed [ret=%d]\n\n", result);
        return -1;
    }

    memset(capa, 0x00, sizeof(struct tagCapabilities));
    if (NULL != rep.Capabilities) {
        if (NULL != rep.Capabilities->Media) {
            if (NULL != rep.Capabilities->Media->XAddr) {
                strncpy(capa->MediaXAddr, rep.Capabilities->Media->XAddr, sizeof(capa->MediaXAddr) - 1);
            }
        }
        if (NULL != rep.Capabilities->PTZ){
            if(NULL != rep.Capabilities->PTZ->XAddr){
                strncpy(capa->PTZXAddr, rep.Capabilities->PTZ->XAddr, sizeof(capa->PTZXAddr) - 1);
            }
        }
        if (NULL != rep.Capabilities->Events) {
            if (NULL != rep.Capabilities->Events->XAddr) {
                strncpy(capa->EventXAddr, rep.Capabilities->Events->XAddr, sizeof(capa->EventXAddr) - 1);
            }
        }
        if (NULL != rep.Capabilities->Imaging) {
            if (NULL != rep.Capabilities->Imaging->XAddr) {
                strncpy(capa->IMGXAddr, rep.Capabilities->Imaging->XAddr, sizeof(capa->IMGXAddr) - 1);
            }
        }
    }
    if (NULL != soap) {
        ONVIF_soap_delete(soap);
    }
    return result;
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
int ONVIF_GetProfiles(const char *MediaXAddr, struct tagProfile **profiles,const char *username, const char *password)
{
    int i = 0;
    int result = 0;
    struct soap *soap = NULL;
    _trt__GetProfiles            req;
    _trt__GetProfilesResponse    rep;

    SOAP_ASSERT(NULL != MediaXAddr);
    SOAP_ASSERT(NULL != username);
    SOAP_ASSERT(NULL != password);
    SOAP_ASSERT(NULL != (soap = ONVIF_soap_new(SOAP_SOCK_TIMEOUT)));

    ONVIF_SetAuthInfo(soap, username, password);

    result = soap_call___trt__GetProfiles(soap, MediaXAddr, NULL, &req, rep);
    printf("GetProfiles ->result = %d\n",result);

    if(0 != result)
    {
        printf("call GetProfiles failed\n");
        return -1;
    }

    if (rep.__sizeProfiles > 0) {                                               // 分配缓存
        (*profiles) = (struct tagProfile *)malloc(rep.__sizeProfiles * sizeof(struct tagProfile));
        SOAP_ASSERT(NULL != (*profiles));
        memset((*profiles), 0x00, rep.__sizeProfiles * sizeof(struct tagProfile));
    }

    for(i = 0; i < rep.__sizeProfiles; i++) {                                   // 提取所有配置文件信息（我们所关心的）
        tt__Profile *ttProfile = rep.Profiles[i];
        struct tagProfile *plst = &(*profiles)[i];

        if (NULL != ttProfile->Name) {                                         // 配置文件名称，区分主码流、子码流
            strncpy(plst->Name, ttProfile->Name, sizeof(plst->Name) - 1);
        }

        if (NULL != ttProfile->token) {                                         // 配置文件Token
            strncpy(plst->token, ttProfile->token, sizeof(plst->token) - 1);
        }

        if (NULL != ttProfile->VideoEncoderConfiguration) {                     // 视频编码器配置信息
            if (NULL != ttProfile->VideoEncoderConfiguration->token) {          // 视频编码器Token
                strncpy(plst->venc.token, ttProfile->VideoEncoderConfiguration->token, sizeof(plst->venc.token) - 1);
            }
            if (NULL != ttProfile->VideoEncoderConfiguration->Resolution) {     // 视频编码器分辨率
                plst->venc.Width  = ttProfile->VideoEncoderConfiguration->Resolution->Width;
                plst->venc.Height = ttProfile->VideoEncoderConfiguration->Resolution->Height;
            }
        }
        //PTZ
        if(NULL != ttProfile->PTZConfiguration){
            if(NULL != ttProfile->PTZConfiguration->token){
                strncpy(plst->PTZtoken, ttProfile->PTZConfiguration->token,sizeof (plst->PTZtoken)-1);
            }
        }
        //声音
        if(NULL != ttProfile->AudioEncoderConfiguration){
            if(NULL != ttProfile->AudioEncoderConfiguration->token){
                strncpy(plst->Audiotoken, ttProfile->AudioEncoderConfiguration->token,sizeof (plst->Audiotoken)-1);
            }
        }
        //videoSource
        if(NULL!=ttProfile->VideoSourceConfiguration){
            if(NULL!=ttProfile->VideoSourceConfiguration->SourceToken)
                strncpy(plst->videoSourceToken,ttProfile->VideoSourceConfiguration->SourceToken,sizeof(plst->videoSourceToken)-1);
        }
    }

    if (NULL != soap) {
        ONVIF_soap_delete(soap);
    }

    return rep.__sizeProfiles;
}

/************************************************************************
**函数：make_uri_withauth
**功能：构造带有认证信息的URI地址
**参数：
        [in]  src_uri       - 未带认证信息的URI地址
        [in]  username      - 用户名
        [in]  password      - 密码
        [out] dest_uri      - 返回的带认证信息的URI地址
        [in]  size_dest_uri - dest_uri缓存大小
**返回：
        0成功，非0失败
**备注：
    1). 例子：
    无认证信息的uri：rtsp://100.100.100.140:554/av0_0
    带认证信息的uri：rtsp://username:password@100.100.100.140:554/av0_0
************************************************************************/
int make_uri_withauth(char *src_uri, char *username, char *password, char *dest_uri, unsigned int size_dest_uri)
{
    int result = 0;
    unsigned int needBufSize = 0;

    SOAP_ASSERT(NULL != src_uri);
    SOAP_ASSERT(NULL != username);
    SOAP_ASSERT(NULL != password);
    SOAP_ASSERT(NULL != dest_uri);
    memset(dest_uri, 0x00, size_dest_uri);

    needBufSize = strlen(src_uri) + strlen(username) + strlen(password) + 3;    // 检查缓存是否足够，包括‘:’和‘@’和字符串结束符
    if (size_dest_uri < needBufSize) {
        SOAP_DBGERR("dest uri buf size is not enough.\n");
        result = -1;
        goto EXIT;
    }

    if (0 == strlen(username) && 0 == strlen(password)) {                       // 生成新的uri地址
        strcpy(dest_uri, src_uri);
    } else {
        char *p = strstr(src_uri, "//");
        if (NULL == p) {
            SOAP_DBGERR("can't found '//', src uri is: %s.\n", src_uri);
            result = -1;
            goto EXIT;
        }
        p += 2;

        memcpy(dest_uri, src_uri, p - src_uri);
        sprintf(dest_uri + strlen(dest_uri), "%s:%s@", username, password);
        strcat(dest_uri, p);
    }

EXIT:

    return result;
}

int ONVIF_RelativeMove(const char* mediaAddr,const char*profileToken,const char* usrName,const char* passWord,int type)
{
    int result;
    struct soap *soap = NULL;
    SOAP_ASSERT(NULL != (soap = ONVIF_soap_new(SOAP_SOCK_TIMEOUT)));

    _tptz__RelativeMove RelativeMove;
    _tptz__RelativeMoveResponse RelativeMoveRep;


    tt__Vector2D v2D;
    v2D.space = "http://www.onvif.org/ver10/tptz/PanTiltSpaces/TranslationGenericSpace";
    tt__Vector1D v1D;
    v1D.space = "http://www.onvif.org/ver10/tptz/ZoomSpaces/TranslationGenericSpace";

    int speed = 5;
    printf("type ====%d\n",type);
    switch (type)
    {
    case UP://up
    {
        printf("up\n");
        v2D.x = 0;
        v2D.y = -((float)speed / 10);
        break;
    }
    case DOWN://down
    {
        printf("down\n");
        v2D.x = 0;
        v2D.y = ((float)speed / 10);
        break;
    }
    case LEFT://left
    {
        printf("left\n");
        v2D.x = ((float)speed / 10);
        v2D.y = 0;
        break;
    }
    case RIGHT://right
    {
        printf("right\n");
        v2D.x = -((float)speed / 10);
        v2D.y = 0;
        break;
    }
    case RESTART://restart
    {
        printf("restart\n");
        break;
    }
    case ZOOMIN://zoonin
    {
        printf("zoonin\n");
        v1D.x= ((float)speed / 10);
        break;
    }
    case ZOOMOUT://zoonout
    {
        printf("zoonout\n");
        v1D.x= -((float)speed / 10);
        break;
    }
    default:
        break;
    }

    tt__PTZVector  Translation;
    Translation.PanTilt = &v2D;
    Translation.Zoom = &v1D;
    RelativeMove.ProfileToken = (char*)profileToken;

    //speed
    tt__PTZSpeed* sp = soap_new_tt__PTZSpeed(soap,-1);
    RelativeMove.Speed = sp;

    tt__Vector2D* sPantilt = soap_new_tt__Vector2D(soap, -1);
    RelativeMove.Speed->PanTilt = sPantilt;

    RelativeMove.Speed->PanTilt->x = 1.0;
    RelativeMove.Speed->PanTilt->y = 1.0;

    tt__Vector1D* szoom = soap_new_tt__Vector1D(soap, -1);
    RelativeMove.Speed->Zoom = szoom;
    RelativeMove.Speed->Zoom->x = 1.0;

    ONVIF_SetAuthInfo(soap, usrName, passWord);
    result = soap_call___tptz__RelativeMove(soap,mediaAddr, NULL, &RelativeMove, RelativeMoveRep);
    printf("PTZ RelativeMove->result:%d\n", result);
    if (NULL != soap) {
        ONVIF_soap_delete(soap);
    }
    return result;
}

int ONVIF_ContinuousMove(const char* ptzXAddr,const char*profileToken,const char* usrName,const char* passWord,int  cmd,float speed)
{
    int result = 0;
    struct soap *soap = NULL;
    _tptz__ContinuousMove continuousMove;
    _tptz__ContinuousMoveResponse continuousMoveResponse;

    //SOAP_ASSERT(!ptzXAddr && !profileToken);
    SOAP_ASSERT(NULL != (soap = ONVIF_soap_new(SOAP_SOCK_TIMEOUT)));

    ONVIF_SetAuthInfo(soap, usrName, passWord);
    continuousMove.ProfileToken = const_cast<char *>(profileToken);
    printf("continuousMove.ProfileToken == %s\n",continuousMove.ProfileToken);
    continuousMove.Velocity = soap_new_tt__PTZSpeed(soap);
    continuousMove.Velocity->PanTilt = soap_new_tt__Vector2D(soap);
    continuousMove.Velocity->Zoom = soap_new_tt__Vector1D(soap);

    switch (cmd)
    {
    case  PTZ_CMD_LEFT:
        continuousMove.Velocity->PanTilt->x = -speed;
        continuousMove.Velocity->PanTilt->y = 0;
        break;
    case  PTZ_CMD_RIGHT:
        continuousMove.Velocity->PanTilt->x = speed;
        continuousMove.Velocity->PanTilt->y = 0;
        break;
    case  PTZ_CMD_UP:
        continuousMove.Velocity->PanTilt->x = 0;
        continuousMove.Velocity->PanTilt->y = speed;
        break;
    case  PTZ_CMD_DOWN:
        continuousMove.Velocity->PanTilt->x = 0;
        continuousMove.Velocity->PanTilt->y = -speed;
        break;
    case  PTZ_CMD_LEFTUP:
        continuousMove.Velocity->PanTilt->x = -speed;
        continuousMove.Velocity->PanTilt->y = speed;
        break;
    case PTZ_CMD_LEFTDOWN:
        continuousMove.Velocity->PanTilt->x = -speed;
        continuousMove.Velocity->PanTilt->y = -speed;
        break;
    case  PTZ_CMD_RIGHTUP:
        continuousMove.Velocity->PanTilt->x = speed;
        continuousMove.Velocity->PanTilt->y = speed;
        break;
    case PTZ_CMD_RIGHTDOWN:
        continuousMove.Velocity->PanTilt->x = speed;
        continuousMove.Velocity->PanTilt->y = -speed;
        break;
    case  PTZ_CMD_ZOOM_IN:
        continuousMove.Velocity->PanTilt->x = 0;
        continuousMove.Velocity->PanTilt->y = 0;
        continuousMove.Velocity->Zoom->x = speed;
        break;
    case  PTZ_CMD_ZOOM_OUT:
        continuousMove.Velocity->PanTilt->x = 0;
        continuousMove.Velocity->PanTilt->y = 0;
        continuousMove.Velocity->Zoom->x = -speed;
        break;
    case  PTZ_CMD_STOP:
        ONVIF_PTZStopMove(ptzXAddr, profileToken);
        break;
    default:
        ONVIF_PTZStopMove(ptzXAddr, profileToken);
        break;
    }

    // 也可以使用soap_call___tptz__RelativeMove实现
    result = soap_call___tptz__ContinuousMove(soap,ptzXAddr, NULL,&continuousMove,continuousMoveResponse);
    SOAP_CHECK_ERROR(result, soap, "ONVIF_PTZContinuousMove2");
   // Sleep(1); //如果当前soap被删除（或者发送stop指令），就会停止移动
    //    std::this_thread::sleep_for(chrono::second(1));
   // ONVIF_PTZStopMove(ptzXAddr, profileToken);
EXIT:
    if (NULL != soap) {
        ONVIF_soap_delete(soap);
    }
    return result;
}

int ONVIF_PTZStopMove(const char* ptzXAddr, const char* ProfileToken){
    int result = 0;
    struct soap *soap = NULL;
    _tptz__Stop tptzStop;
    _tptz__StopResponse tptzStopResponse;

    //SOAP_ASSERT(!ptzXAddr && !ProfileToken);
    SOAP_ASSERT(NULL != (soap = ONVIF_soap_new(SOAP_SOCK_TIMEOUT)));

    ONVIF_SetAuthInfo(soap, USERNAME, PASSWORD);
    tptzStop.ProfileToken = const_cast<char *>(ProfileToken);
    result = soap_call___tptz__Stop(soap, ptzXAddr, NULL, &tptzStop, tptzStopResponse);
    SOAP_CHECK_ERROR(result, soap, "ONVIF_PTZStopMove");

EXIT:
    if (NULL != soap) {
        ONVIF_soap_delete(soap);
    }
    return result;
}

/************************************************************************
**函数：ONVIF_GetVideoEncoderConfigurationOptions
**功能：获取指定视频编码器配置的参数选项集
**参数：
        [in] MediaXAddr   - 媒体服务地址
        [in] ConfigurationToken - 视频编码器配置的令牌字符串，如果为NULL，则获取所有视频编码器配置的选项集（会杂在一起，无法区分选项集是归属哪个视频编码配置器的）
**返回：
        0表明成功，非0表明失败
**备注：
    1). 有两种方式可以获取指定视频编码器配置的参数选项集：一种是根据ConfigurationToken，另一种是根据ProfileToken
************************************************************************/
int ONVIF_GetVideoEncoderConfigurationOptions(const char *MediaXAddr, char *ConfigurationToken, const char* usrName, const char* passWord)
{
    int result = 0;
    struct soap *soap = NULL;
    _trt__GetVideoEncoderConfigurationOptions          req;
    _trt__GetVideoEncoderConfigurationOptionsResponse  rep;

    SOAP_ASSERT(NULL != MediaXAddr);
    SOAP_ASSERT(NULL != (soap = ONVIF_soap_new(SOAP_SOCK_TIMEOUT)));

    req.ConfigurationToken = ConfigurationToken;
    ONVIF_SetAuthInfo(soap, usrName, passWord);
    try
    {
        result = soap_call___trt__GetVideoEncoderConfigurationOptions(soap, MediaXAddr, NULL, &req, rep);
    }
    catch (const std::exception&)
    {
        printf("输入参数有误！\n");
    }

    if (NULL != soap) {
        ONVIF_soap_delete(soap);
    }

    return result;
}

/************************************************************************
**函数：ONVIF_GetVideoEncoderConfiguration
**功能：获取设备上指定的视频编码器配置信息
**参数：
        [in] MediaXAddr - 媒体服务地址
        [in] ConfigurationToken - 视频编码器配置的令牌字符串
**返回：
        0表明成功，非0表明失败
**备注：
************************************************************************/
int ONVIF_GetVideoEncoderConfiguration(const char *MediaXAddr, char *ConfigurationToken, const char* usrName, const char* passWord)
{
    int result = 0;
    struct soap *soap = NULL;
    _trt__GetVideoEncoderConfiguration          req;
    _trt__GetVideoEncoderConfigurationResponse  rep;

    SOAP_ASSERT(NULL != MediaXAddr);
    SOAP_ASSERT(NULL != (soap = ONVIF_soap_new(SOAP_SOCK_TIMEOUT)));

    ONVIF_SetAuthInfo(soap, usrName, passWord);

    req.ConfigurationToken = ConfigurationToken;
    try
    {
        result = soap_call___trt__GetVideoEncoderConfiguration(soap, MediaXAddr, NULL, &req, rep);
    }
    catch (const std::exception& e)
    {
        printf(" 获取视频配置信息接口输入参数有误！\n");
    }

    if (NULL != soap) {
        ONVIF_soap_delete(soap);
    }

    return result;
}

/************************************************************************
**函数：ONVIF_SetVideoEncoderConfiguration
**功能：修改指定的视频编码器配置信息
**参数：
        [in] MediaXAddr - 媒体服务地址
        [in] venc - 视频编码器配置信息
**返回：
        0表明成功，非0表明失败
**备注：
    1). 所设置的分辨率必须是GetVideoEncoderConfigurationOptions返回的“分辨率选项集”中的一种，否则调用SetVideoEncoderConfiguration会失败。
************************************************************************/
int ONVIF_SetVideoEncoderConfiguration(const char *MediaXAddr, struct tagVideoEncoderConfiguration *venc, struct AudioVideoConfiguration &conf, const char* usrName, const char* passWord)
{
    int result = 0;
    struct soap *soap = NULL;

    _trt__GetVideoEncoderConfiguration           gVECfg_req;
    _trt__GetVideoEncoderConfigurationResponse   gVECfg_rep;

    _trt__SetVideoEncoderConfiguration           sVECfg_req;
    _trt__SetVideoEncoderConfigurationResponse   sVECfg_rep;

    SOAP_ASSERT(NULL != MediaXAddr);
    SOAP_ASSERT(NULL != venc);
    SOAP_ASSERT(NULL != (soap = ONVIF_soap_new(SOAP_SOCK_TIMEOUT)));

    gVECfg_req.ConfigurationToken = venc->token;
    ONVIF_SetAuthInfo(soap, usrName, passWord);
    try
    {
        result = soap_call___trt__GetVideoEncoderConfiguration(soap, MediaXAddr, NULL, &gVECfg_req, gVECfg_rep);
    }
    catch (const std::exception&)
    {
        printf("设置视频参数接口输入参数有误！\n");
    }

    SOAP_CHECK_ERROR(result, soap, "GetVideoEncoderConfiguration");

    if (NULL == gVECfg_rep.Configuration) {
        SOAP_DBGERR("video encoder configuration is NULL.\n");
        goto EXIT;
    }

    sVECfg_req.ForcePersistence = true;
    sVECfg_req.Configuration = gVECfg_rep.Configuration;

    if (NULL != sVECfg_req.Configuration->Resolution) {
        //conf.videoEncode
        sVECfg_req.Configuration->Encoding = tt__VideoEncoding__H264;
        sVECfg_req.Configuration->Resolution->Width = venc->Width;
        printf("Width ==%d\n", sVECfg_req.Configuration->Resolution->Width);
        sVECfg_req.Configuration->Resolution->Height = venc->Height;
        printf("height ==%d\n", sVECfg_req.Configuration->Resolution->Height);

        sVECfg_req.Configuration->Quality = conf.imageQuality;//图像质量
        printf("Quality ==%f\n", sVECfg_req.Configuration->Quality);

        sVECfg_req.Configuration->RateControl->BitrateLimit = conf.BitrateLimit;//码率上限
        printf("BitrateLimit ==%d\n", sVECfg_req.Configuration->RateControl->BitrateLimit);

        sVECfg_req.Configuration->RateControl->FrameRateLimit = conf.FrameRateLimit;//视频帧率
        printf("FrameRateLimit ==%d\n", sVECfg_req.Configuration->RateControl->FrameRateLimit);

        sVECfg_req.Configuration->H264->GovLength = conf.GovLength;//1~400 帧间隔
        printf("GovLength ==%d\n",sVECfg_req.Configuration->H264->GovLength);
    }
    else
    {
        printf("GetVideoEncoderConfiguration error! \n ");
        goto EXIT;
    }

    ONVIF_SetAuthInfo(soap, usrName, passWord);
    result = soap_call___trt__SetVideoEncoderConfiguration(soap, MediaXAddr, NULL, &sVECfg_req, sVECfg_rep);
    printf("SetVideoEncoderConfiguration->result === %d\n", result);
    SOAP_CHECK_ERROR(result, soap, "SetVideoEncoderConfiguration");

EXIT:
    if (SOAP_OK == result) {
        SOAP_DBGLOG("\nSetVideoEncoderConfiguration success!!!\n");
    }

    if (NULL != soap) {
        ONVIF_soap_delete(soap);
    }

    return result;
}

//音频配置
//获取多个音频源配置
int ONVIF_GetAudioEncoderConfigurationOptions(const char *DeviceXAddr, const char* token, const char* usrName, const char* passWord)
{
    int result;
    struct soap *soap = NULL;
    SOAP_ASSERT(NULL != (soap = ONVIF_soap_new(SOAP_SOCK_TIMEOUT)));

    _trt__GetAudioEncoderConfigurationOptions reQ;
    _trt__GetAudioEncoderConfigurationOptionsResponse reP;
    reQ.ConfigurationToken = (char*)token;

    ONVIF_SetAuthInfo(soap, usrName, passWord);
    try
    {
        result = soap_call___trt__GetAudioEncoderConfigurationOptions(soap, DeviceXAddr, NULL, &reQ, reP);
    }
    catch (const std::exception&)
    {
        printf("获取多个音频源配置输入参数有误！\n");
    }

    if (NULL != soap) {
        ONVIF_soap_delete(soap);
    }

    return result;
}
//获取音频配置
int ONVIF_GetAudioEncoderConfiguration(const char *DeviceXAddr, const char* token, const char* usrName, const char* passWord)
{
    int result;
    struct soap *soap = NULL;
    SOAP_ASSERT(NULL != (soap = ONVIF_soap_new(SOAP_SOCK_TIMEOUT)));

    _trt__GetAudioEncoderConfiguration reQ;
    _trt__GetAudioEncoderConfigurationResponse reP;
    ONVIF_SetAuthInfo(soap, usrName, passWord);

    reQ.ConfigurationToken = (char*)token;
    try
    {
        result = soap_call___trt__GetAudioEncoderConfiguration(soap, DeviceXAddr, NULL, &reQ, reP);
    }
    catch (const std::exception&)
    {
        printf("获取音频源配置输入参数有误！\n");
    }

    if (NULL != soap) {
        ONVIF_soap_delete(soap);
    }
    return result;
}

int ONVIF_SetAudioEncoderConfiguration(const char *DeviceXAddr, const char *token, struct AudioVideoConfiguration &conf, const char* usrName, const char* passWord)
{
    int result;
    struct soap *soap = NULL;
    SOAP_ASSERT(NULL != (soap = ONVIF_soap_new(SOAP_SOCK_TIMEOUT)));

    //获取音频配置信息
    _trt__GetAudioEncoderConfiguration reQ;
    _trt__GetAudioEncoderConfigurationResponse reP;

    _trt__SetAudioEncoderConfiguration reQS;
    _trt__SetAudioEncoderConfigurationResponse rePS;

    ONVIF_SetAuthInfo(soap, usrName, passWord);
    reQ.ConfigurationToken = (char*)token;
    try
    {
        result = soap_call___trt__GetAudioEncoderConfiguration(soap, DeviceXAddr, NULL, &reQ, reP);
    }
    catch (const std::exception&)
    {
        printf("设置音频源配置输入参数有误！\n");
    }

    SOAP_CHECK_ERROR(result, soap, "GetAudioEncoderConfiguration");

    if (NULL == reP.Configuration) {
        SOAP_DBGERR("video encoder configuration is NULL.\n");
        goto EXIT;
    }

    reQS.ForcePersistence = true;
    reQS.Configuration = reP.Configuration;

    if (NULL != reQS.Configuration)
    {
        reQS.Configuration->Encoding = tt__AudioEncoding__AAC;
        reQS.Configuration->Bitrate = 32;//16 32 64 音频码率
        reQS.Configuration->SampleRate = 16;//采样率
    }
    else
    {
        printf("GetAudioEncoderConfiguration error! \n ");
        goto EXIT;
    }

    ONVIF_SetAuthInfo(soap, USERNAME, PASSWORD);
    result = soap_call___trt__SetAudioEncoderConfiguration(soap, DeviceXAddr, NULL, &reQS, rePS);

EXIT:
    if (SOAP_OK == result) {
        SOAP_DBGLOG("SetAudioEncoderConfiguration success!!!\n");
    }

    if (NULL != soap) {
        ONVIF_soap_delete(soap);
    }
    return result;
}


int ONVIF_GetPictureConfiguretion(const char *ImgXAddr,const char* videoSourceToken,struct imgConfigure* imgConf, const char* usrName, const char* passWord)
{
    int result = 0;
    struct soap *soap = NULL;
    _timg__GetImagingSettings req;
    _timg__GetImagingSettingsResponse rep;
    _timg__GetOptions optionReq;
    _timg__GetOptionsResponse optionRep;

    SOAP_ASSERT(NULL != ImgXAddr);
    SOAP_ASSERT(NULL != (soap = ONVIF_soap_new(SOAP_SOCK_TIMEOUT)));

    printf("call get img setting \n");

    ONVIF_SetAuthInfo(soap, usrName, passWord);

    req.VideoSourceToken=(char*)videoSourceToken;
    result= soap_call___timg__GetImagingSettings(soap,ImgXAddr,NULL,&req,rep);
    printf("get img setting result is %d\n",result);
    if(rep.ImagingSettings!=NULL)
    {
        //    printf("get result is %f，%f,%f\n",*(rep.ImagingSettings->Brightness),*(rep.ImagingSettings->ColorSaturation),*(rep.ImagingSettings->Contrast));
        imgConf->brightness=*(rep.ImagingSettings->Brightness);
        imgConf->contrast=*(rep.ImagingSettings->Contrast);
        imgConf->colorsaturation=*(rep.ImagingSettings->ColorSaturation);
        printf("get img conf is %f %f %f\n",imgConf->brightness,imgConf->contrast,imgConf->colorsaturation);
    }

     ONVIF_SetAuthInfo(soap, usrName, passWord);
    optionReq.VideoSourceToken=(char*)videoSourceToken;
    result=soap_call___timg__GetOptions(soap,ImgXAddr,NULL,&optionReq,optionRep);
    printf("get img options result is %d\n",result);
    if(optionRep.ImagingOptions!=NULL)
    {
        if(optionRep.ImagingOptions->Brightness!=NULL)
        {
            imgConf->brightnessMin=optionRep.ImagingOptions->Brightness->Min;
            imgConf->brightnessMax=optionRep.ImagingOptions->Brightness->Max;
        }
        if(optionRep.ImagingOptions->Contrast!=NULL)
        {
            imgConf->contrastMin=optionRep.ImagingOptions->Contrast->Min;
            imgConf->contrastMax=optionRep.ImagingOptions->Contrast->Max;
        }
        if(optionRep.ImagingOptions->ColorSaturation!=NULL)
        {
            imgConf->colorsaturationMin=optionRep.ImagingOptions->ColorSaturation->Min;
            imgConf->colorsaturationMax=optionRep.ImagingOptions->ColorSaturation->Max;
        }
        printf("get img conf is %f %f %f %f %f %f\n",imgConf->brightnessMin,imgConf->brightnessMax, imgConf->contrastMin,imgConf->contrastMax,imgConf->colorsaturationMin,imgConf->colorsaturationMax);
    }

    if (NULL != soap) {
        ONVIF_soap_delete(soap);
    }
    return result;
}

int ONVIF_SetPictureConfiguretion(const char *ImgXAddr,const char* videoSourceToken,struct imgConfigure* imgConf, const char* usrName, const char* passWord)
{
    int result = 0;
    struct soap *soap = NULL;
    _timg__GetImagingSettings getreq;
    _timg__GetImagingSettingsResponse getrep;
    _timg__SetImagingSettings setreq;
    _timg__SetImagingSettingsResponse setrep;

    SOAP_ASSERT(NULL != ImgXAddr);
    SOAP_ASSERT(NULL != (soap = ONVIF_soap_new(SOAP_SOCK_TIMEOUT)));

    printf("call get img setting \n");

    ONVIF_SetAuthInfo(soap, usrName, passWord);

    getreq.VideoSourceToken=(char*)videoSourceToken;
    result= soap_call___timg__GetImagingSettings(soap,ImgXAddr,NULL,&getreq,getrep);
    printf("get result is %d\n",result);

    if(result!=0)
        return result;

    if(getrep.ImagingSettings==NULL)
    {
        printf("get image setting is NULL\n");
        return -1;
    }

    ONVIF_SetAuthInfo(soap, usrName, passWord);
    bool isKeep=true;
    setreq.ForcePersistence=&isKeep;
    setreq.VideoSourceToken=(char*)videoSourceToken;
    setreq.ImagingSettings=getrep.ImagingSettings;
    setreq.ImagingSettings->Brightness=&(imgConf->brightness);
    setreq.ImagingSettings->Contrast=&(imgConf->contrast);
    setreq.ImagingSettings->ColorSaturation=&(imgConf->colorsaturation);

    result= soap_call___timg__SetImagingSettings(soap,ImgXAddr,NULL,&setreq,setrep);

    printf("set result is %d\n",result);
    if (NULL != soap) {
        ONVIF_soap_delete(soap);
    }
    return result;
}

enum PTZCMD getPTZTYPE(int type)
{
    switch (type) {
    case 0:
        return PTZ_CMD_LEFT;
    case 1:
        return PTZ_CMD_RIGHT;
    case 2:
        return PTZ_CMD_UP;
    case 3:
        return PTZ_CMD_DOWN;
    case 4:
        return PTZ_CMD_LEFTUP;
    case 5:
        return PTZ_CMD_LEFTDOWN;
    case 6:
        return PTZ_CMD_RIGHTUP;
    case 7:
        return PTZ_CMD_RIGHTDOWN;
    case 8:
        return PTZ_CMD_ZOOM_IN;
    case 9:
        return PTZ_CMD_ZOOM_OUT;
    default:
        return PTZ_CMD_STOP;
    }
}
