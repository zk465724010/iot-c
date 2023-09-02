
#include "rtmp_stream.h"
#include "log.h"
#if (defined (__MY_LOG_H__) && defined (_WIN32))
	#include <Windows.h>		// ��־ģ��
#endif

#ifdef _WIN32
	#ifdef __cplusplus
	extern "C" {
	#endif
	#include "libavformat/avformat.h"
	#include  "libavformat/avio.h"
	#include  "libavcodec/avcodec.h"
	#include  "libavutil/avutil.h"
	#include  "libavutil/time.h"
	#include  "libavutil/log.h"
	#include  "libavutil/mathematics.h"
	#ifdef __cplusplus
	};
	#endif
#else
	#ifdef __cplusplus
	extern "C" {
	#endif
	#include "libavformat/avformat.h"
	#include "libavformat/avio.h"
	#include  "libavcodec/avcodec.h"
	#include  "libavutil/avutil.h"
	#include  "libavutil/time.h"
	#include  "libavutil/log.h"
	#include  "libavutil/mathematics.h"
	#ifdef __cplusplus
	};
	#endif
#endif

CRtmpStream::CRtmpStream()
{
	init();
}
CRtmpStream::~CRtmpStream()
{
	release();
}
int CRtmpStream::init()
{
	//av_register_all();
	//avdevice_register_all();	//ע���豸
	//avformat_network_init();	//���ڳ�ʼ�����硣FFmpeg����Ҳ֧�ֽ��װRTSP������,���Ҫ���װ�������ݸ�ʽ����ɵ��øú�����
	//set log
	//av_log_set_callback(custom_output);
	//av_log_set_level(AV_LOG_ERROR);
	int nRet = avformat_network_init();
	if (nRet < 0) {
		char buff[128] = { 0 };
		av_strerror(nRet, buff, sizeof(buff));
		LOG("ERROR", "avformat_network_init function failed %d '%s'\n", nRet, buff);
	}
	return nRet;
}
void CRtmpStream::release()
{
	avformat_network_deinit();
}
int CRtmpStream::open(const stream_param_t *param)
{
	int nRet = -1;
	if (NULL == param) {
		LOG("ERROR", "Parameter error input:%p output:%p\n", param);
		return nRet;
	}
	if ((0 == strlen(param->input)) || (0 == strlen(param->output))) {
		LOG("ERROR","input=%p,output=%p\n", param->input, param->output);
		return nRet;
	}
	AVDictionary* opt = NULL;
	if(param->buffer_size > 0) {
		char buffer_size[64] = {0};
		sprintf(buffer_size, "%d", param->buffer_size);
		av_dict_set(&opt, "buffer_size", buffer_size, 0);
	}
	if ((0 == strcmp("tcp", param->rtsp_transport)) || (0 == strcmp("udp", param->rtsp_transport)))
		av_dict_set(&opt, "rtsp_transport", param->rtsp_transport, 0); // RTSP����ģʽ(ȡֵ:tcp,udp)

	if (param->stimeout > 0) {
		char stimeout[16] = {0};
		sprintf(stimeout, "%d", param->stimeout);
		// ��δ����'stimeout'ѡ��,��IPC����ʱ,av_read_frame����������(��λ:΢��)
		av_dict_set(&opt, "stimeout", stimeout, 0);
	}
	//av_dict_set(&opt, "max_delay", "100000", 0);
	//av_dict_set(&opt, "fflags", "nobuffer", 0);
	//av_dict_set(&opt, "video_size", szVedioSize, 0);	// �ֱ��ʴ�С
	// һ������£�����ͷ�����������ԭʼ��������mjpg��YUYV422
	//av_dict_set(&opt, "pixel_format", av_get_pix_fmt_name(AV_PIX_FMT_YUYV422), 0);// �����ʽ
	//av_dict_set(&opt, "framerate", "30", 0);	// ֡��
	do {
		const AVInputFormat *ifmt = av_find_input_format(param->input);
		char url[256] = { 0 };
		#ifdef _WIN32
		if (0 == strcmp(param->input, "vfwcap")) {
			//av_dict_set(&opt, "rtbufsize", "1024M", 0);
			strcpy(url, param->device_name); // ����ͷ���(��: 0,1,2 ...)
			print_vfw_device();
		}
		else if (0 == strcmp(param->input, "dshow")) {
			// ʹ��Directshow��ע����Ҫ��װ��������screen-capture-recorder
			// �����ַ�� http://sourceforge.net/projects/screencapturer/
			// ��Linux�¿���ʹ��x11grab¼����Ļ����MacOS�¿���ʹ��avfoundation¼����Ļ��
			//
			// ��ѯ�豸����(����ͷ,¼��): ffmpeg -list_devices true -f dshow -i dummy
			// �鿴�豸����: '�豸������'->'�����'
			// ����ͷ������ (��:"Integrated Webcam(����ͷ)","screen-capture-recorder(��Ļ)")
			sprintf(url, "video=%s", param->device_name);
			//av_dict_set(&opt, "vcodec", "libx264", 0);
			//av_dict_set(&opt, "preset", "ultrafast", 0);
			//av_dict_set(&opt, "acodec", "libmp3lame", 0);
			print_dshow_device(param->device_name);
		}
		else if (0 == strcmp(param->input, "gdigrab")) {
			strcpy(url, param->device_name); // ��Ļ����(��: "desktop")
			// gdigrab��FFmpegר������ץȡWindows������豸���ǳ��ʺ�������Ļ¼�ơ���ͨ����ͬ������URL֧�����ַ�ʽ��ץȡ��
			//  (1)."desktop"��ץȡ�������档����ץȡ�����е�һ���ض�������
			//  (2)."title={��������}"��ץȡ��Ļ���ض���һ�����ڣ�Ŀǰ���Ĵ��ڻ����������⣩��
			//gdigrab���⻹֧��һЩ�����������趨ץ����λ�ã�
			//  offset_x��ץ����ʼ������ꡣ
			//  offset_y��ץ����ʼ�������ꡣ
			//  video_size��ץ���Ĵ�С��
			//  framerate��ץ����֡�ʡ�
			//av_dict_set(&opt,"framerate","5",0);	// ץ����֡��
			//av_dict_set(&opt,"offset_x","20",0);	// ץ����ʼ��x����
			//av_dict_set(&opt,"offset_y","40",0);	// ץ����ʼ��y����
			//av_dict_set(&opt,"video_size","640x480",0); // ץ���Ĵ�С
		}
		#elif __linux__
		if (0 == strcmp(param->input, "video4linux2")) {
			// linux�µ�usb����ͷ����Ϊ"/dev/video0 ��/dev/video1"
			strcpy(url, param->device_name); // ����ͷ������ (��:"/dev/video0")
		}
		#endif
		else {
			strcpy(url, param->input);
		}
		LOG("INFO","open input: '%s'\n", url);
		// ������
		nRet = avformat_open_input(&m_ifmt_ctx, url, (AVInputFormat*)ifmt, &opt);
		av_dict_free(&opt);
		opt = NULL;
		char err_msg[128] = { 0 };
		if (nRet < 0) {
			av_strerror(nRet, err_msg, sizeof(err_msg));
			LOG("ERROR", "Could not open input file '%s' %d '%s'\n", url, nRet, err_msg);
			break;
		}
		//m_ifmt_ctx->interrupt_callback.opaque = &read_pkt_time;
		//m_ifmt_ctx->interrupt_callback.callback = interrupt_cbk;
		nRet = avformat_find_stream_info(m_ifmt_ctx, NULL);
		if (nRet < 0) {
			av_strerror(nRet, err_msg, sizeof(err_msg));
			LOG("ERROR", "Failed to retrieve input stream information %d '%s'\n", nRet, err_msg);
			break;
		}
		for (int i = 0; i < m_ifmt_ctx->nb_streams; i++) {
			if (AVMEDIA_TYPE_VIDEO == m_ifmt_ctx->streams[i]->codecpar->codec_type)
				m_video_index = i;
			else if (AVMEDIA_TYPE_AUDIO == m_ifmt_ctx->streams[i]->codecpar->codec_type)
				m_audio_index = i;
		}
		if ((-1 == m_video_index) || ((-1 == m_video_index) && (-1 == m_audio_index))) {
			LOG("ERROR", "video_index=%d, audio_index=%d.\n", m_video_index, m_audio_index);
			break;
		}
		av_dump_format(m_ifmt_ctx, 0, param->input, 0);
		if (param->start_time > 0) { // ���㿪ʼdts (����¼���ļ�ʱ)
			int64_t start_video_dts = m_ifmt_ctx->streams[m_video_index]->start_time + (int64_t)(param->start_time / av_q2d(m_ifmt_ctx->streams[m_video_index]->time_base));
			nRet = av_seek_frame(m_ifmt_ctx, m_video_index, start_video_dts, AVSEEK_FLAG_FRAME);
			if (nRet < 0) {
				char err_msg[128] = {0};
				av_strerror(nRet, err_msg, sizeof(err_msg));
				LOG("WARN", "avformat_find_stream_info function failed [ret:%d,start_time:%lld,msg:'%s']\n", nRet, param->start_time, err_msg);
			}
		}
		//////////////////////////////////////////////////////////////////////////////////////////
		// Output
		nRet = avformat_alloc_output_context2(&m_ofmt_ctx, NULL, param->output_format, param->output);
		if ((nRet < 0) || (NULL == m_ofmt_ctx)) {
			av_strerror(nRet, err_msg, sizeof(err_msg));
			LOG("ERROR", "avformat_alloc_output_context2 function failed [ret:%d,msg:'%s']\n", nRet, err_msg);
			break;
		}
		// �����������Ƶ��
		for (int i = 0; i < m_ifmt_ctx->nb_streams; i++) {
			AVStream* in_stream = m_ifmt_ctx->streams[i];
			AVStream* out_stream = NULL;
			if (AVMEDIA_TYPE_VIDEO == in_stream->codecpar->codec_type) {
				if (AV_CODEC_ID_H265 == in_stream->codecpar->codec_id) {
					m_h265_codec_ctx = make_decoder(in_stream, param);// ����H265������
					m_h264_codec_ctx = make_encoder(in_stream, param); // ����H264������
					if ((NULL == m_h265_codec_ctx) || (NULL == m_h264_codec_ctx)) {
						nRet = -1;
						break;
					}
					printf("thread_count:%d\n\n", m_h265_codec_ctx->thread_count);

					out_stream = avformat_new_stream(m_ofmt_ctx, m_h264_codec_ctx->codec);// ����H264������Ƶ��
					avcodec_parameters_from_context(out_stream->codecpar, m_h264_codec_ctx);
					//av_stream_set_r_frame_rate(out_stream, in_stream->r_frame_rate);//��������֡��
					//out_stream->r_frame_rate = in_stream->r_frame_rate; // ���ϵ�ͬ
					if (param->r_frame_rate > 0) {
						out_stream->r_frame_rate.num = param->r_frame_rate;
						out_stream->r_frame_rate.den = 1;
					}
					else out_stream->r_frame_rate = in_stream->r_frame_rate;
					out_stream->codecpar->codec_tag = 0;
					printf("---------------------------------\n");
					printf("r_frame_rate:%d/%d\n", out_stream->r_frame_rate.num, out_stream->r_frame_rate.den);
					printf("---------------------------------\n");
					continue;
				}
			}
			const AVCodec* codec = avcodec_find_decoder(in_stream->codecpar->codec_id);
			out_stream = avformat_new_stream(m_ofmt_ctx, codec); // new��Ƶ,��Ƶ��
			avcodec_parameters_copy(out_stream->codecpar, in_stream->codecpar);
			out_stream->codecpar->codec_tag = 0;
		} // for()
		av_dump_format(m_ofmt_ctx, 0, param->output, 1);
		print_codec_context(m_h264_codec_ctx);

		if (!(m_ofmt_ctx->oformat->flags & AVFMT_NOFILE)) {
			nRet = avio_open2(&m_ofmt_ctx->pb, param->output, AVIO_FLAG_READ_WRITE, NULL, NULL);
			if (nRet < 0) {
				av_strerror(nRet, err_msg, sizeof(err_msg));
				LOG("ERROR", "avio_open2 function failed [ret:%d,msg:'%s']\n", nRet, err_msg);
				break;
			}
		}
		av_dict_set(&opt, "movflags", "frag_keyframe+empty_moov+omit_tfhd_offset+faststart+separate_moof+disable_chpl+default_base_moof+dash", 0);
		nRet = avformat_write_header(m_ofmt_ctx, &opt);	// 1.д����ļ�ͷ
		av_dict_free(&opt);
		if (nRet < 0) {
			av_strerror(nRet, err_msg, sizeof(err_msg));
			LOG("ERROR", "avformat_write_header function failed [ret:%d,msg:'%s']\n", nRet, err_msg);
			break;
		}
		////////////////////////////////////////////////////////////////
		#ifdef __THREAD11_H__
		if (nRet >= 0)
			nRet = m_thd.Run(ThdProc, this);
		#endif
	} while (false);

	return nRet;
}
void CRtmpStream::close()
{
	#ifdef __THREAD11_H__
	m_thd.Stop();
	#endif
	if (NULL != m_h265_codec_ctx) {
		avcodec_close(m_h265_codec_ctx);
		avcodec_free_context(&m_h265_codec_ctx);
		m_h265_codec_ctx = NULL;
	}
	if (NULL != m_h264_codec_ctx) {
		avcodec_close(m_h264_codec_ctx);
		avcodec_free_context(&m_h264_codec_ctx);
		m_h264_codec_ctx = NULL;
	}
	if (NULL != m_ifmt_ctx) {
		avformat_close_input(&m_ifmt_ctx);
		m_ifmt_ctx = NULL;
	}
	if (NULL != m_ofmt_ctx) {
		if (!(m_ofmt_ctx->oformat->flags & AVFMT_NOFILE))
			avio_close(m_ofmt_ctx->pb);
		avformat_free_context(m_ofmt_ctx);
		m_ofmt_ctx = NULL;
	}
}

#ifdef __THREAD11_H__
void STDCALL CRtmpStream::ThdProc(STATE &state, PAUSE &p, void *pUserData)
{
	CRtmpStream *stream = (CRtmpStream*)pUserData;
	AVPacket pkt;
	AVStream *in_stream = NULL, *out_stream = NULL;
	int64_t start_time = av_gettime();
	bool first_video = true, first_audio = true, first_video_frame = true, o_first_video = true;
	const char* url = stream->m_ifmt_ctx->url;
	bool is_playback = false;
	if (NULL != url) {
		is_playback = (NULL != strstr(url, "rtsp://")) ? false : true;
	}
	double avg_frame_rate = av_q2d(stream->m_ifmt_ctx->streams[stream->m_video_index]->avg_frame_rate); // ƽ����Ƶ֡��
	int sp = (1000000.0 / avg_frame_rate);
	int o_video_index = av_find_best_stream(stream->m_ofmt_ctx, AVMEDIA_TYPE_VIDEO, -1, -1, NULL, 0);
	int o_audio_index = av_find_best_stream(stream->m_ofmt_ctx, AVMEDIA_TYPE_AUDIO, -1, -1, NULL, 0);

	int64_t read_frame_fail = 0, write_frame_fail = 0, read_pkt_time = 0, video_number = 0, audio_number = 0;
	bool first_frame_video = true;
	char err_msg[256] = { 0 };
	AVFrame* frame = av_frame_alloc();
	AVPacket out_pkt;
	av_init_packet(&out_pkt);

	int nRet = 0;
	while (TS_STOP != state) {
		if (TS_PAUSE == state)
			p.wait();
		read_pkt_time = av_gettime();
		nRet = av_read_frame(stream->m_ifmt_ctx, &pkt);
		if (nRet < 0) {
			av_packet_unref(&pkt);
			av_strerror(nRet, err_msg, sizeof(err_msg));
			if (nRet == AVERROR_EOF) {
				LOG("INFO", "File reading end [ret:%d,msg:'%s']\n", nRet, err_msg);
				nRet = 0;
				break;
			}
			else {
				read_frame_fail++;
				LOG("ERROR", "read frame failed [ret:%d,count:%d,msg:'%s']\n", nRet, read_frame_fail, err_msg);
				if (read_frame_fail > 25)
					break;
				continue;
			}
		}
		read_frame_fail = 0;
		if (pkt.stream_index == stream->m_video_index) { // ��Ƶ
			if (first_video) {
				first_video = false;
				pkt.dts = pkt.pts = 0;
			}
			else if (pkt.pts == AV_NOPTS_VALUE) {
				video_number++;
				int64_t duration = (double)AV_TIME_BASE / av_q2d(stream->m_ifmt_ctx->streams[stream->m_video_index]->r_frame_rate);
				pkt.pts = video_number * duration;
				pkt.dts = pkt.pts;
				pkt.duration = duration;
				//printf("video pts=%lld,dts=%lld,duration=%lld\n", pkt.pts, pkt.dts, pkt.duration);
			}
			if (NULL != stream->m_cbk.stream_cbk)
				stream->m_cbk.stream_cbk(pkt.data, pkt.size, in_stream->codecpar->width, in_stream->codecpar->height, in_stream->codecpar->format, stream->m_cbk.user_data);
		}
		else if (pkt.stream_index == stream->m_audio_index) { // ��Ƶ
			if (first_audio) {
				first_audio = false;
				pkt.dts = pkt.pts = 0;
			}
			else if (pkt.pts == AV_NOPTS_VALUE) {
				audio_number++;
				int64_t duration = (double)AV_TIME_BASE / av_q2d(stream->m_ifmt_ctx->streams[stream->m_audio_index]->r_frame_rate);
				pkt.pts = audio_number * duration;
				pkt.dts = pkt.pts;
				pkt.duration = duration;
				printf("audio pts=%lld,dts=%lld,duration=%lld\n", pkt.pts, pkt.dts, pkt.duration);
			}
		}
		else {
			av_packet_unref(&pkt);
			continue;
		}
		AVStream* in_stream = stream->m_ifmt_ctx->streams[pkt.stream_index];
		AVStream* out_stream = stream->m_ofmt_ctx->streams[pkt.stream_index];
		// Convert PTS/DTS
		pkt.pts = av_rescale_q_rnd(pkt.pts, in_stream->time_base, out_stream->time_base, (AVRounding)(AV_ROUND_NEAR_INF | AV_ROUND_PASS_MINMAX));
		pkt.dts = av_rescale_q_rnd(pkt.dts, in_stream->time_base, out_stream->time_base, (AVRounding)(AV_ROUND_NEAR_INF | AV_ROUND_PASS_MINMAX));
		pkt.duration = av_rescale_q(pkt.duration, in_stream->time_base, out_stream->time_base);
		pkt.pos = -1;

		if (AVMEDIA_TYPE_VIDEO == in_stream->codecpar->codec_type) {
			if (AV_CODEC_ID_H265 == in_stream->codecpar->codec_id) {
				// H265���� (H265 -> YUV)
				nRet = avcodec_send_packet(stream->m_h265_codec_ctx, &pkt);
				if (nRet < 0) {
					av_packet_unref(&pkt);
					av_strerror(nRet, err_msg, sizeof(err_msg));
					LOG("WARN", "avcodec_send_packet function failed [ret:%d,msg:'%s',url:'%s']\n", nRet, err_msg, url);
					continue;
				}
				int getframe_ret = 0;
				while (getframe_ret >= 0) {
					// H265���� (H265 -> YUV)
					getframe_ret = avcodec_receive_frame(stream->m_h265_codec_ctx, frame);
					if ((AVERROR(EAGAIN) == getframe_ret) || (AVERROR_EOF == getframe_ret)) {
						//LOG("ERROR", "avcodec_receive_frame function failed %d\n", getframe_ret);
						break;
					}
					else if (getframe_ret < 0) {
						av_strerror(getframe_ret, err_msg, sizeof(err_msg));
						LOG("ERROR", "avcodec_receive_frame function failed [ret:%d,msg:'%s']\n", getframe_ret,err_msg);
						goto end;
					}
					//======================================================================
					// H264���� (YUV -> H264)
					nRet = avcodec_send_frame(stream->m_h264_codec_ctx, frame);
					if (nRet < 0) {
						av_strerror(nRet, err_msg, sizeof(err_msg));
						LOG("ERROR", "avcodec_send_frame function failed [ret:%d,msg:'%s']\n", nRet, err_msg);
						//continue;
						goto end;
					}
					int getpacket_ret = 0;
					while (getpacket_ret >= 0) {
						// H264���� (YUV -> H264)
						getpacket_ret = avcodec_receive_packet(stream->m_h264_codec_ctx, &out_pkt);  // ������packet
						if ((AVERROR(EAGAIN) == getpacket_ret) || (AVERROR_EOF == getpacket_ret)) {
							av_packet_unref(&out_pkt);
							break;
						}
						else if (getpacket_ret < 0) {
							av_strerror(getpacket_ret, err_msg, sizeof(err_msg));
							LOG("ERROR", "avcodec_receive_packet function failed [ret:%d,msg:'%s']\n", getpacket_ret, err_msg);
							break;
						}
						if (o_first_video) {
							o_first_video = false;
							out_pkt.dts = out_pkt.pts = 0;
						}
						if ((0 == out_pkt.duration) && (out_stream->r_frame_rate.den > 0)) {
							int64_t duration = (double)AV_TIME_BASE / av_q2d(out_stream->r_frame_rate);
							out_pkt.duration = av_rescale_q(duration, AVRational{ 1,AV_TIME_BASE }, out_stream->time_base);
						}
						double frame_rate = av_q2d(out_stream->r_frame_rate);
						printf("H265->H264 pts=%lld,dts=%lld,duration=%lld,frame_rate=%0.2f\n", out_pkt.pts, out_pkt.dts, out_pkt.duration, frame_rate);

						nRet = av_interleaved_write_frame(stream->m_ofmt_ctx, &out_pkt);
						if (nRet < 0) {
							av_strerror(nRet, err_msg, sizeof(err_msg));
							write_frame_fail++;
							LOG("ERROR", "av_interleaved_write_frame function faild [ret:%d,count:%d,msg:'%s']\n", nRet, write_frame_fail, err_msg);
							if (write_frame_fail > 25)
								break;
						}
						else write_frame_fail = 0;
						av_packet_unref(&out_pkt);
					}
				} // while
				if (is_playback) { // // H265 ��ʱ
					int64_t n = av_gettime() - read_pkt_time;
					if (n < sp)
						av_usleep(sp - n);
				}
				av_packet_unref(&pkt);
				continue;
			} // if (AV_CODEC_ID_H265)
		} // if (AVMEDIA_TYPE_VIDEO
		
		if (AVMEDIA_TYPE_VIDEO == in_stream->codecpar->codec_type) { // ����
			double avg_frame_rate = av_q2d(in_stream->avg_frame_rate);
			printf("H264 pts=%lld,dts=%lld,duration=%lld,frame_rate=%0.2f\n", pkt.pts, pkt.dts, pkt.duration, avg_frame_rate);
		}
		nRet = av_interleaved_write_frame(stream->m_ofmt_ctx, &pkt);
		if (nRet < 0) {
			av_strerror(nRet, err_msg, sizeof(err_msg));
			write_frame_fail++;
			LOG("ERROR", "av_interleaved_write_frame function faild [ret:%d,count:%d,msg:'%s']\n", nRet, write_frame_fail, err_msg);
			if (write_frame_fail > 25)
				break;
		}
		else write_frame_fail = 0;
		if (is_playback && (pkt.stream_index == stream->m_video_index)) { // H264 & Audio ��ʱ
			int64_t n = av_gettime() - read_pkt_time;
			if (n < sp)
				av_usleep(sp - n);
		}
		av_packet_unref(&pkt);
	}// while

end:
	av_write_trailer(stream->m_ofmt_ctx);	// д�ļ�β
	av_frame_free(&frame);

	LOG("INFO", "Thread exit\n\n");
}
#endif // #ifdef __THREAD11_H__
void CRtmpStream::print_codec_context(AVCodecContext* ctx)
{
	if (NULL != ctx) {
		printf("------------------------------------------\n\n");
		printf("    codec_id:%d\n", ctx->codec_id);
		printf("  codec_type:%d\n", ctx->codec_type);
		printf("     pix_fmt:%d\n", ctx->pix_fmt);	// s_ctx->pix_fmt;//AV_PIX_FMT_YUV420P; // ���ظ�ʽ
		printf("        size:%dx%d\n", ctx->width, ctx->height);  // ��Ƶ�߶�
		printf("   time_base:%d/%d\n", ctx->time_base.num, ctx->time_base.den);// ʱ���
		printf("   framerate:%d/%d\n", ctx->framerate.num, ctx->framerate.den);// ֡��
		printf(" sample_rate:%d\n", ctx->sample_rate);
		//printf(" sample_rate:%d\n",);
		printf("    bit_rate:%lld\n", ctx->bit_rate);                // ����(�����ʹ�С,���ʻ᲻������)
		printf("    gop_size:%d\n", ctx->gop_size);//  50;// s_ctx->gop_size;// ÿ25֡����һ��I֡,I֡ԽС��ƵԽС(I֡���)
		printf("        qmax:%d\n", ctx->qmax);// s_ctx->qmax;// 51;// s_ctx->qmax;//        // �������ϵ��
		printf("        qmin:%d\n", ctx->qmin);// s_ctx->qmin; // 10;// s_ctx->qmin;//        // ��С����ϵ��
		printf("  keyint_min:%d\n", ctx->keyint_min);// s_ctx->keyint_min; //50;// s_ctx->keyint_min;
		printf("   codec_tag:%u\n", ctx->codec_tag);
		printf("    me_range:%d\n", ctx->me_range);
		printf("   max_qdiff:%d\n", ctx->max_qdiff);
		printf("max_b_frames:%d\n", ctx->max_b_frames);
		printf("   qcompress:%0.3f\n", ctx->qcompress);
		printf("       qblur:%0.3f\n", ctx->qblur);
		printf("------------------------------------------\n\n");
	}
}
// ����һ��������
AVCodecContext* CRtmpStream::make_encoder(AVStream* s, const stream_param_t *params)
{
	// int av_codec_is_encoder(const AVCodec * codec);
	// int av_codec_is_decoder(const AVCodec * codec);
	AVCodecContext* encodec_ctx = NULL;
	AVCodecContext* s_ctx = NULL;
	do {
		//////////////////////////////////////////////////////////////////
		const AVCodec* s_codec = avcodec_find_decoder(s->codecpar->codec_id);
		if (NULL == s_codec) {
			LOG("ERROR", "avcodec_find_decoder function faild.\n");
			break;
		}
		s_ctx = avcodec_alloc_context3(s_codec);
		if (NULL == s_ctx) {
			LOG("ERROR", "avcodec_alloc_context3 function faild.\n");
			break;
		}
		avcodec_parameters_to_context(s_ctx, s->codecpar);
		////////////////////////////////////////////////////////////////////
		//const AVCodec* encodec = avcodec_find_encoder_by_name("libx264"); // �������ƻ�ȡ������
		const AVCodec* encodec = avcodec_find_encoder(AV_CODEC_ID_H264);    // ����ID��ȡ������
		if (NULL == encodec) {
			LOG("ERROR", "avcodec_find_encoder function faild.\n");
			break;
		}
		encodec_ctx = avcodec_alloc_context3(encodec);
		if (NULL == encodec_ctx) {
			LOG("ERROR", "avcodec_alloc_context3 function faild.\n");
			break;
		}
		//avcodec_copy_context(encodec_ctx, s_ctx);
		//avcodec_parameters_to_context(encodec_ctx, s->codecpar);
		encodec_ctx->codec_id = encodec->id;
		encodec_ctx->codec_type = AVMEDIA_TYPE_VIDEO;
		encodec_ctx->pix_fmt = AV_PIX_FMT_YUV420P;	// s_ctx->pix_fmt;//AV_PIX_FMT_YUV420P; // ���ظ�ʽ
		encodec_ctx->width = s->codecpar->width;    // ��Ƶ���
		encodec_ctx->height = s->codecpar->height;  // ��Ƶ�߶�
		if (params->time_base > 0)
			encodec_ctx->time_base = AVRational{ 1,params->time_base };// ʱ���
		else encodec_ctx->time_base = s->time_base;
		//�� encodec_ctx->time_base = s->time_base; // ʱ���(����)
		//encodec_ctx->framerate = s->r_frame_rate; // ֡��
		if(params->framerate > 0)
			encodec_ctx->framerate = AVRational{ params->framerate,1 };// ֡��
		else encodec_ctx->framerate = s->r_frame_rate; // ֡��
		encodec_ctx->sample_rate = encodec_ctx->framerate.num;
		encodec_ctx->bit_rate = (params->bit_rate>0)? params->bit_rate: s_ctx->bit_rate; //1600000               // ����(�����ʹ�С,���ʻ᲻������)
		//  50;// s_ctx->gop_size;// ÿ25֡����һ��I֡,I֡ԽС��ƵԽС(I֡���)
		encodec_ctx->gop_size = (params->gop_size > 0) ? params->gop_size : s_ctx->gop_size;
		encodec_ctx->qmax = (params->qmax>0)? params->qmax: s_ctx->qmax;// s_ctx->qmax;// 51;// s_ctx->qmax;//        // �������ϵ��
		encodec_ctx->qmin = (params->qmin>0)? params->qmin: s_ctx->qmin;// s_ctx->qmin; // 10;// s_ctx->qmin;//        // ��С����ϵ��
		encodec_ctx->keyint_min = (params->keyint_min>0)? params->keyint_min: s_ctx->keyint_min;// s_ctx->keyint_min; //50;// s_ctx->keyint_min;
		encodec_ctx->codec_tag = params->codec_tag; // 0
		if(params->me_range > 0)
			encodec_ctx->me_range = params->me_range; // 16
		if(params->max_qdiff > 0)
			encodec_ctx->max_qdiff = params->max_qdiff; // 4
		if(params->max_b_frames > 0)
			encodec_ctx->max_b_frames = params->max_b_frames;   // 1  // Optional Param B֡
		if(params->qblur > 0)
			encodec_ctx->qblur = params->qblur;  // 0.0
		if (params->qcompress > 0)
			encodec_ctx->qcompress = params->qcompress;  // 1, 0.6
		encodec_ctx->flags |= AV_CODEC_FLAG_GLOBAL_HEADER; // add sps,dps in mp4 ,no start code
		AVDictionary* options = NULL;
		//if (AV_CODEC_ID_H264 == encodec_ctx->codec_id) {
			av_dict_set(&options, "preset", "fast", 0);      // x264 fast code (ȡֵ:'fast','slow')
			av_dict_set(&options, "tune", "zerolatency", 0); // x264 no delay
		//}		
		int nRet = avcodec_open2(encodec_ctx, encodec, &options); // �򿪱�����
		if (NULL != options)
			av_dict_free(&options);
		if (nRet < 0) {
			avcodec_free_context(&encodec_ctx);
			encodec_ctx = NULL;
			char err_msg[128] = { 0 };
			av_strerror(nRet, err_msg, sizeof(err_msg));
			LOG("ERROR", "avcodec_open2 function faild [ret:%d,msg:'%s']\n", nRet, err_msg);
		}
	} while (false);
	if (NULL != s_ctx)
		avcodec_free_context(&s_ctx);
	return encodec_ctx;
}
AVCodecContext* CRtmpStream::make_decoder(AVStream* s, const stream_param_t* params)
{
	AVCodecContext* codec_ctx = NULL;
	if (NULL != s) {
		do {
			// ����H265��Ƶ������ (H265 -> YUV420p)
			const AVCodec* codec = avcodec_find_decoder(s->codecpar->codec_id); // ���
			//const AVCodec* codec = avcodec_find_decoder_by_name("h264_mediacodec"); // Ӳ��
			if (NULL == codec) {
				LOG("ERROR", "avcodec_find_decoder_by_name function faild.\n");
				break;
			}
			// H265��Ƶ������������
			codec_ctx = avcodec_alloc_context3(codec);
			if (NULL == codec_ctx) {
				LOG("ERROR", "avcodec_alloc_context3 function failed.\n");
				break;
			}
			int nRet = avcodec_parameters_to_context(codec_ctx, s->codecpar);
			if (nRet < 0) {
				LOG("ERROR", "avcodec_parameters_to_context function failed %d\n", nRet);
				avcodec_free_context(&codec_ctx);
				codec_ctx = NULL;
				break;
			}
			codec_ctx->thread_count = params->thread_count;// ����CPU�߳�����
			printf("----------------------------------------\n");
			printf("thread_count:%d\n", codec_ctx->thread_count);
			printf("----------------------------------------\n\n");
			// ��H265��Ƶ������
			nRet = avcodec_open2(codec_ctx, codec, NULL);
			if (nRet < 0) {
				char err_msg[128] = { 0 };
				av_strerror(nRet, err_msg, sizeof(err_msg));
				LOG("ERROR", "avcodec_open2 function faild [ret:%d,msg:'%s']\n", nRet, err_msg);
				avcodec_free_context(&codec_ctx);
				codec_ctx = NULL;
				break;
			}
		} while (false);
	}
	else LOG("ERROR", "Input parameter error, codecpar=%p\n", s);
	return codec_ctx;
}
int CRtmpStream::free_coder(AVCodecContext** coder)
{
	int nRet = -1;
	if (NULL != coder) {
		if (NULL != *coder) {
			avcodec_close(*coder);
			avcodec_free_context(coder);
			*coder = NULL;
		}
	}
	return nRet;
}

void CRtmpStream::print_dshow_device(const char *device_name)
{
	AVFormatContext *pFormatCtx = avformat_alloc_context();
	AVDictionary* options = NULL;
	//av_dict_set(&options, "list_devices", "true", 0);
	av_dict_set(&options, "devices", "true", 0);
	const AVInputFormat *iformat = av_find_input_format("dshow");
	char url[128] = { 0 };
	sprintf(url, "video=%s", device_name);
	printf("------------ dshow device info ------------\n");
	int nRet = avformat_open_input(&pFormatCtx, url, (AVInputFormat*)iformat, &options);
	printf("-------------------------------------------\n");
	av_dict_free(&options);
	options = NULL;
	if (nRet >= 0) {
		avformat_close_input(&pFormatCtx);
	}
	//else {
	//	char err_str[128] = {0};
	//	av_strerror(nRet, err_str, sizeof(err_str));
	//	LOG("ERROR", "avformat_open_input function failed %d '%s'\n", nRet, err_str);
	//}
	////////////////////////////////////////////////////////////
	av_dict_set(&options, "list_options", "true", 0);
	iformat = av_find_input_format("dshow");
	printf("--------- dshow device option info --------\n");
	nRet = avformat_open_input(&pFormatCtx, url, (AVInputFormat*)iformat, &options);
	printf("-------------------------------------------\n\n");
	av_dict_free(&options);
	options = NULL;
	if (nRet >= 0) {
		avformat_close_input(&pFormatCtx);
	}
	//else {
	//	char err_str[128] = {0};
	//	av_strerror(nRet, err_str, sizeof(err_str));
	//	LOG("ERROR", "avformat_open_input function failed %d '%s'\n", nRet, err_str);
	//}
	avformat_free_context(pFormatCtx);
}
void CRtmpStream::print_vfw_device()
{
	AVFormatContext *pFormatCtx = avformat_alloc_context();
	const AVInputFormat *iformat = av_find_input_format("vfwcap");
	printf("------------- vfw device info -------------\n");
	int nRet = avformat_open_input(&pFormatCtx, "list", (AVInputFormat*)iformat, NULL);
	printf("-------------------------------------------\n\n");
	if (nRet >= 0) {
		avformat_close_input(&pFormatCtx);
	}
	//else {
	//	char err_str[128] = {0};
	//	av_strerror(nRet, err_str, sizeof(err_str));
	//	LOG("ERROR", "avformat_open_input function failed %d '%s'\n", nRet, err_str);
	//}
	avformat_free_context(pFormatCtx);
}
void CRtmpStream::print_avfoundation_device()
{
	// Mac OS
	AVFormatContext *pFormatCtx = avformat_alloc_context();
	AVDictionary* options = NULL;
	av_dict_set(&options, "list_devices", "true", 0);
	const AVInputFormat *iformat = av_find_input_format("avfoundation");
	printf("--------- avfoundation device info --------\n");
	int nRet = avformat_open_input(&pFormatCtx, "", (AVInputFormat*)iformat, &options);
	printf("-------------------------------------------\n\n");
	if (nRet >= 0) {
		avformat_close_input(&pFormatCtx);
	}
	//else {
	//	char err_str[128] = {0};
	//	av_strerror(nRet, err_str, sizeof(err_str));
	//	LOG("ERROR", "avformat_open_input function failed %d '%s'\n", nRet, err_str);
	//}
	avformat_free_context(pFormatCtx);
}
