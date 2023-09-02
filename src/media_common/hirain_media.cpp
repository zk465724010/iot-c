
#include "hirain_media.h"

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

#ifdef _WIN32
    #include <Windows.h>
    #define Sleep(n) Sleep(n)
    #ifdef _WINDOWS_
        #define GetObject GetObject
        #define GetMessage GetMessage
    #endif
#else
    #include <unistd.h>		// usleep()
    #define Sleep(n) usleep((n)*1000)
#endif
#include "log.h"
#include "buffer.h"

/////////// 录像回放,异步线程下载 //////////////////////////////////
#include <condition_variable>
#include<mutex>
std::mutex g_thd_mutex;				// 互斥锁
std::condition_variable g_thd_cv;	// 条件变量
IOContext g_file_ctx;
CMinIO* g_file_io = NULL;
////////////////////////////////////////////////////////////////////

int media_init(minio_info_t* cfg)
{
    if (NULL != cfg) {
        if (NULL == g_file_io) {
            g_file_io = new CMinIO();
            g_file_io->print_info(cfg);
            g_file_io->init(cfg);
        }
        return 0;
    }
    return -1;
}
void media_release()
{
    if (NULL != g_file_io) {
        g_file_io->release();
        delete g_file_io;
        g_file_io = NULL;
    }
}

int media_video_stream(char* control_cmd, stream_param_t* params)
{
    if ((NULL == control_cmd) || (NULL == params) || (NULL == g_file_io)) {
        LOG("ERROR", "Input parameter error, [control_cmd:%p,param:%p,minio:%p]\n", control_cmd, params, g_file_io);
        return -1;
    }
    //avdevice_register_all();	//注册设备
    avformat_network_init();	//用于初始化网络。FFmpeg本身也支持解封装RTSP的数据,如果要解封装网络数据格式，则可调用该函数。
    int nRet = -1;
    AVFormatContext* ifmt_ctx = NULL;
    AVFormatContext* ofmt_ctx_mp4 = NULL;
    AVCodecParameters* video_h264_params = NULL;
    stream_buffer_t  cache_buffer;
    bool is_playback = false;
    AVInputFormat* ifmt = NULL;	// av_find_input_format("h264");
    bool reconnet = false;
    int64_t read_pkt_time = 0;
    char err_msg[64] = { 0 };
    CThread11 download_thd;
    CBuffer<char> data_buffer;
    do {
        if ((NULL != params->in_url) && (NULL == params->read_packet)) { // 点播
            AVDictionary* opts = NULL;
            //av_dict_set(&opts, "buffer_size", "2048000", 0);
            av_dict_set(&opts, "stimeout", "3000000", 0);
            av_dict_set(&opts, "rtsp_transport", "tcp", 0);
            nRet = avformat_open_input(&ifmt_ctx, params->in_url, ifmt, &opts);
            av_dict_free(&opts);
            opts = NULL;
            read_pkt_time = av_gettime();
            //ifmt_ctx->interrupt_callback.opaque = &read_pkt_time;
            //ifmt_ctx->interrupt_callback.callback = media_interrupt_cbk;
        }
        else if ((NULL != params->in_url) && (NULL != params->read_packet)) { // 回放
            is_playback = true;
            g_file_ctx.enable = true;
            g_file_ctx.fuzz_size = g_file_ctx.pos = 0;
            g_file_ctx.remote_name = params->in_url;
            g_file_ctx.filesize = g_file_io->get_file_size(g_file_ctx.remote_name);
            if (g_file_ctx.filesize <= 0) {
                LOG("ERROR", "File size %lld\n", g_file_ctx.filesize);
                break;
            }
            g_file_ctx.fuzz = new uint8_t[BUFF_SIZE + 1];
            memset(g_file_ctx.fuzz, 0, BUFF_SIZE + 1);
            printf("------ buff_size = %lld ------\n\n", BUFF_SIZE);
            nRet = data_buffer.Create(50, media_create_object_cbk, media_destory_object_cbk, NULL, 2048, 50);
            if (nRet < 0) {
                LOG("ERROR", "Create buffer failed [ret:%d]\n", nRet);
                break;
            }
            ifmt_ctx = avformat_alloc_context();
            unsigned char* buffer = (unsigned char*)av_malloc(BUFF_SIZE);
            AVIOContext* avio_ctx = avio_alloc_context(buffer, BUFF_SIZE, 0, &data_buffer, params->read_packet, NULL, params->seek_packet);
            if (NULL == avio_ctx) {
                av_free(buffer);
                LOG("ERROR", "Allocation memory failed %p\n", avio_ctx);
                break;
            }
            ifmt_ctx->pb = avio_ctx;
            ifmt_ctx->flags |= AVFMT_FLAG_CUSTOM_IO;
            nRet = avformat_open_input(&ifmt_ctx, "", ifmt, NULL);
        }
        else {
            LOG("ERROR", "in_url=%p, read_packet=%p\n", params->in_url, params->read_packet);
            break;
        }
    start:
        if ((nRet < 0) || (NULL == ifmt_ctx)) {
            av_strerror(nRet, err_msg, sizeof(err_msg));
            LOG("ERROR", "avformat_open_input function failed [ret:%d,msg:'%s',url:'%s']\n", nRet, err_msg, params->in_url);
            nRet = ERROR_OPEN_URL_FAIL;
            break;
        }
        nRet = avformat_find_stream_info(ifmt_ctx, 0);
        if (nRet < 0) {
            LOG("ERROR", "avformat_find_stream_info function failed %d\n", nRet);
            nRet = ERROR_STREAM_INFO_FAIL;
            break;
        }
        int video_index = -1, audio_index = -1;
        for (int i = 0; i < ifmt_ctx->nb_streams; i++) {
            if (ifmt_ctx->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO) {
                video_index = i;
            }
            if (ifmt_ctx->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_AUDIO) {
                audio_index = i;
            }
        }
        if ((-1 == video_index) || ((-1 == video_index) && (-1 == audio_index))) {
            LOG("ERROR", "video_index=%d, audio_index=%d.\n", video_index, audio_index);
            break;
        }
        av_dump_format(ifmt_ctx, 0, params->in_url, 0);

        if (is_playback && (params->start_time > 0)) { // 计算开始dts (播放录像文件时)
            AVStream* in_stream = ifmt_ctx->streams[video_index];
            int64_t start_video_dts = in_stream->start_time + (int64_t)(params->start_time / av_q2d(in_stream->time_base));
            nRet = av_seek_frame(ifmt_ctx, video_index, start_video_dts, AVSEEK_FLAG_BACKWARD); //AVSEEK_FLAG_FRAME,AVSEEK_FLAG_BACKWARD
            if (nRet < 0) {
                av_strerror(nRet, err_msg, sizeof(err_msg));
                LOG("ERROR", "av_seek_frame function failed [ret:%d,start_time:%lld,dts:%lld,msg:'%s']\n", nRet, params->start_time, start_video_dts, err_msg);
                break;
            }
        }
        //////////////////////////////////////////////////////////////////////////////////////////
        // Output MP4
        avformat_alloc_output_context2(&ofmt_ctx_mp4, NULL, "mp4", params->out_url);
        if (NULL == ofmt_ctx_mp4) {
            LOG("ERROR", "avformat_alloc_output_context2 function failed.\n");
            nRet = ERROR_OPEN_OUTSTREAM_FAIL;
            break;
        }
        if (NULL != params->out_url) {
            if (!(ofmt_ctx_mp4->oformat->flags & AVFMT_NOFILE)) {
                nRet = avio_open2(&ofmt_ctx_mp4->pb, params->out_url, AVIO_FLAG_READ_WRITE, NULL, NULL);
                if (nRet < 0) {
                    av_strerror(nRet, err_msg, sizeof(err_msg));
                    LOG("ERROR", "avio_open2 function failed [ret:%d,msg:'%s']\n", nRet, err_msg);
                    break;
                }
            }
        }
        else {
            if(NULL == cache_buffer.buffer)
                cache_buffer.buffer = (uint8_t*)av_malloc(cache_buffer.buf_size);
            cache_buffer.pos = cache_buffer.buffer;
            cache_buffer.limit = cache_buffer.buffer + cache_buffer.buf_size;
            cache_buffer.write_size = 0;
            unsigned char* buffer = (unsigned char*)av_malloc(32768);
            if (NULL == buffer) {
                LOG("ERROR", "av_malloc function failed\n");
                nRet = ERROR_MEM_FAIL;
                break;
            }
            AVIOContext* avio_out = avio_alloc_context(buffer, 32768, 1, &cache_buffer, NULL, params->write_packet, NULL);
            if (NULL == avio_out) {
                LOG("ERROR", "avio_alloc_context function failed.\n");
                av_free(buffer);
                nRet = ERROR_IOCONTENT_FAIL;
                break;
            }
            ofmt_ctx_mp4->pb = avio_out;
            ofmt_ctx_mp4->flags |= AVFMT_FLAG_CUSTOM_IO;
        }
        ////////////////////////////////////////////////////////////////
        AVCodecContext* video_h265_codec_ctx = NULL; // H265解码器上下文
        AVCodecContext* video_h264_codec_ctx = NULL; // H264编码器上下文

        AVCodec* video_h264_codec = NULL; // H264解码或编码器
        video_h264_params = avcodec_parameters_alloc();
        if (NULL == video_h264_params) {
            LOG("ERROR", "avcodec_parameters_alloc function failed.\n");
            nRet = -100;
            break;
        }
        AVStream* in_stream = ifmt_ctx->streams[video_index];
        if (AV_CODEC_ID_H264 == in_stream->codecpar->codec_id) {
            video_h264_codec = avcodec_find_decoder(in_stream->codecpar->codec_id);
            avcodec_parameters_copy(video_h264_params, in_stream->codecpar);
        }
        else if (AV_CODEC_ID_H265 == in_stream->codecpar->codec_id) {
            /////////////////////////////////////////////////////////////////////////////////
            // 查找H265视频解码器 (H265 -> YUV420p)
            AVCodec* in_video_codec = avcodec_find_decoder(in_stream->codecpar->codec_id);
            if (NULL == in_video_codec) {
                LOG("ERROR", "avcodec_find_decoder function failed.\n");
                nRet = ERROR_FIND_CODE_FAIL;
                break;
            }
            // H265视频解码器上下文
            video_h265_codec_ctx = avcodec_alloc_context3(in_video_codec);
            nRet = avcodec_parameters_to_context(video_h265_codec_ctx, ifmt_ctx->streams[video_index]->codecpar);
            if (nRet < 0) {
                LOG("ERROR", "avcodec_parameters_to_context function failed %d\n", nRet);
                nRet = ERROR_CREATE_CODECOTENT_FAIL;
                break;
            }
            // 打开H265视频解码器
            nRet = avcodec_open2(video_h265_codec_ctx, in_video_codec, NULL);
            if (nRet < 0) {
                LOG("ERROR", "avcodec_open2 function failed %d\n", nRet);
                nRet = ERROR_OPEN_CODE_FAIL;
                break;
            }
            /////////////////////////////////////////////////////////////////////////////////
            // 查找H264视频编码器 (YUV420p -> H264)
            AVCodec* video_h264_codec = avcodec_find_encoder(AV_CODEC_ID_H264);
            //AVCodec* out_video_codec = avcodec_find_encoder_by_name("libx264");
            if (video_h264_codec == NULL) {
                LOG("ERROR", "avcodec_find_decoder function failed.\n");
                nRet = ERROR_FIND_CODE_FAIL;
                break;
            }
            video_h264_codec = video_h264_codec;
            video_h264_codec_ctx = avcodec_alloc_context3(video_h264_codec);
            if (NULL == video_h264_codec_ctx) {
                LOG("ERROR", "avcodec_alloc_context3 function failed.\n");
                nRet = ERROR_CREATE_CODECOTENT_FAIL;
                break;
            }
            video_h264_codec_ctx->codec_id = AV_CODEC_ID_H264;	//m_pOutFmtCtx->oformat->video_codec;
            //video_h264_codec_ctx->codec_id = video_h264_codec->id;
            video_h264_codec_ctx->width = video_h265_codec_ctx->width;
            video_h264_codec_ctx->height = video_h265_codec_ctx->height;
            video_h264_codec_ctx->codec_type = AVMEDIA_TYPE_VIDEO;
            video_h264_codec_ctx->pix_fmt = AV_PIX_FMT_YUV420P;// video_h265_codec_ctx->pix_fmt; // AV_PIX_FMT_YUV420P,in_video_codec_ctx->pix_fmt
            // 25 frame a second
            video_h264_codec_ctx->time_base.num = 1;
            video_h264_codec_ctx->time_base.den = 25;
            video_h264_codec_ctx->framerate.num = 25;    // 帧率
            video_h264_codec_ctx->framerate.den = 1;
            //video_h264_codec_ctx->bit_rate = video_h265_codec_ctx->bit_rate * 2;//h264 byterate
            video_h264_codec_ctx->bit_rate = 1600000;
            video_h264_codec_ctx->gop_size = 25;             // i frame interval (I帧间隔)
            video_h264_codec_ctx->qmin = 10;
            video_h264_codec_ctx->qmax = 51;
            //video_h264_codec_ctx->qcompress = 1; // zk
            //video_h264_codec_ctx->max_b_frames = 3; // zk
            //video_h264_codec_ctx->max_b_frames = 1; // zk
            video_h264_codec_ctx->keyint_min = 50;
            video_h264_codec_ctx->codec_tag = 0;
            video_h264_codec_ctx->flags |= AV_CODEC_FLAG_GLOBAL_HEADER; // add sps,dps in mp4 ,no start code
            AVDictionary* opts = NULL;
            av_dict_set(&opts, "preset", "fast", 0);       // x264 fast code
            av_dict_set(&opts, "tune", "zerolatency", 0);  // x264 no delay
            //av_dict_set(&opts, "preset", "slow", 0);     // zk
            //av_dict_set(&opts, "tune", "zerolatency", 0); // zk
            //av_opt_set_int(video_h264_codec_ctx->priv_data, "crf", 23, 0); // zk
            nRet = avcodec_open2(video_h264_codec_ctx, video_h264_codec, &opts);
            av_dict_free(&opts);
            if (nRet < 0) {
                av_strerror(nRet, err_msg, sizeof(err_msg));
                LOG("ERROR", "avcodec_open2 function failed [ret:%d,msg:'%s']\n", nRet, err_msg);
                nRet = ERROR_OPEN_CODE_FAIL;
                break;
            }
            avcodec_parameters_from_context(video_h264_params, video_h264_codec_ctx);
        } // AV_CODEC_ID_H265

        // new 一个视频流
        AVStream* video_stream = avformat_new_stream(ofmt_ctx_mp4, video_h264_codec);
        if (NULL == video_stream) {
            LOG("ERROR", "avformat_new_stream function failed.\n");
            nRet = ERROR_CREATE_AVSTREAM_FAIL;
            break;
        }
        nRet = avcodec_parameters_copy(video_stream->codecpar, video_h264_params);
        if (nRet < 0) {
            av_strerror(nRet, err_msg, sizeof(err_msg));
            LOG("ERROR", "avcodec_parameters_copy function failed [ret:%d,msg:'%s']\n", nRet, err_msg);
            break;
        }
        video_stream->codecpar->codec_tag = 0;
        if (ofmt_ctx_mp4->oformat->flags & AVFMT_GLOBALHEADER)
            ;// video_stream->codec->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;
        // new 一个音频流
        if (audio_index >= 0) {
            // 回放时,若播放速度不为1.0时,去掉音频(倍速播放时音频没有意义了)
            if ((!is_playback) || (is_playback && (*params->ratio == 1.0))) {
                AVCodecParameters* audio_codecpar = ifmt_ctx->streams[audio_index]->codecpar;
                const AVCodec* audio_codec = avcodec_find_decoder(audio_codecpar->codec_id);
                AVStream* audio_stream = avformat_new_stream(ofmt_ctx_mp4, audio_codec);
                if (NULL == audio_stream) {
                    LOG("ERROR", "avformat_new_stream function failed.\n");
                    nRet = ERROR_CREATE_AVSTREAM_FAIL;
                    break;
                }
                nRet = avcodec_parameters_copy(audio_stream->codecpar, audio_codecpar);
                if (nRet < 0) {
                    LOG("ERROR", "avcodec_parameters_copy function failed [ret:%d]\n", nRet);
                    break;
                }
                audio_stream->codecpar->codec_tag = 0;
                if (ofmt_ctx_mp4->oformat->flags & AVFMT_GLOBALHEADER)
                    ;// audio_stream->codec->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;
            }
        }
        av_dump_format(ofmt_ctx_mp4, 0, 0, 1);
        //////////////////////////////////////////////////////////////////////////////////////////
        if (false==reconnet) { // 断流重连后无需发送视频头帧
            // 发送流的类型
            if ((video_index >= 0) && (-1 == audio_index))  // 视频流
                cache_buffer.write_size = VIDEO_ONLY;
            else if ((video_index >= 0) && (audio_index >= 0))// 混合流
                cache_buffer.write_size = VIDEO_AND_AUDIO;
            if(NULL != params->write_packet)
            nRet = params->write_packet(params->out_opaque, cache_buffer.buffer, cache_buffer.write_size);
            cache_buffer.pos = cache_buffer.buffer;
            cache_buffer.write_size = 0;
        }
        else cache_buffer.reconnet = true;

        AVDictionary* opts = NULL;
        av_dict_set(&opts, "movflags", "frag_keyframe+empty_moov+omit_tfhd_offset+faststart+separate_moof+disable_chpl+default_base_moof+dash", 0);
        cache_buffer.video_head = true;
        nRet = avformat_write_header(ofmt_ctx_mp4, &opts);
        cache_buffer.video_head = false;
        cache_buffer.reconnet = false;
        av_dict_free(&opts);
        if (nRet < 0) {
            av_strerror(nRet, err_msg, sizeof(err_msg));
            LOG("ERROR", "avformat_write_header function failed [ret:%d,msg:'%s']\n", nRet, err_msg);
            nRet = ERROR_OPEN_OUTSTREAM_FAIL;
            break;
        }
        AVFrame* frame = av_frame_alloc();
        AVPacket in_pkt;
        av_init_packet(&in_pkt);
        AVPacket out_pkt;
        av_init_packet(&out_pkt);
        int read_frame_fail = 0, write_frame_fail = 0;
        bool first_video = true, first_audio = true, first_frame_video = true;
        int64_t video_number = 0, audio_number = 0;// , read_frame_time = 0;
        int o_video_index = av_find_best_stream(ofmt_ctx_mp4, AVMEDIA_TYPE_VIDEO, -1, -1, NULL, 0);
        int o_audio_index = av_find_best_stream(ofmt_ctx_mp4, AVMEDIA_TYPE_AUDIO, -1, -1, NULL, 0);
        int sp = (1000000.0 / av_q2d(ifmt_ctx->streams[video_index]->avg_frame_rate));
        double avg_frame_rate = av_q2d(ifmt_ctx->streams[video_index]->avg_frame_rate); // 平均视频帧率
        printf("------------ sp=%d ------------\n", sp);
        
        time_t tm1 = time(NULL);
        int64_t t1 = 0;
        int64_t nnnnnn = 30000000;    // 测试 10s
        int64_t v_pts = 0, a_pts = 0, out_pkt_pts = 0, duration = 0;// ofmt_ctx_mp4->streams[video_index]->time_base.
        bool first_i_frame = true;
        int i = 0, num = 2;// av_q2d(ifmt_ctx->streams[video_index]->avg_frame_rate);
        nRet = av_read_frame(ifmt_ctx, &in_pkt);
        printf("----------- pos:%d, num:%d\n", in_pkt.pos, num);
        AVStream* in_vstream = ifmt_ctx->streams[video_index];

        if (is_playback)
            download_thd.Run(media_download_thdproc, &data_buffer);
 
        while (('r' == *(control_cmd + 1)) || ('p' == *(control_cmd + 1))) {
            nRet = av_read_frame(ifmt_ctx, &in_pkt);
            if (nRet < 0) {
                if (nRet == AVERROR_EOF) {
                    char err_msg[128] = { 0 };
                    av_strerror(nRet, err_msg, sizeof(err_msg));
                    LOG("INFO", "File reading end [ret:%d,msg:'%s']\n", nRet, err_msg);
                    nRet = 0;
                    break;
                }
                else {
                    LOG("ERROR", "read frame failed [ret:%d,count:%d]\n", nRet, read_frame_fail);
                    av_packet_unref(&in_pkt);
                    read_frame_fail++;
                    if (read_frame_fail > 25) // read frame failed
                        break;
                    continue;
                }
            }
            read_frame_fail = 0;
            if (video_index == in_pkt.stream_index) { // 视频流
                if (first_video) {
                    first_video = false;
                    in_pkt.dts = 0;
                    in_pkt.pts = 0;
                }
            }
            else if (audio_index == in_pkt.stream_index) { // 音频流
                if (first_audio) {
                    first_audio = false;
                    in_pkt.dts = 0;
                    in_pkt.pts = 0;
                }
            }
            else { // 其他流
                av_packet_unref(&in_pkt);
                printf("Other streams %d\n", in_pkt.stream_index);
                continue;
            }
            if (in_pkt.pts == AV_NOPTS_VALUE) {
                if (video_index == in_pkt.stream_index) { // 视频流
                    AVRational time_base = ofmt_ctx_mp4->streams[o_video_index]->time_base;
                    int64_t calc_duration = (double)AV_TIME_BASE / av_q2d(ifmt_ctx->streams[video_index]->r_frame_rate);
                    in_pkt.pts = (double)(video_number * calc_duration) / (double)(av_q2d(time_base) * AV_TIME_BASE);
                    in_pkt.dts = in_pkt.pts;
                    in_pkt.duration = 3600;// (double)calc_duration / (double)(av_q2d(time_base) * AV_TIME_BASE);
                    video_number++;
                    printf("video in_pkt.pts=%lld,in_pkt.dts=%lld,in_pkt.duration=%lld\n", in_pkt.pts, in_pkt.dts, in_pkt.duration);
                }
                else if (audio_index == in_pkt.stream_index) { // 音频流
                    AVRational time_base = ofmt_ctx_mp4->streams[o_audio_index]->time_base;
                    int64_t calc_duration = (double)AV_TIME_BASE / av_q2d(ifmt_ctx->streams[audio_index]->r_frame_rate);
                    in_pkt.pts = (double)(audio_number * calc_duration) / (double)(av_q2d(time_base) * AV_TIME_BASE);
                    in_pkt.dts = in_pkt.pts;
                    in_pkt.duration = (double)calc_duration / (double)(av_q2d(time_base) * AV_TIME_BASE);
                    audio_number++;
                    printf("audio in_pkt.pts=%lld,in_pkt.dts=%lld,in_pkt.duration=%lld\n", in_pkt.pts, in_pkt.dts, in_pkt.duration);
                }
            }
            AVStream* in_stream = ifmt_ctx->streams[in_pkt.stream_index];
            AVStream* out_stream = ofmt_ctx_mp4->streams[in_pkt.stream_index];

            if ((video_index == in_pkt.stream_index) && (AV_CODEC_ID_AAC == in_stream->codecpar->codec_id)) {
                LOG("ERROR", "stream:%d, codec_id:%d\n", in_pkt.stream_index, in_stream->codecpar->codec_id);
            }
            if (in_pkt.stream_index == audio_index) { // 音频
                nRet = av_interleaved_write_frame(ofmt_ctx_mp4, &in_pkt);
                if (nRet < 0) {
                    write_frame_fail++;
                    LOG("ERROR", "write frame failed [ret:%d,count:%d]\n", nRet, write_frame_fail);
                    av_packet_unref(&in_pkt);
                    if (write_frame_fail > 25)
                        break;
                    else continue;
                }
                write_frame_fail = 0;
            }
            else { // video
                if (AV_CODEC_ID_H264 == in_vstream->codecpar->codec_id) {
                    //AVStream* in_stream = ifmt_ctx->streams[in_pkt.stream_index];
                    //AVStream* out_stream = ofmt_ctx_mp4->streams[in_pkt.stream_index];
                    //// Convert PTS/DTS
                    //in_pkt.pts = av_rescale_q_rnd(in_pkt.pts, in_stream->time_base, out_stream->time_base, (AVRounding)(AV_ROUND_NEAR_INF | AV_ROUND_PASS_MINMAX));
                    //in_pkt.dts = av_rescale_q_rnd(in_pkt.dts, in_stream->time_base, out_stream->time_base, (AVRounding)(AV_ROUND_NEAR_INF | AV_ROUND_PASS_MINMAX));
                    //in_pkt.duration = av_rescale_q(in_pkt.duration, in_stream->time_base, out_stream->time_base);
                    //in_pkt.pos = -1;
                    //printf("H264 pts=%lld,dts=%lld,duration=%lld,frame_rate=%0.2f\n", in_pkt.pts, in_pkt.dts, in_pkt.duration, avg_frame_rate);
                    nRet = av_interleaved_write_frame(ofmt_ctx_mp4, &in_pkt);
                    if (nRet < 0) {
                        write_frame_fail++;
                        LOG("ERROR", "av_interleaved_write_frame function failed [ret:%d,count:%d]\n", nRet, write_frame_fail);
                        if (write_frame_fail > 25) {
                            av_packet_unref(&in_pkt);
                            goto end;
                        }
                        else continue;
                    }
                    write_frame_fail = 0;
                }
                else if (AV_CODEC_ID_H265 == in_vstream->codecpar->codec_id) {
                    // H265解码 (H265 -> YUV420p)
                    nRet = avcodec_send_packet(video_h265_codec_ctx, &in_pkt);
                    if (nRet < 0) {
                        av_packet_unref(&in_pkt);
                        LOG("ERROR", "avcodec_send_packet function failed %d\n", nRet);
                        break;
                    }
                    int getframe_ret = 0;
                    while (getframe_ret >= 0) {
                        // H265解码 (H265 -> YUV420p)
                        getframe_ret = avcodec_receive_frame(video_h265_codec_ctx, frame);
                        if ((AVERROR(EAGAIN) == getframe_ret) || (AVERROR_EOF == getframe_ret)) {
                            //printf("#\n");
                            break;
                        }
                        else if (getframe_ret < 0) {
                            LOG("ERROR", "avcodec_receive_frame function failed %d\n", getframe_ret);
                            nRet = getframe_ret;
                            //break;
                            goto end;
                        }
                        //if (first_frame_video && (frame->pts == AV_NOPTS_VALUE)) {
                        if (first_frame_video) {
                            first_frame_video = false;
                            frame->pts = 0;
                            frame->pkt_dts = frame->pts;
                            printf("frame->pts=%lld,frame->pkt_dts=%lld,frame->pkt_duration=%lld\n", frame->pts, frame->pkt_dts, frame->pkt_duration);
                        }
                        else {
                            //frame->pts = in_pkt.pts;
                            //frame->pkt_dts = in_pkt.dts;
                            //frame->pkt_duration = in_pkt.duration;
                            //printf("frame->pts=%lld,frame->pkt_dts=%lld,frame->pkt_duration=%lld\n", frame->pts, frame->pkt_dts, frame->pkt_duration);
                        }
                        // H264编码 (YUV420p -> H264)
                        nRet = avcodec_send_frame(video_h264_codec_ctx, frame);
                        if (nRet < 0) {
                            char err_msg[128] = { 0 };
                            av_strerror(nRet, err_msg, sizeof(err_msg));
                            LOG("ERROR", "avcodec_send_frame function failed [ret:%d,msg:'%s']\n", nRet, err_msg);
                            goto end;
                        }
                        int getpacket_ret = 0;
                        while (getpacket_ret >= 0) {
                            // H264编码 (YUV420p -> H264)
                            getpacket_ret = avcodec_receive_packet(video_h264_codec_ctx, &out_pkt);
                            if ((AVERROR(EAGAIN) == getpacket_ret) || (AVERROR_EOF == getpacket_ret)) {
                                //char err_msg[128] = { 0 };
                                //av_strerror(getpacket_ret, err_msg, sizeof(err_msg));
                                //LOG("ERROR", "avcodec_receive_packet function failed [ret:%d,msg:'%s']\n", getpacket_ret, err_msg);
                                av_packet_unref(&out_pkt);
                                break;
                            }
                            else if (getpacket_ret < 0) {
                                LOG("ERROR", "avcodec_receive_packet function failed %d\n", getpacket_ret);
                                break;
                            }
                            // 更新 pts,dts
                            out_pkt.pts = av_rescale_q_rnd(out_pkt.pts, ifmt_ctx->streams[video_index]->time_base, video_stream->time_base, AV_ROUND_NEAR_INF);
                            out_pkt.dts = av_rescale_q_rnd(out_pkt.dts, ifmt_ctx->streams[video_index]->time_base, video_stream->time_base, AV_ROUND_NEAR_INF);
                            //out_pkt.duration = av_rescale_q(out_pkt.duration, ifmt_ctx->streams[video_index]->time_base, video_stream->time_base);
                            out_pkt.duration = av_rescale_q(in_pkt.duration, ifmt_ctx->streams[video_index]->time_base, video_stream->time_base);
                            out_pkt.pos = -1;
                            //printf("H265 pts=%lld,dts=%lld,duration=%lld,frame_rate=%0.2f,in_pkt.pts=%lld,in_pkt.dts=%lld,in_pkt.duration=%lld\n", out_pkt.pts, out_pkt.dts, out_pkt.duration, avg_frame_rate, in_pkt.pts, in_pkt.dts, in_pkt.duration);

                            nRet = av_interleaved_write_frame(ofmt_ctx_mp4, &out_pkt);
                            if (nRet < 0) {
                                write_frame_fail++;
                                LOG("ERROR", "av_interleaved_write_frame function failed [ret:%d,count:%d]\n", nRet, write_frame_fail);
                                if (write_frame_fail > 25) {
                                    av_packet_unref(&out_pkt);
                                    goto end;
                                }
                                else continue;
                            }
                            write_frame_fail = 0;
                            av_packet_unref(&out_pkt);
                        }
                    }
                } // AV_CODEC_ID_H265
                else {
                    LOG("WARN", "Unknown codec_id %d \n", in_vstream->codecpar->codec_id);
                }
            }
            av_packet_unref(&in_pkt);
        } // while


        av_frame_free(&frame);
        int ret = av_write_trailer(ofmt_ctx_mp4);
        if (ret < 0) {
            char err_msg[128] = { 0 };
            av_strerror(ret, err_msg, sizeof(err_msg));
            LOG("ERROR", "av_write_trailer function failed [ret:%d,msg:'%s']\n", ret, err_msg);
        }
    } while (false);

end:
    if (is_playback) {
        download_thd.Stop();
        data_buffer.Destory();
        if (NULL != g_file_ctx.fuzz) {
            delete[] g_file_ctx.fuzz;
            g_file_ctx.fuzz = NULL;
        }
    }
    if (NULL != params->write_packet) {
        if (cache_buffer.write_size > 0)
            params->write_packet(params->out_opaque, cache_buffer.buffer, cache_buffer.write_size);
        params->write_packet(params->out_opaque, NULL, 0);
    }

    if (NULL != video_h264_params) {
        avcodec_parameters_free(&video_h264_params);
    }
    if (NULL != ofmt_ctx_mp4) {
        if (ofmt_ctx_mp4->flags & AVFMT_FLAG_CUSTOM_IO) {	// 自定义输出流
            if (NULL != ofmt_ctx_mp4->pb) {
                av_freep(&ofmt_ctx_mp4->pb->buffer);
                avio_context_free(&ofmt_ctx_mp4->pb);
            }
        }
        else {
            if (!(ofmt_ctx_mp4->oformat->flags & AVFMT_NOFILE)) {
                if (NULL != ofmt_ctx_mp4->pb)
                    avio_close(ofmt_ctx_mp4->pb);
            }
        }
        avformat_free_context(ofmt_ctx_mp4);
        ofmt_ctx_mp4 = NULL;
    }
    if (NULL != ifmt_ctx) {
        if (ifmt_ctx->flags & AVFMT_FLAG_CUSTOM_IO) { // 自定义输入流
            if (NULL != ifmt_ctx->pb) {
                av_freep(&ifmt_ctx->pb->buffer);
                avio_context_free(&ifmt_ctx->pb);
            }
        }
        avformat_close_input(&ifmt_ctx);
        ifmt_ctx = NULL;
    }
    // reconnect
    if ((!is_playback) && ('r' == *(control_cmd + 1)) && (FILE_EOF != nRet)) { // 点播重连
        
        int64_t timeout = 1000000 * 60 * 5;  // 重连超时时间(5分钟)
        int ret = 0, reconnect_cnt = 0;
        while ((timeout > 0) && ('r' == *(control_cmd + 1))) {
            printf("------ reconnect %d ...\n", ++reconnect_cnt);
            AVDictionary* opts = NULL;
            av_dict_set(&opts, "rtsp_transport", "tcp", 0);
            av_dict_set(&opts, "stimeout", "3000000", 0);
            ret = avformat_open_input(&ifmt_ctx, params->in_url, 0, &opts);
            av_dict_free(&opts);
            opts = NULL;
            nRet = ret;
            if (ret >= 0) {
                reconnet = true;
                goto start;
            }
            else {
                char err_msg[128] = { 0 };
                av_strerror(nRet, err_msg, sizeof(err_msg));
                LOG("ERROR", "avformat_open_input function failed [ret:%d,msg:'%s']\n", ret, err_msg);
            }
            int n = 5;
            while ((n > 0) && ('r' == *(control_cmd + 1))) {
                av_usleep(1000000); // 1秒
                n--;
                timeout -= 1000000;
            }
            if ('r' != *(control_cmd + 1))
                break;
        }
        printf("------ reconnect timeout [timeout:%lld, control_cmd:'%c']\n", timeout, *(control_cmd + 1));
    }
    printf("------ thread exit. --------- [ret:%d,control_cmd:'%c',chl:'%s',url:'%s'] ------\n", nRet, *(control_cmd + 1), params->chl, params->in_url);
    return nRet;
}

int64_t media_seek_packet(void* opaque, int64_t offset, int whence)
{
    if (whence == SEEK_CUR) {
        printf("SEEK_CUR\n");
        if (offset > INT64_MAX - g_file_ctx.pos)
            return -1;
        offset += g_file_ctx.pos;
    }
    else if (whence == SEEK_END) {
        printf("SEEK_END\n");
        if (offset > INT64_MAX - g_file_ctx.filesize)
            return -1;
        offset += g_file_ctx.filesize;
    }
    else if (whence == AVSEEK_SIZE) {
        //printf("%d  whence=%d,   offset=%lld\n", i++, whence, offset);
        return g_file_ctx.filesize;
    }
    if (offset < 0 || offset > g_file_ctx.filesize)
        return -1;
    static int64_t i = 1;
    if (i == 1)
        printf("%d  whence=%d,   offset=%lld->%lld (%lld), (tail:%lld)\n", i++, whence, g_file_ctx.pos, offset, ((offset - g_file_ctx.pos)), (g_file_ctx.filesize - offset));
    else
        printf("%d  whence=%d,   offset=%lld->%lld (%0.2fk)\n", i++, whence, g_file_ctx.pos, offset, (double)(((double)(offset - g_file_ctx.pos)) / 1024.0));
    //io->fuzz += offset - io->pos;
    //io->fuzz_size -= offset - io->pos;
    g_file_ctx.pos = offset;
    return 0;
}

int media_read_packet(void* opaque, uint8_t* buf, int buf_size)
{
    if (buf_size > BUFF_SIZE) {
        LOG("ERROR", "buf_size:%d, BUFF_SIZE:%d\n", buf_size, BUFF_SIZE);
        buf_size = BUFF_SIZE;
    }
    if (g_file_ctx.enable) {
        if (g_file_ctx.fuzz_size <= 0) {
            int64_t end = g_file_ctx.pos + buf_size;
            if (end > g_file_ctx.filesize)
                end = g_file_ctx.filesize;
            if (g_file_ctx.pos < end)
                g_file_ctx.fuzz_size = g_file_io->download(g_file_ctx.remote_name, g_file_ctx.pos, end - 1, media_minio_callback, &g_file_ctx);
        }
        int size = FFMIN(buf_size, g_file_ctx.fuzz_size);
        if (!g_file_ctx.fuzz_size) {
            printf("read packet EOF [fuzz_size:%lld,pos:%lld,filesize:%lld]\n", g_file_ctx.fuzz_size, g_file_ctx.pos, g_file_ctx.filesize);
            g_file_ctx.filesize = FFMIN(g_file_ctx.pos, g_file_ctx.filesize);
            return AVERROR_EOF;
    }
        if (g_file_ctx.pos > INT64_MAX - size) {
            printf("read packet EIO [pos:%lld,size:%lld]\n", g_file_ctx.pos, size);
            return AVERROR(EIO);
        }
        static int i = 1;
        //printf("%d read packet %d bytes [buf_size:%d,pos:%lld->%lld(%d)]\n", i++, size, buf_size, io->pos, (io->pos + size), size);
        memcpy(buf, g_file_ctx.fuzz, size);
        //io->fuzz += size;
        g_file_ctx.fuzz_size -= size;
        g_file_ctx.pos += size;
        //io->filesize = FFMAX(io->filesize, io->pos);
        return size;
}
    else {
        CBuffer<char>* buffer = (CBuffer<char>*)opaque;
        if (buffer->GetDataNodeCount() <= 0) {
            if (buffer->m_flag) {
                printf("lock ...\n");
                unique_lock<mutex> lock(g_thd_mutex);
                g_thd_cv.wait(lock);
            }
            else return AVERROR_EOF;
        }
        int bytes = buffer->Read((char*)buf, buf_size);
        if (bytes > 0) {
            ;// buffer->m_io_ctx.pos = bytes;
        }
        else {
            LOG("WARN", "Read buffer %d bytes [DataNodeCount:%d]\n", bytes, buffer->GetDataNodeCount());
        }
        return bytes;
    }
}
int media_write_packet(void* opaque, unsigned char* buf, int buf_size)
{
#if 0
    stream_buffer_t* cache = (stream_buffer_t*)opaque;
    if (cache->pos + buf_size > cache->limit) {
        LOG("ERROR","Write cache buffer over limit [pos:%d,size:%d]\n", (cache->pos + buf_size), cache->buf_size);
        return -1;
    }
    if (cache->reconnet && cache->video_head) { // 断流重连后无需发送视频的头帧
        LOG("WARN", "reconnet=%d, video_head=%d\n", cache->reconnet, cache->video_head);
        return buf_size;
    }
    memcpy(cache->pos, buf, buf_size);
    cache->pos += buf_size;
    cache->write_size += buf_size;
#else
    printf("# write_packet   data:%p, size:%d,  time:%lld\n", buf, buf_size, time(NULL));
    //static FILE* fp = NULL;
    //if (NULL == fp) {
    //    const char* filename = (char*)opaque;
    //    fp = fopen(filename, "wb");
    //}
    //if (NULL != fp) {
    //    if ((NULL != buf) && (buf_size > 0)) {
    //        fwrite(buf, 1, buf_size, fp);
    //    }
    //    else {
    //        fclose(fp);
    //        fp = NULL;
    //    }
    //}
    //else LOG("ERROR", "File is not open %p\n", fp); 
#endif
    return buf_size;
}

void STDCALL media_download_thdproc(STATE& state, PAUSE& p, void* pUserData)
{
    printf("------ download start ...  [pos:%lld] ------\n\n", g_file_ctx.pos);
    g_file_ctx.enable = false;
    CBuffer<char>* buffer = (CBuffer<char>*)pUserData;
    int i = 0, count = 0;
    int upload_count = 9000000;// buffer->m_upload_count;
    while (TS_STOP != state) {
        if (TS_PAUSE == state)
            p.wait();
        if (buffer->GetDataNodeCount() <= upload_count) {
            int64_t end = g_file_ctx.pos + BUFF_SIZE;
            if (end > g_file_ctx.filesize)
                end = g_file_ctx.filesize;
            if (g_file_ctx.pos >= end) {
                LOG("WARN", "download completes %lld-%lld (%lld) [DataNodeCount:%d]\n", g_file_ctx.pos, end, (end - g_file_ctx.pos), buffer->GetDataNodeCount());
                break;
            }
            //printf("%d Start download %lld-%lld (%lld) [DataNodeCount:%d]\n",i++, g_file_ctx.pos, end - 1,(end-g_file_ctx.pos), buffer->GetDataNodeCount());
            int64_t bytes = g_file_io->download(g_file_ctx.remote_name, g_file_ctx.pos, end - 1, media_minio_callback, buffer);
            if (bytes > 0) {
                g_thd_cv.notify_all();
                int64_t pos = g_file_ctx.pos;
                g_file_ctx.pos += bytes;
                //printf("POS: %lld -> %lld\n", pos, g_file_ctx.pos);
                //buffer->m_upload_count--;
                //printf("download data %lld bytes [DataNodeCount:%d]\n", bytes, buffer->GetDataNodeCount());
            }
            else LOG("ERROR", "%d Download failed %lld-%lld (%lld) [DataNodeCount:%d]\n", i++, g_file_ctx.pos, end - 1, (end - g_file_ctx.pos), buffer->GetDataNodeCount());
        }
        else {
            //printf("download thread sleep[DataNodeCount:%d]\n", buffer->GetDataNodeCount());
            Sleep(1000);
        }
    }
    buffer->m_flag = false;
    printf("------ download end ------\n\n");
}
int STDCALL media_minio_callback(void* data, size_t size, void* arg)
{
    if (g_file_ctx.enable) {
        //IOContext* io = (IOContext*)arg;
        //printf("download data %lld bytes\n", size);
        memcpy(g_file_ctx.fuzz, data, size);
    }
    else {
        CBuffer<char>* buffer = (CBuffer<char>*)arg;
        buffer->Write((char*)data, size);
    }
    return 0;
}
int media_create_object_cbk(void*& obj, int& obj_size, void* user_data)
{
    try {
        obj = new char[BUFF_SIZE + 1];
        obj_size = BUFF_SIZE;
    }
    catch (const bad_alloc& e) {
        LOG("ERROR", "Allocation memory failed.\n");
        return -1;
    }
    return 0;
}
void media_destory_object_cbk(void*& obj, void* user_data)
{
    delete[] obj;
}

int media_video_to_image(stream_param_t* params)
{
    if (NULL == params) {
        LOG("ERROR", "Input parameter error, [params:%p]\n", params);
        return -1;
    }
    int nRet = -1;
    AVFormatContext* ifmt_ctx = NULL;
    AVFormatContext* ofmt_cxt = NULL;
    AVCodecContext* video_codec_ctx = NULL;       // 视频解码器上下文
    AVCodecContext* video_encoder_ctx = NULL;     // 视频编码上下文
    //av_register_all();
    avformat_network_init();
    //avdevice_register_all();	//注册设备
    //av_log_set_callback(custom_output);
    //av_log_set_level(AV_LOG_ERROR);
    char err_msg[64] = { 0 };
    do {
        const AVInputFormat* ifmt = NULL;//av_find_input_format(url);
        AVDictionary* opts = NULL;
        if (NULL != params->in_url) {
            av_dict_set(&opts, "buffer_size", "2048000", 0);
            av_dict_set(&opts, "stimeout", "3000000", 0);
            av_dict_set(&opts, "rtsp_transport", "tcp", 0);
            nRet = avformat_open_input(&ifmt_ctx, params->in_url, (AVInputFormat*)ifmt, &opts);
            av_dict_free(&opts);
        }
        else {
            ifmt_ctx = avformat_alloc_context();
            unsigned char* buffer = (unsigned char*)av_malloc(BUFF_SIZE);
            AVIOContext* avio_ctx = avio_alloc_context(buffer, BUFF_SIZE, 0, params->in_opaque, params->read_packet, NULL, params->seek_packet);
            if (NULL == avio_ctx) {
                av_free(buffer);
                av_dict_free(&opts);
                LOG("ERROR", "Allocation memory failed %p\n", avio_ctx);
                break;
            }
            ifmt_ctx->pb = avio_ctx;
            ifmt_ctx->flags |= AVFMT_FLAG_CUSTOM_IO;
            nRet = avformat_open_input(&ifmt_ctx, "", (AVInputFormat*)ifmt, &opts);
        }
        if (nRet < 0) {
            av_strerror(nRet, err_msg, sizeof(err_msg));
            LOG("ERROR", "avformat_open_input failed '%s' [ret:%d,msg:'%s']\n", params->in_url?params->in_url:"null", nRet, err_msg);
            break;
        }
        nRet = avformat_find_stream_info(ifmt_ctx, NULL);
        if (nRet < 0) {
            av_strerror(nRet, err_msg, sizeof(err_msg));
            LOG("ERROR", "avformat_find_stream_info failed [ret:%d,msg:'%s']\n", nRet, err_msg);
            break;
        }
        av_dump_format(ifmt_ctx, 0, params->in_url, 0);
        int audio_index = av_find_best_stream(ifmt_ctx, AVMEDIA_TYPE_AUDIO, -1, -1, NULL, 0);
        int video_index = av_find_best_stream(ifmt_ctx, AVMEDIA_TYPE_VIDEO, -1, -1, NULL, 0);
        if (-1 == video_index) {
            LOG("ERROR", "av_find_best_stream failed function failed.\n");
            break;
        }
        // 查找并打开解码器
        AVStream* in_vstream = ifmt_ctx->streams[video_index]; // 视频解码器参数
        const AVCodec* video_codec = avcodec_find_decoder(in_vstream->codecpar->codec_id);
        if (NULL == video_codec) {
            LOG("ERROR", "avcodec_find_decoder function failed.\n");
            break;
        }
        video_codec_ctx = avcodec_alloc_context3(video_codec);
        nRet = avcodec_parameters_to_context(video_codec_ctx, in_vstream->codecpar);
        if (nRet < 0) {
            av_strerror(nRet, err_msg, sizeof(err_msg));
            LOG("ERROR", "avcodec_parameters_to_context failed [ret:%d,msg:'%s']\n", nRet, err_msg);
            break;
        }
        nRet = avcodec_open2(video_codec_ctx, video_codec, NULL);     // 打开解码器
        if (nRet < 0) {
            av_strerror(nRet, err_msg, sizeof(err_msg));
            LOG("ERROR", "avcodec_open2 failed [ret:%d,msg:'%s']\n", nRet, err_msg);
            break;
        }
        ////////////////////////////////////////////////////////////////////////////
        // Output
        //nRet = avformat_alloc_output_context2(&out_fmt_cxt, NULL, "singlejpeg", params->out_url);
        nRet = avformat_alloc_output_context2(&ofmt_cxt, NULL, "mjpeg", params->out_url);
        if (nRet < 0) {
            av_strerror(nRet, err_msg, sizeof(err_msg));
            LOG("ERROR", "avformat_alloc_output_context2 failed [ret:%d,msg:'%s']\n", nRet, err_msg);
            break;
        }
        if (NULL != params->out_url) {
            if (!(ofmt_cxt->oformat->flags & AVFMT_NOFILE)) {
                nRet = avio_open2(&ofmt_cxt->pb, params->out_url, AVIO_FLAG_READ_WRITE, NULL, NULL);
                if (nRet < 0) {
                    av_strerror(nRet, err_msg, sizeof(err_msg));
                    LOG("ERROR", "avio_open2 function failed [ret:%d,msg:'%s']\n", nRet, err_msg);
                    break;
                }
            }
        }
        else {
            unsigned char* buffer = (unsigned char*)av_malloc(BUFF_SIZE);
            if (NULL == buffer) {
                LOG("ERROR", "av_malloc function failed [size:%d]\n", BUFF_SIZE);
                break;
            }
            AVIOContext* avio_out = avio_alloc_context(buffer, BUFF_SIZE, 1, params->out_opaque, NULL, params->write_packet, NULL);
            if (NULL == avio_out) {
                LOG("ERROR", "avio_alloc_context function failed.\n");
                av_free(buffer);
                break;
            }
            ofmt_cxt->pb = avio_out;
            ofmt_cxt->flags = AVFMT_FLAG_CUSTOM_IO;
        }
        // new 一个视频流
        AVStream* out_vstream = avformat_new_stream(ofmt_cxt, video_codec);
        if (NULL == out_vstream) {
            LOG("ERROR", "avformat_new_stream failed %p\n", out_vstream);
            break;
        }
        out_vstream->codecpar->codec_id = ofmt_cxt->oformat->video_codec;
        out_vstream->codecpar->codec_type = AVMEDIA_TYPE_VIDEO;
        out_vstream->codecpar->format = AV_PIX_FMT_YUVJ420P;
        out_vstream->codecpar->height = in_vstream->codecpar->height;
        out_vstream->codecpar->width = in_vstream->codecpar->width;
        out_vstream->time_base.num = 1;
        out_vstream->time_base.den = 25;

        av_dump_format(ofmt_cxt, 0, params->out_url, 1);

        const AVCodec* video_encoder = avcodec_find_encoder(out_vstream->codecpar->codec_id);
        if (NULL == video_encoder) {
            LOG("ERROR", "avcodec_find_encoder fcunction failed\n");
            break;
        }
        video_encoder_ctx = avcodec_alloc_context3(video_encoder);
        if (NULL == video_encoder_ctx) {
            LOG("ERROR", "avcodec_alloc_context3 failed.\n");
            break;
        }
        avcodec_parameters_to_context(video_encoder_ctx, out_vstream->codecpar);
        video_encoder_ctx->time_base.num = 1;
        video_encoder_ctx->time_base.den = 25;
        nRet = avcodec_open2(video_encoder_ctx, video_encoder, NULL);
        if (nRet < 0) {
            av_strerror(nRet, err_msg, sizeof(err_msg));
            LOG("ERROR", "avcodec_open2 function failed [ret:%d,msg:'%s']\n", nRet, err_msg);
            break;
        }
        nRet = avformat_write_header(ofmt_cxt, NULL);       // 1.写入文件头
        if (nRet < 0) {
            av_strerror(nRet, err_msg, sizeof(err_msg));
            LOG("ERROR", "avformat_write_header function failed [ret:%d,msg:'%s']\n", nRet, err_msg);
            break;
        }
        AVPacket out_pkt;
        av_init_packet(&out_pkt);
        /// //////////////////////////////////////////////////////////////////////////

        AVPacket in_pkt;                    // 原始数据包
        av_init_packet(&in_pkt);
        AVFrame* frame = av_frame_alloc();  // 解码后的视频帧
        int read_frame_fail = 0, write_frame_fail = 0;
        bool first_video = true, first_audio = true;
        while (true) {
            nRet = av_read_frame(ifmt_ctx, &in_pkt);
            if (nRet < 0) {
                av_packet_unref(&in_pkt);
                av_strerror(nRet, err_msg, sizeof(err_msg));
                LOG("ERROR", "read frame failed [ret:%d,msg:'%s',count:%d]\n", nRet, err_msg, read_frame_fail);
                if (nRet == AVERROR_EOF) {
                    nRet = FILE_EOF;
                    break;
                }
                else {
                    read_frame_fail++;
                    if (read_frame_fail > 25) { // read frame failed
                        nRet = ERROR_READ_FRAME_FAIL;
                        break;
                    }
                }
                continue;
            }
            read_frame_fail = 0;
            // 当视频流并且为I帧时方可往下执行
            if ((in_pkt.stream_index != video_index) || ((in_pkt.stream_index != video_index) && !(in_pkt.flags & AV_PKT_FLAG_KEY))) {
                av_packet_unref(&in_pkt);
                continue;
            }
            nRet = avcodec_send_packet(video_codec_ctx, &in_pkt); // 解码
            av_packet_unref(&in_pkt);
            if (nRet < 0) {
                av_strerror(nRet, err_msg, sizeof(err_msg));
                LOG("ERROR", "avcodec_send_packet function failed [ret:%d,msg:'%s']\n", nRet, err_msg);
                continue;
            }
            while (nRet >= 0) {
                nRet = avcodec_receive_frame(video_codec_ctx, frame);
                if ((AVERROR(EAGAIN) == nRet) || (AVERROR_EOF == nRet)) {
                    break;
                }
                else if (nRet < 0) {
                    av_strerror(nRet, err_msg, sizeof(err_msg));
                    LOG("ERROR", "avcodec_receive_frame function failed [ret:%d,msg:'%s']\n", nRet, err_msg);
                    goto end;
                }
                nRet = avcodec_send_frame(video_encoder_ctx, frame);
                if (nRet < 0) {
                    av_strerror(nRet, err_msg, sizeof(err_msg));
                    LOG("ERROR", "avcodec_send_frame function failed [ret:%d,msg:'%s']\n", nRet, err_msg);
                    break;
                }
                while (nRet >= 0) {
                    nRet = avcodec_receive_packet(video_encoder_ctx, &out_pkt);
                    if ((nRet == AVERROR(EAGAIN)) || (nRet == AVERROR_EOF)) {
                        nRet = 0;
                        break;
                    }
                    else if (nRet < 0) {
                        av_strerror(nRet, err_msg, sizeof(err_msg));
                        LOG("ERROR", "avcodec_receive_packet function failed [ret:%d,msg:'%s']\n", nRet, err_msg);
                        goto end;
                    }
                    nRet = av_interleaved_write_frame(ofmt_cxt, &out_pkt);
                    av_packet_unref(&out_pkt);
                    if (nRet < 0) {
                        write_frame_fail++;
                        av_strerror(nRet, err_msg, sizeof(err_msg));
                        LOG("ERROR", "av_interleaved_write_frame function failed [ret:%d,count:%d,msg:'%s']\n", nRet, write_frame_fail,err_msg);
                        if (write_frame_fail > 25)
                            goto end;
                    }
                    else {
                        goto end;
                    }
                }//while()
            }//while()
            av_frame_unref(frame);
            //av_packet_unref(&in_pkt);
        } //while(true)
        if (NULL != frame)
            av_frame_free(&frame);
    } while (false);

end:
    if (NULL != ofmt_cxt) {
        nRet = av_write_trailer(ofmt_cxt);  // 3.写入流尾至文件
        if (nRet < 0) {
            av_strerror(nRet, err_msg, sizeof(err_msg));
            LOG("ERROR", "av_write_trailer function failed [ret:%d,msg:'%s']\n", nRet, err_msg);
        }
        if (ofmt_cxt->flags & AVFMT_FLAG_CUSTOM_IO) {	// 自定义输出流
            if (NULL != ofmt_cxt->pb) {
                av_freep(&ofmt_cxt->pb->buffer);
                avio_context_free(&ofmt_cxt->pb);
            }
        }
        else {
            if (!(ofmt_cxt->oformat->flags & AVFMT_NOFILE)) {
                if (NULL != ofmt_cxt->pb)
                    avio_close(ofmt_cxt->pb);
            }
        }
        avformat_free_context(ofmt_cxt);
        ofmt_cxt = NULL;
    }
    if (NULL != ifmt_ctx) {
        if (ifmt_ctx->flags & AVFMT_FLAG_CUSTOM_IO) { // 自定义输入流
            if (NULL != ifmt_ctx->pb) {
                av_freep(&ifmt_ctx->pb->buffer);
                avio_context_free(&ifmt_ctx->pb);
            }
        }
        avformat_close_input(&ifmt_ctx);
        ifmt_ctx = NULL;
    }
    if (NULL != video_encoder_ctx) {
        avcodec_close(video_encoder_ctx);
        avcodec_free_context(&video_encoder_ctx);
    }
    if (NULL != video_codec_ctx) {
        avcodec_close(video_codec_ctx);
        avcodec_free_context(&video_codec_ctx);
    }
    return nRet;
}
int image_write_packet(void* opaque, uint8_t* buf, int buf_size)
{
    printf("output  data:%p,  size:%d\n", buf, buf_size);
#if 1
    const char* remote_obj_name = (char*)opaque;
    if ((buf_size > 0) && (NULL != remote_obj_name) && (NULL != g_file_io)) {
        g_file_io->upload((char*)buf, buf_size, remote_obj_name);
    }
    else LOG("ERROR", "buf=%p,buf_size=%d,remote_name=%p,g_file_io=%p\n", buf, buf_size, remote_obj_name, g_file_io);
#else
    static FILE* fp = NULL;
    if (NULL == fp) {
        const char* filename = (char*)opaque;
        fp = fopen(filename, "wb");
    }
    if (NULL != fp) {
        if ((NULL != buf) && (buf_size > 0)) {
            fwrite(buf, 1, buf_size, fp);
        }
        else {
            fclose(fp);
            fp = NULL;
        }
    }
    else LOG("ERROR","File is not open %p\n", fp);
#endif
    return buf_size;
}

int media_interrupt_cbk(void* opaque)
{
    static int64_t timeout = 3 * 1000 * 1000;
    int64_t* read_pkt_time = (int64_t*)opaque;
    if (NULL != read_pkt_time) {
        if ((av_gettime() - *read_pkt_time) >= timeout) {
            printf("timeout:%lld\n", (av_gettime() - *read_pkt_time));
            return -1;
        }
    }
    //printf("interrupt_cbk !\n");
    return 0;
}

