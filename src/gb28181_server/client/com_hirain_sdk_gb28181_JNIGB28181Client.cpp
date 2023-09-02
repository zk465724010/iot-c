
#include "com_hirain_sdk_gb28181_JNIGB28181Client.h"
#include "gb28181client_api.h"
#include "log.h"
#include "common_jni.h"
#include "media.h"
//#include "gb28181client_api.h"
#include <time.h>

JavaVM *g_java_vm = NULL;

extern CUdpClient g_udp_client;
extern bool g_is_init;
extern CMedia g_media;
extern local_cfg_t g_local_cfg;
extern char	g_udp_ser_ip[24];		// gb28181server.dll模块UDP的IP
extern int	g_udp_ser_port;			// gb28181server.dll模块UDP的Port
extern char	g_media_ip[24];			// 本地的Media的IP

std::mutex g_mutex;

int STDCALL on_media(void *pData, int nLen, void *pUserData);
int STDCALL on_record(void *pData, int nLen, void *pUserData);


/*
 * Class:     com_hirain_sdk_gb28181_JNIGB28181Client
 * Method:    init
 * Signature: (Lcom/hirain/sdk/JNIEntity/LocalConfig;)I
 */
JNIEXPORT jint JNICALL Java_com_hirain_sdk_gb28181_JNIGB28181Client_init(JNIEnv *env, jobject obj, jobject cfg)
{
	LOG("INFO","------ init start ------\n\n");
	int nRet = -1;
	if ((NULL == env) || (NULL == obj) || (NULL == cfg)) {
		LOG("ERROR","env=%p, obj=%p, cfg=%p\n\n", env, obj, cfg);
		return nRet;
	}
	env->GetJavaVM(&g_java_vm);		// ��ȡJava�����
	if (NULL == g_java_vm) {
		LOG("ERROR","GetJavaVM error\n\n");
		return nRet;
	}
	jclass cls = env->GetObjectClass(cfg);
	if (NULL != cls)
	{
		jfieldID ip = env->GetFieldID(cls, "localSipIp", "Ljava/lang/String;");
		jstring_to_char(env, cfg, ip, g_local_cfg.addr.ip, sizeof(g_local_cfg.addr.ip), "ip");	// gb28181server.dll模块UDP的IP
		jfieldID port = env->GetFieldID(cls, "localSipPort", "I");
		if (NULL != port) {
			g_local_cfg.addr.port = 9000;// env->GetIntField(cfg, port);				// gb28181server.dll模块UDP的Port
		}
		else LOG("ERROR","port == null\n\n");
		
		jfieldID ip2 = env->GetFieldID(cls, "localSipIpTwo", "Ljava/lang/String;");
		jstring_to_char(env, cfg, ip2, g_local_cfg.dev.addr.ip, sizeof(g_local_cfg.dev.addr.ip), "ip2");// gb28181client.dll模块media的IP
		#if 0
		jfieldID port2 = env->GetFieldID(cls, "localSipPortTwo", "I");
		if (NULL != port2) {
			g_local_cfg.dev.addr.port = env->GetIntField(cfg, port2);
		}
		else LOG("ERROR","port2 == null\n\n");
		#endif
		nRet = 0;
		LOG("INFO","peer_ip:%s, peer_port:%d, media:%s\n\n", g_local_cfg.addr.ip, g_local_cfg.addr.port, g_local_cfg.dev.addr.ip);
	}
	else LOG("ERROR","------ GetObjectClass failed -----\n\n");
	//if (0 == nRet) {
	//	nRet = init(NULL);
	//}
	LOG("INFO","------ init end [ret:%d] ------\n\n", nRet);
    return nRet;
}

/*
 * Class:     com_hirain_sdk_gb28181_JNIGB28181Client
 * Method:    uninit
 * Signature: ()V
 */
JNIEXPORT void JNICALL Java_com_hirain_sdk_gb28181_JNIGB28181Client_uninit(JNIEnv *env, jobject obj)
{
	LOG("INFO","------ uninit start ------\n\n");
	uninit();

	LOG("INFO","------ uninit end ------\n\n");
}

/*
 * Class:     com_hirain_sdk_gb28181_JNIGB28181Client
 * Method:    test
 * Signature: (I)I
 */
JNIEXPORT jint JNICALL Java_com_hirain_sdk_gb28181_JNIGB28181Client_test(JNIEnv *, jobject, jint)
{
	LOG("INFO","------ test ------\n\n");
    return 0;
}

/*
 * Class:     com_hirain_sdk_gb28181_JNIGB28181Client
 * Method:    play
 * Signature: (Lcom/hirain/sdk/JNIEntity/Operation;)I
 */
JNIEXPORT jint JNICALL Java_com_hirain_sdk_gb28181_JNIGB28181Client_play(JNIEnv *env, jobject obj, jobject opt)
{
	LOG("INFO","------ play start ------\n\n");
	int nRet = -1;
	if ((NULL == env) || (NULL == obj) || (NULL == opt)) {
		LOG("ERROR","env=%p, obj=%p, cfg=%p\n\n", env, obj, opt);
		return nRet;
	}
	if(NULL == g_java_vm) {
		env->GetJavaVM(&g_java_vm);		// 获取Java虚拟机
		if (NULL == g_java_vm) {
			LOG("ERROR", "GetJavaVM error\n\n");
			return nRet;
		}
	}

	JniOperation jni_opt;
	nRet = jni_opt.init(env, opt);
	if (0 != nRet) {
		LOG("ERROR","jni_opt.init = %d\n\n", nRet);
		return nRet;
	}
	char token[32] = { 0 };
	char gid[32] = { 0 };
	jstring_to_char(env, opt, jni_opt.token, token, sizeof(token), "token");
	int cmd = jint_to_int(env, opt, jni_opt.cmd, "cmd");

	if (false == g_is_init) {
		nRet = init();
		if (0 != nRet) {
			LOG("ERROR", "init faield %d\n\n", nRet);
			return nRet;
		}
		g_is_init = true;
		jstring_to_char(env, opt, jni_opt.peer_ip, g_udp_ser_ip, sizeof(g_udp_ser_ip), "gb28181server.dll模块UDP的IP");
		g_udp_ser_port = jint_to_int(env, opt, jni_opt.peer_port, "gb28181server.dll模块UDP的Port");
		jstring_to_char(env, opt, jni_opt.local_ip, g_media_ip, sizeof(g_media_ip), "本地的Media的IP");
	}

	user_data_t user_data;
	if (CTRL_PLAY_START == cmd) {	// 点播
		strcpy(user_data.token, token);
		jstring_to_char(env, opt, jni_opt.gid, user_data.gid, sizeof(user_data.gid), "gid");

		addr_info_t dev;
		jstring_to_char(env, opt, jni_opt.id, dev.id, sizeof(dev.id), "deviceId");
		jstring_to_char(env, opt, jni_opt.ip, dev.ip, sizeof(dev.ip), "deviceIp");
		dev.port = jint_to_int(env, opt, jni_opt.port, "devicePort");
		if ((0 == strlen(dev.id)) || (0 == strlen(dev.ip)) || (dev.port <= 0)) {
			LOG("ERROR","------ Play (%s:%s:%d) ------\n\n", dev.id, dev.ip, dev.port);
			return -1;
		}
		LOG("INFO","------ Play [dev=%s:%s:%d call_id=%s] ------\n\n", dev.id, dev.ip, dev.port, token);
		udp_msg_t msg;
		msg.cmd = CTRL_PLAY_START;
		memcpy(&msg.dev, &dev, sizeof(addr_info_t));
		strcpy(msg.call_id, token);
		nRet = play(token, &msg, NULL, NULL, on_media, &user_data, sizeof(user_data_t));
	}
	else if (CTRL_PLAYBAK_START == cmd) {	// 回播
		strcpy(user_data.token, token);
		jstring_to_char(env, opt, jni_opt.gid, user_data.gid, sizeof(user_data.gid), "gid");

		char path[256] = { 0 };
		jstring_to_char(env, opt, jni_opt.file_path, path, sizeof(path), "file_path");
		if (strlen(path) > 0) {		// 回放本地的录像文件
			LOG("INFO","------ Playback [path=%s call_id=%s]------\n\n", path, token);
			nRet = play(token, NULL, path, NULL, on_media, &user_data, sizeof(user_data_t));
		}
		else {						// 回放设备端的录像
			long start_time = jlong_to_long(env, opt, jni_opt.start_time, "StartTime");
			long end_time = jlong_to_long(env, opt, jni_opt.end_time, "EndTime");
			if ((start_time > 0) && (end_time > 0)) {
				addr_info_t dev;
				jstring_to_char(env, opt, jni_opt.id, dev.id, sizeof(dev.id), "deviceId");
				jstring_to_char(env, opt, jni_opt.ip, dev.ip, sizeof(dev.ip), "deviceIp");
				dev.port = jint_to_int(env, opt, jni_opt.port, "devicePort");
				LOG("INFO","------ Playback [dev=%s:%s:%d time=%ld~%ld call_id=%s] ------\n\n", dev.id, dev.ip, dev.port, start_time, end_time, token);
				udp_msg_t msg;
				msg.cmd = CTRL_PLAYBAK_START;
				memcpy(&msg.dev, &dev, sizeof(addr_info_t));
				strcpy(msg.call_id, token);
				msg.start_time = start_time;
				msg.end_time = end_time;
				nRet = play(token, &msg, NULL, NULL, on_media, &user_data, sizeof(user_data_t));
			}
			else LOG("ERROR","start_time:%ld, end_time:%ld\n\n", start_time, end_time);
		}
	}
	else if (CTRL_PLAYBAK_POS == cmd) {	// 回播-拖放
		int pos = jint_to_int(env, opt, jni_opt.pos, "pos");
		LOG("INFO","pos=%d\n\n", pos);
		playbak_ctrl(token, cmd, pos);
	}
	else if (CTRL_PLAYBAK_SPEED == cmd) { // 回播-速度控制(0.5, 1.0, 2.0 等)
		playbak_ctrl(token, cmd, 1.0);
	}
	else if ((CTRL_PLAYBAK_PAUSE == cmd)  // 回播-暂停
		|| (CTRL_PLAYBAK_CONTINUE == cmd) // 回播-继续
		|| (CTRL_PLAY_PAUSE == cmd)		// 点播-暂停
		|| (CTRL_PLAY_CONTINUE == cmd)) {// 点播-继续
		playbak_ctrl(token, cmd);
	}
	LOG("INFO","token:%s, gid:%s cmd:%d\n\n", token, gid, cmd);
	LOG("INFO","------ play end [ret:%d] ------\n\n", nRet);

    return nRet;
}

/*
 * Class:     com_hirain_sdk_gb28181_JNIGB28181Client
 * Method:    stop
 * Signature: (Lcom/hirain/sdk/JNIEntity/Operation;)I
 */
JNIEXPORT jint JNICALL Java_com_hirain_sdk_gb28181_JNIGB28181Client_stop(JNIEnv *env, jobject obj, jobject opt)
{
	LOG("INFO","------ stop start ------\n\n");
	int nRet = -1;
#if 1
	char token[32] = { 0 };
	int cmd = -1;
	do {
		if ((NULL == env) || (NULL == obj) || (NULL == opt)) {
			LOG("ERROR","env=%p, obj=%p, opt=%p\n\n", env, obj, opt);
			break;
		}
		jclass cls_obj = env->GetObjectClass(opt);
		if (NULL == cls_obj) {
			LOG("ERROR","cls_obj=%p\n\n", cls_obj);
			break;
		}
		if(false == g_is_init) {
			nRet = init();
			if (0 != nRet) {
				LOG("ERROR", "init faield %d\n\n", nRet);
				break;
			}
			g_is_init = true;
			jfieldID jfield_peer_ip = env->GetFieldID(cls_obj, "sipIp", "Ljava/lang/String;");
			if (NULL == jfield_peer_ip) {
				LOG("ERROR", "jfield_peer_ip=%p\n\n", jfield_peer_ip);
				break;
			}
			jstring_to_char(env, opt, jfield_peer_ip, g_udp_ser_ip, sizeof(g_udp_ser_ip), "gb28181server.dll模块UDP的IP");
			jfieldID jfield_peer_port = env->GetFieldID(cls_obj, "sipUdpPort", "I");
			if (NULL == jfield_peer_port) {
				LOG("ERROR", "jfield_peer_port=%p\n\n", jfield_peer_port);
				break;
			}
			g_udp_ser_port = env->GetIntField(opt, jfield_peer_port);
		}
		jfieldID jfield_token = env->GetFieldID(cls_obj, "token", "Ljava/lang/String;");
		if (NULL == jfield_token) {
			LOG("ERROR","jfield_token=%p\n\n", jfield_token);
			break;
		}
		jstring_to_char(env, opt, jfield_token, token, sizeof(token), "token");

		jfieldID jfield_cmd = env->GetFieldID(cls_obj, "cmd", "I");
		if (NULL == jfield_cmd) {
			LOG("ERROR","jfield_cmd=%p\n\n", jfield_cmd);
			break;
		}
		cmd = env->GetIntField(opt, jfield_cmd);

		LOG("INFO","token:%s, cmd:%d\n\n", token, cmd);
		nRet = stop(token, cmd);
		
	} while (false);
	LOG("INFO","------ stop end [token:%s, cmd:%d, ret:%d] ------\n\n", token, cmd, nRet);
#endif
    return nRet;
}

/*
 * Class:     com_hirain_sdk_gb28181_JNIGB28181Client
 * Method:    record
 * Signature: (Lcom/hirain/sdk/JNIEntity/Operation;Ljava/lang/Object;)I
 */
JNIEXPORT jint JNICALL Java_com_hirain_sdk_gb28181_JNIGB28181Client_record(JNIEnv *env, jobject obj, jobject opt, jobject vd)
{
	char current_time[32] = {0};
	g_media.timestamp_to_string(time(NULL), current_time, sizeof(current_time));
	LOG("INFO","------ record start %s ------\n\n", current_time);
	//std::lock_guard<std::mutex> lock(g_mutex);
	int nRet = -1;
	JniOperation jni_opt;
	nRet = jni_opt.init(env, opt);
	if (0 == nRet) {
		if (false == g_is_init) {
			nRet = init();
			if (0 != nRet) {
				LOG("ERROR", "init faield %d\n\n", nRet);
				return nRet;
			}
			g_is_init = true;
			jstring_to_char(env, opt, jni_opt.peer_ip, g_udp_ser_ip, sizeof(g_udp_ser_ip), "gb28181server.dll模块UDP的IP");
			g_udp_ser_port = jint_to_int(env, opt, jni_opt.peer_port, "gb28181server.dll模块UDP的Port");
			jstring_to_char(env, opt, jni_opt.local_ip, g_media_ip, sizeof(g_media_ip), "本地的Media的IP");
		}
		static int msg_size = sizeof(udp_msg_t);
		udp_msg_t msg;
		jstring_to_char(env, opt, jni_opt.id, msg.dev.id, sizeof(msg.dev.id), "deviceId");
		jstring_to_char(env, opt, jni_opt.ip, msg.dev.ip, sizeof(msg.dev.ip), "deviceIp");
		msg.dev.port = jint_to_int(env, opt, jni_opt.port, "devicePort");
		int cmd = jint_to_int(env, opt, jni_opt.cmd, "cmd");
		if (CTRL_REC_START == cmd) {
			char path[256] = { 0 };
			jstring_to_char(env, opt, jni_opt.file_path, path, sizeof(path), "filePath");
			if (strlen(path) > 0) { // 录像到本地
				msg.cmd = CTRL_PLAY_START;
				sprintf(msg.call_id, "%s", g_media.generate_random_string(30).c_str());
				unis_record_t rec;
				strcpy(rec.filename, path);
				rec.duration = jint_to_int(env, opt, jni_opt.duration, "stop_second");
				rec.duration = (rec.duration > 0) ? rec.duration : 3600;
				strcpy(rec.call_id, msg.call_id); // 测试
				LOG("INFO","Local record (dev:%s:%s:%d,duration:%d,path:%s)\n\n", msg.dev.id, msg.dev.ip, msg.dev.port, rec.duration, rec.filename);
				nRet = -1;
				int port = g_media.alloc_port();
				if (port > 0) {
					strcpy(msg.media_ip, g_media_ip);
					msg.media_port = port;
					nRet = (msg_size == g_udp_client.Send(&msg, msg_size, g_udp_ser_ip, g_udp_ser_port)) ? 0 : -1;
					if (0 == nRet) {
						//LOG("INFO","UDP client send message to '%s:%d' successfully ^-^\n\n", peer_ip, peer_port);
						nRet = (g_media.create_channel(msg.call_id, msg.media_ip, port, on_record, NULL, &rec) >= 0) ? 0 : -1;
						if (0 == nRet) {
							LOG("INFO", "Create channel '%s'\n\n", msg.call_id);
							//LOG("INFO","1 join_channel '%s'\n\n", msg.call_id);
							g_media.join_channel(msg.call_id);	// Join channel
							//LOG("INFO","2 join_channel '%s'\n\n", msg.call_id);
							msg.cmd = CTRL_PLAY_STOP;
							nRet = (msg_size == g_udp_client.Send(&msg, msg_size, g_udp_ser_ip, g_udp_ser_port)) ? 0 : -1;// Terminate call invite
							if (0 == nRet) {
								LOG("INFO","Destroy channel '%s'\n\n", msg.call_id);
								g_media.destroy_channel(msg.call_id);// Destroy channel
								JniVideoTapeFile video;
								nRet = video.init(env, vd);
								if (0 == nRet) {
									char buff[32] = { 0 };
									int duration = rec.duration;
									int houre = duration / 3600;
									int minute = (duration % 3600) / 60;
									int second = (duration % 3600) % 60;
									sprintf(buff, "%02d:%02d:%02d", houre, minute, second);
									video.SetJStringField(video.duration, buff);
									video.SetIntField(video.video_encode, 0);// 0:H264 1:H265 2:其他
									//jni_opt.SetObjectField(jni_opt.tape_file, video.obj);
								}
								else LOG("ERROR","JniVideoTapeFile init failed %d.\n\n", nRet);
							}
							else LOG("ERROR","UDP client send message to '%s:%d' failed %d/%d\n\n", g_udp_ser_ip, g_udp_ser_port);
						}
						else LOG("ERROR","Create local record channel '%s' failed %d [dev:%s:%s:%d]\n\n", msg.call_id,nRet, msg.dev.id, msg.dev.ip, msg.dev.port);
					}
					else LOG("ERROR","UDP client send message to '%s:%d' failed %d\n\n", g_udp_ser_ip, g_udp_ser_port, nRet);
					g_media.free_port(port);
				}
				else LOG("ERROR","alloc_port failed %d.\n\n", port);
			}
			else {	// 录像到设备
				nRet = -1;
				msg.cmd = CTRL_REC_START;
				msg.start_time = jlong_to_long(env, opt, jni_opt.start_time, "StartTime");
				msg.end_time = jlong_to_long(env, opt, jni_opt.end_time, "EndTime");
				LOG("INFO","Device record (dev:%s:%s:%d,time:%ld~%ld)\n\n", msg.dev.id, msg.dev.ip, msg.dev.port, msg.start_time, msg.end_time);
				if ((msg.start_time > 0) && (msg.end_time > msg.start_time)) {
					nRet = (msg_size == g_udp_client.Send(&msg, msg_size, g_udp_ser_ip, g_udp_ser_port)) ? 0 : -1;
					if (0 != nRet)
						LOG("ERROR","UDP client send message to '%s:%d' failed %d\n\n", g_udp_ser_ip, g_udp_ser_port, nRet);
				}
				else LOG("ERROR","start_time:%ld, end_time:%ld\n\n", msg.start_time, msg.end_time);
			}
		}
		else if(CTRL_REC_STOP == cmd){
			msg.cmd = CTRL_REC_STOP;
			LOG("INFO","Device record stop (dev:%s:%s:%d)\n\n", msg.dev.id, msg.dev.ip, msg.dev.port);
			nRet = (msg_size == g_udp_client.Send(&msg, msg_size, g_udp_ser_ip, g_udp_ser_port)) ? 0 : -1;
			if (0 != nRet)
				LOG("ERROR","UDP client send message to '%s:%d' failed %d\n\n", g_udp_ser_ip, g_udp_ser_port, nRet);
		}
		else LOG("ERROR","Unknown command cmd=%d\n\n", cmd);
	}
	else LOG("ERROR","JniOperation init failed %d\n\n", nRet);
	LOG("INFO","------ record end [ret:%d] ------\n\n", nRet);
	return nRet;
}

/*
 * Class:     com_hirain_sdk_gb28181_JNIGB28181Client
 * Method:    control
 * Signature: (Lcom/hirain/sdk/entity/gb28181/Operation;)I
 */
JNIEXPORT jint JNICALL Java_com_hirain_sdk_gb28181_JNIGB28181Client_control(JNIEnv *env, jobject obj, jobject opt)
{
	LOG("INFO", "------ control start ------\n\n");
	int nRet = -1;
	do {
		if ((NULL == env) || (NULL == obj) || (NULL == opt)) {
			LOG("ERROR", "Parameter error. env=%p, obj=%p, cfg=%p\n\n", env, obj, opt);
			break;
		}
		JniOperation jni_opt;
		nRet = jni_opt.init(env, opt);
		if (0 != nRet) {
			LOG("ERROR", "jni_opt.init = %d\n\n", nRet);
			break;
		}
		if (false == g_is_init) {
			nRet = init();
			if (0 != nRet) {
				LOG("ERROR", "init faield %d\n\n", nRet);
				break;
			}
			g_is_init = true;
			jstring_to_char(env, opt, jni_opt.peer_ip, g_udp_ser_ip, sizeof(g_udp_ser_ip), "gb28181server.dll模块UDP的IP");
			g_udp_ser_port = jint_to_int(env, opt, jni_opt.peer_port, "gb28181server.dll模块UDP的Port");
		}
		int cmd = jint_to_int(env, opt, jni_opt.cmd, "cmd");
		if ((CTRL_DEV_CTRL < cmd) && (cmd <= CTRL_PTZ_STOP)) { // 云台控制
			addr_info_t dev;
			jstring_to_char(env, opt, jni_opt.id, dev.id, sizeof(dev.id), "deviceId");
			jstring_to_char(env, opt, jni_opt.ip, dev.ip, sizeof(dev.ip), "deviceIp");
			dev.port = jint_to_int(env, opt, jni_opt.port, "devicePort");
			if ((0 == strlen(dev.id)) || (0 == strlen(dev.ip)) || (dev.port <= 0)) {
				LOG("ERROR", "Device address info error (%s:%s:%d) ------\n\n", dev.id, dev.ip, dev.port);
				break;
			}
			LOG("INFO", "------ Control [dev=%s:%s:%d] ------\n\n", dev.id, dev.ip, dev.port);
			nRet = control(&dev, cmd);
		}
		else LOG("ERROR", "Unknown control command %d\n", cmd);
	} while (false);
	LOG("INFO", "------ control end [ret:%d] ------\n\n", nRet);
	return nRet;
}

#if 1
// 点播或回放
int STDCALL on_media(void *pData, int nLen, void *pUserData)
{
	user_data_t *user_data = (user_data_t*)pUserData;
	if ((NULL == user_data) || (NULL == g_java_vm)) {
		LOG("ERROR","UDP client data:%p, size:%d user_data:%p java_vm:%p\n", pData, nLen, user_data, g_java_vm);
		return 0;
	}
	JNIEnv *env = NULL;
	g_java_vm->AttachCurrentThread((void**)&env, NULL);
	if (NULL != env) {
		JniWebSocketServer	web_skt;
		int ret = web_skt.init(env);
		if (0 == ret) {
			jbyteArray bytes = NULL;
			if ((NULL != pData) && (nLen > 0)) {
				bytes = env->NewByteArray(nLen);
				if(NULL != bytes)
					env->SetByteArrayRegion(bytes, 0, nLen, (jbyte*)pData);
				else LOG("ERROR","NewByteArray failed.\n\n");
			}
			else {
				nLen = 1;
				bytes = env->NewByteArray(nLen);
				if (NULL != bytes)
					env->SetByteArrayRegion(bytes, 0, nLen, (jbyte*)"20");//回放结束标志'20'
				else LOG("ERROR","NewByteArray failed.\n\n");
			}
			if (NULL != bytes) {
				jstring token = char_to_jstring(env, user_data->token);
				jstring gid = char_to_jstring(env, user_data->gid);
				if ((NULL != token) && (NULL != gid)) {
					env->CallStaticObjectMethod(web_skt.cls, web_skt.sendGbPlayStream, bytes, gid, token);
					//LOG("INFO","%d frame size %d\n", index++, nLen);
					if (user_data->n > 0) {
						LOG("INFO","FrameSize=%d, call_id=%s\n", nLen, user_data->token);
						user_data->n--;
					}
				}
				else LOG("ERROR","token=%p, gid=%p\n\n", token, gid);
				env->DeleteLocalRef(bytes);
			}
		}
		else LOG("ERROR","JniWebSocketServer failed\n\n");
	}
	else LOG("ERROR","env=null\n\n");
	g_java_vm->DetachCurrentThread();
	return 0;
}
// 录像
int STDCALL on_record(void *pData, int nLen, void *pUserData)
{
	unis_record_t *rec = (unis_record_t*)pUserData;
	if ((NULL != pData) && (nLen > 0)) {
		if ((NULL == rec->fp) && (0 == rec->size)) {
			rec->start_time = time(NULL);
			rec->end_time = rec->start_time + rec->duration + 1;
			rec->fp = fopen(rec->filename, "wb");
			char current_time[32] = { 0 };
			g_media.timestamp_to_string(rec->start_time, current_time, sizeof(current_time));// 测试
			LOG("INFO","%s: Create a file '%s' %ds (%lld~%lld)[call_id:%s]\n\n", current_time, rec->filename, (rec->end_time-rec->start_time), rec->start_time, rec->end_time,rec->call_id);
			if (NULL == rec->fp) {
				LOG("ERROR","File open failure '%s'\n\n", rec->filename);
				return -1;
			}
		}
		if (NULL != rec->fp) {
			int write_bytes = fwrite(pData, 1, nLen, rec->fp);
			if (write_bytes > 0)
				rec->size += write_bytes;
		}
		if (rec->end_time <= time(NULL)) {
			if (NULL != rec->fp) {
				fclose(rec->fp);
				rec->fp = NULL;
				char current_time[32] = { 0 };
				g_media.timestamp_to_string(time(NULL), current_time, sizeof(current_time));// 测试
				LOG("INFO","%s: -------- File closed 1 '%s' --------\n\n", current_time, rec->filename);
			}
			return -1;
		}
	}
	else {
		//rec->end_time = time(NULL);
		if (NULL != rec->fp) {
			fclose(rec->fp);
			rec->fp = NULL;
			char current_time[32] = { 0 };
			g_media.timestamp_to_string(time(NULL), current_time, sizeof(current_time));// 测试
			LOG("INFO","%s: -------- File closed 2 '%s' --------\n\n", current_time, rec->filename);
			return -1;
		}
	}
	//LOG("INFO","------ on_record end ------ \n\n");
	return 0;
}
#endif

/*
 * Class:     com_hirain_sdk_gb28181_JNIGB28181Client
 * Method:    snapshoot
 * Signature: (Lcom/hirain/sdk/entity/gb28181/Operation;)I
 */
JNIEXPORT jint JNICALL Java_com_hirain_sdk_gb28181_JNIGB28181Client_snapshoot(JNIEnv* env, jobject obj, jobject opt)
{
	LOG("INFO", "------ snapshoot start ------\n\n");
	int nRet = -1;
	do {
		if ((NULL == env) || (NULL == obj) || (NULL == opt)) {
			LOG("ERROR", "Parameter error. env=%p, obj=%p, opt=%p\n\n", env, obj, opt);
			break;
		}

		JniOperation jni_opt;
		nRet = jni_opt.init(env, opt);
		if (0 != nRet) {
			LOG("ERROR", "jni_opt.init = %d\n\n", nRet);
			break;
		}
		if (false == g_is_init) {
			nRet = init();
			if (0 != nRet) {
				LOG("ERROR", "init faield %d\n\n", nRet);
				break;
			}
			g_is_init = true;
			jstring_to_char(env, opt, jni_opt.peer_ip, g_udp_ser_ip, sizeof(g_udp_ser_ip), "gb28181server.dll模块UDP的IP");
			g_udp_ser_port = jint_to_int(env, opt, jni_opt.peer_port, "gb28181server.dll模块UDP的Port");
			jstring_to_char(env, opt, jni_opt.local_ip, g_media_ip, sizeof(g_media_ip), "本地的Media的IP");
		}
		addr_info_t dev;
		jstring_to_char(env, opt, jni_opt.id, dev.id, sizeof(dev.id), "deviceId");
		jstring_to_char(env, opt, jni_opt.ip, dev.ip, sizeof(dev.ip), "deviceIp");
		dev.port = jint_to_int(env, opt, jni_opt.port, "devicePort");
		char filename[256] = {0};
		jstring_to_char(env, opt, jni_opt.file_path, filename, sizeof(filename), "filename");

		if ((0 == strlen(dev.id)) || (0 == strlen(dev.ip)) || (dev.port <= 0) || (0 == strlen(filename))) {
			LOG("ERROR", "device '%s:%s:%d', filename '%s'\n", dev.id, dev.ip, dev.port, filename);
			break;
		}
		nRet = snapshoot(&dev, filename);
	} while (false);
	LOG("INFO", "------ snapshoot end [ret:%d] ------\n\n", nRet);
	return nRet;
}