/* DO NOT EDIT THIS FILE - it is machine generated */
#include <jni.h>
/* Header for class com_hirain_sdk_rtsp_JNIDeviceAccess */

#ifndef _Included_com_hirain_sdk_rtsp_JNIDeviceAccess
#define _Included_com_hirain_sdk_rtsp_JNIDeviceAccess
#ifdef __cplusplus
extern "C" {
#endif
/*
 * Class:     com_hirain_sdk_rtsp_JNIDeviceAccess
 * Method:    ONVIFDetectDevice
 * Signature: ()Ljava/util/List;
 */
JNIEXPORT jobject JNICALL Java_com_hirain_sdk_rtsp_JNIDeviceAccess_ONVIFDetectDevice
  (JNIEnv *, jobject);

/*
 * Class:     com_hirain_sdk_rtsp_JNIDeviceAccess
 * Method:    ONVIFNewDetectDevice
 * Signature: ()Ljava/util/List;
 */
JNIEXPORT jobject JNICALL Java_com_hirain_sdk_rtsp_JNIDeviceAccess_ONVIFNewDetectDevice
  (JNIEnv *, jobject);

/*
 * Class:     com_hirain_sdk_rtsp_JNIDeviceAccess
 * Method:    ONVIFGetDeviceInformation
 * Signature: (Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;)Lcom/hirain/sdk/entity/rtsp/DeviceInformationResponse;
 */
JNIEXPORT jobject JNICALL Java_com_hirain_sdk_rtsp_JNIDeviceAccess_ONVIFGetDeviceInformation
  (JNIEnv *, jobject, jstring, jstring, jstring);

/*
 * Class:     com_hirain_sdk_rtsp_JNIDeviceAccess
 * Method:    ONVIFGetCapabilities
 * Signature: (Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;)Lcom/hirain/sdk/entity/rtsp/DeviceCapabilities;
 */
JNIEXPORT jobject JNICALL Java_com_hirain_sdk_rtsp_JNIDeviceAccess_ONVIFGetCapabilities
  (JNIEnv *, jobject, jstring, jstring, jstring);

/*
 * Class:     com_hirain_sdk_rtsp_JNIDeviceAccess
 * Method:    ONVIFGetNetworkProtocols
 * Signature: (Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;)Lcom/hirain/sdk/entity/rtsp/TdsSetNetworkProtocols;
 */
JNIEXPORT jobject JNICALL Java_com_hirain_sdk_rtsp_JNIDeviceAccess_ONVIFGetNetworkProtocols
  (JNIEnv *, jobject, jstring, jstring, jstring);

/*
 * Class:     com_hirain_sdk_rtsp_JNIDeviceAccess
 * Method:    ONVIFGetNetworkDefaultGateway
 * Signature: (Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;)Lcom/hirain/sdk/entity/rtsp/TdsSetNetworkDefaultGateway;
 */
JNIEXPORT jobject JNICALL Java_com_hirain_sdk_rtsp_JNIDeviceAccess_ONVIFGetNetworkDefaultGateway
  (JNIEnv *, jobject, jstring, jstring, jstring);

/*
 * Class:     com_hirain_sdk_rtsp_JNIDeviceAccess
 * Method:    ONVIFGetNetworkInterface
 * Signature: (Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;)Lcom/hirain/sdk/entity/rtsp/TdsSetNetworkDefaultGateway;
 */
JNIEXPORT jobject JNICALL Java_com_hirain_sdk_rtsp_JNIDeviceAccess_ONVIFGetNetworkInterface
  (JNIEnv *, jobject, jstring, jstring, jstring);

/*
 * Class:     com_hirain_sdk_rtsp_JNIDeviceAccess
 * Method:    ONVIFGetProfiles
 * Signature: (Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;)Ljava/util/List;
 */
JNIEXPORT jobject JNICALL Java_com_hirain_sdk_rtsp_JNIDeviceAccess_ONVIFGetProfiles
  (JNIEnv *, jobject, jstring, jstring, jstring);

/*
 * Class:     com_hirain_sdk_rtsp_JNIDeviceAccess
 * Method:    ONVIFGetStreamUri
 * Signature: (Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;)Ljava/lang/String;
 */
JNIEXPORT jstring JNICALL Java_com_hirain_sdk_rtsp_JNIDeviceAccess_ONVIFGetStreamUri
  (JNIEnv *, jobject, jstring, jstring, jstring, jstring);

/*
 * Class:     com_hirain_sdk_rtsp_JNIDeviceAccess
 * Method:    ONVIFGetHostName
 * Signature: (Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;)Ljava/lang/String;
 */
JNIEXPORT jstring JNICALL Java_com_hirain_sdk_rtsp_JNIDeviceAccess_ONVIFGetHostName
  (JNIEnv *, jobject, jstring, jstring, jstring);

/*
 * Class:     com_hirain_sdk_rtsp_JNIDeviceAccess
 * Method:    ONVIFGetIPCStatus
 * Signature: (Ljava/util/List;Ljava/lang/String;Ljava/lang/String;)Ljava/util/List;
 */
JNIEXPORT jobject JNICALL Java_com_hirain_sdk_rtsp_JNIDeviceAccess_ONVIFGetIPCStatus
  (JNIEnv *, jobject, jobject, jstring, jstring);

#ifdef __cplusplus
}
#endif
#endif