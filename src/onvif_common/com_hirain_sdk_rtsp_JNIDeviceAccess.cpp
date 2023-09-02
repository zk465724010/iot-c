#include "com_hirain_sdk_rtsp_JNIDeviceAccess.h"
#include "onvifSDK_com.h"
#include "log.h"
#include "cmap.h"
#include "Thread11.h"
#include "thread_pool.h"
//#include <WinSock2.h>
//#include <WS2tcpip.h>
extern "C"
{
#include "libavformat/avformat.h"
#include  "libavutil/time.h"
};
//#include <windows.h>
//#include <string>
using namespace std;
CMap<string, device_status_t> g_dev_map;
CThread11 *g_thds = NULL;
ThreadPool* g_thd_pool = NULL;

/*功能：设备发现
 * Class:     com_hirain_sdk_rtsp_JNIDeviceAccess
 * Method:    ONVIFDetectDevice
 * Signature: ()Ljava/util/List;
 * 返回值：IPC服务地址
 */
JNIEXPORT jobject JNICALL Java_com_hirain_sdk_rtsp_JNIDeviceAccess_ONVIFDetectDevice
(JNIEnv *env, jobject jthis)
{
    //返回对象初始化
    //获取ArrayList类引用
    jclass cls_ArrayList = env->FindClass("java/util/ArrayList");

    //获取ArrayList构造函数id
    jmethodID construct = env->GetMethodID(cls_ArrayList, "<init>", "()V");
    //创建一个ArrayList对象
    jobject obj_ArrayList = env->NewObject(cls_ArrayList, construct, "");
    //获取ArrayList对象的add()的methodID
    jmethodID arrayList_add = env->GetMethodID(cls_ArrayList, "add","(Ljava/lang/Object;)Z");

    //获取WsddProbeMatchType类 //com/hirain/sdk/JNIEntity
    jclass cls_user = env->FindClass("com/hirain/sdk/JNIEntity/WsddProbeMatchType");
    //获取StuInfo的构造函数
    jmethodID construct_user = env->GetMethodID(cls_user, "<init>","()V");
    //jobject obj_user = env->NewObject(cls_user,construct_user,"");

    int result = 0;
    struct soap *soap = NULL;										//soap环境变量
    struct wsdd__ProbeType		probeType;							//用于发送Probe消息
    struct __wsdd__ProbeMatches	probeMatches;						//用于接收Probe消息
    struct wsdd__ProbeMatchType  *probeMatch;

    SOAP_ASSERT(NULL != (soap = ONVIF_soap_new(SOAP_SOCK_TIMEOUT)));

    ONVIF_init_header(soap);										//设置消息头描述
    ONVIF_init_ProbeType(soap, &probeType);							//设置寻找的设备的范围和类型

    result = soap_send___wsdd__Probe(soap, SOAP_MCAST_ADDR, NULL, &probeType);
    //    SOAP_CHECK_ERROR(result, soap, "DetectDevice");
    while (SOAP_OK == result)
    {
        memset(&probeMatches, 0x00, sizeof(probeMatches));
        result = soap_recv___wsdd__ProbeMatches(soap, &probeMatches);
        printf("result = %d\n",result);
        if (SOAP_OK == result)
        {
            if (soap->error)
            {
                soap_perror(soap, "ProbeMatches");
            }
            else // 成功接收到设备的应答消息
            {
                if (NULL != probeMatches.wsdd__ProbeMatches)
                {
                    printf("size = %d\n",probeMatches.wsdd__ProbeMatches->__sizeProbeMatch);
                    for (int i = 0; i < probeMatches.wsdd__ProbeMatches->__sizeProbeMatch; i++)
                    {
                        probeMatch = probeMatches.wsdd__ProbeMatches->ProbeMatch + i;
                        {
                            jobject obj_user = env->NewObject(cls_user,construct_user,"");

                            printf("Target EP Address			:= %s\r\n", probeMatch->wsa__EndpointReference.Address);
                            jstring uidaddr = CStr2jstring(env,probeMatch->wsa__EndpointReference.Address);
                            jfieldID uidID = env->GetFieldID(cls_user,"uuid","Ljava/lang/String;");
                            env->SetObjectField(obj_user,uidID,uidaddr);

                            printf("Target Service Address		:= %s\r\n", probeMatch->XAddrs);
                            jstring serviceaddr = CStr2jstring(env,probeMatch->XAddrs);
                            jfieldID serviceID = env->GetFieldID(cls_user,"serviceXAddrs","Ljava/lang/String;");
                            env->SetObjectField(obj_user,serviceID,serviceaddr);

                            //jobject jObject = env->NewObject(jcls, jid,uidaddr,serviceaddr);
                            env->CallObjectMethod(obj_ArrayList, arrayList_add, obj_user);
                        }
                    }

                }
            }
        }
        else if (soap->error)
        {
            printf("[%d] soap error 2: %d, %s, %s\n", __LINE__, soap->error, *soap_faultcode(soap), *soap_faultstring(soap));
        }
    }
    //EXIT:
    if (NULL != soap) {
        ONVIF_soap_delete(soap);
    }
    return obj_ArrayList;
}

/*
 * Class:     com_hirain_sdk_rtsp_JNIDeviceAccess
 * Method:    ONVIFNewDetectDevice
 * Signature: ()Ljava/util/List;
 */
JNIEXPORT jobject JNICALL Java_com_hirain_sdk_rtsp_JNIDeviceAccess_ONVIFNewDetectDevice
(JNIEnv *env, jobject jthis)
{
    /**************jni初始化***********************/
    //返回对象初始化
    //获取ArrayList类引用
    jclass cls_ArrayList = env->FindClass("java/util/ArrayList");

    //获取ArrayList构造函数id
    jmethodID construct = env->GetMethodID(cls_ArrayList, "<init>", "()V");
    //创建一个ArrayList对象
    jobject obj_ArrayList = env->NewObject(cls_ArrayList, construct, "");
    //获取ArrayList对象的add()的methodID
    jmethodID arrayList_add = env->GetMethodID(cls_ArrayList, "add","(Ljava/lang/Object;)Z");

    //获取WsddProbeMatchType类 //com/hirain/sdk/JNIEntity
    jclass cls_user = env->FindClass("com/hirain/sdk/JNIEntity/WsddProbeMatchType");
    if(NULL == cls_user) {
        printf("find WsddProbeMatchType error\n");
    }
    //获取StuInfo的构造函数
    jmethodID construct_user = env->GetMethodID(cls_user, "<init>","()V");
    /************************************/

    /* 探测消息(Probe)，这些内容是ONVIF Device Test Tool 15.06工具搜索IPC时的Probe消息，通过Wireshark抓包工具抓包到的 */
    const char *probe = "<?xml version=\"1.0\" encoding=\"utf-8\"?><Envelope xmlns:dn=\"http://www.onvif.org/ver10/network/wsdl\" xmlns=\"http://www.w3.org/2003/05/soap-envelope\"><Header><wsa:MessageID xmlns:wsa=\"http://schemas.xmlsoap.org/ws/2004/08/addressing\">uuid:fc0bad56-5f5a-47f3-8ae2-c94a4e907d70</wsa:MessageID><wsa:To xmlns:wsa=\"http://schemas.xmlsoap.org/ws/2004/08/addressing\">urn:schemas-xmlsoap-org:ws:2005:04:discovery</wsa:To><wsa:Action xmlns:wsa=\"http://schemas.xmlsoap.org/ws/2004/08/addressing\">http://schemas.xmlsoap.org/ws/2005/04/discovery/Probe</wsa:Action></Header><Body><Probe xmlns:xsi=\"http://www.w3.org/2001/XMLSchema-instance\" xmlns:xsd=\"http://www.w3.org/2001/XMLSchema\" xmlns=\"http://schemas.xmlsoap.org/ws/2005/04/discovery\"><Types>dn:NetworkVideoTransmitter</Types><Scopes /></Probe></Body></Envelope>";

    int ret;
    SOAP_SOCKET s;
    SOAP_SOCKLEN_T len;
    char recv_buff[4096] = { 0 };
    sockaddr_in multi_addr;
    sockaddr_in client_addr;
    sockaddr_in bind_addr;
#ifdef WIN32
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {                             // 初始化Windows Sockets DLL
        printf("Could not open Windows connection.\n");
        return obj_ArrayList;
    }
    if (LOBYTE(wsaData.wVersion) != 2 || HIBYTE(wsaData.wVersion) != 2) {
        printf("the version of WinSock DLL is not 2.2.\n");
        return obj_ArrayList;
    }
#endif

    s = socket(AF_INET, SOCK_DGRAM, 0);
    // 建立数据报套接字
    if (s < 0) {
        printf("socket init failed\n");
    }
    memset(&bind_addr,0,sizeof(bind_addr));
    char local_ip4[INET_ADDRSTRLEN] = {0};
    get_local_ip_using_create_socket(local_ip4);
    printf("local_ip4 ===================%s\n",local_ip4);

    bind_addr.sin_addr.s_addr = inet_addr(local_ip4);
    bind_addr.sin_family = AF_INET;
    bind_addr.sin_port = htons(0);
    int n = ::bind(s, (struct sockaddr*)&bind_addr, sizeof(bind_addr));
    if(n<0)
        printf("bind fail\n");
    multi_addr.sin_addr.s_addr = inet_addr(CAST_ADDR);
    multi_addr.sin_family = AF_INET;
    multi_addr.sin_port = htons(CAST_PORT);

    if(UDPsendMessage(multi_addr, s, probe) < 0)
    {
        SLEEP(1);
        UDPsendMessage(multi_addr, s, probe);
    }//发送UDP广播
    printf("Send Probe message to [%s:%d]\n\n", CAST_ADDR, CAST_PORT);
    //SLEEP(2);
    unsigned long on = 1;

#if defined(__linux__) || defined(__linux)
    int flag = fcntl(s, F_GETFL, 0);
    if (flag < 0)
    {
        printf("fcntl failed.\n");
//        exit(1);
        return obj_ArrayList;
    }
//    printf("1\n");
    flag |= O_NONBLOCK;
    if (fcntl(s, F_SETFL, flag) < 0)
    {
        printf("fcntl failed.\n");
//        exit(1);
        return obj_ArrayList;
    }
//    printf("2\n");
#else
    int ReValue =ioctlsocket(s, FIONBIO, &on);
    printf("ioctlsocket === %d\n",ReValue);
#endif

    fd_set read_fdset;
    FD_ZERO(&read_fdset);
    FD_SET(s,&read_fdset);
    struct timeval timeout;
    timeout.tv_sec=5;
    timeout.tv_usec=0;
    for(;;)
    {
        ret=select(s+1,&read_fdset,NULL,NULL,&timeout);
        printf("select result is %d\n",ret);
        if(ret<=0)
            break;
        // 接收IPC的应答消息(ProbeMatch)
        len = sizeof(client_addr);
        memset(recv_buff, 0, sizeof(recv_buff));
        memset(&client_addr, 0, sizeof(sockaddr));
        ret = recvfrom(s, recv_buff, sizeof(recv_buff) - 1, 0, (struct sockaddr*)&client_addr, &len);
        printf("recvfrom ====%d\n",ret);
        if(ret < 0)
        {
            break;
        }else
        {
            //printf("===Recv ProbeMatch from [%s:%d]===\n", inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port));
            //printf("===recv_buff===%s\n\n", recv_buff);
            jobject obj_user = env->NewObject(cls_user,construct_user,"");

            char uuid[ONVIF_ADDRESS_SIZE]={'0'};
            splitChar(recv_buff,1,uuid);

            //char getUid[ONVIF_ADDRESS_SIZE]={'0'};
            //removeChar(uuid,getUid);
            printf("uuid ====%s\n",uuid);
            if(NULL == uuid)
            {
                return obj_ArrayList;
            }
            //printf("uuid ====%s\n",uuid);
            jstring uidaddr = CStr2jstring(env,uuid);
            jfieldID uidID = env->GetFieldID(cls_user,"uuid","Ljava/lang/String;");
            env->SetObjectField(obj_user,uidID,uidaddr);

            char xaddr[ONVIF_ADDRESS_SIZE]={'0'};
            splitChar(recv_buff,2,xaddr);

            char getXaddr[ONVIF_ADDRESS_SIZE]={'0'};
            if(NULL != xaddr)
            {
                removeChar(xaddr,getXaddr);
            }else
            {
                printf("xaddr is emtipy !\n");
                return obj_ArrayList;
            }
            printf("getXaddr ====%s\n",getXaddr);
            jstring serviceaddr = CStr2jstring(env,getXaddr);
            jfieldID serviceID = env->GetFieldID(cls_user,"serviceXAddrs","Ljava/lang/String;");
            env->SetObjectField(obj_user,serviceID,serviceaddr);

            env->CallObjectMethod(obj_ArrayList, arrayList_add, obj_user);
        }
        //        SLEEP(1);
    }
    soap_closesocket(s);
    return obj_ArrayList;
}

/*
 * Class:     com_hirain_sdk_rtsp_JNIDeviceAccess
 * Method:    ONVIFGetDeviceInformation
 * Signature: (Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;)Lcom/hirain/sdk/entity/rtsp/DeviceInformationResponse;
 */
JNIEXPORT jobject JNICALL Java_com_hirain_sdk_rtsp_JNIDeviceAccess_ONVIFGetDeviceInformation
(JNIEnv *env, jobject jthis, jstring jdeviceAddr, jstring jusrName, jstring jpassword)
{
    const char* userName = jstring2CStr(env,jusrName);
    const char* passWord = jstring2CStr(env,jpassword);
    const char* deviceAddr = jstring2CStr(env,jdeviceAddr);

    int result = 0;
    struct soap *soap = NULL;
    _tds__GetDeviceInformation           devinfo_req;
    _tds__GetDeviceInformationResponse   devinfo_resp;

    SOAP_ASSERT(NULL != deviceAddr);
    SOAP_ASSERT(NULL != (soap = ONVIF_soap_new(SOAP_SOCK_TIMEOUT)));

    //鉴权
    ONVIF_SetAuthInfo(soap, userName, passWord);
    result = soap_call___tds__GetDeviceInformation(soap, deviceAddr, NULL, &devinfo_req, devinfo_resp);
    //    SOAP_CHECK_ERROR(result, soap, "GetDeviceInformation");
    if(0 != result)
    {
        return NULL;
    }

    jclass jClass = env->FindClass("com/hirain/sdk/JNIEntity/DeviceInformationResponse");
    jmethodID jMethod = env->GetMethodID(jClass, "<init>", "()V");
    jobject jObject = env->NewObject(jClass, jMethod);

    printf("Manufacturer:%s\n", devinfo_resp.Manufacturer);
    jfieldID ManufacturerID = env->GetFieldID(jClass,"manuFacturer","Ljava/lang/String;");


    jstring Manufacturer = CStr2jstring(env,devinfo_resp.Manufacturer);
    env->SetObjectField(jObject,ManufacturerID,Manufacturer);
    if (NULL != soap) {
        ONVIF_soap_delete(soap);
    }
    return jObject;
}


/**
  * 获取设备能力信息
  * @param deviceXAddr
  * @return：返回PTZ的服务地址，包含（是：有PTZ功能，否：无）
  * Class:     com_hirain_sdk_rtsp_JNIDeviceAccess
  * Method:    ONVIFGetCapabilities
  * Signature: (Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;)Lcom/hirain/sdk/entity/rtsp/DeviceCapabilities;
  */
JNIEXPORT jobject JNICALL Java_com_hirain_sdk_rtsp_JNIDeviceAccess_ONVIFGetCapabilities
(JNIEnv *env, jobject jthis, jstring jdeviceAddr, jstring jusrName, jstring jpassword)
{
    const char* userName = jstring2CStr(env,jusrName);
    const char* passWord = jstring2CStr(env,jpassword);
    const char* deviceAddr = jstring2CStr(env,jdeviceAddr);

    int result = 0;
    struct soap *soap = NULL;
    SOAP_ASSERT(NULL != (soap = ONVIF_soap_new(SOAP_SOCK_TIMEOUT)));

    _tds__GetCapabilities            req;
    _tds__GetCapabilitiesResponse    rep;

    ONVIF_SetAuthInfo(soap, userName, passWord);
    result = soap_call___tds__GetCapabilities(soap, deviceAddr, NULL, &req, rep);
    //    SOAP_CHECK_ERROR(result, soap, "GetCapabilities");
    //jni初始化
    jclass jClass = env->FindClass("com/hirain/sdk/JNIEntity/DeviceCapabilities");
    jmethodID jMethod = env->GetMethodID(jClass, "<init>", "()V");
    jobject jObject = env->NewObject(jClass, jMethod);

    if (NULL != rep.Capabilities) {
        if (NULL != rep.Capabilities->Analytics) {
            jfieldID jAnalyticsID = env->GetFieldID(jClass,"Analytics_XAddr","Ljava/lang/String;");
            env->SetObjectField(jObject,jAnalyticsID,CStr2jstring(env,rep.Capabilities->Analytics->XAddr));
        }
        if (NULL != rep.Capabilities->Device) {
            jfieldID jDeviceID = env->GetFieldID(jClass,"Device_XAddr","Ljava/lang/String;");
            env->SetObjectField(jObject,jDeviceID,CStr2jstring(env,rep.Capabilities->Device->XAddr));
        }
        if (NULL != rep.Capabilities->Events) {
            jfieldID jEventsID = env->GetFieldID(jClass,"Events_XAddr","Ljava/lang/String;");
            env->SetObjectField(jObject,jEventsID,CStr2jstring(env,rep.Capabilities->Events->XAddr));
        }
        if (NULL != rep.Capabilities->Imaging) {
            jfieldID jImagingID = env->GetFieldID(jClass,"Imaging_XAddr","Ljava/lang/String;");
            env->SetObjectField(jObject,jImagingID,CStr2jstring(env,rep.Capabilities->Imaging->XAddr));
        }
        if (NULL != rep.Capabilities->Media) {
            jfieldID jMediaID = env->GetFieldID(jClass,"Media_XAddr","Ljava/lang/String;");
            env->SetObjectField(jObject,jMediaID,CStr2jstring(env,rep.Capabilities->Media->XAddr));
        }
        if (NULL != rep.Capabilities->PTZ) {
            jfieldID jPTZID = env->GetFieldID(jClass,"PTZ_XAddr","Ljava/lang/String;");
            env->SetObjectField(jObject,jPTZID,CStr2jstring(env,rep.Capabilities->PTZ->XAddr));
        }
    }
    //EXIT:
    if (NULL != soap) {
        ONVIF_soap_delete(soap);
    }

    return jObject;
}

/**
  * 获取网络协议
  * @param deviceXAddr
  * @return 端口号， 网络协议（HTTP HTTPS RTSP）
  * Class:     com_hirain_sdk_rtsp_JNIDeviceAccess
  * Method:    ONVIFGetNetworkProtocols
  * Signature: (Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;)Lcom/hirain/sdk/entity/rtsp/TdsSetNetworkProtocols;
  */
JNIEXPORT jobject JNICALL Java_com_hirain_sdk_rtsp_JNIDeviceAccess_ONVIFGetNetworkProtocols
(JNIEnv *env, jobject jthis, jstring jdeviceAddr, jstring jusrName, jstring jpassword)
{
    const char* userName = jstring2CStr(env,jusrName);
    const char* passWord = jstring2CStr(env,jpassword);
    const char* deviceAddr = jstring2CStr(env,jdeviceAddr);

    int result;
    struct soap *soap = NULL;
    SOAP_ASSERT(NULL != (soap = ONVIF_soap_new(SOAP_SOCK_TIMEOUT)));

    _tds__GetNetworkProtocols netProtocol;
    _tds__GetNetworkProtocolsResponse netProtocolRep;

    ONVIF_SetAuthInfo(soap, userName, passWord);
    result = soap_call___tds__GetNetworkProtocols(soap, deviceAddr, NULL, &netProtocol, netProtocolRep);
    //SOAP_CHECK_ERROR(result, soap, "GetNetworkProtocols");

    //jni初始化
    jclass jClass = env->FindClass("com/hirain/sdk/JNIEntity/TdsSetNetworkProtocols");
    jmethodID jMethod = env->GetMethodID(jClass, "<init>", "()V");
    jobject jObject = env->NewObject(jClass, jMethod);

    //    std::string port = "";
    char cport[64];
    for (size_t i = 0; i < netProtocolRep.__sizeNetworkProtocols; i++)
    {
        tt__NetworkProtocol *NetworkProtocols = netProtocolRep.NetworkProtocols[i];
#
        //        port += std::to_string(NetworkProtocols->Port[0]);
        sprintf(cport,"%d",NetworkProtocols->Port[0]);
        printf("Port:%s\n", cport);
        if(i  != netProtocolRep.__sizeNetworkProtocols -1)
        {
            sprintf(cport,"%s",";");
        }
    }
    jfieldID jportID = env->GetFieldID(jClass,"tTNetworkProtocolPort","Ljava/lang/String;");
    env->SetObjectField(jObject,jportID,CStr2jstring(env,cport));

    //EXIT:
    if (NULL != soap) {
        ONVIF_soap_delete(soap);
    }
    return jObject;
}

/*
 * Class:     com_hirain_sdk_rtsp_JNIDeviceAccess
 * Method:    ONVIFGetNetworkDefaultGateway
 * Signature: (Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;)Lcom/hirain/sdk/entity/rtsp/TdsSetNetworkDefaultGateway;
 */
JNIEXPORT jobject JNICALL Java_com_hirain_sdk_rtsp_JNIDeviceAccess_ONVIFGetNetworkDefaultGateway
(JNIEnv *env, jobject jthis, jstring jdeviceAddr, jstring jusrName, jstring jpassword)
{
    const char* userName = jstring2CStr(env,jusrName);
    const char* passWord = jstring2CStr(env,jpassword);
    const char* deviceAddr = jstring2CStr(env,jdeviceAddr);

    int result;
    struct soap *soap = NULL;
    SOAP_ASSERT(NULL != (soap = ONVIF_soap_new(SOAP_SOCK_TIMEOUT)));

    _tds__GetNetworkDefaultGateway netGateway;
    _tds__GetNetworkDefaultGatewayResponse netGatewayRep;
    ONVIF_SetAuthInfo(soap, userName, passWord);
    result = soap_call___tds__GetNetworkDefaultGateway(soap, deviceAddr, NULL, &netGateway, netGatewayRep);
    //    SOAP_CHECK_ERROR(result, soap, "GetNetworkDefaultGateway");
    //jni初始化
    jclass jClass = env->FindClass("com/hirain/sdk/JNIEntity/TdsSetNetworkDefaultGateway");
    jmethodID jMethod = env->GetMethodID(jClass, "<init>", "()V");
    jobject jObject = env->NewObject(jClass, jMethod);

    if(NULL != netGatewayRep.NetworkGateway)
    {
        if(netGatewayRep.NetworkGateway->__sizeIPv4Address > 0)
        {
            printf("__sizeIPv4Address:%d\n", netGatewayRep.NetworkGateway->__sizeIPv4Address);
            printf("IPv4Address:%s\n", *netGatewayRep.NetworkGateway->IPv4Address);

            jfieldID jip4ID = env->GetFieldID(jClass,"sizeIPv4Address","I");
            env->SetIntField(jObject,jip4ID,(jint)netGatewayRep.NetworkGateway->__sizeIPv4Address);

            jfieldID jaddrID = env->GetFieldID(jClass,"ipv4Address","Ljava/lang/String;");
            env->SetObjectField(jObject,jaddrID,CStr2jstring(env,*netGatewayRep.NetworkGateway->IPv4Address));
        }
    }
    //EXIT:
    if (NULL != soap) {
        ONVIF_soap_delete(soap);
    }
    return jObject;
}

/*
 * Class:     com_hirain_sdk_rtsp_JNIDeviceAccess
 * Method:    ONVIFGetNetworkInterface
 * Signature: (Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;)Lcom/hirain/sdk/entity/rtsp/TdsSetNetworkDefaultGateway;
 */
JNIEXPORT jobject JNICALL Java_com_hirain_sdk_rtsp_JNIDeviceAccess_ONVIFGetNetworkInterface
(JNIEnv *env, jobject jthis, jstring jdeviceAddr, jstring jusrName, jstring jpassword)
{
    const char* userName = jstring2CStr(env,jusrName);
    const char* passWord = jstring2CStr(env,jpassword);
    const char* deviceAddr = jstring2CStr(env,jdeviceAddr);

    int result;
    struct soap *soap = NULL;
    SOAP_ASSERT(NULL != (soap = ONVIF_soap_new(SOAP_SOCK_TIMEOUT)));

    _tds__GetNetworkInterfaces netInter;
    _tds__GetNetworkInterfacesResponse netInterRep;

    ONVIF_SetAuthInfo(soap, userName, passWord);
    result = soap_call___tds__GetNetworkInterfaces(soap, deviceAddr, NULL, &netInter, netInterRep);
    printf("get Network_Interface->result:%d\n", result);
    printf("__sizeNetworkInterfaces:%d\n", netInterRep.__sizeNetworkInterfaces);

    //jni初始化com.hirain.sdk.JNIEntity.TdsSetNetworkDefaultGateway
    jclass jClass = env->FindClass("com/hirain/sdk/JNIEntity/TdsSetNetworkDefaultGateway");
    jmethodID jMethod = env->GetMethodID(jClass, "<init>", "()V");
    jobject jObject = env->NewObject(jClass, jMethod);

    for (size_t i = 0; i < netInterRep.__sizeNetworkInterfaces; i++)
    {
        if(NULL != netInterRep.NetworkInterfaces[i])
        {
            if(NULL != netInterRep.NetworkInterfaces[i]->IPv4)
            {
                if(NULL !=netInterRep.NetworkInterfaces[i]->IPv4->Config)
                {
                    if( netInterRep.NetworkInterfaces[i]->IPv4->Config->DHCP)//for for DHCP
                    {
                        if(NULL != netInterRep.NetworkInterfaces[i]->IPv4->Config->FromDHCP)
                        {
                            if(NULL != netInterRep.NetworkInterfaces[i]->IPv4->Config->FromDHCP->Address)
                            {
                                printf("Address:%s\n",  netInterRep.NetworkInterfaces[i]->IPv4->Config->FromDHCP->Address);
                                jfieldID jaddrID = env->GetFieldID(jClass,"ipv4Address","Ljava/lang/String;");
                                env->SetObjectField(jObject,jaddrID,CStr2jstring(env, netInterRep.NetworkInterfaces[i]->IPv4->Config->FromDHCP->Address));
                            }
                        }

                    }else
                    {
                        if(NULL != netInterRep.NetworkInterfaces[i]->IPv4->Config->Manual[0])
                        {
                            if(NULL != netInterRep.NetworkInterfaces[i]->IPv4->Config->Manual[0]->Address)
                            {
                                printf("Address:%s\n",  netInterRep.NetworkInterfaces[i]->IPv4->Config->Manual[0]->Address);
                                jfieldID jaddrID = env->GetFieldID(jClass,"ipv4Address","Ljava/lang/String;");
                                env->SetObjectField(jObject,jaddrID,CStr2jstring(env, netInterRep.NetworkInterfaces[i]->IPv4->Config->Manual[0]->Address));
                            }
                        }
                    }
                }
            }
        }

    }

    if (NULL != soap) {
        ONVIF_soap_delete(soap);
    }
    return jObject;
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
JNIEXPORT jobject JNICALL Java_com_hirain_sdk_rtsp_JNIDeviceAccess_ONVIFGetProfiles
(JNIEnv *env, jobject jthis, jstring jmediaXAddr, jstring jusrName, jstring jpassword)
{
    const char* userName = jstring2CStr(env,jusrName);
    const char* passWord = jstring2CStr(env,jpassword);
    const char* deviceAddr = jstring2CStr(env,jmediaXAddr);

    int i = 0;
    int result = 0;
    struct soap *soap = NULL;
    SOAP_ASSERT(NULL != (soap = ONVIF_soap_new(SOAP_SOCK_TIMEOUT)));

    _trt__GetProfiles            req;
    _trt__GetProfilesResponse    rep;

    ONVIF_SetAuthInfo(soap, userName, passWord);
    result = soap_call___trt__GetProfiles(soap, deviceAddr, NULL, &req, rep);
    //    SOAP_CHECK_ERROR(result, soap, "GetProfiles");

    //返回对象初始化
    //获取ArrayList类引用
    jclass cls_ArrayList = env->FindClass("java/util/ArrayList");

    //获取ArrayList构造函数id
    jmethodID construct = env->GetMethodID(cls_ArrayList, "<init>", "()V");
    //创建一个ArrayList对象
    jobject obj_ArrayList = env->NewObject(cls_ArrayList, construct, "");
    //获取ArrayList对象的add()的methodID
    jmethodID arrayList_add = env->GetMethodID(cls_ArrayList, "add","(Ljava/lang/Object;)Z");

    //获取WsddProbeMatchType类 //com/hirain/sdk/JNIEntity
    jclass cls_user = env->FindClass("com/hirain/sdk/JNIEntity/MediaProFiles");
    //获取StuInfo的构造函数
    jmethodID construct_user = env->GetMethodID(cls_user, "<init>","()V");

    for(i = 0; i < rep.__sizeProfiles; i++) {                                   // 提取所有配置文件信息（我们所关心的）
        tt__Profile *ttProfile = rep.Profiles[i];
        jobject jObject = env->NewObject(cls_user,construct_user,"");
        if (NULL != ttProfile)
        {
            // 配置文件Token
            jfieldID nameID = env->GetFieldID(cls_user,"name","Ljava/lang/String;");
            env->SetObjectField(jObject,nameID,CStr2jstring(env,ttProfile->Name));

            // 配置文件Token
            jfieldID tokenID = env->GetFieldID(cls_user,"token","Ljava/lang/String;");
            env->SetObjectField(jObject,tokenID,CStr2jstring(env,ttProfile->token));
        }

        if(NULL != ttProfile->VideoSourceConfiguration)
        {
            jfieldID nameID = env->GetFieldID(cls_user,"VideoSourceConfiguration_name","Ljava/lang/String;");
            env->SetObjectField(jObject,nameID,CStr2jstring(env,ttProfile->VideoSourceConfiguration->Name));

            jfieldID useCountID = env->GetFieldID(cls_user,"VideoSourceConfiguration_UseCount","I");
            env->SetIntField(jObject,useCountID,ttProfile->VideoSourceConfiguration->UseCount);

            jfieldID tokenID = env->GetFieldID(cls_user,"VideoSourceConfiguration_token","Ljava/lang/String;");
            env->SetObjectField(jObject,tokenID,CStr2jstring(env,ttProfile->VideoSourceConfiguration->token));

            jfieldID SourceTokenID = env->GetFieldID(cls_user,"VideoSourceConfiguration_SourceToken","Ljava/lang/String;");
            env->SetObjectField(jObject,SourceTokenID,CStr2jstring(env,ttProfile->VideoSourceConfiguration->SourceToken));
            if(NULL != ttProfile->VideoSourceConfiguration->Bounds)
            {
                jfieldID xID = env->GetFieldID(cls_user,"VideoSourceConfiguration_Bounds_x","I");
                env->SetIntField(jObject,xID,ttProfile->VideoSourceConfiguration->Bounds->x);

                jfieldID yID = env->GetFieldID(cls_user,"VideoSourceConfiguration_Bounds_y","I");
                env->SetIntField(jObject,yID,ttProfile->VideoSourceConfiguration->Bounds->y);

                jfieldID widthID = env->GetFieldID(cls_user,"VideoSourceConfiguration_Bounds_width","I");
                env->SetIntField(jObject,widthID,ttProfile->VideoSourceConfiguration->Bounds->width);

                jfieldID heightID = env->GetFieldID(cls_user,"VideoSourceConfiguration_Bounds_height","I");
                env->SetIntField(jObject,heightID,ttProfile->VideoSourceConfiguration->Bounds->height);
            }
        }

        //视频编码
        if(NULL != ttProfile->VideoEncoderConfiguration)// 视频编码器配置信息
        {
            jfieldID nameID = env->GetFieldID(cls_user,"VideoEncoderConfiguration_name","Ljava/lang/String;");
            env->SetObjectField(jObject,nameID,CStr2jstring(env,ttProfile->VideoEncoderConfiguration->Name));

            jfieldID useCountID = env->GetFieldID(cls_user,"VideoEncoderConfiguration_UseCount","I");
            env->SetIntField(jObject,useCountID,ttProfile->VideoEncoderConfiguration->UseCount);

            // 视频编码器Token
            jfieldID tokenID = env->GetFieldID(cls_user,"VideoEncoderConfiguration_token","Ljava/lang/String;");
            env->SetObjectField(jObject,tokenID,CStr2jstring(env,ttProfile->VideoEncoderConfiguration->token));

            jfieldID EncodingID = env->GetFieldID(cls_user,"VideoEncoderConfiguration_Encoding","Ljava/lang/String;");
            const char* encoding = dump_enum2str_VideoEncoding(ttProfile->VideoEncoderConfiguration->Encoding);
            env->SetObjectField(jObject,EncodingID,CStr2jstring(env,encoding));

            if(NULL != ttProfile->VideoEncoderConfiguration->Resolution)// 视频编码器分辨率
            {
                jfieldID xID = env->GetFieldID(cls_user,"VideoEncoderConfiguration_Resolution_width","I");
                env->SetIntField(jObject,xID,ttProfile->VideoEncoderConfiguration->Resolution->Width);

                jfieldID yID = env->GetFieldID(cls_user,"VideoEncoderConfiguration_Resolution_height","I");
                env->SetIntField(jObject,yID,ttProfile->VideoEncoderConfiguration->Resolution->Height);
            }

            jfieldID QualityID = env->GetFieldID(cls_user,"VideoEncoderConfiguration_Quality","F");
            env->SetFloatField(jObject,QualityID,ttProfile->VideoEncoderConfiguration->Quality);

            if(NULL!= ttProfile->VideoEncoderConfiguration->RateControl)
            {
                jfieldID FrameRateLimitID = env->GetFieldID(cls_user,"VideoEncoderConfiguration_VideoRateControl_FrameRateLimit","I");
                env->SetIntField(jObject,FrameRateLimitID,ttProfile->VideoEncoderConfiguration->RateControl->FrameRateLimit);

                jfieldID EncodingIntervalID = env->GetFieldID(cls_user,"VideoEncoderConfiguration_VideoRateControl_EncodingInterval","I");
                env->SetIntField(jObject,EncodingIntervalID,ttProfile->VideoEncoderConfiguration->RateControl->EncodingInterval);

                jfieldID BitrateLimitID = env->GetFieldID(cls_user,"VideoEncoderConfiguration_VideoRateControl_BitrateLimit","I");
                env->SetIntField(jObject,BitrateLimitID,ttProfile->VideoEncoderConfiguration->RateControl->BitrateLimit);
            }
        }
        //音频资源
        if (NULL != ttProfile->AudioSourceConfiguration) {                     // 视频编码器配置信息
            jfieldID nameID = env->GetFieldID(cls_user,"AudioSourceConfiguration_name","Ljava/lang/String;");
            env->SetObjectField(jObject,nameID,CStr2jstring(env,ttProfile->AudioSourceConfiguration->Name));

            jfieldID UseCountID = env->GetFieldID(cls_user,"AudioSourceConfiguration_UseCount","I");
            env->SetIntField(jObject,UseCountID,ttProfile->AudioSourceConfiguration->UseCount);

            jfieldID tokenID = env->GetFieldID(cls_user,"AudioSourceConfiguration_token","Ljava/lang/String;");
            env->SetObjectField(jObject,tokenID,CStr2jstring(env,ttProfile->AudioSourceConfiguration->token));

            jfieldID SourceTokenID = env->GetFieldID(cls_user,"AudioSourceConfiguration_SourceToken","Ljava/lang/String;");
            env->SetObjectField(jObject,SourceTokenID,CStr2jstring(env,ttProfile->AudioSourceConfiguration->SourceToken));
        }
        //音频编码
        if(NULL != ttProfile->AudioEncoderConfiguration)
        {
            jfieldID nameID = env->GetFieldID(cls_user,"AudioEncoderConfiguration_name","Ljava/lang/String;");
            env->SetObjectField(jObject,nameID,CStr2jstring(env,ttProfile->AudioEncoderConfiguration->token));

            jfieldID UseCountID = env->GetFieldID(cls_user,"AudioEncoderConfiguration_UseCount","I");
            env->SetIntField(jObject,UseCountID,ttProfile->AudioEncoderConfiguration->UseCount);

            jfieldID tokenID = env->GetFieldID(cls_user,"AudioEncoderConfiguration_token","Ljava/lang/String;");
            env->SetObjectField(jObject,tokenID,CStr2jstring(env,ttProfile->AudioEncoderConfiguration->token));

            jfieldID BitrateID = env->GetFieldID(cls_user,"AudioEncoderConfiguration_Bitrate","I");
            env->SetIntField(jObject,BitrateID,ttProfile->AudioEncoderConfiguration->Bitrate);

            jfieldID SampleRateID = env->GetFieldID(cls_user,"AudioEncoderConfiguration_SampleRate","I");
            env->SetIntField(jObject,SampleRateID,ttProfile->AudioEncoderConfiguration->SampleRate);

            jfieldID EncodingID = env->GetFieldID(cls_user,"AudioEncoderConfiguration_Encoding","Ljava/lang/String;");
            const char* encod = dump_enum2str_AudioEncoding(ttProfile->AudioEncoderConfiguration->Encoding);
            env->SetObjectField(jObject,EncodingID,CStr2jstring(env,encod));
        }
        env->CallObjectMethod(obj_ArrayList, arrayList_add, jObject);
    }

    //EXIT:

    if (NULL != soap) {
        ONVIF_soap_delete(soap);
    }

    return obj_ArrayList;
}

/*
 * Class:     com_hirain_sdk_rtsp_JNIDeviceAccess
 * Method:    ONVIFGetStreamUri
 * Signature: (Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;)Ljava/lang/String;
 */
JNIEXPORT jstring JNICALL Java_com_hirain_sdk_rtsp_JNIDeviceAccess_ONVIFGetStreamUri
(JNIEnv *env, jobject jthis, jstring jmediaAddr,jstring ProfileToken, jstring jusrName, jstring jpassword)
{
    const char* userName = jstring2CStr(env,jusrName);
    const char* passWord = jstring2CStr(env,jpassword);
    const char* deviceAddr = jstring2CStr(env,jmediaAddr);

    int result = 0;
    struct soap *soap = NULL;
    tt__StreamSetup              ttStreamSetup;
    tt__Transport                ttTransport;
    _trt__GetStreamUri           req;
    _trt__GetStreamUriResponse   rep;

    SOAP_ASSERT(NULL != (soap = ONVIF_soap_new(SOAP_SOCK_TIMEOUT)));

    ttStreamSetup.Stream = tt__StreamType__RTP_Unicast;
    ttStreamSetup.Transport = &ttTransport;
    ttStreamSetup.Transport->Protocol = tt__TransportProtocol__RTSP;
    ttStreamSetup.Transport->Tunnel = NULL;
    req.StreamSetup = &ttStreamSetup;
    req.ProfileToken = jstring2CStr(env,ProfileToken);

    ONVIF_SetAuthInfo(soap, userName, passWord);
    result = soap_call___trt__GetStreamUri(soap, deviceAddr, NULL, &req, rep);
    //    SOAP_CHECK_ERROR(result, soap, "GetStreamUri");

    jstring mediaUri;
    if (NULL != rep.MediaUri)
    {
        if (NULL != rep.MediaUri->Uri)
        {
            mediaUri = CStr2jstring(env,rep.MediaUri->Uri);
        }
        else {
            SOAP_DBGERR("Not enough cache!\n");
        }
    }
    //EXIT:
    if (NULL != soap) {
        ONVIF_soap_delete(soap);
    }

    return mediaUri;
}

/*
 * Class:     com_hirain_sdk_rtsp_JNIDeviceAccess
 * Method:    ONVIFGetHostName
 * Signature: (Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;)Ljava/lang/String;
 */
JNIEXPORT jstring JNICALL Java_com_hirain_sdk_rtsp_JNIDeviceAccess_ONVIFGetHostName
(JNIEnv *env, jobject jthis, jstring jdeviceAddr, jstring jusrName, jstring jpassword)
{
    const char* userName = jstring2CStr(env,jusrName);
    const char* passWord = jstring2CStr(env,jpassword);
    const char* deviceAddr = jstring2CStr(env,jdeviceAddr);

    int result;
    struct soap *soap = NULL;
    SOAP_ASSERT(NULL != (soap = ONVIF_soap_new(SOAP_SOCK_TIMEOUT)));

    _tds__GetHostname hostname;
    _tds__GetHostnameResponse hostnameRep;

    ONVIF_SetAuthInfo(soap, userName, passWord);
    result = soap_call___tds__GetHostname(soap, deviceAddr, NULL, &hostname, hostnameRep);
    //    SOAP_CHECK_ERROR(result, soap, "GetHostName");

    //printf("hostname->name:%s\n", hostnameRep.HostnameInformation->Name);
    jstring jHostnameName;
    if(NULL != hostnameRep.HostnameInformation)
    {
        // jfieldID jHostnameId = env->GetFieldID(jClass,"result","Ljava/lang/String;");
        jHostnameName = CStr2jstring(env,hostnameRep.HostnameInformation->Name);
        //env->SetObjectField(jObject,jHostnameId,jHostnameName);
    }
    //EXIT:
    if (NULL != soap) {
        ONVIF_soap_delete(soap);
    }
    return jHostnameName;
}
static int interrupt_cbk(void* opaque)
{
    int64_t* timeout = (int64_t*)opaque;
    if (av_gettime() >= *timeout) {
        return -1; // 非0即推出循环
    }
    return 0;
}void STDCALL device_status_proc(STATE& state, PAUSE& p, void* pUserData);
void device_status_proc2(int opaque);
JavaVM* g_java_vm = NULL;
/*
 * Class:     com_hirain_sdk_rtsp_JNIDeviceAccess
 * Method:    ONVIFGetIPCStatus
 * Signature: (Ljava/util/List;Ljava/lang/String;Ljava/lang/String;)Ljava/util/List;
 */
JNIEXPORT jobject JNICALL Java_com_hirain_sdk_rtsp_JNIDeviceAccess_ONVIFGetIPCStatus
(JNIEnv *env, jobject jthis, jobject jAddrList, jstring jusrName, jstring jpassword)
{
    printf("------------ GetIPCStatus start ------------\n");
    if (!jAddrList) {
        LOG("ERROR", "Input parameter error [DeviceList=%p]\n", jAddrList);
        return NULL;
    }
    if (NULL == g_java_vm) {
        env->GetJavaVM(&g_java_vm);		// 获取Java虚拟机
        if (NULL == g_java_vm) {
            LOG("ERROR", "GetJavaVM failed\n");
            return NULL;
        }
    }
    static int thread_count = 16;
    if (NULL == g_thd_pool) {
        g_thd_pool = new ThreadPool(thread_count);
    }
    g_thd_pool->start();
    std::this_thread::sleep_for(std::chrono::milliseconds(500));

    jobject arr_list_obj = NULL;
    do {
        jclass list_jcls = env->GetObjectClass(jAddrList);
        if (NULL == list_jcls) {
            LOG("ERROR", "GetObjectClass function failed.\n");
            break;
        }
        jmethodID list_get = env->GetMethodID(list_jcls, "get", "(I)Ljava/lang/Object;");
        if (NULL == list_get) {
            LOG("ERROR", "GetMethodID function failed.\n");
            break;
        }
        jfieldID jsize_id = env->GetFieldID(list_jcls, "size", "I");
        if (NULL == jsize_id) {
            LOG("ERROR", "GetFieldID function failed.\n");
            break;
        }
        jint size = env->GetIntField(jAddrList, jsize_id);
        if (size <= 0) {
            LOG("ERROR", "Device list is empty [size:%d]\n", size);
            break;
        }
        /////////////////////////////////////////////////////////////
        jclass arr_list_cls = env->FindClass("java/util/ArrayList");
        if (NULL == arr_list_cls) {
            LOG("ERROR", "FindClass function failed.\n");
            break;
        }
        jmethodID construct = env->GetMethodID(arr_list_cls, "<init>", "()V");
        if (NULL == construct) {
            LOG("ERROR", "GetMethodID function failed.\n");
            break;
        }
        arr_list_obj = env->NewObject(arr_list_cls, construct, "");
        if (NULL == arr_list_obj) {
            LOG("ERROR", "NewObject function failed.\n");
            break;
        }
        jmethodID arr_list_add = env->GetMethodID(arr_list_cls, "add", "(Ljava/lang/Object;)Z");
        if (NULL == arr_list_add) {
            LOG("ERROR", "GetMethodID function failed.\n");
            break;
        }
        /////////////////////////////////////////////////////////////////
        jclass cls_user = env->FindClass("com/hirain/video/domain/dto/operation/DeviceStatusDTO");
        if (NULL == cls_user) {
            LOG("ERROR", "FindClass function failed.\n");
            break;
        }
        jmethodID construct_user = env->GetMethodID(cls_user, "<init>", "()V");
        printf("\033[32m device list %d\033[0m\n\n", size);
        for (int i = 0; i < size; ++i) {
            jfieldID jfield_addr = env->GetFieldID(cls_user, "deviceAddress", "Ljava/lang/String;");
            jfieldID jfield_gid = env->GetFieldID(cls_user, "gid", "Ljava/lang/String;");
            jfieldID jfield_status = env->GetFieldID(cls_user, "onlineStatus", "I");
            if (!jfield_addr || !jfield_gid || !jfield_status) {
                LOG("ERROR", "addrID=%p, jgid=%p, jstatus=%p\n", jfield_addr, jfield_gid, jfield_status);
                break;
            }
            jobject item = env->CallObjectMethod(jAddrList, list_get, i);
            if (!item) {
                LOG("ERROR", "item=%p\n", item);
                break;
            }
            jstring jurl = (jstring)env->GetObjectField(item, jfield_addr);
            const char* url = jstring2CStr(env, jurl);
            if (!url) {
                LOG("ERROR", "url=%p\n", url);
                break;
            }
            jstring jgid = (jstring)env->GetObjectField(item, jfield_gid);
            const char* gid = jstring2CStr(env, jgid);
            if (!gid) {
                LOG("ERROR", "gid=%p\n", gid);
                break;
            }
            device_status_t dev;
            strcpy(dev.url, url);
            strcpy(dev.gid, gid);
            g_dev_map.add(dev.gid, &dev);
        }//for
        int count = g_dev_map.size();
        int n = (count > 16) ? 16 : 2;
        //if (NULL == g_thds)
        //    g_thds = new CThread11[16];
        //for (int i = 0; i < n; i++) {
        //    g_thds[i].Run(device_status_proc);
        //}
        //for (int i = 0; i < n; i++)
        //    g_thds[i].Join();
        for (int i = 0; i < n; i++) {
            g_thd_pool->appendTask(std::bind(device_status_proc2, i));
        }
    } while (false);

    printf("------------ GetIPCStatus end ------------\n");
    return arr_list_obj;
}

void device_status_proc2(int opaque)
{
    if (NULL == g_java_vm) {
        LOG("ERROR", "Java virtual machine is not found.\n");
        return;
    }
    JNIEnv* env = NULL;
    g_java_vm->AttachCurrentThread((void**)&env, NULL);
    if (NULL == env) {
        LOG("ERROR", "env=%p\n", env);
        return;
    }
    const char* cls_name = "com/hirain/server/service/operation/VideoChannelProviderImpl";
    jclass cls = env->FindClass(cls_name);
    if (NULL == cls) {
        LOG("ERROR", "FindClass function failed '%s'\n\n", cls_name);
        return;
    }
    jmethodID construct = env->GetMethodID(cls, "<init>", "()V");
    if (NULL == construct) {
        if (env->ExceptionOccurred()) // 捕获异常
            env->ExceptionDescribe();
        LOG("ERROR", "GetMethodID function failed.\n\n");
        return;
    }
    jobject obj = env->NewObject(cls, construct, "");
    if (NULL == obj) {
        if (env->ExceptionOccurred())
            env->ExceptionDescribe();
        LOG("ERROR", "NewObject function failed.\n\n");
        return;
    }
    jmethodID jupdate_status = env->GetMethodID(cls, "updateOnlineStatusByGid", "(Ljava/lang/String;I)V");
    if (NULL == jupdate_status) {
        if (env->ExceptionOccurred())
            env->ExceptionDescribe();
        LOG("ERROR", "------ batchInsert=null ------\n\n");
        return;
    }
    //jmethodID jupdate_status = env->GetStaticMethodID(cls, "updateOnlineStatusByGid", "(Ljava/lang/String;I)V");
    //if (NULL == cls) {
    //    LOG("ERROR", "GetStaticMethodID function failed.\n");
    //    return;
    //}
    printf("\033[32m ------ thread %u runing ... ------\033[0m\n\n", std::this_thread::get_id());
    device_status_t dev;
    while (true) {
        memset(&dev, 0, sizeof(dev));
        int nRet = g_dev_map.get(&dev);
        if (0 == nRet) {
            AVDictionary* opts = NULL;
            av_dict_set(&opts, "rtsp_transport", "tcp", 0);
            //av_dict_set(&opts, "stimeout", "3000000", 0);
            AVFormatContext* ifmt_ctx = avformat_alloc_context();
            ifmt_ctx->interrupt_callback.callback = interrupt_cbk;
            int64_t timeout = av_gettime() + 1000000; // 1秒
            ifmt_ctx->interrupt_callback.opaque = &timeout;
            int64_t test = av_gettime();

            int ret = avformat_open_input(&ifmt_ctx, dev.url, 0, &opts);
            printf("\033[32m[%s][%lld ms]\033[0m\t%s\n", (ret >= 0 ? "在线" : "离线"), (int64_t)((av_gettime() - test) / 1000.0), dev.url);
            if (ret >= 0) {
                dev.status = 1;
                avformat_close_input(&ifmt_ctx);
            }
            else LOG("ERROR", "avformat_open_input function failed [url:%s]\n", dev.url);
            av_dict_free(&opts);
            if (NULL != ifmt_ctx)
                avformat_free_context(ifmt_ctx);

            printf("\033[32m gid='%s', status=%d \033[0m\n\n", dev.gid, dev.status);
            jstring jgid = char_to_jstring(env, dev.gid);
            if (NULL == jgid) {
                LOG("ERROR", "jgid=%p\n", jgid);
                continue;
            }
            //env->CallStaticVoidMethod(cls, jupdate_status, jgid, dev.status);
            env->CallVoidMethod(obj, jupdate_status, jgid, dev.status);
        }
        else break;
    }
    g_java_vm->DetachCurrentThread();

    printf("\033[32m ------ thread %u stop ------\033[0m\n\n", std::this_thread::get_id());
}

void STDCALL device_status_proc(STATE& state, PAUSE& p, void* pUserData)
{
    if (NULL == g_java_vm) {
        LOG("ERROR", "Java virtual machine is not found.\n");
        return;
    }
    JNIEnv* env = NULL;
    g_java_vm->AttachCurrentThread((void**)&env, NULL);
    if (NULL == env) {
        LOG("ERROR", "env=%p\n", env);
        return;
    }
    const char* cls_name = "com/hirain/server/service/operation/VideoChannelProviderImpl";
    jclass cls = env->FindClass(cls_name);
    if (NULL == cls) {
        LOG("ERROR", "FindClass function failed '%s'\n\n", cls_name);
        return;
    }
    jmethodID construct = env->GetMethodID(cls, "<init>", "()V");
    if (NULL == construct) {
        if (env->ExceptionOccurred()) // 捕获异常
            env->ExceptionDescribe();
        LOG("ERROR", "GetMethodID function failed.\n\n");
        return;
    }
    jobject obj = env->NewObject(cls, construct, "");
    if (NULL == obj) {
        if (env->ExceptionOccurred())
            env->ExceptionDescribe();
        LOG("ERROR", "NewObject function failed.\n\n");
        return;
    }
    jmethodID jupdate_status = env->GetMethodID(cls, "updateOnlineStatusByGid", "(Ljava/lang/String;I)V");
    if (NULL == jupdate_status) {
        if (env->ExceptionOccurred())
            env->ExceptionDescribe();
        LOG("ERROR", "------ batchInsert=null ------\n\n");
        return;
    }
    //jmethodID jupdate_status = env->GetStaticMethodID(cls, "updateOnlineStatusByGid", "(Ljava/lang/String;I)V");
    //if (NULL == cls) {
    //    LOG("ERROR", "GetStaticMethodID function failed.\n");
    //    return;
    //}
    printf("\033[32m ------ thread %u runing ... ------\033[0m\n\n", std::this_thread::get_id());
    device_status_t dev;
    while (true) {
        memset(&dev, 0, sizeof(dev));
        int nRet = g_dev_map.get(&dev);
        if (0 == nRet) {
            AVDictionary* opts = NULL;
            av_dict_set(&opts, "rtsp_transport", "tcp", 0);
            //av_dict_set(&opts, "stimeout", "3000000", 0);
            AVFormatContext* ifmt_ctx = avformat_alloc_context();
            ifmt_ctx->interrupt_callback.callback = interrupt_cbk;
            int64_t timeout = av_gettime() + 1000000; // 1秒
            ifmt_ctx->interrupt_callback.opaque = &timeout;
            int64_t test = av_gettime();

            int ret = avformat_open_input(&ifmt_ctx, dev.url, 0, &opts);
            printf("\033[32m[%s][%lld ms]\033[0m\t%s\n", (ret >= 0 ? "在线" : "离线"), (int64_t)((av_gettime()-test)/1000.0), dev.url);
            if (ret >= 0) {
                dev.status = 1;
                avformat_close_input(&ifmt_ctx);
            }
            else LOG("ERROR", "avformat_open_input function failed [url:%s]\n", dev.url);
            av_dict_free(&opts);
            if (NULL != ifmt_ctx)
            avformat_free_context(ifmt_ctx);

            printf("\033[32m gid='%s', status=%d \033[0m\n\n", dev.gid, dev.status);
            jstring jgid = char_to_jstring(env, dev.gid);
            if (NULL == jgid) {
                LOG("ERROR","jgid=%p\n", jgid);
                continue;
            }
            //env->CallStaticVoidMethod(cls, jupdate_status, jgid, dev.status);
            env->CallVoidMethod(obj, jupdate_status, jgid, dev.status);
        }
        else break;
    }
    g_java_vm->DetachCurrentThread();

    printf("\033[32m ------ thread %u stop ------\033[0m\n\n", std::this_thread::get_id());
}
