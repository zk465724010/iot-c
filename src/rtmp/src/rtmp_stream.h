
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
	//	#include "libavdevice/avdevice.h"	// ��ý���豸���������
	//	#include "libswscale/swscale.h"		// �����ʽת��
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
	//	#include "libavdevice/avdevice.h"	// ��ý���豸���������
	//	#include "libswscale/swscale.h"		// �����ʽת��
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
	char id[32] = { 0 };				// ����ID
	char input[128] = { 0 };			// ����
	char device_name[32] = {0};			// �豸���ƻ���(����ͷ,��Ļ��)
	int stimeout = 5000000;				// ��ʱʱ��,��δ����'stimeout'��,��IPC����ʱ,av_read_frame����������(��λ:΢��)
	int buffer_size = 1024000;			// ��������С
	char rtsp_transport[8] = "tcp";	// RTSP����ģʽ(ȡֵ:tcp, udp)
	int64_t start_time = 0;				// ¼�񲥷���ʼʱ��
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

	char output[128] = { 0 };			// ���
	char output_format[8] = "flv";	// �����ʽ����(ȡֵ:mp4, flv, mpegts...)
	char opt = 1;						// �������� (1:���� 2:����)
}stream_param_t;

typedef struct rtmp_user_cbk
{
	// ԭʼ���ص�(����)
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

	AVCodecContext* make_encoder(AVStream* s, const stream_param_t* params); // ������
	AVCodecContext* make_decoder(AVStream* s, const stream_param_t* params); // ������
	int free_coder(AVCodecContext** coder);	// ���ٱ������

	void print_codec_context(AVCodecContext *ctx);
private:
	AVFormatContext *m_ifmt_ctx = NULL;
	AVFormatContext *m_ofmt_ctx = NULL;
	int m_video_index = -1;
	int m_audio_index = -1;
	rtmp_user_cbk_t m_cbk;

	AVCodecContext* m_h265_codec_ctx = NULL;	// H265����������
	AVCodecContext* m_h264_codec_ctx = NULL;	// H264����������

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

