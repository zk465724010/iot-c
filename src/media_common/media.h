#ifndef MEDIA_COMMON_H
#define MEDIA_COMMON_H

// 2022.3.21 zk
#include "minio.h"
#include "Thread11.h"


//write stream function pointer
#define CACHE_BIT_MAZ_WIDTH 8192*1024
#define CACHE_TIME_SECOND 2
enum media_error_code{
    NO_ERRORS=0,            //无错误
    ERROR_ARG,              //参数错误
    ERROR_OPEN_URL_FAIL,    //打开rtsp url 失败
    ERROR_OPEN_FILE_FAIL,   //打开分片文件失败
    ERROR_STREAM_INFO_FAIL,//分析流信息失败
    ERROR_OPEN_OUTSTREAM_FAIL,//打开输出失败
    ERROR_CREATE_AVSTREAM_FAIL,//创建输出流失败
    ERROR_WRITE_PKT_FAIL,   //写包失败
    ERROR_NO_VIDEO_STREAM,  //没有视频流
    ERROR_SEEK_FAIL,        //拖动失败
    ERROR_READ_FRAME_FAIL,  //读包失败
    ERROR_MEM_FAIL,         //内存分配错误
    ERROR_IOCONTENT_FAIL,   //创建输出IO失败
    ERROR_FIND_CODE_FAIL,   //查找编码格式失败
    ERROR_CREATE_CODECOTENT_FAIL, //分配编码上下文失败
    ERROR_OPEN_CODE_FAIL ,  //打开解码失败
    ERROR_DECODE_PACKET_FAIL,//解码失败
    ERROR_ENCODE_PACKET_FAIL,//编码失败
    VIDEO_ONLY,             //只有视频流
    VIDEO_AND_AUDIO,        //音视频一起
    FILE_EOF                //文件结束
};

typedef struct _stream_cache_buff
{
    unsigned char* cache_buff = NULL;
    unsigned char* pos = NULL;
    unsigned char* limit = NULL;
    int write_size = 0;
    bool video_head = false;    // 是否是视频头帧
    bool reconnet = false;      // 是否是断流重连后的视频头帧
} stream_cache_buff;

///////////////////////////////////////////////////////////////////////////////////////////
//#define BUFF_SIZE  5242880
//#define BUFF_SIZE  524288       // 512K
#define BUFF_SIZE  1048576       // 1M
//#define BUFF_SIZE  32768      // 32k
//#define BUFF_SIZE  10240      // 10k
//#define BUFF_SIZE  1024       // 1k

typedef int (*custom_cbk)(void* opaque, unsigned char* buf, int buf_size);
typedef struct tag_user_data {
    CMinIO* minio = NULL;
    void* arg = NULL;
    int *part_number = NULL;
    Aws::String upload_id;
}user_data_t;
///////////////////////////////////////////////////////////////////////////////////////////


typedef int (*write_stream)(void * opaque,unsigned char *buf, int bufsize);
extern int rtsp_stream(const char * rtsp_url, void* fun_arg,write_stream writedata_dealfun,char * control_cmd);
extern int slice_mp4(const char* slice_file,int start_time,void* fun_arg,write_stream writedata_dealfun, char * control_cmd,float* speed);
extern int rtsp_slice(const char * rtsp_url,const  char* filename, char * control_cmd);
extern int rtsp_muti_slice(const char * rtsp_url,const  char** filenames,int slice_num,int slice_second, char * control_cmd);
extern int rtsp_stream_direct(const char * rtsp_url, void* fun_arg,write_stream writedata_dealfun,char* control_cmd);
extern int rtsp_slice_autostop(const char * rtsp_url,const  char* filename, char * control_cmd,int stopsecond,int* video_type,int* recordtime, custom_cbk cbk=NULL, void* arg=NULL);
extern int slice_h265toh264_mp4(const char* slice_file,int start_time,void* fun_arg,write_stream writedata_dealfun,char* control_cmd);
extern int rtsp_slice_codeautostop(const char * rtsp_url,const  char* filename, char * control_cmd,int stopsecond,int* video_type,int* recordtime,int dropInterval, custom_cbk cbk = NULL, void* arg = NULL);
extern int tcp_h264_stream(const char * tcp_ip,int port, void* fun_arg,write_stream writedata_dealfun,char* control_cmd);
extern int tcp_slice_autostop(const char * tcp_ip,int tcp_port,const  char* filename, char * control_cmd,int stopsecond,int* video_type,int* recordtime);

///////////////////////////////////////////////////////////////////////////////////////////
// 2022.3.21 zk
typedef struct tag_stream_param {
    // Input
    const char* in_url = NULL;     // 输入URL(如:rtsp,本地文件路径等)
    int (*in_custom)(void* opaque, uint8_t* buf, int buf_size) = NULL;// 自定义输入回调
    void* in_opaque = NULL;
    int64_t start_time = 0;         // 录像起始时间(播放本地录像文件时有效)
    float* ratio = NULL;            // 播放倍率,回放时有效 (共享变量)
    /// /////////////////////////////
    // Output
    const char* out_url = NULL;    // 输出文件名称
    int (*out_custom)(void* opaque, uint8_t* buf, int buf_size) = NULL;// 自定义输出回调
    void* out_opaque = NULL;
    // test
    const char* chl = NULL;         // 通道ID
}stream_param_t;
typedef struct user_data2_t {
    int64_t start = 0;
    int64_t step = 0;
    int64_t filesize = 0;
    const char* filename = NULL;
}user_data2_t;
typedef struct IOContext {
    int64_t pos = 0;        // 当前位置
    int64_t filesize = 0;   // 文件大小
    uint8_t* fuzz = NULL;   // 数据起始指针
    int64_t fuzz_size = 0;   // 剩余数据大小
    const char* remote_name = NULL;
    bool enable = true;
} IOContext;

int video_to_image(const char* url, const char* filename, custom_cbk cbk = NULL, void *arg=NULL);
int video_stream(char* control_cmd, stream_param_t *param);
int write_packet(void* opaque, unsigned char* buf, int buf_size);

int playback_stream(char* control_cmd, stream_param_t* param);
int read_packet(void* opaque, uint8_t* buf, int buf_size);
int playback_write_packet(void* opaque, unsigned char* buf, int buf_size);
int64_t seek_packet(void* opaque, int64_t offset, int whence);
void STDCALL download_thdproc(STATE& state, PAUSE& p, void* pUserData);
int STDCALL minio_callback(void* data, size_t size, void* arg);
int create_object_cbk(void*& obj, int& obj_size, void* user_data);
void destory_object_cbk(void*& obj, void* user_data);


#endif // MEDIA_COMMON_H
