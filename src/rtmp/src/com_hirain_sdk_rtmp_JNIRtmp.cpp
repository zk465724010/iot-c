
#include "com_hirain_sdk_rtmp_JNIRtmp.h"
#include "log.h"
#include "common_jni.h"
#include "rtmp_media.h"
#if (defined (__MY_LOG_H__) && defined (_WIN32))
#include <Windows.h>		// 日志模块
#endif

extern CRtmpMedia g_rtmp_media;

/*
 * Class:     com_hirain_sdk_rtmp_JNIRtmp
 * Method:    createChannel
 * Signature: (Lcom/hirain/sdk/JNIEntity/RtmpChannelParam;)I
 */
JNIEXPORT jint JNICALL Java_com_hirain_sdk_rtmp_JNIRtmp_createChannel(JNIEnv *env, jobject obj, jobject param)
{
	LOG("INFO", "------ create channel start ------\n");
	int nRet = -1;
	do {
		if ((NULL == env) || (NULL == obj) || (NULL == param)) {
			LOG("ERROR", "Input parameter error (env:%p obj:%p param:%p)\n", env, obj, param);
			break;
		}
		jclass cls = env->GetObjectClass(param);
		if (NULL == cls) {
			LOG("ERROR", "GetObjectClass function failed\n");
			break;
		}

		jfieldID jtype = env->GetFieldID(cls, "type", "I");
		if (NULL == jtype) {
			LOG("ERROR", "GetFieldID function failed 'type'\n");
			break;
		}
		int chl_type = env->GetIntField(param, jtype);
		if (1 == chl_type) {
		}
		else if (2 == chl_type) {
		}
		else {
			LOG("ERROR", "Unknown channel type %d\n", chl_type);
			//break;
		}

		// 流的ID
		jfieldID jchl_id = env->GetFieldID(cls, "channelId", "Ljava/lang/String;");
		if (NULL == jchl_id) {
			LOG("ERROR", "GetFieldID function failed 'channelId'\n");
			break;
		}
		stream_param_t streamparam;
		jstring_to_char(env, param, jchl_id, streamparam.id, sizeof(streamparam.id), "channelId");

		// 取流地址
		jfieldID jinput = env->GetFieldID(cls, "takeUrl", "Ljava/lang/String;");
		if (NULL == jinput) {
			LOG("ERROR", "GetFieldID function failed 'takeUrl'\n");
			break;
		}
		jstring_to_char(env, param, jinput, streamparam.input, sizeof(streamparam.input), "takeUrl");
		
		// 推流地址
		jfieldID jrtmp = env->GetFieldID(cls, "pushRtmpUrl", "Ljava/lang/String;");
		if (NULL == jrtmp) {
			LOG("ERROR", "GetFieldID function failed 'pushRtmpUrl'\n");
			break;
		}
		jstring_to_char(env, param, jrtmp, streamparam.output, sizeof(streamparam.output), "pushRtmpUrl");
		LOG("INFO", "chl_id:%s type:%d input:'%s' output:'%s'\n", streamparam.id, chl_type, streamparam.input, streamparam.output);

		// 创建通道
		nRet = g_rtmp_media.create_channel(streamparam.id, &streamparam);
	} while (false);

	LOG("INFO", "------ create channel end ------\n");
	return nRet;
}

/*
 * Class:     com_hirain_sdk_rtmp_JNIRtmp
 * Method:    destoryChannel
 * Signature: (Ljava/lang/String;)I
 */
JNIEXPORT jint JNICALL Java_com_hirain_sdk_rtmp_JNIRtmp_destoryChannel(JNIEnv *env, jobject obj, jstring jid)
{
	LOG("INFO", "------ destory channel start ------\n");
	int nRet = -1;
	do {
		if ((NULL == env) || (NULL == obj) || (NULL == jid)) {
			LOG("ERROR", "Input parameter error (env:%p obj:%p id:%p)\n", env, obj, jid);
			break;
		}
		char chl_id[64] = { 0 };
		nRet = jstring_to_char(env, jid, chl_id, sizeof(chl_id), "chl_id");
		if (0 != nRet) {
			break;
		}
		nRet = g_rtmp_media.destroy_channel(chl_id);
	} while (false);

	LOG("INFO", "------ destory channel end ------\n");
	return nRet;
}
