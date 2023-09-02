
#include <stdio.h>
#include<string.h>
#include "media.h"
//#include "Thread11.h"
#ifdef _WIN32
//Windows
extern "C"
{
#include "libavformat/avformat.h"
#include  "libavformat/avio.h"
#include  "libavcodec/avcodec.h"
#include  "libavutil/avutil.h"
#include  "libavutil/time.h"
#include  "libavutil/log.h"
#include  "libavutil/mathematics.h"
};
//#include <winsock2.h>
#include <Windows.h>
#define GetMessage GetMessage
#define Sleep(n) Sleep(n)

#else
//Linux...
#ifdef __cplusplus
extern "C"
{
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
#include <sys/socket.h>
#include <arpa/inet.h>
#include<unistd.h>		// close
#define Sleep(n) usleep((n)*1000)
#endif

#define MAX_READ_FRAME_FAIL 5
#define MAX_WRITE_FRAME_FAIL 5
#define RECONNECT_TIME 30

char g_log_path[256] = {0};

///////// MinIO //////////////////////////////////////////////////////
#include "log.h"
#include "buffer.h"
#ifdef _WIN32
    #ifdef _WINDOWS_
    #define GetObject GetObject
    #define GetMessage GetMessage
    #endif
#endif
CMinIO* g_minio = NULL;
/////////// 录像回放,异步线程下载 //////////////////////////////////
#include <condition_variable>
#include<mutex>
std::mutex g_mutex;				// 互斥锁.
std::condition_variable g_cv;	// 条件变量
IOContext g_io;
////////////////////////////////////////////////////////////////////

#ifdef _WIN32
#pragma comment(lib, "ws2_32.lib")
#endif
//write log function
void custom_output(void* ptr, int level, const char* fmt,va_list vl)
{
    if( level > av_log_get_level())
        return;
    static char ffmpeg_log[256] = { 0 };
    sprintf(ffmpeg_log, "%s/media_common.log", g_log_path);
    FILE *fp = fopen(ffmpeg_log,"a+");
    if (NULL != fp) {
        time_t curr_time = time(NULL);
        char* time_str = ctime(&curr_time);
        char* end_str = time_str + strlen(time_str) - 1;
        if (*end_str == '\n')
            *end_str = '\0';
        fprintf(fp, "%s:", time_str);
        vfprintf(fp, fmt, vl);
        fflush(fp);
        fclose(fp);
    }
    else LOG("ERROR","Open log file failed '%s'\n", ffmpeg_log);
}

int write_packet(void *opaque, unsigned char *buf, int buf_size)
{
    stream_cache_buff* cache_write=(stream_cache_buff*)opaque;
    if(cache_write->pos+buf_size>cache_write->limit){
        LOG("ERROR", "cache buff over limit [buf_size:%d]\n", buf_size);
        return -1;
    }
    if (cache_write->reconnet && cache_write->video_head) { // 断流重连后无需发送视频的头帧
        av_log(NULL, AV_LOG_ERROR, "reconnet=%d, video_head=%d\n", cache_write->reconnet, cache_write->video_head);
        return buf_size;
    }
    memcpy(cache_write->pos,buf,buf_size);
    cache_write->pos+=buf_size;
    cache_write->write_size+=buf_size;
    return buf_size;
}
/*
send rtsp req to ipc,recv video stream and audio stream and send to buff
rtsp_url rtsp url
fun_arg write funtion arg
writedata_dealfun write funtion pointer
*/
int rtsp_stream(const char * rtsp_url, void* fun_arg,write_stream writedata_dealfun,char* control_cmd)
{
    int ret,fail_read_frame=0,fail_write_frame=0;
    bool first_video=false,first_audio=false,first_lgnite=false,first_stream_type=false;
    unsigned int index,video_index=-1,audio_index=-1;
    AVPacket pkt;
    AVDictionary *opts = NULL;
    AVFormatContext *ifmt_ctx = NULL,*ofmt_ctx_mp4=NULL;
    media_error_code return_error=NO_ERRORS;
    int buffsize=32768;
    unsigned char* outbuffer=NULL;
    AVIOContext *avio_out=NULL;
    stream_cache_buff  write_cache_buff;
    int64_t last_cache_pts=0,now_pts=0;
    bool isSend=false;
    char lastcmd='r';

    if(rtsp_url==NULL||fun_arg==NULL||writedata_dealfun==NULL||control_cmd==NULL)
        return ERROR_ARG;

    memset(&write_cache_buff,0,sizeof (write_cache_buff));
    //set log
    av_log_set_callback(custom_output);
    av_log_set_level(AV_LOG_ERROR);

    //set rtsp
    AVDictionary* options = NULL;
    av_dict_set(&options, "buffer_size", "2048000", 0);
    av_dict_set(&options,"rtsp_transport", "tcp", 0);
    av_dict_set(&options,"stimeout","3000000",0);
    //Input
    if ((ret = avformat_open_input(&ifmt_ctx,rtsp_url, 0, &options)) < 0) {
        av_log(ifmt_ctx,AV_LOG_ERROR, "ERROR,Could not open rtsp url %s,error is %d\n",rtsp_url,ret);
        return_error=ERROR_OPEN_URL_FAIL;
        goto end;
    }
    av_dict_free(&options);
    options=NULL;
openout:
    fail_read_frame=0;
    fail_write_frame=0;
    first_video=false;
    first_audio=false;
    first_lgnite=false;
    //find stream information in input
    if ((ret = avformat_find_stream_info(ifmt_ctx, 0)) < 0) {
        av_log(ifmt_ctx,AV_LOG_ERROR,"ERROR Failed to retrieve input %s stream information,error is %d  \n",rtsp_url,ret);
        return_error=ERROR_STREAM_INFO_FAIL;
        goto end;
    }

    //pritnf input stream format
    av_dump_format(ifmt_ctx, 0, rtsp_url, 0);

    //Output
    avformat_alloc_output_context2(&ofmt_ctx_mp4, NULL, "mp4", NULL);
    if (!ofmt_ctx_mp4) {
        av_log(ifmt_ctx,AV_LOG_ERROR,"ERROR create  output fail\n");
        return_error = ERROR_OPEN_OUTSTREAM_FAIL;
        goto end;
    }

    // set write to buffer
    outbuffer=(uint8_t*)av_malloc(buffsize);
    if(outbuffer==NULL)
    {
        av_log(ifmt_ctx,AV_LOG_ERROR,"ERROR Failed malloc buffer in %s \n",rtsp_url);
        return_error=ERROR_MEM_FAIL;
        goto end;

    }
    //threearg must be 1 for write
    // avio_out =avio_alloc_context(outbuffer, buffsize,1,fun_arg,NULL,writedata_dealfun,NULL);

    write_cache_buff.cache_buff=(uint8_t*)av_malloc(CACHE_BIT_MAZ_WIDTH*CACHE_TIME_SECOND);
    write_cache_buff.pos=write_cache_buff.cache_buff;
    write_cache_buff.limit=write_cache_buff.cache_buff+CACHE_BIT_MAZ_WIDTH*CACHE_TIME_SECOND;
    write_cache_buff.write_size=0;
    avio_out =avio_alloc_context(outbuffer, buffsize,1,&write_cache_buff,NULL,write_packet,NULL);
    if(avio_out==NULL)
    {
        av_log(ifmt_ctx,AV_LOG_ERROR,"ERROR Failed malloc AVIOContext in %s \n",rtsp_url);
        av_free(outbuffer);
        return_error=ERROR_IOCONTENT_FAIL;
        goto end;
    }
    ofmt_ctx_mp4->pb=avio_out;
    ofmt_ctx_mp4->flags=AVFMT_FLAG_CUSTOM_IO;
    //create streams in output
    for (index = 0; index < ifmt_ctx->nb_streams; index++) {
        AVStream *in_stream = ifmt_ctx->streams[index];
        if(in_stream->codecpar->codec_type==AVMEDIA_TYPE_VIDEO)
            video_index=index;
        else if(in_stream->codecpar->codec_type==AVMEDIA_TYPE_AUDIO)
            audio_index=index;
        else {
            continue;
        }
        AVCodec* codec= avcodec_find_decoder(in_stream->codecpar->codec_id);
        AVStream *out_stream = avformat_new_stream(ofmt_ctx_mp4,codec);

        if (!out_stream) {
            av_log(ifmt_ctx,AV_LOG_ERROR,"ERROR Fail to create out stream in %s\n",rtsp_url);
            return_error = ERROR_CREATE_AVSTREAM_FAIL;
            goto end;
        }

        //Copy the settings of AVCodecContext
        AVCodecContext* pCodecCtx = avcodec_alloc_context3(codec);
        ret = avcodec_parameters_to_context(pCodecCtx, in_stream->codecpar);
        if(ret<0){
            av_log(ifmt_ctx,AV_LOG_ERROR,"ERROR Fail to copy out stream codec in %s\n",rtsp_url);
            return_error = ERROR_CREATE_AVSTREAM_FAIL;
            avcodec_free_context(&pCodecCtx);
            goto end;
        }
        pCodecCtx->codec_tag = 0;

        if (ofmt_ctx_mp4->oformat->flags & AVFMT_GLOBALHEADER) {
            pCodecCtx->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;
        }

        ret = avcodec_parameters_from_context(out_stream->codecpar, pCodecCtx);

        if (ret < 0) {
            av_log(ifmt_ctx,AV_LOG_ERROR,"ERROR Fail to copy out stream codec in %s\n",rtsp_url);
            return_error = ERROR_CREATE_AVSTREAM_FAIL;
            avcodec_free_context(&pCodecCtx);
            goto end;

        }
        avcodec_free_context(&pCodecCtx);
    }
    //print out stream format
    av_dump_format(ofmt_ctx_mp4, 0, 0, 1);

    //send video only or video and audio to client
    if(first_stream_type==false)
    {
        if(audio_index==-1)
            write_cache_buff.write_size=VIDEO_ONLY;
        else
            write_cache_buff.write_size=VIDEO_AND_AUDIO;
        ret=writedata_dealfun(fun_arg,write_cache_buff.cache_buff,write_cache_buff.write_size);
        write_cache_buff.pos=write_cache_buff.cache_buff;
        write_cache_buff.write_size=0;
        first_stream_type=true;
    }
    //fmp4
    //av_dict_set(&opts, "movflags", "frag_keyframe+empty_moov", 0);
    av_dict_set(&opts, "movflags", "frag_keyframe+empty_moov+omit_tfhd_offset+faststart+separate_moof+disable_chpl+default_base_moof+dash", 0);
    //Write file header
    if (avformat_write_header(ofmt_ctx_mp4, &opts) < 0) {
        printf( "Error occurred when write head to  output file\n");
        return_error = ERROR_OPEN_OUTSTREAM_FAIL;
        goto end;
    }
    av_dict_free(&opts);

    //ret=writedata_dealfun(fun_arg,write_cache_buff.cache_buff,write_cache_buff.write_size);
    //if(ret<0)
    //  av_log(ofmt_ctx_mp4,AV_LOG_ERROR,"ERROR Fail to send head to %s  cache \n",rtsp_url);
    //read an write
    while (*(control_cmd+1)=='r'||*(control_cmd+1)=='i') {
        if ((ret=av_read_frame(ifmt_ctx, &pkt))< 0){
            av_log(ifmt_ctx,AV_LOG_ERROR,"ERROR fail read a frame in %s,error is %d\n",rtsp_url,ret);
            av_packet_unref(&pkt);
            fail_read_frame++;
            if(fail_read_frame>MAX_READ_FRAME_FAIL)
            {
                return_error = ERROR_READ_FRAME_FAIL;
                av_log(ifmt_ctx,AV_LOG_ERROR,"ERROR fail read frame over max in %s\n",rtsp_url);
                break;
            }
            else
                continue;
        }
        else {
            fail_read_frame=0;
        }
        //not deal not video audio data
        if(pkt.stream_index!=video_index&&pkt.stream_index!=audio_index)
        {
            av_packet_unref(&pkt);
            continue;
        }
        // set first packet pts 0 for sometimes first dts is lager than others
        if(first_video==0&&pkt.stream_index==video_index)
        {
            pkt.dts=0;
            pkt.pts=0;
            first_video=true;
        }

        if(first_audio==0&&pkt.stream_index==audio_index)
        {
            pkt.dts=0;
            pkt.pts=0;
            first_audio=true;
        }

        //Write
        // printf("read a frame indexis %d %ld\n",pkt.stream_index,pkt.dts);
#if 0
        AVStream* in_stream  = ifmt_ctx->streams[pkt.stream_index];
        AVStream* out_stream = ofmt_ctx_mp4->streams[pkt.stream_index];
        // copy packet */
        pkt.pts = av_rescale_q_rnd(pkt.pts, in_stream->time_base, out_stream->time_base, (AVRounding)(AV_ROUND_NEAR_INF|AV_ROUND_PASS_MINMAX));
        pkt.dts = av_rescale_q_rnd(pkt.dts, in_stream->time_base, out_stream->time_base, (AVRounding)(AV_ROUND_NEAR_INF|AV_ROUND_PASS_MINMAX));
        pkt.duration = av_rescale_q(pkt.duration, in_stream->time_base, out_stream->time_base);
        pkt.pos = -1;
#endif

        //printf("Write %8d frames to output file\n",frame_index);
        //if cache data time lager than CACHE_TIME_SECOND,send cache data
        if(pkt.stream_index==video_index){
            //change mode only after send  packts
            if(isSend==true)
            {
                if(*(control_cmd+1)!=lastcmd)
                    lastcmd=*(control_cmd+1);
                isSend=false;
            }
            if(lastcmd=='i'&&!(pkt.flags&AV_PKT_FLAG_KEY))
            {
                av_packet_unref(&pkt);
                continue;
            }
            AVRational time_base=ifmt_ctx->streams[video_index]->time_base;
            AVRational time_base_q={1,AV_TIME_BASE};
            now_pts = av_rescale_q((pkt.pts-last_cache_pts), time_base, time_base_q);
            //printf("now_pts is %ld\n",now_pts);
            //add i frame to cache top
            //if(now_pts>=CACHE_TIME_SECOND*AV_TIME_BASE&&pkt.flags& AV_PKT_FLAG_KEY){
            if(pkt.flags& AV_PKT_FLAG_KEY){
                //if(now_pts>=CACHE_TIME_SECOND*AV_TIME_BASE){
                //mark lgnite start write
                if(first_lgnite==false)
                {
                    printf("mark start\n");
                    ret=writedata_dealfun(fun_arg,NULL,0);
                    if(ret<0)
                        av_log(ofmt_ctx_mp4,AV_LOG_ERROR,"ERROR Fail to mark  %s start write cache \n",rtsp_url);
                    first_lgnite=true;
                }
                // printf("write cache\n");
                ret=writedata_dealfun(fun_arg,write_cache_buff.cache_buff,write_cache_buff.write_size);
                if(ret<0)
                    av_log(ofmt_ctx_mp4,AV_LOG_ERROR,"ERROR Fail to send  %s  cache \n",rtsp_url);
                write_cache_buff.pos=write_cache_buff.cache_buff;
                write_cache_buff.write_size=0;
                last_cache_pts=pkt.pts;
                isSend=true;
            }
        }
        if ((ret=av_interleaved_write_frame(ofmt_ctx_mp4, &pkt))< 0) {
            av_log(ofmt_ctx_mp4,AV_LOG_ERROR,"ERROR Fail to write packet %s,error is %d\n",rtsp_url,ret);
            fail_write_frame++;
            if(fail_write_frame>MAX_WRITE_FRAME_FAIL)
            {
                av_packet_unref(&pkt);
                av_log(ofmt_ctx_mp4,AV_LOG_ERROR,"ERROR Fail write packet over max %s\n",rtsp_url);
                return_error = ERROR_WRITE_PKT_FAIL;
                goto end;
            }
        }else
            fail_write_frame=0;
        av_packet_unref(&pkt);
    }
    //Write file trailer
    av_write_trailer(ofmt_ctx_mp4);
end:
    avformat_close_input(&ifmt_ctx);
    avio_context_free(&avio_out);
    avformat_free_context(ofmt_ctx_mp4);
    if(write_cache_buff.cache_buff!=NULL)
        av_free(write_cache_buff.cache_buff);
#if 1
    if(return_error!=ERROR_OPEN_URL_FAIL&&return_error!=ERROR_READ_FRAME_FAIL||*(control_cmd+1)=='s')
    {
        return return_error;
    }
    //reconnect
    else {
        do{
            printf("wait reconnect\n");
            av_usleep(RECONNECT_TIME*AV_TIME_BASE);
            printf("open input\n");
            ifmt_ctx=NULL;
            av_dict_set(&options,"rtsp_transport", "tcp", 0);
            av_dict_set(&options,"stimeout","3000000",0);
            ret = avformat_open_input(&ifmt_ctx,rtsp_url, 0, &options);
            printf("ret is %d\n",ret);
            av_dict_free(&options);
            options=NULL;
            //read packet again
            if(ret>=0)
            {
                printf("go to open out\n");
                goto openout;

            }
            else {
                avformat_close_input(&ifmt_ctx);
            }
        }while(ret<0&&*(control_cmd+1)=='r');
    }
#endif
    return return_error;
}


/*
get stream from slice and tranfer a fmp4 file
slice_file slice file name
start_time play start time
fun_arg write funtion arg
writedata_dealfun write funtion pointer
*/
int slice_mp4(const char* slice_file,int start_time,void* fun_arg,write_stream writedata_dealfun,char* control_cmd,float* speed)
{
    int ret,video_index=-1,audio_index=-1,fail_read_frame=0,fail_write_frame=0;
    unsigned int index;
    AVFormatContext *ifmt_ctx = NULL,*ofmt_ctx_mp4=NULL;
    int64_t start_video_dts=0;
    AVDictionary *opts = NULL;
    AVPacket pkt;
    media_error_code return_error=NO_ERRORS;
    int buffsize=32768;
    unsigned char* outbuffer=NULL;
    AVIOContext *avio_out=NULL;
    int64_t play_time=0,play_pts=0,now_pts=0,now_time=0,last_cache_pts=0;
    bool first_video_packet=false,first_lgnite=false;
    stream_cache_buff  write_cache_buff;
    float playspeed=1.0;
    bool isSend=false,first_stream_type=false;
    char lastcmd='r';

    // play back need
    int64_t packet_interval=0,find_frame_time=0;
    int64_t last_added_video_pts=0,last_added_audio_pts=0;
    int64_t last_real_video_pts=0,last_real_audio_pts=0;
    bool has_send=false;

    if(slice_file==NULL||start_time<0||fun_arg==NULL||writedata_dealfun==NULL||control_cmd==NULL||speed==NULL)
        return ERROR_ARG;

    memset(&write_cache_buff,0,sizeof (write_cache_buff));
    //set log
    av_log_set_callback(custom_output);
    av_log_set_level(AV_LOG_ERROR);
    //Input open first slice
    if ((ret = avformat_open_input(&ifmt_ctx,slice_file,0, 0)) < 0) {
        av_log(ifmt_ctx,AV_LOG_ERROR,"ERROR Fail to open slice file %s\n",slice_file);
        return_error = ERROR_OPEN_FILE_FAIL;
        goto end;
    }
    //find stream information in input
    if ((ret = avformat_find_stream_info(ifmt_ctx, 0)) < 0) {
        av_log(ifmt_ctx,AV_LOG_ERROR,"ERROR Fail to get slice information %s\n",slice_file);
        return_error = ERROR_STREAM_INFO_FAIL;
        goto end;
    }
    for(index=0; index<ifmt_ctx->nb_streams; index++) {
        if(ifmt_ctx->streams[index]->codecpar->codec_type==AVMEDIA_TYPE_VIDEO){
            video_index=index;
        }else if(ifmt_ctx->streams[index]->codecpar->codec_type==AVMEDIA_TYPE_AUDIO){
            audio_index=index;
        }
    }
    av_dump_format(ifmt_ctx, 0,0, 0);

    if(video_index<0){
        av_log(ifmt_ctx,AV_LOG_ERROR,"ERROR no video stream in %s\n",slice_file);
        return_error = ERROR_NO_VIDEO_STREAM;
        goto end;
    }

    //compute start dts
    start_video_dts=ifmt_ctx->streams[video_index]->start_time+(int64_t) ( start_time/av_q2d(ifmt_ctx->streams[video_index]->time_base));
    ret = av_seek_frame(ifmt_ctx, video_index, start_video_dts, AVSEEK_FLAG_FRAME);
    if(ret<0){
        av_log(ifmt_ctx,AV_LOG_ERROR,"ERROR seek to %d in %s fail\n",start_time,slice_file);
        return_error = ERROR_SEEK_FAIL;
        goto end;
    }

    //Output
    avformat_alloc_output_context2(&ofmt_ctx_mp4, NULL, "mp4", NULL);
    if (!ofmt_ctx_mp4) {
        av_log(ifmt_ctx,AV_LOG_ERROR,"ERROR create output stream in %s\n",slice_file);
        return_error = ERROR_OPEN_OUTSTREAM_FAIL;
        goto end;
    }


    // set write to buffer
    outbuffer=(unsigned char*)av_malloc(buffsize);
    if(outbuffer==NULL)
    {
        av_log(ifmt_ctx,AV_LOG_ERROR,"ERROR Failed malloc buffer in %s \n",slice_file);
        return_error=ERROR_MEM_FAIL;
        goto end;

    }
    //three arg must be 1 for write
    // avio_out =avio_alloc_context(outbuffer, buffsize,1,fun_arg,NULL,writedata_dealfun,NULL);
    write_cache_buff.cache_buff=(uint8_t*)av_malloc(CACHE_BIT_MAZ_WIDTH*CACHE_TIME_SECOND);
    write_cache_buff.pos=write_cache_buff.cache_buff;
    write_cache_buff.limit=write_cache_buff.cache_buff+CACHE_BIT_MAZ_WIDTH*CACHE_TIME_SECOND;
    write_cache_buff.write_size=0;
    avio_out =avio_alloc_context(outbuffer, buffsize,1,&write_cache_buff,NULL,write_packet,NULL);
    if(avio_out==NULL)
    {
        av_log(ifmt_ctx,AV_LOG_ERROR,"ERROR Failed malloc AVIOContext in %s \n",slice_file);
        av_free(outbuffer);
        return_error=ERROR_IOCONTENT_FAIL;
        goto end;

    }

    ofmt_ctx_mp4->pb=avio_out;
    ofmt_ctx_mp4->flags=AVFMT_FLAG_CUSTOM_IO;

    //create streams in output
    for (index = 0; index < ifmt_ctx->nb_streams; index++) {
        if(index!=video_index&&*speed!=1.0)
            continue;
        AVStream *in_stream = ifmt_ctx->streams[index];
        AVCodec* codec= avcodec_find_decoder(in_stream->codecpar->codec_id);
        AVStream *out_stream = avformat_new_stream(ofmt_ctx_mp4,codec);

        if (!out_stream) {
            av_log(ifmt_ctx,AV_LOG_ERROR,"ERROR Fail to create out stream in %s\n",slice_file);
            return_error = ERROR_CREATE_AVSTREAM_FAIL;
            goto end;
        }

        //Copy the settings of AVCodecContext
        AVCodecContext* pCodecCtx = avcodec_alloc_context3(codec);
        ret = avcodec_parameters_to_context(pCodecCtx, in_stream->codecpar);
        if(ret<0){
            av_log(ifmt_ctx,AV_LOG_ERROR,"ERROR Fail to copy out stream codec in %s\n",slice_file);
            return_error = ERROR_CREATE_AVSTREAM_FAIL;
            avcodec_free_context(&pCodecCtx);
            goto end;
        }
        pCodecCtx->codec_tag = 0;

        if (ofmt_ctx_mp4->oformat->flags & AVFMT_GLOBALHEADER) {
            pCodecCtx->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;
        }
        ret = avcodec_parameters_from_context(out_stream->codecpar, pCodecCtx);
        if (ret < 0) {
            av_log(ifmt_ctx,AV_LOG_ERROR,"ERROR Fail to copy out stream codec in %s\n",slice_file);
            return_error = ERROR_CREATE_AVSTREAM_FAIL;
            avcodec_free_context(&pCodecCtx);
            goto end;
        }
        avcodec_free_context(&pCodecCtx);
    }

    //print out stream format
    av_dump_format(ofmt_ctx_mp4, 0, 0, 1);
    //send video only or video and audio to client
    if(first_stream_type==false)
    {
        if(audio_index==-1)
            write_cache_buff.write_size=VIDEO_ONLY;
        else
            write_cache_buff.write_size=VIDEO_AND_AUDIO;
        ret=writedata_dealfun(fun_arg,write_cache_buff.cache_buff,write_cache_buff.write_size);
        write_cache_buff.pos=write_cache_buff.cache_buff;
        write_cache_buff.write_size=0;
        first_stream_type=true;
    }
    //fmp4
    //av_dict_set(&opts, "movflags", "frag_keyframe+empty_moov", 0);
    av_dict_set(&opts, "movflags", "frag_keyframe+empty_moov+omit_tfhd_offset+faststart+separate_moof+disable_chpl+default_base_moof+dash", 0);
    //Write file header
    if (avformat_write_header(ofmt_ctx_mp4, &opts) < 0) {
        av_log(ifmt_ctx,AV_LOG_ERROR,"Error occurred when write header  in %s\n",slice_file);
        return_error = ERROR_OPEN_OUTSTREAM_FAIL;
        goto end;
    }
    av_dict_free(&opts);

    //read an write
    play_time=av_gettime();
    //r run p pause s stop
    while (*(control_cmd+1)=='r'||*(control_cmd+1)=='p'||*(control_cmd+1)=='i') {
        //pause wait run or stop
        if(*(control_cmd+1)=='p') {
            printf("wait\n");
            while(*(control_cmd+1)=='p')
                av_usleep(500000);
            if(*(control_cmd+1)=='r'||*(control_cmd+1)=='i') {
                //recompute start time;
                printf("continue\n");
                play_time=av_gettime();
                first_video_packet=false;
                last_cache_pts=0;
            }
            else {
                printf("cmd quit\n");
                goto end;
            }
        }
        //if((av_gettime() - play_time)>10000000)
        // *speed=-4.0;
        //not recv invaild speed,changge speed ,recompute time
        if(*speed!=playspeed) {
            if(*speed==0||*speed>8.0||*speed<-8.0)
                av_log(ifmt_ctx,AV_LOG_ERROR,"ERROR play speed set error %f\n",*speed);
            else {
                playspeed=*speed;
                play_time=av_gettime();
                first_video_packet=false;
            }
        }
        // playback ,find pre i frame
        if(playspeed<0&&has_send==true) {
            has_send=false;
            find_frame_time=last_real_video_pts;
            // printf("find fram time is %ld\n",find_frame_time);
            find_frame_time-=(int64_t) ( CACHE_TIME_SECOND/av_q2d(ifmt_ctx->streams[video_index]->time_base));
            if(find_frame_time<0) {
                av_log(ifmt_ctx,AV_LOG_ERROR,"ERROR play back seek to %d in %s is end\n",start_time,slice_file);
                return_error = NO_ERRORS;
                ret=writedata_dealfun(fun_arg,write_cache_buff.cache_buff,write_cache_buff.write_size);
                if(ret<0)
                    av_log(ofmt_ctx_mp4,AV_LOG_ERROR,"ERROR Fail to send  %s  cache \n",slice_file);

                //send end to client
                write_cache_buff.write_size=FILE_EOF;
                ret=writedata_dealfun(fun_arg,write_cache_buff.cache_buff,write_cache_buff.write_size);
                if(ret<0)
                    av_log(ofmt_ctx_mp4,AV_LOG_ERROR,"ERROR Fail to send  %s  cache \n",slice_file);
                break;
            }
            ret = av_seek_frame(ifmt_ctx, video_index, find_frame_time, AVSEEK_FLAG_FRAME|AVSEEK_FLAG_BACKWARD);
            if(ret<0){
                av_log(ifmt_ctx,AV_LOG_ERROR,"ERROR play back seek to %d in %s fail\n",start_time,slice_file);
                return_error = ERROR_SEEK_FAIL;
                goto end;
            }
        }
        ret = av_read_frame(ifmt_ctx, &pkt);
        if (ret < 0) {
            // slice end
            if(ret==AVERROR_EOF ){
                printf("INFO read a slice  %s end\n",slice_file);
                return_error = NO_ERRORS;
                ret=writedata_dealfun(fun_arg,write_cache_buff.cache_buff,write_cache_buff.write_size);
                if(ret<0)
                    av_log(ofmt_ctx_mp4,AV_LOG_ERROR,"ERROR Fail to send  %s  cache \n",slice_file);
                //send end to client
                write_cache_buff.write_size=FILE_EOF;
                ret = writedata_dealfun(fun_arg,write_cache_buff.cache_buff,write_cache_buff.write_size);
                if(ret<0)
                    av_log(ofmt_ctx_mp4,AV_LOG_ERROR,"ERROR Fail to send  %s  cache \n",slice_file);
                break;
            }
            else {
                av_log(ifmt_ctx,AV_LOG_ERROR,"ERROR fail read a frame in %s,error is %d\n",slice_file,ret);
                av_packet_unref(&pkt);
                fail_read_frame++;
                if(fail_read_frame>MAX_READ_FRAME_FAIL) {
                    return_error = ERROR_READ_FRAME_FAIL;
                    av_log(ifmt_ctx,AV_LOG_ERROR,"ERROR fail read frame over max in %s\n",slice_file);
                    break;
                }
                else
                    continue;
            }
        }
        fail_read_frame=0;

        //recompute pts dts for ffmpeg not allow decrease
        if(pkt.stream_index != video_index && playspeed != 1.0) { // 倍速播放时,音频丢掉
            av_packet_unref(&pkt);
            continue;
        }
        if(pkt.stream_index == video_index) { // 视频
            //printf("aaa%ld ,%ld,%ld\n",pkt.dts,last_added_video_pts,last_real_video_pts);
            if(pkt.pts > last_real_video_pts)
                packet_interval = pkt.pts - last_real_video_pts;
            else
                packet_interval = last_real_video_pts - pkt.pts;

            last_real_video_pts = pkt.pts;
            pkt.pts = last_added_video_pts+packet_interval;
            pkt.dts = pkt.pts;
            last_added_video_pts = pkt.dts;
        }
        else if (pkt.stream_index == audio_index) { // 音频
            if(pkt.pts > last_real_audio_pts)
                packet_interval = pkt.pts - last_real_audio_pts;
            else
                packet_interval = last_real_audio_pts - pkt.pts;

            last_real_audio_pts = pkt.pts;
            pkt.pts = last_added_audio_pts + packet_interval;
            pkt.dts = pkt.pts;
            last_added_audio_pts = pkt.dts;
        }
        //send i frame only
        if((playspeed > 2.0 || playspeed < 0) && pkt.stream_index == video_index && !(pkt.flags&AV_PKT_FLAG_KEY)) {
            av_packet_unref(&pkt);
            continue;
        }
        #if 1
            AVStream* in_stream  = ifmt_ctx->streams[pkt.stream_index];
            AVStream* out_stream = ofmt_ctx_mp4->streams[pkt.stream_index];
            // copy packet */
            pkt.pts = av_rescale_q_rnd(pkt.pts, in_stream->time_base, out_stream->time_base, (AVRounding)(AV_ROUND_NEAR_INF|AV_ROUND_PASS_MINMAX));
            pkt.dts = av_rescale_q_rnd(pkt.dts, in_stream->time_base, out_stream->time_base, (AVRounding)(AV_ROUND_NEAR_INF|AV_ROUND_PASS_MINMAX));
            pkt.duration = av_rescale_q(pkt.duration, in_stream->time_base, out_stream->time_base);
            pkt.pos = -1;
        #endif

        //delay
        if(pkt.stream_index == video_index){ // 视频
            if(first_video_packet==false){
                play_pts=pkt.pts;
                first_video_packet=true;
            }
            if(isSend==true) {
                if(*(control_cmd+1)!=lastcmd)
                    lastcmd=*(control_cmd+1);
                isSend=false;
            }
            if(lastcmd=='i'&&!(pkt.flags&AV_PKT_FLAG_KEY)) {
                av_packet_unref(&pkt);
                continue;
            }
            now_pts=pkt.pts-play_pts;
            AVRational time_base=ofmt_ctx_mp4->streams[video_index]->time_base;
            AVRational time_base_q={1,AV_TIME_BASE};
            now_pts = av_rescale_q((pkt.pts-play_pts), time_base, time_base_q);

            //if cache data time lager than CACHE_TIME_SECOND,send cache data
            if(pkt.flags& AV_PKT_FLAG_KEY){
                //mark lgnite start write
                if(first_lgnite==false) {
                    printf("mark start\n");
                    ret = writedata_dealfun(fun_arg,NULL,0);
                    if(ret<0)
                        av_log(ofmt_ctx_mp4,AV_LOG_ERROR,"ERROR Fail to mark  %s start write cache \n",slice_file);
                    first_lgnite = true;
                }
                else{

                    ret = writedata_dealfun(fun_arg,write_cache_buff.cache_buff,write_cache_buff.write_size);
                    if(ret<0)
                        av_log(ofmt_ctx_mp4,AV_LOG_ERROR,"ERROR Fail to send  %s  cache \n",slice_file);
                    write_cache_buff.pos=write_cache_buff.cache_buff;
                    write_cache_buff.write_size=0;

                    last_cache_pts = now_pts;
                    if(playspeed < 0)
                        has_send = true;
                    isSend = true;
                }
            }
            now_time = av_gettime() - play_time;
            // printf("%ld %ld\n",now_pts,now_time);
            //compute play speed time

            // printf("compute play speed is %f\n",playspeed);
            if(playspeed>0)
                now_time=(int64_t)(now_time*1.0*playspeed);
            else
                now_time=(int64_t)(now_time*-1.0*playspeed);

            //printf("%ld %ld\n",now_pts,now_time);
            if (now_pts > now_time)
                av_usleep(now_pts - now_time);
        } // if(pkt.stream_index == video_index)

        // printf("write %ld type %d,i %d\n",pkt.dts,pkt.stream_index,pkt.flags&AV_PKT_FLAG_KEY);
        ret = av_interleaved_write_frame(ofmt_ctx_mp4, &pkt);
        if (ret < 0) {
            av_log(ofmt_ctx_mp4,AV_LOG_ERROR,"ERROR Fail to write packet %s,error is %d\n",slice_file,ret);
            fail_write_frame++;
            if(fail_write_frame > MAX_WRITE_FRAME_FAIL) {
                av_packet_unref(&pkt);
                av_log(ofmt_ctx_mp4,AV_LOG_ERROR,"ERROR Fail write packet over max %s\n",slice_file);
                return_error = ERROR_WRITE_PKT_FAIL;
                goto end;
            }
            else continue;
        }
        fail_write_frame=0;
        //printf("Write %8d frames to output file\n",frame_index);

        av_packet_unref(&pkt);
    } // while

    //Write file trailer
    av_write_trailer(ofmt_ctx_mp4);

end:
    avformat_close_input(&ifmt_ctx);
    avio_context_free(&avio_out);
    avformat_free_context(ofmt_ctx_mp4);
    if(outbuffer!=NULL)
        av_free(outbuffer);
    if(write_cache_buff.cache_buff!=NULL)
        av_free(write_cache_buff.cache_buff);
    return return_error;
}

/*
send rtsp req to ipc,recv video stream and audio stream and save to format mp4 slice
rtsp_url rtsp url
filename slice name
control_cmd write control end
*/
int rtsp_slice(const char * rtsp_url,const  char* filename, char * control_cmd)
{
    int ret,fail_read_frame=0,fail_write_frame=0;
    bool first_video=false,first_audio=false;
    unsigned int index,video_index=-1,audio_index=-1;
    AVPacket pkt;
    AVDictionary *opts = NULL;
    AVFormatContext *ifmt_ctx = NULL,*ofmt_ctx_mp4=NULL;
    media_error_code return_error=NO_ERRORS;


    if(rtsp_url==NULL||filename==NULL||control_cmd==NULL)
        return ERROR_ARG;
    //set log
    av_log_set_callback(custom_output);
    av_log_set_level(AV_LOG_ERROR);

    AVDictionary* options = NULL;
    av_dict_set(&options,"rtsp_transport", "tcp", 0);
    av_dict_set(&options,"stimeout","3000000",0);

    //Input
    if ((ret = avformat_open_input(&ifmt_ctx,rtsp_url, 0, &options)) < 0) {
        av_log(ifmt_ctx,AV_LOG_ERROR, "ERROR,Could not open rtsp url %s\n",rtsp_url);
        return_error=ERROR_OPEN_URL_FAIL;
        goto end;
    }
    //find stream information in input
    if ((ret = avformat_find_stream_info(ifmt_ctx, 0)) < 0) {
        av_log(ifmt_ctx,AV_LOG_ERROR,"ERROR Failed to retrieve input %s stream information \n",rtsp_url);
        return_error=ERROR_STREAM_INFO_FAIL;
        goto end;
    }

    //pritnf input stream format
    av_dump_format(ifmt_ctx, 0, rtsp_url, 0);

    //Output
    avformat_alloc_output_context2(&ofmt_ctx_mp4, NULL, "mp4", filename);
    if (!ofmt_ctx_mp4) {
        av_log(ifmt_ctx,AV_LOG_ERROR,"ERROR create  output fail in "
                                     ""
                                     "%s\n",filename);
        return_error = ERROR_OPEN_OUTSTREAM_FAIL;
        goto end;
    }

    //create streams in output
    for (index = 0; index < ifmt_ctx->nb_streams; index++) {
        AVStream *in_stream = ifmt_ctx->streams[index];
        if(in_stream->codecpar->codec_type==AVMEDIA_TYPE_VIDEO)
            video_index=index;
        else if(in_stream->codecpar->codec_type==AVMEDIA_TYPE_AUDIO)
            audio_index=index;
        else {
            continue;
        }
        AVCodec* codec= avcodec_find_decoder(in_stream->codecpar->codec_id);
        AVStream *out_stream = avformat_new_stream(ofmt_ctx_mp4,codec);

        if (!out_stream) {
            av_log(ifmt_ctx,AV_LOG_ERROR,"ERROR Fail to create out stream in %s\n",rtsp_url);
            ret = ERROR_CREATE_AVSTREAM_FAIL;
            goto end;
        }

        //Copy the settings of AVCodecContext
        AVCodecContext* pCodecCtx = avcodec_alloc_context3(codec);
        ret = avcodec_parameters_to_context(pCodecCtx, in_stream->codecpar);
        if(ret<0){
            av_log(ifmt_ctx,AV_LOG_ERROR,"ERROR Fail to copy out stream codec in %s\n",rtsp_url);
            ret = ERROR_CREATE_AVSTREAM_FAIL;
            avcodec_free_context(&pCodecCtx);
            goto end;
        }
        pCodecCtx->codec_tag = 0;

        if (ofmt_ctx_mp4->oformat->flags & AVFMT_GLOBALHEADER) {
            pCodecCtx->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;
        }

        ret = avcodec_parameters_from_context(out_stream->codecpar, pCodecCtx);

        if (ret < 0) {
            av_log(ifmt_ctx,AV_LOG_ERROR,"ERROR Fail to copy out stream codec in %s\n",rtsp_url);
            ret = ERROR_CREATE_AVSTREAM_FAIL;
            avcodec_free_context(&pCodecCtx);
            goto end;

        }
        avcodec_free_context(&pCodecCtx);
    }


    //print out stream format
    av_dump_format(ofmt_ctx_mp4, 0, filename, 1);

    //Open output file
    if (!(ofmt_ctx_mp4->flags & AVFMT_NOFILE)) {
        if (avio_open(&ofmt_ctx_mp4->pb, filename, AVIO_FLAG_WRITE) < 0) {
            av_log( ofmt_ctx_mp4,AV_LOG_ERROR,"Could not open output file %s\n", filename);
            ret = ERROR_OPEN_OUTSTREAM_FAIL;
            goto end;
        }
    }
    //fmp4
    av_dict_set(&opts, "movflags", "frag_keyframe+empty_moov", 0);
    //Write file header
    if (avformat_write_header(ofmt_ctx_mp4, &opts) < 0) {
        av_log(ofmt_ctx_mp4, AV_LOG_ERROR,"Error occurred when write output file head\n");
        ret = ERROR_OPEN_OUTSTREAM_FAIL;
        goto end;
    }
    av_dict_free(&opts);

    //read an write
    //printf("control_cmd is %c\n",*(control_cmd+1));
    while (*(control_cmd+1)=='r') {
        //  printf("control_cmd is %c\n",*(control_cmd+1));
        if ((ret=av_read_frame(ifmt_ctx, &pkt))< 0){
            av_log(ifmt_ctx,AV_LOG_ERROR,"ERROR fail read a frame in %s,error is %d\n",rtsp_url,ret);
            av_packet_unref(&pkt);
            fail_read_frame++;
            if(fail_read_frame>MAX_READ_FRAME_FAIL)
            {
                return_error = ERROR_READ_FRAME_FAIL;
                av_log(ifmt_ctx,AV_LOG_ERROR,"ERROR fail read frame over max in %s\n",rtsp_url);
                break;
            }
            else
                continue;
        }
        else {
            fail_read_frame=0;
        }
        if(pkt.stream_index!=video_index&&pkt.stream_index!=audio_index)
        {
            av_packet_unref(&pkt);
            continue;
        }
        // set first packet pts 0 for sometimes first dts is lager than others
        if(first_video==0&&pkt.stream_index==video_index)
        {
            pkt.dts=0;
            pkt.pts=0;
            first_video=true;
        }
        if(first_audio==0&&pkt.stream_index==audio_index)
        {
            pkt.dts=0;
            pkt.pts=0;
            first_audio=true;
        }

        //printf("read a frame indexis %d %ld\n",pkt.stream_index,pkt.dts);

        if ((ret=av_interleaved_write_frame(ofmt_ctx_mp4, &pkt))< 0) {
            av_log(ofmt_ctx_mp4,AV_LOG_ERROR,"ERROR Fail to write packet %s,error is %d\n",rtsp_url,ret);
            fail_write_frame++;
            if(fail_write_frame>MAX_WRITE_FRAME_FAIL)
            {
                av_packet_unref(&pkt);
                av_log(ofmt_ctx_mp4,AV_LOG_ERROR,"ERROR Fail write packet over max %s\n",rtsp_url);
                return_error = ERROR_WRITE_PKT_FAIL;
                goto end;
            }
            else {
                continue;
            }
        }
        else {
            fail_write_frame=0;
        }
        //printf("Write %8d frames to output file\n",frame_index);
        av_packet_unref(&pkt);
    }
    //Write file trailer
    av_write_trailer(ofmt_ctx_mp4);
end:

    avformat_close_input(&ifmt_ctx);
    /* close output */
    if (ofmt_ctx_mp4 && !(ofmt_ctx_mp4->flags & AVFMT_NOFILE))
        avio_close(ofmt_ctx_mp4->pb);
    avformat_free_context(ofmt_ctx_mp4);
    return return_error;
}

/*
send rtsp req to ipc,recv video stream and audio stream and save to format mp4 slice
rtsp_url rtsp url
filenames slice names
slice_num number of slice
slice_second slice time
control_cmd control
*/
int rtsp_muti_slice(const char * rtsp_url,const  char** filenames,int slice_num,int slice_second, char * control_cmd)
{
    int ret,slice_index=0,fail_read_frame=0,fail_write_frame=0;
    bool first_video=false,first_audio=false;
    unsigned int index,video_index=-1,audio_index=-1;
    AVPacket pkt;
    AVDictionary *opts = NULL;
    AVFormatContext *ifmt_ctx = NULL,*ofmt_ctx_mp4=NULL;
    media_error_code return_error=NO_ERRORS;
    int64_t slice_start=0,now_time=0;

    if(rtsp_url==NULL||filenames==NULL||control_cmd==NULL||slice_num<=0||slice_second<=0)
        return ERROR_ARG;
    //set log
    av_log_set_callback(custom_output);
    av_log_set_level(AV_LOG_ERROR);

    AVDictionary* options = NULL;
    av_dict_set(&options,"rtsp_transport", "tcp", 0);
    av_dict_set(&options,"stimeout","3000000",0);

    //Input
    if ((ret = avformat_open_input(&ifmt_ctx,rtsp_url, 0, &options)) < 0) {
        av_log(ifmt_ctx,AV_LOG_ERROR, "ERROR,Could not open rtsp url %s\n",rtsp_url);
        return_error=ERROR_OPEN_URL_FAIL;
        goto end;
    }
    //find stream information in input
    if ((ret = avformat_find_stream_info(ifmt_ctx, 0)) < 0) {
        av_log(ifmt_ctx,AV_LOG_ERROR,"ERROR Failed to retrieve input %s stream information \n",rtsp_url);
        return_error=ERROR_STREAM_INFO_FAIL;
        goto end;
    }

    //pritnf input stream format
    // av_dump_format(ifmt_ctx, 0, rtsp_url, 0);

    slice_start=av_gettime();
    slice_start/=10000;
    slice_start%=slice_second*100;
    printf("start is %ld\n",slice_start);
    //open slices
slice:
    avformat_alloc_output_context2(&ofmt_ctx_mp4, NULL, "mp4", filenames[slice_index]);
    if (!ofmt_ctx_mp4) {
        av_log(ifmt_ctx,AV_LOG_ERROR,"ERROR create  output fail %s\n",filenames[slice_index]);
        return_error = ERROR_OPEN_OUTSTREAM_FAIL;
        goto end;
    }

    //create streams in output
    for (index = 0; index < ifmt_ctx->nb_streams; index++) {
        AVStream *in_stream = ifmt_ctx->streams[index];
        if(in_stream->codecpar->codec_type==AVMEDIA_TYPE_VIDEO)
            video_index=index;
        else if(in_stream->codecpar->codec_type==AVMEDIA_TYPE_AUDIO)
            audio_index=index;
        else {
            continue;
        }
        AVCodec* codec= avcodec_find_decoder(in_stream->codecpar->codec_id);
        AVStream *out_stream = avformat_new_stream(ofmt_ctx_mp4,codec);

        if (!out_stream) {
            av_log(ifmt_ctx,AV_LOG_ERROR,"ERROR Fail to create out stream in %s\n",rtsp_url);
            ret = ERROR_CREATE_AVSTREAM_FAIL;
            goto end;
        }

        //Copy the settings of AVCodecContext
        AVCodecContext* pCodecCtx = avcodec_alloc_context3(codec);
        ret = avcodec_parameters_to_context(pCodecCtx, in_stream->codecpar);
        if(ret<0){
            av_log(ifmt_ctx,AV_LOG_ERROR,"ERROR Fail to copy out stream codec in %s\n",rtsp_url);
            ret = ERROR_CREATE_AVSTREAM_FAIL;
            avcodec_free_context(&pCodecCtx);
            goto end;
        }
        pCodecCtx->codec_tag = 0;

        if (ofmt_ctx_mp4->oformat->flags & AVFMT_GLOBALHEADER) {
            pCodecCtx->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;
        }

        ret = avcodec_parameters_from_context(out_stream->codecpar, pCodecCtx);

        if (ret < 0) {
            av_log(ifmt_ctx,AV_LOG_ERROR,"ERROR Fail to copy out stream codec in %s\n",rtsp_url);
            ret = ERROR_CREATE_AVSTREAM_FAIL;
            avcodec_free_context(&pCodecCtx);
            goto end;

        }
        avcodec_free_context(&pCodecCtx);
    }


    //print out stream format
    av_dump_format(ofmt_ctx_mp4, 0, filenames[slice_index], 1);

    //Open output file
    if (!(ofmt_ctx_mp4->flags & AVFMT_NOFILE)) {
        if (avio_open(&ofmt_ctx_mp4->pb, filenames[slice_index], AVIO_FLAG_WRITE) < 0) {
            av_log( ofmt_ctx_mp4,AV_LOG_ERROR,"Could not open output file '%s'", filenames[slice_index]);
            return_error = ERROR_OPEN_OUTSTREAM_FAIL;
            goto end;
        }
    }
    //fmp4
    av_dict_set(&opts, "movflags", "frag_keyframe+empty_moov", 0);
    //Write file header
    if (avformat_write_header(ofmt_ctx_mp4, &opts) < 0) {
        av_log( ofmt_ctx_mp4,AV_LOG_ERROR,"Error occurred when write output file header\n");
        return_error = ERROR_OPEN_OUTSTREAM_FAIL;
        goto end;
    }
    av_dict_free(&opts);

    //last read key frame
    if(slice_index>0)
    {
        if ((ret=av_interleaved_write_frame(ofmt_ctx_mp4, &pkt))< 0) {
            av_log(ofmt_ctx_mp4,AV_LOG_ERROR,"ERROR Fail to write packet %s,error is %d\n",rtsp_url,ret);
            return_error = ERROR_WRITE_PKT_FAIL;
            goto end;
        }
        av_packet_unref(&pkt);
    }
    //read an write
    // printf("control_cmd is %c\n",*(control_cmd+1));
    while (*(control_cmd+1)=='r') {
        //printf("control_cmd is %c\n",*(control_cmd+1));
        if ((ret=av_read_frame(ifmt_ctx, &pkt))< 0){
            av_log(ifmt_ctx,AV_LOG_ERROR,"ERROR fail read a frame in %s,error is %d\n",rtsp_url,ret);
            av_packet_unref(&pkt);
            fail_read_frame++;
            if(fail_read_frame>MAX_READ_FRAME_FAIL)
            {
                return_error = ERROR_READ_FRAME_FAIL;
                av_log(ifmt_ctx,AV_LOG_ERROR,"ERROR fail read frame over max in %s\n",rtsp_url);
                break;
            }
            else
                continue;
        }
        else {
            fail_read_frame=0;
        }
        if(pkt.stream_index!=video_index&&pkt.stream_index!=audio_index)
        {
            av_packet_unref(&pkt);
            continue;
        }
        // set first packet pts 0 for sometimes first dts is lager than others
        if(first_video==0&&pkt.stream_index==video_index)
        {
            pkt.dts=0;
            pkt.pts=0;
            first_video=true;
        }
        if(first_audio==0&&pkt.stream_index==audio_index)
        {
            pkt.dts=0;
            pkt.pts=0;
            first_audio=true;
        }
        //if time over and i frame,next slice
        if(pkt.stream_index==video_index&&pkt.flags& AV_PKT_FLAG_KEY){
            now_time=av_gettime();
            now_time/=10000;
            now_time%=slice_second*100;
            printf("now_time is %ld %ld\n",now_time,slice_start);
            if(now_time<slice_start)
            {
                printf("%d %d\n",slice_index,slice_num);
                slice_index++;
                if(slice_index>=slice_num)
                    break;
                else
                {
                    av_write_trailer(ofmt_ctx_mp4);
                    if (ofmt_ctx_mp4 && !(ofmt_ctx_mp4->flags & AVFMT_NOFILE))
                        avio_close(ofmt_ctx_mp4->pb);
                    avformat_free_context(ofmt_ctx_mp4);
                    slice_start=now_time;
                    goto slice;
                }
            }
            slice_start=now_time;
        }
        // printf("read a frame indexis %d %ld\n",pkt.stream_index,pkt.dts);

        if ((ret=av_interleaved_write_frame(ofmt_ctx_mp4, &pkt))< 0) {
            av_log(ofmt_ctx_mp4,AV_LOG_ERROR,"ERROR Fail to write packet %s,error is %d\n",rtsp_url,ret);
            fail_write_frame++;
            if(fail_write_frame>MAX_WRITE_FRAME_FAIL)
            {
                av_packet_unref(&pkt);
                av_log(ofmt_ctx_mp4,AV_LOG_ERROR,"ERROR Fail write packet over max %s\n",rtsp_url);
                return_error = ERROR_WRITE_PKT_FAIL;
                goto end;
            }
            else {
                continue;
            }
        }
        else {
            fail_write_frame=0;
        }

        //printf("Write %8d frames to output file\n",frame_index);
        av_packet_unref(&pkt);
    }
    //Write file trailer
    av_write_trailer(ofmt_ctx_mp4);
end:

    avformat_close_input(&ifmt_ctx);
    /* close output */
    if (ofmt_ctx_mp4 && !(ofmt_ctx_mp4->flags & AVFMT_NOFILE))
        avio_close(ofmt_ctx_mp4->pb);
    avformat_free_context(ofmt_ctx_mp4);
    return return_error;
}


/*
send rtsp req to ipc,recv video stream and audio stream and send to buff
rtsp_url rtsp url
fun_arg write funtion arg
writedata_dealfun write funtion pointer
*/
int rtsp_stream_direct(const char * rtsp_url, void* fun_arg,write_stream writedata_dealfun,char* control_cmd)
{
    int ret,fail_read_frame=0,fail_write_frame=0;
    bool first_video=false,first_audio=false;
    unsigned int index,video_index=-1,audio_index=-1;
    AVPacket pkt;
    AVDictionary *opts = NULL;
    AVFormatContext *ifmt_ctx = NULL,*ofmt_ctx_mp4=NULL;
    media_error_code return_error=NO_ERRORS;
    int buffsize=4096;
    unsigned char* outbuffer=NULL;
    AVIOContext *avio_out=NULL;

    if(rtsp_url==NULL||fun_arg==NULL||writedata_dealfun==NULL||control_cmd==NULL)
        return ERROR_ARG;

    //set log
    av_log_set_callback(custom_output);
    av_log_set_level(AV_LOG_ERROR);

    //set rtsp
    AVDictionary* options = NULL;
    av_dict_set(&options,"rtsp_transport", "tcp", 0);
    av_dict_set(&options,"stimeout","3000000",0);
    //Input
    if ((ret = avformat_open_input(&ifmt_ctx,rtsp_url, 0, &options)) < 0) {
        av_log(ifmt_ctx,AV_LOG_ERROR, "ERROR,Could not open rtsp url %s,error is %d\n",rtsp_url,ret);
        return_error=ERROR_OPEN_URL_FAIL;
        goto end;
    }
    //find stream information in input
    if ((ret = avformat_find_stream_info(ifmt_ctx, 0)) < 0) {
        av_log(ifmt_ctx,AV_LOG_ERROR,"ERROR Failed to retrieve input %s stream information,error is %d  \n",rtsp_url,ret);
        return_error=ERROR_STREAM_INFO_FAIL;
        goto end;
    }

    //pritnf input stream format
    av_dump_format(ifmt_ctx, 0, rtsp_url, 0);

    //Output
    avformat_alloc_output_context2(&ofmt_ctx_mp4, NULL, "mp4", NULL);
    if (!ofmt_ctx_mp4) {
        av_log(ifmt_ctx,AV_LOG_ERROR,"ERROR create  output fail\n");
        return_error = ERROR_OPEN_OUTSTREAM_FAIL;
        goto end;
    }

    // set write to buffer
    outbuffer=(uint8_t*)av_malloc(buffsize);
    if(outbuffer==NULL)
    {
        av_log(ifmt_ctx,AV_LOG_ERROR,"ERROR Failed malloc buffer in %s \n",rtsp_url);
        return_error=ERROR_MEM_FAIL;
        goto end;

    }
    //threearg must be 1 for write
    avio_out =avio_alloc_context(outbuffer, buffsize,1,fun_arg,NULL,writedata_dealfun,NULL);
    if(avio_out==NULL)
    {
        av_log(ifmt_ctx,AV_LOG_ERROR,"ERROR Failed malloc AVIOContext in %s \n",rtsp_url);
        av_free(outbuffer);
        return_error=ERROR_IOCONTENT_FAIL;
        goto end;
    }
    ofmt_ctx_mp4->pb=avio_out;
    ofmt_ctx_mp4->flags=AVFMT_FLAG_CUSTOM_IO;

    //create streams in output
    for (index = 0; index < ifmt_ctx->nb_streams; index++) {
        AVStream *in_stream = ifmt_ctx->streams[index];
        if(in_stream->codecpar->codec_type==AVMEDIA_TYPE_VIDEO)
            video_index=index;
        else if(in_stream->codecpar->codec_type==AVMEDIA_TYPE_AUDIO)
            audio_index=index;
        else {
            continue;
        }
        AVCodec* codec= avcodec_find_decoder(in_stream->codecpar->codec_id);
        AVStream *out_stream = avformat_new_stream(ofmt_ctx_mp4,codec);

        if (!out_stream) {
            av_log(ifmt_ctx,AV_LOG_ERROR,"ERROR Fail to create out stream in %s\n",rtsp_url);
            ret = ERROR_CREATE_AVSTREAM_FAIL;
            goto end;
        }

        //Copy the settings of AVCodecContext
        AVCodecContext* pCodecCtx = avcodec_alloc_context3(codec);
        ret = avcodec_parameters_to_context(pCodecCtx, in_stream->codecpar);
        if(ret<0){
            av_log(ifmt_ctx,AV_LOG_ERROR,"ERROR Fail to copy out stream codec in %s\n",rtsp_url);
            ret = ERROR_CREATE_AVSTREAM_FAIL;
            avcodec_free_context(&pCodecCtx);
            goto end;
        }
        pCodecCtx->codec_tag = 0;

        if (ofmt_ctx_mp4->oformat->flags & AVFMT_GLOBALHEADER) {
            pCodecCtx->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;
        }
        ret = avcodec_parameters_from_context(out_stream->codecpar, pCodecCtx);
        if (ret < 0) {
            av_log(ifmt_ctx,AV_LOG_ERROR,"ERROR Fail to copy out stream codec in %s\n",rtsp_url);
            ret = ERROR_CREATE_AVSTREAM_FAIL;
            avcodec_free_context(&pCodecCtx);
            goto end;
        }
        avcodec_free_context(&pCodecCtx);
    }
    //print out stream format
    av_dump_format(ofmt_ctx_mp4, 0, 0, 1);

    //fmp4
    //av_dict_set(&opts, "movflags", "frag_keyframe+empty_moov", 0);
    av_dict_set(&opts, "movflags", "frag_keyframe+empty_moov+omit_tfhd_offset+faststart+separate_moof+disable_chpl+default_base_moof+dash", 0);
    //Write file header
    if (avformat_write_header(ofmt_ctx_mp4, &opts) < 0) {
        av_log(ifmt_ctx,AV_LOG_ERROR,"Error occurred when write output file header\n");
        ret=ERROR_OPEN_OUTSTREAM_FAIL;
        goto end;
    }
    av_dict_free(&opts);

    //read an write
    while (*(control_cmd+1)=='r') {
        if ((ret=av_read_frame(ifmt_ctx, &pkt))< 0){
            av_log(ifmt_ctx,AV_LOG_ERROR,"ERROR fail read a frame in %s,error is %d\n",rtsp_url,ret);
            av_packet_unref(&pkt);
            fail_read_frame++;
            if(fail_read_frame>MAX_READ_FRAME_FAIL)
            {
                return_error = ERROR_READ_FRAME_FAIL;
                av_log(ifmt_ctx,AV_LOG_ERROR,"ERROR fail read frame over max in %s\n",rtsp_url);
                break;
            }
            else
                continue;
        }
        else {
            fail_read_frame=0;
        }
        if(pkt.stream_index!=video_index&&pkt.stream_index!=audio_index)
        {
            av_packet_unref(&pkt);
            continue;
        }
        // set first packet pts 0 for sometimes first dts is lager than others
        if(first_video==0&&pkt.stream_index==video_index)
        {
            pkt.dts=0;
            pkt.pts=0;
            first_video=true;
        }
        if(first_audio==0&&pkt.stream_index==audio_index)
        {
            pkt.dts=0;
            pkt.pts=0;
            first_audio=true;
        }
        //Write
        // printf("read a frame indexis %d %ld\n",pkt.stream_index,pkt.dts);
#if 0
        AVStream* in_stream  = ifmt_ctx->streams[pkt.stream_index];
        AVStream* out_stream = ofmt_ctx_mp4->streams[pkt.stream_index];
        // copy packet */
        pkt.pts = av_rescale_q_rnd(pkt.pts, in_stream->time_base, out_stream->time_base, (AVRounding)(AV_ROUND_NEAR_INF|AV_ROUND_PASS_MINMAX));
        pkt.dts = av_rescale_q_rnd(pkt.dts, in_stream->time_base, out_stream->time_base, (AVRounding)(AV_ROUND_NEAR_INF|AV_ROUND_PASS_MINMAX));
        pkt.duration = av_rescale_q(pkt.duration, in_stream->time_base, out_stream->time_base);
        pkt.pos = -1;
#endif
        //printf("Write %8d frames to output file\n",frame_index);
        if ((ret=av_interleaved_write_frame(ofmt_ctx_mp4, &pkt))< 0) {
            av_log(ofmt_ctx_mp4,AV_LOG_ERROR,"ERROR Fail to write packet %s,error is %d\n",rtsp_url,ret);
            fail_write_frame++;
            if(fail_write_frame>MAX_WRITE_FRAME_FAIL)
            {
                av_packet_unref(&pkt);
                av_log(ofmt_ctx_mp4,AV_LOG_ERROR,"ERROR Fail write packet over max %s\n",rtsp_url);
                return_error = ERROR_WRITE_PKT_FAIL;
                goto end;
            }
        }else
            fail_write_frame=0;
        av_packet_unref(&pkt);
    }
    //Write file trailer
    av_write_trailer(ofmt_ctx_mp4);
end:
    avformat_close_input(&ifmt_ctx);
    avio_context_free(&avio_out);
    avformat_free_context(ofmt_ctx_mp4);
    return return_error;
}

#define RECONNECT_TIME 60
/*
send rtsp req to ipc,recv video stream and audio stream and save to format mp4 slice
rtsp_url rtsp url
filename slice name
control_cmd write control end
stopsecond stop time
video_type  code type
recordtime  real record time
*/
int rtsp_slice_autostop(const char * rtsp_url,const  char* filename, char * control_cmd,int stopsecond,int* video_type,int* recordtime, custom_cbk cbk/*=NULL*/, void* arg/*=NULL*/)
{
    int ret = -1,fail_read_frame=0,fail_write_frame=0,next_stop_time=0;
    bool first_video=false,first_audio=false;
    unsigned int index,video_index=-1,audio_index=-1;
    AVPacket pkt;
    AVFormatContext *ifmt_ctx = NULL,*ofmt_ctx_mp4=NULL;
    media_error_code return_error=NO_ERRORS;
    //int64_t start_time=0,now_time=0,record_all_time=0;
    time_t stop_time = 0;
    int64_t audio_number = 0, video_number = 0;
    int videoCode_type=3;
    char err_msg[64] = {0};
    //set log
    av_log_set_callback(custom_output);
    av_log_set_level(AV_LOG_ERROR);
    if(rtsp_url==NULL||control_cmd==NULL||stopsecond<=0||video_type==NULL||recordtime==NULL)
        return ERROR_ARG;

    AVDictionary* options = NULL;
    av_dict_set(&options, "buffer_size", "2048000", 0);
    av_dict_set(&options,"rtsp_transport", "tcp", 0);
    av_dict_set(&options,"stimeout","3000000",0);
    //Input
    if ((ret = avformat_open_input(&ifmt_ctx,rtsp_url, 0, &options)) < 0) {
        av_log(ifmt_ctx,AV_LOG_ERROR, "ERROR,Could not open rtsp url %s\n",rtsp_url);
        av_strerror(ret, err_msg, sizeof(err_msg));
        LOG("ERROR", "avformat_open_input function failed [ret:%d,msg:'%s']\n", ret, err_msg);
        return_error=ERROR_OPEN_URL_FAIL;
        av_dict_free(&options);
        goto end;
    }
    av_dict_free(&options);
    //find stream information in input
    if ((ret = avformat_find_stream_info(ifmt_ctx, 0)) < 0) {
        av_log(ifmt_ctx,AV_LOG_ERROR,"ERROR Failed to retrieve input %s stream information \n",rtsp_url);
        av_strerror(ret, err_msg, sizeof(err_msg));
        LOG("ERROR", "avformat_find_stream_info function failed [ret:%d,msg:'%s']\n", ret, err_msg);
        return_error=ERROR_STREAM_INFO_FAIL;
        goto end;
    }
    //LOG("INFO", "[slice] ------ 3 ------\n");
    av_dump_format(ifmt_ctx, 0, rtsp_url, 0);
    //Output
    ret = avformat_alloc_output_context2(&ofmt_ctx_mp4, NULL, "mp4", filename);
    if ((ret < 0) || (!ofmt_ctx_mp4)) {
        return_error = ERROR_OPEN_OUTSTREAM_FAIL;
        av_strerror(ret, err_msg, sizeof(err_msg));
        LOG("ERROR","avformat_alloc_output_context2 function failed [ret:%d,msg:'%s']\n", ret,err_msg);
        av_log(ifmt_ctx, AV_LOG_ERROR, "ERROR create  output fail in %s\n", filename ? filename : "null");
        goto end;
    }
    if (NULL != filename) {
        if (!(ofmt_ctx_mp4->oformat->flags & AVFMT_NOFILE)) {
            ret = avio_open2(&ofmt_ctx_mp4->pb, filename, AVIO_FLAG_READ_WRITE, NULL, NULL);
            if (ret < 0) {
                av_strerror(ret, err_msg, sizeof(err_msg));
                LOG("ERROR", "avio_open2 function failed [ret:%d,msg:'%s']\n", ret, err_msg);
                av_log(ofmt_ctx_mp4, AV_LOG_ERROR, "avio_open function failed %d \"%s\"\n", ret, filename);
                return_error = ERROR_OPEN_OUTSTREAM_FAIL;
                goto end;
            }
        }
    }
    else {
        unsigned char* buffer = (uint8_t*)av_malloc(BUFF_SIZE);
        AVIOContext *avio_ctx = avio_alloc_context(buffer, BUFF_SIZE, 1, arg, NULL, cbk, NULL);
        if (NULL == avio_ctx) {
            av_log(ofmt_ctx_mp4, AV_LOG_ERROR, "avio_alloc_context function failed\n");
            LOG("ERROR", "avio_alloc_context function failed\n");
            av_free(buffer);
            goto end;
        }
        ofmt_ctx_mp4->pb = avio_ctx;
        ofmt_ctx_mp4->flags = AVFMT_FLAG_CUSTOM_IO;
    }
    for (index = 0; index < ifmt_ctx->nb_streams; index++) {
        AVStream *in_stream = ifmt_ctx->streams[index];
        if(in_stream->codecpar->codec_type==AVMEDIA_TYPE_VIDEO)
            video_index=index;
        else if(in_stream->codecpar->codec_type==AVMEDIA_TYPE_AUDIO)
            audio_index=index;
        else {
            continue;
        }
        AVCodec* codec= avcodec_find_decoder(in_stream->codecpar->codec_id);
        AVStream *out_stream = avformat_new_stream(ofmt_ctx_mp4,codec);
        if (!out_stream) {
            av_log(ifmt_ctx,AV_LOG_ERROR,"ERROR Fail to create out stream in %s\n",rtsp_url);
            return_error = ERROR_CREATE_AVSTREAM_FAIL;
            LOG("ERROR", "avformat_new_stream function failed.\n");
            goto end;
        }
        int ret = avcodec_parameters_copy(out_stream->codecpar, in_stream->codecpar);
        if (ret < 0) {
            av_strerror(ret, err_msg, sizeof(err_msg));
            LOG("ERROR", "avcodec_parameters_copy function failed [ret:%d,msg:'%s']\n", ret, err_msg);
            break;
        }
        out_stream->codecpar->codec_tag = 0;
        if (ofmt_ctx_mp4->oformat->flags & AVFMT_GLOBALHEADER)
            ;// out_stream->codec->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;
        //AVCodecContext* pCodecCtx = avcodec_alloc_context3(codec);
        //ret = avcodec_parameters_to_context(pCodecCtx, in_stream->codecpar);
        //if(ret<0){
        //    av_log(ifmt_ctx,AV_LOG_ERROR,"ERROR Fail to copy out stream codec in %s\n",rtsp_url);
        //    return_error = ERROR_CREATE_AVSTREAM_FAIL;
        //    avcodec_free_context(&pCodecCtx);
        //    av_strerror(ret, err_msg, sizeof(err_msg));
        //    LOG("ERROR", "avcodec_parameters_to_context function failed [ret:%d,msg:'%s']\n", ret, err_msg);
        //    goto end;
        //}
        //pCodecCtx->codec_tag = 0;
        //if (ofmt_ctx_mp4->oformat->flags & AVFMT_GLOBALHEADER) {
        //    pCodecCtx->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;
        //}
        //ret = avcodec_parameters_from_context(out_stream->codecpar, pCodecCtx);
        //if (ret < 0) {
        //    av_log(ifmt_ctx,AV_LOG_ERROR,"ERROR Fail to copy out stream codec in %s\n",rtsp_url);
        //    av_strerror(ret, err_msg, sizeof(err_msg));
        //    LOG("ERROR", "avcodec_parameters_from_context function failed [ret:%d,msg:'%s']\n", ret, err_msg);
        //    return_error = ERROR_CREATE_AVSTREAM_FAIL;
        //    avcodec_free_context(&pCodecCtx);
        //    goto end;
        //}
        //avcodec_free_context(&pCodecCtx);
    }
    //LOG("INFO", "[slice] ------ 10 ------\n");
    //get video type
    if(ifmt_ctx->streams[video_index]->codecpar->codec_id==AV_CODEC_ID_H264)
        videoCode_type=0;
    else if(ifmt_ctx->streams[video_index]->codecpar->codec_id==AV_CODEC_ID_HEVC)
        videoCode_type=1;
    else
        videoCode_type=2;
    *video_type=videoCode_type;
    printf("get video type is %d\n",videoCode_type);
    av_dump_format(ofmt_ctx_mp4, 0, filename, 1);
    //av_dict_set(&options, "movflags", "frag_keyframe+empty_moov", 0);
    ret = avformat_write_header(ofmt_ctx_mp4, NULL);
    av_dict_free(&options);
    if (ret < 0) {
        av_log( ofmt_ctx_mp4,AV_LOG_ERROR,"Error occurred when write output file header\n");
        return_error=ERROR_OPEN_OUTSTREAM_FAIL;
        av_strerror(ret, err_msg, sizeof(err_msg));
        LOG("ERROR", "avformat_write_header function failed [ret:%d,msg:'%s']\n", ret, err_msg);
        goto end;
    }

    while (*(control_cmd+1)=='r') {
        if ((ret=av_read_frame(ifmt_ctx, &pkt))< 0){
            av_log(ifmt_ctx,AV_LOG_ERROR,"ERROR fail read a frame in %s,error is %d\n",rtsp_url,ret);
            av_strerror(ret, err_msg, sizeof(err_msg));
            LOG("ERROR", "av_read_frame function failed [ret:%d,msg:'%s']\n", ret, err_msg);
            av_packet_unref(&pkt);
            fail_read_frame++;
            if(fail_read_frame>MAX_READ_FRAME_FAIL) {
                return_error = ERROR_READ_FRAME_FAIL;
                av_log(ifmt_ctx,AV_LOG_ERROR,"ERROR fail read frame over max in %s\n",rtsp_url);
                break;
            }
            continue;
        }
        fail_read_frame = 0;
        if (pkt.stream_index == audio_index) { // audio
            if (false == first_audio) {
                pkt.dts = 0;
                pkt.pts = 0;
                first_audio = true;
            }
            else if (pkt.pts == AV_NOPTS_VALUE) {
                pkt.duration = (double)AV_TIME_BASE / av_q2d(ifmt_ctx->streams[audio_index]->r_frame_rate);
                pkt.pts = ++audio_number * pkt.duration;
                pkt.dts = pkt.pts;
                printf("audio pts=%lld,dts=%lld,duration=%lld\n", pkt.pts, pkt.dts, pkt.duration);
            }
        }
        else if (pkt.stream_index == video_index) { // video
            if (false == first_video) {
                pkt.dts = 0;
                pkt.pts = 0;
                first_video = true;
                //int64_t start_time = av_gettime();
                //int64_t next_stop_time = stopsecond - (start_time / AV_TIME_BASE) % stopsecond;
                stop_time = time(NULL) + (stopsecond - (time(NULL) % stopsecond));
            }
            else if (pkt.pts == AV_NOPTS_VALUE) {
                pkt.duration = (double)AV_TIME_BASE / av_q2d(ifmt_ctx->streams[video_index]->r_frame_rate);
                pkt.pts = ++video_number * pkt.duration;
                pkt.dts = pkt.pts;
                printf("video pts=%lld,dts=%lld,duration=%lld\n", pkt.pts, pkt.dts, pkt.duration);
            }
        }
        else {
            LOG("WARN","Other stream %d\n", pkt.stream_index);
            av_packet_unref(&pkt);
            continue;
        }
        //if time over and i frame,stop
        if((pkt.stream_index == video_index) && (pkt.flags & AV_PKT_FLAG_KEY)) {
            //now_time=av_gettime();
            //if (((now_time - start_time) / AV_TIME_BASE) > next_stop_time) {
            if(time(NULL) > stop_time) {
                AVRational time_base = ifmt_ctx->streams[video_index]->time_base;
                AVRational time_base_q = {1,AV_TIME_BASE};
                int64_t record_all_time = av_rescale_q((pkt.pts), time_base, time_base_q);
                printf("record time is %lld\n", record_all_time);
                *recordtime = record_all_time/AV_TIME_BASE;
                av_packet_unref(&pkt);
                return_error=NO_ERRORS;
                break;
            }
        }
        //printf("video pts=%lld,dts=%lld,duration=%lld\n", pkt.pts, pkt.dts, pkt.duration);
        if ((ret = av_interleaved_write_frame(ofmt_ctx_mp4, &pkt)) < 0) {
            av_log(ofmt_ctx_mp4,AV_LOG_ERROR,"ERROR Fail to write packet %s,error is %d\n",rtsp_url,ret);
            av_strerror(ret, err_msg, sizeof(err_msg));
            LOG("ERROR", "av_interleaved_write_frame function failed [ret:%d,msg:'%s']\n", ret, err_msg);
            fail_write_frame++;
            av_packet_unref(&pkt);
            if(fail_write_frame > MAX_WRITE_FRAME_FAIL) {
                av_log(ofmt_ctx_mp4,AV_LOG_ERROR,"ERROR Fail write packet over max %s\n",rtsp_url);
                return_error = ERROR_WRITE_PKT_FAIL;
                goto end;
            }
            continue;
        }
        fail_write_frame = 0;
        av_packet_unref(&pkt);
    }// while
    if ((ret = av_write_trailer(ofmt_ctx_mp4)) < 0) {
        char err_msg[64] = { 0 };
        av_strerror(ret, err_msg, sizeof(err_msg));
        LOG("ERROR", "av_write_trailer function failed [ret:%d,msg:'%s']\n", ret, err_msg);
    }
end:
    if (NULL != ifmt_ctx) {
        avformat_close_input(&ifmt_ctx);
    }
    if (NULL != ofmt_ctx_mp4) {
        if (NULL != filename) {
            if(!(ofmt_ctx_mp4->flags & AVFMT_NOFILE))
            avio_close(ofmt_ctx_mp4->pb);
        }
        else if (ofmt_ctx_mp4->flags & AVFMT_FLAG_CUSTOM_IO) {
            if (NULL != ofmt_ctx_mp4->pb) {
                av_freep(&ofmt_ctx_mp4->pb->buffer);
                avio_context_free(&ofmt_ctx_mp4->pb);
            }
        }
        avformat_free_context(ofmt_ctx_mp4);
    }
    return return_error;
#if 0
    if(ret!=ERROR_OPEN_URL_FAIL&&ret!=ERROR_READ_FRAME_FAIL)
    {
        /* close output */
        if (ofmt_ctx_mp4 && !(ofmt_ctx_mp4->flags & AVFMT_NOFILE))
            avio_close(ofmt_ctx_mp4->pb);
        avformat_free_context(ofmt_ctx_mp4);
        return return_error;
    }
    //reconnect
    else {
        do{
            last_time=now_time;
            av_usleep(RECONNECT_TIME*AV_TIME_BASE);
            ret = avformat_open_input(&ifmt_ctx,rtsp_url, 0, &options);
            now_time=av_gettime();
            now_time/=10000;
            now_time%=stopsecond*100;
            if(now_time>last_time)//time over
            {
                /* close output */
                if (ofmt_ctx_mp4 && !(ofmt_ctx_mp4->flags & AVFMT_NOFILE))
                    avio_close(ofmt_ctx_mp4->pb);
                avformat_free_context(ofmt_ctx_mp4);
                return return_error;
            }
            //read packet again
            if(ret>=0)
            {
                if(ofmt_ctx_mp4!=NULL)
                    goto readpacket;
                else
                    goto openout;

            }
        }while(ret<0&&now_time>last_time);
    }
#endif
}



int slice_h265toh264_mp4(const char* slice_file,int start_time,void* fun_arg,write_stream writedata_dealfun,char* control_cmd)
{
    AVFormatContext *ifmt_ctx = NULL,*ofmt_ctx_mp4=NULL;
    int ret,index,video_index=-1,audio_index=-1,getframe_return=0,getpacket_return=0,fail_read_frame=0,fail_write_frame=0;
    const AVCodec* inCode_Video=NULL,*outCode_Video=NULL;
    AVCodecContext* inCode_VideoCTX=NULL,*outCode_VideoCTX=NULL;
    int64_t start_video_dts=0;
    int frist_out_packet=-1;
    bool first_lgnite=false;
    AVStream* out_video_stream=NULL;
    AVDictionary *opts = NULL;
    int64_t play_time=0,play_pts=0,last_cache_pts=0;
    media_error_code return_error=NO_ERRORS;
    int buffsize=32768;
    unsigned char* outbuffer=NULL;
    AVIOContext *avio_out=NULL;
    stream_cache_buff  write_cache_buff;
    AVPacket in_pkt;
    AVPacket out_pkt;
    AVFrame *pFrame = av_frame_alloc();
    AVDictionary *param = 0;


    if(slice_file==NULL||start_time<0||fun_arg==NULL||writedata_dealfun==NULL||control_cmd==NULL)
        return ERROR_ARG;
    memset(&write_cache_buff,0,sizeof (write_cache_buff));
    av_init_packet(&in_pkt);
    av_init_packet(&out_pkt);


    if ((ret = avformat_open_input(&ifmt_ctx,slice_file,0, 0)) < 0) {
        av_log(ifmt_ctx,AV_LOG_ERROR,"ERROR Fail to open slice file %s\n",slice_file);
        return_error = ERROR_OPEN_FILE_FAIL;
        goto end;
    }
    //find stream information in input
    if ((ret = avformat_find_stream_info(ifmt_ctx, 0)) < 0) {
        av_log(ifmt_ctx,AV_LOG_ERROR,"ERROR Fail to get slice information %s\n",slice_file);
        return_error = ERROR_STREAM_INFO_FAIL;
        goto end;
    }
    for(index=0; index<ifmt_ctx->nb_streams; index++) {
        if(ifmt_ctx->streams[index]->codecpar->codec_type==AVMEDIA_TYPE_VIDEO){
            video_index=index;
        }
        if(ifmt_ctx->streams[index]->codecpar->codec_type==AVMEDIA_TYPE_AUDIO){
            audio_index=index;
        }
    }
    av_dump_format(ifmt_ctx, 0,0, 0);
    if(video_index<0){
        av_log(ifmt_ctx,AV_LOG_ERROR,"ERROR no video stream in %s\n",slice_file);
        return_error = ERROR_NO_VIDEO_STREAM;
        goto end;
    }

    //compute start dts
    start_video_dts=ifmt_ctx->streams[video_index]->start_time+(int64_t) ( start_time/av_q2d(ifmt_ctx->streams[video_index]->time_base));
    ret = av_seek_frame(ifmt_ctx, video_index, start_video_dts, AVSEEK_FLAG_FRAME);
    if(ret<0){
        av_log(ifmt_ctx,AV_LOG_ERROR,"ERROR seek to %d in %s fail\n",start_time,slice_file);
        return_error = ERROR_SEEK_FAIL;
        goto end;
    }

    //Output
    avformat_alloc_output_context2(&ofmt_ctx_mp4, NULL, "mp4", NULL);
    if (!ofmt_ctx_mp4) {
        av_log(ifmt_ctx,AV_LOG_ERROR,"ERROR create output fail\n");
        return_error = ERROR_OPEN_OUTSTREAM_FAIL;
        goto end;
    }
    // set write to buffer
    outbuffer=(unsigned char*)av_malloc(buffsize);
    if(outbuffer==NULL)
    {
        av_log(ifmt_ctx,AV_LOG_ERROR,"ERROR Failed malloc buffer in %s \n",slice_file);
        return_error=ERROR_MEM_FAIL;
        goto end;

    }
    //three arg must be 1 for write
    write_cache_buff.cache_buff=(uint8_t*)av_malloc(CACHE_BIT_MAZ_WIDTH*CACHE_TIME_SECOND);
    write_cache_buff.pos=write_cache_buff.cache_buff;
    write_cache_buff.limit=write_cache_buff.cache_buff+CACHE_BIT_MAZ_WIDTH*CACHE_TIME_SECOND;
    write_cache_buff.write_size=0;
    avio_out =avio_alloc_context(outbuffer, buffsize,1,&write_cache_buff,NULL,write_packet,NULL);
    if(avio_out==NULL)
    {
        av_log(ifmt_ctx,AV_LOG_ERROR,"ERROR Failed malloc AVIOContext in %s \n",slice_file);
        av_free(outbuffer);
        return_error=ERROR_IOCONTENT_FAIL;
        goto end;

    }

    ofmt_ctx_mp4->pb=avio_out;
    ofmt_ctx_mp4->flags=AVFMT_FLAG_CUSTOM_IO;

    printf("start video\n");
    inCode_Video=avcodec_find_decoder(ifmt_ctx->streams[video_index]->codecpar->codec_id);
    if(inCode_Video==NULL)
    {
        av_log(ifmt_ctx,AV_LOG_ERROR,"ERROR in video stream code not see in %s\n",slice_file);
        return_error = ERROR_FIND_CODE_FAIL;
        goto end;
    }
    inCode_VideoCTX=avcodec_alloc_context3(inCode_Video);
    ret=avcodec_parameters_to_context(inCode_VideoCTX,ifmt_ctx->streams[video_index]->codecpar);
    printf("input bitrate is %ld,timebase is %d %d\n",inCode_VideoCTX->bit_rate,ifmt_ctx->streams[video_index]->time_base.den,ifmt_ctx->streams[video_index]->time_base.num);
    if(ret<0)
    {
        av_log(ifmt_ctx,AV_LOG_ERROR,"ERROR copy in code parameters to codecontext fail in %s\n",slice_file);
        return_error = ERROR_CREATE_CODECOTENT_FAIL;
        goto end;
    }
    if(avcodec_open2(inCode_VideoCTX, inCode_Video, NULL) < 0)
    {
        av_log(ifmt_ctx,AV_LOG_ERROR,"open in code fail in %s\n",slice_file);
        return_error = ERROR_OPEN_CODE_FAIL;
        goto end;
    }


    outCode_Video=avcodec_find_encoder(AV_CODEC_ID_H264);
    //outCode_Video=avcodec_find_encoder_by_name("libx264");
    if(outCode_Video==NULL){
        av_log(ifmt_ctx,AV_LOG_ERROR,"ERROR out video stream code not see in %s\n",slice_file);
        return_error = ERROR_FIND_CODE_FAIL;
        goto end;
    }

    outCode_VideoCTX=avcodec_alloc_context3(outCode_Video);

    outCode_VideoCTX->height=inCode_VideoCTX->height;
    outCode_VideoCTX->width=inCode_VideoCTX->width;
    outCode_VideoCTX->pix_fmt=AV_PIX_FMT_YUV420P;

    // 25frame a second
    outCode_VideoCTX->time_base.den=25;
    outCode_VideoCTX->time_base.num=1;
    outCode_VideoCTX->framerate.den=1;
    outCode_VideoCTX->framerate.num=25;
    outCode_VideoCTX->codec_id=outCode_Video->id;
    outCode_VideoCTX->codec_type=AVMEDIA_TYPE_VIDEO;
    outCode_VideoCTX->gop_size=50;//i frame interval
    outCode_VideoCTX->keyint_min=50;
    outCode_VideoCTX->bit_rate=inCode_VideoCTX->bit_rate*2;//h264 byterate
    outCode_VideoCTX->qmin = 10;
    outCode_VideoCTX->qmax = 51;
    outCode_VideoCTX->codec_tag = 0;

    av_dict_set(&param, "preset", "fast", 0);//x264 fast code
    av_dict_set(&param, "tune", "zerolatency", 0);//x264 no delay

    outCode_VideoCTX->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;//add sps,dps in mp4 ,no start code
    if(avcodec_open2(outCode_VideoCTX, outCode_Video,&param) < 0)
    {
        av_log(ifmt_ctx,AV_LOG_ERROR,"open out code fail in %s\n",slice_file);
        return_error = ERROR_OPEN_CODE_FAIL;
        goto end;
    }


    out_video_stream=avformat_new_stream(ofmt_ctx_mp4,outCode_Video);
    if (!out_video_stream) {
        av_log(ifmt_ctx,AV_LOG_ERROR,"ERROR Fail to create out stream in %s\n",slice_file);
        ret = ERROR_CREATE_AVSTREAM_FAIL;
        goto end;
    }

    ret=avcodec_parameters_from_context(out_video_stream->codecpar,outCode_VideoCTX);

    if(ret<0)
    {
        av_log(ifmt_ctx,AV_LOG_ERROR,"ERROR copy out code parameters from codecontext to stream fail in %s\n",slice_file);
        return_error = ERROR_CREATE_CODECOTENT_FAIL;
        goto end;
    }

#if 1
    //start audio
    if(audio_index>=0)
    {
        AVStream *in_stream = ifmt_ctx->streams[audio_index];
        const AVCodec* codec= avcodec_find_decoder(in_stream->codecpar->codec_id);
        AVStream *out_stream = avformat_new_stream(ofmt_ctx_mp4,codec);

        if (!out_stream) {
            av_log(ifmt_ctx,AV_LOG_ERROR,"ERROR Fail to create out stream in %s\n",slice_file);
            ret = ERROR_CREATE_AVSTREAM_FAIL;
            goto end;
        }

        //Copy the settings of AVCodecContext
        AVCodecContext* pCodecCtx = avcodec_alloc_context3(codec);
        ret = avcodec_parameters_to_context(pCodecCtx, in_stream->codecpar);
        if(ret<0){
            av_log(ifmt_ctx,AV_LOG_ERROR,"ERROR Fail to copy out stream codec in %s\n",slice_file);
            ret = ERROR_CREATE_AVSTREAM_FAIL;
            avcodec_free_context(&pCodecCtx);
            goto end;
        }
        pCodecCtx->codec_tag = 0;

        if (ofmt_ctx_mp4->oformat->flags & AVFMT_GLOBALHEADER) {
            pCodecCtx->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;
        }


        ret = avcodec_parameters_from_context(out_stream->codecpar, pCodecCtx);

        if (ret < 0) {
            av_log(ifmt_ctx,AV_LOG_ERROR,"ERROR Fail to copy out stream codec in %s\n",slice_file);
            ret = ERROR_CREATE_AVSTREAM_FAIL;
            avcodec_free_context(&pCodecCtx);
            goto end;

        }
        avcodec_free_context(&pCodecCtx);
    }
#endif


    // if (ofmt_ctx_mp4->oformat->flags & AVFMT_GLOBALHEADER) {
    //    ofmt_ctx_mp4->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;
    // }

    av_dump_format(ofmt_ctx_mp4, 0, 0, 1);

    av_dict_set(&opts, "movflags", "frag_keyframe+empty_moov+omit_tfhd_offset+faststart+separate_moof+disable_chpl+default_base_moof+dash", 0);
    if (avformat_write_header(ofmt_ctx_mp4, &opts) < 0) {
        printf( "Error occurred when write head to  output file\n");
        ret = ERROR_OPEN_OUTSTREAM_FAIL;
        goto end;
    }
    av_dict_free(&opts);

    //read an write
    play_time=av_gettime();
    while (*(control_cmd+1)=='r'||*(control_cmd+1)=='p') {
        //pause wait run or stop
        if(*(control_cmd+1)=='p')
        {
            printf("wait\n");
            while(*(control_cmd+1)=='p')
                av_usleep(500000);
            if(*(control_cmd+1)=='r')
            {
                //recompute start time;
                printf("continue\n");
                //play_time=av_gettime();
                frist_out_packet=-1;
                last_cache_pts=0;
            }
            else
                goto end;
        }
        ret = av_read_frame(ifmt_ctx, &in_pkt);

        if (ret< 0)
        {
            // slice end
            if(ret==AVERROR_EOF ){

                av_log(ifmt_ctx,AV_LOG_INFO,"INFO read a slice  %s end\n",slice_file);
                return_error = NO_ERRORS;
                ret=writedata_dealfun(fun_arg,write_cache_buff.cache_buff,write_cache_buff.write_size);
                if(ret<0)
                    av_log(ofmt_ctx_mp4,AV_LOG_ERROR,"ERROR Fail to send  %s  cache \n",slice_file);

                //send end to client
                write_cache_buff.write_size=FILE_EOF;
                ret=writedata_dealfun(fun_arg,write_cache_buff.cache_buff,write_cache_buff.write_size);
                if(ret<0)
                    av_log(ofmt_ctx_mp4,AV_LOG_ERROR,"ERROR Fail to send  %s  cache \n",slice_file);
                break;
            }
            else
            {
                av_log(ifmt_ctx,AV_LOG_ERROR,"ERROR fail read a frame in %s,error is %d\n",slice_file,ret);
                av_packet_unref(&in_pkt);
                fail_read_frame++;
                if(fail_read_frame>MAX_READ_FRAME_FAIL)
                {
                    return_error = ERROR_READ_FRAME_FAIL;
                    av_log(ifmt_ctx,AV_LOG_ERROR,"ERROR fail read frame over max in %s\n",slice_file);
                    break;
                }
                else
                    continue;
            }
        }
        else
            fail_read_frame=0;
        //if(in_pkt.stream_index!=video_index)
        if(in_pkt.stream_index!=video_index&&in_pkt.stream_index!=audio_index)
        {
            av_packet_unref(&in_pkt);
            continue;
        }
        //write audio
        if(in_pkt.stream_index==audio_index)
        {
            //Write
            // printf("write audio packet %ld\n",in_pkt.pts);
            if (av_interleaved_write_frame(ofmt_ctx_mp4, &in_pkt) < 0) {
                av_log(ofmt_ctx_mp4,AV_LOG_ERROR,"ERROR Fail to write packet %s,error is %d\n",slice_file,ret);
                fail_write_frame++;
                if(fail_write_frame>MAX_WRITE_FRAME_FAIL)
                {
                    av_packet_unref(&in_pkt);
                    av_log(ofmt_ctx_mp4,AV_LOG_ERROR,"ERROR Fail write packet over max %s\n",slice_file);
                    return_error = ERROR_WRITE_PKT_FAIL;
                    goto end;
                }
                else {
                    av_packet_unref(&in_pkt);
                    continue;
                }
            }
            else
                fail_write_frame=0;
            //printf("Write %8d frames to output file\n",frame_index);
            av_packet_unref(&in_pkt);
            continue;
        }

        //printf("get an inpacket\n");
        ret=avcodec_send_packet(inCode_VideoCTX,&in_pkt);
        if(ret<0)
        {
            av_packet_unref(&in_pkt);
            return_error = ERROR_DECODE_PACKET_FAIL;
            av_log(ifmt_ctx,AV_LOG_ERROR,"ERROR fail send packet in %s,error is %d\n",slice_file,ret);
            break;
        }
        getframe_return=0;
        while(getframe_return>=0)
        {
            getframe_return=avcodec_receive_frame(inCode_VideoCTX,pFrame);
            if (getframe_return == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
                break;
            } else if (getframe_return < 0) {
                return_error = ERROR_DECODE_PACKET_FAIL;
                av_log(ifmt_ctx,AV_LOG_ERROR,"ERROR fail avcodec_receive_frame packet in %s,error is %d\n",slice_file,ret);
                goto end;
            }
            else
            {
                //  printf("gframe ,send frame\n");
                ret = avcodec_send_frame(outCode_VideoCTX,pFrame);
                if(ret < 0)
                {
                    return_error = ERROR_ENCODE_PACKET_FAIL;
                    av_log(ifmt_ctx,AV_LOG_ERROR,"ERROR fail avcodec_send_frame packet in %s,error is %d\n",slice_file,ret);
                    goto end;
                }
                getpacket_return=0;
                while(getpacket_return>=0)
                {
                    getpacket_return=avcodec_receive_packet(outCode_VideoCTX,&out_pkt);
                    if( AVERROR(EAGAIN) == getpacket_return || AVERROR_EOF == getpacket_return)
                    {
                        // printf("wai out packet\n");
                        av_packet_unref(&out_pkt);
                        break;
                    }
                    else if(getpacket_return < 0 )
                    {
                        return_error = ERROR_ENCODE_PACKET_FAIL;
                        av_log(ifmt_ctx,AV_LOG_ERROR,"ERROR fail avcodec_receive_packet packet in %s,error is %d\n",slice_file,ret);
                        break;
                    }
                    // printf("get out packet,write\n");

                    //update pts,dts
                    AVRational time_base=out_video_stream->time_base;
                    AVRational time_base_q={1,AV_TIME_BASE};

                    out_pkt.pts = av_rescale_q_rnd(out_pkt.pts, ifmt_ctx->streams[video_index]->time_base, out_video_stream->time_base, AV_ROUND_NEAR_INF);
                    out_pkt.dts = av_rescale_q_rnd(out_pkt.dts, ifmt_ctx->streams[video_index]->time_base, out_video_stream->time_base, AV_ROUND_NEAR_INF);
                    out_pkt.duration = av_rescale_q(out_pkt.duration, ifmt_ctx->streams[video_index]->time_base, out_video_stream->time_base);
                    out_pkt.pos=-1;

                    if(frist_out_packet<0)
                    {
                        play_time=av_gettime();
                        play_pts=out_pkt.pts;
                        //printf("tanscode time is %lld\n",play_time);
                        frist_out_packet=1;
                    }
                    int64_t now_pts = av_rescale_q((out_pkt.pts-play_pts), time_base, time_base_q);
                    // printf("pkt pts is %lld,time is %lld\n",now_pts,av_gettime()-play_time);
                    if((now_pts-last_cache_pts)>=CACHE_TIME_SECOND*AV_TIME_BASE&&out_pkt.flags& AV_PKT_FLAG_KEY){
                        //mark lgnite start write
                        if(first_lgnite==false)
                        {
                            printf("mark start\n");
                            ret=writedata_dealfun(fun_arg,NULL,0);
                            if(ret<0)
                                av_log(ofmt_ctx_mp4,AV_LOG_ERROR,"ERROR Fail to mark  %s start write cache \n",slice_file);
                            first_lgnite=true;
                        }
                        ret=writedata_dealfun(fun_arg,write_cache_buff.cache_buff,write_cache_buff.write_size);
                        if(ret<0)
                            av_log(ofmt_ctx_mp4,AV_LOG_ERROR,"ERROR Fail to send  %s  cache \n",slice_file);
                        write_cache_buff.pos=write_cache_buff.cache_buff;
                        write_cache_buff.write_size=0;
                        last_cache_pts=now_pts;
                    }
                    // printf("wait some\n");
                    if (now_pts > (av_gettime()-play_time))
                        av_usleep(now_pts - (av_gettime()-play_time));
                    //printf("write video packet %ld\n",out_pkt.pts);

                    ret = av_interleaved_write_frame(ofmt_ctx_mp4, &out_pkt);

                    if (ret < 0)
                    {
                        av_log(ofmt_ctx_mp4,AV_LOG_ERROR,"ERROR Fail to write packet %s,error is %d\n",slice_file,ret);
                        fail_write_frame++;
                        if(fail_write_frame>MAX_WRITE_FRAME_FAIL)
                        {
                            av_packet_unref(&out_pkt);
                            av_log(ofmt_ctx_mp4,AV_LOG_ERROR,"ERROR Fail write packet over max %s\n",slice_file);
                            return_error = ERROR_WRITE_PKT_FAIL;
                            goto end;
                        }
                        else {
                            continue;
                        }
                    }
                    else
                        fail_write_frame=0;
                    av_packet_unref(&out_pkt);

                }
            }
        }
    }
    //Write file trailer
    av_write_trailer(ofmt_ctx_mp4);

end:
    avformat_close_input(&ifmt_ctx);
    if(inCode_VideoCTX!=NULL)
        avcodec_free_context(&inCode_VideoCTX);
    if(outCode_VideoCTX!=NULL)
        avcodec_free_context(&outCode_VideoCTX);
    avio_context_free(&avio_out);
    avformat_free_context(ofmt_ctx_mp4);
    if(outbuffer!=NULL)
        av_free(outbuffer);
    if(write_cache_buff.cache_buff!=NULL)
        av_free(write_cache_buff.cache_buff);
    return return_error;
}


/*
send rtsp req to ipc,recv video stream and audio stream and code and decode to save to format mp4 slice
rtsp_url rtsp url
filename slice name
control_cmd write control end
stopsecond stop time
video_type  code type
recordtime  real record time
*/
int rtsp_slice_codeautostop(const char * rtsp_url,const  char* filename, char * control_cmd,int stopsecond,int* video_type,int* recordtime,int dropInterval, custom_cbk cbk/*=NULL*/, void* arg/*=NULL*/)
{

    int ret,fail_read_frame=0,fail_write_frame=0,next_stop_time=0,getframe_return=0,getpacket_return=0,frame_num=0;
    bool first_video=false,first_audio=false;
    unsigned int index,video_index=-1,audio_index=-1;
    AVPacket pkt;
    AVDictionary *opts = NULL;
    AVFormatContext *ifmt_ctx = NULL,*ofmt_ctx_mp4=NULL;
    media_error_code return_error=NO_ERRORS;
    int64_t start_time=0,now_time=0,record_all_time=0;
    AVCodecContext* inCode_VideoCTX=NULL,*outCode_VideoCTX=NULL;
    AVDictionary *param = 0;
    int videoCode_type=3;
    AVFrame *pFrame = av_frame_alloc();
    AVPacket out_pkt;
    av_init_packet(&out_pkt);
    //set log
    av_log_set_callback(custom_output);
    av_log_set_level(AV_LOG_ERROR);
    if(rtsp_url==NULL||control_cmd==NULL||stopsecond<=0||video_type==NULL||recordtime==NULL||dropInterval<=0)
        return ERROR_ARG;

    AVDictionary* options = NULL;
    av_dict_set(&options,"rtsp_transport", "tcp", 0);
    av_dict_set(&options,"stimeout","3000000",0);

    //Input
    if ((ret = avformat_open_input(&ifmt_ctx,rtsp_url, 0, &options)) < 0) {
        av_log(ifmt_ctx,AV_LOG_ERROR, "ERROR,Could not open rtsp url %s\n",rtsp_url);
        return_error=ERROR_OPEN_URL_FAIL;
        goto end;
    }
    //find stream information in input
    if ((ret = avformat_find_stream_info(ifmt_ctx, 0)) < 0) {
        av_log(ifmt_ctx,AV_LOG_ERROR,"ERROR Failed to retrieve input %s stream information \n",rtsp_url);
        return_error=ERROR_STREAM_INFO_FAIL;
        goto end;
    }
    //pritnf input stream format
    av_dump_format(ifmt_ctx, 0, rtsp_url, 0);

    //Output
    avformat_alloc_output_context2(&ofmt_ctx_mp4, NULL, "mp4", filename);
    if (!ofmt_ctx_mp4) {
        av_log(ifmt_ctx,AV_LOG_ERROR,"ERROR create  output fail in %s\n",filename);
        return_error = ERROR_OPEN_OUTSTREAM_FAIL;
        goto end;
    }
    //create streams in output
    for (index = 0; index < ifmt_ctx->nb_streams; index++) {
        AVStream *in_stream = ifmt_ctx->streams[index];
        if(in_stream->codecpar->codec_type==AVMEDIA_TYPE_VIDEO)
            video_index=index;
        else if(in_stream->codecpar->codec_type==AVMEDIA_TYPE_AUDIO)
            audio_index=index;
        else
            continue;
        const AVCodec* codec= avcodec_find_decoder(in_stream->codecpar->codec_id);
        //Copy the settings of AVCodecContext
        AVCodecContext* pCodecCtx = avcodec_alloc_context3(codec);
        ret = avcodec_parameters_to_context(pCodecCtx, in_stream->codecpar);
        if(ret<0){
            av_log(ifmt_ctx,AV_LOG_ERROR,"ERROR Fail to copy out stream codec in %s\n",rtsp_url);
            return_error = ERROR_CREATE_AVSTREAM_FAIL;
            avcodec_free_context(&pCodecCtx);
            goto end;
        }
        if(in_stream->codecpar->codec_type==AVMEDIA_TYPE_VIDEO) {
            inCode_VideoCTX=pCodecCtx;
            if(avcodec_open2(inCode_VideoCTX, codec, NULL) < 0) {
                av_log(ifmt_ctx,AV_LOG_ERROR,"open in code fail in %s at rtsp_slice_codeautostop\n",rtsp_url);
                return_error = ERROR_OPEN_CODE_FAIL;
                avcodec_free_context(&pCodecCtx);
                goto end;
            }
            const AVCodec*  outCode_Video=avcodec_find_encoder(AV_CODEC_ID_H264);
            outCode_VideoCTX=avcodec_alloc_context3(outCode_Video);
            outCode_VideoCTX->height=inCode_VideoCTX->height;
            outCode_VideoCTX->width=inCode_VideoCTX->width;
            outCode_VideoCTX->pix_fmt=AV_PIX_FMT_YUV420P;
            outCode_VideoCTX->codec_id=outCode_Video->id;
            outCode_VideoCTX->codec_type=AVMEDIA_TYPE_VIDEO;
            outCode_VideoCTX->time_base.den=25/dropInterval;
            outCode_VideoCTX->time_base.num=1;
            outCode_VideoCTX->framerate.den=1;
            outCode_VideoCTX->framerate.num=25/dropInterval;

            printf("width %d\n",inCode_VideoCTX->height);
            if(inCode_VideoCTX->height==480)
                outCode_VideoCTX->bit_rate=1200000/dropInterval;
            else if(inCode_VideoCTX->height==720)
                outCode_VideoCTX->bit_rate=2800000/dropInterval;
            else if(inCode_VideoCTX->height==1080)
                outCode_VideoCTX->bit_rate=6400000/dropInterval;

            if(dropInterval!=25)
                outCode_VideoCTX->gop_size=outCode_VideoCTX->time_base.den*2;//i frame interval
            else
                outCode_VideoCTX->gop_size=outCode_VideoCTX->time_base.den;//i frame interval
            outCode_VideoCTX->keyint_min=outCode_VideoCTX->gop_size;
            outCode_VideoCTX->codec_tag = 0;
            outCode_VideoCTX->qmin = 10;
            outCode_VideoCTX->qmax = 40;
            outCode_VideoCTX->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;
            av_dict_set(&param, "preset", "fast", 0);//x264 fast code
            av_dict_set(&param, "tune", "zerolatency", 0);//x264 no delay
            outCode_VideoCTX->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;//add sps,dps in mp4 ,no start code
            if(avcodec_open2(outCode_VideoCTX, outCode_Video,&param) < 0)
            {
                av_log(ifmt_ctx,AV_LOG_ERROR,"open out code fail in %s at rtsp_slice_codeautostop\n",rtsp_url);
                return_error = ERROR_OPEN_CODE_FAIL;
                goto end;
            }
            AVStream *out_stream = avformat_new_stream(ofmt_ctx_mp4,outCode_Video);
            if (!out_stream) {
                av_log(ifmt_ctx,AV_LOG_ERROR,"ERROR Fail to create out stream in %s\n",rtsp_url);
                return_error = ERROR_CREATE_AVSTREAM_FAIL;
                goto end;
            }
            ret = avcodec_parameters_from_context(out_stream->codecpar, outCode_VideoCTX);
            out_stream->time_base.den=in_stream->time_base.den;
            out_stream->time_base.num=in_stream->time_base.num;
            //printf("in q is ",inCode_VideoCTX->priv_data);
            printf("incode timebase %d %d,out code timebase %d\n",in_stream->time_base.den,in_stream->time_base.num,out_stream->time_base.den);
            if (ret < 0) {
                av_log(ifmt_ctx,AV_LOG_ERROR,"ERROR Fail to copy out stream codec in %s\n",rtsp_url);
                return_error = ERROR_CREATE_AVSTREAM_FAIL;
                avcodec_free_context(&outCode_VideoCTX);
                avcodec_free_context(&pCodecCtx);
                goto end;
            }
        }
        else {
            pCodecCtx->codec_tag = 0;
            if (ofmt_ctx_mp4->oformat->flags & AVFMT_GLOBALHEADER) {
                pCodecCtx->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;
            }
            AVStream *out_stream = avformat_new_stream(ofmt_ctx_mp4,codec);
            if (!out_stream) {
                av_log(ifmt_ctx,AV_LOG_ERROR,"ERROR Fail to create out stream in %s\n",rtsp_url);
                return_error = ERROR_CREATE_AVSTREAM_FAIL;
                goto end;
            }
            ret = avcodec_parameters_from_context(out_stream->codecpar, pCodecCtx);
            if (ret < 0) {
                av_log(ifmt_ctx,AV_LOG_ERROR,"ERROR Fail to copy out stream codec in %s\n",rtsp_url);
                return_error = ERROR_CREATE_AVSTREAM_FAIL;
                avcodec_free_context(&pCodecCtx);
                goto end;
            }
            avcodec_free_context(&pCodecCtx);
        }
    }
    //get video type
    if(ifmt_ctx->streams[video_index]->codecpar->codec_id==AV_CODEC_ID_H264)
        videoCode_type=0;
    else if(ifmt_ctx->streams[video_index]->codecpar->codec_id==AV_CODEC_ID_HEVC)
        videoCode_type=1;
    else
        videoCode_type=2;
    *video_type=videoCode_type;
    printf("get video type is %d\n",videoCode_type);
    //print out stream format
    av_dump_format(ofmt_ctx_mp4, 0, filename, 1);

    //Open output file
    if (NULL != filename) {
        if (!(ofmt_ctx_mp4->oformat->flags & AVFMT_NOFILE)) {
            // 创建和初始化一个和该URL相关的AVIOContext
            ret = avio_open2(&ofmt_ctx_mp4->pb, filename, AVIO_FLAG_READ_WRITE, NULL, NULL);
            if (ret < 0) {
                LOG("ERROR", "avio_open2 failed %d\n", ret);
                av_log(ofmt_ctx_mp4, AV_LOG_ERROR, "avio_open function failed %d \"%s\"\n", ret, filename);
                return_error = ERROR_OPEN_OUTSTREAM_FAIL;
                goto end;
            }
        }
    }
    else {
        unsigned char* buffer = (uint8_t*)av_malloc(BUFF_SIZE);
        ofmt_ctx_mp4->pb = avio_alloc_context(buffer, BUFF_SIZE, 1, arg, NULL, cbk, NULL);
        if (NULL == ofmt_ctx_mp4->pb) {
            av_log(ofmt_ctx_mp4, AV_LOG_ERROR, "avio_alloc_context function failed %d\n");
            LOG("ERROR", "avio_alloc_context failed.\n");
        }
        ofmt_ctx_mp4->flags = AVFMT_FLAG_CUSTOM_IO;
    }
    //av_dict_set(&opts, "movflags", "frag_keyframe+empty_moov", 0);
    if (avformat_write_header(ofmt_ctx_mp4, &opts) < 0) {
        av_log( ofmt_ctx_mp4,AV_LOG_ERROR,"Error occurred when write output file header\n");
        return_error=ERROR_OPEN_OUTSTREAM_FAIL;
        goto end;
    }
    av_dict_free(&opts);
    while (*(control_cmd+1)=='r') {
        if ((ret=av_read_frame(ifmt_ctx, &pkt))< 0){
            av_log(ifmt_ctx,AV_LOG_ERROR,"ERROR fail read a frame in %s,error is %d\n",rtsp_url,ret);
            av_packet_unref(&pkt);
            fail_read_frame++;
            if(fail_read_frame>MAX_READ_FRAME_FAIL)
            {
                return_error = ERROR_READ_FRAME_FAIL;
                av_log(ifmt_ctx,AV_LOG_ERROR,"ERROR fail read frame over max in %s\n",rtsp_url);
                break;
            }
            else
                continue;
        }
        else {
            fail_read_frame=0;
        }
        if(pkt.stream_index!=video_index&&pkt.stream_index!=audio_index) {
            av_packet_unref(&pkt);
            continue;
        }
        // set first packet pts 0 for sometimes first dts is lager than others
        if(first_video==0&&pkt.stream_index==video_index) {
            pkt.dts=0;
            pkt.pts=0;
            first_video=true;
            start_time=av_gettime();
            next_stop_time=stopsecond-(start_time/AV_TIME_BASE)%stopsecond;
            printf("next stop time is %d\n",next_stop_time);
        }
        if(first_audio==0&&pkt.stream_index==audio_index) {
            pkt.dts=0;
            pkt.pts=0;
            first_audio=true;
        }
        //printf("read a frame indexis %d %ld\n",pkt.stream_index,pkt.dts);
        //if time over and i frame,stop
        if(pkt.stream_index==video_index&&pkt.flags& AV_PKT_FLAG_KEY){
            now_time=av_gettime();
            //printf("now_time is %d \n",(now_time-start_time)/AV_TIME_BASE);
            if((now_time-start_time)/AV_TIME_BASE>next_stop_time) {
                AVRational time_base=ifmt_ctx->streams[video_index]->time_base;
                AVRational time_base_q={1,AV_TIME_BASE};
                record_all_time = av_rescale_q((pkt.pts), time_base, time_base_q);
                printf("record time is %lld\n",record_all_time);
                *recordtime=record_all_time/AV_TIME_BASE;
                av_packet_unref(&pkt);
                return_error=NO_ERRORS;
                break;
            }
        }
        if(pkt.stream_index==audio_index) {
            //Write
            // printf("write audio packet %ld\n",in_pkt.pts);
            if (av_interleaved_write_frame(ofmt_ctx_mp4, &pkt) < 0) {
                av_log(ofmt_ctx_mp4,AV_LOG_ERROR,"ERROR Fail to write packet %s,error is %d\n",rtsp_url,ret);
                fail_write_frame++;
                if(fail_write_frame>MAX_WRITE_FRAME_FAIL) {
                    av_packet_unref(&pkt);
                    av_log(ofmt_ctx_mp4,AV_LOG_ERROR,"ERROR Fail write packet over max %s\n",rtsp_url);
                    return_error = ERROR_WRITE_PKT_FAIL;
                    goto end;
                }
                else {
                    av_packet_unref(&pkt);
                    continue;
                }
            }
            else
                fail_write_frame=0;
            //printf("Write %8d frames to output file\n",frame_index);
            av_packet_unref(&pkt);
            continue;
        }
        //  printf("get an inpacket %d\n",pkt.stream_index);
        ret=avcodec_send_packet(inCode_VideoCTX,&pkt);
        if(ret<0) {
            av_packet_unref(&pkt);
            return_error = ERROR_DECODE_PACKET_FAIL;
            av_log(ifmt_ctx,AV_LOG_ERROR,"ERROR fail send packet in %s,error is %d\n",rtsp_url,ret);
            break;
        }
        getframe_return=0;
        while(getframe_return>=0) {
            getframe_return=avcodec_receive_frame(inCode_VideoCTX,pFrame);
            if (getframe_return == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
                break;
            } else if (getframe_return < 0) {
                return_error = ERROR_DECODE_PACKET_FAIL;
                av_log(ifmt_ctx,AV_LOG_ERROR,"ERROR fail avcodec_receive_frame packet in %s,error is %d\n",rtsp_url,ret);
                goto end;
            }
            else {
                frame_num++;
                if(frame_num%dropInterval!=0)
                    continue;
                //printf("gframe ,send frame\n");
                ret = avcodec_send_frame(outCode_VideoCTX,pFrame);
                if(ret < 0) {
                    return_error = ERROR_ENCODE_PACKET_FAIL;
                    av_log(ifmt_ctx,AV_LOG_ERROR,"ERROR fail avcodec_send_frame packet in %s,error is %d\n",rtsp_url,ret);
                    goto end;
                }
                // printf("get packet\n");
                getpacket_return=0;
                while(getpacket_return>=0) {
                    getpacket_return=avcodec_receive_packet(outCode_VideoCTX,&out_pkt);
                    if( AVERROR(EAGAIN) == getpacket_return || AVERROR_EOF == getpacket_return) {
                        // printf("wai out packet\n");
                        av_packet_unref(&out_pkt);
                        break;
                    }
                    else if(getpacket_return < 0 ) {
                        return_error = ERROR_ENCODE_PACKET_FAIL;
                        av_log(ifmt_ctx,AV_LOG_ERROR,"ERROR fail avcodec_receive_packet packet in %s,error is %d\n",rtsp_url,ret);
                        break;
                    }
                    //printf("get out packet,write\n");
                    //update pts
                    // printf("%d %d %d %d \n",ifmt_ctx->streams[video_index]->time_base.den,ifmt_ctx->streams[video_index]->time_base.num,out_timebase.den,out_timebase.num);
                    //out_pkt.pts = av_rescale_q_rnd(out_pkt.pts, ifmt_ctx->streams[video_index]->time_base, out_timebase, AV_ROUND_NEAR_INF);
                    // out_pkt.dts = av_rescale_q_rnd(out_pkt.dts, ifmt_ctx->streams[video_index]->time_base, out_timebase, AV_ROUND_NEAR_INF);
                    // out_pkt.duration = av_rescale_q(out_pkt.duration, ifmt_ctx->streams[video_index]->time_base, out_timebase);
                    // out_pkt.pos=-1;
                    //printf("time stamp %ld\n",out_pkt.pts);
                    ret = av_interleaved_write_frame(ofmt_ctx_mp4, &out_pkt);
                    if (ret < 0) {
                        av_log(ofmt_ctx_mp4,AV_LOG_ERROR,"ERROR Fail to write packet %s,error is %d\n",rtsp_url,ret);
                        fail_write_frame++;
                        if(fail_write_frame>MAX_WRITE_FRAME_FAIL) {
                            av_packet_unref(&out_pkt);
                            av_log(ofmt_ctx_mp4,AV_LOG_ERROR,"ERROR Fail write packet over max %s\n",rtsp_url);
                            return_error = ERROR_WRITE_PKT_FAIL;
                            goto end;
                        }
                        else {
                            continue;
                        }
                    }
                    else
                        fail_write_frame=0;
                    av_packet_unref(&out_pkt);
                }
            }
        }
    }
    //Write file trailer
    av_write_trailer(ofmt_ctx_mp4);
end:
    avformat_close_input(&ifmt_ctx);
    // close output
    if (NULL != ofmt_ctx_mp4) {
        if (!(ofmt_ctx_mp4->flags & AVFMT_NOFILE))
        if(NULL != ofmt_ctx_mp4->pb)
            avio_close(ofmt_ctx_mp4->pb);
        if (ofmt_ctx_mp4->flags & AVFMT_FLAG_CUSTOM_IO) {
            if (NULL != ofmt_ctx_mp4->pb->buffer)
                av_free(ofmt_ctx_mp4->pb->buffer);
        }
        avformat_free_context(ofmt_ctx_mp4);
    }
    if(inCode_VideoCTX!=NULL)
        avcodec_free_context(&inCode_VideoCTX);
    if(outCode_VideoCTX!=NULL)
        avcodec_free_context(&outCode_VideoCTX);
    return return_error;
}

/*
*/

int fill_iobuffer(void * opaque,uint8_t *buf, int bufsize)
{
    int* sock=(int*)opaque;
    int size=recv(*sock,(char*)buf,bufsize,0);
    //  printf("recv %d\n",size);
    return size;
}

/*
send rtsp req to ipc,recv video stream and audio stream and send to buff
tcp_ip tcp ip
port  tcp port
fun_arg write funtion arg
writedata_dealfun write funtion pointer
*/
int tcp_h264_stream(const char * tcp_ip,int port, void* fun_arg,write_stream writedata_dealfun,char* control_cmd)
{
    int ret,fail_read_frame=0,fail_write_frame=0;
    bool first_video=false,first_audio=false,first_lgnite=false,first_stream_type=false;
    unsigned int index,video_index=-1,audio_index=-1;
    AVPacket pkt;
    AVDictionary *opts = NULL;
    AVFormatContext *ifmt_ctx = NULL,*ofmt_ctx_mp4=NULL;
    media_error_code return_error=NO_ERRORS;
    int buffsize=32768;
    unsigned char* outbuffer=NULL;
    AVIOContext *avio_out=NULL;
    stream_cache_buff  write_cache_buff;
    int64_t last_cache_pts=0,now_pts=0;
    bool isSend=false;
    char lastcmd='r';
    long frame_index=0;
    const AVInputFormat* ifmt = NULL;
    AVIOContext* avio_in = NULL;
    unsigned char* inbuffer = NULL;
    int sock = 0;

    if(tcp_ip==NULL||fun_arg==NULL||writedata_dealfun==NULL||control_cmd==NULL||port<=0)
        return ERROR_ARG;

    memset(&write_cache_buff,0,sizeof (write_cache_buff));
    //set log
    // av_log_set_callback(custom_output);
    //av_log_set_level(AV_LOG_ERROR);

    ifmt_ctx= avformat_alloc_context();
    if(ifmt_ctx==NULL)
    {
        av_log(ifmt_ctx,AV_LOG_ERROR, "ERROR avformat_alloc_context fail in %s \n",tcp_ip);
        return_error=ERROR_MEM_FAIL;
        goto end;
    }
    //tcp connect,and recv data
#ifdef _WIN32
    WORD	wVersionRequested;
    WSADATA wsaData;
    wVersionRequested	=	MAKEWORD(2,2);

    ret	=	WSAStartup(wVersionRequested,&wsaData);
    if(ret!=0)
    {
        return_error=ERROR_OPEN_URL_FAIL;
        goto end;
    }
#else
#endif
    sock=socket(AF_INET,SOCK_STREAM,0);
    struct sockaddr_in addrSrc;
    memset(&addrSrc,0,sizeof(struct sockaddr_in));

    addrSrc.sin_family=AF_INET;
    addrSrc.sin_port=htons(port);
    addrSrc.sin_addr.s_addr=inet_addr(tcp_ip);

    ret = connect(sock,(sockaddr*)&addrSrc,sizeof(sockaddr));

    if(ret<0)
    {
        av_log(ifmt_ctx,AV_LOG_ERROR, "ERROR,Could not open tcp address %s %d,error is %d\n",tcp_ip,port,ret);
        return_error=ERROR_OPEN_URL_FAIL;
        goto end;
    }

    //read from recv tcp buff
    inbuffer=(unsigned char *)av_malloc(buffsize);
    if(inbuffer==NULL)
    {
        av_log(ifmt_ctx,AV_LOG_ERROR,"ERROR Failed malloc buffer in %s \n",tcp_ip);
        return_error=ERROR_MEM_FAIL;
        goto end;

    }
    avio_in =avio_alloc_context(inbuffer, buffsize,0,&sock,fill_iobuffer,NULL,NULL);
    if(avio_in==NULL)
    {
        av_log(ifmt_ctx,AV_LOG_ERROR,"ERROR Failed malloc AVIOContext in %s \n",tcp_ip);
        av_free(inbuffer);
        return_error=ERROR_IOCONTENT_FAIL;
        goto end;

    }
    ifmt_ctx->pb=avio_in;
    
    //Input
    ifmt=av_find_input_format("h264");
    if ((ret = avformat_open_input(&ifmt_ctx,NULL, (AVInputFormat*)ifmt, NULL)) < 0) {
        av_log(ifmt_ctx,AV_LOG_ERROR, "ERROR,Could not open tcp %s,error is %d\n",tcp_ip,ret);
        return_error=ERROR_OPEN_URL_FAIL;
        goto end;
    }

    fail_read_frame=0;
    fail_write_frame=0;
    first_video=false;
    first_audio=false;
    first_lgnite=false;
    //find stream information in input
    if ((ret = avformat_find_stream_info(ifmt_ctx, 0)) < 0) {
        av_log(ifmt_ctx,AV_LOG_ERROR,"ERROR Failed to retrieve input %s stream information,error is %d  \n",tcp_ip,ret);
        return_error=ERROR_STREAM_INFO_FAIL;
        goto end;
    }

    //pritnf input stream format
    av_dump_format(ifmt_ctx, 0, tcp_ip, 0);

    //Output
    avformat_alloc_output_context2(&ofmt_ctx_mp4, NULL, "mp4", NULL);
    if (!ofmt_ctx_mp4) {
        av_log(ifmt_ctx,AV_LOG_ERROR,"ERROR create  output fail\n");
        return_error = ERROR_OPEN_OUTSTREAM_FAIL;
        goto end;
    }

    // set write to buffer
    outbuffer=(uint8_t*)av_malloc(buffsize);
    if(outbuffer==NULL)
    {
        av_log(ifmt_ctx,AV_LOG_ERROR,"ERROR Failed malloc buffer in %s \n",tcp_ip);
        return_error=ERROR_MEM_FAIL;
        goto end;

    }
    //threearg must be 1 for write
    // avio_out =avio_alloc_context(outbuffer, buffsize,1,fun_arg,NULL,writedata_dealfun,NULL);

    write_cache_buff.cache_buff=(uint8_t*)av_malloc(CACHE_BIT_MAZ_WIDTH*CACHE_TIME_SECOND);
    write_cache_buff.pos=write_cache_buff.cache_buff;
    write_cache_buff.limit=write_cache_buff.cache_buff+CACHE_BIT_MAZ_WIDTH*CACHE_TIME_SECOND;
    write_cache_buff.write_size=0;
    avio_out =avio_alloc_context(outbuffer, buffsize,1,&write_cache_buff,NULL,write_packet,NULL);
    if(avio_out==NULL)
    {
        av_log(ifmt_ctx,AV_LOG_ERROR,"ERROR Failed malloc AVIOContext in %s \n",tcp_ip);
        av_free(outbuffer);
        return_error=ERROR_IOCONTENT_FAIL;
        goto end;

    }


    ofmt_ctx_mp4->pb=avio_out;
    ofmt_ctx_mp4->flags=AVFMT_FLAG_CUSTOM_IO;

    //create streams in output
    for (index = 0; index < ifmt_ctx->nb_streams; index++) {
        AVStream *in_stream = ifmt_ctx->streams[index];
        if(in_stream->codecpar->codec_type==AVMEDIA_TYPE_VIDEO)
            video_index=index;
        else if(in_stream->codecpar->codec_type==AVMEDIA_TYPE_AUDIO)
            audio_index=index;
        else {
            continue;
        }
        const AVCodec* codec= avcodec_find_decoder(in_stream->codecpar->codec_id);
        AVStream *out_stream = avformat_new_stream(ofmt_ctx_mp4,codec);

        if (!out_stream) {
            av_log(ifmt_ctx,AV_LOG_ERROR,"ERROR Fail to create out stream in %s\n",tcp_ip);
            return_error = ERROR_CREATE_AVSTREAM_FAIL;
            goto end;
        }

        //Copy the settings of AVCodecContext
        AVCodecContext* pCodecCtx = avcodec_alloc_context3(codec);
        ret = avcodec_parameters_to_context(pCodecCtx, in_stream->codecpar);
        if(ret<0){
            av_log(ifmt_ctx,AV_LOG_ERROR,"ERROR Fail to copy out stream codec in %s\n",tcp_ip);
            return_error = ERROR_CREATE_AVSTREAM_FAIL;
            avcodec_free_context(&pCodecCtx);
            goto end;
        }
        pCodecCtx->codec_tag = 0;

        if (ofmt_ctx_mp4->oformat->flags & AVFMT_GLOBALHEADER) {
            pCodecCtx->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;
        }

        ret = avcodec_parameters_from_context(out_stream->codecpar, pCodecCtx);

        if (ret < 0) {
            av_log(ifmt_ctx,AV_LOG_ERROR,"ERROR Fail to copy out stream codec in %s\n",tcp_ip);
            return_error = ERROR_CREATE_AVSTREAM_FAIL;
            avcodec_free_context(&pCodecCtx);
            goto end;

        }
        avcodec_free_context(&pCodecCtx);

    }


    //print out stream format
    av_dump_format(ofmt_ctx_mp4, 0, 0, 1);

    //send video only or video and audio to client
    if(first_stream_type==false)
    {
        if(audio_index==-1)
            write_cache_buff.write_size=VIDEO_ONLY;
        else
            write_cache_buff.write_size=VIDEO_AND_AUDIO;
        ret=writedata_dealfun(fun_arg,write_cache_buff.cache_buff,write_cache_buff.write_size);
        write_cache_buff.pos=write_cache_buff.cache_buff;
        write_cache_buff.write_size=0;
        first_stream_type=true;
    }
    //fmp4
    av_dict_set(&opts, "movflags", "frag_keyframe+empty_moov+omit_tfhd_offset+faststart+separate_moof+disable_chpl+default_base_moof+dash", 0);
    //Write file header
    if (avformat_write_header(ofmt_ctx_mp4, &opts) < 0) {
        printf( "Error occurred when write head to  output file\n");
        return_error = ERROR_OPEN_OUTSTREAM_FAIL;
        goto end;
    }
    av_dict_free(&opts);

    //read an write
    //r run i send i frame only
    while (*(control_cmd+1)=='r'||*(control_cmd+1)=='i') {
        if ((ret=av_read_frame(ifmt_ctx, &pkt))< 0){
            av_log(ifmt_ctx,AV_LOG_ERROR,"ERROR fail read a frame in %s,error is %d\n",tcp_ip,ret);
            av_packet_unref(&pkt);
            fail_read_frame++;
            if(fail_read_frame>MAX_READ_FRAME_FAIL)
            {
                return_error = ERROR_READ_FRAME_FAIL;
                av_log(ifmt_ctx,AV_LOG_ERROR,"ERROR fail read frame over max in %s\n",tcp_ip);
                break;
            }
            else
                continue;
        }
        else {
            fail_read_frame=0;
        }
        //not deal not video audio data
        if(pkt.stream_index!=video_index&&pkt.stream_index!=audio_index)
        {
            av_packet_unref(&pkt);
            continue;
        }
        // set first packet pts 0 for sometimes first dts is lager than others
        if(first_video==0&&pkt.stream_index==video_index)
        {
            pkt.dts=0;
            pkt.pts=0;
            first_video=true;
        }

        if(first_audio==0&&pkt.stream_index==audio_index)
        {
            pkt.dts=0;
            pkt.pts=0;
            first_audio=true;
        }

        //Write
        // printf("read a frame indexis %d %ld\n",pkt.stream_index,pkt.dts);

        //Simple Write PTS
        if(pkt.pts==AV_NOPTS_VALUE){
            //Write PTS
            AVRational time_base1=ifmt_ctx->streams[video_index]->time_base;
            //Duration between 2 frames (us)
            int64_t calc_duration=(double)AV_TIME_BASE/av_q2d(ifmt_ctx->streams[video_index]->r_frame_rate);
            //Parameters
            pkt.pts=(double)(frame_index*calc_duration)/(double)(av_q2d(time_base1)*AV_TIME_BASE);
            pkt.dts=pkt.pts;
            pkt.duration=(double)calc_duration/(double)(av_q2d(time_base1)*AV_TIME_BASE);
            frame_index++;
        }
#if 1
        AVStream* in_stream  = ifmt_ctx->streams[pkt.stream_index];
        AVStream* out_stream = ofmt_ctx_mp4->streams[pkt.stream_index];
        // copy packet */
        pkt.pts = av_rescale_q_rnd(pkt.pts, in_stream->time_base, out_stream->time_base, (AVRounding)(AV_ROUND_NEAR_INF|AV_ROUND_PASS_MINMAX));
        pkt.dts = av_rescale_q_rnd(pkt.dts, in_stream->time_base, out_stream->time_base, (AVRounding)(AV_ROUND_NEAR_INF|AV_ROUND_PASS_MINMAX));
        pkt.duration = av_rescale_q(pkt.duration, in_stream->time_base, out_stream->time_base);
        pkt.pos = -1;
#endif
        //printf("Write %8d frames to output file\n",frame_index);
        //if cache data time lager than CACHE_TIME_SECOND,send cache data
        if(pkt.stream_index==video_index){
            //change mode only after send  packts
            if(isSend==true)
            {
                if(*(control_cmd+1)!=lastcmd)
                    lastcmd=*(control_cmd+1);
                isSend=false;
            }
            if(lastcmd=='i'&&!(pkt.flags&AV_PKT_FLAG_KEY))
            {
                av_packet_unref(&pkt);
                continue;
            }
            AVRational time_base=ifmt_ctx->streams[video_index]->time_base;
            AVRational time_base_q={1,AV_TIME_BASE};
            now_pts = av_rescale_q((pkt.pts-last_cache_pts), time_base, time_base_q);
            //printf("now_pts is %ld\n",now_pts);
            //add i frame to cache top
            //if(now_pts>=CACHE_TIME_SECOND*AV_TIME_BASE&&pkt.flags& AV_PKT_FLAG_KEY){
            if(pkt.flags& AV_PKT_FLAG_KEY){
                //printf("index %ld\n",frame_index);
                //if(now_pts>=CACHE_TIME_SECOND*AV_TIME_BASE){
                //mark lgnite start write
                if(first_lgnite==false)
                {
                    printf("mark start\n");
                    ret=writedata_dealfun(fun_arg,NULL,0);
                    if(ret<0)
                        av_log(ofmt_ctx_mp4,AV_LOG_ERROR,"ERROR Fail to mark  %s start write cache \n",tcp_ip);
                    first_lgnite=true;
                }
                // printf("write cache\n");
                ret=writedata_dealfun(fun_arg,write_cache_buff.cache_buff,write_cache_buff.write_size);
                if(ret<0)
                    av_log(ofmt_ctx_mp4,AV_LOG_ERROR,"ERROR Fail to send  %s  cache \n",tcp_ip);
                write_cache_buff.pos=write_cache_buff.cache_buff;
                write_cache_buff.write_size=0;
                last_cache_pts=pkt.pts;
                isSend=true;
            }
        }
        if ((ret=av_interleaved_write_frame(ofmt_ctx_mp4, &pkt))< 0) {
            av_log(ofmt_ctx_mp4,AV_LOG_ERROR,"ERROR Fail to write packet %s,error is %d\n",tcp_ip,ret);
            fail_write_frame++;
            if(fail_write_frame>MAX_WRITE_FRAME_FAIL)
            {
                av_packet_unref(&pkt);
                av_log(ofmt_ctx_mp4,AV_LOG_ERROR,"ERROR Fail write packet over max %s\n",tcp_ip);
                return_error = ERROR_WRITE_PKT_FAIL;
                goto end;
            }
        }else
            fail_write_frame=0;
        av_packet_unref(&pkt);
    }
    //Write file trailer
    av_write_trailer(ofmt_ctx_mp4);
end:
    avformat_close_input(&ifmt_ctx);
    avio_context_free(&avio_out);
    avio_context_free(&avio_in);
    avformat_free_context(ofmt_ctx_mp4);
    if(write_cache_buff.cache_buff!=NULL)
        av_free(write_cache_buff.cache_buff);
#ifdef _WIN32
    closesocket(sock);
    WSACleanup();
#else
    close(sock);
#endif
    return return_error;
}

/*
send tcp,recv h264 stream  and save to format mp4 slice
tcp_ip tcp ip
tcp_port tcp port
filename slice name
control_cmd write control end
stopsecond stop time
video_type  code type
recordtime  real record time
*/
int tcp_slice_autostop(const char * tcp_ip,int tcp_port,const  char* filename, char * control_cmd,int stopsecond,int* video_type,int* recordtime)
{
    int ret,fail_read_frame=0,fail_write_frame=0,next_stop_time=0;
    bool first_video=false,first_audio=false;
    unsigned int index,video_index=-1,audio_index=-1;
    AVPacket pkt;
    AVDictionary *opts = NULL;
    AVFormatContext *ifmt_ctx = NULL,*ofmt_ctx_mp4=NULL;
    media_error_code return_error=NO_ERRORS;
    int64_t start_time=0,now_time=0,record_all_time=0;
    int videoCode_type=3;
    int buffsize=32768;
    long frame_index=0;
    AVIOContext* avio_in = NULL;
    int sock = 0;
    unsigned char* inbuffer = NULL;
    const AVInputFormat* ifmt = NULL;

    //set log
    av_log_set_callback(custom_output);
    av_log_set_level(AV_LOG_ERROR);


    if(tcp_ip==NULL||tcp_port<0||filename==NULL||control_cmd==NULL||stopsecond<=0||video_type==NULL||recordtime==NULL)
        return ERROR_ARG;


    ifmt_ctx= avformat_alloc_context();
    if(ifmt_ctx==NULL)
    {
        av_log(ifmt_ctx,AV_LOG_ERROR, "ERROR avformat_alloc_context fail in %s \n",tcp_ip);
        return_error=ERROR_MEM_FAIL;
        goto end;
    }
    //tcp connect,and recv data
#ifdef _WIN32
    WORD	wVersionRequested;
    WSADATA wsaData;
    wVersionRequested	=	MAKEWORD(2,2);

    ret	=	WSAStartup(wVersionRequested,&wsaData);
    if(ret!=0)
    {
        return_error=ERROR_OPEN_URL_FAIL;
        goto end;
    }
#else
#endif
    sock = socket(AF_INET, SOCK_STREAM, 0);
    struct sockaddr_in addrSrc;
    memset(&addrSrc,0,sizeof(struct sockaddr_in));

    addrSrc.sin_family=AF_INET;
    addrSrc.sin_port=htons(tcp_port);
    addrSrc.sin_addr.s_addr=inet_addr(tcp_ip);

    ret = connect(sock,(sockaddr*)&addrSrc,sizeof(sockaddr));

    if(ret<0)
    {
        av_log(ifmt_ctx,AV_LOG_ERROR, "ERROR,Could not open tcp address %s %d,error is %d\n",tcp_ip,tcp_port,ret);
        return_error=ERROR_OPEN_URL_FAIL;
        goto end;
    }

    //read from recv tcp buff
    inbuffer=(unsigned char *)av_malloc(buffsize);
    if(inbuffer==NULL)
    {
        av_log(ifmt_ctx,AV_LOG_ERROR,"ERROR Failed malloc buffer in %s \n",tcp_ip);
        return_error=ERROR_MEM_FAIL;
        goto end;

    }
    avio_in =avio_alloc_context(inbuffer, buffsize,0,&sock,fill_iobuffer,NULL,NULL);
    if(avio_in==NULL)
    {
        av_log(ifmt_ctx,AV_LOG_ERROR,"ERROR Failed malloc AVIOContext in %s \n",tcp_ip);
        av_free(inbuffer);
        return_error=ERROR_IOCONTENT_FAIL;
        goto end;

    }
    ifmt_ctx->pb=avio_in;

    //Input
    ifmt=av_find_input_format("h264");

    if ((ret = avformat_open_input(&ifmt_ctx,NULL, (AVInputFormat*)ifmt, NULL)) < 0) {
        av_log(ifmt_ctx,AV_LOG_ERROR, "ERROR,Could not open tcp %s,error is %d\n",tcp_ip,ret);
        return_error=ERROR_OPEN_URL_FAIL;
        goto end;
    }
    //find stream information in input
    if ((ret = avformat_find_stream_info(ifmt_ctx, 0)) < 0) {
        av_log(ifmt_ctx,AV_LOG_ERROR,"ERROR Failed to retrieve input %s stream information \n",tcp_ip);
        return_error=ERROR_STREAM_INFO_FAIL;
        goto end;
    }

    //pritnf input stream format
    av_dump_format(ifmt_ctx, 0, tcp_ip, 0);

    //Output
    avformat_alloc_output_context2(&ofmt_ctx_mp4, NULL, "mp4", filename);
    if (!ofmt_ctx_mp4) {
        av_log(ifmt_ctx,AV_LOG_ERROR,"ERROR create  output fail in %s\n",filename);
        return_error = ERROR_OPEN_OUTSTREAM_FAIL;
        goto end;
    }

    //create streams in output
    for (index = 0; index < ifmt_ctx->nb_streams; index++) {
        AVStream *in_stream = ifmt_ctx->streams[index];
        if(in_stream->codecpar->codec_type==AVMEDIA_TYPE_VIDEO)
            video_index=index;
        else if(in_stream->codecpar->codec_type==AVMEDIA_TYPE_AUDIO)
            audio_index=index;
        else {
            continue;
        }
        const AVCodec* codec= avcodec_find_decoder(in_stream->codecpar->codec_id);
        AVStream *out_stream = avformat_new_stream(ofmt_ctx_mp4,codec);

        if (!out_stream) {
            av_log(ifmt_ctx,AV_LOG_ERROR,"ERROR Fail to create out stream in %s\n",tcp_ip);
            return_error = ERROR_CREATE_AVSTREAM_FAIL;
            goto end;
        }

        //Copy the settings of AVCodecContext
        AVCodecContext* pCodecCtx = avcodec_alloc_context3(codec);
        ret = avcodec_parameters_to_context(pCodecCtx, in_stream->codecpar);
        if(ret<0){
            av_log(ifmt_ctx,AV_LOG_ERROR,"ERROR Fail to copy out stream codec in %s\n",tcp_ip);
            return_error = ERROR_CREATE_AVSTREAM_FAIL;
            avcodec_free_context(&pCodecCtx);
            goto end;
        }
        pCodecCtx->codec_tag = 0;

        if (ofmt_ctx_mp4->oformat->flags & AVFMT_GLOBALHEADER) {
            pCodecCtx->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;
        }

        ret = avcodec_parameters_from_context(out_stream->codecpar, pCodecCtx);

        if (ret < 0) {
            av_log(ifmt_ctx,AV_LOG_ERROR,"ERROR Fail to copy out stream codec in %s\n",tcp_ip);
            return_error = ERROR_CREATE_AVSTREAM_FAIL;
            avcodec_free_context(&pCodecCtx);
            goto end;

        }
        avcodec_free_context(&pCodecCtx);
    }

    //get video type
    if(ifmt_ctx->streams[video_index]->codecpar->codec_id==AV_CODEC_ID_H264)
        videoCode_type=0;
    else if(ifmt_ctx->streams[video_index]->codecpar->codec_id==AV_CODEC_ID_HEVC)
        videoCode_type=1;
    else
        videoCode_type=2;
    *video_type=videoCode_type;

    printf("get video type is %d\n",videoCode_type);

    //print out stream format
    av_dump_format(ofmt_ctx_mp4, 0, filename, 1);

    //Open output file
    if (!(ofmt_ctx_mp4->flags & AVFMT_NOFILE)) {
        if (avio_open(&ofmt_ctx_mp4->pb, filename, AVIO_FLAG_WRITE) < 0) {
            av_log( ofmt_ctx_mp4,AV_LOG_ERROR,"Could not open output file %s\n", filename);
            return_error=ERROR_OPEN_OUTSTREAM_FAIL;
            goto end;
        }
    }
    //fmp4
    av_dict_set(&opts, "movflags", "frag_keyframe+empty_moov", 0);
    //Write file header
    if (avformat_write_header(ofmt_ctx_mp4, &opts) < 0) {
        av_log( ofmt_ctx_mp4,AV_LOG_ERROR,"Error occurred when write output file header\n");
        return_error=ERROR_OPEN_OUTSTREAM_FAIL;
        goto end;
    }
    av_dict_free(&opts);

    //read an write
    //printf("control_cmd is %c\n",*(control_cmd+1));


    while (*(control_cmd+1)=='r') {
        //  printf("control_cmd is %c\n",*(control_cmd+1));
        if ((ret=av_read_frame(ifmt_ctx, &pkt))< 0){
            av_log(ifmt_ctx,AV_LOG_ERROR,"ERROR fail read a frame in %s,error is %d\n",tcp_ip,ret);
            av_packet_unref(&pkt);
            fail_read_frame++;
            if(fail_read_frame>MAX_READ_FRAME_FAIL)
            {
                return_error = ERROR_READ_FRAME_FAIL;
                av_log(ifmt_ctx,AV_LOG_ERROR,"ERROR fail read frame over max in %s\n",tcp_ip);
                break;
            }
            else
                continue;
        }
        else {
            fail_read_frame=0;
        }
        if(pkt.stream_index!=video_index&&pkt.stream_index!=audio_index)
        {
            av_packet_unref(&pkt);
            continue;
        }

        // set first packet pts 0 for sometimes first dts is lager than others
        if(first_video==0&&pkt.stream_index==video_index)
        {
            pkt.dts=0;
            pkt.pts=0;
            first_video=true;
            start_time=av_gettime();
            next_stop_time=stopsecond-(start_time/AV_TIME_BASE)%stopsecond;
            printf("next stop time is %d\n",next_stop_time);
        }
        if(first_audio==0&&pkt.stream_index==audio_index)
        {
            pkt.dts=0;
            pkt.pts=0;
            first_audio=true;
        }
        //Simple Write PTS
        if(pkt.pts==AV_NOPTS_VALUE){
            //Write PTS
            AVRational time_base1=ifmt_ctx->streams[video_index]->time_base;
            //Duration between 2 frames (us)
            int64_t calc_duration=(double)AV_TIME_BASE/av_q2d(ifmt_ctx->streams[video_index]->r_frame_rate);
            //Parameters
            pkt.pts=(double)(frame_index*calc_duration)/(double)(av_q2d(time_base1)*AV_TIME_BASE);
            pkt.dts=pkt.pts;
            pkt.duration=(double)calc_duration/(double)(av_q2d(time_base1)*AV_TIME_BASE);
            frame_index++;
        }
#if 1
        AVStream* in_stream  = ifmt_ctx->streams[pkt.stream_index];
        AVStream* out_stream = ofmt_ctx_mp4->streams[pkt.stream_index];
        // copy packet */
        pkt.pts = av_rescale_q_rnd(pkt.pts, in_stream->time_base, out_stream->time_base, (AVRounding)(AV_ROUND_NEAR_INF|AV_ROUND_PASS_MINMAX));
        pkt.dts = av_rescale_q_rnd(pkt.dts, in_stream->time_base, out_stream->time_base, (AVRounding)(AV_ROUND_NEAR_INF|AV_ROUND_PASS_MINMAX));
        pkt.duration = av_rescale_q(pkt.duration, in_stream->time_base, out_stream->time_base);
        pkt.pos = -1;
#endif
        //printf("read a frame indexis %d %ld\n",pkt.stream_index,pkt.dts);
        //if time over and i frame,stop
        if(pkt.stream_index==video_index&&pkt.flags& AV_PKT_FLAG_KEY){
            now_time=av_gettime();
            //printf("now_time is %d \n",(now_time-start_time)/AV_TIME_BASE);
            if((now_time-start_time)/AV_TIME_BASE>next_stop_time)
            {
                AVRational time_base=ofmt_ctx_mp4->streams[video_index]->time_base;
                AVRational time_base_q={1,AV_TIME_BASE};
                record_all_time = av_rescale_q((pkt.pts), time_base, time_base_q);
                printf("record time is %lld\n",record_all_time);
                *recordtime=record_all_time/AV_TIME_BASE;
                av_packet_unref(&pkt);
                return_error=NO_ERRORS;
                break;
            }
        }
        if ((ret=av_interleaved_write_frame(ofmt_ctx_mp4, &pkt))< 0) {
            av_log(ofmt_ctx_mp4,AV_LOG_ERROR,"ERROR Fail to write packet %s,error is %d\n",tcp_ip,ret);
            fail_write_frame++;
            if(fail_write_frame>MAX_WRITE_FRAME_FAIL)
            {
                av_packet_unref(&pkt);
                av_log(ofmt_ctx_mp4,AV_LOG_ERROR,"ERROR Fail write packet over max %s\n",tcp_ip);
                return_error = ERROR_WRITE_PKT_FAIL;
                goto end;
            }
            else {
                continue;
            }
        }
        else {
            fail_write_frame=0;
        }
        //printf("Write %8d frames to output file\n",frame_index);
        av_packet_unref(&pkt);
    }
    //Write file trailer
    av_write_trailer(ofmt_ctx_mp4);
end:

    avformat_close_input(&ifmt_ctx);
    /* close output */
    avio_context_free(&avio_in);
    if (ofmt_ctx_mp4 && !(ofmt_ctx_mp4->flags & AVFMT_NOFILE))
        avio_close(ofmt_ctx_mp4->pb);
    avformat_free_context(ofmt_ctx_mp4);
#ifdef _WIN32
    closesocket(sock);
    WSACleanup();
#else
    close(sock);
#endif
    return return_error;
}


///////////////////////////////////////////////////////////////////////////////////////////
//#define VERSION_NEW
// 2022.3.21 zk
int save_to_jpeg(AVFrame* frame, AVStream *s, const char* filename, custom_cbk cbk = NULL, void* arg=NULL);
int video_to_image(const char* url, const char* filename, custom_cbk cbk/*=NULL*/, void* arg/*=NULL*/)
{
    int nRet = -1;
    if (NULL == url) {
        LOG("ERROR", "Parameter error %p\n", url);
        return nRet;
    }
    AVFormatContext* ifmt_ctx = NULL;
    AVCodecContext* video_codec_ctx = NULL;// 视频解码器上下文
    AVFormatContext* ofmt_cxt = NULL;
    AVCodecContext* encoder_ctx = NULL;     // 视频编码上下文
    //av_register_all();
    avformat_network_init();
    //avdevice_register_all();	//注册设备
    av_log_set_callback(custom_output);
    //av_log_set_level(AV_LOG_ERROR);
    char err_msg[64] = {0};
    do {
        const AVInputFormat* ifmt = av_find_input_format(url);
        AVDictionary* opts = NULL;
        //av_dict_set_int(&opts, "rtbufsize", 1843200000, 0);
        //av_dict_set(&opts, "buffer_size", "2097152", 0);
        //av_dict_set(&opts, "max_delay", "0", 0);
        av_dict_set(&opts, "stimeout", "6000000", 0);
        //av_dict_free(&opts);
        nRet = avformat_open_input(&ifmt_ctx, url, (AVInputFormat*)ifmt, &opts);
        av_dict_free(&opts);
        if (nRet < 0) {
            av_strerror(nRet, err_msg, sizeof(err_msg));
            LOG("ERROR", "avformat_open_input failed '%s' [ret:%d,msg:'%s']\n", url, nRet, err_msg);
            break;
        }
        nRet = avformat_find_stream_info(ifmt_ctx, NULL);
        if (nRet < 0) {
            av_strerror(nRet, err_msg, sizeof(err_msg));
            LOG("ERROR", "avformat_find_stream_info failed [ret:%d,msg:'%s']\n", nRet, err_msg);
            break;
        }
        av_dump_format(ifmt_ctx, 0, url, 0);
        int video_index = av_find_best_stream(ifmt_ctx, AVMEDIA_TYPE_VIDEO, -1, -1, NULL, 0);
        if (-1 == video_index) {
            LOG("ERROR", "av_find_best_stream failed [ret:%d,msg:'%s']\n", nRet);
            nRet = -1;
            break;
        }
        //=========================================================================================
        // 查找并打开解码器
        AVStream* in_vstream = ifmt_ctx->streams[video_index]; // 视频解码器参数
        const AVCodec* video_codec = avcodec_find_decoder(in_vstream->codecpar->codec_id);
        if (NULL == video_codec) {
            LOG("ERROR", "avcodec_find_decoder function failed.\n");
            break;
        }
        video_codec_ctx = avcodec_alloc_context3(NULL);
        if (avcodec_parameters_to_context(video_codec_ctx, in_vstream->codecpar) < 0) {
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
#ifdef VERSION_NEW
        //=========================================================================================
        //nRet = avformat_alloc_output_context2(&out_fmt_cxt, NULL, "singlejpeg", filename);
        nRet = avformat_alloc_output_context2(&ofmt_cxt, NULL, "mjpeg", filename);
        if (nRet < 0) {
            av_strerror(nRet, err_msg, sizeof(err_msg));
            LOG("ERROR", "avformat_alloc_output_context2 failed [ret:%d,msg:'%s']\n", nRet, err_msg);
            break;
        }
        nRet = avio_open2(&ofmt_cxt->pb, filename, AVIO_FLAG_READ_WRITE, NULL, NULL);
        if (nRet < 0) {
            av_strerror(nRet, err_msg, sizeof(err_msg));
            LOG("ERROR", "avio_open2 failed [ret:%d,msg:'%s']\n", nRet, err_msg);
            avformat_free_context(ofmt_cxt);
            ofmt_cxt = NULL;
            break;
        }
        // new一个流并为其设置视频参数
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
        av_dump_format(ofmt_cxt, 0, filename, 1);
        // 编码器
        const AVCodec* video_encoder = avcodec_find_encoder(out_vstream->codecpar->codec_id);
        if (NULL == video_encoder) {
            LOG("ERROR", "avcodec_find_encoder fcunction failed\n");
            break;
        }
        encoder_ctx = avcodec_alloc_context3(video_encoder);
        if (NULL == encoder_ctx) {
            LOG("ERROR", "avcodec_alloc_context3 failed.\n");
            break;
        }
        avcodec_parameters_to_context(encoder_ctx, out_vstream->codecpar);
        encoder_ctx->time_base.num = 1;
        encoder_ctx->time_base.den = 25;
        nRet = avcodec_open2(encoder_ctx, video_encoder, NULL);
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
#endif

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
            if ((in_pkt.stream_index != video_index) || (!(in_pkt.flags & AV_PKT_FLAG_KEY))) {
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
            int getframe_ret = 0;
            while (getframe_ret >= 0) {
                getframe_ret = avcodec_receive_frame(video_codec_ctx, frame);
                if ((AVERROR(EAGAIN) == nRet) || (AVERROR_EOF == nRet)) {
                    break;
                }
                else if (getframe_ret < 0) {
                    av_strerror(getframe_ret, err_msg, sizeof(err_msg));
                    LOG("ERROR", "avcodec_receive_frame function failed [ret:%d,msg:'%s']\n", getframe_ret, err_msg);
                    goto end;
                }
                #ifndef VERSION_NEW
                    nRet = save_to_jpeg(frame, ifmt_ctx->streams[video_index], filename, cbk, arg);
                #else
                    nRet = avcodec_send_frame(encoder_ctx, frame);
                    if (nRet < 0) {
                        av_strerror(nRet, err_msg, sizeof(err_msg));
                        LOG("ERROR", "avcodec_send_frame function failed [ret:%d,msg:'%s']\n", nRet, err_msg);
                        break;
                    }
                    int getpacket_ret = 0;
                    while (getpacket_ret >= 0) {
                        getpacket_ret = avcodec_receive_packet(encoder_ctx, &out_pkt);
                        if ((AVERROR(EAGAIN) == getpacket_ret) || (AVERROR_EOF == getpacket_ret))
                            break;
                        else if (getpacket_ret < 0) {
                            av_strerror(nRet, err_msg, sizeof(err_msg));
                            LOG("ERROR", "avcodec_receive_packet function failed [ret:%d,msg:'%s']\n", getpacket_ret, err_msg);
                            break;
                        }
                        nRet = av_interleaved_write_frame(ofmt_cxt, &out_pkt);
                        av_packet_unref(&out_pkt);
                        if (nRet < 0) {
                            write_frame_fail++;
                            av_strerror(nRet, err_msg, sizeof(err_msg));
                            LOG("ERROR", "av_interleaved_write_frame function failed [ret:%d,msg:'%s']\n", nRet, err_msg);
                            if (write_frame_fail > 25)
                                goto end;
                        }
                        else {
                            nRet = av_write_trailer(ofmt_cxt);  // 3.写入流尾至文件
                            if (nRet < 0) {
                                av_strerror(nRet, err_msg, sizeof(err_msg));
                                LOG("ERROR", "av_write_trailer function failed [ret:%d,msg:'%s']\n", nRet, err_msg);
                            }
                            goto end;
                        }
                    }//while()
                #endif
            }//while()
            av_frame_unref(frame);
            //av_packet_unref(&in_pkt);
            break;
        } // while
        if (NULL != frame)
            av_frame_free(&frame);
    } while (false);

end:
    if (NULL != ofmt_cxt) {
        if (NULL != ofmt_cxt->pb)
            avio_close(ofmt_cxt->pb);
        avformat_free_context(ofmt_cxt);
    }
    if (NULL != encoder_ctx) {
        avcodec_close(encoder_ctx);
        avcodec_free_context(&encoder_ctx);
    }
    if (NULL != video_codec_ctx) {
        avcodec_close(video_codec_ctx);
        avcodec_free_context(&video_codec_ctx);
    }
    if (NULL != ifmt_ctx) {
        avformat_close_input(&ifmt_ctx);
        //avcodec_close(avc_cxt);
        avformat_free_context(ifmt_ctx);
    }
    return nRet;
}
int save_to_jpeg(AVFrame* frame, AVStream* s, const char* filename, custom_cbk cbk/*=NULL*/, void* arg/*=NULL*/)
{
    AVFormatContext* out_fmt_cxt = NULL;
    AVCodecContext* encoder_ctx = NULL;
    int nRet = -1;
    do {
        //nRet = avformat_alloc_output_context2(&out_fmt_cxt, NULL, "singlejpeg", filename);
        nRet = avformat_alloc_output_context2(&out_fmt_cxt, NULL, "mjpeg", filename);
        if (nRet < 0) {
            //av_log(out_fmt_cxt, AV_LOG_ERROR, "avformat_alloc_output_context2 error\n");
            char err_msg[128] = {0};
            av_strerror(nRet, err_msg, sizeof(err_msg));
            LOG("ERROR", "avformat_alloc_output_context2 failed[ret:%d,msg:'%s']\n", nRet, err_msg);
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
        if (NULL != stream) {
            stream->codecpar->codec_id = out_fmt_cxt->oformat->video_codec;
            stream->codecpar->codec_type = AVMEDIA_TYPE_VIDEO;
            stream->codecpar->format = AV_PIX_FMT_YUVJ420P;
            stream->codecpar->height = s->codecpar->height;
            stream->codecpar->width = s->codecpar->width;
            stream->time_base.num = 1;
            stream->time_base.den = 25;
        }
        else {
            av_log(out_fmt_cxt, AV_LOG_ERROR, "avformat_new_stream failed.\n");
            LOG("ERROR", "avformat_new_stream failed %d\n", nRet);
            break;
        }
        av_dump_format(out_fmt_cxt, 0, filename, 1);
        // 编码器
        const AVCodec* video_encoder = avcodec_find_encoder(stream->codecpar->codec_id);
        if (NULL == video_encoder) {
            av_log(out_fmt_cxt, AV_LOG_ERROR, "Encoder not found.\n");
            LOG("ERROR", "avcodec_find_encoder failed %d\n", nRet);
            break;
        }
        encoder_ctx = avcodec_alloc_context3(video_encoder);
        if (NULL == encoder_ctx) {
            LOG("ERROR", "avcodec_alloc_context3 failed.\n");
            break;
        }
        avcodec_parameters_to_context(encoder_ctx, stream->codecpar);
        encoder_ctx->time_base.num = 1;
        encoder_ctx->time_base.den = 25;
        nRet = avcodec_open2(encoder_ctx, video_encoder, NULL);
        if (nRet < 0) {
            av_log(out_fmt_cxt, AV_LOG_ERROR, "avcodec_open2 function failed %d\n", nRet);
            char err_msg[128] = { 0 };
            av_strerror(nRet, err_msg, sizeof(err_msg));
            LOG("ERROR", "avcodec_open2 function failed [ret:%d,msg:'%s']\n", nRet, err_msg);
            break;
        }
        nRet = avformat_write_header(out_fmt_cxt, NULL);       // 1.写入文件头
        if (nRet < 0) {
            char err_msg[128] = { 0 };
            av_strerror(nRet, err_msg, sizeof(err_msg));
            LOG("ERROR", "avformat_write_header function failed [ret:%d,msg:'%s']\n", nRet, err_msg);
            break;
        }
        AVPacket packet;
        av_init_packet(&packet);
        nRet = avcodec_send_frame(encoder_ctx, frame);
        if (nRet < 0) {
            av_packet_unref(&packet);
            char err_msg[128] = { 0 };
            av_strerror(nRet, err_msg, sizeof(err_msg));
            LOG("ERROR", "avcodec_send_frame function failed [ret:%d,msg:'%s']\n", nRet, err_msg);
            break;
        }
        int getpacket_ret = 0;
        while (getpacket_ret >= 0) {
            getpacket_ret = avcodec_receive_packet(encoder_ctx, &packet);
            if ((AVERROR(EAGAIN) == getpacket_ret) || (AVERROR_EOF == getpacket_ret))
                break;
            else if (getpacket_ret < 0) {
                LOG("ERROR", "avcodec_receive_packet function failed %d\n", getpacket_ret);
                break;
            }
            //nRet = av_write_frame(out_fmt_cxt, &packet); // 2.写入编码后的视频帧
            nRet = av_interleaved_write_frame(out_fmt_cxt, &packet);
            if (nRet < 0) {
                av_packet_unref(&packet);
                char err_msg[128] = { 0 };
                av_strerror(nRet, err_msg, sizeof(err_msg));
                LOG("ERROR", "av_interleaved_write_frame function failed [ret:%d,msg:'%s']\n", nRet, err_msg);
                break;
            }
        }
        av_packet_unref(&packet);
        nRet = av_write_trailer(out_fmt_cxt);  // 3.写入流尾至文件
        if (nRet < 0) {
            av_packet_unref(&packet);
            char err_msg[128] = { 0 };
            av_strerror(nRet, err_msg, sizeof(err_msg));
            LOG("ERROR", "av_write_trailer function failed [ret:%d,msg:'%s']\n", nRet, err_msg);
            break;
        }
    } while (false);
    if (NULL != out_fmt_cxt) {
        if (out_fmt_cxt->flags & AVFMT_FLAG_CUSTOM_IO) {
            avio_context_free(&out_fmt_cxt->pb);
        }
        else {
            if (NULL != out_fmt_cxt->pb)
                avio_close(out_fmt_cxt->pb);
        }
        avformat_free_context(out_fmt_cxt);
    }
    if (NULL != encoder_ctx) {
        avcodec_close(encoder_ctx);
        avcodec_free_context(&encoder_ctx);
    }
    return nRet;
}
static int interrupt_cbk(void* opaque)
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

int video_stream(char* control_cmd, stream_param_t* param)
{
    if ((NULL == control_cmd) || (NULL == param) || (NULL == g_minio)) {
        LOG("ERROR", "Input parameter error, [control_cmd:%p,param:%p,minio:%p]\n", control_cmd, param, g_minio);
        return -1;
    }
    //avdevice_register_all();	//注册设备
    avformat_network_init();	//用于初始化网络。FFmpeg本身也支持解封装RTSP的数据,如果要解封装网络数据格式，则可调用该函数。
    //set log
    //av_log_set_callback(custom_output);
    //av_log_set_level(AV_LOG_ERROR);

    int nRet = -1;
    AVFormatContext* ifmt_ctx = NULL;
    AVFormatContext* ofmt_ctx_mp4 = NULL;
    AVCodecParameters* video_h264_params = NULL;
    stream_cache_buff  write_cache_buff;
    AVDictionary* opts = NULL;
    bool is_playback = false;
    AVInputFormat* ifmt = NULL;	// av_find_input_format("h264");
    bool reconnet = false;
    int64_t read_pkt_time = 0;
    char err_msg[64] = {0};
    do {
        if ((NULL != param->in_url) && (NULL == param->in_custom)) { // 点播
            av_dict_set(&opts, "buffer_size", "2048000", 0);
            av_dict_set(&opts, "stimeout", "3000000", 0);
            av_dict_set(&opts, "rtsp_transport", "tcp", 0);
            nRet = avformat_open_input(&ifmt_ctx, param->in_url, ifmt, &opts);
            read_pkt_time = av_gettime();
            ifmt_ctx->interrupt_callback.opaque = &read_pkt_time;
            ifmt_ctx->interrupt_callback.callback = interrupt_cbk;
        }
        else if ((NULL != param->in_url) && (NULL != param->in_custom)) { // 回放
            is_playback = true;
            ifmt_ctx = avformat_alloc_context();
            unsigned char* buffer = (unsigned char*)av_malloc(BUFF_SIZE);
            AVIOContext* avio_ctx = avio_alloc_context(buffer, BUFF_SIZE, 0, NULL, param->in_custom, NULL, seek_packet);
            if (NULL == avio_ctx) {
                av_free(buffer);
                av_dict_free(&opts);
                LOG("ERROR", "Allocation memory failed %p\n", avio_ctx);
                break;
            }
            ifmt_ctx->pb = avio_ctx;
            ifmt_ctx->flags |= AVFMT_FLAG_CUSTOM_IO;
            printf("\033[32m ------ in.buffer_size:%d ------ \033[0m\n\n", ifmt_ctx->pb->buffer_size);
            nRet = avformat_open_input(&ifmt_ctx, "", ifmt, NULL);
        }
        else {
            LOG("ERROR","in_url=%p, in_custom=%p\n", param->in_url, param->in_custom);
            break;
        }
        av_dict_free(&opts);
        opts = NULL;
start:
        if ((nRet < 0) || (NULL == ifmt_ctx)) {
            av_strerror(nRet, err_msg, sizeof(err_msg));
            LOG("ERROR", "avformat_open_input function failed [ret:%d,msg:'%s',url:'%s']\n", nRet, err_msg, param->in_url);
            nRet = ERROR_OPEN_URL_FAIL;
            break;
        }
        printf("\033[32m ------ Play: '%s' ------ \033[0m\n\n", is_playback ? "Playback" : "Live");
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
        av_dump_format(ifmt_ctx, 0, param->in_url, 0);
        AVStream* in_vstream = ifmt_ctx->streams[video_index];
        AVStream* in_astream = (-1 != audio_index) ? ifmt_ctx->streams[audio_index] : NULL;
        if (is_playback && (param->start_time >= 0)) { // 计算开始dts (播放录像文件时)
            int64_t start_video_dts = in_vstream->start_time + (int64_t)(param->start_time / av_q2d(in_vstream->time_base));
            nRet = av_seek_frame(ifmt_ctx, video_index, start_video_dts, AVSEEK_FLAG_FRAME); //AVSEEK_FLAG_FRAME,AVSEEK_FLAG_BACKWARD
            if (nRet < 0) {
                av_strerror(nRet, err_msg, sizeof(err_msg));
                LOG("ERROR", "av_seek_frame function failed [ret:%d,start_time:%lld,dts:%lld,msg:'%s']\n", nRet, param->start_time, start_video_dts, err_msg);
                break;
            }
        }
        //////////////////////////////////////////////////////////////////////////////////////////
        #ifdef COMPILE_EXE
            #ifdef _WIN32
            //const char* ouput = "C:/Users/zhangkun/Desktop/output.mp4";
            const char* ouput = NULL;
            #else
            //const char* ouput = "./output.mp4";
            const char* ouput = NULL;
            #endif
        #else
            const char* ouput = NULL;
        #endif
        // Output MP4
        avformat_alloc_output_context2(&ofmt_ctx_mp4, NULL, "mp4", ouput);
        if (NULL == ofmt_ctx_mp4) {
            LOG("ERROR", "avformat_alloc_output_context2 function failed.\n");
            nRet = ERROR_OPEN_OUTSTREAM_FAIL;
            break;
        }
        if (NULL != ouput) {
            if (!(ofmt_ctx_mp4->oformat->flags & AVFMT_NOFILE)) {
                nRet = avio_open2(&ofmt_ctx_mp4->pb, ouput, AVIO_FLAG_READ_WRITE, NULL, NULL);
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
                LOG("ERROR", "av_malloc function failed\n");
                nRet = ERROR_MEM_FAIL;
                break;
            }
            write_cache_buff.cache_buff = (uint8_t*)av_malloc(CACHE_BIT_MAZ_WIDTH * CACHE_TIME_SECOND);
            write_cache_buff.pos = write_cache_buff.cache_buff;
            write_cache_buff.limit = write_cache_buff.cache_buff + ((CACHE_BIT_MAZ_WIDTH * CACHE_TIME_SECOND) -1);
            write_cache_buff.write_size = 0;
            AVIOContext* avio_out = avio_alloc_context(buffer, BUFF_SIZE, 1, &write_cache_buff, NULL, write_packet, NULL);
            if (NULL == avio_out) {
                LOG("ERROR", "avio_alloc_context function failed.\n");
                av_free(buffer);
                nRet = ERROR_IOCONTENT_FAIL;
                break;
            }
            ofmt_ctx_mp4->pb = avio_out;
            ofmt_ctx_mp4->flags = AVFMT_FLAG_CUSTOM_IO;
            printf("\033[32m ------ out.buffer_size:%d ------ \033[0m\n\n", ofmt_ctx_mp4->pb->buffer_size);
        }
        ////////////////////////////////////////////////////////////////
        AVCodecContext* video_h265_codec_ctx = NULL;
        AVCodecContext* video_h264_codec_ctx = NULL;
        AVCodec* video_h264_codec = NULL; // H264解码或编码器
        video_h264_params = avcodec_parameters_alloc();
        if (NULL == video_h264_params) {
            LOG("ERROR", "avcodec_parameters_alloc function failed.\n");
            nRet = -100;
            break;
        }
        if (AV_CODEC_ID_H264 == in_vstream->codecpar->codec_id) {
            video_h264_codec = avcodec_find_decoder(in_vstream->codecpar->codec_id);
            avcodec_parameters_copy(video_h264_params, in_vstream->codecpar);
        }
        else if (AV_CODEC_ID_H265 == in_vstream->codecpar->codec_id) {
            /////////////////////////////////////////////////////////////////////////////////
            // 查找H265视频解码器 (H265 -> YUV420p)
            AVCodec* in_video_codec = avcodec_find_decoder(in_vstream->codecpar->codec_id);
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
            //video_h265_codec_ctx->thread_count = 16;//设置解码线程数量
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
            video_h264_codec_ctx->codec_id = video_h264_codec->id; // AV_CODEC_ID_H264
            video_h264_codec_ctx->width = video_h265_codec_ctx->width;
            video_h264_codec_ctx->height = video_h265_codec_ctx->height;
            video_h264_codec_ctx->codec_type = AVMEDIA_TYPE_VIDEO;
            video_h264_codec_ctx->pix_fmt = AV_PIX_FMT_YUV420P;// video_h265_codec_ctx->pix_fmt, AV_PIX_FMT_YUV420P
            // 25 frame a second
            video_h264_codec_ctx->time_base.num = 1;
            video_h264_codec_ctx->time_base.den = 25;
            video_h264_codec_ctx->framerate.num = 25;    // 帧率
            video_h264_codec_ctx->framerate.den = 1;
            //video_h264_codec_ctx->bit_rate = video_h265_codec_ctx->bit_rate * 2;//h264 byterate
            video_h264_codec_ctx->bit_rate = 1600000;
            video_h264_codec_ctx->gop_size = 50;    // I帧间隔,i frame interval
            video_h264_codec_ctx->keyint_min = 50;  // 最小I间隔
            video_h264_codec_ctx->qmin = 10;
            video_h264_codec_ctx->qmax = 51;
            //video_h264_codec_ctx->qcompress = 1; // zk
            //video_h264_codec_ctx->max_b_frames = 3; // zk
            //video_h264_codec_ctx->max_b_frames = 1; // zk
            //video_h264_codec_ctx->thread_count = 16;//设置编码线程数量
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
        AVStream* out_vstream = avformat_new_stream(ofmt_ctx_mp4, video_h264_codec);
        if (NULL == video_stream) {
            LOG("ERROR", "avformat_new_stream function failed.\n");
            nRet = ERROR_CREATE_AVSTREAM_FAIL;
            break;
        }
        nRet = avcodec_parameters_copy(out_vstream->codecpar, video_h264_params);
        if (nRet < 0) {
            LOG("ERROR", "avcodec_parameters_copy function failed [ret:%d]\n", nRet);
            break;
        }
        out_vstream->time_base = video_h264_codec_ctx->time_base;
        //out_vstream->time_base = in_vstream->time_base;
        out_vstream->codecpar->codec_tag = 0;
        if (ofmt_ctx_mp4->oformat->flags & AVFMT_GLOBALHEADER)
            ;// video_stream->codec->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;
        // new 一个音频流
        AVStream* out_astream = NULL;
        if (audio_index >= 0) {
            const AVCodec* a_codec = avcodec_find_decoder(in_astream->codecpar->codec_id);
            out_astream = avformat_new_stream(ofmt_ctx_mp4, a_codec);
            if (NULL == out_astream) {
                LOG("ERROR", "avformat_new_stream function failed.\n");
                nRet = ERROR_CREATE_AVSTREAM_FAIL;
                break;
            }
            nRet = avcodec_parameters_copy(out_astream->codecpar, in_astream->codecpar);
            if (nRet < 0) {
                LOG("ERROR", "avcodec_parameters_copy function failed [ret:%d]\n", nRet);
                break;
            }
            out_astream->codecpar->codec_tag = 0;
            if (ofmt_ctx_mp4->oformat->flags & AVFMT_GLOBALHEADER)
                ;// audio_stream->codec->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;
        }
        av_dump_format(ofmt_ctx_mp4, 0, 0, 1);
        //////////////////////////////////////////////////////////////////////////////////////////
        if (false == reconnet) { // 断流重连后无需发送视频头帧
            // 发送流的类型
            if ((video_index >= 0) && (-1 == audio_index))  // 视频流
                write_cache_buff.write_size = VIDEO_ONLY;
            else if ((video_index >= 0) && (audio_index >= 0))// 混合流
                write_cache_buff.write_size = VIDEO_AND_AUDIO;
            nRet = param->out_custom(param->out_opaque, write_cache_buff.cache_buff, write_cache_buff.write_size);
            write_cache_buff.pos = write_cache_buff.cache_buff;
            write_cache_buff.write_size = 0;
            ///////////////////////////////////////////////////////////////////////////////////////////
        }
        else write_cache_buff.reconnet = true;
        av_dict_set(&opts, "movflags", "frag_keyframe+empty_moov+omit_tfhd_offset+faststart+separate_moof+disable_chpl+default_base_moof+dash", 0);
        write_cache_buff.video_head = true;
        nRet = avformat_write_header(ofmt_ctx_mp4, &opts);
        write_cache_buff.video_head = false;
        write_cache_buff.reconnet = false;
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
        int sp = (1000000.0 / av_q2d(in_vstream->avg_frame_rate)); 
        double avg_frame_rate = av_q2d(in_vstream->avg_frame_rate); // 平均视频帧率
        printf("------------ sp=%d ------------\n", sp);
        // 'r': 运行或继续(run), 'p': 暂停 (pause), 's': 停止(stop)
        time_t tm1 = time(NULL);
        int64_t t1 = 0;
        int64_t nnnnnn = 30000000;    // 测试 10s
        int64_t v_pts = 0, a_pts = 0, out_pkt_pts = 0, duration = 0;// ofmt_ctx_mp4->streams[video_index]->time_base.
        bool first_i_frame = true;
        int i = 0;
        nRet = av_read_frame(ifmt_ctx, &in_pkt);
        while (('r' == *(control_cmd + 1)) || ('p' == *(control_cmd + 1))) {
            if ('p' == *(control_cmd + 1)) { // 暂停
                printf("usleep ...\n");
                av_usleep(500000);
                continue;
            }
            read_pkt_time = av_gettime();
            if (in_pkt.size <= 0)
                nRet = av_read_frame(ifmt_ctx, &in_pkt);
            else printf("pkt.size %d\n", in_pkt.size);
            if (nRet < 0) {
                if (nRet == AVERROR_EOF) {
                    LOG("ERROR", "File reading end %d.\n", nRet);
                    nRet = FILE_EOF;
                    break;
                }
                else {
                    av_strerror(nRet, err_msg, sizeof(err_msg));
                    LOG("ERROR", "av_read_frame failed [ret:%d,msg:'%s',count:%d]\n", nRet, err_msg, read_frame_fail);
                    av_packet_unref(&in_pkt);
                    read_frame_fail++;
                    if (read_frame_fail > 25) { // read frame failed
                        nRet = ERROR_READ_FRAME_FAIL;
                        break;
                    }
                    continue;
                }
            }
            read_frame_fail = 0;
            if (video_index == in_pkt.stream_index) { // 视频流
                if (first_video) {
                    first_video = false;
                    in_pkt.dts = in_pkt.pts = 0;
                }
                else if (in_pkt.pts == AV_NOPTS_VALUE) {
                    in_pkt.duration = (double)AV_TIME_BASE / av_q2d(in_vstream->r_frame_rate);
                    in_pkt.pts = ++video_number * in_pkt.duration;
                    in_pkt.dts = in_pkt.pts;
                    printf("video pts=%lld,dts=%lld,duration=%lld\n", in_pkt.pts, in_pkt.dts, in_pkt.duration);
                }
                if (in_pkt.flags & AV_PKT_FLAG_KEY)  // I帧
                { 
                    if(write_cache_buff.write_size > 0) {
                        //int64_t t1 = av_gettime();
                        param->out_custom(param->out_opaque, write_cache_buff.cache_buff, write_cache_buff.write_size);
                        //int64_t t2 = av_gettime();
                        //printf("websocket [data:%d, time:%lld, current:%lld]\n", write_cache_buff.write_size,(t2-t1)/1000, time(NULL));
                        write_cache_buff.pos = write_cache_buff.cache_buff;
                        write_cache_buff.write_size = 0;
                    }
                }
            }
            else if (audio_index == in_pkt.stream_index) { // 音频流
                if (is_playback && (1.0 != *param->ratio)) { // 回放 (倍率不为1.0时,音频可以去掉)
                    av_packet_unref(&in_pkt);
                    continue;
                }
                if (first_audio) {
                    first_audio = false;
                    in_pkt.dts = in_pkt.pts = 0;
                }
                else if (in_pkt.pts == AV_NOPTS_VALUE) {
                    in_pkt.duration = (double)AV_TIME_BASE / av_q2d(in_astream->r_frame_rate);
                    in_pkt.pts = ++audio_number * in_pkt.duration;
                    in_pkt.dts = in_pkt.pts;
                    printf("audio pts=%lld,dts=%lld,duration=%lld\n", in_pkt.pts, in_pkt.dts, in_pkt.duration);
                }
            }
            else { // 其他流
                printf(" other stream.\n");
                av_packet_unref(&in_pkt);
                continue;
            }
            if (in_pkt.stream_index == audio_index) { // 音频
                //AVStream* in_stream = ifmt_ctx->streams[audio_index];
                //AVStream* out_stream = ofmt_ctx_mp4->streams[o_audio_index];
                //// Convert PTS/DTS
                //in_pkt.pts = av_rescale_q_rnd(in_pkt.pts, in_stream->time_base, out_stream->time_base, (AVRounding)(AV_ROUND_NEAR_INF | AV_ROUND_PASS_MINMAX));
                //in_pkt.dts = av_rescale_q_rnd(in_pkt.dts, in_stream->time_base, out_stream->time_base, (AVRounding)(AV_ROUND_NEAR_INF | AV_ROUND_PASS_MINMAX));
                //in_pkt.duration = av_rescale_q(in_pkt.duration, in_stream->time_base, out_stream->time_base);
                //in_pkt.pos = -1;
                // 回放速度控制,拖放时PTS时间从0递增(pts不能直接从拖放点开始)
                //if (is_playback && ((param->start_time > 0) && (in_pkt.pts != AV_NOPTS_VALUE))) {
                //    in_pkt.duration = in_pkt.duration / (*param->ratio);
                //    if (in_pkt.stream_index == audio_index) {
                //        a_pts += in_pkt.duration;
                //        in_pkt.pts = a_pts;
                //    }
                //    else {
                //        v_pts += in_pkt.duration;
                //        in_pkt.pts = v_pts;
                //    }
                //    in_pkt.dts = in_pkt.pts;
                //}
                nRet = av_interleaved_write_frame(ofmt_ctx_mp4, &in_pkt);
                if (nRet < 0) {
                    av_strerror(nRet, err_msg, sizeof(err_msg));
                    LOG("ERROR", "av_interleaved_write_frame function failed [ret:%d,count:%d,msg:'%s']\n", nRet, write_frame_fail, err_msg);
                    av_packet_unref(&in_pkt);
                    if (write_frame_fail++ >= 25) {
                        nRet = ERROR_WRITE_PKT_FAIL;
                        break;
                    }
                    else continue;
                }
                write_frame_fail = 0;
            }
            else {
                if (AV_CODEC_ID_H264 == in_vstream->codecpar->codec_id) {
                    printf("H264\n");
                    // Convert PTS/DTS
                    in_pkt.pts = av_rescale_q_rnd(in_pkt.pts, in_vstream->time_base, out_vstream->time_base, (AVRounding)(AV_ROUND_NEAR_INF | AV_ROUND_PASS_MINMAX));
                    in_pkt.dts = av_rescale_q_rnd(in_pkt.dts, in_vstream->time_base, out_vstream->time_base, (AVRounding)(AV_ROUND_NEAR_INF | AV_ROUND_PASS_MINMAX));
                    in_pkt.duration = av_rescale_q(in_pkt.duration, in_vstream->time_base, out_vstream->time_base);
                    in_pkt.pos = -1;
                    // 回放速度控制,拖放时PTS时间从0递增(pts不能直接从拖放点开始)
                    if (is_playback && ((1.0 != *param->ratio) || ((param->start_time > 0) && (in_pkt.pts != AV_NOPTS_VALUE)))) {
                        in_pkt.duration = in_pkt.duration / (*param->ratio);
                        v_pts += in_pkt.duration;
                        in_pkt.pts = v_pts;
                        in_pkt.dts = in_pkt.pts;
                    }
                    if ((av_gettime() - t1) >= nnnnnn) {
                        //printf("------ H264,chl='%s',pts=%lld,dts=%lld,duration=%lld,frame_rate=%0.2f,speed_ratio=%0.3f,url='%s'\n", param->chl, in_pkt.pts, in_pkt.dts, in_pkt.duration, avg_frame_rate, (*param->ratio), param->in_url);
                        t1 = av_gettime();
                    }
                    //printf("------ H264,chl='%s',pts=%lld,dts=%lld,duration=%lld,frame_rate=%0.2f,speed_ratio=%0.3f,url='%s'\n",param->chl, in_pkt.pts, in_pkt.dts, in_pkt.duration, avg_frame_rate, (*param->ratio),param->in_url);
                    nRet = av_interleaved_write_frame(ofmt_ctx_mp4, &in_pkt);
                    if (nRet < 0) {
                        av_strerror(nRet, err_msg, sizeof(err_msg));
                        LOG("ERROR", "av_interleaved_write_frame function failed [ret:%d,count:%d,msg:'%s']\n", nRet, write_frame_fail, err_msg);
                        av_packet_unref(&in_pkt);
                        if (write_frame_fail++ >= 25) {
                            nRet = ERROR_WRITE_PKT_FAIL;
                            break;
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
                        av_strerror(nRet, err_msg, sizeof(err_msg));
                        LOG("WARN", "avcodec_send_packet function failed [ret:%d,msg:'%s',chl:'%s',url:'%s']\n", nRet, err_msg, param->chl, param->in_url);
                        nRet = ERROR_DECODE_PACKET_FAIL;
                        break;
                        //continue;
                    }
                    int getframe_ret = 0;
                    while (getframe_ret >= 0) {
                        // H265解码 (H265 -> YUV420p)
                        getframe_ret = avcodec_receive_frame(video_h265_codec_ctx, frame);
                        if ((AVERROR(EAGAIN) == getframe_ret) || (AVERROR_EOF == getframe_ret)) {
                            break;
                        }
                        else if (getframe_ret < 0) {
                            LOG("ERROR", "avcodec_receive_frame function failed %d\n", getframe_ret);
                            nRet = ERROR_DECODE_PACKET_FAIL;;
                            //break;
                            goto end;
                        }
                        if (first_frame_video) {
                            first_frame_video = false;
                            frame->pts = 0;
                            frame->pkt_dts = frame->pts;
                            printf("frame->pts=%lld,frame->pkt_dts=%lld,frame->pkt_duration=%lld\n", frame->pts, frame->pkt_dts, frame->pkt_duration);
                        }
                        else frame->pts = in_pkt.pts;
                        // H264编码 (YUV420p -> H264)
                        nRet = avcodec_send_frame(video_h264_codec_ctx, frame);
                        if (nRet < 0) {
                            char err_msg[128] = { 0 };
                            av_strerror(nRet, err_msg, sizeof(err_msg));
                            LOG("ERROR", "avcodec_send_frame function failed [ret:%d,msg:'%s']\n", nRet, err_msg);
                            nRet = ERROR_ENCODE_PACKET_FAIL;
                            goto end;
                        }
                        int getpacket_ret = 0;
                        while (getpacket_ret >= 0) {
                            // H264编码 (YUV420p -> H264)
                            getpacket_ret = avcodec_receive_packet(video_h264_codec_ctx, &out_pkt);
                            if ((AVERROR(EAGAIN) == getpacket_ret) || (AVERROR_EOF == getpacket_ret)) {
                                //av_strerror(getpacket_ret, err_msg, sizeof(err_msg));
                                //LOG("ERROR", "avcodec_receive_packet function failed [ret:%d,msg:'%s']\n", getpacket_ret, err_msg);
                                av_packet_unref(&out_pkt);
                                break;
                            }
                            else if (getpacket_ret < 0) {
                                LOG("ERROR", "avcodec_receive_packet function failed %d\n", getpacket_ret);
                                break;
                            }
                            //out_pkt.stream_index = in_pkt.stream_index;
                            // 更新 pts,dts
                            out_pkt.pts = av_rescale_q_rnd(out_pkt.pts, in_vstream->time_base, out_vstream->time_base, AV_ROUND_NEAR_INF);
                            out_pkt.dts = av_rescale_q_rnd(out_pkt.dts, in_vstream->time_base, out_vstream->time_base, AV_ROUND_NEAR_INF);
                            //out_pkt.duration = av_rescale_q(out_pkt.duration, in_vstream->time_base, out_vstream->time_base);
                            out_pkt.duration = av_rescale_q(in_pkt.duration, in_vstream->time_base, out_vstream->time_base);
                            out_pkt.pos = -1;
                            if (is_playback && ((1.0 != *param->ratio) || ((param->start_time > 0) && (in_pkt.pts != AV_NOPTS_VALUE)))) {
                                out_pkt.duration = out_pkt.duration / (*param->ratio);
                                out_pkt_pts += out_pkt.duration;
                                out_pkt.pts = out_pkt_pts;
                                out_pkt.dts = out_pkt.pts;
                            }
                            if ((av_gettime() - t1) >= nnnnnn) {
                                ;// printf("------ H265,chl='%s',pts=%lld,dts=%lld,duration=%lld,frame_rate=%0.2f,speed_ratio=%0.3f,url='%s'\n", param->chl, out_pkt.pts, out_pkt.dts, out_pkt.duration, avg_frame_rate, (*param->ratio), param->in_url);
                                t1 = av_gettime();
                            }
                            //printf("------ H265,chl='%s',pts=%lld,dts=%lld,duration=%lld,frame_rate=%0.2f,speed_ratio=%0.3f,url='%s'\n", param->chl, out_pkt.pts, out_pkt.dts, out_pkt.duration, avg_frame_rate, (*param->ratio), param->in_url);
                            nRet = av_interleaved_write_frame(ofmt_ctx_mp4, &out_pkt);
                            if (nRet < 0) {
                                av_strerror(nRet, err_msg, sizeof(err_msg));
                                LOG("ERROR", "av_interleaved_write_frame function failed [ret:%d,count:%d,msg:'%s']\n", nRet, write_frame_fail, err_msg);
                                if (write_frame_fail++ >= 25) {
                                    av_packet_unref(&out_pkt);
                                    nRet = ERROR_WRITE_PKT_FAIL;
                                    goto end;
                                }
                                else continue;
                            }
                            write_frame_fail = 0;
                            av_packet_unref(&out_pkt);
                        }
                    }
                }
                else LOG("WARN","Unknown stream type %d\n", in_vstream->codecpar->codec_id);
                if (is_playback) { // 回放
                    int64_t n = av_gettime() - read_pkt_time;
                    if (n < sp) {
                        av_usleep(sp - n);
                    }
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
    if (NULL != write_cache_buff.cache_buff) {
        if (write_cache_buff.write_size > 0) {
            int ret = param->out_custom(param->out_opaque, write_cache_buff.cache_buff, write_cache_buff.write_size);
            if (ret < 0)
                LOG("ERROR", "ret=%d\n", ret);
        }
        av_free(write_cache_buff.cache_buff);
        write_cache_buff.cache_buff = NULL;
        param->out_custom(param->out_opaque, NULL, 0);
    }
    if (NULL != video_h264_params)
        avcodec_parameters_free(&video_h264_params);
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
    if ((!is_playback) && ('r' == *(control_cmd + 1)) && (FILE_EOF!=nRet)) { // 点播
        int64_t timeout = 1000000 * 60 * 5;  // 重连超时时间(5分钟)
        int ret = 0, reconnect_cnt = 0;
        while ((timeout > 0) && ('r' == *(control_cmd + 1))) {
            printf("------ reconnect %d ...\n", ++reconnect_cnt);
            av_dict_set(&opts, "rtsp_transport", "tcp", 0);
            av_dict_set(&opts, "stimeout", "3000000", 0);
            ret = avformat_open_input(&ifmt_ctx, param->in_url, 0, &opts);
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
    printf("------ thread exit. --------- [ret:%d,control_cmd:'%c',chl:'%s',url:'%s'] ------\n", nRet, *(control_cmd + 1),param->chl, param->in_url);
    return nRet;
}

int playback_stream(char* control_cmd, stream_param_t* param)
{
    if ((NULL == control_cmd) || (NULL == param) || (NULL == g_minio)) {
        LOG("ERROR", "Input parameter error, [control_cmd:%p,param:%p,minio:%p]\n", control_cmd, param, g_minio);
        return -1;
    }
    //avdevice_register_all();
    avformat_network_init();
    int nRet = -1;
    AVFormatContext* ifmt_ctx = NULL;
    AVFormatContext* ofmt_ctx_mp4 = NULL;
    AVCodecParameters* video_h264_params = NULL;
    bool is_playback = false;
    char err_msg[64] = {0};
    CThread11 download_thd;
    CBuffer<char> data_buffer;
    do {
        if ((NULL != param->in_url) && (NULL == param->in_custom)) { // 回放-本地文件
            nRet = avformat_open_input(&ifmt_ctx, param->in_url, NULL, NULL);
        }
        else if ((NULL != param->in_url) && (NULL != param->in_custom)) { // 回放-MinIO
            is_playback = true;
            g_io.enable = true;
            g_io.fuzz_size = g_io.pos = 0;
            g_io.remote_name = param->in_url;
            g_io.filesize = g_minio->get_file_size(g_io.remote_name);
            if (g_io.filesize <= 0) {
                LOG("ERROR", "File size %lld\n", g_io.filesize);
                break;
            }
            g_io.fuzz = new uint8_t[BUFF_SIZE + 1];
            memset(g_io.fuzz, 0, BUFF_SIZE + 1);
            printf("------ in.buff_size = %lld ------\n\n", BUFF_SIZE);
            nRet = data_buffer.Create(50, create_object_cbk, destory_object_cbk, NULL, 2048, 50);
            if (nRet < 0) {
                LOG("ERROR", "Create buffer failed [ret:%d]\n", nRet);
                break;
            }
            ifmt_ctx = avformat_alloc_context();
            unsigned char* buffer = (unsigned char*)av_malloc(BUFF_SIZE);
            AVIOContext* avio_ctx = avio_alloc_context(buffer, BUFF_SIZE, 0, &data_buffer, param->in_custom, NULL, seek_packet);
            if (NULL == avio_ctx) {
                av_free(buffer);
                LOG("ERROR", "Allocation memory failed %p\n", avio_ctx);
                break;
            }
            ifmt_ctx->pb = avio_ctx;
            ifmt_ctx->flags |= AVFMT_FLAG_CUSTOM_IO;
            printf("\033[32m ------ in.buffer_size:%d ------ \033[0m\n\n", ifmt_ctx->pb->buffer_size);
            nRet = avformat_open_input(&ifmt_ctx, "", NULL, NULL);
        }
        else {
            LOG("ERROR","in_url=%p, in_custom=%p\n", param->in_url, param->in_custom);
            break;
        }
        if ((nRet < 0) || (NULL == ifmt_ctx)) {
            av_strerror(nRet, err_msg, sizeof(err_msg));
            LOG("ERROR", "avformat_open_input function failed [ret:%d,msg:'%s',url:'%s']\n", nRet, err_msg, param->in_url);
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
        av_dump_format(ifmt_ctx, 0, param->in_url, 0);
        AVStream* in_vstream = ifmt_ctx->streams[video_index];
        AVStream* in_astream = (-1 != audio_index) ? ifmt_ctx->streams[audio_index] : NULL;
        if (is_playback && (param->start_time >= 0)) {
            int64_t start_video_dts = in_vstream->start_time + (int64_t)(param->start_time / av_q2d(in_vstream->time_base));
            nRet = av_seek_frame(ifmt_ctx, video_index, start_video_dts, AVSEEK_FLAG_FRAME); //AVSEEK_FLAG_FRAME,AVSEEK_FLAG_BACKWARD
            printf("\033[32m ------ seek_frame %llds ------ \033[0m\n\n", param->start_time);
            if (nRet < 0) {
                av_strerror(nRet, err_msg, sizeof(err_msg));
                LOG("ERROR", "av_seek_frame function failed [ret:%d,start_time:%lld,dts:%lld,msg:'%s']\n", nRet, param->start_time, start_video_dts, err_msg);
                break;
            }
        }
        //////////////////////////////////////////////////////////////////////////////////////////
        // Output MP4
        nRet = avformat_alloc_output_context2(&ofmt_ctx_mp4, NULL, "mp4", param->out_url);
        if ((nRet < 0) || (NULL == ofmt_ctx_mp4)) {
            av_strerror(nRet, err_msg, sizeof(err_msg));
            LOG("ERROR", "avformat_alloc_output_context2 function failed [ret:%d,msg:'%s']\n", nRet, err_msg);
            nRet = ERROR_OPEN_OUTSTREAM_FAIL;
            break;
        }
        if (NULL != param->out_url) {
            if (!(ofmt_ctx_mp4->oformat->flags & AVFMT_NOFILE)) {
                nRet = avio_open2(&ofmt_ctx_mp4->pb, param->out_url, AVIO_FLAG_READ_WRITE, NULL, NULL);
                if (nRet < 0) {
                    av_strerror(nRet, err_msg, sizeof(err_msg));
                    LOG("ERROR", "avio_open2 function failed [ret:%d,msg:'%s']\n", nRet, err_msg);
                    break;
                }
            }
        }
        else {
            int buf_size = 524288; // BUFF_SIZE
            unsigned char* buffer = (unsigned char*)av_malloc(buf_size);
            if (NULL == buffer) {
                LOG("ERROR", "av_malloc function failed\n");
                nRet = ERROR_MEM_FAIL;
                break;
            }
            AVIOContext* avio_out = avio_alloc_context(buffer, buf_size, 1, param, NULL, playback_write_packet, NULL);
            if (NULL == avio_out) {
                LOG("ERROR", "avio_alloc_context function failed.\n");
                av_free(buffer);
                nRet = ERROR_IOCONTENT_FAIL;
                break;
            }
            ofmt_ctx_mp4->pb = avio_out;
            ofmt_ctx_mp4->flags |= AVFMT_FLAG_CUSTOM_IO;
            printf("\033[32m ------ out.buffer_size:%d ------ \033[0m\n\n", ofmt_ctx_mp4->pb->buffer_size);
        }
        ////////////////////////////////////////////////////////////////
        AVCodecContext* video_h265_codec_ctx = NULL;
        AVCodecContext* video_h264_codec_ctx = NULL;
        AVCodec* video_h264_codec = NULL; // H264解码或编码器
        video_h264_params = avcodec_parameters_alloc();
        if (NULL == video_h264_params) {
            LOG("ERROR", "avcodec_parameters_alloc function failed.\n");
            nRet = -100;
            break;
        }
        if (AV_CODEC_ID_H264 == in_vstream->codecpar->codec_id) {
            video_h264_codec = avcodec_find_decoder(in_vstream->codecpar->codec_id);
            avcodec_parameters_copy(video_h264_params, in_vstream->codecpar);
        }
        else if (AV_CODEC_ID_H265 == in_vstream->codecpar->codec_id) {
            /////////////////////////////////////////////////////////////////////////////////
            // 查找H265视频解码器 (H265 -> YUV420p)
            AVCodec* in_video_codec = avcodec_find_decoder(in_vstream->codecpar->codec_id);
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
            video_h265_codec_ctx->thread_count = 16;//设置解码线程数量
            // 打开H265视频解码器
            nRet = avcodec_open2(video_h265_codec_ctx, in_video_codec, NULL);
            if (nRet < 0) {
                LOG("ERROR", "avcodec_open2 function failed %d\n", nRet);
                nRet = ERROR_OPEN_CODE_FAIL;
                break;
            }
            /////////////////////////////////////////////////////////////////////////////////
            // 查找H264视频编码器 (YUV420p -> H264)
            video_h264_codec = avcodec_find_encoder(AV_CODEC_ID_H264);
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
            video_h264_codec_ctx->codec_id = video_h264_codec->id; // AV_CODEC_ID_H264
            video_h264_codec_ctx->width = video_h265_codec_ctx->width;
            video_h264_codec_ctx->height = video_h265_codec_ctx->height;
            video_h264_codec_ctx->codec_type = AVMEDIA_TYPE_VIDEO;
            video_h264_codec_ctx->pix_fmt = AV_PIX_FMT_YUV420P;// video_h265_codec_ctx->pix_fmt, AV_PIX_FMT_YUV420P
            // 25 frame a second
            video_h264_codec_ctx->time_base.num = 1;
            video_h264_codec_ctx->time_base.den = 25;
            video_h264_codec_ctx->framerate.num = 25;    // 帧率
            video_h264_codec_ctx->framerate.den = 1;
            //video_h264_codec_ctx->bit_rate = video_h265_codec_ctx->bit_rate * 2;//h264 byterate
            video_h264_codec_ctx->bit_rate = 1600000;
            video_h264_codec_ctx->gop_size = 50;    // I帧间隔,i frame interval
            video_h264_codec_ctx->keyint_min = 50;  // 最小I间隔
            video_h264_codec_ctx->qmin = 10;
            video_h264_codec_ctx->qmax = 51;
            //video_h264_codec_ctx->qcompress = 1; // zk
            //video_h264_codec_ctx->max_b_frames = 3; // zk
            //video_h264_codec_ctx->max_b_frames = 1; // zk
            video_h264_codec_ctx->thread_count = 16;//设置编码线程数量
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
        AVStream* out_vstream = avformat_new_stream(ofmt_ctx_mp4, video_h264_codec);
        if (NULL == video_stream) {
            LOG("ERROR", "avformat_new_stream function failed.\n");
            nRet = ERROR_CREATE_AVSTREAM_FAIL;
            break;
        }
        nRet = avcodec_parameters_copy(out_vstream->codecpar, video_h264_params);
        if (nRet < 0) {
            LOG("ERROR", "avcodec_parameters_copy function failed [ret:%d]\n", nRet);
            break;
        }
        //out_vstream->time_base = video_h264_codec_ctx->time_base;
        out_vstream->time_base = in_vstream->time_base;
        out_vstream->codecpar->codec_tag = 0;
        if (ofmt_ctx_mp4->oformat->flags & AVFMT_GLOBALHEADER)
            ;// video_stream->codec->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;

        // new 一个音频流
        AVStream* out_astream = NULL;
        if (audio_index >= 0) {
            const AVCodec* a_codec = avcodec_find_decoder(in_astream->codecpar->codec_id);
            out_astream = avformat_new_stream(ofmt_ctx_mp4, a_codec);
            if (NULL == out_astream) {
                LOG("ERROR", "avformat_new_stream function failed.\n");
                nRet = ERROR_CREATE_AVSTREAM_FAIL;
                break;
            }
            nRet = avcodec_parameters_copy(out_astream->codecpar, in_astream->codecpar);
            if (nRet < 0) {
                LOG("ERROR", "avcodec_parameters_copy function failed [ret:%d]\n", nRet);
                break;
            }
            out_astream->codecpar->codec_tag = 0;
            if (ofmt_ctx_mp4->oformat->flags & AVFMT_GLOBALHEADER)
                ;// audio_stream->codec->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;
        }
        av_dump_format(ofmt_ctx_mp4, 0, 0, 1);
        //////////////////////////////////////////////////////////////////////////////////////////
        // 发送流的类型
        if (NULL != param->out_custom) {
            int len = 0;
            if ((video_index >= 0) && (-1 == audio_index))
                len = VIDEO_ONLY;// 视频流
            else if ((video_index >= 0) && (audio_index >= 0))
                len = VIDEO_AND_AUDIO; // 复合流
            uint8_t buffer[24] = {0};
            #ifndef COMPILE_EXE
            nRet = param->out_custom(param->out_opaque, buffer, len);
            #endif
        }
        ///////////////////////////////////////////////////////////////////////////////////////////
        AVDictionary *opts = NULL;
        av_dict_set(&opts, "movflags", "frag_keyframe+empty_moov+omit_tfhd_offset+faststart+separate_moof+disable_chpl+default_base_moof+dash", 0);
        nRet = avformat_write_header(ofmt_ctx_mp4, &opts);
        av_dict_free(&opts);
        opts = NULL;
        if (nRet < 0) {
            av_strerror(nRet, err_msg, sizeof(err_msg));
            LOG("ERROR", "avformat_write_header function failed [ret:%d,msg:'%s']\n", nRet, err_msg);
            nRet = ERROR_OPEN_OUTSTREAM_FAIL;
            break;
        }
        AVPacket in_pkt;
        av_init_packet(&in_pkt);
        AVPacket out_pkt;
        av_init_packet(&out_pkt);
        AVFrame* frame = av_frame_alloc();

        time_t t1 = 0, nnnnnn = 30;

        int read_frame_fail = 0, write_frame_fail = 0;
        bool first_video = true, first_audio = true;
        int64_t v_pts = 0, a_pts = 0;
        int64_t video_number = 0, audio_number = 0, read_pkt_time = 0, out_pts = 0,duration = 0;
        int sp = (1000000.0 / av_q2d(in_vstream->avg_frame_rate)); 
        double avg_frame_rate = av_q2d(in_vstream->avg_frame_rate); // 平均视频帧率
        int64_t frame_number = 0;
        nRet = av_read_frame(ifmt_ctx, &in_pkt);
        printf("\033[32m ------ sp=%d, pos=%d ------ \033[0m\n\n", sp, in_pkt.pos);

        if (is_playback)
            download_thd.Run(download_thdproc, &data_buffer);
        while (('r' == *(control_cmd + 1)) || ('p' == *(control_cmd + 1))) {// 'r': 运行或继续(run), 'p': 暂停 (pause), 's': 停止(stop)
            if ('p' == *(control_cmd + 1)) { // 暂停
                av_usleep(500000);
                continue;
            }
            read_pkt_time = av_gettime();
            if (in_pkt.size <= 0)
                nRet = av_read_frame(ifmt_ctx, &in_pkt);
            else printf("pkt.size %d\n", in_pkt.size);
            if (nRet < 0) {
                if (nRet == AVERROR_EOF) {
                    LOG("ERROR", "File reading end %d.\n", nRet);
                    nRet = FILE_EOF;
                    break;
                }
                else {
                    av_strerror(nRet, err_msg, sizeof(err_msg));
                    LOG("ERROR", "av_read_frame failed [ret:%d,msg:'%s',count:%d]\n", nRet, err_msg, read_frame_fail);
                    av_packet_unref(&in_pkt);
                    if (read_frame_fail++ >= 25) { // read frame failed
                        nRet = ERROR_READ_FRAME_FAIL;
                        break;
                    }
                    continue;
                }
            }
            read_frame_fail = 0;
#if 0
            if (video_index == in_pkt.stream_index) { // 视频流
                if (first_video) {
                    first_video = false;
                    in_pkt.dts = in_pkt.pts = 0;
                }
                #if 1
                    else if (in_pkt.pts == AV_NOPTS_VALUE) {
                        in_pkt.duration = (double)AV_TIME_BASE / av_q2d(in_vstream->r_frame_rate);
                        in_pkt.pts = ++video_number * in_pkt.duration;
                        in_pkt.dts = in_pkt.pts;
                        printf("\033[32m video pts=%lld,dts=%lld,duration=%lld \033[0m\n", in_pkt.pts, in_pkt.dts, in_pkt.duration);
                    }
                #else
                    else if ((in_pkt.pts == AV_NOPTS_VALUE) || (param->start_time > 0) || (1.0 != *param->ratio)) {
                        in_pkt.duration = in_vstream->time_base.den / av_q2d(in_vstream->r_frame_rate);
                        if (1.0 != *param->ratio) {
                            in_pkt.duration = in_pkt.duration / (*param->ratio);
                            video_number += in_pkt.duration;
                            in_pkt.pts = video_number;
                        }
                        else in_pkt.pts = ++video_number * in_pkt.duration;
                        in_pkt.dts = in_pkt.pts;
                        //printf("video pts=%lld,dts=%lld,duration=%lld\n", in_pkt.pts, in_pkt.dts, in_pkt.duration);
                    }
                #endif
            }
            else if (audio_index == in_pkt.stream_index) { // 音频流
                //if (is_playback && (1.0 != *param->ratio)) { // 回放 (倍率不为1.0时,音频可以去掉)
                //    av_packet_unref(&in_pkt);
                //    continue;
                //}
                if (first_audio) {
                    first_audio = false;
                    in_pkt.dts = in_pkt.pts = 0;
                }
                else if (in_pkt.pts == AV_NOPTS_VALUE) {
                    in_pkt.duration = (double)AV_TIME_BASE / av_q2d(in_astream->r_frame_rate);
                    in_pkt.pts = ++audio_number * in_pkt.duration;
                    in_pkt.dts = in_pkt.pts;
                    printf("\033[32m audio pts=%lld,dts=%lld,duration=%lld \033[0m\n", in_pkt.pts, in_pkt.dts, in_pkt.duration);
                }
            }
            else { // 其他流
                printf(" other stream.\n");
                av_packet_unref(&in_pkt);
                continue;
            }
#else
            if ((video_index != in_pkt.stream_index) && (audio_index != in_pkt.stream_index)) {
                printf(" other stream %d [video_index:%d, audio_index:%d]\n", in_pkt.stream_index, video_index, audio_index);
                av_packet_unref(&in_pkt);
                continue;
            }
#endif
            if (in_pkt.stream_index == audio_index) { // 音频
                //////////////////////////////////////////
                in_pkt.dts = in_pkt.pts = a_pts;
                if (in_pkt.duration <= 0)
                    in_pkt.duration = in_astream->time_base.den / av_q2d(in_vstream->r_frame_rate);

                if (is_playback && (1.0 != *param->ratio))
                    in_pkt.duration = in_pkt.duration / *param->ratio;
                a_pts += in_pkt.duration;
                av_packet_rescale_ts(&in_pkt, in_astream->time_base, out_astream->time_base);
                ////////////////////////////////////////////

                nRet = av_interleaved_write_frame(ofmt_ctx_mp4, &in_pkt);
                av_packet_unref(&in_pkt);
                if (nRet < 0) {
                    av_strerror(nRet, err_msg, sizeof(err_msg));
                    LOG("ERROR", "av_interleaved_write_frame function failed [ret:%d,count:%d,msg:'%s']\n", nRet, write_frame_fail, err_msg);
                    if (write_frame_fail++ >= 25) {
                        nRet = ERROR_WRITE_PKT_FAIL;
                        break;
                    }
                    else continue;
                }
                write_frame_fail = 0;
            }
            else {
                if (AV_CODEC_ID_H264 == in_vstream->codecpar->codec_id) {
                    ////////////////////////////////////////////////
                    in_pkt.dts = in_pkt.pts = v_pts;
                    if (in_pkt.duration <= 0)
                        in_pkt.duration = in_vstream->time_base.den / av_q2d(in_vstream->r_frame_rate);
                    if (is_playback && (1.0 != *param->ratio))
                        in_pkt.duration = in_pkt.duration / *param->ratio;
                    v_pts += in_pkt.duration;
                    /////////////////////////////////////////////

                    av_packet_rescale_ts(&in_pkt, in_vstream->time_base, out_vstream->time_base);
                    nRet = av_interleaved_write_frame(ofmt_ctx_mp4, &in_pkt);
                    if (nRet < 0) {
                        av_strerror(nRet, err_msg, sizeof(err_msg));
                        LOG("ERROR", "av_interleaved_write_frame function failed [ret:%d,count:%d,msg:'%s']\n", nRet, write_frame_fail, err_msg);
                        if (write_frame_fail++ >= 25) {
                            nRet = ERROR_WRITE_PKT_FAIL;
                            break;
                        }
                    }
                    write_frame_fail = 0;
                }
                else if (AV_CODEC_ID_H265 == in_vstream->codecpar->codec_id) {
                    // H265解码 (H265 -> YUV420p)
                    nRet = avcodec_send_packet(video_h265_codec_ctx, &in_pkt);
                    if (nRet < 0) {
                        av_strerror(nRet, err_msg, sizeof(err_msg));
                        LOG("WARN", "avcodec_send_packet function failed [ret:%d,msg:'%s',chl:'%s',url:'%s']\n", nRet, err_msg, param->chl, param->in_url);
                        nRet = ERROR_DECODE_PACKET_FAIL;
                        break;
                        //continue;
                    }
                    int getframe_ret = 0;
                    while (getframe_ret >= 0) {
                        // H265解码 (H265 -> YUV420p)
                        getframe_ret = avcodec_receive_frame(video_h265_codec_ctx, frame);
                        if ((AVERROR(EAGAIN) == getframe_ret) || (AVERROR_EOF == getframe_ret)) {
                            break;
                        }
                        else if (getframe_ret < 0) {
                            LOG("ERROR", "avcodec_receive_frame function failed %d\n", getframe_ret);
                            nRet = ERROR_DECODE_PACKET_FAIL;;
                            //break;
                            goto end;
                        }
                        // H264编码 (YUV420p -> H264)
                        nRet = avcodec_send_frame(video_h264_codec_ctx, frame);
                        if (nRet < 0) {
                            char err_msg[128] = { 0 };
                            av_strerror(nRet, err_msg, sizeof(err_msg));
                            LOG("ERROR", "avcodec_send_frame function failed [ret:%d,msg:'%s']\n", nRet, err_msg);
                            nRet = ERROR_ENCODE_PACKET_FAIL;
                            goto end;
                        }
                        int getpacket_ret = 0;
                        while (getpacket_ret >= 0) {
                            // H264编码 (YUV420p -> H264)
                            getpacket_ret = avcodec_receive_packet(video_h264_codec_ctx, &out_pkt);
                            if ((AVERROR(EAGAIN) == getpacket_ret) || (AVERROR_EOF == getpacket_ret)) {
                                //av_strerror(getpacket_ret, err_msg, sizeof(err_msg));
                                //LOG("ERROR", "avcodec_receive_packet function failed [ret:%d,msg:'%s']\n", getpacket_ret, err_msg);
                                av_packet_unref(&out_pkt);
                                break;
                            }
                            else if (getpacket_ret < 0) {
                                LOG("ERROR", "avcodec_receive_packet function failed %d\n", getpacket_ret);
                                break;
                            }
                            out_pkt.stream_index = video_index;
                            if (out_pkt.duration <= 0)
                                out_pkt.duration = in_pkt.duration;
                            //av_packet_rescale_ts(&out_pkt, in_vstream->time_base, out_vstream->time_base);
                            //if (1.0 != *param->ratio)
                            //    out_pkt.duration = out_pkt.duration / (*param->ratio);
                            //out_pkt.pts = out_pts;
                            //out_pkt.dts = out_pkt.pts;
                            //out_pts += out_pkt.duration;
                            //////////////////////////////////////////////////////
                            out_pkt.dts = out_pkt.pts = v_pts;
                            if (is_playback && (1.0 != *param->ratio))
                                out_pkt.duration = out_pkt.duration / *param->ratio;
                            v_pts += out_pkt.duration;
                            av_packet_rescale_ts(&out_pkt, in_vstream->time_base, out_vstream->time_base);
                         
                            if ((time(NULL) - t1) >= nnnnnn) {
                                printf("\033[32mpts=%lld,dts=%lld,duration=%lld,frame_rate=%0.2f,speed_ratio=%0.3f,current:%lld\033[0m\n", out_pkt.pts, out_pkt.dts, out_pkt.duration, avg_frame_rate, (*param->ratio), time(NULL));
                                t1 = time(NULL);
                            }
                            nRet = av_interleaved_write_frame(ofmt_ctx_mp4, &out_pkt);
                            if (nRet < 0) {
                                av_strerror(nRet, err_msg, sizeof(err_msg));
                                LOG("ERROR", "av_interleaved_write_frame function failed [ret:%d,count:%d,msg:'%s']\n", nRet, write_frame_fail, err_msg);
                                if (write_frame_fail++ >= 25) {
                                    av_packet_unref(&out_pkt);
                                    nRet = ERROR_WRITE_PKT_FAIL;
                                    goto end;
                                }
                                else continue;
                            }
                            write_frame_fail = 0;
                            av_packet_unref(&out_pkt);
                        }
                    }
                }
                else {
                    LOG("WARN", "Unknown codec_id %d \n", in_vstream->codecpar->codec_id);
                }
                if (is_playback) { // 回放
                    int64_t n = av_gettime() - read_pkt_time;
                    //int64_t threshold = (1.0 != *param->ratio) ? (sp / *param->ratio) : sp;
                    //if (n < threshold)
                    //    av_usleep(sp - n);
                    if (n < sp)
                         av_usleep(sp - n);
                }
                av_packet_unref(&in_pkt);
            }
            //av_packet_unref(&in_pkt);
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
        if (NULL != g_io.fuzz) {
            delete[] g_io.fuzz;
            g_io.fuzz = NULL;
        }
    }
    if (NULL != video_h264_params)
        avcodec_parameters_free(&video_h264_params);
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
    printf("------ thread exit. --------- [ret:%d,control_cmd:'%c',chl:'%s',url:'%s'] ------\n", nRet, *(control_cmd + 1),param->chl, param->in_url);
    return nRet;
}
int64_t seek_packet(void* opaque, int64_t offset, int whence)
{
    if (whence == SEEK_CUR) {
        printf("SEEK_CUR\n");
        if (offset > INT64_MAX - g_io.pos)
            return -1;
        offset += g_io.pos;
    }
    else if (whence == SEEK_END) {
        printf("SEEK_END\n");
        if (offset > INT64_MAX - g_io.filesize)
            return -1;
        offset += g_io.filesize;
    }
    else if (whence == AVSEEK_SIZE) {
        //printf("%d  whence=%d,   offset=%lld\n", i++, whence, offset);
        return g_io.filesize;
    }
    if (offset < 0 || offset > g_io.filesize)
        return -1;
    static int64_t i = 1;
    if (i == 1)
        printf("%d  whence=%d,   offset=%lld->%lld (%lld), (tail:%lld)\n", i++, whence, g_io.pos, offset, ((offset - g_io.pos)), (g_io.filesize - offset));
    else
        printf("%d  whence=%d,   offset=%lld->%lld (%0.2fk)\n", i++, whence, g_io.pos, offset, (double)(((double)(offset - g_io.pos)) / 1024.0));
    //io->fuzz += offset - io->pos;
    //io->fuzz_size -= offset - io->pos;
    g_io.pos = offset;
    return 0;
}
int read_packet(void* opaque, uint8_t* buf, int buf_size)
{
    if (buf_size > BUFF_SIZE) {
        LOG("ERROR", "buf_size:%d, BUFF_SIZE:%d\n", buf_size, BUFF_SIZE);
        buf_size = BUFF_SIZE;
    }
    if (g_io.enable) {
        if (g_io.fuzz_size <= 0) {
            int64_t end = g_io.pos + buf_size;
            if (end > g_io.filesize)
                end = g_io.filesize;
            if(g_io.pos < end)
                g_io.fuzz_size = g_minio->download(g_io.remote_name, g_io.pos, end - 1, minio_callback, &g_io);
        }
        int size = FFMIN(buf_size, g_io.fuzz_size);
        if (!g_io.fuzz_size) {
            printf("read packet EOF [fuzz_size:%lld,pos:%lld,filesize:%lld]\n", g_io.fuzz_size, g_io.pos, g_io.filesize);
            g_io.filesize = FFMIN(g_io.pos, g_io.filesize);
            return AVERROR_EOF;
        }
        if (g_io.pos > INT64_MAX - size) {
            printf("read packet EIO [pos:%lld,size:%lld]\n", g_io.pos, size);
            return AVERROR(EIO);
        }
        memcpy(buf, g_io.fuzz, size);
        //io->fuzz += size;
        g_io.fuzz_size -= size;
        g_io.pos += size;
        //io->filesize = FFMAX(io->filesize, io->pos);
        return size;
    }
    else {
        CBuffer<char>* buffer = (CBuffer<char>*)opaque;
        if (buffer->GetDataNodeCount() <= 0) {
            if (buffer->m_flag) {
                printf("wait data ...\n");
                time_t t1 = time(NULL);
                unique_lock<mutex> lock(g_mutex);
                g_cv.wait(lock);
                printf("wait data %ds\n", (int)(time(NULL)-t1));
            }
            else return AVERROR_EOF;
        }
        int bytes = buffer->Read((char*)buf, buf_size);
        if (bytes > 0) {
            if (bytes != buf_size)
                LOG("WARN", "bytes=%d, buf_size=%d\n", bytes, buf_size);
        }
        else {
            LOG("WARN","Read buffer %d bytes [DataNodeCount:%d]\n", bytes, buffer->GetDataNodeCount());
        }
        return bytes;
    }
}
int playback_write_packet(void* opaque, unsigned char* buf, int buf_size)
{
    stream_param_t* params = (stream_param_t*)opaque;
    static int64_t i = 0;
    if (NULL != params->out_custom) {
        int64_t t1 = av_gettime();
        params->out_custom(params->out_opaque, buf, buf_size);
        //printf("%d playback_write_packet  size:%d,  websocket_time:%lld, current:%lld\n", i++, buf_size, (av_gettime() - t1)/1000, time(NULL));
    }
    else printf("%d playback_write_packet  size:%d,  current:%lld\n", i++, buf_size, time(NULL));
    return buf_size;
}
void STDCALL download_thdproc(STATE& state, PAUSE& p, void* pUserData)
{
    printf("------ download start ...  [pos:%lld] ------\n\n", g_io.pos);
    g_io.enable = false;
    CBuffer<char>* buffer = (CBuffer<char>*)pUserData;
    int i = 0;
    int data_node_count = 10;
    while (TS_STOP != state && (g_io.pos < g_io.filesize)) {
        if (TS_PAUSE == state)
            p.wait();
        if (buffer->GetDataNodeCount() < data_node_count) {
            int upload_count = 5;
            while (upload_count--) {
                int64_t end = g_io.pos + BUFF_SIZE;
                if (end > g_io.filesize)
                    end = g_io.filesize;
                if (g_io.pos >= g_io.filesize) {
                    LOG("WARN", "download completes %lld-%lld (%lld) [DataNodeCount:%d]\n", g_io.pos, end, (end - g_io.pos), buffer->GetDataNodeCount());
                    break;
                }
                int64_t bytes = g_minio->download(g_io.remote_name, g_io.pos, end - 1, minio_callback, buffer);
                if (bytes > 0) {
                    g_cv.notify_all();
                    int64_t pos = g_io.pos;
                    g_io.pos += bytes;
                }
                else LOG("ERROR", "%d Download failed %lld-%lld (%lld) [DataNodeCount:%d]\n", i++, g_io.pos, end - 1, (end - g_io.pos), buffer->GetDataNodeCount());
            }
        }
        else {
            Sleep(1000);
        }
    }
    buffer->m_flag = false;
    printf("------ download end ------\n\n");
}
int STDCALL minio_callback(void* data, size_t size, void* arg)
{
    if (g_io.enable) {
        memcpy(g_io.fuzz, data, size);
    }
    else {
        CBuffer<char>* buffer = (CBuffer<char>*)arg;
        buffer->Write((char*)data, size);
    }
    return 0;
}
int create_object_cbk(void*& obj, int& obj_size, void* user_data)
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
void destory_object_cbk(void*& obj, void* user_data)
{
    delete[] obj;
}
