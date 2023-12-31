/* DO NOT EDIT THIS FILE - it is machine generated */
#include <jni.h>
/* Header for class com_hirain_sdk_gb28181_JNIGB28181 */

#ifndef _Included_com_hirain_sdk_gb28181_JNIGB28181
#define _Included_com_hirain_sdk_gb28181_JNIGB28181
#ifdef __cplusplus
extern "C" {
#endif
/*
 * Class:     com_hirain_sdk_gb28181_JNIGB28181
 * Method:    uninit
 * Signature: ()V
 */
JNIEXPORT void JNICALL Java_com_hirain_sdk_gb28181_JNIGB28181_uninit
  (JNIEnv *, jobject);

/*
 * Class:     com_hirain_sdk_gb28181_JNIGB28181
 * Method:    open
 * Signature: (Lcom/hirain/sdk/entity/gb28181/LocalConfig;)I
 */
JNIEXPORT jint JNICALL Java_com_hirain_sdk_gb28181_JNIGB28181_open
  (JNIEnv *, jobject, jobject);

/*
 * Class:     com_hirain_sdk_gb28181_JNIGB28181
 * Method:    close
 * Signature: ()I
 */
JNIEXPORT jint JNICALL Java_com_hirain_sdk_gb28181_JNIGB28181_close
  (JNIEnv *, jobject);

/*
 * Class:     com_hirain_sdk_gb28181_JNIGB28181
 * Method:    query
 * Signature: (Lcom/hirain/sdk/entity/gb28181/Operation;)I
 */
JNIEXPORT jint JNICALL Java_com_hirain_sdk_gb28181_JNIGB28181_query
  (JNIEnv *, jobject, jobject);

/*
 * Class:     com_hirain_sdk_gb28181_JNIGB28181
 * Method:    control
 * Signature: (Lcom/hirain/sdk/entity/gb28181/Operation;)I
 */
JNIEXPORT jint JNICALL Java_com_hirain_sdk_gb28181_JNIGB28181_control
  (JNIEnv *, jobject, jobject);

/*
 * Class:     com_hirain_sdk_gb28181_JNIGB28181
 * Method:    play
 * Signature: (Lcom/hirain/sdk/entity/gb28181/Operation;)I
 */
JNIEXPORT jint JNICALL Java_com_hirain_sdk_gb28181_JNIGB28181_play
  (JNIEnv *, jobject, jobject);

/*
 * Class:     com_hirain_sdk_gb28181_JNIGB28181
 * Method:    stop
 * Signature: (Lcom/hirain/sdk/entity/gb28181/Operation;)I
 */
JNIEXPORT jint JNICALL Java_com_hirain_sdk_gb28181_JNIGB28181_stop
  (JNIEnv *, jobject, jobject);

/*
 * Class:     com_hirain_sdk_gb28181_JNIGB28181
 * Method:    test
 * Signature: (I)I
 */
JNIEXPORT jint JNICALL Java_com_hirain_sdk_gb28181_JNIGB28181_test
  (JNIEnv *, jobject, jint);

/*
 * Class:     com_hirain_sdk_gb28181_JNIGB28181
 * Method:    addPlatform
 * Signature: (Ljava/lang/Object;)I
 */
JNIEXPORT jint JNICALL Java_com_hirain_sdk_gb28181_JNIGB28181_addPlatform
  (JNIEnv *, jobject, jobject);

/*
 * Class:     com_hirain_sdk_gb28181_JNIGB28181
 * Method:    delPlatform
 * Signature: (Ljava/lang/String;)I
 */
JNIEXPORT jint JNICALL Java_com_hirain_sdk_gb28181_JNIGB28181_delPlatform
  (JNIEnv *, jobject, jstring);

/*
 * Class:     com_hirain_sdk_gb28181_JNIGB28181
 * Method:    updatePlatform
 * Signature: (Ljava/lang/Object;)I
 */
JNIEXPORT jint JNICALL Java_com_hirain_sdk_gb28181_JNIGB28181_updatePlatform
  (JNIEnv *, jobject, jobject);

/*
 * Class:     com_hirain_sdk_gb28181_JNIGB28181
 * Method:    addDevice
 * Signature: (Ljava/util/List;)I
 */
JNIEXPORT jint JNICALL Java_com_hirain_sdk_gb28181_JNIGB28181_addDevice
  (JNIEnv *, jobject, jobject);

#ifdef __cplusplus
}
#endif
#endif
