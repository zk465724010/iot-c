#ifndef ONVIFSDK_WIN64_H_
#define ONVIFSDK_WIN64_H_

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <string>
#include <iostream>
#include <string.h>

//onvif header
#include "soapH.h"
#include "soapStub.h"
#include "wsseapi.h"
#include "wsaapi.h"

#include "onvifSDK_structDefine.h"
#include "jni.h"
#define CHARSIZE 128

#ifdef __cplusplus
extern "C" {
#endif

typedef struct AudioVideoConfiguration
{
	char streamType[CHARSIZE];						//"码流类型（主码流、子码流、第三码流）"
	char videoType[CHARSIZE];						//"视频类型（视频流、复合流）"
	int width;										// "分辨率宽度（1280*720、1920*1080、2560*1440）"
	int height;										// "分辨率高度
	char bitType[CHARSIZE];							//"码率类型（定码率、变码率）"
	float imageQuality;								//"图像质量（最低、较低、低、中、较高、最高） 0~5或1~6
	int FrameRateLimit;								//视频帧率
	int BitrateLimit;								//码率上限
	char videoEncode[CHARSIZE];						//视频编码(H.264、 H.265)
	char encodeComplex[CHARSIZE];					//编码复杂度（低、中、高）
	int GovLength;									//"I帧间隔"
	char audioEncode[CHARSIZE];						//音频编码（G.722.1、G.711ulaw、G.711alaw、MP2L2、G.726、AAC、PCM）"
}AUDIOVIDEOCONF;

enum PTZCMD
{
    PTZ_CMD_LEFT=1,
    PTZ_CMD_RIGHT,
    PTZ_CMD_UP,
    PTZ_CMD_DOWN,
    PTZ_CMD_LEFTUP,
    PTZ_CMD_LEFTDOWN,
    PTZ_CMD_RIGHTUP,
    PTZ_CMD_RIGHTDOWN,
    PTZ_CMD_ZOOM_IN,
    PTZ_CMD_ZOOM_OUT,
    PTZ_CMD_STOP
};

typedef struct device_status {
    char url[256] = { 0 };    // 设备rtsp地址
    char gid[32] = { 0 };     //
    int status = 0;         // 设备状态(0:离线 1:在线)
}device_status_t;

int UDPsendMessage(sockaddr_in multi_addr, SOAP_SOCKET s, const char *probe);
int get_local_ip_using_create_socket(char *str_ip);
int get_netway_ip_using_res(char *str_ip);

void splitChar(char *buff, int index, char *backBuff);
void removeChar(const char *buffer, char *GetBuff);

char* jstring2CStr( JNIEnv *env, jstring jstr );
jstring CStr2jstring( JNIEnv *env, const char* str );
int jstringCompare(JNIEnv *env, jstring String1, const char* String2);
jstring char_to_jstring(JNIEnv* env, const char* str);


const char *dump_enum2str_VideoEncoding(enum tt__VideoEncoding e);
const char *dump_enum2str_AudioEncoding(enum tt__AudioEncoding e);

/**************************************************************************************************/
void soap_perror(struct soap *soap, const char *str);
void* ONVIF_soap_malloc(struct soap *soap, unsigned int n);
struct soap *ONVIF_soap_new(int timeout);

void ONVIF_soap_delete(struct soap *soap);
int ONVIF_SetAuthInfo(struct soap *soap, const char *username, const char *password);
void ONVIF_init_header(struct soap *soap);

void ONVIF_init_ProbeType(struct soap *soap, struct wsdd__ProbeType *probe);
void soap_perror(struct soap *soap, const char *str);
int ONVIF_SetAuthInfo(struct soap *soap, const char *username, const char *password);

int ONVIF_GetCapabilities(const char *DeviceXAddr, struct tagCapabilities *capa,const char *username, const char *password);
int make_uri_withauth(char *src_uri, char *username, char *password, char *dest_uri, unsigned int size_dest_uri);
int ONVIF_GetProfiles(const char *MediaXAddr, struct tagProfile **profiles, const char *username, const char *password);

int ONVIF_RelativeMove(const char* mediaAddr,const char*profileToken, const char* usrName,const char* passWord,int type);
int ONVIF_ContinuousMove(const char* mediaAddr, const char*profileToken, const char* usrName, const char* passWord, int cmd,float speed);
int ONVIF_PTZStopMove(const char* ptzXAddr, const char* ProfileToken);

int ONVIF_GetVideoEncoderConfigurationOptions(const char *MediaXAddr, char *ConfigurationToken, const char* usrName, const char* passWord);
int ONVIF_GetVideoEncoderConfiguration(const char *MediaXAddr, char *ConfigurationToken, const char* usrName, const char* passWord);
int ONVIF_SetVideoEncoderConfiguration(const char *MediaXAddr, struct tagVideoEncoderConfiguration *venc, struct AudioVideoConfiguration &conf,const char* usrName, const char* passWord);

int ONVIF_GetAudioEncoderConfigurationOptions(const char *DeviceXAddr, const char* token, const char* usrName, const char* passWord);
int ONVIF_GetAudioEncoderConfiguration(const char *DeviceXAddr, const char* token, const char* usrName, const char* passWord);
int ONVIF_SetAudioEncoderConfiguration(const char *DeviceXAddr, const char *token, struct AudioVideoConfiguration &conf, const char* usrName, const char* passWord);
int ONVIF_GetPictureConfiguretion(const char *ImgXAddr,const char* videoSourceToken,struct imgConfigure* imgConf, const char* usrName, const char* passWord);
int ONVIF_SetPictureConfiguretion(const char *ImgXAddr,const char* videoSourceToken,struct imgConfigure* imgConf, const char* usrName, const char* passWord);
enum PTZCMD getPTZTYPE(int type);
#ifdef __cplusplus
}
#endif

#endif // __ONVIFSDK_WIN64_H__
