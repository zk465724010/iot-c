#ifndef VIDEO_QUALITY
#define VIDEO_QUALITY
enum error_code{
    NO_ERROR=0, //无错误
    ERROR_ARG,  //参数错误
    ERROR_OPEN_URL_FAIL, //打开rtsp url 失败
    ERROR_OPEN_FILE_FAIL, //打开分片文件失败
    ERROR_STREAM_INFO_FAIL,//分析流信息失败
    ERROR_OPEN_OUTSTREAM_FAIL,//打开输出失败
    ERROR_CREATE_AVSTREAM_FAIL,//创建输出流失败
    ERROR_WRITE_PKT_FAIL,//写包失败
    ERROR_NO_VIDEO_STREAM,//没有视频流
    ERROR_SEEK_FAIL,//拖动失败
    ERROR_READ_FRAME_FAIL,//读包失败
    ERROR_MEM_FAIL,//内存分配错误
    ERROR_IOCONTENT_FAIL,//创建输出IO失败
    ERROR_FIND_CODE_FAIL, //查找编码格式失败
    ERROR_CREATE_CODECOTENT_FAIL, //分配编码上下文失败
    ERROR_OPEN_CODE_FAIL //打开解码失败
};
extern int video_quality(char* url);
extern int create_same_video(const char* rtsp_url,const char* out_file);
#endif  
