
#include "av_stream.h"
#include "log.h"
#include <assert.h>
#include <queue>			// queue<xxx>
using namespace  std;

#ifdef USE_UDP_CLIENT
#include "udp_client.h"
#include "common.h"
#endif

#ifdef _WIN32
//#include <WS2tcpip.h>		// inet_ntop
#include <io.h>				//_access() 函数


#elif __linux__
#include<unistd.h>			// access()函数
#include <sys/socket.h>
#include <arpa/inet.h>		// htonl(),htons()
#endif

#include "h264.h"

/*
#include <sys/socket.h>
//#include <cstdlib>
#include <string.h>
#include <arpa/inet.h>
#include<unistd.h>		// close
//#include<netinet/in.h>
//#include<sys/types.h>
//#include<sys/select.h>
#include <sys/select.h>
*/

#define BUFF_SIZE  10240
#define MAX_READ_FRAME_FAIL 25
#define MAX_WRITE_FRAME_FAIL 25

typedef enum errcode_e
{
	ERR_OK = 0,
	ERR_UNKNOWN = -1,
	ERR_ALLOC_FAILED = -1000,
	ERR_PARAM_ERROR = ERR_ALLOC_FAILED - 1
}errcode_e;

const char *CAVStream::errstr(int err)
{
	static char buff[64] = {0};
	switch (err)
	{
	case ERR_OK: sprintf(buff, "Successful\0");
	case ERR_ALLOC_FAILED: sprintf(buff, "Failed to allocate memory\0");
	case ERR_PARAM_ERROR: sprintf(buff, "Parameter error\0");
	default :  sprintf(buff, "Unknown error %d\0", err);
	}
	return buff;
}

CAVStream::CAVStream()
{
	init();
}

CAVStream::~CAVStream()
{
	release();
}

int CAVStream::init()
{
	int nRet = ERR_OK;
	//av_register_all();
	//avcodec_register_all();
	avformat_network_init();

	// Input format context
	m_in_fmt_ctx = avformat_alloc_context();
	if (NULL == m_in_fmt_ctx) {
		nRet = ERR_ALLOC_FAILED;
		LOG("ERROR","m_in_fmt_ctx=%p '%s'\n\n", m_in_fmt_ctx, errstr(nRet));
	}
	return nRet;
}
void CAVStream::release()
{
	if (NULL != m_in_fmt_ctx) {
		//avformat_close_input(&m_in_fmt_ctx);
		#if 0
		if (NULL != m_in_fmt_ctx->pb) {
			if (NULL != m_in_fmt_ctx->pb->buffer) {
				av_free(m_in_fmt_ctx->pb->buffer);
				m_in_fmt_ctx->pb->buffer = NULL;
			}
			avio_context_free(&m_in_fmt_ctx->pb);
			m_in_fmt_ctx->pb = NULL;
		}
		#endif
		avformat_free_context(m_in_fmt_ctx);
		m_in_fmt_ctx = NULL;
	}
	if (NULL != m_out_fmt_ctx) {
		#if 0
		if (NULL != m_out_fmt_ctx->pb) {
			if (NULL != m_out_fmt_ctx->pb->buffer) {
				av_free(m_out_fmt_ctx->pb->buffer);
				m_out_fmt_ctx->pb->buffer = NULL;
			}
			avio_context_free(&m_out_fmt_ctx->pb);
			m_out_fmt_ctx->pb = NULL;
		}
		#endif
		avformat_free_context(m_out_fmt_ctx);
		m_out_fmt_ctx = NULL;
	}
	m_pCallback = NULL;
	m_pCallback2 = NULL;
	if ((NULL != m_pUserData) && (m_nUserDataSize > 0)) {
		free(m_pUserData);
		m_pUserData = NULL;
		m_nUserDataSize = 0;
	}
	#ifdef USE_UDP_CLIENT
	if (NULL != m_udp) {
		CUdpClient *client = (CUdpClient*)m_udp;
		client->Close();
		delete client;
		m_udp = NULL;
	}
	#endif
}

int CAVStream::open(const char *url, STREAM_CBK cbk/*=NULL*/, STREAM_CBK cbk2/*=NULL*/, void *arg/*=NULL*/, int arg_size/*=0*/)
{
	if (NULL == m_in_fmt_ctx) {
		LOG("ERROR","m_in_fmt_ctx=%p\n\n", m_in_fmt_ctx);
		return -1;
	}
	int nRet = ERR_UNKNOWN;
	//////////////// Input ////////////////////////
	if (NULL != url)
	{
		#ifdef _WIN32
		if (0 != _access(url, 0))
		#elif __linux__
		if(0 != access(url, F_OK))
		#endif
		{
			LOG("ERROR","Video file does not exist '%s'\n\n", url);
			return -1;
		}
		if ((NULL != m_pUserData) && (m_nUserDataSize > 0)) {
			free(m_pUserData);
			m_pUserData = NULL;
			m_nUserDataSize = 0;
		}
		m_pCallback = cbk;
		m_pCallback2 = cbk2;
		m_pUserData = arg;
		if ((NULL != arg) && (arg_size > 0)) {
			m_pUserData = malloc(arg_size + 1);
			if (NULL != m_pUserData) {
				memset(m_pUserData, 0, arg_size + 1);
				memcpy(m_pUserData, arg, arg_size);
				m_nUserDataSize = arg_size;
			}
		}

		const AVInputFormat *ifmt = av_find_input_format(url);
		AVDictionary *opts = NULL;
		//av_dict_set_int(&opts, "rtbufsize", 1843200000, 0);
		//av_dict_set(&opts, "buffer_size", "2097152", 0);
		//av_dict_set(&opts, "max_delay", "0", 0);
		//av_dict_set(&opts, "stimeout", "6000000", 0); //  ��������ʱ�����ó�ʱ(rtsp)
		nRet = avformat_open_input(&m_in_fmt_ctx, url, (AVInputFormat*)ifmt, &opts);
	}
	else {
		LOG("ERROR","%s\n\n", errstr(ERR_PARAM_ERROR));
		return ERR_PARAM_ERROR;
	}
	LOG("INFO","avformat_open_input('%s') = %d \n\n", url, nRet);
	if (nRet >= 0) {
		nRet = avformat_find_stream_info(m_in_fmt_ctx, NULL);
		LOG("INFO","avformat_find_stream_info = %d\n\n", nRet);
		av_dump_format(m_in_fmt_ctx, 0, NULL, 0);

		//int second, h, m, s;
		//second = (m_in_fmt_ctx->duration) / 1000000;
		//h = second  / 3600;
		//m = (second % 3600) / 60;
		//s = (second % 60);
		//printf("Duration:%02d:%02d:%02d [duration:%lld second:%d]\n\n", h, m, s, m_in_fmt_ctx->duration, second);
		//getchar();

		if (nRet >= 0) {
			for (int i = 0; i < m_in_fmt_ctx->nb_streams; i++) {
				AVStream *in_stream = m_in_fmt_ctx->streams[i];
				if (in_stream->codecpar->codec_type == AVMEDIA_TYPE_VIDEO)
					m_in_video_index = i;	// Video index
				#ifdef USE_AUDIO_STREAM
				else if (in_stream->codecpar->codec_type == AVMEDIA_TYPE_AUDIO)
					m_in_audio_index = i;	// Audio index
				#endif
			}
		}
		else {
			print_ffmpeg_error(nRet);
			LOG("ERROR","avformat_find_stream_info failed %d\n\n", nRet);
			release();
			return nRet;
		}
	}
	else {
		print_ffmpeg_error(nRet);
		LOG("ERROR","avformat_open_input failed %d\n\n", nRet);
		release();
		return nRet;
	}

	//////////////// Output ////////////////////////
	if ((nRet >= 0) && (NULL != cbk2))
	{		
		if (NULL != m_cache_buff.data) {
			memset(&m_cache_buff, 0, sizeof(cache_buff_t));
		}
		m_cache_buff.data = (uint8_t*)av_malloc(CACHE_MAX_SIZE+1);
		if (NULL != m_cache_buff.data) {
			m_cache_buff.pos = m_cache_buff.data;
			m_cache_buff.size = CACHE_MAX_SIZE;
		}
		
		nRet = avformat_alloc_output_context2(&m_out_fmt_ctx, NULL, "mp4", NULL);
		assert((nRet >= 0) && (NULL != m_out_fmt_ctx));
		unsigned char* buff_out = (uint8_t*)av_malloc(BUFF_SIZE);
		m_out_fmt_ctx->pb = avio_alloc_context(buff_out, BUFF_SIZE, 1, this, NULL, write_buffer, NULL);
		m_out_fmt_ctx->flags = AVFMT_FLAG_CUSTOM_IO;

		#if 0
		// Create streams in output
		for (int i = 0; (nRet >= 0) && (i < m_in_fmt_ctx->nb_streams); i++) {
			#ifdef USE_AUDIO_STREAM
			if ((m_in_fmt_ctx->streams[i]->codecpar->codec_type != AVMEDIA_TYPE_VIDEO) &&
				(m_in_fmt_ctx->streams[i]->codecpar->codec_type != AVMEDIA_TYPE_AUDIO))
			#else
			if (m_in_fmt_ctx->streams[i]->codecpar->codec_type != AVMEDIA_TYPE_VIDEO)
			#endif
			{
				continue;
			}
			AVCodec* codec = avcodec_find_decoder(m_in_fmt_ctx->streams[i]->codecpar->codec_id);
			AVStream *out_stream = avformat_new_stream(m_out_fmt_ctx, codec);
			//AVMediaType out_media_type = out_stream->codecpar->codec_type;
			//AVMediaType out_media_type = m_in_fmt_ctx->streams[i]->codecpar->codec_type;
			if (m_in_fmt_ctx->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO)
				m_out_video_index = out_stream->index;	// Video index
			#ifdef USE_AUDIO_STREAM
			else if (m_in_fmt_ctx->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_AUDIO)
				m_out_audio_index = out_stream->index;	// Audio index
			#endif
			else {
				LOG("ERROR","out_media_type=%d\n\n", m_in_fmt_ctx->streams[i]->codecpar->codec_type);
				continue;
			}
			//Copy the settings of AVCodecContext
			AVCodecContext* pCodecCtx = avcodec_alloc_context3(codec);
			if (NULL != pCodecCtx) {
				int ret = avcodec_parameters_to_context(pCodecCtx, m_in_fmt_ctx->streams[i]->codecpar);
				if (ret < 0)
					LOG("ERROR","avcodec_parameters_to_context failed %d\n\n", ret);
				pCodecCtx->codec_tag = 0;

				if (m_out_fmt_ctx->oformat->flags & AVFMT_GLOBALHEADER)
					pCodecCtx->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;

				ret = avcodec_parameters_from_context(out_stream->codecpar, pCodecCtx);
				if (ret < 0)
					LOG("ERROR","avcodec_parameters_from_context failed %d\n\n", ret);
				avcodec_free_context(&pCodecCtx);
			}
		}
		#else
		/////////// Video
		if (m_in_video_index >= 0) {
			AVStream *in_stream = m_in_fmt_ctx->streams[m_in_video_index];
			const AVCodec* codec = avcodec_find_decoder(in_stream->codecpar->codec_id);
			m_pOutVideoStream = avformat_new_stream(m_out_fmt_ctx, codec);
			assert(NULL != m_pOutVideoStream);
			//Copy the settings of AVCodecContext
			AVCodecContext* pCodecCtx = avcodec_alloc_context3(codec);
			assert(NULL != pCodecCtx);
			int ret = avcodec_parameters_to_context(pCodecCtx, in_stream->codecpar);
			assert(ret >= 0);
			pCodecCtx->codec_tag = 0;
			if (m_out_fmt_ctx->oformat->flags & AVFMT_GLOBALHEADER)
				pCodecCtx->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;
			ret = avcodec_parameters_from_context(m_pOutVideoStream->codecpar, pCodecCtx);
			assert(ret >= 0);
			avcodec_free_context(&pCodecCtx);
		}

		///////////// Audio
		if (m_in_audio_index >= 0) {
			const AVCodec* pInAudioCodec = avcodec_find_decoder(m_in_fmt_ctx->streams[m_in_audio_index]->codecpar->codec_id);
			assert(NULL != pInAudioCodec);
			m_pInAudioCodectx = avcodec_alloc_context3(pInAudioCodec);
			assert(NULL != m_pInAudioCodectx);
			int ret = avcodec_parameters_to_context(m_pInAudioCodectx, m_in_fmt_ctx->streams[m_in_audio_index]->codecpar);
			if (m_pInAudioCodectx->channels <= 0) {
				m_pInAudioCodectx->channels = 2;
				m_pInAudioCodectx->channel_layout = av_get_default_channel_layout(2);
				m_pInAudioCodectx->sample_rate = 16000;
			}
			ret = avcodec_open2(m_pInAudioCodectx, pInAudioCodec, NULL);
			assert(ret >= 0);
			const AVCodec*  pOutAudioCodec = avcodec_find_encoder(AV_CODEC_ID_AAC);
			m_pOutAudioCodectx = avcodec_alloc_context3(pOutAudioCodec);
			assert(NULL != m_pOutAudioCodectx);
			m_pOutAudioCodectx->codec = pOutAudioCodec;
			m_pOutAudioCodectx->sample_rate = m_pInAudioCodectx->sample_rate;
			m_pOutAudioCodectx->channel_layout = m_pInAudioCodectx->channel_layout;
			m_pOutAudioCodectx->channels = m_pInAudioCodectx->channels;
			m_pOutAudioCodectx->sample_fmt = AV_SAMPLE_FMT_FLTP;
			m_pOutAudioCodectx->codec_tag = 0;
			m_pOutAudioCodectx->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;
			m_pOutAudioCodectx->bit_rate = m_pInAudioCodectx->bit_rate;
			m_pOutAudioCodectx->codec_type = AVMEDIA_TYPE_AUDIO;
			ret = avcodec_open2(m_pOutAudioCodectx, pOutAudioCodec, NULL);
			assert(ret >= 0);
			m_pOutAudioStream = avformat_new_stream(m_out_fmt_ctx, pOutAudioCodec);
			assert(NULL != m_pOutAudioStream);
			avcodec_parameters_from_context(m_pOutAudioStream->codecpar, m_pOutAudioCodectx);
		}
		#endif
		av_dump_format(m_out_fmt_ctx, 0, NULL, 1);
		AVDictionary *opts = NULL;
		av_dict_set(&opts, "movflags", "frag_keyframe+empty_moov+omit_tfhd_offset+faststart+separate_moof+disable_chpl+default_base_moof+dash", 0);
		m_head_flag = true;
		nRet = avformat_write_header(m_out_fmt_ctx, &opts);// Write file header
		LOG("INFO","avformat_write_header = %d\n\n", nRet);
		if (NULL != m_pCallback2) {
			char data[32] = { 0 };
			if (m_in_audio_index >= 0) {// 复合流
				strcpy(data, "AAAAAAAAAAAAAAAAAAA");// 长度 19
				m_pCallback2(data, strlen(data), m_pUserData);
			}
			else {			// 视频流
				strcpy(data, "BBBBBBBBBBBBBBBBBB");// 长度 18
				m_pCallback2(data, strlen(data), m_pUserData);
			}
			if ((NULL != m_head_data) && (m_head_len > 0)) {// 输出拼接后的头部信息
				LOG("INFO","-------- head_data=%p, head_len=%d --------\n\n", m_head_data, m_head_len);
				m_pCallback2(m_head_data, m_head_len, m_pUserData);
				free(m_head_data);
				m_head_data = NULL;
				m_head_len = 0;
			}
		}
		m_head_flag = false;
		av_dict_free(&opts);
		if (nRet < 0) {
			LOG("ERROR","avformat_write_header failed %d\n\n", nRet);
			print_ffmpeg_error(nRet);
		}
	}
	//else DEBUG("ERROR: ret=%d wt_cbk=%p\n\n", nRet, wt_cbk);
	if (nRet >= 0) {
		nRet = m_stream_thd.Run(on_playback, this);
		if (0 != nRet)
			LOG("ERROR","Thread running failure %d\n\n", nRet);
	}
	return nRet;
}

int CAVStream::open_file(const char *filename, udp_addr_t *addr, STREAM_CBK cbk/*=NULL*/, void *arg/*=NULL*/, int arg_size/*=0*/)
{
	if ((NULL != filename) && (NULL != addr)) {

		#ifdef _WIN32
		if (0 != _access(filename, 0)) {
		#elif __linux__
		if (0 != access(filename, F_OK)) {
		#endif
			LOG("ERROR","File does not exist '%s'\n\n", filename);
			return -1;
		}

		CUdpClient *udp = new CUdpClient();
		int nRet = udp->Open(addr->ip, addr->port);
		if (0 == nRet) {
			udp->SetPeerAddress(addr->peer_ip, addr->peer_port);
			//int buff_size = 1024 * 1024 * 50;
			//udp->SetSockOption(SO_SNDBUF, &buff_size, sizeof(buff_size));
			m_udp = udp;

			if ((NULL != m_pUserData) && (m_nUserDataSize > 0)) {
				free(m_pUserData);
				m_pUserData = NULL;
				m_nUserDataSize = 0;
			}
			m_pCallback = cbk;
			m_pUserData = arg;
			if ((NULL != arg) && (arg_size > 0)) {
				m_pUserData = malloc(arg_size + 1);
				if (NULL != m_pUserData) {
					memset(m_pUserData, 0, arg_size + 1);
					memcpy(m_pUserData, arg, arg_size);
					m_nUserDataSize = arg_size;
				}
			}

			m_file = fopen(filename, "rb");
			if (NULL != m_file) {
				nRet = m_stream_thd.Run(on_readfile, this);
				if (0 != nRet)
					LOG("ERROR","Thread running failure %d\n\n", nRet);
			}
			else {
				nRet = -1;
				LOG("ERROR","Open file failed '%s'\n\n", filename);
			}
		}
		else {
			delete udp;
			LOG("ERROR","UDP client '%s:%d' open failed %d\n\n", addr->ip, addr->port, nRet);
		}
		return nRet;
	}
	else LOG("ERROR","filename=%p, addr=%p\n\n", filename, addr);
	return -1;
}

#ifdef USE_UDP_CLIENT
int CAVStream::open(const char *ip, int port, STREAM_CBK cbk/*=NULL*/, STREAM_CBK cbk2/*=NULL*/, void *arg/*=NULL*/, int arg_size/*=0*/)
{
	int nRet = -1;
	CUdpClient *udp = new CUdpClient();
	nRet = udp->Open(ip, port);
	if (0 != nRet) {
		LOG("ERROR","UDP client '%s:%d' open failed %d\n\n", ip, port, nRet);
		delete udp;
		return nRet;
	}
	LOG("INFO", "UDP client '%s:%d' open succeed %d\n\n", ip, port, nRet);
	if ((NULL != m_pUserData) && (m_nUserDataSize > 0)) {
		free(m_pUserData);
		m_pUserData = NULL;
		m_nUserDataSize = 0;
	}
	m_pCallback = cbk;
	m_pCallback2 = cbk2;
	m_pUserData = arg;
	if ((NULL != arg) && (arg_size > 0)) {
		m_pUserData = malloc(arg_size + 1);
		if (NULL != m_pUserData) {
			memset(m_pUserData, 0, arg_size + 1);
			memcpy(m_pUserData, arg, arg_size);
			m_nUserDataSize = arg_size;
		}
	}
	int buff_size = 1024 * 1024 * 50;
	udp->SetSockOption(SO_RCVBUF, &buff_size, sizeof(buff_size));
	m_udp = udp;
	unsigned char* buff_in = (unsigned char*)av_malloc(BUFF_SIZE);
	m_in_fmt_ctx->pb = avio_alloc_context(buff_in, BUFF_SIZE, 0, this, read_buffer, NULL, NULL);
	m_in_fmt_ctx->flags = AVFMT_FLAG_CUSTOM_IO;
	//AVProbeData probeData;
	//probeData.buf = (unsigned char*)pData;
	//probeData.buf_size = nLen;
	//probeData.filename = "";
	//AVInputFormat* ifmt = av_probe_input_format(&probeData, 1);
	AVInputFormat *ifmt = NULL; // av_find_input_format("mpeg");//mpegtsraw dshow
	AVDictionary *opts = NULL;
	//av_dict_set(&opts, "timeout", "90000", 0);
	nRet = avformat_open_input(&m_in_fmt_ctx, "", ifmt, &opts);
	LOG("INFO","avformat_open_input = %d \n\n", nRet);
	if (nRet >= 0) {
		nRet = avformat_find_stream_info(m_in_fmt_ctx, NULL);
		LOG("INFO","avformat_find_stream_info = %d\n\n", nRet);
		av_dump_format(m_in_fmt_ctx, 0, NULL, 0);
		if (nRet >= 0) {
			for (int i = 0; i < m_in_fmt_ctx->nb_streams; i++) {
				//AVStream *in_stream = m_in_fmt_ctx->streams[i];
				AVMediaType av_type = m_in_fmt_ctx->streams[i]->codecpar->codec_type;
				if (av_type == AVMEDIA_TYPE_VIDEO)
					m_in_video_index = i;	// Video index
				#ifdef USE_AUDIO_STREAM
				else if (av_type == AVMEDIA_TYPE_AUDIO)
					m_in_audio_index = i;	// Audio index
				#endif
			}
		}
		else {
			print_ffmpeg_error(nRet);
			LOG("ERROR","avformat_find_stream_info failed %d\n\n", nRet);
			release();
			return nRet;
		}
	}
	else {
		print_ffmpeg_error(nRet);
		LOG("ERROR","avformat_open_input failed %d\n\n", nRet);
		release();
		return nRet;
	}
	//////////////// Output ////////////////////////
	//if ((nRet >= 0) && (NULL != wt_cbk))
	if (nRet >= 0)
	{
		if (NULL != m_cache_buff.data) {
			memset(&m_cache_buff, 0, sizeof(cache_buff_t));
		}
		m_cache_buff.data = (uint8_t*)av_malloc(CACHE_MAX_SIZE + 1);
		if (NULL != m_cache_buff.data) {
			m_cache_buff.pos = m_cache_buff.data;
			m_cache_buff.size = CACHE_MAX_SIZE;
		}
		else LOG("ERROR", "av_malloc function failed %p\n", m_cache_buff.data);
		nRet = avformat_alloc_output_context2(&m_out_fmt_ctx, NULL, "mp4", NULL);
		assert((nRet >= 0) && (NULL != m_out_fmt_ctx));
		unsigned char* buff_out = (uint8_t*)av_malloc(BUFF_SIZE);
		m_out_fmt_ctx->pb = avio_alloc_context(buff_out, BUFF_SIZE, 1, this, NULL, write_buffer, NULL);
		m_out_fmt_ctx->flags = AVFMT_FLAG_CUSTOM_IO;
		#if 0
		// Create streams in output
		for (int i = 0; (nRet >= 0) && (i < m_in_fmt_ctx->nb_streams); i++) {
			#ifdef USE_AUDIO_STREAM
			if ((m_in_fmt_ctx->streams[i]->codecpar->codec_type != AVMEDIA_TYPE_VIDEO) &&
				(m_in_fmt_ctx->streams[i]->codecpar->codec_type != AVMEDIA_TYPE_AUDIO))
			#else
			if (m_in_fmt_ctx->streams[i]->codecpar->codec_type != AVMEDIA_TYPE_VIDEO)
			#endif
			{
				continue;
			}
			AVCodec* codec = avcodec_find_decoder(m_in_fmt_ctx->streams[i]->codecpar->codec_id);
			AVStream *out_stream = avformat_new_stream(m_out_fmt_ctx, codec);
			//AVMediaType out_media_type = out_stream->codecpar->codec_type;
			//AVMediaType out_media_type = m_in_fmt_ctx->streams[i]->codecpar->codec_type;
			if (m_in_fmt_ctx->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO)
				m_out_video_index = out_stream->index;	// Video index
			#ifdef USE_AUDIO_STREAM
			else if (m_in_fmt_ctx->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_AUDIO)
				m_out_audio_index = out_stream->index;	// Audio index
			#endif
			else {
				LOG("ERROR","out_media_type=%d\n\n", m_in_fmt_ctx->streams[i]->codecpar->codec_type);
				continue;
			}
			//Copy the settings of AVCodecContext
			AVCodecContext* pCodecCtx = avcodec_alloc_context3(codec);
			if (NULL != pCodecCtx) {
				int ret = avcodec_parameters_to_context(pCodecCtx, m_in_fmt_ctx->streams[i]->codecpar);
				if (ret < 0)
					LOG("ERROR","avcodec_parameters_to_context failed %d\n\n", ret);
				pCodecCtx->codec_tag = 0;

				if (m_out_fmt_ctx->oformat->flags & AVFMT_GLOBALHEADER)
					pCodecCtx->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;

				ret = avcodec_parameters_from_context(out_stream->codecpar, pCodecCtx);
				if (ret < 0)
					LOG("ERROR","avcodec_parameters_from_context failed %d\n\n", ret);
				avcodec_free_context(&pCodecCtx);
			}
		}
		#else
		/////////// Video
		if (m_in_video_index >= 0) {
			AVStream *in_stream = m_in_fmt_ctx->streams[m_in_video_index];
			const AVCodec* codec = avcodec_find_decoder(in_stream->codecpar->codec_id);
			m_pOutVideoStream = avformat_new_stream(m_out_fmt_ctx, codec);
			assert(NULL != m_pOutVideoStream);
			//Copy the settings of AVCodecContext
			AVCodecContext* pCodecCtx = avcodec_alloc_context3(codec);
			assert(NULL != pCodecCtx);
			int ret = avcodec_parameters_to_context(pCodecCtx, in_stream->codecpar);
			assert(ret >= 0);
			pCodecCtx->codec_tag = 0;
			if (m_out_fmt_ctx->oformat->flags & AVFMT_GLOBALHEADER)
				pCodecCtx->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;
			ret = avcodec_parameters_from_context(m_pOutVideoStream->codecpar, pCodecCtx);
			assert(ret >= 0);
			avcodec_free_context(&pCodecCtx);
		}

		///////////// Audio
		if (m_in_audio_index >= 0) {
			const AVCodec* pInAudioCodec = avcodec_find_decoder(m_in_fmt_ctx->streams[m_in_audio_index]->codecpar->codec_id);
			assert(NULL != pInAudioCodec);
			m_pInAudioCodectx = avcodec_alloc_context3(pInAudioCodec);
			assert(NULL != m_pInAudioCodectx);
			int ret = avcodec_parameters_to_context(m_pInAudioCodectx, m_in_fmt_ctx->streams[m_in_audio_index]->codecpar);
			if (m_pInAudioCodectx->channels <= 0) {
				m_pInAudioCodectx->channels = 2;
				m_pInAudioCodectx->channel_layout = av_get_default_channel_layout(2);
				m_pInAudioCodectx->sample_rate = 16000;
			}
			ret = avcodec_open2(m_pInAudioCodectx, pInAudioCodec, NULL);
			assert(ret >= 0);
			const AVCodec*  pOutAudioCodec = avcodec_find_encoder(AV_CODEC_ID_AAC);
			m_pOutAudioCodectx = avcodec_alloc_context3(pOutAudioCodec);
			assert(NULL != m_pOutAudioCodectx);
			m_pOutAudioCodectx->codec = pOutAudioCodec;
			m_pOutAudioCodectx->sample_rate = m_pInAudioCodectx->sample_rate;
			m_pOutAudioCodectx->channel_layout = m_pInAudioCodectx->channel_layout;
			m_pOutAudioCodectx->channels = m_pInAudioCodectx->channels;
			m_pOutAudioCodectx->sample_fmt = AV_SAMPLE_FMT_FLTP;
			m_pOutAudioCodectx->codec_tag = 0;
			m_pOutAudioCodectx->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;
			m_pOutAudioCodectx->bit_rate = m_pInAudioCodectx->bit_rate;
			m_pOutAudioCodectx->codec_type = AVMEDIA_TYPE_AUDIO;
			ret = avcodec_open2(m_pOutAudioCodectx, pOutAudioCodec, NULL);
			assert(ret >= 0);
			m_pOutAudioStream = avformat_new_stream(m_out_fmt_ctx, pOutAudioCodec);
			assert(NULL != m_pOutAudioStream);
			avcodec_parameters_from_context(m_pOutAudioStream->codecpar, m_pOutAudioCodectx);
		}
		#endif
		av_dump_format(m_out_fmt_ctx, 0, NULL, 1);
		/*if (!(m_out_fmt_ctx->flags & AVFMT_NOFILE)) {
			if (avio_open(&m_out_fmt_ctx->pb, "C:/123.mp4", AVIO_FLAG_WRITE) < 0) {
				printf("Could not open output file.\n\n");
				return -1;
			}
			//avio_close(m_out_fmt_ctx->pb);
		}*/
		AVDictionary *opts = NULL;
		av_dict_set(&opts, "movflags", "frag_keyframe+empty_moov+omit_tfhd_offset+faststart+separate_moof+disable_chpl+default_base_moof+dash", 0);
		m_head_flag = true;
		nRet = avformat_write_header(m_out_fmt_ctx, &opts);// Write file header
		LOG("INFO","avformat_write_header = %d\n\n", nRet);
		if (NULL != m_pCallback2) {
			#ifndef USE_COMPILE_EXE
			char data[32] = { 0 };
			if (m_in_audio_index >= 0) {// 复合流
				strcpy(data, "AAAAAAAAAAAAAAAAAAA");// 长度 19
				m_pCallback2(data, strlen(data), m_pUserData);
			}
			else {			// 音频流
				strcpy(data, "BBBBBBBBBBBBBBBBBB");// 长度 18
				m_pCallback2(data, strlen(data), m_pUserData);
			}
			#endif
			if ((NULL != m_head_data) && (m_head_len > 0)) {// 输出拼接后的头部信息
				LOG("INFO","-------- head_data=%p, head_len=%d --------\n\n", m_head_data, m_head_len);
				m_pCallback2(m_head_data, m_head_len, m_pUserData);
				free(m_head_data);
				m_head_data = NULL;
				m_head_len = 0;
			}
		}
		m_head_flag = false;
		av_dict_free(&opts);
		if (nRet < 0) {
			LOG("ERROR","write_header failed %d ------\n\n", nRet);
			print_ffmpeg_error(nRet);
		}
	}
	if (nRet >= 0) {
		nRet = m_stream_thd.Run(on_stream, this);
		if (0 != nRet)
			LOG("ERROR","Thread running failure %d\n\n", nRet);
	}
	return nRet;
}
int CAVStream::write_buffer(void *arg, uint8_t *buf, int buf_size)
{
	//DEBUG("INFO: ------ rwrite_buffer %d -------\n\n", buf_size);
	CAVStream *stream = (CAVStream*)arg;
	if (stream->m_head_flag) {
		if (NULL == stream->m_head_data) {
			//LOG("INFO", "---- A1 ----\n");
			stream->m_head_data = (uint8_t*)malloc(buf_size+1);
			memcpy(stream->m_head_data, buf, buf_size);
			stream->m_head_len = buf_size;
			//LOG("INFO", "---- A2 ----\n");
		}
		else {
			//LOG("INFO", "---- A3 ----\n");
			stream->m_head_data = (uint8_t*)realloc(stream->m_head_data, stream->m_head_len+buf_size+1);
			memcpy(stream->m_head_data + stream->m_head_len, buf, buf_size);
			stream->m_head_len += buf_size;
			//LOG("INFO", "---- A4 ----\n");
		}
	}
	else if(NULL != stream->m_pCallback2){
		#if 0
			stream->m_pCallback2(buf, buf_size, stream->m_pUserData2);
		#else
		if (NULL != stream->m_cache_buff.pos) {
			if ((stream->m_cache_buff.length + buf_size) <= stream->m_cache_buff.size) {
				memcpy(stream->m_cache_buff.pos, buf, buf_size);
				stream->m_cache_buff.pos += buf_size;
				stream->m_cache_buff.length += buf_size;
			}
			else LOG("ERROR","Write cache overflows\n\n");
		}
		#endif
	}
	return buf_size;
}
int CAVStream::read_buffer(void *arg, uint8_t *buff, int buf_size)
{
	CAVStream *stream = (CAVStream*)arg;
	if ((NULL == stream) || (NULL == stream->m_udp))
		return 0;
	static int rtp_head_size = sizeof(rtp_header_t);

	CUdpClient *udp = (CUdpClient*)stream->m_udp;
	char buf[BUFF_SIZE] = { 0 };
	int nRet = udp->Recv(buf, BUFF_SIZE);
	if (nRet >= rtp_head_size) {
		nRet -= rtp_head_size;
		memcpy(buff, buf + rtp_head_size, nRet);
		
		#if 0
		rtp_header_t *hd = (rtp_header_t*)buf;
		printf("v:%d p:%d x:%d cc:%d m:%d pt:%d seq:%u time:%ld ssrc:%ld size:%d\n", hd->version, hd->padding,\
			hd->extension, hd->csrc_len, hd->marker, hd->payload, \
			ntohs(hd->seq_no), ntohl(hd->timestamp), ntohl(hd->ssrc), nRet);
		#endif
		if (stream->m_mod_head) {
			stream->m_mod_head = false;
			char *data = (char*)buff;
			data += 60;
			LOG("INFO","============= 0x%X 0x%X =============\n\n", *data, *(data + 1));
			*data = 0x0F;
			LOG("INFO","============= 0x%X 0x%X =============\n\n", *data, *(data + 1));
		}
		if (NULL != stream->m_pCallback) {
			int ret = stream->m_pCallback(buff, nRet, stream->m_pUserData);
			if (ret < 0)
				stream->m_stream_thd.Stop(false);
		}
		/*
		//RFC3551
		char payload_str[10] = { 0 };
		char payload = hd->payload;
		switch (payload)
		{
		case 0:
		case 1:
		case 2:
		case 3:
		case 4:
		case 5:
		case 6:
		case 7:
		case 8:
		case 9:
		case 10:
		case 11:
		case 12:
		case 13:
		case 14:
		case 15:
		case 16:
		case 17:
		case 18: sprintf(payload_str, "Audio"); break;
		case 31: sprintf(payload_str, "H.261"); break;
		case 32: sprintf(payload_str, "MPV"); break;
		case 33: sprintf(payload_str, "MP2T"); break;
		case 34: sprintf(payload_str, "H.263"); break;
		case 96: sprintf(payload_str, "H.264"); break;
		default: sprintf(payload_str, "other"); break;
		}
		//Parse MPEGTS
		char *rtp_data = buf + rtp_head_size;	// RTP ����
		int rtp_data_size = nRet;				// RTP���ݴ�С
		if (payload == 33) {
			MPEGTS_FIXED_HEADER mpegts_header;
			for (int i = 0; i < rtp_data_size; i = i + 188) {
				if (rtp_data[i] != 0x47)
					break;
				//MPEGTS Header
				//memcpy((void*)&mpegts_header, rtp_data+i, sizeof(MPEGTS_FIXED_HEADER));
				fprintf(stdout, "   [MPEGTS Pkt]\n");
			}
		}
		*/
	}
	else {
		LOG("WARN", "UDP client no data was received %d [rtp_head_size:%d]\n\n", nRet, rtp_head_size);
	}
	return (nRet > 0) ? nRet : 0;
}
#endif

void CAVStream::close(bool is_sync/*=true*/)
{
	//DEBUG("INFO: Close stream.\n\n");
	m_stream_thd.Stop(is_sync);
	// Input
	if (NULL != m_in_fmt_ctx) {
		#if 1
		if ((NULL != m_in_fmt_ctx->pb) && (m_in_fmt_ctx->flags & AVFMT_FLAG_CUSTOM_IO)) {
			if (NULL != m_in_fmt_ctx->pb->buffer) {
				av_free(m_in_fmt_ctx->pb->buffer);
				m_in_fmt_ctx->pb->buffer = NULL;
			}
			avio_context_free(&m_in_fmt_ctx->pb);
			m_in_fmt_ctx->pb = NULL;
		}
		#endif
		avformat_close_input(&m_in_fmt_ctx);
	}
	// Output
	if (NULL != m_out_fmt_ctx) {
		#if 1
		if ((NULL != m_out_fmt_ctx->pb) && (m_out_fmt_ctx->flags & AVFMT_FLAG_CUSTOM_IO)) {
			if (NULL != m_out_fmt_ctx->pb->buffer) {
				av_free(m_out_fmt_ctx->pb->buffer);
				m_out_fmt_ctx->pb->buffer = NULL;
			}
			avio_context_free(&m_out_fmt_ctx->pb);
			m_out_fmt_ctx->pb = NULL;
		}
		#endif
		avformat_free_context(m_out_fmt_ctx);
		m_out_fmt_ctx = NULL;
	}
	#ifdef USE_UDP_CLIENT
	if (NULL != m_udp) {
		CUdpClient *udp = (CUdpClient*)m_udp;
		udp->Close(is_sync);
		delete udp;
		m_udp = NULL;
	}
	#endif

	m_pCallback = NULL;
	m_pCallback2 = NULL;
	if ((NULL != m_pUserData) && (m_nUserDataSize > 0)) {
		free(m_pUserData);
		m_pUserData = NULL;
		m_nUserDataSize = 0;
	}

	if (NULL != m_pInAudioCodectx) {
		avcodec_close(m_pInAudioCodectx);
		avcodec_free_context(&m_pInAudioCodectx);
		m_pInAudioCodectx = NULL;
	}
	if (NULL != m_pOutAudioCodectx) {
		avcodec_close(m_pOutAudioCodectx);
		avcodec_free_context(&m_pOutAudioCodectx);
		m_pOutAudioCodectx = NULL;
	}
	if (NULL != m_file) {
		fclose(m_file);
		m_file = NULL;
	}
}



#if 0
void STDCALL CAVStream::on_stream(STATE &state, void *pUserData)
{
	LOG("INFO","---------- media stream thread is running ... ----------\n\n");
	CAVStream *stream = (CAVStream*)pUserData;

	bool first_video = false, first_audio = false;
	int nRet = 0, frame_index = 0, read_frame_fail = 0, write_frame_fail = 0;

	#ifdef USE_AUDIO_STREAM
		#if 0
			AVBitStreamFilterContext* aac_flt_ctx = av_bitstream_filter_init("aac_adtstoasc");
		#else
			AVBSFContext *av_bsf_ctx = NULL;
			const AVBitStreamFilter *aacfilter = av_bsf_get_by_name("aac_adtstoasc");
			if (NULL != aacfilter) {
				int ret = av_bsf_alloc(aacfilter, &av_bsf_ctx);
				if ((ret < 0) || (NULL == av_bsf_ctx)) {
					LOG("ERROR","av_bsf_alloc failed.\n\n");
					return;
				}
			}
			else {
				LOG("ERROR","Unkonw bitstream filter.\n\n");
				av_log(NULL, AV_LOG_ERROR, "Unkonw bitstream filter");
			}
		#endif
	#endif
	/*
	if (NULL != stream->m_out_fmt_ctx) {
		AVDictionary *opts = NULL;	// дMP4ͷ
		av_dict_set(&opts, "movflags", "frag_keyframe+empty_moov+omit_tfhd_offset+faststart+separate_moof+disable_chpl+default_base_moof+dash", 0);
		nRet = avformat_write_header(stream->m_out_fmt_ctx, &opts);// Write file header
		DEBUG("INFO: avformat_write_header = %d\n\n", nRet);
		av_dict_free(&opts);
		if (nRet < 0) {
			DEBUG("ERROR: write_header failed %d ------\n\n", nRet);
			stream->print_ffmpeg_error(nRet);
			DEBUG("---------- media stream thread has exited ----------\n\n");
			return;
		}
	}
	*/
	AVPacket pkt;
	while (TS_RUNING == state)
	{
		nRet = av_read_frame(stream->m_in_fmt_ctx, &pkt); // Read frame
		//DEBUG("INFO: av_read_frame=%d\n", nRet);
		if (nRet >= 0)
		{
			read_frame_fail = 0;
			//AVStream *in_stream = stream->m_in_fmt_ctx->streams[pkt.stream_index];
			AVStream *in_stream = stream->m_in_fmt_ctx->streams[pkt.stream_index];

			#ifndef USE_AUDIO_STREAM
			// ������Ƶ
			if (in_stream->codec->codec_type != AVMEDIA_TYPE_VIDEO) {
				av_packet_unref(&pkt);
				continue;
			}
			#endif
			// ���˷���Ƶ����Ƶ��
			if ((pkt.stream_index != stream->m_in_video_index) && (pkt.stream_index != stream->m_in_audio_index)) {
				av_packet_unref(&pkt);
				LOG("INFO","------ stream_index=%d ------\n\n", pkt.stream_index);
				continue;
			}
			// ��Ƶ
			if ((false == first_video) && (pkt.stream_index == stream->m_in_video_index)) {
				first_video = true;
				pkt.dts = 0;
				pkt.pts = 0;
			}
			#ifdef USE_AUDIO_STREAM
			// ��Ƶ
			if ((false == first_audio) && (pkt.stream_index == stream->m_in_audio_index)) {
				first_audio = true;
				pkt.dts = 0;
				pkt.pts = 0;
			}
			#endif
			if (pkt.stream_index == stream->m_in_video_index)
			{
				//if (AV_NOPTS_VALUE == pkt.pts)
				{
					// ������Ƶ֡��DTS��PTS
					//Write PTS
					AVRational time_base1 = in_stream->time_base;
					//Duration between 2 frames (us)
					int64_t calc_duration = (double)AV_TIME_BASE / av_q2d(in_stream->r_frame_rate);
					//Parameters
					pkt.pts = (double)(frame_index*calc_duration) / (double)(av_q2d(time_base1)*AV_TIME_BASE);
					pkt.dts = pkt.pts;
					pkt.duration = (double)calc_duration / (double)(av_q2d(time_base1)*AV_TIME_BASE);
					frame_index++;
					//printf(" video dts=%lld,pts=%lld,duration=%lld\n", pkt.dts, pkt.pts, pkt.duration);
				}
			}
			#ifdef USE_AUDIO_STREAM
			else if (pkt.stream_index == stream->m_in_audio_index) {
				if (NULL != stream->m_out_fmt_ctx) {
					AVStream *out_stream = stream->m_out_fmt_ctx->streams[pkt.stream_index];
					//AVStream *out_stream = pClient->m_out_fmt_ctx->streams[videoindex_out];
					//Convert PTS/DTS
					pkt.pts = av_rescale_q_rnd(pkt.pts, in_stream->time_base, out_stream->time_base, (AVRounding)(AV_ROUND_NEAR_INF | AV_ROUND_PASS_MINMAX));
					pkt.dts = av_rescale_q_rnd(pkt.dts, in_stream->time_base, out_stream->time_base, (AVRounding)(AV_ROUND_NEAR_INF | AV_ROUND_PASS_MINMAX));
					pkt.duration = av_rescale_q(pkt.duration, in_stream->time_base, out_stream->time_base);
					//pkt.pos = -1;
					//pkt.stream_index = videoindex_out;
					#if 1
						nRet = av_bsf_send_packet(av_bsf_ctx, &pkt);		//��pkt�������͵�filter��ȥ
						if (nRet >= 0) {
							nRet = av_bsf_receive_packet(av_bsf_ctx, &pkt);	//��ȡ����������ݣ���ͬһ��pkt
							if (nRet < 0)
								LOG("ERROR"," av_bsf_receive_packet failed.\n\n");
						}
						else LOG("ERROR","av_bsf_send_packet failed.\n\n");
					#else
						av_bitstream_filter_filter(aac_flt_ctx, out_stream->codec, NULL, &pkt.data, &pkt.size, pkt.data, pkt.size, 0);
					#endif
					//printf(" audio dts=%lld,pts=%lld,duration=%lld\n", pkt.dts, pkt.pts, pkt.duration);
				}
			}
			else LOG("WARN","pkt.stream_index=%d\n\n", pkt.stream_index);
			#endif

			if ((NULL != stream->m_pCallback) && (NULL == stream->m_udp)) {
				stream->m_pCallback(pkt.data, pkt.size, stream->m_pUserData);
			}

			if (NULL != stream->m_out_fmt_ctx) {
				nRet = av_interleaved_write_frame(stream->m_out_fmt_ctx, &pkt);// Write frame
				if (nRet >= 0) {
					write_frame_fail = 0;
				}
				else {
					write_frame_fail++;
					if (write_frame_fail > MAX_WRITE_FRAME_FAIL) {
						LOG("ERROR","write_frame_fail = %d\n\n", write_frame_fail);
						stream->print_ffmpeg_error(nRet);
						av_packet_unref(&pkt);
						break;
					}
				}
			}
		}
		else {
			stream->print_ffmpeg_error(nRet);
			read_frame_fail++;
			if (read_frame_fail > MAX_READ_FRAME_FAIL) {
				LOG("ERROR","read_frame_fail = %d\n\n", read_frame_fail);
				stream->print_ffmpeg_error(nRet);
				av_packet_unref(&pkt);
				break;
			}
		}
		av_packet_unref(&pkt);
	} // while (TS_RUNING == state)

	if (NULL != stream->m_out_fmt_ctx) {
		// дMP4β
		av_write_trailer(stream->m_out_fmt_ctx);
		if(NULL != stream->m_pCallback2)
			stream->m_pCallback2(NULL, 0, stream->m_pUserData);
	}
	LOG("INFO","---------- media stream thread has exited ----------\n\n");
}
#else
void STDCALL CAVStream::on_stream(STATE &state, PAUSE &p, void *pUserData)
{
	LOG("INFO","---------- media stream thread is running ... ----------\n\n");
	CAVStream *stream = (CAVStream*)pUserData;
	bool first_video = false, first_audio = false;
	int nRet = 0, frame_index = 0, read_frame_fail = 0, write_frame_fail = 0;
	int64_t frame_index_v = 0;
	int64_t frame_index_a = 0;
	int64_t last_audio_write = 0;
	queue<AVPacket*> video_queue;
	AVFrame* pFrame = av_frame_alloc();
	AVPacket in_pkt;
	AVPacket out_pkt;
	size_t n = 0;
	while (TS_STOP != state)
	{
		//if (TS_PAUSE == state) {
		//	p.wait();
		//}
		av_init_packet(&in_pkt);
		av_init_packet(&out_pkt);
		nRet = av_read_frame(stream->m_in_fmt_ctx, &in_pkt);
		if (nRet < 0) {
			stream->print_ffmpeg_error(nRet);
			read_frame_fail++;
			if (read_frame_fail > MAX_READ_FRAME_FAIL) {
				LOG("ERROR","av_read_frame failed count %d,break\n\n", read_frame_fail);
				break;
			}
			av_usleep(200000);
			continue;
		}
		if (stream->m_is_pause) { // 轮播
			if(stream->m_cache_buff.length > 0) { // 清空缓存
				stream->m_cache_buff.pos = stream->m_cache_buff.data;
				stream->m_cache_buff.length = 0;
				if (false == video_queue.empty()) {
					LOG("INFO","Clear video queue\n");
					AVPacket* first = video_queue.front();
					if (NULL != first)
						av_packet_unref(first);
					video_queue.pop();
				}
			}
			continue;
		}
		//if (!(stream->m_in_fmt_ctx->flags & AVFMT_FLAG_CUSTOM_IO)) { // playback
		//	printf("playback\n");
		//	av_usleep(30000);
		//}
		//if(in_pkt.stream_index!=video_index)
		if ((in_pkt.stream_index != stream->m_in_video_index) && (in_pkt.stream_index != stream->m_in_audio_index)) {
			LOG("ERROR","stream_index:%d\n", in_pkt.stream_index);
			av_packet_unref(&in_pkt);
			continue;
		}
		if (in_pkt.stream_index == stream->m_in_video_index) {
			//if(in_pkt.pts==AV_NOPTS_VALUE||in_pkt.pts==0) 
			{
				//Write PTS
				AVRational time_base = stream->m_pOutVideoStream->time_base;
				//Duration between 2 frames (us)
				int64_t calc_duration = (double)AV_TIME_BASE / av_q2d(stream->m_in_fmt_ctx->streams[stream->m_in_video_index]->r_frame_rate);
				//Parameters
				in_pkt.pts = (double)(frame_index_v*calc_duration) / (double)(av_q2d(time_base)*AV_TIME_BASE);
				in_pkt.dts = in_pkt.pts;
				in_pkt.duration = (double)calc_duration / (double)(av_q2d(time_base)*AV_TIME_BASE);
				frame_index_v++;
			}
			AVPacket* temp;
			//printf("push %ld\n",in_pkt.pts);
			temp = av_packet_clone(&in_pkt);
			video_queue.push(temp);
			av_packet_unref(&in_pkt);
			continue;
		}
		nRet = avcodec_send_packet(stream->m_pInAudioCodectx, &in_pkt);
		if (nRet < 0) {
			av_packet_unref(&in_pkt);
			stream->print_ffmpeg_error(nRet);
			LOG("ERROR","avcodec_send_packet failed %d, break\n\n", nRet);
			//break;
		}
		int getframe_return = 0;
		while (getframe_return >= 0)
		{
			getframe_return = avcodec_receive_frame(stream->m_pInAudioCodectx, pFrame);
			if ((getframe_return == AVERROR(EAGAIN)) || (nRet == AVERROR_EOF)) {
				//LOG("ERROR", "avcodec_receive_frame failed %d\n", nRet);
				break;
			}
			else if (getframe_return < 0) {
				LOG("WARN","get frame fail\n");
				break;
			}
			else {
				nRet = avcodec_send_frame(stream->m_pOutAudioCodectx, pFrame);
				if (nRet < 0) {
					LOG("ERROR","avcodec_send_frame failed %d\n", nRet);
					break;
				}
				int getpacket_return = 0;
				while (getpacket_return >= 0) {
					getpacket_return = avcodec_receive_packet(stream->m_pOutAudioCodectx, &out_pkt);
					if (AVERROR(EAGAIN) == getpacket_return || AVERROR_EOF == getpacket_return) {
						// printf("wai out packet\n");
						av_packet_unref(&out_pkt);
						break;
					}
					else if (getpacket_return < 0) {
						LOG("WARN","revice packe fail\n");
						break;
					}
					if (1) {
						//Write PTS
						AVRational time_base = stream->m_pOutAudioStream->time_base;
						//Duration between 2 frames (us)
						int64_t calc_duration = (double)AV_TIME_BASE / stream->m_pOutAudioCodectx->sample_rate;
						//Parameters
						out_pkt.pts = (double)(frame_index_a*calc_duration) / (double)(av_q2d(time_base)*AV_TIME_BASE);
						out_pkt.dts = out_pkt.pts;
						out_pkt.duration = (double)calc_duration*pFrame->nb_samples / (double)(av_q2d(time_base)*AV_TIME_BASE);
						frame_index_a += pFrame->nb_samples;
					}
					last_audio_write = out_pkt.pts;
					while (1) {
						if (video_queue.empty()) {
							//DEBUG("INFO: video_queue is empty\n\n");
							break;
						}
						AVPacket* first = video_queue.front();
						if (first == NULL) {
							LOG("ERROR","video_queue.front()==null\n\n");
							break;
						}
						//if(first->pts<=last_audio_write)
						if (av_compare_ts(first->pts, stream->m_pOutVideoStream->time_base, last_audio_write, stream->m_pOutAudioStream->time_base) <= 0)
						{
							if (first->pts == last_audio_write) {
								first->pts -= 1;
								first->dts -= 1;
							}
							//  printf("video pts %ld\n",first->pts);
							if (av_interleaved_write_frame(stream->m_out_fmt_ctx, first) < 0) {
								LOG("INFO","write video frame fail\n");
								break;
							}
							video_queue.pop();
							av_packet_unref(first);
						}
						else {
							//DEBUG("av_compare_ts() > 0\n\n");
							break;
						}
					}
					//printf("audio pts %ld\n",out_pkt.pts);
					out_pkt.flags |= AV_PKT_FLAG_KEY;
					out_pkt.stream_index = stream->m_in_audio_index;
					nRet = av_interleaved_write_frame(stream->m_out_fmt_ctx, &out_pkt);
					if (nRet < 0) {
						LOG("ERROR","av_interleaved_write_frame failed %d, break\n", nRet);
						break;
					}
					if ((NULL != stream->m_pCallback2) && (stream->m_cache_buff.length > 0)) {
						stream->m_pCallback2(stream->m_cache_buff.data, stream->m_cache_buff.length, stream->m_pUserData);
						stream->m_cache_buff.pos = stream->m_cache_buff.data;
						stream->m_cache_buff.length = 0;
					}
					//else LOG("ERROR", "cbk=%p, len=%d\n", stream->m_pCallback2, stream->m_cache_buff.length);
					av_packet_unref(&out_pkt);
				}
			}
		}
	}
	if (NULL != stream->m_pCallback)
		stream->m_pCallback(NULL, 0, stream->m_pUserData);
	if (NULL != stream->m_out_fmt_ctx) {
		nRet = av_write_trailer(stream->m_out_fmt_ctx);
		LOG("INFO","av_write_trailer %d\n\n", nRet);
		if (NULL != stream->m_pCallback2)
			stream->m_pCallback2(NULL, 0, stream->m_pUserData);
	}
	av_frame_free(&pFrame);
	LOG("INFO","---------- media stream thread has exited [state:%d] ----------\n\n", state);
}

void STDCALL CAVStream::on_playback(STATE &state, PAUSE &p, void *pUserData)
{
	LOG("INFO","---------- media playback thread is running ... ----------\n\n");
	CAVStream *stream = (CAVStream*)pUserData;
	bool first_video = false, first_audio = false;
	int nRet = 0, frame_index = 0, read_frame_fail = 0, write_frame_fail = 0;
	int64_t frame_index_v = 0;
	int64_t frame_index_a = 0;
	int64_t last_audio_write = 0;
	queue<AVPacket*> video_queue;
	AVFrame* pFrame = av_frame_alloc();
	AVPacket in_pkt;
	AVPacket out_pkt;
	int sp = 1000000 / stream->m_in_fmt_ctx->streams[stream->m_in_video_index]->r_frame_rate.num - 10000;
	while (TS_STOP != state)
	{
		if (TS_PAUSE == state) {
			p.wait();
		}
		av_init_packet(&in_pkt);
		av_init_packet(&out_pkt);
		nRet = av_read_frame(stream->m_in_fmt_ctx, &in_pkt);
		//printf(" data:%p size:%d\n", in_pkt.data, in_pkt.size);
		if (nRet < 0) {
			stream->print_ffmpeg_error(nRet);
			read_frame_fail++;
			if (read_frame_fail > MAX_READ_FRAME_FAIL) {
				LOG("ERROR","av_read_frame failed count %d,break\n\n", read_frame_fail);
				break;
			}
			av_usleep(200000);
			continue;
		}

		if (!(stream->m_in_fmt_ctx->flags & AVFMT_FLAG_CUSTOM_IO)) { // playback
			//printf("playback\n");
			if (NULL != stream->m_pCallback) {
				stream->m_pCallback(in_pkt.data, in_pkt.size, stream->m_pUserData);
			}
			av_usleep(sp);
		}
		if (NULL == stream->m_out_fmt_ctx)
			continue;

		//if(in_pkt.stream_index!=video_index)
		if ((in_pkt.stream_index != stream->m_in_video_index) && (in_pkt.stream_index != stream->m_in_audio_index)) {
			av_packet_unref(&in_pkt);
			continue;
		}
		if (in_pkt.stream_index == stream->m_in_video_index) {
			//if(in_pkt.pts==AV_NOPTS_VALUE||in_pkt.pts==0)
			{
				//Write PTS
				AVRational time_base = stream->m_pOutVideoStream->time_base;
				//Duration between 2 frames (us)
				int64_t calc_duration = (double)AV_TIME_BASE / av_q2d(stream->m_in_fmt_ctx->streams[stream->m_in_video_index]->r_frame_rate);
				//Parameters
				in_pkt.pts = (double)(frame_index_v*calc_duration) / (double)(av_q2d(time_base)*AV_TIME_BASE);
				in_pkt.dts = in_pkt.pts;
				in_pkt.duration = (double)calc_duration / (double)(av_q2d(time_base)*AV_TIME_BASE);
				frame_index_v++;
			}
			AVPacket* temp;
			//printf("push %ld\n",in_pkt.pts);
			temp = av_packet_clone(&in_pkt);
			video_queue.push(temp);
			av_packet_unref(&in_pkt);
			continue;
		}
		// FHEADER * phead=(FHEADER*)in_pkt.data;
	   //  printf("%d %d %d %d %d %d\n",phead->version,phead->layer,phead->bit_rate_index,phead->sample_rate_index,phead->channel_mode,phead->padding);
	   // in_pkt.data[1]|=3<<0;
		//in_pkt.data[2]|=2<<2;
		//printf("%d %d %d %d %d\n",phead->version,phead->layer,phead->bit_rate_index,phead->sample_rate_index,phead->channel_mode);
		// av_packet_unref(&in_pkt);
	   // continue;
		nRet = avcodec_send_packet(stream->m_pInAudioCodectx, &in_pkt);
		if (nRet < 0) {
			av_packet_unref(&in_pkt);
			//stream->print_ffmpeg_error(nRet);
			//DEBUG("ERROR: avcodec_send_packet failed %d, break\n\n", nRet);
			//break;
		}
		int getframe_return = 0;
		while (getframe_return >= 0)
		{
			getframe_return = avcodec_receive_frame(stream->m_pInAudioCodectx, pFrame);
			if ((getframe_return == AVERROR(EAGAIN)) || (nRet == AVERROR_EOF)) {
				break;
			}
			else if (getframe_return < 0) {
				LOG("WARN","get frame fail\n");
				break;
			}
			else {
				nRet = avcodec_send_frame(stream->m_pOutAudioCodectx, pFrame);
				if (nRet < 0) {
					LOG("WARN","send frame fail\n");
					break;
				}
				int getpacket_return = 0;
				while (getpacket_return >= 0) {
					getpacket_return = avcodec_receive_packet(stream->m_pOutAudioCodectx, &out_pkt);
					if (AVERROR(EAGAIN) == getpacket_return || AVERROR_EOF == getpacket_return) {
						// printf("wai out packet\n");
						av_packet_unref(&out_pkt);
						break;
					}
					else if (getpacket_return < 0) {
						LOG("WARN","revice packe fail\n");
						break;
					}
					if (1)
					{
						//Write PTS
						AVRational time_base = stream->m_pOutAudioStream->time_base;
						//Duration between 2 frames (us)
						int64_t calc_duration = (double)AV_TIME_BASE / stream->m_pOutAudioCodectx->sample_rate;
						//Parameters
						out_pkt.pts = (double)(frame_index_a*calc_duration) / (double)(av_q2d(time_base)*AV_TIME_BASE);
						out_pkt.dts = out_pkt.pts;
						out_pkt.duration = (double)calc_duration*pFrame->nb_samples / (double)(av_q2d(time_base)*AV_TIME_BASE);
						frame_index_a += pFrame->nb_samples;
					}
					last_audio_write = out_pkt.pts;
					while (1) {
						if (video_queue.empty()) {
							//DEBUG("INFO: video_queue is empty\n\n");
							break;
						}
						AVPacket* first = video_queue.front();
						if (first == NULL) {
							LOG("ERROR","video_queue.front()==null\n\n");
							break;
						}
						//if(first->pts<=last_audio_write)
						if (av_compare_ts(first->pts, stream->m_pOutVideoStream->time_base, last_audio_write, stream->m_pOutAudioStream->time_base) <= 0)
						{
							if (first->pts == last_audio_write) {
								first->pts -= 1;
								first->dts -= 1;
							}
							//  printf("video pts %ld\n",first->pts);
							if (av_interleaved_write_frame(stream->m_out_fmt_ctx, first) < 0) {
								LOG("INFO","write video frame fail\n");
								break;
							}
							video_queue.pop();
							av_packet_unref(first);
						}
						else {
							//DEBUG("av_compare_ts() > 0\n\n");
							break;
						}
					};
					//printf("audio pts %ld\n",out_pkt.pts);
					out_pkt.flags |= AV_PKT_FLAG_KEY;
					out_pkt.stream_index = stream->m_in_audio_index;
					nRet = av_interleaved_write_frame(stream->m_out_fmt_ctx, &out_pkt);
					if (nRet < 0) {
						LOG("ERROR","write audio frame fail, break\n");
						break;
					}
					if ((NULL != stream->m_pCallback2) && (stream->m_cache_buff.length > 0)) {
						stream->m_pCallback2(stream->m_cache_buff.data, stream->m_cache_buff.length, stream->m_pUserData);
						stream->m_cache_buff.pos = stream->m_cache_buff.data;
						stream->m_cache_buff.length = 0;
					}
					av_packet_unref(&out_pkt);
				}
			}
		}
	}
	if (NULL != stream->m_pCallback)
		stream->m_pCallback(NULL, 0, stream->m_pUserData);
	if (NULL != stream->m_out_fmt_ctx) {
		nRet = av_write_trailer(stream->m_out_fmt_ctx);
		LOG("INFO","av_write_trailer %d\n\n", nRet);
		if (NULL != stream->m_pCallback2)
			stream->m_pCallback2(NULL, 0, stream->m_pUserData);
	}
	av_frame_free(&pFrame);
	LOG("INFO","---------- media playback thread has exited [state:%d] ----------\n\n", state);
}

void STDCALL CAVStream::on_readfile(STATE &state, PAUSE &p, void *pUserData)
{
	LOG("INFO","---------- read file thread is running ... ----------\n\n");
	CAVStream *stream = (CAVStream*)pUserData;
	CUdpClient *client = (CUdpClient*)stream->m_udp;
	int size = 1024 * 1024 * 5;
	NALU_t *nal = AllocNALU(size); // 8000000
	if (NULL != nal)
	{
		char buff[2048] = { 0 };
		static int rtp_hd_size = sizeof(rtp_header_t);
		unsigned long timestamp = 0;
		int pocket_number = 0;   //包号
		//int frame_number = 0;     //帧号

		rtp_header_t *rtp_head = (rtp_header_t*)buff;
		rtp_head->payload = H264;		//Payload type
		rtp_head->version = 2;          //Payload version
		rtp_head->marker = 0;			//Marker sign
		rtp_head->ssrc = 999999999;	    //帧数	
		rtp_head->timestamp = htonl(timestamp);

		char* payload = buff + rtp_hd_size;	// RTP载荷
		int nRet = 0;
		while ((TS_STOP != state) && (!feof(stream->m_file))) {
			if (TS_PAUSE == state)
				p.wait();

			GetAnnexbNALU(nal, stream->m_file);
			//printf("nal: %d %d\n\n", nal->len, nal->max_size);
			if (nal->len < 1400) {  //打单包
				rtp_head->marker = (0xC0 == nal->buf[3]) ? 1 : 0;
				rtp_head->seq_no = htons(pocket_number++);
				memcpy(payload, nal->buf, nal->len);
				nRet = client->Send(buff, (nal->len + rtp_hd_size));
				if (nRet < 1)
					LOG("ERROR","Send data failed %d\n\n", nRet);
			}
			else if (nal->len >= 1400) { //打分片包
				int size = nal->len;
				unsigned char *data = nal->buf;

				rtp_head->marker = 0;
				while (size > 0) {
					rtp_head->seq_no = htons(pocket_number++);

					int len = min(size, 1400);
					memcpy(payload, data, len); // RTP载荷

					nRet = client->Send(buff, (rtp_hd_size + len));
					if (nRet > 0) {
						data += len;
						size -= len;
					}
					else LOG("ERROR","Send data failed %d\n\n", nRet);
				}
			}
			if (nal->buf[4] != 0xc0) {
				timestamp += 3600;
				rtp_head->timestamp = htonl(timestamp);
			}
			#if _WIN32
			Sleep(12);
			#else
			usleep(12000);
			#endif
		}
		FreeNALU(nal);
	}
	if (NULL != stream->m_pCallback) {
		stream->m_pCallback(NULL, 0, stream->m_pUserData);
	}
	LOG("INFO","---------- read file thread has exited [state:%d] ----------\n\n", state);
}

#endif


void CAVStream::join()
{
	m_stream_thd.Join();
}
void CAVStream::detach()
{
	m_stream_thd.Detach();
}
int CAVStream::seek(int pos, int flags)
{
	int nRet = -1;
	if((NULL != m_in_fmt_ctx) && (m_in_video_index >= 0) && (pos >= 0)) {
		
		AVStream *pStream = m_in_fmt_ctx->streams[m_in_video_index];
		// 流的时长(单位:秒)
		int64_t video_duration = pStream->duration * av_q2d(pStream->time_base);
		if (pos <= video_duration) {
			// 计算开始时间(单位:time_base)
			int64_t start_video_dts = pStream->start_time + (int64_t)(pos / av_q2d(pStream->time_base));
			if (NULL != m_cache_buff.pos) {
				m_cache_buff.pos = m_cache_buff.data;
				m_cache_buff.length = 0;
			}
			// AVSEEK_FLAG_BACKWARD,AVSEEK_FLAG_FRAME
			// AVSEEK_FLAG_BACKWARD: 转跳至第n秒处的第一个I帧(若没有则向前找第一个)
			nRet = av_seek_frame(m_in_fmt_ctx, m_in_video_index, start_video_dts, AVSEEK_FLAG_FRAME);
			LOG("INFO","pos:%d/%lld, start_video_dts:%lld, ret:%d\n\n", pos, video_duration, start_video_dts, nRet);

			// 跳到音频80秒（80 000毫秒）处的帧（采样）
			//av_seek_frame(m_in_fmt_ctx, m_in_audio_index, 80000 * aud_time_scale / time_base, AVSEEK_FLAG_BACKWARD);
			if (nRet < 0)
				LOG("ERROR","av_seek_fram failed %d\n\n", nRet);
		}
		else LOG("ERROR","pos=%d, duration=%d\n\n", pos, video_duration);
	}
	else LOG("ERROR","m_in_fmt_ctx=%p,m_in_video_index=%d,pos=%d\n\n", m_in_fmt_ctx, m_in_video_index, pos);
	return nRet;
}
// 播放速度(0.5, 1.0, 2.0 等)
void CAVStream::speed(float value)
{
}
void CAVStream::pause(bool flag/*=true*/)
{
	if(flag)
		m_stream_thd.Pause();
	else m_is_pause = true;
}
void CAVStream::continu(bool flag/*=true*/)
{
	if (flag)
		m_stream_thd.Continue();
	else m_is_pause = false;
}
STATE CAVStream::status()
{
	return m_stream_thd.GetState();
}
int CAVStream::print_ffmpeg_error(int err)
{
	char buff[256] = { 0 };
	av_strerror(err, buff, sizeof(buff));
	LOG("ERROR"," '%s'\n", buff);
	return err;
}
int snapshoot_read_buffer(void* arg, uint8_t* buff, int buf_size);
int CAVStream::snapshoot(const char* ip, int port, const char* filename)
{
	int nRet = -1;
	if ((NULL == ip) || (NULL != filename)) {
		LOG("ERROR", "Parameter error %p %p\n", ip, filename);
		return nRet;
	}
	AVCodecContext* video_codec_ctx = NULL;   // 视频解码器上下文
	AVFormatContext* in_fmt_ctx = avformat_alloc_context();
	//av_register_all();
	avformat_network_init();

	do {
		CUdpClient udp;
		nRet = udp.Open(ip, port);
		if (0 != nRet) {
			LOG("ERROR", "Udp client opened failed %d\n", nRet);
			break;
		}
		int buf_size = 1024 * 1024 * 5;
		udp.SetSockOption(SO_RCVBUF, &buf_size, sizeof(buf_size));
		unsigned char* buff_in = (unsigned char*)av_malloc(BUFF_SIZE);
		in_fmt_ctx->pb = avio_alloc_context(buff_in, BUFF_SIZE, 0, &udp, snapshoot_read_buffer, NULL, NULL);
		in_fmt_ctx->flags = AVFMT_FLAG_CUSTOM_IO;

		AVInputFormat* ifmt = NULL;
		AVDictionary* opts = NULL;
		//av_dict_set_int(&opts, "rtbufsize", 1843200000, 0);
		//av_dict_set(&opts, "buffer_size", "2097152", 0);
		//av_dict_set(&opts, "max_delay", "0", 0);
		av_dict_set(&opts, "stimeout", "9000000", 0);
		nRet = avformat_open_input(&in_fmt_ctx, "", ifmt, &opts);
		av_dict_free(&opts);
		if (nRet < 0) {
			av_log(in_fmt_ctx, AV_LOG_ERROR, "avformat_open_input failed %d\n", nRet);
			break;
		}
		nRet = avformat_find_stream_info(in_fmt_ctx, NULL);
		if (nRet < 0) {
			av_log(NULL, AV_LOG_ERROR, "video stream not found.\n");
			avformat_close_input(&in_fmt_ctx);
			break;
		}
		av_dump_format(in_fmt_ctx, 0, NULL, 0);

		int video_index = -1;
		for (int i = 0; i < in_fmt_ctx->nb_streams; i++) {
			if (in_fmt_ctx->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO) {
				video_index = i;
				break;
			}
		}
		if (-1 == video_index) {
			av_log(NULL, AV_LOG_ERROR, "video stream not found.\n");
			avformat_close_input(&in_fmt_ctx);
			nRet = -1;
			break;
		}
		// 获取解码器上下文
		AVCodecParameters* codec_params = in_fmt_ctx->streams[video_index]->codecpar;
		// 获取解码器
		const AVCodec* codec = avcodec_find_decoder(codec_params->codec_id);
		if (NULL == codec) {
			LOG("ERROR", "Codec not found!\n");
			avformat_close_input(&in_fmt_ctx);
			break;
		}
		// 解码器上下文
		video_codec_ctx = avcodec_alloc_context3(NULL);
		if (avcodec_parameters_to_context(video_codec_ctx, codec_params) < 0) {
			LOG("ERROR", "avcodec_parameters_to_context error\n");
			avformat_close_input(&in_fmt_ctx);
			break;
		}
		nRet = avcodec_open2(video_codec_ctx, codec, NULL);
		if (nRet < 0) {
			av_log(in_fmt_ctx, AV_LOG_ERROR, "Failed to open decoder\n");
			avformat_close_input(&in_fmt_ctx);
			break;
		}
		//uint8_t* buffer = NULL;
		//AVFrame* frame_rgb = NULL;          // 转换后的RGB帧
		//SwsContext* pSwsCtx = NULL;         // 图像处理上下文

		AVFrame* frame = av_frame_alloc();              // 解码后的视频帧
		AVPacket packet;
		int read_frame_fail = 0;
		while (true) {
			nRet = av_read_frame(in_fmt_ctx, &packet);
			if (nRet < 0) {
				read_frame_fail++;
				if (read_frame_fail >= 25) {
					char buff[256] = { 0 };
					av_strerror(nRet, buff, sizeof(buff));
					LOG("ERROR", "av_read_frame error %d \"%s\"\n", nRet, buff);
					break;
				}
				av_usleep(200000);
			}
			if (packet.stream_index != video_index) {
				av_packet_unref(&packet);
				continue;
			}
			//nRet = avcodec_decode_video2(codec_ctx, frame, &gotFrame, packet);
			nRet = avcodec_send_packet(video_codec_ctx, &packet); // 发送帧到解码队列
			if ((nRet < 0) || (AVERROR(EAGAIN) == nRet) || (AVERROR_EOF == nRet)) {
				char buff[256] = { 0 };
				av_strerror(nRet, buff, sizeof(buff));
				LOG("ERROR", "avcodec_send_packet error %d \"%s\"\n", nRet, buff);
				//break;
			}
			//nRet = avcodec_receive_frame(video_codec_ctx, frame);
			//if (nRet < 0) {
			//    char buff[256] = { 0 };
			//    av_strerror(nRet, buff, sizeof(buff));
			//    LOG("ERROR", "avcodec_send_packet error %d \"%s\"\n", nRet, buff);
			//    break;
			//}
			//
			while (nRet >= 0) {
				nRet = avcodec_receive_frame(video_codec_ctx, frame);
				if ((AVERROR(EAGAIN) == nRet) || (AVERROR_EOF == nRet)) {
					//char buff[256] = { 0 };
					//av_strerror(nRet, buff, sizeof(buff));
					//LOG("ERROR", "avcodec_receive_frame error %d \"%s\"\n", nRet, buff);
					break;
				}
				//printf("frame: %d\n", video_codec_ctx->frame_number);
				// 存储为JPEG
				nRet = save_to_jpeg(frame, video_codec_ctx->width, video_codec_ctx->height, filename);
			}
			av_frame_unref(frame);
			av_packet_unref(&packet);
			//break;
		} // while (true)
		if (NULL != frame)
			av_frame_free(&frame);
	} while (false);

	if (NULL != video_codec_ctx)
		avcodec_close(video_codec_ctx);
	if (NULL != in_fmt_ctx) {
		//avcodec_close(avc_cxt);
		if (NULL != in_fmt_ctx->pb) {
			if (NULL != in_fmt_ctx->pb->buffer) {
				av_free(in_fmt_ctx->pb->buffer);
				in_fmt_ctx->pb->buffer = NULL;
			}
			avio_context_free(&in_fmt_ctx->pb);
			in_fmt_ctx->pb = NULL;
		}
		avformat_free_context(in_fmt_ctx);
	}
	return nRet;
}
int snapshoot_read_buffer(void* arg, uint8_t* buff, int buf_size)
{
	CUdpClient* udp = (CUdpClient*)arg;
	static char buf[BUFF_SIZE] = { 0 };
	int nRet = udp->Recv(buf, BUFF_SIZE);
	if (nRet > 0) {
		static int rtp_head_size = sizeof(rtp_header_t);
		nRet -= rtp_head_size;
		memcpy(buff, buf + rtp_head_size, nRet);
	}
	return (nRet > 0) ? nRet : 0;
}
int CAVStream::save_to_jpeg(AVFrame* frame, int w, int h, const char* filename, custom_cbk cbk/*=NULL*/, void* arg/*=NULL*/)
{
	AVFormatContext* out_fmt_cxt = NULL;
	AVCodecContext* codec_ctx = NULL;
	int nRet = -1;
	do {
		nRet = avformat_alloc_output_context2(&out_fmt_cxt, NULL, "singlejpeg", filename);
		if (nRet < 0) {
			av_log(out_fmt_cxt, AV_LOG_ERROR, "avformat_alloc_output_context2 error\n");
			LOG("ERROR", "avformat_alloc_output_context2 failed %d\n", nRet);
			break;
		}
		// 设置输出文件的格式
		//out_fmt_cxt->oformat = av_guess_format("mjpeg",NULL,NULL);
		if (NULL != filename) {
			// 创建和初始化一个和该URL相关的AVIOContext
			nRet = avio_open2(&out_fmt_cxt->pb, filename, AVIO_FLAG_READ_WRITE, NULL, NULL);
			if (nRet < 0) {
				LOG("ERROR", "avio_open2 failed %d\n", nRet);
				av_log(out_fmt_cxt, AV_LOG_ERROR, "avio_open function failed %d \"%s\"\n", nRet, filename);
				avformat_free_context(out_fmt_cxt);
				out_fmt_cxt = NULL;
				break;
			}
		}
		else {
			unsigned char* buffer = (uint8_t*)av_malloc(BUFF_SIZE);
			out_fmt_cxt->pb = avio_alloc_context(buffer, BUFF_SIZE, 1, arg, NULL, cbk, NULL);
			if (NULL == out_fmt_cxt->pb) {
				av_log(out_fmt_cxt, AV_LOG_ERROR, "avio_alloc_context function failed %d\n");
				LOG("ERROR", "avio_alloc_context failed %d\n", nRet);
				break;
			}
			out_fmt_cxt->flags = AVFMT_FLAG_CUSTOM_IO;
		}

		// new一个流并为其设置视频参数
		AVStream* stream = avformat_new_stream(out_fmt_cxt, NULL);
		if (NULL == stream) {
			av_log(out_fmt_cxt, AV_LOG_ERROR, "avformat_new_stream failed.\n");
			LOG("ERROR", "avformat_new_stream failed %d\n", nRet);
			break;
		}
		av_dump_format(out_fmt_cxt, 0, filename, 1);

		const AVCodec* codec = avcodec_find_encoder(stream->codecpar->codec_id);
		if (NULL == codec) {
			av_log(out_fmt_cxt, AV_LOG_ERROR, "Encoder not found.\n");
			LOG("ERROR", "avcodec_find_encoder failed %d\n", nRet);
			break;
		}
		codec_ctx = avcodec_alloc_context3(codec);
		if (NULL == codec_ctx) {
			LOG("ERROR", "avcodec_alloc_context3 failed.\n");
			break;
		}
		codec_ctx->codec_id = out_fmt_cxt->oformat->video_codec;
		codec_ctx->codec_type = AVMEDIA_TYPE_VIDEO;
		codec_ctx->pix_fmt = AV_PIX_FMT_YUVJ420P;
		codec_ctx->height = h;
		codec_ctx->width = w;
		codec_ctx->time_base.num = 1;
		codec_ctx->time_base.den = 25;
		if (avcodec_open2(codec_ctx, codec, NULL) < 0) {
			av_log(out_fmt_cxt, AV_LOG_ERROR, "Failed to open encoder.\n");
			LOG("ERROR", "avcodec_open2 failed %d\n", nRet);
			break;
		}
		avcodec_parameters_from_context(stream->codecpar, codec_ctx);

		avformat_write_header(out_fmt_cxt, NULL);       // 1.写入文件头

		AVPacket packet;
		av_init_packet(&packet);
		//int got_picture = 0;
		//nRet = avcodec_encode_video2(codec_ctx, packet, frame, &got_picture);
		//if (nRet < 0) {
		//    av_log(out_fmt_cxt, AV_LOG_ERROR, "Encode failed.\n");
		//    LOG("ERROR", "avcodec_encode_video2 failed %d\n", nRet);
		//    break;
		//}
		//if (got_picture == 1) {
		//    nRet = av_write_frame(out_fmt_cxt, packet); // 2.写入编码后的视频帧
		//    if(nRet < 0)
		//        LOG("ERROR", "av_write_frame failed %d\n", nRet);
		//}
		/////////////////////////
		nRet = avcodec_send_frame(codec_ctx, frame);
		if (nRet < 0) {
			char err_msg[128] = { 0 };
			av_strerror(nRet, err_msg, sizeof(err_msg));
			LOG("ERROR", "avcodec_send_frame function failed [ret:%d,msg:'%s']\n", nRet, err_msg);
			break;
		}
		int getpacket_ret = 0;
		while (getpacket_ret >= 0) {
			// H264编码 (YUV420p -> H264)
			getpacket_ret = avcodec_receive_packet(codec_ctx, &packet);
			if ((AVERROR(EAGAIN) == getpacket_ret) || (AVERROR_EOF == getpacket_ret))
				break;
			else if (getpacket_ret < 0) {
				LOG("ERROR", "avcodec_receive_packet function failed %d\n", getpacket_ret);
				break;
			}
			//nRet = av_write_frame(out_fmt_cxt, &packet); // 2.写入编码后的视频帧
			nRet = av_interleaved_write_frame(out_fmt_cxt, &packet);
		}
		av_packet_unref(&packet);
		nRet = av_write_trailer(out_fmt_cxt);  // 3.写入流尾至文件
		if (nRet < 0)
			LOG("ERROR", "av_write_frame failed %d\n", nRet);
		//av_frame_unref(frame);
	} while (false);
	if (NULL != out_fmt_cxt) {
		if (out_fmt_cxt->flags & AVFMT_FLAG_CUSTOM_IO) {
			avio_context_free(&out_fmt_cxt->pb);
		}
		else {
			if (NULL != out_fmt_cxt->pb)
				avio_close(out_fmt_cxt->pb);
		}
		if (NULL != codec_ctx)
			avcodec_close(codec_ctx);
		avformat_free_context(out_fmt_cxt);
	}
	if (NULL != codec_ctx)
		avcodec_free_context(&codec_ctx);
	return nRet;
}