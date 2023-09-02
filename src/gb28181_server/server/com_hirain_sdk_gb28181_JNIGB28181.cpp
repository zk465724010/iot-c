
#include "com_hirain_sdk_gb28181_JNIGB28181.h"
#include "gb28181server_api.h"
#include "log.h"
#include "common.h"
#include "parse_xml.h"
#include "common_jni.h"
//#include "media.h"
#include "common_func.h"


#ifdef USE_PLATFORM_UNIS
#define USE_DEBUG_OUTPUT

extern callback_t g_callback;
extern local_cfg_t g_local_cfg;
//extern CMedia *g_media;
extern CMap<string, dev_info_t>	g_dev_map;
#ifdef USE_DEVICE_ACCESS
extern CMap<string, access_info_t> g_access_map;
#endif

JavaVM *g_java_vm = NULL;

void register_dev_callback(dev_info_t *dev, void *arg)
{
	LOG("INFO","------ register_dev_callback start '%s' ------\n\n", dev->addr.id);
	//LOG("INFO","Device '%s:%s:%d' [%s]\n\n", dev->addr.id, dev->addr.ip, dev->addr.port, ((0!=dev->status)?"register":"unregister"));
	print_device_info(dev);
	if ((NULL != dev) && (NULL != g_java_vm)) {
		JNIEnv *env = NULL;
		g_java_vm->AttachCurrentThread((void**)&env, NULL);
		if (NULL != env) {
			JniDeviceInfo jni_dev;
			int nRet = jni_dev.init(env, NULL);
			if (0 == nRet) {
				//DEBUG("INFO: device '%s:%s:%d'\n\n", dev->addr.id, dev->addr.ip, dev->addr.port);
				jni_dev.SetJStringField(jni_dev.id, dev->addr.id);
				if (strlen(dev->parent_id) < 5)
					LOG("ERROR","Device '%s' parent_id='%s'\n\n", dev->addr.id, dev->parent_id);
				jni_dev.SetJStringField(jni_dev.parent_id, dev->parent_id);
				jni_dev.SetJStringField(jni_dev.ip, dev->addr.ip);
				jni_dev.SetIntField(jni_dev.port, dev->addr.port);
				jni_dev.SetIntField(jni_dev.type, dev->type);
				jni_dev.SetIntField(jni_dev.protocol, dev->protocol);
				jni_dev.SetJStringField(jni_dev.username, dev->username);
				jni_dev.SetJStringField(jni_dev.password, dev->password);
				char *buff = new char[128];
				memset(buff, 0, 128);
				ansi_to_utf8(dev->name, strlen(dev->name), buff, 128);
				//DEBUG("INFO: DeviceName:%s\n\n", buff);
				jni_dev.SetJStringField(jni_dev.name, buff);
				delete buff;
				jni_dev.SetJStringField(jni_dev.manufacturer, dev->manufacturer);
				jni_dev.SetJStringField(jni_dev.firmware, dev->firmware);
				jni_dev.SetJStringField(jni_dev.model, dev->model);
				jni_dev.SetIntField(jni_dev.status, dev->status);
				jni_dev.SetIntField(jni_dev.expires, dev->expires);
				jni_dev.SetIntField(jni_dev.keepalive, dev->keepalive);
				jni_dev.SetIntField(jni_dev.max_k_timeout, dev->max_k_timeout);
				jni_dev.SetIntField(jni_dev.channels, dev->chl_map.size());
				#if 1
				if (dev->chl_map.size() > 0) { // 通道信息
					JniArrayList jni_array_list;
					nRet = jni_array_list.init(env, NULL);
					if (0 == nRet) {
						auto chl = dev->chl_map.begin();
						for (; chl != dev->chl_map.end(); ++chl) {
							JniChannelInfo	jni_chl;
							nRet = jni_chl.init(env, NULL);
							if (0 == nRet) {
								jni_chl.SetJStringField(jni_chl.id, chl->second.id);
								jni_chl.SetJStringField(jni_chl.parent_id, chl->second.parent_id);
								char *channel_name = new char[128];
								memset(channel_name, 0, 128);
								ansi_to_utf8(chl->second.name, strlen(chl->second.name), channel_name, 128);
								//DEBUG("INFO: ChannelName:%s\n\n", channel_name);
								jni_chl.SetJStringField(jni_chl.name, channel_name);
								delete channel_name;
								jni_chl.SetJStringField(jni_chl.manufacturer, chl->second.manufacturer);
								jni_chl.SetJStringField(jni_chl.model, chl->second.model);
								jni_chl.SetJStringField(jni_chl.owner, chl->second.owner);
								jni_chl.SetJStringField(jni_chl.civil_code, chl->second.civil_code);
								jni_chl.SetJStringField(jni_chl.address, chl->second.address);
								jni_chl.SetJStringField(jni_chl.ip_address, chl->second.ip_address);
								jni_chl.SetIntField(jni_chl.port, chl->second.port);
								char buff[64] = { 0 };
								sprintf(buff, "%d\0", chl->second.register_way);
								jni_chl.SetJStringField(jni_chl.register_way, buff);
								sprintf(buff, "%d\0", chl->second.safety_way);
								jni_chl.SetJStringField(jni_chl.safety_way, buff);
								sprintf(buff, "%d\0", chl->second.parental);
								jni_chl.SetJStringField(jni_chl.parental, buff);
								sprintf(buff, "%d\0", chl->second.secrecy);
								jni_chl.SetJStringField(jni_chl.secrecy, buff);
								sprintf(buff, "%d\0", chl->second.status);
								//jni_chl.SetJStringField(jni_chl.status, buff);
								jni_chl.SetIntField(jni_chl.status, chl->second.status);
								sprintf(buff, "%lf\0", chl->second.longitude);
								jni_chl.SetJStringField(jni_chl.longitude, buff);
								sprintf(buff, "%lf\0", chl->second.latitude);
								jni_chl.SetJStringField(jni_chl.latitude, buff);

								env->CallObjectMethod(jni_array_list.obj, jni_array_list.jadd, jni_chl.obj);
							}
							else LOG("ERROR", "JNI VideoGbdeviceinfo initialization failed.\n\n");
						}
						jni_dev.SetObjectField(jni_dev.chl_map, jni_array_list.obj);
					}
					else LOG("ERROR", "JNI ArrayList initialization failed.\n\n");
				}
				#endif
				JniDeviceinfoProviderImp jni_imp;
				nRet = jni_imp.init(env);
				if ((0 == nRet) && (NULL != jni_imp.obj) && (NULL != jni_imp.batchInsert) && (NULL != jni_dev.obj)) {
					jstring id = char_to_jstring(env, g_local_cfg.addr.id);
					jni_imp.env->CallObjectMethod(jni_imp.obj, jni_imp.batchInsert, jni_dev.obj, id);
				}
				else LOG("ERROR","ret:%d,jni_imp.obj:%p,batchInsert:%p,jni_dev.obj:%p\n\n", nRet, jni_imp.obj, jni_imp.batchInsert, jni_dev.obj);
			}
			else LOG("ERROR","JNI VideoGbdeviceinfo initialize failed.\n\n");
		}
		else LOG("ERROR","env=%p\n\n", env);
		g_java_vm->DetachCurrentThread();
	}
	else LOG("ERROR","dev=%p, g_java_vm=%p\n\n", dev, g_java_vm);
	LOG("INFO","------ register_dev_callback end ------\n\n");
}
void query_dev_callback(dev_info_t *dev, void *arg)
{
	LOG("INFO","------ query_dev_callback start ------\n\n");
	print_device_info(dev);
	if ((NULL != dev) && (NULL != g_java_vm)) {
		JNIEnv *env = NULL;
		g_java_vm->AttachCurrentThread((void**)&env, NULL);
		if (NULL != env) {
			JniDeviceInfo	jni_dev;
			int nRet = jni_dev.init(env, NULL);
			if (0 == nRet) {
				set_jstring_field(env, jni_dev.obj, jni_dev.id, dev->addr.id, "deviceId");
				set_jstring_field(env, jni_dev.obj, jni_dev.parent_id, dev->parent_id, "deviceParentId");
				set_jstring_field(env, jni_dev.obj, jni_dev.ip, dev->addr.ip, "deviceIp");
				set_int_field(env, jni_dev.obj, jni_dev.port, dev->addr.port, "devicePort");
				set_int_field(env, jni_dev.obj, jni_dev.type, dev->type, "deviceType");
				set_int_field(env, jni_dev.obj, jni_dev.protocol, dev->protocol, "protocol");
				set_jstring_field(env, jni_dev.obj, jni_dev.username, dev->username, "username");
				set_jstring_field(env, jni_dev.obj, jni_dev.password, dev->password, "password");
				if (strlen(dev->name) > 0) {
					char *buff = new char[128];
					memset(buff, 0, 128);
					ansi_to_utf8(dev->name, strlen(dev->name), buff, 128);
					set_jstring_field(env, jni_dev.obj, jni_dev.name, buff, "deviceName");
					delete buff;
				}
				else set_jstring_field(env, jni_dev.obj, jni_dev.name, dev->addr.id, "deviceName");
				set_jstring_field(env, jni_dev.obj, jni_dev.manufacturer, dev->manufacturer, "manufacturer");
				set_jstring_field(env, jni_dev.obj, jni_dev.firmware, dev->firmware, "firmware");
				set_jstring_field(env, jni_dev.obj, jni_dev.model, dev->model, "model");
				set_int_field(env, jni_dev.obj, jni_dev.status, dev->status, "status");
				set_int_field(env, jni_dev.obj, jni_dev.expires, dev->expires, "expires");
				set_int_field(env, jni_dev.obj, jni_dev.keepalive, dev->keepalive, "keepalive");
				set_int_field(env, jni_dev.obj, jni_dev.max_k_timeout, dev->max_k_timeout, "max_k_timeout");
				set_int_field(env, jni_dev.obj, jni_dev.channels, dev->chl_map.size(), "channels");
				#if 1
				if (dev->chl_map.size() > 0) { // 通道信息
					JniArrayList jni_array_list;
					nRet = jni_array_list.init(env, NULL);
					if (0 == nRet) {
						auto chl = dev->chl_map.begin();
						for (; chl != dev->chl_map.end(); ++chl) {
							JniChannelInfo	jni_chl;
							nRet = jni_chl.init(env, NULL);
							if (0 == nRet) {
								set_jstring_field(env, jni_chl.obj, jni_chl.id, chl->second.id, "channelId");
								set_jstring_field(env, jni_chl.obj, jni_chl.parent_id, chl->second.parent_id, "channelParentId");
								if (strlen(chl->second.name) > 0) {
									char *channel_name = new char[128];
									memset(channel_name, 0, 128);
									ansi_to_utf8(chl->second.name, strlen(chl->second.name), channel_name, 128);
									set_jstring_field(env, jni_chl.obj, jni_chl.name, channel_name, "channelName");
									delete channel_name;
								}
								else set_jstring_field(env, jni_chl.obj, jni_chl.name, chl->second.id, "channelName");
								set_jstring_field(env, jni_chl.obj, jni_chl.manufacturer, chl->second.manufacturer, "manufacturer");
								set_jstring_field(env, jni_chl.obj, jni_chl.model, chl->second.model, "model");
								set_jstring_field(env, jni_chl.obj, jni_chl.owner, chl->second.owner, "owner");
								set_jstring_field(env, jni_chl.obj, jni_chl.civil_code, chl->second.civil_code, "civil_code");
								set_jstring_field(env, jni_chl.obj, jni_chl.address, chl->second.address, "address");
								set_jstring_field(env, jni_chl.obj, jni_chl.ip_address, chl->second.ip_address, "ip_address");
								set_int_field(env, jni_chl.obj, jni_chl.port, chl->second.port, "channelPort");

								char buff[64] = { 0 };
								sprintf(buff, "%d\0", chl->second.register_way);
								set_jstring_field(env, jni_chl.obj, jni_chl.register_way, buff, "register_way");
								sprintf(buff, "%d\0", chl->second.safety_way);
								set_jstring_field(env, jni_chl.obj, jni_chl.safety_way, buff, "safety_way");
								sprintf(buff, "%d\0", chl->second.parental);
								set_jstring_field(env, jni_chl.obj, jni_chl.parental, buff, "parental");
								sprintf(buff, "%d\0", chl->second.secrecy);
								set_jstring_field(env, jni_chl.obj, jni_chl.secrecy, buff, "secrecy");
								//sprintf(buff, "%d\0", chl->second.status);
								//set_jstring_field(env, jni_chl.obj, jni_chl.status, buff, "status");
								set_int_field(env, jni_chl.obj, jni_chl.status, chl->second.status, "status");
								sprintf(buff, "%lf\0", chl->second.longitude);
								set_jstring_field(env, jni_chl.obj, jni_chl.longitude, buff, "longitude");
								sprintf(buff, "%lf\0", chl->second.latitude);
								set_jstring_field(env, jni_chl.obj, jni_chl.latitude, buff, "latitude");

								env->CallObjectMethod(jni_array_list.obj, jni_array_list.jadd, jni_chl.obj);
							}
							else LOG("ERROR", "JNI VideoGbdeviceinfo initialize failed!\n\n");
						}
						jni_dev.SetObjectField(jni_dev.chl_map, jni_array_list.obj);
					}
					else LOG("ERROR", "JNI ArrayList initialize failed!\n\n");
				}
				#endif

				JniDeviceinfoProviderImp jni_imp;
				int nRet = jni_imp.init(env);
				if (0 == nRet) {
					jni_imp.env->CallObjectMethod(jni_imp.obj, jni_imp.QueryDevice, jni_dev.obj);
				}
				else LOG("ERROR","JniDeviceinfoProviderImp initialize failed %d\n\n", nRet);
			}
			else LOG("ERROR","JNI VideoGbdeviceinfo initialize failed.\n\n");
		}
		else LOG("ERROR","env=%p\n\n", env);
		g_java_vm->DetachCurrentThread();
	}
	else LOG("ERROR","dev=%p, g_java_vm=%p\n\n", dev, g_java_vm);
	LOG("INFO","------ query_dev_callback end ------\n\n");
}
void keepalive_callback(dev_info_t *dev, void *arg)
{
	//LOG("INFO","------ keepalive_callback ------\n\n");
	LOG("INFO","Device '%s' [%s]\n\n", dev->addr.id, ((0 != dev->status) ? "Online" : "Offline"));
	query_dev_callback(dev, arg);
}

void control_callback(const char *id, int status, void *arg)
{
	LOG("INFO","------ control_callback ------\n\n");
	//DEBUG("控制设备'%s' [%s]\n\n", id, ((0 == status) ? "成功" : "失败"));
}

// 查询录像回调
int query_record_callback(const char *dev_id, map<string, record_info_t> *rec_map)
{
	int nRet = -1;
	if ((NULL != dev_id) && ((NULL != rec_map)) && (NULL != g_java_vm)) {
		JNIEnv *env = NULL;
		g_java_vm->AttachCurrentThread((void**)&env, NULL);
		if (NULL != env) {
			JniDeviceinfoProviderImp jni_imp;
			nRet = jni_imp.init(env);
			if ((0 == nRet) && (NULL != jni_imp.QueryRecord)) {
				JniGBRecord jni_rec;
				nRet = jni_rec.init(env, NULL);
				if (0 == nRet) {
					nRet = -1;
					auto rec = rec_map->begin();
					if (rec != rec_map->end()) {
						jstring jdev_id = char_to_jstring(env, dev_id);
						set_object_field(env, jni_rec.obj, jni_rec.dev_id, jdev_id, "dev_id");
						jstring chl_id = char_to_jstring(env, rec->second.id);
						set_object_field(env, jni_rec.obj, jni_rec.chl_id, chl_id, "chl_id");
						jstring start_time = char_to_jstring(env, rec->second.start_time);
						set_object_field(env, jni_rec.obj, jni_rec.start_time, start_time, "start_time");
						jstring end_time = char_to_jstring(env, rec->second.end_time);
						set_object_field(env, jni_rec.obj, jni_rec.end_time, end_time, "end_time");
						jstring file_path = char_to_jstring(env, rec->second.file_path);
						set_object_field(env, jni_rec.obj, jni_rec.file_path, file_path, "file_path");
						jstring address = char_to_jstring(env, rec->second.address);
						set_object_field(env, jni_rec.obj, jni_rec.address, address, "address");
						set_int_field(env, jni_rec.obj, jni_rec.secrecy, (int)rec->second.secrecy, "secrecy");
						char szType[16] = {0};
						sprintf(szType, "%d", rec->second.type);
						jstring type = char_to_jstring(env, szType);
						set_object_field(env, jni_rec.obj, jni_rec.type, type, "type");
						jstring rec_id = char_to_jstring(env, rec->second.recorder_id);
						set_object_field(env, jni_rec.obj, jni_rec.rec_id, rec_id, "rec_id");
						jstring name = char_to_jstring(env, rec->second.name);
						set_object_field(env, jni_rec.obj, jni_rec.name, name, "name");

						jobject jni_obj = jni_imp.env->CallObjectMethod(jni_imp.obj, jni_imp.QueryRecord, jni_rec.obj);
						rec_map->clear();
						if (NULL != jni_obj) {
							JniArrayList jni_rec_list;
							nRet = jni_rec_list.init(env, jni_obj);
							if (0 == nRet) {
								int size = jni_rec_list.size();
								if(size < 1)
									LOG("INFO","Query record list size %d\n\n", size);
								for (int i = 0; i < size; i++) {
									jobject jobj_rec = jni_rec_list.get(i);
									if (NULL == jobj_rec) {
										LOG("ERROR","JniArrayList get(%d) failed\n\n", i);
										break;
									}
									JniGBRecord jni_rec;
									nRet = jni_rec.init(env, jobj_rec);
									if (0 != nRet) {
										LOG("ERROR","JniGBRecord init failed\n\n");
										break;
									}
									record_info_t rec;
									jstring_to_char(env, jni_rec.obj, jni_rec.chl_id, rec.id, sizeof(rec.id), "id");
									jstring_to_char(env, jni_rec.obj, jni_rec.start_time, rec.start_time, sizeof(rec.start_time), "start_time");
									jstring_to_char(env, jni_rec.obj, jni_rec.end_time, rec.end_time, sizeof(rec.end_time), "end_time");
									jstring_to_char(env, jni_rec.obj, jni_rec.file_path, rec.file_path, sizeof(rec.file_path), "file_path");
									jstring_to_char(env, jni_rec.obj, jni_rec.address, rec.address, sizeof(rec.address), "address");
									rec.secrecy = jint_to_int(env, jni_rec.obj, jni_rec.secrecy, "secrecy");
									char type[16] = {0};
									jstring_to_char(env, jni_rec.obj, jni_rec.type, type, sizeof(type), "type");
									rec.type = atoi(type);
									jstring_to_char(env, jni_rec.obj, jni_rec.rec_id, rec.recorder_id, sizeof(rec.recorder_id), "recorder_id");
									jstring_to_char(env, jni_rec.obj, jni_rec.name, rec.name, sizeof(rec.name), "name");
									rec.size = jint_to_int(env, jni_rec.obj, jni_rec.size, "size");

									//DEBUG("INFO: Query record '%s'\n", rec.file_path);
									rec_map->insert(map<string, record_info_t>::value_type(rec.start_time, rec));
								}
								nRet = rec_map->size();
							}
							else LOG("ERROR","JniArrayList init failed %d\n\n", nRet);
						}
						else LOG("ERROR","Query database failed [record_list=%p]\n\n", jni_obj);
					}
					else LOG("ERROR","record size %d\n\n", rec_map->size());
				}
				else LOG("ERROR","JniGBRecord init failed %d\n\n", nRet);
			}
			else LOG("ERROR","ret=%d, QueryRecord=%p\n\n", nRet, jni_imp.QueryRecord);
		}
		else LOG("ERROR","env=%p\n\n", env);
	}
	else LOG("ERROR","dev_id=%p, rec_map=%p, g_java_vm=%p\n\n", dev_id, rec_map, g_java_vm);
	return nRet;
}

/*
 * Class:     com_hirain_sdk_gb28181_JNIGB28181
 * Method:    open
 * Signature: (Lcom/hirain/sdk/entity/gb28181/LocalConfig;)I
 */
JNIEXPORT jint JNICALL Java_com_hirain_sdk_gb28181_JNIGB28181_open(JNIEnv *env, jobject obj, jobject cfg)
{
	LOG("INFO","------ open start ------\n\n");
	if ((NULL == env) || (NULL == obj) || (NULL == cfg))
	{
		LOG("ERROR","env=%p, obj=%p, cfg=%p\n\n", env, obj, cfg);
		LOG("ERROR","------ open end ------\n\n");
		return -1;
	}
#if 0
	// JNI多线程,获取Java虚拟机,重新获取JNIEnv
	JNIEnv* jni_env = NULL;
	JavaVM* java_vm = NULL;
	env->GetJavaVM(&java_vm);
	if (NULL != java_vm) {
		java_vm->AttachCurrentThread((void**)&jni_env, NULL);
		if (NULL == jni_env) {
			if (env->ExceptionOccurred())
				env->ExceptionDescribe();
		}
	}
	else {
		if (env->ExceptionOccurred())
			env->ExceptionDescribe();
	}
	if (NULL != java_vm)
		java_vm->DetachCurrentThread();	//销毁虚拟机
#else
	if (NULL == g_java_vm) {
		env->GetJavaVM(&g_java_vm);		// 获取Java虚拟机
		if (NULL == g_java_vm) {
			LOG("ERROR", "GetJavaVM error\n\n");
			return -2;
		}
	}
#endif

	int nRet = -1;
	jclass cls = env->GetObjectClass(cfg);
	if (NULL != cls) {
		jfieldID id = env->GetFieldID(cls, "localSipId", "Ljava/lang/String;");
		if (NULL != id) {
			jstring_to_char(env, cfg, id, g_local_cfg.addr.id, sizeof(g_local_cfg.addr.id), "id");
			strcpy(g_local_cfg.dev.addr.id, g_local_cfg.addr.id);	// ID2
		}
		else LOG("ERROR", "------ id=null ------\n\n");
		jfieldID ip = env->GetFieldID(cls, "localSipIp", "Ljava/lang/String;");
		if (NULL != ip) {
			jstring_to_char(env, cfg, ip, g_local_cfg.addr.ip, sizeof(g_local_cfg.addr.ip), "ip");
			strcpy(g_local_cfg.dev.addr.ip, g_local_cfg.addr.ip);	// IP2
		}
		else LOG("ERROR", "------ ip=null ------\n\n");
		jfieldID port = env->GetFieldID(cls, "localSipPort", "I");
		if (NULL != port) {
			g_local_cfg.addr.port = env->GetIntField(cfg, port);
			g_local_cfg.dev.addr.port = g_local_cfg.addr.port + 1;	// Port2
		}
		else LOG("ERROR", "------ port=null ------\n\n");
		#if 0
		jfieldID ip2 = env->GetFieldID(cls, "localSipIpTwo", "Ljava/lang/String;");
		if (NULL != ip2) {
			jstring_to_char(env, cfg, ip2, g_local_cfg.dev.addr.ip, sizeof(g_local_cfg.dev.addr.ip), "ip2");
		}
		else LOG("ERROR", "------ ip2=null ------\n\n");
		jfieldID port2 = env->GetFieldID(cls, "localSipPortTwo", "I");
		if (NULL != port2) {
			g_local_cfg.dev.addr.port = env->GetIntField(cfg, port2);
		}
		else LOG("ERROR", "------ port2=null ------\n\n");
		#endif
		jfieldID domain = env->GetFieldID(cls, "localSipDomain", "Ljava/lang/String;");
		if (NULL != domain) {
			jstring_to_char(env, cfg, domain, g_local_cfg.domain, sizeof(g_local_cfg.domain), "domain");
		}
		else LOG("ERROR", "------ domain=null ------\n\n");
		jfieldID password = env->GetFieldID(cls, "localSipPassword", "Ljava/lang/String;");
		if (NULL != password) {
			jstring_to_char(env, cfg, password, g_local_cfg.password, sizeof(g_local_cfg.password), "password");
		}
		else LOG("ERROR", "------ password=null ------\n\n");
		jfieldID username = env->GetFieldID(cls, "localSipUsername", "Ljava/lang/String;");
		if (NULL != username) {
			int ret = jstring_to_char(env, cfg, username, g_local_cfg.dev.username, sizeof(g_local_cfg.dev.username), "username");
			if (0 != ret)
				strcpy(g_local_cfg.dev.username, g_local_cfg.dev.addr.id);
		}
		else LOG("ERROR", "------ username=null ------\n\n");
		jfieldID manufacturer = env->GetFieldID(cls, "manufacturer", "Ljava/lang/String;");
		if (NULL != manufacturer) {
			int ret = jstring_to_char(env, cfg, manufacturer, g_local_cfg.dev.manufacturer, sizeof(g_local_cfg.dev.manufacturer), "manufacturer");
			if (0 != ret)
				strcpy(g_local_cfg.dev.manufacturer, "UniStrong");
		}
		else LOG("ERROR", "------ manufacturer=null ------\n\n");
		jfieldID firmware = env->GetFieldID(cls, "firmware", "Ljava/lang/String;");
		if (NULL != firmware) {
			int ret = jstring_to_char(env, cfg, firmware, g_local_cfg.dev.firmware, sizeof(g_local_cfg.dev.firmware), "firmware");
			if (0 != ret)
				strcpy(g_local_cfg.dev.firmware, "V1.0.0");
		}
		else LOG("ERROR", "------ firmware=null ------\n\n");
		jfieldID model = env->GetFieldID(cls, "model", "Ljava/lang/String;");
		if (NULL != model) {
			int ret = jstring_to_char(env, cfg, model, g_local_cfg.dev.model, sizeof(g_local_cfg.dev.model), "model");
			if (0 != ret)
				strcpy(g_local_cfg.dev.model, "UNIS-2021");
		}
		else LOG("ERROR", "------ model=null ------\n\n");
		jfieldID expires = env->GetFieldID(cls, "expires", "I");
		if (NULL != expires) {
			g_local_cfg.dev.expires = env->GetIntField(cfg, expires);
			g_local_cfg.dev.expires = (g_local_cfg.dev.expires > 0) ? g_local_cfg.dev.expires : 3600;
		}
		else LOG("ERROR", "------ expires=null ------\n\n");
		jfieldID register_interval = env->GetFieldID(cls, "registerInterval", "I");
		if (NULL != register_interval) {
			g_local_cfg.dev.register_interval = env->GetIntField(cfg, register_interval);
			g_local_cfg.dev.register_interval = (g_local_cfg.dev.register_interval > 0) ? g_local_cfg.dev.register_interval : 60;
		}
		else LOG("ERROR", "------ register_interval=null ------\n\n");
		jfieldID keepalive = env->GetFieldID(cls, "keepalive", "I");
		if (NULL != keepalive) {
			g_local_cfg.dev.keepalive = env->GetIntField(cfg, keepalive);
			g_local_cfg.dev.keepalive = (g_local_cfg.dev.keepalive >= 5) ? g_local_cfg.dev.keepalive : 30;
		}
		else LOG("ERROR", "------ keepalive=null ------\n\n");
		jfieldID max_timeout = env->GetFieldID(cls, "maxTimeout", "I");
		if (NULL != max_timeout) {
			g_local_cfg.dev.max_k_timeout = env->GetIntField(cfg, max_timeout);
			g_local_cfg.dev.max_k_timeout = (g_local_cfg.dev.max_k_timeout > 0) ? g_local_cfg.dev.max_k_timeout : 3;
		}
		else LOG("ERROR", "------ max_timeout=null ------\n\n");
		nRet = 0;
	}
	else LOG("ERROR", "------ GetObjectClass failed ------\n\n");
    if(0 == nRet) {
        nRet = unis_open(g_local_cfg.addr.ip, g_local_cfg.addr.port);
    }
	// 设置回调
	g_callback.registration = register_dev_callback;
	g_callback.registration2 = register_dev_callback;
	g_callback.query_catalog = query_dev_callback;
	g_callback.keepalive = keepalive_callback;
	g_callback.query_record = query_record_callback;

	LOG("INFO","------ open end [ret:%d] ------\n\n", nRet);
	return nRet;
}

/*
 * Class:     com_hirain_sdk_gb28181_JNIGB28181
 * Method:    close
 * Signature: ()I
 */
JNIEXPORT jint JNICALL Java_com_hirain_sdk_gb28181_JNIGB28181_close(JNIEnv* env, jobject obj)
{
	LOG("INFO","------ close start ------\n\n");
	if ((NULL == env) || (NULL == obj))
	{
		LOG("ERROR","env=%p, obj=%p, opt=%p\n\n", env, obj);
		LOG("ERROR","------ close end ------\n\n");
		return -1;
	}
	int nRet = unis_close();
	unis_uninit();
	LOG("INFO","------ close end %d ------\n\n", nRet);
	return nRet;
}

/*
 * Class:     com_hirain_sdk_gb28181_JNIGB28181
 * Method:    query
 * Signature: (Lcom/hirain/sdk/JNIEntity/Operation;)I
 */
JNIEXPORT jint JNICALL Java_com_hirain_sdk_gb28181_JNIGB28181_query(JNIEnv *env, jobject obj, jobject opt)
{
	LOG("INFO","------ query start ------\n\n");
	if ((NULL == env) || (NULL == obj) || (NULL == opt))
	{
		LOG("ERROR","env=%p, obj=%p, opt=%p\n\n", env, obj, opt);
		LOG("ERROR","------ query end ------\n\n");
		return -1;
	}
	addr_info_t dev;
	int cmd = 0, nRet = -1;
	do {
		jclass cls = (env)->GetObjectClass(opt);
		if (NULL == cls) {
			LOG("ERROR","GetObjectClass failed cls=%p\n\n", cls);
			break;
		}
		jfieldID jfield_id = (env)->GetFieldID(cls, "deviceId", "Ljava/lang/String;");
		int ret = jstring_to_char(env, opt, jfield_id, dev.id, sizeof(dev.id), "deviceId");
		if (0 != ret) {
			LOG("ERROR","Get device id failed %d\n\n", ret);
			break;
		}
		jfieldID jfield_ip = (env)->GetFieldID(cls, "deviceIp", "Ljava/lang/String;");
		ret = jstring_to_char(env, opt, jfield_ip, dev.ip, sizeof(dev.ip), "deviceIp");
		if (0 != ret) {
			LOG("ERROR","Get device ip failed %d\n\n", ret);
			break;
		}
		jfieldID jfield_port = (env)->GetFieldID(cls, "devicePort", "I");
		if (NULL == jfield_port) {
			LOG("ERROR","Get device port failed %p\n\n", jfield_port);
			break;
		}
		dev.port = (env)->GetIntField(opt, jfield_port);

		jfieldID jfield_cmd = (env)->GetFieldID(cls, "cmd", "I");
		if (NULL == jfield_cmd) {
			LOG("ERROR","Get cmd failed %p\n\n", jfield_cmd);
			break;
		}
		cmd = (env)->GetIntField(opt, jfield_cmd);
		nRet = 0;

	} while (false);

	if (0 == nRet) {
		operation_t dev_opt;
		switch (cmd)
		{
		case QUERY_DEV_INFO: dev_opt.cmd = QUERY_DEV_INFO; break;
		case QUERY_DEV_STATUS: dev_opt.cmd = QUERY_DEV_STATUS; break;
		case QUERY_DEV_CATALOG: dev_opt.cmd = QUERY_DEV_CATALOG; break;
		case QUERY_DEV_RECORD: dev_opt.cmd = QUERY_DEV_RECORD; break;
		case QUERY_DEV_CFG: dev_opt.cmd = QUERY_DEV_CFG; break;
		case QUERY_DEV_ALARM: dev_opt.cmd = QUERY_DEV_ALARM; break;
		case QUERY_DEV_PRESET: dev_opt.cmd = QUERY_DEV_PRESET; break;
		case QUERY_DEV_MP: dev_opt.cmd = QUERY_DEV_MP; break;
		//default : break;
		}
		g_callback.query = query_dev_callback;
		//g_callback.query = register_dev_callback;
		nRet = unis_query(&dev, &dev_opt);
	}
	else LOG("ERROR","ret=%d\n\n", nRet);
	
	LOG("INFO","------ query end [ret:%d] ------\n\n", nRet);
	return nRet;
}

/*
 * Class:     com_hirain_sdk_gb28181_JNIGB28181
 * Method:    control
 * Signature: (Lcom/hirain/sdk/JNIEntity/Operation;)I
 */
JNIEXPORT jint JNICALL Java_com_hirain_sdk_gb28181_JNIGB28181_control(JNIEnv *env, jobject obj, jobject opt)
{
	LOG("INFO","------ control start ------\n\n");
	if ((NULL == env) || (NULL == obj) || (NULL == opt))
	{
		LOG("ERROR","env=%p, obj=%p, opt=%p\n\n", env, obj, opt);
		LOG("ERROR","------ control end ------\n\n");
		return -1;
	}

	jclass objectClass = env->GetObjectClass(opt);  //opt即传递进来的类的对象
    //取得该String属性的ID
    jfieldID jfield_id = env->GetFieldID(objectClass,"deviceId","Ljava/lang/String;");
    jfieldID jfield_ip = env->GetFieldID(objectClass,"deviceIp","Ljava/lang/String;");
    jfieldID jfield_port = env->GetFieldID(objectClass,"devicePort","I");
    jfieldID jfield_cmd = env->GetFieldID(objectClass,"cmd","I");
	
	addr_info_t dev;

	jstring_to_char(env, opt, jfield_id, dev.id, sizeof(dev.id), "deviceId");
	jstring_to_char(env, opt, jfield_ip, dev.ip, sizeof(dev.ip), "deviceIp");
    if(NULL != jfield_port) {
        dev.port = env->GetIntField(opt, jfield_port);
    }

	int cmd = 0;
    if(NULL != jfield_cmd) {
        cmd = (env)->GetIntField(opt, jfield_cmd);
    }

	LOG("INFO","cmd:%d '%s:%s:%d'\n\n", cmd, dev.id, dev.ip, dev.port);

	operation_t dev_opt;
	switch(cmd)
	{
		case CTRL_PTZ_LEFT: dev_opt.cmd = CTRL_PTZ_LEFT; break;
		case CTRL_PTZ_RIGHT: dev_opt.cmd = CTRL_PTZ_RIGHT; break;
		case CTRL_PTZ_UP: dev_opt.cmd = CTRL_PTZ_UP; break;
		case CTRL_PTZ_DOWN: dev_opt.cmd = CTRL_PTZ_DOWN; break;
		case CTRL_PTZ_LARGE: dev_opt.cmd = CTRL_PTZ_LARGE; break;
		case CTRL_PTZ_SMALL: dev_opt.cmd = CTRL_PTZ_SMALL; break;
		case CTRL_PTZ_STOP: dev_opt.cmd = CTRL_PTZ_STOP; break;
		case CTRL_REC_START: dev_opt.cmd = CTRL_REC_START; break;
		case CTRL_REC_STOP: dev_opt.cmd = CTRL_REC_STOP; break;
		case CTRL_GUD_START: dev_opt.cmd = CTRL_GUD_START; break;
		case CTRL_GUD_STOP: dev_opt.cmd = CTRL_GUD_STOP; break;
		case CTRL_ALM_RESET: dev_opt.cmd = CTRL_ALM_RESET; break;
		case CTRL_TEL_BOOT: dev_opt.cmd = CTRL_TEL_BOOT; break;
		case CTRL_IFR_SEND: dev_opt.cmd = CTRL_IFR_SEND; break;
		case CTRL_DEV_CFG: dev_opt.cmd = CTRL_DEV_CFG; break;
		default : LOG("ERROR","------ Unknown command [cmd:%d] ------\n\n", cmd);
	}
	int nRet = 0;
	nRet = unis_control(&dev,  &dev_opt);

	if (NULL != dev_opt.unis_record)
		delete dev_opt.unis_record;

	LOG("INFO","------ control end [ret:%d] ------\n\n", nRet);
	return nRet;
}

/*
 * Class:     com_hirain_sdk_gb28181_JNIGB28181
 * Method:    addPlatform
 * Signature: (Ljava/lang/Object;)I
 */
JNIEXPORT jint JNICALL Java_com_hirain_sdk_gb28181_JNIGB28181_addPlatform(JNIEnv *env, jobject obj, jobject platform)
{
	LOG("INFO","------ add platform start ------\n\n");
	int nRet = -1;
	pfm_info_t pfm;

	JniPlatform jni_pfm;
	nRet = jni_pfm.init(env, platform);
	if (0 == nRet) {
		pfm.enable = jint_to_int(env, platform, jni_pfm.enable, "enable") > 0 ? 1 : 0;
		jstring_to_char(env, platform, jni_pfm.id, pfm.addr.id, sizeof(pfm.addr.id), "id");
		jstring_to_char(env, platform, jni_pfm.ip, pfm.addr.ip, sizeof(pfm.addr.ip), "ip");
		pfm.addr.port = jint_to_int(env, platform, jni_pfm.port, "port");
		jstring_to_char(env, platform, jni_pfm.domain, pfm.domain, sizeof(pfm.domain), "domain");
		jstring_to_char(env, platform, jni_pfm.platform, pfm.platform, sizeof(pfm.platform), "platform");
		jstring_to_char(env, platform, jni_pfm.protocol, pfm.protocol, sizeof(pfm.protocol), "protocol");
		jstring_to_char(env, platform, jni_pfm.username, pfm.username, sizeof(pfm.username), "username");
		jstring_to_char(env, platform, jni_pfm.password, pfm.password, sizeof(pfm.password), "password");
		pfm.expires = jint_to_int(env, platform, jni_pfm.expires, "expires");
		pfm.expires = pfm.expires > 0 ? pfm.expires : 3600;
		pfm.register_interval = jint_to_int(env, platform, jni_pfm.register_interval, "register_interval");
		pfm.register_interval = pfm.register_interval > 0 ? pfm.register_interval : 3600;
		pfm.keepalive = jint_to_int(env, platform, jni_pfm.keepalive, "keepalive");
		pfm.keepalive = pfm.keepalive > 0 ? pfm.keepalive : 3600;
		print_platform_info(&pfm);
		nRet = add_platform(&pfm);
	}
	else LOG("ERROR","JniPlatform init failed %d \n\n", nRet);

	LOG("INFO","------ add platform end %d ------\n\n", nRet);
	return nRet;
}

/*
 * Class:     com_hirain_sdk_gb28181_JNIGB28181
 * Method:    delPlatform
 * Signature: (Ljava/lang/String;)I
 */
JNIEXPORT jint JNICALL Java_com_hirain_sdk_gb28181_JNIGB28181_delPlatform(JNIEnv *env, jobject obj, jstring jpfm_id)
{
	LOG("INFO","------ del platform start ------\n\n");

	int nRet = -1;
	char pfm_id[32] = {0};
	jstring_to_char(env, jpfm_id, pfm_id, sizeof(pfm_id), "pfm_id");
	if(strlen(pfm_id) > 0) {
		nRet = del_platform(pfm_id);
	}
	LOG("INFO","------ del platform end %d ------\n\n", nRet);
	return nRet;
}

/*
 * Class:     com_hirain_sdk_gb28181_JNIGB28181
 * Method:    updatePlatform
 * Signature: (Ljava/lang/Object;)I
 */
JNIEXPORT jint JNICALL Java_com_hirain_sdk_gb28181_JNIGB28181_updatePlatform(JNIEnv *env, jobject obj, jobject platform)
{
	LOG("INFO","------ update platform start ------\n\n");
	int nRet = -1;
	pfm_info_t pfm;
	JniPlatform jni_pfm;
	nRet = jni_pfm.init(env, platform);
	if (0 == nRet) {
		pfm.enable = jint_to_int(env, platform, jni_pfm.enable, "enable") > 0 ? 1 : 0;
		jstring_to_char(env, platform, jni_pfm.id, pfm.addr.id, sizeof(pfm.addr.id), "id");
		jstring_to_char(env, platform, jni_pfm.ip, pfm.addr.ip, sizeof(pfm.addr.ip), "ip");
		pfm.addr.port = jint_to_int(env, platform, jni_pfm.port, "port");
		jstring_to_char(env, platform, jni_pfm.domain, pfm.domain, sizeof(pfm.domain), "domain");
		jstring_to_char(env, platform, jni_pfm.platform, pfm.platform, sizeof(pfm.platform), "platform");
		jstring_to_char(env, platform, jni_pfm.protocol, pfm.protocol, sizeof(pfm.protocol), "protocol");
		jstring_to_char(env, platform, jni_pfm.username, pfm.username, sizeof(pfm.username), "username");
		jstring_to_char(env, platform, jni_pfm.password, pfm.password, sizeof(pfm.password), "password");
		pfm.expires = jint_to_int(env, platform, jni_pfm.expires, "expires");
		pfm.expires = pfm.expires > 0 ? pfm.expires : 3600;
		pfm.register_interval = jint_to_int(env, platform, jni_pfm.register_interval, "register_interval");
		pfm.register_interval = pfm.register_interval > 0 ? pfm.register_interval : 3600;
		pfm.keepalive = jint_to_int(env, platform, jni_pfm.keepalive, "keepalive");
		pfm.keepalive = pfm.keepalive > 0 ? pfm.keepalive : 3600;
		print_platform_info(&pfm);
		nRet = update_platform(&pfm);
	}
	else LOG("ERROR","JniPlatform init failed %d \n\n", nRet);
	LOG("INFO","------ update platform end %d ------\n\n", nRet);
	return nRet;
}

/*
 * Class:     com_hirain_sdk_gb28181_JNIGB28181
 * Method:    addDevice
 * Signature: (Ljava/util/List;)I
 */
JNIEXPORT jint JNICALL Java_com_hirain_sdk_gb28181_JNIGB28181_addDevice(JNIEnv *env, jobject obj, jobject list)
{
	LOG("INFO","---------------- addDevice start ------------------\n\n");
	int nRet = -1;
	if ((NULL == env) || (NULL == obj) || (NULL == list)) {
		LOG("ERROR","env=%p, obj=%p, list=%p\n\n", env, obj, list);
		return nRet;
	}
	JniArrayList jni_dev_list;
	nRet = jni_dev_list.init(env, list);
	if (0 == nRet) {
		int size = jni_dev_list.size();
		LOG("INFO","Device list size %d\n\n", size);
		for (int i = 0; i < size; i++) {
			dev_info_t dev;
			jobject jobj_dev = jni_dev_list.get(i);
			if (NULL == jobj_dev) {
				LOG("ERROR","JniArrayList get failed\n\n");
				continue;
			}
			JniDeviceInfo jni_dev;
			nRet = jni_dev.init(env, jobj_dev);
			if (0 != nRet) {
				LOG("ERROR","JniDeviceInfo init failed\n\n");
				continue;
			}
			jstring_to_char(env, jni_dev.obj, jni_dev.id, dev.addr.id, sizeof(dev.addr.id), "deviceId");
			jstring_to_char(env, jni_dev.obj, jni_dev.parent_id, dev.parent_id, sizeof(dev.parent_id), "parentId");
			jstring_to_char(env, jni_dev.obj, jni_dev.ip, dev.addr.ip, sizeof(dev.addr.ip), "deviceIp");
			dev.addr.port = jint_to_int(env, jni_dev.obj, jni_dev.port, "devicePort");
			dev.type = jint_to_int(env, jni_dev.obj, jni_dev.type, "deviceType");
			dev.protocol = jint_to_int(env, jni_dev.obj, jni_dev.protocol, "protocol");
			jstring_to_char(env, jni_dev.obj, jni_dev.username, dev.username, sizeof(dev.username), "username");
			jstring_to_char(env, jni_dev.obj, jni_dev.password, dev.password, sizeof(dev.password), "password");
			
			jstring_to_char(env, jni_dev.obj, jni_dev.name, dev.name, sizeof(dev.name), "name");
			LOG("INFO", "1 dev.name=%s\n\n", dev.name);
			char *buff = new char[128];
			memset(buff, 0, 128);
			int n = utf8_to_ansi(dev.name, strlen(dev.name), buff, 128);
			if (n > 0) {
				memset(dev.name, 0, sizeof(dev.name));
				strcpy(dev.name, buff);
			}
			else LOG("ERROR", "utf8_to_ansi failed %d\n\n", n);
			delete[] buff;
			LOG("INFO", "2 dev.name=%s\n\n", dev.name);

			jstring_to_char(env, jni_dev.obj, jni_dev.manufacturer, dev.manufacturer, sizeof(dev.manufacturer), "manufacturer");
			jstring_to_char(env, jni_dev.obj, jni_dev.firmware, dev.firmware, sizeof(dev.firmware), "firmware");
			jstring_to_char(env, jni_dev.obj, jni_dev.model, dev.model, sizeof(dev.model), "model");
			//dev.status = jint_to_int(env, jni_dev.obj, jni_dev.status, "status");
			dev.status = 0;
			dev.expires = jint_to_int(env, jni_dev.obj, jni_dev.expires, "expires");
			dev.keepalive = jint_to_int(env, jni_dev.obj, jni_dev.keepalive, "keepalive");
			dev.max_k_timeout = jint_to_int(env, jni_dev.obj, jni_dev.max_k_timeout, "max_k_timeout");
			dev.channels = jint_to_int(env, jni_dev.obj, jni_dev.channels, "channels");
			//dev.keepalive_flag = dev.keepalive * dev.max_k_timeout;
			//DEBUG("INFO: dev%d: %s:%s:%d\n\n", i+1, dev.addr.id, dev.addr.ip, dev.addr.port);
			#if 1
			jobject chl_map = get_object_field(env, jni_dev.obj, jni_dev.chl_map, "chl_map");
			if (NULL != chl_map) {
				JniArrayList jni_chl_list;
				nRet = jni_chl_list.init(env, chl_map);
				if (0 == nRet) {
					int chl_size = jni_chl_list.size();
					LOG("INFO","Device '%s' channel size %d\n\n", dev.addr.id, chl_size);

					for (int j = 0; j < chl_size; j++) {
						LOG("INFO","Device '%s' channel[%d]\n\n", dev.addr.id, j);

						jobject jobj_chl = jni_chl_list.get(j);
						if (NULL == jobj_chl) {
							LOG("ERROR","JniArrayList get failed\n\n");
							continue;
						}
						JniChannelInfo jni_chl;
						nRet = jni_chl.init(env, jobj_chl);
						if (0 != nRet) {
							LOG("ERROR","JniChannelInfo init failed %d\n\n", nRet);
							continue;
						}
						chl_info_t chl;
						jstring_to_char(env, jni_chl.obj, jni_chl.id, chl.id, sizeof(chl.id), "ChannelID");
						LOG("INFO","Device '%s' channel[%d] '%s'\n\n", dev.addr.id, j, chl.id);

						jstring_to_char(env, jni_chl.obj, jni_chl.parent_id, chl.parent_id, sizeof(chl.parent_id), "parent_id");
						jstring_to_char(env, jni_chl.obj, jni_chl.name, chl.name, sizeof(chl.name), "ChannelName");
						LOG("INFO", "1 chl.name=%s\n\n", chl.name);
						char *buff = new char[128];
						memset(buff, 0, 128);
						int n = utf8_to_ansi(chl.name, strlen(chl.name), buff, 128);
						if (n > 0) {
							memset(chl.name, 0, sizeof(chl.name));
							strcpy(chl.name, buff);
						}
						else LOG("ERROR", "utf8_to_ansi failed %d\n\n", n);
						delete[] buff;
						LOG("INFO", "2 chl.name=%s\n\n", chl.name);

						jstring_to_char(env, jni_chl.obj, jni_chl.manufacturer, chl.manufacturer, sizeof(chl.manufacturer), "manufacturer");
						jstring_to_char(env, jni_chl.obj, jni_chl.model, chl.model, sizeof(chl.model), "model");
						jstring_to_char(env, jni_chl.obj, jni_chl.owner, chl.owner, sizeof(chl.owner), "owner");
						jstring_to_char(env, jni_chl.obj, jni_chl.civil_code, chl.civil_code, sizeof(chl.civil_code), "civil_code");
						jstring_to_char(env, jni_chl.obj, jni_chl.address, chl.address, sizeof(chl.address), "address");
						jstring_to_char(env, jni_chl.obj, jni_chl.ip_address, chl.ip_address, sizeof(chl.ip_address), "ip_address");
						chl.port = jint_to_int(env, jni_chl.obj, jni_chl.port, "ChannelPort");
						char register_way[24] = {0};
						jstring_to_char(env, jni_chl.obj, jni_chl.register_way, register_way, sizeof(register_way), "register_way");
						chl.register_way = atoi(register_way);
						char safety_way[24] = { 0 };
						jstring_to_char(env, jni_chl.obj, jni_chl.safety_way, safety_way, sizeof(safety_way), "safety_way");
						chl.safety_way = atoi(safety_way);
						char parental[24] = { 0 };
						jstring_to_char(env, jni_chl.obj, jni_chl.parental, parental, sizeof(parental), "parental");
						chl.parental = atoi(parental);
						char secrecy[24] = { 0 };
						jstring_to_char(env, jni_chl.obj, jni_chl.secrecy, secrecy, sizeof(secrecy), "secrecy");
						chl.secrecy = atoi(secrecy);
						//char status[24] = { 0 };
						//jstring_to_char(env, jni_chl.obj, jni_chl.status, status, sizeof(status), "status");
						//chl.status = atoi(status);
						chl.status = jint_to_int(env, jni_chl.obj, jni_chl.status, "status");

						char longitude[24] = { 0 };
						jstring_to_char(env, jni_chl.obj, jni_chl.longitude, longitude, sizeof(longitude), "longitude");
						chl.longitude = atof(longitude);
						char latitude[24] = { 0 };
						jstring_to_char(env, jni_chl.obj, jni_chl.latitude, latitude, sizeof(latitude), "latitude");
						chl.latitude = atof(latitude);
						//DEBUG("INFO: chl: %s:%s:%d:%s\n\n", chl.id, chl.ip_address, chl.port,chl.name);
						dev.chl_map.insert(map<string, chl_info_t>::value_type(chl.id, chl));
					}
				}
				else LOG("ERROR","JniArrayList init failed %d\n\n", nRet);
			}
			else LOG("ERROR","chl_map=%p\n\n", chl_map);
			#endif
			g_dev_map.add(dev.addr.id, &dev);
		}
		LOG("INFO","==================== addDevice ======================\n");
		print_device_map(&g_dev_map);
		LOG("INFO", "==================== addDevice ======================\n");
	}
	else LOG("ERROR","JniArrayList init failed %d\n\n", nRet);
	LOG("INFO","------ addDevice end %d ------\n\n", nRet);
	return nRet;
}

#ifdef USE_DEVICE_ACCESS
/*
 * Class:     gb28181_JNIGB28181
 * Method:    addAuthDevice
 * Signature: (Ljava/lang/Object;)I
 */
JNIEXPORT jint JNICALL Java_gb28181_JNIGB28181_addAuthDevice(JNIEnv *env, jobject obj, jobject list)
{
	LOG("INFO", "---------------- addAuthDevice start ------------------\n\n");
	int nRet = -1;
	if ((NULL == env) || (NULL == obj) || (NULL == list)) {
		LOG("ERROR", "env=%p, obj=%p, list=%p\n\n", env, obj, list);
		return nRet;
	}
	JniArrayList jni_dev_list;
	nRet = jni_dev_list.init(env, list);
	if (0 == nRet) {
		int size = jni_dev_list.size();
		LOG("INFO", "Device list size %d\n\n", size);
		for (int i = 0; i < size; i++) {
			jobject jobj = jni_dev_list.get(i);
			if (NULL == jobj) {
				LOG("ERROR", "JniArrayList get(%d) failed.\n", i);
				continue;
			}
			jclass cls = env->GetObjectClass(jobj);
			if (NULL != cls) {
				do {
					access_info_t acc;
					jfieldID id = env->GetFieldID(cls, "id", "Ljava/lang/String;");
					if (NULL == id) {
						LOG("ERROR", "id=%p\n", id);
						break;
					}
					jstring_to_char(env, jobj, id, acc.addr.id, sizeof(acc.addr.id), "id");
					
					jfieldID ip = env->GetFieldID(cls, "ip", "Ljava/lang/String;");
					if (NULL == ip) {
						LOG("ERROR", "ip=%p\n", ip);
						break;
					}
					jstring_to_char(env, jobj, ip, acc.addr.ip, sizeof(acc.addr.ip), "ip");

					jfieldID port = env->GetFieldID(cls, "port", "I");
					if (NULL == port) {
						LOG("ERROR", "port=%p\n", port);
						break;
					}
					acc.addr.port = jint_to_int(env, jobj, port, "port");

					jfieldID auth = env->GetFieldID(cls, "auth", "I");
					if (NULL == auth) {
						LOG("ERROR", "auth=%p\n", auth);
						break;
					}
					acc.auth = jint_to_int(env, jobj, auth, "auth");

					jfieldID realm = env->GetFieldID(cls, "realm", "Ljava/lang/String;");
					if (NULL == realm) {
						LOG("ERROR", "realm=%p\n", realm);
						break;
					}
					jstring_to_char(env, jobj, realm, acc.realm, sizeof(acc.realm), "realm");

					jfieldID password = env->GetFieldID(cls, "password", "Ljava/lang/String;");
					if (NULL == password) {
						LOG("ERROR", "realm=%p\n", password);
						break;
					}
					jstring_to_char(env, jobj, password, acc.password, sizeof(acc.password), "password");

					jfieldID share = env->GetFieldID(cls, "share", "I");
					if (NULL == share) {
						LOG("ERROR", "share=%p\n", share);
						break;
					}
					acc.share = jint_to_int(env, jobj, share, "share");

					// ..............

					g_access_map.add(acc.addr.id, &acc);
					nRet = 0;
				} while (false);
			}
			else {
				LOG("ERROR", "GetObjectClass failed.\n\n");
				continue;
			}
		}
	}
	else LOG("ERROR", "JniArrayList init failed %d\n\n", nRet);
	LOG("INFO", "------ addAuthDevice end %d ------\n\n", nRet);
	return nRet;
}

/*
 * Class:     gb28181_JNIGB28181
 * Method:    delAuthDevice
 * Signature: (Ljava/lang/Object;)I
 */
JNIEXPORT jint JNICALL Java_gb28181_JNIGB28181_delAuthDevice(JNIEnv *env, jobject obj, jstring jdev_id)
{
	LOG("INFO", "------ del AuthDevice start ------\n\n");
	int nRet = -1;
	if ((NULL == env) || (NULL == obj) || (NULL == jdev_id)) {
		LOG("ERROR", "env=%p, obj=%p, dev_id=%p\n\n", env, obj, jdev_id);
		return nRet;
	}

	char dev_id[32] = { 0 };
	jstring_to_char(env, jdev_id, dev_id, sizeof(dev_id), "dev_id");
	if (strlen(dev_id) > 0) {
		g_access_map.del(dev_id);
		nRet = 0;
	}
	LOG("INFO", "------ delete AuthDevice end %d ------\n\n", nRet);
	return nRet;
}

/*
 * Class:     gb28181_JNIGB28181
 * Method:    updateAuthDevice
 * Signature: (Ljava/lang/Object;)I
 */
JNIEXPORT jint JNICALL Java_gb28181_JNIGB28181_updateAuthDevice(JNIEnv *env, jobject obj, jobject jaccess)
{
	LOG("INFO", "------ update AuthDevice start ------\n\n");
	int nRet = -1;
	if ((NULL == env) || (NULL == obj) || (NULL == jaccess)) {
		LOG("ERROR", "env=%p, obj=%p, dev_id=%p\n\n", env, obj, jaccess);
		return nRet;
	}

	jclass cls = env->GetObjectClass(jaccess);
	if (NULL != cls) {
		do {
			access_info_t acc;
			jfieldID id = env->GetFieldID(cls, "id", "Ljava/lang/String;");
			if (NULL == id) {
				LOG("ERROR", "id=%p\n", id);
				break;
			}
			jstring_to_char(env, jaccess, id, acc.addr.id, sizeof(acc.addr.id), "id");

			jfieldID ip = env->GetFieldID(cls, "ip", "Ljava/lang/String;");
			if (NULL == ip) {
				LOG("ERROR", "ip=%p\n", ip);
				break;
			}
			jstring_to_char(env, jaccess, ip, acc.addr.ip, sizeof(acc.addr.ip), "ip");

			jfieldID port = env->GetFieldID(cls, "port", "I");
			if (NULL == port) {
				LOG("ERROR", "port=%p\n", port);
				break;
			}
			acc.addr.port = jint_to_int(env, jaccess, port, "port");

			jfieldID auth = env->GetFieldID(cls, "auth", "I");
			if (NULL == auth) {
				LOG("ERROR", "auth=%p\n", auth);
				break;
			}
			acc.auth = jint_to_int(env, jaccess, auth, "auth");

			jfieldID realm = env->GetFieldID(cls, "realm", "Ljava/lang/String;");
			if (NULL == realm) {
				LOG("ERROR", "realm=%p\n", realm);
				break;
			}
			jstring_to_char(env, jaccess, realm, acc.realm, sizeof(acc.realm), "realm");

			jfieldID password = env->GetFieldID(cls, "password", "Ljava/lang/String;");
			if (NULL == password) {
				LOG("ERROR", "realm=%p\n", password);
				break;
			}
			jstring_to_char(env, jaccess, password, acc.password, sizeof(acc.password), "password");

			jfieldID share = env->GetFieldID(cls, "share", "I");
			if (NULL == share) {
				LOG("ERROR", "share=%p\n", share);
				break;
			}
			acc.share = jint_to_int(env, jaccess, share, "share");

			// ..............

			auto iter = g_access_map.find(acc.addr.id);
			if (iter != g_access_map.end()) {
				iter->second = acc;
				nRet = 0;
			}
		} while (false);
	}
	LOG("INFO", "------ update AuthDevice end %d ------\n\n", nRet);
	return nRet;
}
#endif

/*
 * Class:     com_hirain_sdk_gb28181_JNIGB28181
 * Method:    test
 * Signature: (I)I
 */
JNIEXPORT jint JNICALL Java_com_hirain_sdk_gb28181_JNIGB28181_test(JNIEnv *env, jobject obj, jint n)
{
	LOG("INFO","Hello, I'm JNI Interface test.\n\n");
	int nRet = 0;
	return nRet;
}

#endif// #ifdef USE_PLATFORM_CASCADE