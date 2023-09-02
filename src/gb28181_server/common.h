
#ifndef __COMMON_H__
#define __COMMON_H__

#include <map>
#include <vector>
using namespace std;
#include <string.h>
#include <string>


typedef enum _opt_cmd_e
{
    UNKNOWN_CMD = 0,            // 未知类型     
    // 查询命令
    QUERY_DEV_INFO    = 100,    // 设备信息
	QUERY_DEV_STATUS,           // 设备状态
    QUERY_DEV_CATALOG,          // 设备目录
	QUERY_DEV_RECORD,           // 录像
	QUERY_DEV_CFG,              // 设备配置信息

	QUERY_DEV_ALARM,            // 告警信息
	QUERY_DEV_PRESET,           // 预置位信息
	QUERY_DEV_MP,               //

    // 控制命令
	CTRL_DEV_CTRL = 200,		// 设备控制
	CTRL_PTZ_LEFT,				// 左转
	CTRL_PTZ_RIGHT,             // 右转
	CTRL_PTZ_UP,                // 上转
	CTRL_PTZ_DOWN,              // 下转
	CTRL_PTZ_LARGE,             // 放大
	CTRL_PTZ_SMALL,             // 缩小
	CTRL_PTZ_STOP,              // 停止控制
	CTRL_REC_START,             // 开始录像
	CTRL_REC_STOP,              // 停止录像
	CTRL_GUD_START,             // 布防
	CTRL_GUD_STOP,              // 撤防
	CTRL_ALM_RESET,             // 报警复位
	CTRL_TEL_BOOT,              // 远程重启
	CTRL_IFR_SEND,              // 强制关键帧(I帧)
	CTRL_DEV_CFG,               // 配置设备

	CTRL_PLAY_START,            // 点播-开始
	CTRL_PLAY_STOP,				// 点播-停止
	CTRL_PLAY_PAUSE,	        // 点播-暂停
	CTRL_PLAY_CONTINUE,			// 点播-继续

	CTRL_PLAYBAK_START,			// 回播-开始
	CTRL_PLAYBAK_STOP,			// 回播-停止

	CTRL_PLAYBAK_PAUSE,	        // 回播-暂停
	CTRL_PLAYBAK_CONTINUE,	    // 回播-继续
	CTRL_PLAYBAK_POS,			// 回播-拖放
	CTRL_PLAYBAK_SPEED,			// 回播-速度控制(0.5, 1.0, 2.0 等)

	CTRL_DOWNLOAD_START,        // 下载
	CTRL_DOWNLOAD_STOP,

	CTRL_TALK_START,            // 对讲
	CTRL_TALK_STOP,

    NOTIFY_KEEPALIVE = 300,     // 心跳
    NOTIFY_MEDIA_STATUS,         // 媒体状态

	NOTIFY_ONLINE				// 上线通知

}cmd_e;

typedef struct _addr_info_t
{
	char id[32] = { 0 };
	char ip[24] = { 0 };
	int  port = 0;
}addr_info_t;

typedef struct access_info_t
{
	addr_info_t addr;				// 外域地址信息
	short share = 1;				// 是否共享(0:不共享 1:共享)
	short auth = 1;					// 开启鉴权
	char realm[16] = { 0 };			// 鉴权realm
	char password[32] = { 0 };		// 鉴权密码
	//char protocol[64] = {0};		// 联网协议("GB/T2881-2016" "GB/T2881-2011" "DB33/T629-2011")
	//short stream_way = 1;			// 取流方式(1:UDP 2:TCP_active 3:TCP_passive)
	//short subscribe = 0;			// 开启订阅(0:不开启 1:开启)
}access_info_t;

typedef struct pfm_info
{
	addr_info_t addr;               // 服务器地址信息
	char domain[16] = {0};          // 服务器域
	char platform[24] = {0};        // 平台接入方式(如: 'GB/T28181-2011', 'GB/T28181-2016'等)
	char protocol[16] = {0};        // 数据传输协议(1:UDP 2:TCP)
	char username[32] = {0};		// 用户名
	char password[32] = {0};        // 鉴权密码
	int  expires = 3600;            // 注册有效期(取值:3600 ~ 100000)
	short register_interval = 60;   // 注册间隔(取值:60 ~ 600)
	short keepalive = 5;            // 心跳周期(取值:5 ~ 255)
	short keepalive_flag = 0;       // 心跳标志
	char status = 0;                // 注册状态(0:未注册 1:注册成功 2:注册失败 3:注销成功 4:注册中)
    char enable = 0;                // 使能开关(0:不使能 1:使能)
	size_t cseq = 1;
    pfm_info()
    {
        strcpy(addr.id, "34020000002000000008");
        strcpy(addr.ip, "192.168.41.131");
        addr.port = 5060;
        strcpy(domain, "3402000000");
        strcpy(username, "34020000002000000001");
		strcpy(password, "123456");
		strcpy(protocol, "UDP");
	}
}pfm_info_t;

typedef struct chl_info
{
    //char key[64] = {0};			// 设备ID#通道ID
    char id[32] = {0};			    // 通道ID
	char parent_id[64] = {0};		// 父设备/区域/系统ID(必选)
	char name[32] = {0};            // 名称
	char manufacturer[32] = {0};    // 制造商
	char model[32] = {0};           // 当为设备时,设备型号(必选)
	char owner[32] = {0};           // 当为设备时,设备归属(必选)
	char civil_code[32] = {0};      // 行政区域(必选)
	char address[24] = {0};         // 当为设备时,安装地址(必选)
	char ip_address[24] = {0};      // 设备/区域/系统IP地址(可选)
	int  port = 0;                  // 设备/区域/系统端口(可选)
	char safety_way = 0;            // 信令安全模式(可选)缺省为0(0:不采用、2:S/MIME签名方式、3:S/MIME加密签名同时采用方式、4:数字摘要方式
    char parental = 0;              // 是否有子设备(必选)1有,0没有
	char secrecy = 0;               // 保密属性(缺省为0, 0:不涉密, 1:涉密)
    char status = 0;                // 设备状态(必选) (0:OFF 1:ON)
	char register_way = 1;          // 注册方式(必选)缺省为1(1:符合IETFRFC3261标准的认证注册模式、2:基于口令的双向认证注册模式、3:基于数字证书的双向认证注册模式)
	double longitude = 0.0;			// 经度(可选)
	double latitude = 0.0;			// 纬度(可选)
	short share = 1;				 // 是否共享给上级（0:不共享 1:共享）

	/*
	char cert_num[32] = {0};		// 证书序列号(有证书的设备必选)
	char certifiable = 0;			// 证书有效标识(有证书的设备必选)缺省为0(证书有效标识: 0:无效、1:有效)
	char err_code = 1;				// 无效原因码(有证书且证书无效的设备必选)
	char end_time[24] = {0};		// 证书终止有效期(有证书的设备必选)
	char block[24] = {0};			// 警区(可选)
	char password[32] = {0};		// 设备口令(可选)
	double longitude = 0.0;			// 经度(可选)
	double latitude = 0.0;			// 纬度(可选)
	//////////////////////////////////
	char ptz_type = 0;				// 摄像机类型扩展,标识摄像机类型:1-球机;2-半球;3-固定枪机;4-遥控枪机。当目录项为摄像机时可选
	char position_type = 0;			// 摄像机位置类型扩展。1-省际检查站、2-党政机关、3-车站码头、4-中心广 场、5-体育场馆、6-商业中心、7-宗教场所、8-校园周边、9-治安复杂区域、10-交通 干线。当目录项为摄像机时可选
	char room_type = 0;				// 摄像机安装位置室外、室内属性。1-室外、2-室内。当目录项为摄像机时可 选,缺省为1。
	char use_type = 0;				// 摄像机用途属性。1-治安、2-交通、3-重点。当目录项为摄像机时可选。
	char supply_light_type = 0;		// 摄像机补光属性。1-无补光、2-红外补光、3-白光补光。当目录项为摄像机 时可选,缺省为1。
	char direction_type = 0;		// 摄像机监视方位属性。1-东、2-西、3-南、4-北、5-东南、6-东北、7-西南、8-西 北。当目录项为摄像机时且为固定摄像机或设置看守位摄像机时可选。
	char resolution = 0;			// 摄像机支持的分辨率,可有多个分辨率值,各个取值间以“/”分隔。分辨率 取值参见附录F中SDP f字段规定。当目录项为摄像机时可选。
	char business_group_id[32] = {0};// 虚拟组织所属的业务分组ID,业务分组根据特定的业务需求制定,一个业 务分组包含一组特定的虚拟组织。
	char download_speed = 0;		// 下载倍速范围(可选),各可选参数以“/”分隔,如设备支持1,2,4倍速下 载则应写为“1/2/4”
	char svc_space_support_mode = 0;// 空域编码能力,取值0:不支持;1:1级增强(1个增强层);2:2级增强(2个增强层);3:3级增强(3个增强层)(可选)
	char svc_time_support_mode = 0;	// 时域编码能力,取值0:不支持;1:1级增强;2:2级增强;3:3级增强(可选)
	*/
}chl_info_t;

// 'DeviceStatus'
typedef struct status_t_
{
    char online = 0;                // 0:OFFLINE 1:ONLINE
    char status = 0;                // 0:OK 1:ERROR
    char time[24] = {0};            // 设备时间 (如: '2021-06-22T14:08:31')
    char encode = 0;                // 0:OFF 1:ON
    char record = 0;                // 0:OFF 1:ON
}status_t;


// 设备基础配置信息
typedef struct basic_param_t_
{
    char name[32] = {0};            // 设备名称
	int   expires = 3600;           // 注册有效期(取值:3600 ~ 100000)
	short keepalive = 60;            // 心跳周期(取值:5 ~ 255)
	short max_k_timeout = 3;        // 最大心跳超时次数(取值:3 ~ 255)
	int position_capability = 0;    // 位置能力
}basic_param_t;

// 基本参数配置: "BasicParam"
// 视频参数范围: "VideoParamOpt"
// SVAC 编码配置: "SVACEncodeConfig"
// SVAC 解码配置: "SVACDe-codeConfig"
// 可同时查询多个配置类型,各类型以“/”分隔,可返回与查询SN值相同的多个响应,每个响应对应一个配置类型。

// 'ConfigDownload'
typedef struct config_t_
{
    basic_param_t basic;
    //video_param_t video;        // 视频参数
    //encode_param_t encode;      // SVAC 编码配置
    //decode_param_t decode;      // SVAC 解码配置
}config_t;


// 'RecordInfo'
typedef struct record_info_t_
{
    char id[32] = {0};              // 设备/区域编码(必选)
    char name[32] = {0};            // 设备/区域名称(必选)
    char file_path[256] = {0};       // 文件路径名 (可选)
    char address[24] = {0};         // 录像地址(可选)
    char start_time[24] = {0};      // 录像开始时间(可选) (如: '2021-06-22T14:50:12')
    char end_time[24] = {0};        // 录像结束时间(可选) (如: '2021-06-23T14:50:12')
    char secrecy = 0;               // 保密属性(缺省为0, 0:不涉密, 1:涉密)
    char type = 1;                  // 录像产生类型(可选)(1:all 2:manual(手动录像) 3:time 4:alarm )
	char recorder_id[32] = {0};		// 录像触发者ID(可选)
	int size = 0;                   // 录像文件大小,单位:Byte(可选)
}record_info_t;

typedef struct dev_info_t_
{
	addr_info_t addr;
	char  parent_id[64] = { 0 };	 // 父设备/区域/系统ID(必选)
	char  platform[24] = {0};        // 平台接入方式(如: 'GB/T28181-2011', 'GB/T28181-2016'等)
	char  type = 1;                  // 设备类型(1:IPC 2:NVR 3:平台)
	char  protocol = 1;              // 数据传输协议(1:UDP 2:TCP 3:UDP+TCP)
	char  username[32] = {0};        // 用户名
	char  password[32] = {0};        // 用户密码(鉴权密码)
    char  name[32] = {0};            // 设备名称
	char  manufacturer[32] = {0};    // 制造商
	char  firmware[32] = {0};        // 固件版本
	char  model[32] = {0};           // 型号
	char  status = 0;		         // 状态(0:离线 1:在线 2:离开[注销])
	int   expires = 3600;            // 注册有效期(取值:3600 ~ 100000)
    short register_interval = 60;    // 注册间隔(取值:60 ~ 600)
	short keepalive = 60;             // 心跳周期(取值:5 ~ 255)
	short max_k_timeout = 3;         // 最大心跳超时次数(取值:3 ~ 255)
	short keepalive_flag = 0;        // 心跳超时标记
	int   channels = 0;              // 通道数
	short share = 1;				 // 是否共享给上级（0:不共享 1:共享）
	short auth = 1;					 // 是否开启鉴权
    cmd_e cmd_type = UNKNOWN_CMD;
	int sn = 1;
	int sum_num = 0;
	int num = 0;
    map<string, chl_info_t> chl_map;    // 通道集合(key为: ParentID#ChannelID)
    map<string, record_info_t> rec_map; // 录像集合(key为录像起始时间)
    status_t* dev_status = NULL;        // 设备状态
    config_t* cfg = NULL;
    addr_info_t *pfm = NULL;

    dev_info_t_() {
        strcpy(platform, "GB/T28181-2016");
        strcpy(password, "123456");
    }
    ~dev_info_t_() {
        if(NULL!= dev_status)
            delete dev_status;
        if(NULL!= cfg)
            delete cfg;
    }
}dev_info_t;

typedef struct local_cfg_t_
{
    dev_info_t dev;         // 与上级平台
	addr_info_t addr;       // 与设备
	addr_info_t media;       // 与设备
	char domain[16] = {0};  // 服务器域
	char password[16] = {0}; // 密码
}local_cfg_t;

typedef struct session_t_
{
	char call_id[64] = { 0 };   // Call ID
	//int d_tid = 0;                // 事务ID (用于Invite回答)
	int d_cid = 0;              // Call ID (用于停止点播)
	int d_did = 0;             // SIP对话的唯一id (用于停止点播,发送ACK)
	//char gid[32] = { 0 };		// 设备或通道ID
	//char token[32] = { 0 };

	int p_tid = 0;                // 事务ID (用于Invite回答)
	int p_cid = 0;
	int p_did = 0;

}session_t;

typedef struct sdp_info_t_
{
	// v
	char v[8] = {0};				// SDP版本 (如:"0")
	// o
	char o_username[32] = {0};		// 用户名 (如:"-"或"34020000002000000001")
	char o_sess_id[32] = {0};		// 会话ID (如:"0")
	char o_sess_version[8] = {0};	// 版本 (如:"0")
	char o_nettype[8] = {0};		// 网络类型 (如: "IN")
	char o_addrtype[8] = {0};		// 地址类型 (如:"IP4")
	char o_addr[16] = {0}; 			// IP地址 (如:"172.16.9.132")
	// s
	char s[32] = {0};				// 会话名称(如: Play:点播请求 Playback:回播请求 Download:文件下载 Talk:语音对讲 EmbeddedNetDVR:)
	// c
	//char c_nettype[8] = {0};		// 网络类型 (如:"IN")
	//char c_addrtype[8] = {0};		// 地址类型 (如:"IP4")
	//char c_addr[16] = {0}; 		// 音视频流的目的地址 (如:"172.16.9.132")
	// t
	time_t t_start_time = 0;
	time_t t_end_time = 0;
	// c
	typedef struct c_connect {
		char nettype[4] = {0};		// 网络类型 (如: "IN")
		char addrtype[4] = {0};		// 地址类型 (如: "IP4")
		char addr[16] = {0};		// 音视频流的目的地址 (如: "192.168.2.105")
		char addr_multicast_ttl[24] = {0};	// TTL value for multicast address (如: )
		char addr_multicast_int[24] = {0};	// Number of multicast address (如: )
	}c_connect_t;
	typedef struct m_media {
		char media[8] = { 0 };		// 媒体 (如: "video","audio")
		int  port = 0;				// 媒体目的端口 (如: 25060)
		char proto[16] = { 0 };		// 传输协议 (如: "RTP/AVP")
		char number_of_port[16] = {0};
		vector<int> payloads;		// 载荷类型 (如: 96 97 98 客户端支持码流格式)
		typedef struct a_attribute {
			char field[24] = {0};	// 字段 (如: "recvonly","sendonly","rtpmap","downloadspeed"等)
			char value[24] = {0};	// 字段值 (如: "96 PS/90000","97 MPEG4/90000","98 H264/90000"等)
		}a_attribute_t;
		// a
		vector<a_attribute_t> a;	// 媒体属性 (key:field_name; value:field_value 如:"a=rtpmap:96 PS/90000")
		// c
		c_connect_t c;				// 媒体连接信息 (如: "c=IN IP4 192.168.2.105")
	}m_media_t;
	// m
	map<string, m_media_t> m;		// 媒体信息 (key:"video","audio"; value:m_media_t 如: "m=video 5080 RTP/AVP 96 97 98")
}sdp_info_t;

///////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////
typedef struct sip_to_t_
{
#if 0
    char id[32] = {0};
    char ip[24] = {0};
    int  port   = 0;
    char tag[32] = {0};
#else
	char scheme[16] = {0};		// Uri Scheme (sip or sips)  如: "sip"
	char username[32] = { 0 };	// Username
	char password[32] = { 0 };	// Password
	char ip[24] = { 0 };		// Domain
	int  port = 0;				// Port number
	char tag[32] = { 0 };
#endif
}sip_to_t;
typedef sip_to_t sip_from_t;
typedef sip_to_t sip_contact_t;
typedef struct sip_via_t_
{
    char version[4] = {0};      // SIP版本(如:'2.0')
    char protocol[4] = {0};     // 传输协议(如:'UDP')
    char ip[24] = {0};          // IP地址
    int  port   = 0;            // 端口
    int  rport   = 0;           // 端口
    char branch[64] = {0};      // branch
}sip_via_t;


#if 1

// 'DeviceInfo'
typedef struct response_dev_info_t_
{
    //char id[32] = {0};            // 设备ID
    char name[32] = {0};            // 设备名称
	char manufacturer[32] = {0};    // 制造商
	char firmware[32] = {0};        // 固件版本
	char model[32] = {0};           // 型号
    int  channels = 0;              // 通道数
}response_dev_info_t;

// 'DeviceStatus'
typedef struct response_dev_status_t_
{
    //char id[32] = {0};              // 设备ID
    char online = 0;                // 0:OFFLINE 1:ONLINE
    char status = 0;                // 0:OK 1:ERROR
    char time[24] = {0};            // 设备时间 (如: '2021-06-22T14:08:31')
    char encode = 0;                // 0:OFF 1:ON
    char record = 0;                // 0:OFF 1:ON
}response_dev_status_t;

// 'Catalog'
typedef struct response_catalog_t_
{
    //char id[32] = {0};              // 设备ID
    int sum_num = 0;                // item的总条数
    short num = 0;                  // 当前response中item的条数
    map<string,chl_info_t> chl_map; // 通道集合
}response_catalog_t;

// 'RecordInfo'
typedef struct response_record_info_t_
{
    //char id[32] = {0};              // 设备ID
    int sum_num = 0;                // item的总条数
    short num = 0;                  // 当前response中item的条数
    map<string,record_info_t> rcd_map; // 录像集合

}response_record_info_t;

#endif


typedef struct sip_body_t_
{
    char opt[16] = {0};
	cmd_e cmd_type = UNKNOWN_CMD;
	int  sn = 0;
    char id[32] = {0};              // 设备ID

    response_dev_info_t*   dev_info = NULL;
    response_dev_status_t* dev_status = NULL;
    response_catalog_t*    catalog = NULL;
    basic_param_t*         basic_param = NULL;
    ~sip_body_t_()
    {
        if(NULL != dev_info)
            delete dev_info;
        if(NULL != dev_status)
            delete dev_status;
        if(NULL != catalog)
            delete catalog;
        if(NULL != basic_param)
            delete basic_param;
    }
}sip_body_t;


// 回播控制
typedef struct playback_ctrl_t_
{
	typedef enum playback_cmd_ {
		CTRL_PLAY = 1,
		CTRL_PAUSE,
		CTRL_FF,
		CTRL_SLOW,
		CTRL_TEAR
	}playback_cmd;
	playback_cmd opt;

	char cmd[16] = {0};
	int range = 0;
	float scale = 0;
	char pause_time[4] = {0};
}playback_ctrl_t;

typedef struct unis_record_
{
	char filename[256] = { 0 };
	//char dev_id[32] = { 0 };
	int duration = 0;
	time_t start_time = 0;
	time_t end_time = 0;
	FILE *fp = NULL;
	//char date[16] = { 0 };		// 录像日期
	//char duration[16] = { 0 };	// 录像时长
	int size = 0;				// 文件大小
	//char filename[64] = { 0 };	// 文件名称
	short encode = 3;			// 视频编码格式(1:h.264 2:h.265 3:其他)
	char call_id[32] = {0};		// 测试
	FILE *mp4 = NULL;			// 测试
	char filename_mp4[128] = { 0 }; // 测试
}unis_record_t;

typedef struct operation_t_
{
	cmd_e cmd = UNKNOWN_CMD;
    //addr_info_t dev;
	//int 	sn = 1;
	const char *call_id = NULL;
    const char *body = NULL;
	//unsigned char ptz_speed = 127;
    addr_info_t media;                  // 点播或回播时的媒体地址
    record_info_t* record = NULL;
    basic_param_t* basic_param = NULL;
    playback_ctrl_t* pbk_ctrl = NULL;
	
	//char path[128] = { "C:/" };	//录像路径
	//int stop_second = 3600;

	unis_record_t *unis_record = NULL;
	operation_t_() {

	}
}operation_t;

typedef struct udp_msg
{
    addr_info_t dev;
    char media_ip[24] = {0};
    int media_port = 0;
	char call_id[32] = { 0 };
	cmd_e cmd = UNKNOWN_CMD;
	//char path[128] = {0};	//录像路径

	time_t start_time;
	time_t end_time;
	unsigned char response = 0;
}udp_msg_t;

typedef struct user_data
{
#if 0
	char gid[64] = { 0 };
	char token[64] = { 0 };
#else
	char gid[128] = { 0 };	// 测试
	char token[64] = { 0 };// 测试
	char filename[128] = { 0 }; // 测试
	FILE *fp = NULL; // 测试
	char filename_mp4[128] = { 0 }; // 测试
	FILE *fp_mp4 = NULL; // 测试
	int n = 10;	// 测试
#endif
}user_data_t;


#pragma pack (1)	// 按2字节对齐
typedef struct rtp_header_t {
	/* byte 0 */
	unsigned char csrc_len : 4;       /* expect 0 */
	unsigned char extension : 1;      /* expect 1 */
	unsigned char padding : 1;        /* expect 0 */
	unsigned char version : 2;        /* expect 2 */
	/* byte 1 */
	unsigned char payload : 7;
	unsigned char marker : 1;        /* expect 1 */
	/* bytes 2, 3 */
	unsigned short seq_no;
	/* bytes 4-7 */
	unsigned  int timestamp;
	/* bytes 8-11 */
	unsigned int ssrc;            /* stream number is used here. */
} rtp_header_t;
#pragma pack ()	// 取消指定对齐，恢复缺省对齐




#endif