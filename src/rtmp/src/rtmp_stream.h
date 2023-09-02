
#ifndef __RTMP_STREAM_H__
#define __RTMP_STREAM_H__

#include "thread11.h"
#ifdef _WIN32
	//extern "C"
	//{
	//	#include "libavformat/avformat.h"
	//	#include "libavcodec/avcodec.h"
	//	#include "libavcodec/packet.h"
	//	#include "libavutil/mathematics.h"
	//	#include "libavutil/time.h"
	//	#include "libavdevice/avdevice.h"	// 多媒体设备交互的类库
	//	#include "libswscale/swscale.h"		// 编码格式转换
	//};
	#define STDCALL __stdcall
#elif __linux__
	//#ifdef __cplusplus
	//extern "C"
	//{
	//#endif
	//	#include "libavformat/avformat.h"
	//	#include "libavcodec/avcodec.h"
	//	#include "libavcodec/packet.h"
	//	#include "libavutil/mathematics.h"
	//	#include "libavutil/time.h"
	//	#include "libavdevice/avdevice.h"	// 多媒体设备交互的类库
	//	#include "libswscale/swscale.h"		// 编码格式转换
	//#ifdef __cplusplus
	//};
	//#endif
	#define STDCALL __attribute__((__stdcall__))
#endif

#ifdef __cplusplus
extern "C" {
#endif
#include "libavformat/avformat.h"
#ifdef __cplusplus
};
#endif


typedef struct stream_param
{
	char id[32] = { 0 };				// 流的ID
	char input[128] = { 0 };			// 输入
	char device_name[32] = {0};			// 设备名称或编号(摄像头,屏幕等)
	int stimeout = 5000000;				// 超时时间,若未设置'stimeout'项,当IPC掉线时,av_read_frame函数会阻塞(单位:微妙)
	int buffer_size = 1024000;			// 缓冲区大小
	char rtsp_transport[8] = "tcp";	// RTSP传输模式(取值:tcp, udp)
	int64_t start_time = 0;				// 录像播放起始时间
	/////////////////////////////////////
	int time_base = 0;		// 1/25
	int framerate = 0;		// 25/1
	int r_frame_rate = 0;	// 25/1
	int bit_rate = 0;		// 1600000
	int gop_size = 0;		// 50
	int qmax = 0;			// 51
	int qmin = 0;			// 10
	int keyint_min = 0;		// 50
	int codec_tag = 0;
	//-----------------------------
	int me_range = 0;
	int max_qdiff = 0;
	int max_b_frames = 0;
	float qcompress = 0.0;
	float qblur = 0.0;
	int thread_count = 0;
	/////////////////////////////////////

	char output[128] = { 0 };			// 输出
	char output_format[8] = "flv";	// 输出格式名称(取值:mp4, flv, mpegts...)
	char opt = 1;						// 操作类型 (1:推流 2:拉流)
}stream_param_t;

typedef struct rtmp_user_cbk
{
	// 原始流回调(裸流)
	int(STDCALL* stream_cbk)(const void* data, int size, int width, int height, int pix_fmt, void* user_data) = NULL;
	void *user_data = NULL;
}rtmp_user_cbk_t;

class CRtmpStream
{
public:
	CRtmpStream();
	~CRtmpStream();

	int open(const stream_param_t *param);
	void close();

	void print_dshow_device(const char *device_name);
	void print_vfw_device();
	void print_avfoundation_device(); // Mac OS

private:
	int init();
	void release();

	AVCodecContext* make_encoder(AVStream* s, const stream_param_t* params); // 编码器
	AVCodecContext* make_decoder(AVStream* s, const stream_param_t* params); // 解码器
	int free_coder(AVCodecContext** coder);	// 销毁编解码器

	void print_codec_context(AVCodecContext *ctx);
private:
	AVFormatContext *m_ifmt_ctx = NULL;
	AVFormatContext *m_ofmt_ctx = NULL;
	int m_video_index = -1;
	int m_audio_index = -1;
	rtmp_user_cbk_t m_cbk;

	AVCodecContext* m_h265_codec_ctx = NULL;	// H265解码上下文
	AVCodecContext* m_h264_codec_ctx = NULL;	// H264编码上下文

#ifdef __THREAD11_H__
public:
	void pause(){ m_thd.Pause();}
	void continu() { m_thd.Continue();}
	void join() { m_thd.Join();}
	STATE state() { m_thd.GetState(); }

private:
	CThread11	m_thd;
	static void STDCALL ThdProc(STATE &state, PAUSE &p, void *pUserData);
#endif
};

#endif

