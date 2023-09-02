/* DO NOT EDIT THIS FILE - it is machine generated */
#include <jni.h>
/* Header for class com_hirain_sdk_rtsp_JNIAudioVideoConfiguration */

#ifndef _Included_com_hirain_sdk_rtsp_JNIAudioVideoConfiguration
#define _Included_com_hirain_sdk_rtsp_JNIAudioVideoConfiguration
#ifdef __cplusplus
extern "C" {
#endif
/*
 * Class:     com_hirain_sdk_rtsp_JNIAudioVideoConfiguration
 * Method:    ONVIFGetVideoEncoderConfiguration
 * Signature: (Ljava/lang/String;ILjava/lang/String;Ljava/lang/String;)Lcom/hirain/sdk/entity/rtsp/AudioVideoConfiguration;
 */
JNIEXPORT jobject JNICALL Java_com_hirain_sdk_rtsp_JNIAudioVideoConfiguration_ONVIFGetVideoEncoderConfiguration
  (JNIEnv *, jobject, jstring, jint, jstring, jstring);

/*
 * Class:     com_hirain_sdk_rtsp_JNIAudioVideoConfiguration
 * Method:    ONVIFSetVideoEncoderConfiguration
 * Signature: (Lcom/hirain/sdk/entity/rtsp/AudioVideoConfiguration;Ljava/util/List;)Ljava/util/List;
 */
JNIEXPORT jobject JNICALL Java_com_hirain_sdk_rtsp_JNIAudioVideoConfiguration_ONVIFSetVideoEncoderConfiguration
  (JNIEnv *, jobject, jobject, jobject);

/*
 * Class:     com_hirain_sdk_rtsp_JNIAudioVideoConfiguration
 * Method:    ONVIFSetVideoAndAudioEncoderConfigure
 * Signature: (Lcom/hirain/sdk/entity/rtsp/AudioVideoConfiguration;Ljava/lang/String;Ljava/lang/String;)I
 */
JNIEXPORT jint JNICALL Java_com_hirain_sdk_rtsp_JNIAudioVideoConfiguration_ONVIFSetVideoAndAudioEncoderConfigure
  (JNIEnv *, jobject, jobject, jstring, jstring);

/*
 * Class:     com_hirain_sdk_rtsp_JNIAudioVideoConfiguration
 * Method:    ONVIFGetVideoChannelParam
 * Signature: (Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;)Ljava/lang/Object;
 */
JNIEXPORT jobject JNICALL Java_com_hirain_sdk_rtsp_JNIAudioVideoConfiguration_ONVIFGetVideoChannelParam
  (JNIEnv *, jobject, jstring, jstring, jstring);

/*
 * Class:     com_hirain_sdk_rtsp_JNIAudioVideoConfiguration
 * Method:    ONVIFGetDeviceDefaultConfigure
 * Signature: (Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;I)Lcom/hirain/sdk/entity/rtsp/DeviceDefaultConfigure;
 */
JNIEXPORT jobject JNICALL Java_com_hirain_sdk_rtsp_JNIAudioVideoConfiguration_ONVIFGetDeviceDefaultConfigure
  (JNIEnv *, jobject, jstring, jstring, jstring, jint);

/*
 * Class:     com_hirain_sdk_rtsp_JNIAudioVideoConfiguration
 * Method:    ONVFISetVideoChannelParam
 * Signature: (Ljava/lang/Object;Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;)I
 */
JNIEXPORT jint JNICALL Java_com_hirain_sdk_rtsp_JNIAudioVideoConfiguration_ONVFISetVideoChannelParam
  (JNIEnv *, jobject, jobject, jstring, jstring, jstring);

/*
 * Class:     com_hirain_sdk_rtsp_JNIAudioVideoConfiguration
 * Method:    ONVIFSetWaterMark
 * Signature: (Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;)I
 */
JNIEXPORT jint JNICALL Java_com_hirain_sdk_rtsp_JNIAudioVideoConfiguration_ONVIFSetWaterMark
  (JNIEnv *, jobject, jstring, jstring, jstring, jstring);

/*
 * Class:     com_hirain_sdk_rtsp_JNIAudioVideoConfiguration
 * Method:    ONVIFBatchSetWaterMark
 * Signature: (Ljava/util/List;)Ljava/util/List;
 */
JNIEXPORT jobject JNICALL Java_com_hirain_sdk_rtsp_JNIAudioVideoConfiguration_ONVIFBatchSetWaterMark
  (JNIEnv *, jobject, jobject);

#ifdef __cplusplus
}
#endif
#endif