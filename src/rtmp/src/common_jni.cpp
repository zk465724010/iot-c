
#include "common_jni.h"
#include <jni.h>
#include <string>
#include "log.h"
#if (defined (__MY_LOG_H__) && defined (_WIN32))
#include <Windows.h>		// 日志模块
#endif

jstring char_to_jstring(JNIEnv* env, const char* str)
{
	jstring jstr = NULL;
	do {
		if ((NULL == env) || (NULL == str)) {
			LOG("ERROR", "Input parameter error (env:%p str:%p)\n", env, str);
			break;
		}
		int str_len = strlen(str);
		// 分配一个字节数组
		jbyteArray bytes = env->NewByteArray(str_len);
		if (NULL == bytes) {
			LOG("ERROR", "NewByteArray function faild (str_len:%d)\n", str_len);
			break;
		}
		// 数据拷贝
		env->SetByteArrayRegion(bytes, 0, str_len, (jbyte*)str);
		
		jstring encoding = env->NewStringUTF("utf-8");
		if (NULL == encoding) {
			LOG("ERROR", "NewStringUTF function failed.\n");
			break;
		}

		// 查找JNI中String对象的类
		jclass cls = env->FindClass("Ljava/lang/String;");
		if (NULL == cls) {
			LOG("ERROR", "FindClass function (cls:%p)\n", cls);
			break;
		}
		// 获取String类型的构造方法ID
		jmethodID construct = env->GetMethodID(cls, "<init>", "([BLjava/lang/String;)V");
		if (NULL == construct) {
			LOG("ERROR", "Failed to get the constructor ID.\n");
			break;
		}
		// new一个String对象并初始化值
		jstr = (jstring)env->NewObject(cls, construct, bytes, encoding);
		if (NULL == jstr)
			LOG("ERROR", "NewObject function failed.\n");
	} while (false);
	return jstr;
}

int jstring_to_char(JNIEnv* env, jobject obj, jfieldID field_id, char *buff, int size, const char *describe)
{
	int nRet = -1;
	if ((NULL != env) && (NULL != obj) && (NULL != field_id) && (NULL != buff) && (size > 0)) {
		jstring jstr = (jstring)env->GetObjectField(obj, field_id);
		if (NULL != jstr) {
			const char *str = env->GetStringUTFChars(jstr, false);
			if (NULL != str) {
				strncpy(buff, str, size-1);
				env->ReleaseStringUTFChars(jstr, str);
				nRet = 0;
			}
			else LOG("ERROR","GetStringUTFChars failed '%s'\n\n", describe);
		}
		else LOG("WARN","GetObjectField failed '%s'\n\n", describe);
	}
	else LOG("ERROR","env=%p,obj=%p,field_id=%p,buff=%p,size=%d,describe=%s\n\n", env, obj, field_id, buff, size, describe);
	return nRet;
}

int jstring_to_char(JNIEnv* env, jstring jstr, char *buff, int size, const char *describe/*=NULL*/)
{
	int nRet = -1;
	if ((NULL != env) && (NULL != jstr) && (NULL != buff) && (size > 0)) {
		const char *str = env->GetStringUTFChars(jstr, false);
		if (NULL != str) {
			strncpy(buff, str, size - 1);
			env->ReleaseStringUTFChars(jstr, str);
			nRet = 0;
		}
		else LOG("ERROR","GetStringUTFChars failed '%s'\n\n", (NULL!=describe)? describe:"");
	}
	else LOG("ERROR","env=%p,jstr=%p,,buff=%p,size=%d,describe=%s\n\n", env, jstr, buff, size, (NULL != describe) ? describe : "");
	return nRet;
}
int jint_to_int(JNIEnv* env, jobject obj, jfieldID field_id, const char *describe/*=NULL*/)
{
	if ((NULL != env) && (NULL != obj) && (NULL != field_id) && (NULL != describe)) {
		return env->GetIntField(obj, field_id);
	}
	else LOG("ERROR","env=%p,obj=%p,field_id=%p,describe=%s\n\n", env, obj, field_id, (NULL != describe) ? describe : "null");
	return -1;
}

long jlong_to_long(JNIEnv* env, jobject obj, jfieldID field_id, const char *describe/*=NULL*/)
{
	if ((NULL != env) && (NULL != obj) && (NULL != field_id) && (NULL != describe)) {
		return env->GetLongField(obj, field_id);
	}
	else LOG("ERROR","env=%p,obj=%p,field_id=%p,describe=%s\n\n", env, obj, field_id, (NULL != describe) ? describe : "null");
	return -1;
}

jobject get_object_field(JNIEnv *env, jobject obj, jfieldID field_id, const char *describe/*=NULL*/)
{
	if ((NULL != env) && (NULL != obj) && (NULL != field_id)) {
		return env->GetObjectField(obj, field_id);
	}
	else LOG("ERROR","env=%p,obj=%p,field_id=%p,value=%p,describe=%s\n\n", env, obj, field_id, (NULL != describe) ? describe : "null");
	return NULL;
}

int set_object_field(JNIEnv *env, jobject obj, jfieldID field_id, jobject value, const char *describe/*=NULL*/)
{
	int nRet = -1;
	if ((NULL != env) && (NULL != obj) && (NULL != field_id) && (NULL != value)) {
		env->SetObjectField(obj, field_id, value);
		nRet = 0;
	}
	else LOG("ERROR","env=%p,obj=%p,field_id=%p,value=%p,describe=%s\n\n", env, obj, field_id, value, (NULL != describe) ? describe : "null");
	return nRet;
}

int set_jstring_field(JNIEnv *env, jobject obj, jfieldID field_id, const char *value, const char *describe/*=NULL*/)
{
	int nRet = -1;
	if ((NULL != env) && (NULL != obj) && (NULL != field_id) && (NULL != value)) {
		jstring jstr = char_to_jstring(env, value);
		if (NULL != jstr) {
			env->SetObjectField(obj, field_id, jstr);
			nRet = 0;
		}
		else LOG("ERROR","char_to_jstring failed ('%s'),describe=%s\n\n", value, (NULL != describe) ? describe : "null");
	}
	else LOG("ERROR","env=%p,obj=%p,field_id=%p,value=%p,describe=%s\n\n", env, obj, field_id, value, (NULL != describe) ? describe : "null");
	return nRet;
}

int set_int_field(JNIEnv *env, jobject obj, jfieldID field_id, jint value, const char *describe/*=NULL*/)
{
	int nRet = -1;
	if ((NULL != env) && (NULL != obj) && (NULL != field_id)) {
		env->SetIntField(obj, field_id, value);
		nRet = 0;
	}
	else LOG("ERROR","env=%p,obj=%p,field_id=%p,describe=%s\n\n", env, obj, field_id, (NULL != describe) ? describe : "null");
	return nRet;
}
