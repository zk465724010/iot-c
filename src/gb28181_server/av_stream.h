
#ifndef __AV_STREAM_H__
#define __AV_STREAM_H__

#include "Thread11.h"

#define USE_AUDIO_STREAM
//#define USE_WRITE_FILE
#define USE_UDP_CLIENT

#ifdef _WIN32
	extern "C"
	{
	#include <libavformat/avformat.h>
	#include  <libavformat/avio.h>
	#include  <libavcodec/avcodec.h>
	#include  <libavutil/avutil.h>
	#include  <libavutil/time.h>
	#include  <libavutil/log.h>
	#include  <libavutil/mathematics.h>
	#include <libswscale/swscale.h>
	#include <libavutil/opt.h>
	#include <libavutil/imgutils.h>
	#include <libavutil/frame.h>
	#include <libavutil/channel_layout.h>
	};
	#if 0
	#pragma comment(lib,"avformat.lib")
	//工具库，包括获取错误信息等
	#pragma comment(lib,"avutil.lib")
	//编解码的库
	#pragma comment(lib,"avcodec.lib")
	#endif
	#define STDCALL __stdcall
#elif __linux__
	#ifdef __cplusplus
	extern "C"
	{
	#endif
	#include <libavformat/avformat.h>
	#include <libavformat/avio.h>
	#include  <libavcodec/avcodec.h>
	#include  <libavutil/avutil.h>
	#include  <libavutil/time.h>
	#include  <libavutil/log.h>
	#include  <libavutil/mathematics.h>
	#include <libavutil/channel_layout.h>
	#ifdef __cplusplus
	};
	#endif
	#define STDCALL __attribute__((__stdcall__))
#endif

//======== 测试 ================
#define CACHE_MAX_SIZE 8192*1024*2
typedef struct cache_buff
{
	uint8_t* data = NULL;
	uint8_t* pos = NULL;
	int size = 0;
	int length = 0;
	~cache_buff() {
		if (NULL != data) {
			av_free(data);
			data = NULL;
			printf("INFO: --------- Cache free ----------\n\n");
		}
	}
}cache_buff_t;
#ifdef USE_UDP_CLIENT
typedef struct udp_addr_t {
	char ip[16] = { 0 };		// 本地ip
	int port = 0;				// 本地port
	char peer_ip[16] = { 0 };	// 对方ip
	int peer_port = 0;			// 对方port
}udp_addr_t;

#endif

typedef int(STDCALL *STREAM_CBK)(void *pData, int nSize, void *pUserData);
typedef int(*READ_CBK)(void *opaque, uint8_t *buf, int buf_size);
typedef int(*WRITE_CBK)(void *opaque, uint8_t *buf, int buf_size);
typedef int64_t(*SEEK_CBK)(void *opaque, int64_t offset, int whence);
typedef int (*custom_cbk)(void* opaque, unsigned char* buf, int buf_size);
//typedef int (*write_stream)(void * opaque,unsigned char *buf, int bufsize);

class CAVStream
{
public:
	CAVStream();
	~CAVStream();

	int open(const char *url, STREAM_CBK cbk = NULL, STREAM_CBK cbk2 = NULL, void *arg = NULL, int arg_size = 0);
	#ifdef USE_UDP_CLIENT
	int open(const char *ip, int port, STREAM_CBK cbk = NULL, STREAM_CBK cbk2 = NULL, void *arg = NULL, int arg_size = 0);
	#endif

	int open_file(const char *filename, udp_addr_t *addr, STREAM_CBK cbk = NULL, void *arg = NULL, int arg_size = 0);

	// 快照
	int snapshoot(const char* ip, int port, const char* filename);

	void close(bool is_sync = true);
	//void set_callback(STREAM_CBK cbk, void *arg = NULL, int arg_size = 0);

	const char *errstr(int err);
	int print_ffmpeg_error(int err);

	void join();
	void detach();

	int seek(int pos, int flags);// 拖放
	void speed(float value);	// 播放速度(0.5, 1.0, 2.0 等)
	void pause(bool flag = true);// 暂停播放
	void continu(bool flag = true);	// 继续播放

	STATE status();

private:
	int init();
	void release();
	int save_to_jpeg(AVFrame* frame, int w, int h, const char* filename, custom_cbk cbk = NULL, void* arg = NULL);

	static int write_buffer(void *arg, uint8_t *buf, int buf_size);
	bool m_mod_head = true;		// 是否修改头

	bool m_head_flag = false;
	uint8_t* m_head_data = NULL;
	int m_head_len = 0;

#ifdef USE_UDP_CLIENT
private:
	static int read_buffer(void *arg, uint8_t *buff, int buf_size);
public:
	void *m_udp = NULL;
#endif

private:
	STREAM_CBK m_pCallback = NULL;				// 原始流输出
	void*	m_pUserData = NULL;
	int		m_nUserDataSize = 0;

	STREAM_CBK m_pCallback2 = NULL;				// MP4流输出

	AVFormatContext* m_in_fmt_ctx = NULL;		// 输入格式上下文
	AVFormatContext* m_out_fmt_ctx = NULL;		// 输出格式上下文

	AVCodecContext* m_pInAudioCodectx = NULL;
	AVCodecContext* m_pOutAudioCodectx = NULL;
	AVStream *m_pOutVideoStream = NULL;
	AVStream *m_pOutAudioStream = NULL;

	// Input
	int m_in_video_index = -1;
	int m_in_audio_index = -1;

	// Output
	int m_out_video_index = -1;
	int m_out_audio_index = -1;

	bool m_is_pause = false;		// 轮播

	FILE* m_file = NULL;

	// ------- 测试 -------
	cache_buff_t m_cache_buff;

private:
	READ_CBK  m_read_packet = NULL;
	WRITE_CBK m_write_packet = NULL;
	SEEK_CBK  m_seek = NULL;

private:
	CThread11 m_stream_thd;
	static void STDCALL on_stream(STATE &state, PAUSE &p, void *pUserData);// 点播
	static void STDCALL on_playback(STATE &state, PAUSE &p, void *pUserData);// 回放
	static void STDCALL on_readfile(STATE &state, PAUSE &p, void *pUserData);// 读文件
};



#endif
