#ifndef ONVIFSDK_STRUCTDEFINE_H_
#define ONVIFSDK_STRUCTDEFINE_H_

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <string>
#include <iostream>

#define SOAP_ASSERT     assert
#define SOAP_DBGLOG     printf
#define SOAP_DBGERR     printf

#define SOAP_TO         "urn:schemas-xmlsoap-org:ws:2005:04:discovery"
#define SOAP_ACTION     "http://schemas.xmlsoap.org/ws/2005/04/discovery/Probe"

#define SOAP_MCAST_ADDR "soap.udp://239.255.255.250:3702"                       // onvif规定的组播地址

#define SOAP_ITEM       ""                                                      // 寻找的设备范围
//#define SOAP_ITEM       "onvif://www.onvif.org"
#define SOAP_TYPES      "dn:NetworkVideoTransmittertds"                            // 寻找的设备类型

#define SOAP_SOCK_TIMEOUT    (10)               // socket超时时间（单秒秒）

#define USERNAME    "Skwl@2020"
#define PASSWORD    "Skwl@2021"

#define ONVIF_ADDRESS_SIZE   (1024)                                              // URI地址长度
#define ONVIF_TOKEN_SIZE     (65)                                               // token长度

/******************设备发现******************************************************/
/* 从技术层面来说，通过单播、多播、广播三种方式都能探测到IPC，但多播最具实用性*/
#define COMM_TYPE_UNICAST         1                                             // 单播
#define COMM_TYPE_MULTICAST       2                                             // 多播
#define COMM_TYPE_BROADCAST       3                                             // 广播
#define COMM_TYPE                 COMM_TYPE_MULTICAST

/* 发送探测消息（Probe）的目标地址、端口号 */
#if COMM_TYPE == COMM_TYPE_UNICAST
#define CAST_ADDR "100.100.100.15"                                          // 单播地址，预先知道的IPC地址
#elif COMM_TYPE == COMM_TYPE_MULTICAST
#define CAST_ADDR "239.255.255.250"                                         // 多播地址，固定的239.255.255.250
#elif COMM_TYPE == COMM_TYPE_BROADCAST
#define CAST_ADDR "100.100.100.255"                                         // 广播地址
#endif

#define LOCATE_IP  "192.168.42.152"
#define CAST_PORT 3702                                                          // 端口号

#if 0
/* 以下几个宏是为了socket编程能够跨平台，这几个宏是从gsoap中拷贝来的 */
#ifndef SOAP_SOCKET
# ifdef WIN32
#  define SOAP_SOCKET SOCKET
#  define soap_closesocket(n) closesocket(n)
# else
#  define SOAP_SOCKET int
#  define soap_closesocket(n) close(n)
# endif
#endif

#if defined(_AIX) || defined(AIX)
# if defined(_AIX43)
#  define SOAP_SOCKLEN_T socklen_t
# else
#  define SOAP_SOCKLEN_T int
# endif
#elif defined(SOCKLEN_T)
# define SOAP_SOCKLEN_T SOCKLEN_T
#elif defined(__socklen_t_defined) || defined(_SOCKLEN_T) || defined(CYGWIN) || defined(FREEBSD) || defined(__FreeBSD__) || defined(OPENBSD) || defined(__QNX__) || defined(QNX) || defined(OS390) || defined(__ANDROID__) || defined(_XOPEN_SOURCE)
# define SOAP_SOCKLEN_T socklen_t
#elif defined(IRIX) || defined(WIN32) || defined(__APPLE__) || defined(SUN_OS) || defined(OPENSERVER) || defined(TRU64) || defined(VXWORKS) || defined(HP_UX)
# define SOAP_SOCKLEN_T int
#elif !defined(SOAP_SOCKLEN_T)
# define SOAP_SOCKLEN_T size_t
#endif
#endif

#ifdef WIN32
#define SLEEP(n)    Sleep(1000 * (n))
#else
#define SLEEP(n)    sleep((n))
#endif

/*************************************************************************************/
/* 视频编码器配置信息 */
struct tagVideoEncoderConfiguration
{
    char token[ONVIF_TOKEN_SIZE];                                               // 唯一标识该视频编码器的令牌字符串
    int Width;                                                                  // 分辨率
    int Height;
};

/* 设备配置信息 */
struct tagProfile {
    char Name[ONVIF_TOKEN_SIZE];
    char token[ONVIF_TOKEN_SIZE];                                               // 唯一标识设备配置文件的令牌字符串
    char PTZtoken[ONVIF_TOKEN_SIZE];                                            //PTZ token
    char Audiotoken[ONVIF_TOKEN_SIZE];                                          //Audio token
    struct tagVideoEncoderConfiguration venc;// 视频编码器配置信息
    char videoSourceToken[ONVIF_TOKEN_SIZE];//video source token for img setting use
};
/* 设备能力信息 */
struct tagCapabilities {
    char MediaXAddr[ONVIF_ADDRESS_SIZE];// 媒体服务地址
    char PTZXAddr[ONVIF_ADDRESS_SIZE];
    char EventXAddr[ONVIF_ADDRESS_SIZE];// 事件服务地址
    char IMGXAddr[ONVIF_ADDRESS_SIZE];// image set address
};

// image seting
struct imgConfigure{
    float   brightness;
    float   contrast;
    float   colorsaturation;
    float brightnessMin;
    float brightnessMax;
    float contrastMin;
    float contrastMax;
    float colorsaturationMin;
    float colorsaturationMax;
};

enum PTZControl
{
    UP = 0,
    DOWN,
    LEFT,
    RIGHT,
    RESTART,
    ZOOMIN,
    ZOOMOUT
};

#define SOAP_CHECK_ERROR(result, soap, str) \
    do { \
        if (SOAP_OK != (result) || SOAP_OK != (soap)->error) { \
            soap_perror((soap), (str)); \
            if (SOAP_OK == (result)) { \
                (result) = (soap)->error; \
            } \
            goto EXIT; \
        } \
} while (0)

#endif // ONVIFSDK_STRUCTDEFINE_H_
