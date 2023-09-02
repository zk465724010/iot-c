
#ifndef __COMMON_JNI_H__
#define __COMMON_JNI_H__

#include <jni.h>
#include <string>
#include "log.h"

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
							LOG("ERROR","NewObject error\n\n");
						return jstr;
					}
					else LOG("ERROR","NewStringUTF error\n\n");
				}
				else LOG("ERROR","NewByteArray error\n\n");
			}
			else LOG("ERROR","GetMethodID error\n\n");
		}
		else LOG("ERROR","FindClass error\n\n");
	}
	else LOG("ERROR","env=%p, str=%p\n\n", env, str);
	return NULL;
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

int jstring_to_char(JNIEnv* env, jstring jstr, char *buff, int size, const char *describe=NULL)
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
int jint_to_int(JNIEnv* env, jobject obj, jfieldID field_id, const char *describe = NULL)
{
	if ((NULL != env) && (NULL != obj) && (NULL != field_id) && (NULL != describe)) {
		return env->GetIntField(obj, field_id);
	}
	else LOG("ERROR","env=%p,obj=%p,field_id=%p,describe=%s\n\n", env, obj, field_id, (NULL != describe) ? describe : "null");
	return -1;
}

long jlong_to_long(JNIEnv* env, jobject obj, jfieldID field_id, const char *describe = NULL)
{
	if ((NULL != env) && (NULL != obj) && (NULL != field_id) && (NULL != describe)) {
		return env->GetLongField(obj, field_id);
	}
	else LOG("ERROR","env=%p,obj=%p,field_id=%p,describe=%s\n\n", env, obj, field_id, (NULL != describe) ? describe : "null");
	return -1;
}

jobject get_object_field(JNIEnv *env, jobject obj, jfieldID field_id, const char *describe = NULL)
{
	if ((NULL != env) && (NULL != obj) && (NULL != field_id)) {
		return env->GetObjectField(obj, field_id);
	}
	else LOG("ERROR","env=%p,obj=%p,field_id=%p,value=%p,describe=%s\n\n", env, obj, field_id, (NULL != describe) ? describe : "null");
	return NULL;
}

int set_object_field(JNIEnv *env, jobject obj, jfieldID field_id, jobject value, const char *describe = NULL)
{
	int nRet = -1;
	if ((NULL != env) && (NULL != obj) && (NULL != field_id) && (NULL != value)) {
		env->SetObjectField(obj, field_id, value);
		nRet = 0;
	}
	else LOG("ERROR","env=%p,obj=%p,field_id=%p,value=%p,describe=%s\n\n", env, obj, field_id, value, (NULL != describe) ? describe : "null");
	return nRet;
}

int set_jstring_field(JNIEnv *env, jobject obj, jfieldID field_id, const char *value, const char *describe = NULL)
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

int set_int_field(JNIEnv *env, jobject obj, jfieldID field_id, jint value, const char *describe = NULL)
{
	int nRet = -1;
	if ((NULL != env) && (NULL != obj) && (NULL != field_id)) {
		env->SetIntField(obj, field_id, value);
		nRet = 0;
	}
	else LOG("ERROR","env=%p,obj=%p,field_id=%p,describe=%s\n\n", env, obj, field_id, (NULL != describe) ? describe : "null");
	return nRet;
}

typedef struct JniArrayList_ {
	JNIEnv *env = NULL;
	jclass cls = NULL;
	jmethodID construct = NULL;
	jobject obj = NULL;

	jmethodID jadd = NULL;		// 添加元素	(如: add("zhangsan")或add(1, "zhangsan"))
	jmethodID jsize = NULL;		// 获取ArrayList的大小
	jmethodID jset = NULL;		// 设置元素 (如: set(1, "zhangsan");)
	jmethodID jget = NULL;		// 查找元素 (如: get(0))
	jmethodID jclear = NULL;	// 清空集合内的所有元素
	jmethodID jremove = NULL;	// 删除元素(根据元素或下标删除, 如: remove("zhangsan")或remove(0))

	int init(JNIEnv *env, jobject jobj) {
		int nRet = -1;
		while (NULL != env) {
			this->env = env;
			if (NULL != jobj) {
				obj = jobj;
				cls = env->GetObjectClass(obj);
				if (NULL == cls) {
					LOG("ERROR","GetObjectClass failed.\n\n");
					break;
				}
			}
			else {
				const char *class_nane = "java/util/ArrayList";
				cls = env->FindClass(class_nane);	// java/util/ArrayList
				if (NULL == cls) {
					if (env->ExceptionOccurred())
						env->ExceptionDescribe();
					LOG("ERROR","------ FindClass failed '%s' ------\n\n", class_nane);
					break;
				}
				construct = env->GetMethodID(cls, "<init>", "()V");
				if (NULL == construct) {
					if (env->ExceptionOccurred())
						env->ExceptionDescribe();
					LOG("ERROR","------ Failed to get constructor ------\n\n");
					break;
				}
				obj = env->NewObject(cls, construct, "");
				if (NULL == obj) {
					if (env->ExceptionOccurred())
						env->ExceptionDescribe();
					LOG("ERROR","------ NewObject failed ------\n\n");
					break;
				}
			}
			jadd = env->GetMethodID(cls, "add", "(Ljava/lang/Object;)Z");
			if (NULL == jadd) {
				if (env->ExceptionOccurred())
					env->ExceptionDescribe();
				LOG("ERROR","------ The 'add' method was not found ------\n\n");
				break;
			}
			jsize = env->GetMethodID(cls, "size", "()I");
			if (NULL == jsize) {
				LOG("ERROR","------ The 'size' method was not found ------\n\n");
				break;
			}
			jget = env->GetMethodID(cls, "get", "(I)Ljava/lang/Object;");
			if (NULL == jget) {
				LOG("ERROR","------ The 'get' method was not found ------\n\n");
				break;
			}
			nRet = 0;
			break;
		}
		return nRet;
	}
	void add(jobject obj) {
		if ((NULL != obj) && (NULL != jadd)) {
			env->CallObjectMethod(obj, jget, obj);
			return;
		}
		LOG("ERROR","obj=%p,jadd=%p\n\n", obj, jadd);
	}
	jint size() {
		if ((NULL != obj) && (NULL != jsize)) {
			return env->CallIntMethod(obj, jsize);
		}
		return -1;
	}
	jobject get(int index) {
		if ((NULL != obj) && (NULL != jget) && (index >= 0)) {
			return env->CallObjectMethod(obj, jget, index);
		}
		LOG("ERROR","obj=%p,jget=%p,index=%d\n\n", obj, jget, index);
		return NULL;
	}
}JniArrayList;

/*
// Java中 List为抽象类,不能被实例化,故没有构造函数
// ArrayList 继承与 List ,如: List<object> list = new ArrayList<object>;
typedef struct JniList_ {
	JNIEnv *env = NULL;
	jclass cls = NULL;
	jmethodID add = NULL;

	int init(JNIEnv *env, jobject obj) {
		int nRet = -1;
		while (NULL != env) {
			this->env = env;
			const char *class_nane = "Ljava/util/List;";
			cls = env->FindClass(class_nane);
			if (NULL == cls) {
				if (env->ExceptionOccurred())
					env->ExceptionDescribe();
				DEBUG("ERROR: ------ FindClass failed '%s' ------\n\n", class_nane);
				break;
			}
			add = env->GetMethodID(cls, "add", "(Ljava/lang/Object;)Z");
			if (NULL == add) {
				if (env->ExceptionOccurred())
					env->ExceptionDescribe();
				DEBUG("ERROR: ------ The 'add' method was not found ------\n\n");
				break;
			}
			nRet = 0;
			break;
		}
		return nRet;
	}
}JniList;
*/


////////// 本地配置类 /////////////////////////////////////
typedef struct JniLocalConfig_ {
	JNIEnv *env = NULL;
	jclass cls = NULL;				// ��
	//jmethodID construct = NULL;		// ���췽����ID
	jobject obj = NULL;				// ͨ������
	
	jfieldID id = NULL;
	jfieldID ip = NULL;
	jfieldID port = NULL;
	jfieldID ip2 = NULL;
	jfieldID port2 = NULL;
	jfieldID domain = NULL;
	jfieldID password = NULL;
	jfieldID username = NULL;
	jfieldID manufacturer = NULL;
	jfieldID firmware = NULL;
	jfieldID model = NULL;
	jfieldID expires = NULL;
	jfieldID register_interval = NULL;
	jfieldID keepalive = NULL;
	jfieldID max_timeout = NULL;

	// ƽ̨��Ϣ
	jfieldID pfm_id = NULL;
	jfieldID pfm_ip = NULL;
	jfieldID pfm_port = NULL;
	jfieldID pfm_domain = NULL;
	jfieldID pfm_password = NULL;

	int init(JNIEnv *env, jobject obj_cfg) {
		int nRet = -1;
		while ((NULL != env) && (NULL != obj_cfg)) {
			this->env = env;
			this->obj = obj_cfg;
			cls = env->GetObjectClass(obj_cfg);
			if (NULL == cls) {
				if (env->ExceptionOccurred())
					env->ExceptionDescribe();
				LOG("ERROR","------ GetObjectClass failed ------\n\n");
				break;
			}
			id = env->GetFieldID(cls, "localSipId", "Ljava/lang/String;");
			if (NULL == id) {
				if (env->ExceptionOccurred())
					env->ExceptionDescribe();
				LOG("ERROR","------ id=null ------\n\n");
				break;
			}
			ip = env->GetFieldID(cls, "localSipIp", "Ljava/lang/String;");
			if (NULL == ip) {
				if (env->ExceptionOccurred())
					env->ExceptionDescribe();
				LOG("ERROR","------ ip=null ------\n\n");
				break;
			}
			port = env->GetFieldID(cls, "localSipPort", "I");
			if (NULL == port) {
				if (env->ExceptionOccurred())
					env->ExceptionDescribe();
				LOG("ERROR","------ port=null ------\n\n");
				break;
			}
			ip2 = env->GetFieldID(cls, "localSipIpTwo", "Ljava/lang/String;");
			if (NULL == ip2) {
				if (env->ExceptionOccurred())
					env->ExceptionDescribe();
				LOG("ERROR","------ ip2=null ------\n\n");
				break;
			}
			port2 = env->GetFieldID(cls, "localSipPortTwo", "I");
			if (NULL == port2) {
				if (env->ExceptionOccurred())
					env->ExceptionDescribe();
				LOG("ERROR","------ port2=null ------\n\n");
				break;
			}
			domain = env->GetFieldID(cls, "localSipDomain", "Ljava/lang/String;");
			if (NULL == domain) {
				if (env->ExceptionOccurred())
					env->ExceptionDescribe();
				LOG("ERROR","------ domain=null ------\n\n");
				break;
			}
			password = env->GetFieldID(cls, "localSipPassword", "Ljava/lang/String;");
			if (NULL == password) {
				if (env->ExceptionOccurred())
					env->ExceptionDescribe();
				LOG("ERROR","------ password=null ------\n\n");
				break;
			}
			username = env->GetFieldID(cls, "localSipUsername", "Ljava/lang/String;");
			if (NULL == username) {
				if (env->ExceptionOccurred())
					env->ExceptionDescribe();
				LOG("ERROR","------ username=null ------\n\n");
				break;
			}
			manufacturer = env->GetFieldID(cls, "manufacturer", "Ljava/lang/String;");
			if (NULL == manufacturer) {
				if (env->ExceptionOccurred())
					env->ExceptionDescribe();
				LOG("ERROR","------ manufacturer=null ------\n\n");
				break;
			}
			firmware = env->GetFieldID(cls, "firmware", "Ljava/lang/String;");
			if (NULL == firmware) {
				if (env->ExceptionOccurred())
					env->ExceptionDescribe();
				LOG("ERROR","------ firmware=null ------\n\n");
				break;
			}
			model = env->GetFieldID(cls, "model", "Ljava/lang/String;");
			if (NULL == model) {
				if (env->ExceptionOccurred())
					env->ExceptionDescribe();
				LOG("ERROR","------ model=null ------\n\n");
				break;
			}
			expires = env->GetFieldID(cls, "expires", "I");
			if (NULL == expires) {
				if (env->ExceptionOccurred())
					env->ExceptionDescribe();
				LOG("ERROR","------ expires=null ------\n\n");
				break;
			}
			register_interval = env->GetFieldID(cls, "registerInterval", "I");
			if (NULL == register_interval) {
				if (env->ExceptionOccurred())
					env->ExceptionDescribe();
				LOG("ERROR","------ register_interval=null ------\n\n");
				break;
			}
			keepalive = env->GetFieldID(cls, "keepalive", "I");
			if (NULL == keepalive) {
				if (env->ExceptionOccurred())
					env->ExceptionDescribe();
				LOG("ERROR","------ keepalive=null ------\n\n");
				break;
			}
			max_timeout = env->GetFieldID(cls, "maxTimeout", "I");
			if (NULL == max_timeout) {
				if (env->ExceptionOccurred())
					env->ExceptionDescribe();
				LOG("ERROR","------ max_timeout=null ------\n\n");
				break;
			}
			nRet = 0;
			break;
		}
		return nRet;
	}
}JniLocalConfig;

////////// 设备类 /////////////////////////////////////
typedef struct JniDeviceInfo_ {
	JNIEnv *env = NULL;
	jclass cls = NULL;
	jmethodID construct = NULL;
	jobject obj = NULL;

	jfieldID id = NULL;
	jfieldID parent_id = NULL;
	jfieldID ip = NULL;
	jfieldID port = NULL;
	jfieldID type = NULL;
	jfieldID protocol = NULL;
	jfieldID username = NULL;
	jfieldID password = NULL;
	jfieldID name = NULL;
	jfieldID manufacturer = NULL;
	jfieldID firmware = NULL;
	jfieldID model = NULL;
	jfieldID status = NULL;
	jfieldID expires = NULL;
	jfieldID keepalive = NULL;
	jfieldID max_k_timeout = NULL;
	jfieldID channels = NULL;
	jfieldID chl_map = NULL;
	int init(JNIEnv *env, jobject jobj) {
		int nRet = -1;			
		while (NULL != env) {
			this->env = env;
			if (NULL != jobj) {
				this->obj = jobj;
				cls = env->GetObjectClass(this->obj);
				if (NULL == cls) {
					if (env->ExceptionOccurred())
						env->ExceptionDescribe();
					LOG("ERROR","------ GetObjectClass failed ------\n\n");
					break;
				}
			}
			else {
				const char *cls_name = "com/hirain/domain/entity/operation/VideoGbdeviceinfo";
				cls = env->FindClass(cls_name);
				if (NULL == cls) {
					if (env->ExceptionOccurred())
						env->ExceptionDescribe();
					LOG("ERROR","FindClass failed ('%s').\n\n", cls_name);
					break;
				}
				construct = env->GetMethodID(cls, "<init>", "()V");
				if (NULL == construct) {
					if (env->ExceptionOccurred())
						env->ExceptionDescribe();
					LOG("ERROR","GetMethodID failed.\n\n");
					break;
				}
				obj = env->NewObject(cls, construct, "");
				if (NULL == obj) {
					if (env->ExceptionOccurred())
						env->ExceptionDescribe();
					LOG("ERROR","NewObject failed.\n\n");
					break;
				}
			}
			id = env->GetFieldID(cls, "deviceId", "Ljava/lang/String;");
			if (NULL == id) {
				if (env->ExceptionOccurred())
					env->ExceptionDescribe();
				LOG("ERROR","------ deviceId=null ------\n\n");
				break;
			}
			parent_id = env->GetFieldID(cls, "parentId", "Ljava/lang/String;");
			if (NULL == parent_id) {
				if (env->ExceptionOccurred())
					env->ExceptionDescribe();
				LOG("ERROR","------ parent_id=null ------\n\n");
				break;
			}
			ip = env->GetFieldID(cls, "deviceIp", "Ljava/lang/String;");
			if (NULL == ip) {
				if (env->ExceptionOccurred())
					env->ExceptionDescribe();
				LOG("ERROR","------ deviceIp=null ------\n\n");
				break;
			}
			port = env->GetFieldID(cls, "devicePort", "I");
			if (NULL == port) {
				if (env->ExceptionOccurred())
					env->ExceptionDescribe();
				LOG("ERROR","------ devicePort=null ------\n\n");
				break;
			}
			type = env->GetFieldID(cls, "deviceType", "I");
			if (NULL == type) {
				if (env->ExceptionOccurred())
					env->ExceptionDescribe();
				LOG("ERROR","------ deviceType=null ------\n\n");
				break;
			}
			protocol = env->GetFieldID(cls, "protocol", "I");
			if (NULL == protocol) {
				if (env->ExceptionOccurred())
					env->ExceptionDescribe();
				LOG("ERROR","------ protocol=null ------\n\n");//4
				break;
			}
			username = env->GetFieldID(cls, "userName", "Ljava/lang/String;");
			if (NULL == username) {
				if (env->ExceptionOccurred())
					env->ExceptionDescribe();
				LOG("ERROR","------ username=null ------\n\n");//5
				break;
			}
			password = env->GetFieldID(cls, "passWord", "Ljava/lang/String;");
			if (NULL == password) {
				if (env->ExceptionOccurred())
					env->ExceptionDescribe();
				LOG("ERROR","------ password=null ------\n\n");//6
				break;
			}
			name = env->GetFieldID(cls, "deviceName", "Ljava/lang/String;");
			if (NULL == name) {
				if (env->ExceptionOccurred())
					env->ExceptionDescribe();
				LOG("ERROR","------ name=null ------\n\n");//7
				break;
			}
			manufacturer = env->GetFieldID(cls, "manufacturer", "Ljava/lang/String;");
			if (NULL == manufacturer) {
				if (env->ExceptionOccurred())
					env->ExceptionDescribe();
				LOG("ERROR","------ manufacturer=null ------\n\n");//8
				break;
			}
			firmware = env->GetFieldID(cls, "firmware", "Ljava/lang/String;");
			if (NULL == firmware) {
				if (env->ExceptionOccurred())
					env->ExceptionDescribe();
				LOG("ERROR","------ firmware=null ------\n\n");//9
				break;
			}
			model = env->GetFieldID(cls, "model", "Ljava/lang/String;");
			if (NULL == model) {
				if (env->ExceptionOccurred())
					env->ExceptionDescribe();
				LOG("ERROR","------ model=null ------\n\n");//10
				break;
			}
			status = env->GetFieldID(cls, "deviceStatus", "I");
			if (NULL == status) {
				if (env->ExceptionOccurred())
					env->ExceptionDescribe();
				LOG("ERROR","------ status=null ------\n\n");//11
				break;
			}
			expires = env->GetFieldID(cls, "expires", "I");
			if (NULL == expires) {
				if (env->ExceptionOccurred())
					env->ExceptionDescribe();
				LOG("ERROR","------ expires=null ------\n\n");//12
				break;
			}
			keepalive = env->GetFieldID(cls, "keepalive", "I");
			if (NULL == keepalive) {
				if (env->ExceptionOccurred())
					env->ExceptionDescribe();
				LOG("ERROR","------ keepalive=null ------\n\n");//13
				break;
			}
			max_k_timeout = env->GetFieldID(cls, "maxKTimeout", "I");
			if (NULL == max_k_timeout) {
				if (env->ExceptionOccurred())
					env->ExceptionDescribe();
				LOG("ERROR","------ max_k_timeout=null ------\n\n");//14
				break;
			}
			channels = env->GetFieldID(cls, "channels", "I");
			if (NULL == channels) {
				if (env->ExceptionOccurred())
					env->ExceptionDescribe();
				LOG("ERROR","------ channels=null ------\n\n");//15
				break;
			}
			#if 1
			chl_map = env->GetFieldID(cls, "children", "Ljava/util/List;");
			if (NULL == chl_map) {
				LOG("ERROR","------ chl_map = null ------\n\n");//17
				//break;
			}
			#endif
			nRet = 0;
			break;
		}
		return nRet;
	}	// init
	int SetObjectField(jfieldID field_id, jobject value) {
		int nRet = -1;
		if((NULL != env) && (NULL != obj) && (NULL != field_id) && (NULL != value)) {
			env->SetObjectField(obj, field_id, value);
			nRet = 0;
		}
		else LOG("ERROR","env=%p, obj=%p, field_id=%p, value=%p\n\n", env, obj, field_id, value);
		return nRet;
	}
	int SetJStringField(jfieldID field_id, const char *value) {
		int nRet = -1;
		if((NULL != field_id) && (NULL != value) && (NULL != env) && (NULL != obj)) {
			jstring jstr = char_to_jstring(env, value);
			if(NULL != jstr) {
				env->SetObjectField(obj, field_id, jstr);
				nRet = 0;
			}
			else LOG("ERROR","char_to_jstring failed ('%s')\n\n", value);
		}
		else LOG("ERROR","env=%p, obj=%p, field_id=%p, value=%p\n\n", env, obj, field_id, value);
		return nRet;
	}
	int SetIntField(jfieldID field_id, jint value) {
		int nRet = -1;
		if((NULL != field_id) && (NULL != env) && (NULL != obj)) {
			env->SetIntField(obj, field_id, value);
			nRet = 0;
		}
		else LOG("ERROR","env=%p, obj=%p, field_id=%p\n\n", env, obj, field_id);
		return nRet;
	}
}JniDeviceInfo;

////////// 通道类 /////////////////////////////////////
typedef struct JniChannelInfo_ {
	JNIEnv *env = NULL;
	jclass cls = NULL;				// ��
	jmethodID construct = NULL;		// ���췽����ID
	jobject obj = NULL;				// ͨ������

	jfieldID id = NULL;				// ͨ��ID
	jfieldID parent_id = NULL;		// �ϼ�ID
	jfieldID name = NULL;			// ����
	jfieldID manufacturer = NULL;	// ������
	jfieldID model = NULL;			// �ͺ�
	jfieldID owner = NULL;			//
	jfieldID civil_code = NULL;		//
	jfieldID address = NULL;		// ��ַ
	jfieldID ip_address = NULL;		// IP
	jfieldID port = NULL;			// �˿�
	jfieldID register_way = NULL;
	jfieldID safety_way = NULL;
	jfieldID parental = NULL;		// �Ƿ�Ϊ���豸(1:�� 0:��)
	jfieldID secrecy = NULL;		// ��������(ȱʡΪ0, 0:������, 1:����)
	jfieldID status = NULL;			// ״̬ (0:OFF 1:ON)
	jfieldID longitude = NULL;			// 经度(可选)
	jfieldID latitude = NULL;			// 纬度(可选)

	int init(JNIEnv *env, jobject jobj) {
		int nRet = -1;
		while (NULL != env) {
			this->env = env;
			if (NULL != jobj) {
				obj = jobj;
				cls = env->GetObjectClass(obj);
				if (NULL == cls) {
					if (env->ExceptionOccurred())
						env->ExceptionDescribe();
					LOG("ERROR","------ GetObjectClass failed ------\n\n");
					break;
				}
			}
			else {
				const char *cls_name = "com/hirain/domain/entity/operation/VideoGbchannelinfo";
				cls = env->FindClass(cls_name);
				if (NULL == cls) {
					if (env->ExceptionOccurred())
						env->ExceptionDescribe();
					LOG("ERROR","FindClass failed ('%s')\n\n", cls_name);
					break;
				}
				construct = env->GetMethodID(cls, "<init>", "()V");
				if (NULL == construct) {
					if (env->ExceptionOccurred())
						env->ExceptionDescribe();
					LOG("ERROR","GetMethodID failed.\n\n");
					break;
				}
				obj = env->NewObject(cls, construct, "");
				if (NULL == obj) {
					if (env->ExceptionOccurred())
						env->ExceptionDescribe();
					LOG("ERROR","------ NewObject failed. ------\n\n");
					break;
				}
			}
			id = env->GetFieldID(cls, "channelId", "Ljava/lang/String;");
			if (NULL == id) {
				if (env->ExceptionOccurred())
					env->ExceptionDescribe();
				LOG("ERROR","------ id=null ------\n\n");
				break;
			}
			parent_id = env->GetFieldID(cls, "parentId", "Ljava/lang/String;");
			if (NULL == parent_id) {
				if (env->ExceptionOccurred())
					env->ExceptionDescribe();
				LOG("ERROR","------ parent_id=null ------\n\n");
				break;
			}
			name = env->GetFieldID(cls, "channelName", "Ljava/lang/String;");
			if (NULL == name) {
				if (env->ExceptionOccurred())
					env->ExceptionDescribe();
				LOG("ERROR","------ name=null ------\n\n");
				break;
			}
			manufacturer = env->GetFieldID(cls, "manufacturer", "Ljava/lang/String;");
			if (NULL == manufacturer) {
				if (env->ExceptionOccurred())
					env->ExceptionDescribe();
				LOG("ERROR","------ manufacturer=null ------\n\n");
				break;
			}
			model = env->GetFieldID(cls, "model", "Ljava/lang/String;");
			if (NULL == model) {
				if (env->ExceptionOccurred())
					env->ExceptionDescribe();
				LOG("ERROR","------ model=null ------\n\n");
				break;
			}
			owner = env->GetFieldID(cls, "owner", "Ljava/lang/String;");
			if (NULL == owner) {
				if (env->ExceptionOccurred())
					env->ExceptionDescribe();
				LOG("ERROR","------ owner=null ------\n\n");
				break;
			}
			civil_code = env->GetFieldID(cls, "civilCode", "Ljava/lang/String;");
			if (NULL == civil_code) {
				if (env->ExceptionOccurred())
					env->ExceptionDescribe();
				LOG("ERROR","------ civil_code=null ------\n\n");
				break;
			}
			address = env->GetFieldID(cls, "address", "Ljava/lang/String;");
			if (NULL == address) {
				if (env->ExceptionOccurred())
					env->ExceptionDescribe();
				LOG("ERROR","------ address=null ------\n\n");
				break;
			}
			ip_address = env->GetFieldID(cls, "ipAddress", "Ljava/lang/String;");
			if (NULL == ip_address) {
				if (env->ExceptionOccurred())
					env->ExceptionDescribe();
				LOG("ERROR","------ ip_address=null ------\n\n");
				break;
			}
			port = env->GetFieldID(cls, "port", "I");
			if (NULL == port) {
				if (env->ExceptionOccurred())
					env->ExceptionDescribe();
				LOG("ERROR","------ port=null ------\n\n");
				break;
			}
			register_way = env->GetFieldID(cls, "registerWay", "Ljava/lang/String;");
			if (NULL == register_way) {
				if (env->ExceptionOccurred())
					env->ExceptionDescribe();
				LOG("ERROR","------ register_way=null ------\n\n");
				break;
			}
			safety_way = env->GetFieldID(cls, "safetyWay", "Ljava/lang/String;");
			if (NULL == safety_way) {
				if (env->ExceptionOccurred())
					env->ExceptionDescribe();
				LOG("ERROR","------ safety_way=null ------\n\n");
				break;
			}
			parental = env->GetFieldID(cls, "parental", "Ljava/lang/String;");
			if (NULL == parental) {
				if (env->ExceptionOccurred())
					env->ExceptionDescribe();
				LOG("ERROR","------ parental=null ------\n\n");
				break;
			}
			secrecy = env->GetFieldID(cls, "secrecy", "Ljava/lang/String;");
			if (NULL == secrecy) {
				if (env->ExceptionOccurred())
					env->ExceptionDescribe();
				LOG("ERROR","------ secrecy=null ------\n\n");
				break;
			}
			//status = env->GetFieldID(cls, "channelStatus", "Ljava/lang/String;");
			status = env->GetFieldID(cls, "channelStatus", "I");
			if (NULL == status) {
				if (env->ExceptionOccurred())
					env->ExceptionDescribe();
				LOG("ERROR","------ status=null ------\n\n");
				break;
			}
			longitude = env->GetFieldID(cls, "longitude", "Ljava/lang/String;");
			if (NULL == longitude) {
				LOG("ERROR","------ longitude=null ------\n\n");
				break;
			}
			latitude = env->GetFieldID(cls, "latitude", "Ljava/lang/String;");
			if (NULL == latitude) {
				LOG("ERROR","------ latitude=null ------\n\n");
				break;
			}
			nRet = 0;
			break;
		}
		return nRet;
	}
	int SetObjectField(jfieldID field_id, jobject value) {
		int nRet = -1;
		if((NULL != field_id) && (NULL != value)) {
			env->SetObjectField(obj, field_id, value);
			nRet = 0;
		}
		else LOG("ERROR","field_id=%p, value=%p\n\n", field_id, value);
		return nRet;
	}
	int SetJStringField(jfieldID field_id, const char *value) {
		int nRet = -1;
		if((NULL != field_id) && (NULL != value)) {
			jstring jstr = char_to_jstring(env, value);
			if(NULL != jstr) {
				env->SetObjectField(obj, field_id, jstr);
				nRet = 0;
			}
			else LOG("ERROR","char_to_jstring failed ('%s')\n\n", value);
		}
		else LOG("ERROR","field_id=%p, value=%p\n\n", field_id, value);

		return nRet;
	}
	int SetIntField(jfieldID field_id, jint value) {
		int nRet = -1;
		if(NULL != field_id) {
			env->SetIntField(obj, field_id, value);
			nRet = 0;
		}
		else LOG("ERROR","field_id=%p\n\n", field_id);
		return nRet;
	}
}JniChannelInfo;

typedef struct JniDeviceinfoProviderImp_ {
	JNIEnv *env = NULL;
	jclass cls = NULL;
	jmethodID construct = NULL;
	jobject obj = NULL;

	jmethodID batchInsert = NULL;	// 设备注册与注销
	jmethodID QueryRecord = NULL;	// 录像查询
	jmethodID QueryDevice = NULL;	// 设备查询(设备查询,心跳)

	int init(JNIEnv *env) {
		int nRet = -1;
		while (NULL != env) {
			this->env = env;
			//const char *cls_name = "com/hirain/sip/provider/SipAbilityProviderImpl";
			const char *cls_name = "com/hirain/sip/service/GbCallbackService";
			cls = env->FindClass(cls_name);
			if (NULL == cls) {
				if (env->ExceptionOccurred())
					env->ExceptionDescribe();
				LOG("ERROR","FindClass failed ('%s')\n\n", cls_name);
				break;
			}
			construct = env->GetMethodID(cls, "<init>", "()V");
			if (NULL == construct) {
				if (env->ExceptionOccurred())
					env->ExceptionDescribe();
				LOG("ERROR","GetMethodID failed.\n\n");
				break;
			}
			obj = env->NewObject(cls, construct, "");
			if (NULL == obj) {
				if (env->ExceptionOccurred())
					env->ExceptionDescribe();
				LOG("ERROR","NewObject failed.\n\n");
				break;
			}
			batchInsert = env->GetMethodID(cls, "insertGbDeviceinfo", "(Lcom/hirain/domain/entity/operation/VideoGbdeviceinfo;Ljava/lang/String;)V");
			if (NULL == batchInsert) {
				if (env->ExceptionOccurred())
					env->ExceptionDescribe();
				LOG("ERROR","------ batchInsert=null ------\n\n");
				break;
			}
			QueryRecord = env->GetMethodID(cls, "queryVideoByCondition", "(Lcom/hirain/domain/param/operation/GbVideoInformationParam;)Ljava/util/List;");
			if (NULL == QueryRecord) {
				if (env->ExceptionOccurred())
					env->ExceptionDescribe();
				LOG("ERROR","------ QueryRecord=null ------\n\n");
				break;
			}
			QueryDevice = env->GetMethodID(cls, "queryDeviceInformation", "(Lcom/hirain/domain/entity/operation/VideoGbdeviceinfo;)V");
			if (NULL == QueryRecord) {
				if (env->ExceptionOccurred())
					env->ExceptionDescribe();
				LOG("ERROR","------ QueryDevice=null ------\n\n");
				break;
			}
			nRet = 0;
			break;
		}
		return nRet;
	}
}JniDeviceinfoProviderImp;

#if 0
//////// 设备注册回调时会用到
typedef struct JniSipSerConfig_{
	JNIEnv *env = NULL;
	jclass cls = NULL;				//
	jmethodID construct = NULL;		//
	jobject obj = NULL;				//
	jfieldID id = NULL;				// ID
	jfieldID ip = NULL;				// IP
	jfieldID port = NULL;			// Port
	jfieldID domain = NULL;			// domain
	jfieldID password = NULL;		// password
	int init(JNIEnv *env) {
		int nRet = -1;
		while (NULL != env) {
			this->env = env;
			const char *cls_name = "com/hirain/domain/entity/operation/VideoSipconfig";
			cls = env->FindClass(cls_name);
			if (NULL == cls) {
				LOG("ERROR","FindClass failed ('%s')\n\n", cls_name);
				break;
			}
			construct = env->GetMethodID(cls, "<init>", "()V");
			if (NULL == construct) {
				LOG("ERROR","GetMethodID failed.\n\n");
				break;
			}
			obj = env->NewObject(cls, construct, "");
			if (NULL == obj) {
				LOG("ERROR","NewObject failed.\n\n");
				break;
			}
			id = env->GetFieldID(cls, "sip_id", "Ljava/lang/String;");
			if (NULL == id) {
				LOG("ERROR","------ id=null ------\n\n");
				break;
			}
			ip = env->GetFieldID(cls, "sip_host", "Ljava/lang/String;");
			if (NULL == ip) {
				LOG("ERROR","------ ip=null ------\n\n");
				break;
			}
			port = env->GetFieldID(cls, "sip_port", "I");
			if (NULL == port) {
				LOG("ERROR","------ port=null ------\n\n");
				break;
			}
			domain = env->GetFieldID(cls, "sip_domain", "Ljava/lang/String;");
			if (NULL == domain) {
				LOG("ERROR","------ ip=null ------\n\n");
				break;
			}
			password = env->GetFieldID(cls, "sip_password", "Ljava/lang/String;");
			if (NULL == password) {
				LOG("ERROR","------ ip=null ------\n\n");
				break;
			}
			nRet = 0;
			break;
		}
		return nRet;
	}
}JniSipSerConfig;
#endif

typedef struct JniWebSocketServer_ {
	JNIEnv *env = NULL;
	jclass cls = NULL;				// ��
	//jmethodID construct = NULL;		// ���췽����ID
	//jobject obj = NULL;				// ͨ������
	//jmethodID send = NULL;			// Send���� (��̬����)
	jmethodID sendGbPlayStream = NULL;
	//jmethodID sendGbPlayBackStream = NUL;
	int init(JNIEnv *env) {
		int nRet = -1;
		while (NULL != env) {
			this->env = env;
			const char *cls_name = "com/hirain/media/websocket/WebSocketServer";
			cls = env->FindClass(cls_name);
			if (NULL == cls) {
				if (env->ExceptionOccurred())
					env->ExceptionDescribe();
				LOG("ERROR","FindClass failed('%s')\n\n", cls_name);
				break;
			}
			sendGbPlayStream = env->GetStaticMethodID(cls, "sendGbPlayStream", "([BLjava/lang/String;Ljava/lang/String;)V");
			if (NULL == sendGbPlayStream) {
				if (env->ExceptionOccurred())
					env->ExceptionDescribe();
				LOG("ERROR","------ sendGbPlayStream=null ------\n\n");
				break;
			}
			#if 0
			sendGbPlayBackStream = env->GetStaticMethodID(cls, "sendGbPlayBackStream", "([BLjava/lang/String;Ljava/lang/String;)V");
			if (NULL == send) {
				if (env->ExceptionOccurred())
					env->ExceptionDescribe();
				LOG("ERROR","------ send=null ------\n\n");
				break;
			}
			#endif
			// env->CallStaticObjectMethod(cls, send, bytes, size);
			nRet = 0;
			break;
		}
		return nRet;
	}
}JniWebSocketServer;

typedef struct JniOperation_{
	JNIEnv *env = NULL;
	jclass cls = NULL;
	jmethodID construct = NULL;
	jobject obj = NULL;

	// 基本参数
	jfieldID id = NULL;			// 设备ID
	jfieldID ip = NULL;			// 设备IP
	jfieldID port = NULL;		// 设备Port
	jfieldID cmd = NULL;		// 命令类型

	jfieldID peer_ip = NULL;	//设备所属SIP服务的IP (video-sip服务模块的UDP通信IP)
	jfieldID peer_port = NULL;	//设备所属SIP服务的UDP通信端口 (video-sip服务模块的UDP通信Port)
	jfieldID local_ip = NULL;	//本地IP (video-media模块的IP，主要用于接收摄像头的视频流)

	// 点播与回放参数
	jfieldID gid = NULL;
	jfieldID token = NULL;
	
	// 录像参数
	jfieldID file_path = NULL;		// 录像文件路径
	jfieldID duration = NULL;	    // 录像时长
	jfieldID tape_file = NULL;		// 录像文件结果返回
	jfieldID frame_speed = NULL;	// 抽帧存储
	
	// 其他参数
	jfieldID start_time = NULL;
	jfieldID end_time = NULL;
	jfieldID ptz_speed = NULL;
	jfieldID pos = NULL;

	int init(JNIEnv *env, jobject opt) {
		int nRet = -1;
		while (NULL != env) {
			this->env = env;
			if (NULL != opt) {
				this->obj = opt;
				cls = env->GetObjectClass(opt);
				if (NULL == cls) {
					LOG("ERROR","GetObjectClass failed.\n\n");
					break;
				}
			}
			else {
				//const char *cls_name = "com/hirain/sdk/JNIEntity/Operation";
				const char *cls_name = "com/hirain/sdk/entity/gb28181/Operation";
				cls = env->FindClass(cls_name);
				if (NULL == cls) {
					LOG("ERROR","FindClass failed '%s'.\n\n", cls_name);
					break;
				}
				construct = env->GetMethodID(cls, "<init>", "()V");
				if (NULL == construct) {
					LOG("ERROR","GetMethodID failed 'construct'.\n\n");
					break;
				}
				obj = env->NewObject(cls, construct, "");
				if (NULL == obj) {
					LOG("ERROR","NewObject failed.\n\n");
					break;
				}
			}
			id = env->GetFieldID(cls, "deviceId", "Ljava/lang/String;");
			if (NULL == id) {
				if (env->ExceptionOccurred())
					env->ExceptionDescribe();
				LOG("WARN","id == null\n\n");
				//break;
			}
			ip = env->GetFieldID(cls, "deviceIp", "Ljava/lang/String;");
			if (NULL == ip) {
				if (env->ExceptionOccurred())
					env->ExceptionDescribe();
				LOG("WARN","ip == null\n\n");
				//break;
			}
			port = env->GetFieldID(cls, "devicePort", "I");
			if (NULL == port) {
				if (env->ExceptionOccurred())
					env->ExceptionDescribe();
				LOG("WARN","port == null\n\n");
				//break;
			}
			cmd = env->GetFieldID(cls, "cmd", "I");
			if (NULL == cmd) {
				LOG("WARN","cmd == null\n\n");
				//break;
			}
			peer_ip = env->GetFieldID(cls, "sipIp", "Ljava/lang/String;");
			if (NULL == peer_ip) {
				if (env->ExceptionOccurred())
					env->ExceptionDescribe();
				//break;
			}
			peer_port = env->GetFieldID(cls, "sipUdpPort", "I");
			if (NULL == peer_port) {
				if (env->ExceptionOccurred())
					env->ExceptionDescribe();
				//break;
			}
			local_ip = env->GetFieldID(cls, "mediaIp", "Ljava/lang/String;");
			if (NULL == local_ip) {
				if (env->ExceptionOccurred())
					env->ExceptionDescribe();
				//break;
			}
			#if 0
			// 打开这里会导致录像(本地录像)接口假返回
			start_time = env->GetFieldID(cls, "startTime", "L");
			if (NULL == start_time) {
				if (env->ExceptionOccurred())
					env->ExceptionDescribe();
				LOG("WARN","start_time == null\n\n");
				//break;
			}
			end_time = env->GetFieldID(cls, "endTime", "L");
			if (NULL == end_time) {
				LOG("WARN","end_time == null\n\n");
				//break;
			}
			ptz_speed = env->GetFieldID(cls, "ptzSpeed", "I");
			if (NULL == ptz_speed) {
				if (env->ExceptionOccurred())
					env->ExceptionDescribe();
				LOG("WARN","ptz_speed == null\n\n");
				//break;
			}
			#endif
			////////////// 点播与回放 ///////////////////////////////////
			gid = env->GetFieldID(cls, "gid", "Ljava/lang/String;");
			if (NULL == gid) {
				if (env->ExceptionOccurred())
					env->ExceptionDescribe();
				LOG("WARN","gid == null\n\n");
				//break;
			}
			token = env->GetFieldID(cls, "token", "Ljava/lang/String;");
			if (NULL == token) {
				if (env->ExceptionOccurred())
					env->ExceptionDescribe();
				LOG("WARN","token == null\n\n");
				//break;
			}
			////////////// 录像 ///////////////////////////////////
			file_path = env->GetFieldID(cls, "filePath", "Ljava/lang/String;");
			if (NULL == gid) {
				LOG("WARN","file_path == null\n\n");
				//break;
			}
			duration = env->GetFieldID(cls, "stopSecond", "I");
			if (NULL == duration) {
				LOG("WARN","stop_second == null\n\n");
				//break;
			}
			tape_file = env->GetFieldID(cls, "tapeFile", "Ljava/lang/Object;");
			if (NULL == tape_file) {
				LOG("WARN","tape_file == null\n\n");
				//break;
			}
			frame_speed = env->GetFieldID(cls, "frameSpeed", "I");
			if (NULL == frame_speed) {
				LOG("WARN","frame_speed == null\n\n");
				//break;
			}
			pos = env->GetFieldID(cls, "pos", "I");
			if (NULL == frame_speed) {
				LOG("WARN","pos == null\n\n");
				//break;
			}
			nRet = 0;
			break;
		}
		return nRet;
	}	// init()
	int SetObjectField(jfieldID field_id, jobject value) {
		int nRet = -1;
		if ((NULL != field_id) && (NULL != value)) {
			env->SetObjectField(obj, field_id, value);
			nRet = 0;
		}
		else LOG("ERROR","field_id=%p, value=%p\n\n", field_id, value);
		return nRet;
	}
	int SetJStringField(jfieldID field_id, const char *value) {
		int nRet = -1;
		if ((NULL != field_id) && (NULL != value)) {
			jstring jstr = char_to_jstring(env, value);
			if (NULL != jstr) {
				env->SetObjectField(obj, field_id, jstr);
				nRet = 0;
			}
			else LOG("ERROR","char_to_jstring failed ('%s')\n\n", value);
		}
		else LOG("ERROR","field_id=%p, value=%p\n\n", field_id, value);
		return nRet;
	}
	int SetIntField(jfieldID field_id, jint value) {
		int nRet = -1;
		if (NULL != field_id) {
			env->SetIntField(obj, field_id, value);
			nRet = 0;
		}
		else LOG("ERROR","field_id=%p\n\n", field_id);
		return nRet;
	}
}JniOperation;


// 录像文件类
typedef struct JniVideoTapeFile_ {
	JNIEnv *env = NULL;
	jclass cls = NULL;				// ��
	jmethodID construct = NULL;		// ���췽����ID
	jobject obj = NULL;				// �豸����

	jfieldID chl_id = NULL;
	jfieldID file_date = NULL;
	jfieldID start_time = NULL;
	jfieldID end_time = NULL;
	jfieldID duration = NULL;		// 录像时长
	jfieldID size = NULL;			// 文件大小
	jfieldID url = NULL;
	jfieldID file_name = NULL;
	jfieldID video_encode = NULL;	// 视频编码格式(1:h.264, 2:h.265, 3:其他)
	jfieldID status = NULL;			// 状态(0:删除, 1:正常)

	int init(JNIEnv *env,jobject o_obj=NULL) {
		int nRet = -1;
		while (NULL != env) {
			this->env = env;
			if (NULL != o_obj) {
				this->obj = o_obj;
				cls = env->GetObjectClass(o_obj);
				if (NULL == cls) {
					LOG("ERROR","GetObjectClass failed.\n\n");
					break;
				}
			}
			else {
				const char *cls_name = "com/hirain/domain/entity/operation/VideoTapeFile";
				cls = env->FindClass(cls_name);
				if (NULL == cls) {
					LOG("ERROR","FindClass failed ('%s').\n\n", cls_name);
					break;
				}
				construct = env->GetMethodID(cls, "<init>", "()V");
				if (NULL == construct) {
					LOG("ERROR","GetMethodID failed.\n\n");
					break;
				}
				obj = env->NewObject(cls, construct, "");
			}
			if (NULL == obj) {
				LOG("ERROR","NewObject failed.\n\n");
				break;
			}

			chl_id = env->GetFieldID(cls, "channelId", "Ljava/lang/String;");
			if (NULL == chl_id) {
				LOG("ERROR","------ chl_id=null ------\n\n");
				break;
			}
			file_date = env->GetFieldID(cls, "fileDate", "Ljava/lang/String;");
			if (NULL == file_date) {
				LOG("ERROR","------ file_date=null ------\n\n");
				break;
			}
			start_time = env->GetFieldID(cls, "startTime", "Ljava/lang/String;");
			if (NULL == start_time) {
				LOG("ERROR","------ start_time=null ------\n\n");
				break;
			}
			end_time = env->GetFieldID(cls, "endTime", "Ljava/lang/String;");
			if (NULL == end_time) {
				LOG("ERROR","------ end_time=null ------\n\n");
				break;
			}
			duration = env->GetFieldID(cls, "duration", "Ljava/lang/String;");
			if (NULL == duration) {
				LOG("ERROR","------ duration=null ------\n\n");
				break;
			}
			size = env->GetFieldID(cls, "size", "I");
			if (NULL == size) {
				LOG("ERROR","------ size=null ------\n\n");
				break;
			}
			url = env->GetFieldID(cls, "url", "Ljava/lang/String;");
			if (NULL == url) {
				LOG("ERROR","------ url=null ------\n\n");
				break;
			}
			file_name = env->GetFieldID(cls, "fileName", "Ljava/lang/String;");
			if (NULL == file_name) {
				LOG("ERROR","------ file_name=null ------\n\n");
				break;
			}
			video_encode = env->GetFieldID(cls, "videoEncode", "I");
			if (NULL == video_encode) {
				LOG("ERROR","------ video_encode=null ------\n\n");
				break;
			}
			status = env->GetFieldID(cls, "status", "I");
			if (NULL == status) {
				LOG("ERROR","------ status=null ------\n\n");
				break;
			}
			nRet = 0;
			break;
		}
		return nRet;
	}	// init
	int SetObjectField(jfieldID field_id, jobject value) {
		int nRet = -1;
		if ((NULL != field_id) && (NULL != value)) {
			env->SetObjectField(obj, field_id, value);
			nRet = 0;
		}
		else LOG("ERROR","field_id=%p, value=%p\n\n", field_id, value);
		return nRet;
	}
	int SetJStringField(jfieldID field_id, const char *value) {
		int nRet = -1;
		if ((NULL != field_id) && (NULL != value)) {
			jstring jstr = char_to_jstring(env, value);
			if (NULL != jstr) {
				env->SetObjectField(obj, field_id, jstr);
				nRet = 0;
			}
			else LOG("ERROR","char_to_jstring failed ('%s')\n\n", value);
		}
		else LOG("ERROR","field_id=%p, value=%p\n\n", field_id, value);
		return nRet;
	}
	int SetIntField(jfieldID field_id, jint value) {
		int nRet = -1;
		if (NULL != field_id) {
			env->SetIntField(obj, field_id, value);
			nRet = 0;
		}
		else LOG("ERROR","field_id=%p\n\n", field_id);
		return nRet;
	}
}JniVideoTapeFile;
/////////////////////////////////////////////

// 录像
typedef struct JniGBRecord_{
	JNIEnv *env = NULL;
	jclass cls = NULL;
	jobject obj = NULL;

	jfieldID dev_id = NULL;
	jfieldID chl_id = NULL;
	jfieldID start_time = NULL;
	jfieldID end_time = NULL;
	jfieldID file_path = NULL;
	jfieldID address = NULL;
	jfieldID secrecy = NULL;
	jfieldID type = NULL;
	jfieldID rec_id = NULL;
	jfieldID name = NULL;
	jfieldID size = NULL;
	int init(JNIEnv *env, jobject o_obj = NULL) {
		int nRet = -1;
		while (NULL != env) {
			this->env = env;
			if (NULL != o_obj) {
				this->obj = o_obj;
				cls = env->GetObjectClass(o_obj);
				if (NULL == cls) {
					LOG("ERROR","GetObjectClass failed.\n\n");
					break;
				}
			}
			else {
				const char *cls_name = "com/hirain/domain/param/operation/GbVideoInformationParam";
				cls = env->FindClass(cls_name);
				if (NULL == cls) {
					LOG("ERROR","FindClass failed ('%s').\n\n", cls_name);
					break;
				}
				jmethodID construct = env->GetMethodID(cls, "<init>", "()V");
				if (NULL == construct) {
					LOG("ERROR","GetMethodID construct failed.\n\n");
					break;
				}
				obj = env->NewObject(cls, construct, "");
			}
			if (NULL == obj) {
				LOG("ERROR","NewObject failed.\n\n");
				break;
			}

			dev_id = env->GetFieldID(cls, "deviceId", "Ljava/lang/String;");
			if (NULL == dev_id) {
				LOG("ERROR","------ dev_id=null ------\n\n");
				break;
			}
			chl_id = env->GetFieldID(cls, "channelId", "Ljava/lang/String;");
			if (NULL == dev_id) {
				LOG("ERROR","------ chl_id=null ------\n\n");
				break;
			}
			start_time = env->GetFieldID(cls, "startTime", "Ljava/lang/String;");
			if (NULL == start_time) {
				LOG("ERROR","------ start_time=null ------\n\n");
				break;
			}
			end_time = env->GetFieldID(cls, "endTime", "Ljava/lang/String;");
			if (NULL == end_time) {
				LOG("ERROR","------ end_time=null ------\n\n");
				break;
			}
			file_path = env->GetFieldID(cls, "filePath", "Ljava/lang/String;");
			if (NULL == file_path) {
				LOG("ERROR","------ file_path=null ------\n\n");
				break;
			}
			address = env->GetFieldID(cls, "address", "Ljava/lang/String;");
			if (NULL == address) {
				LOG("ERROR","------ address=null ------\n\n");
				break;
			}
			secrecy = env->GetFieldID(cls, "secrecy", "I");
			if (NULL == secrecy) {
				LOG("ERROR","------ secrecy=null ------\n\n");
				break;
			}
			type = env->GetFieldID(cls, "type", "Ljava/lang/String;");
			if (NULL == type) {
				LOG("ERROR","------ type=null ------\n\n");
				break;
			}
			rec_id = env->GetFieldID(cls, "recorderId", "Ljava/lang/String;");
			if (NULL == rec_id) {
				LOG("ERROR","------ rec_id=null ------\n\n");
				break;
			}
			name = env->GetFieldID(cls, "name", "Ljava/lang/String;");
			if (NULL == name) {
				LOG("ERROR","------ name=null ------\n\n");
				break;
			}
			size = env->GetFieldID(cls, "fileSize", "I");
			if (NULL == size) {
				LOG("ERROR","------ size=null ------\n\n");
				break;
			}
			nRet = 0;
			break;
		}
		return nRet;
	}	// init
}JniGBRecord;
/////////////////////////////////////////////

// 国标平台配置类
typedef struct JniPlatform_ {
	JNIEnv *env = NULL;
	jclass cls = NULL;				//
	jmethodID construct = NULL;		//
	jobject obj = NULL;				//
	jfieldID enable = NULL;
	jfieldID id = NULL;				// ID
	jfieldID ip = NULL;				// IP
	jfieldID port = NULL;			// Port
	jfieldID domain = NULL;			// domain
	jfieldID platform = NULL;		// 平台接入方式(如: 'GB/T28181-2011', 'GB/T28181-2016'等)
	jfieldID protocol = NULL;		// 数据传输协议(1:UDP 2:TCP 3:UDP+TCP)
	jfieldID username = NULL;		// 用户名
	jfieldID password = NULL;		// 鉴权密码
	jfieldID  expires = NULL;		// 注册有效期(取值:3600 ~ 100000)
	jfieldID register_interval = NULL;// 注册间隔(取值:60 ~ 600)
	jfieldID keepalive = NULL;		// 心跳周期(取值:5 ~ 255)
	jfieldID status = NULL;			// 注册状态(0:未注册 1:注册成功 2:注册失败 3:注销成功 4:注册中)

	int init(JNIEnv *env, jobject pfm = NULL) {
		int nRet = -1;
		while (NULL != env) {
			this->env = env;
			if (NULL != pfm) {
				obj = pfm;
				cls = env->GetObjectClass(pfm);
				if (NULL == cls) {
					LOG("ERROR","GetObjectClass failed.\n\n");
					break;
				}
			}
			else {
				const char *cls_name = "com/hirain/domain/entity/operation/VideoCascadeConfig";
				cls = env->FindClass(cls_name);
				if (NULL == cls) {
					LOG("ERROR","FindClass failed '%s'.\n\n", cls_name);
					break;
				}
				construct = env->GetMethodID(cls, "<init>", "()V");
				if (NULL == construct) {
					LOG("ERROR","GetMethodID failed 'construct'.\n\n");
					break;
				}
				obj = env->NewObject(cls, construct, "");
				if (NULL == obj) {
					LOG("ERROR","NewObject failed.\n\n");
					break;
				}
			}
			enable = env->GetFieldID(cls, "isAccess", "I");
			if (NULL == enable) {
				LOG("ERROR","------ enable=null ------\n\n");
			}
			id = env->GetFieldID(cls, "sipId", "Ljava/lang/String;");
			if (NULL == id) {
				LOG("ERROR","------ id=null ------\n\n");
				break;
			}
			ip = env->GetFieldID(cls, "sipHost", "Ljava/lang/String;");
			if (NULL == ip) {
				LOG("ERROR","------ ip=null ------\n\n");
				break;
			}
			port = env->GetFieldID(cls, "sipPort", "I");
			if (NULL == port) {
				LOG("ERROR","------ port=null ------\n\n");
				break;
			}
			domain = env->GetFieldID(cls, "sipDomain", "Ljava/lang/String;");
			if (NULL == domain) {
				LOG("ERROR","------ ip=null ------\n\n");
				break;
			}
			platform = env->GetFieldID(cls, "accessWay", "Ljava/lang/String;");
			if (NULL == platform) {
				LOG("ERROR","------ platform=null ------\n\n");
			}
			protocol = env->GetFieldID(cls, "transportProtocol", "Ljava/lang/String;");
			if (NULL == protocol) {
				LOG("ERROR","------ protocol=null ------\n\n");
			}
			username = env->GetFieldID(cls, "sipUsername", "Ljava/lang/String;");
			if (NULL == username) {
				LOG("ERROR","------ username=null ------\n\n");
				break;
			}
			password = env->GetFieldID(cls, "sipPassword", "Ljava/lang/String;");
			if (NULL == password) {
				LOG("ERROR","------ password=null ------\n\n");
				break;
			}
			expires = env->GetFieldID(cls, "registerExpires", "I");
			if (NULL == expires) {
				LOG("ERROR","------ expires=null ------\n\n");
				break;
			}
			register_interval = env->GetFieldID(cls, "registerInerval", "I");
			if (NULL == register_interval) {
				LOG("ERROR","------ register_interval=null ------\n\n");
				break;
			}
			keepalive = env->GetFieldID(cls, "keepalive", "I");
			if (NULL == keepalive) {
				LOG("ERROR","------ keepalive=null ------\n\n");
				break;
			}
			#if 0
			max_k_timeout = env->GetFieldID(cls, "maxKTimeout", "I");
			if (NULL == max_k_timeout) {
				LOG("ERROR","------ max_k_timeout=null ------\n\n");
				break;
			}
			status = env->GetFieldID(cls, "registerStatus", "I");
			if (NULL == status) {
				LOG("ERROR","------ status=null ------\n\n");
			}
			#endif
			nRet = 0;
			break;
		}
		return nRet;
	}
}JniPlatform;

#endif