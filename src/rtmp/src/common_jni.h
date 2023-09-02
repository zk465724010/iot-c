
#ifndef __COMMON_JNI_H__
#define __COMMON_JNI_H__

#include <jni.h>
#include <string>

/*
// Java����   ����
// boolean     Z
// byte       B
// char       C
// short    S
// int    I
// long    L
// float    F
// double    D
// void    V
*/
jstring char_to_jstring(JNIEnv* env, const char* str);

int jstring_to_char(JNIEnv* env, jobject obj, jfieldID field_id, char *buff, int size, const char *describe);
int jstring_to_char(JNIEnv* env, jstring jstr, char *buff, int size, const char *describe = NULL);

int jint_to_int(JNIEnv* env, jobject obj, jfieldID field_id, const char *describe = NULL);

long jlong_to_long(JNIEnv* env, jobject obj, jfieldID field_id, const char *describe = NULL);

jobject get_object_field(JNIEnv *env, jobject obj, jfieldID field_id, const char *describe = NULL);

int set_object_field(JNIEnv *env, jobject obj, jfieldID field_id, jobject value, const char *describe = NULL);

int set_jstring_field(JNIEnv *env, jobject obj, jfieldID field_id, const char *value, const char *describe = NULL);

int set_int_field(JNIEnv *env, jobject obj, jfieldID field_id, jint value, const char *describe = NULL);

#endif