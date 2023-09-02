
#include <stdio.h>
#include<string.h>
#include "media_common.h"
#ifdef _WIN32
//Windows
extern "C"
{
#include "libavformat/avformat.h"
#include  "libavformat/avio.h"
#include  "libavcodec/avcodec.h"
#include  "libavutil/avutil.h"
#include  "libavutil/time.h"
};
#else
//Linux...
#ifdef __cplusplus
extern "C"
{
#endif
#include <libavformat/avformat.h>
#include <libavformat/avio.h>
#include  <libavcodec/avcodec.h>
#include  <libavutil/avutil.h>
#include  <libavutil/time.h>
#ifdef __cplusplus
};
#endif
#endif

enum error_code{
    NO_ERROR=0,
    ERROR_ARG,
    ERROR_OPEN_URL_FAIL,
    ERROR_OPEN_FILE_FAIL,
    ERROR_STREAM_INFO_FAIL,
    ERROR_OPEN_OUTSTREAM_FAIL,
    ERROR_CREATE_AVSTREAM_FAIL,
    ERROR_WRITE_PKT_FAIL,
    ERROR_NO_VIDEO_STREAM,
    ERROR_SEEK_FAIL,
    ERROR_READ_FRAME_FAIL,
    ERROR_MEM_FAIL,
    ERROR_IOCONTENT_FAIL
};

typedef struct _stream_cache_buff
{
    uint8_t* cache_buff;
    uint8_t* pos;
    uint8_t* limit;
    int write_size;
} stream_cache_buff;


int write_cache(void *opaque, unsigned char *buf, int buf_size)
{
    stream_cache_buff* cache_write=(stream_cache_buff*)opaque;
    if(cache_write->pos+buf_size>cache_write->limit){
        av_log(NULL,AV_LOG_ERROR,"ERROR write cache buff over limit");
        return -1;
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
    int ret;
    bool first_video=false,first_audio=false,first_lgnite=false;
    unsigned int index,video_index=-1,audio_index=-1;
    AVPacket pkt;
    AVDictionary *opts = NULL;
    AVFormatContext *ifmt_ctx = NULL,*ofmt_ctx_mp4=NULL;
    error_code return_error=NO_ERROR;
    int buffsize=32768;
    unsigned char* outbuffer=NULL;
    AVIOContext *avio_out=NULL;
    stream_cache_buff  write_cache_buff;
    int64_t last_cache_pts=0,now_pts=0;

    if(rtsp_url==NULL||fun_arg==NULL||writedata_dealfun==NULL||control_cmd==NULL)
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
    avio_out =avio_alloc_context(outbuffer, buffsize,1,&write_cache_buff,NULL,write_cache,NULL);
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
    av_dict_set(&opts, "movflags", "frag_keyframe+empty_moov", 0);
    //Write file header
    if (avformat_write_header(ofmt_ctx_mp4, &opts) < 0) {
        printf( "Error occurred when opening output file\n");
        goto end;
    }
    av_dict_free(&opts);

    //read an write
    while (*(control_cmd+1)=='r') {
        if ((ret=av_read_frame(ifmt_ctx, &pkt))< 0){
            av_log(ifmt_ctx,AV_LOG_ERROR,"ERROR fail read a frame in %s,error is \n",rtsp_url,ret);
            return_error = ERROR_READ_FRAME_FAIL;
            break;
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

            AVRational time_base=ifmt_ctx->streams[video_index]->time_base;
            AVRational time_base_q={1,AV_TIME_BASE};
            now_pts = av_rescale_q((pkt.pts-last_cache_pts), time_base, time_base_q);
            //printf("now_pts is %ld\n",now_pts);
            if(now_pts>=CACHE_TIME_SECOND*AV_TIME_BASE){
                //mark lgnite start write
               if(first_lgnite==false)
               {
                   printf("mark start\n");
                   ret=writedata_dealfun(fun_arg,NULL,0);
                   if(ret<0)
                       av_log(ofmt_ctx_mp4,AV_LOG_ERROR,"ERROR Fail to mark  %s start write cache \n",rtsp_url);
                   first_lgnite=true;
               }
               printf("write cache\n");
               ret=writedata_dealfun(fun_arg,write_cache_buff.cache_buff,write_cache_buff.write_size);
               if(ret<0)
                    av_log(ofmt_ctx_mp4,AV_LOG_ERROR,"ERROR Fail to send  %s  cache \n",rtsp_url);
               write_cache_buff.pos=write_cache_buff.cache_buff;
               write_cache_buff.write_size=0;
               last_cache_pts=pkt.pts;
            }
        }
        if ((ret=av_interleaved_write_frame(ofmt_ctx_mp4, &pkt))< 0) {
            av_log(ofmt_ctx_mp4,AV_LOG_ERROR,"ERROR Fail to write packet %s,error is %d\n",rtsp_url,ret);
            return_error = ERROR_WRITE_PKT_FAIL;
            av_packet_unref(&pkt);
            break;
        }
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
    return return_error;
}


/*
get stream from slice and tranfer a fmp4 file
slice_file slice file name
start_time play start time
fun_arg write funtion arg
writedata_dealfun write funtion pointer
*/
int slice_mp4(const char* slice_file,int start_time,void* fun_arg,write_stream writedata_dealfun,char* control_cmd)
{
    int ret,video_index=-1,audio_index=-1;
    unsigned int index;
    AVFormatContext *ifmt_ctx = NULL,*ofmt_ctx_mp4=NULL;
    int64_t start_video_dts=0;
    AVDictionary *opts = NULL;
    AVPacket pkt;
    error_code return_error=NO_ERROR;
    int buffsize=32768;
    unsigned char* outbuffer=NULL;
    AVIOContext *avio_out=NULL;
    int64_t play_time=0,play_pts=0,now_pts,now_time,last_cache_pts=0;
    bool first_video_packet=false,first_lgnite=false;
    stream_cache_buff  write_cache_buff;

    if(slice_file==NULL||start_time<0||fun_arg==NULL||writedata_dealfun==NULL||control_cmd==NULL)
        return ERROR_ARG;


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
        av_log(ifmt_ctx,AV_LOG_ERROR,"ERROR create %s output fail\n");
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
    avio_out =avio_alloc_context(outbuffer, buffsize,1,&write_cache_buff,NULL,write_cache,NULL);
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
        AVStream *in_stream = ifmt_ctx->streams[index];
        AVCodec* codec= avcodec_find_decoder(in_stream->codecpar->codec_id);
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


    //print out stream format
    av_dump_format(ofmt_ctx_mp4, 0, 0, 1);

    //fmp4
    av_dict_set(&opts, "movflags", "frag_keyframe+empty_moov", 0);
    //Write file header
    if (avformat_write_header(ofmt_ctx_mp4, &opts) < 0) {
        printf( "Error occurred when opening output file\n");
        goto end;
    }
    av_dict_free(&opts);

    //read an write
    play_time=av_gettime();
    while (*(control_cmd+1)=='r') {
        if ((ret=av_read_frame(ifmt_ctx, &pkt) )< 0)
        {
            // slice end
            if(ret==AVERROR_EOF ){

                av_log(ifmt_ctx,AV_LOG_INFO,"INFO read a slice  %s end\n",slice_file);
                return_error = ERROR_READ_FRAME_FAIL;
                ret=writedata_dealfun(fun_arg,write_cache_buff.cache_buff,write_cache_buff.write_size);
                if(ret<0)
                     av_log(ofmt_ctx_mp4,AV_LOG_ERROR,"ERROR Fail to send  %s  cache \n",slice_file);
                break;
            }
            else
            {
                av_log(ifmt_ctx,AV_LOG_ERROR,"ERROR fail read a frame in %s,error is %d\n",slice_file,ret);
                return_error = ERROR_READ_FRAME_FAIL;
                break;
            }
        }

        //delay
        if(pkt.stream_index==video_index){
            if(first_video_packet==false){
                play_pts=pkt.pts;
                first_video_packet=true;
            }
            now_pts=pkt.pts-play_pts;
            AVRational time_base=ifmt_ctx->streams[video_index]->time_base;
            AVRational time_base_q={1,AV_TIME_BASE};
            now_pts = av_rescale_q((pkt.pts-play_pts), time_base, time_base_q);

            //if cache data time lager than CACHE_TIME_SECOND,send cache data
            if((now_pts-last_cache_pts)>=CACHE_TIME_SECOND*AV_TIME_BASE){
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
            now_time = av_gettime() - play_time;
            //printf("%ld %ld\n",now_pts,now_time);
            if (now_pts > now_time)
                av_usleep(now_pts - now_time);
        }

        //Write
        if (av_interleaved_write_frame(ofmt_ctx_mp4, &pkt) < 0) {
            av_log(ofmt_ctx_mp4,AV_LOG_ERROR,"ERROR Fail to write packet %s,error is \n",slice_file,ret);
            return_error = ERROR_WRITE_PKT_FAIL;
            break;
        }
        //printf("Write %8d frames to output file\n",frame_index);
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

/*
send rtsp req to ipc,recv video stream and audio stream and save to format mp4 slice
rtsp_url rtsp url
filename slice name
control_cmd write control end
*/
int rtsp_slice(const char * rtsp_url,const  char* filename, char * control_cmd)
{
    int ret;
    bool first_video=false,first_audio=false;
    unsigned int index,video_index=-1,audio_index=-1;
    AVPacket pkt;
    AVDictionary *opts = NULL;
    AVFormatContext *ifmt_ctx = NULL,*ofmt_ctx_mp4=NULL;
    error_code return_error=NO_ERROR;


    if(rtsp_url==NULL||filename==NULL||control_cmd==NULL)
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
        av_log(ifmt_ctx,AV_LOG_ERROR,"ERROR create  output fail %\n",filename);
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
            av_log( ofmt_ctx_mp4,AV_LOG_ERROR,"Could not open output file '%s'", filename);
            goto end;
        }
    }
    //fmp4
    av_dict_set(&opts, "movflags", "frag_keyframe+empty_moov", 0);
    //Write file header
    if (avformat_write_header(ofmt_ctx_mp4, &opts) < 0) {
        printf( "Error occurred when opening output file\n");
        goto end;
    }
    av_dict_free(&opts);

    //read an write
    printf("control_cmd is %c\n",*(control_cmd+1));
    while (*(control_cmd+1)=='r') {
        printf("control_cmd is %c\n",*(control_cmd+1));
        if ((ret=av_read_frame(ifmt_ctx, &pkt))< 0){
            av_log(ifmt_ctx,AV_LOG_ERROR,"ERROR fail read a frame in %s,error is \n",rtsp_url,ret);
            return_error = ERROR_READ_FRAME_FAIL;
            break;
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

        printf("read a frame indexis %d %ld\n",pkt.stream_index,pkt.dts);

        if ((ret=av_interleaved_write_frame(ofmt_ctx_mp4, &pkt))< 0) {
            av_log(ofmt_ctx_mp4,AV_LOG_ERROR,"ERROR Fail to write packet %s,error is %d\n",rtsp_url,ret);
            return_error = ERROR_WRITE_PKT_FAIL;
            av_packet_unref(&pkt);
            break;
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
    int ret,slice_index=0;
    bool first_video=false,first_audio=false;
    unsigned int index,video_index=-1,audio_index=-1;
    AVPacket pkt;
    AVDictionary *opts = NULL;
    AVFormatContext *ifmt_ctx = NULL,*ofmt_ctx_mp4=NULL;
    error_code return_error=NO_ERROR;
    int64_t slice_start=0,now_time=0;

    if(rtsp_url==NULL||filenames==NULL||control_cmd==NULL||slice_num<=0||slice_second<=0)
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
   // av_dump_format(ifmt_ctx, 0, rtsp_url, 0);

    slice_start=av_gettime();
    slice_start/=10000;
    slice_start%=slice_second*100;
    printf("start is %ld\n",slice_start);
    //open slices
 slice:
        avformat_alloc_output_context2(&ofmt_ctx_mp4, NULL, "mp4", filenames[slice_index]);
        if (!ofmt_ctx_mp4) {
            av_log(ifmt_ctx,AV_LOG_ERROR,"ERROR create  output fail %\n",filenames[slice_index]);
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
                goto end;
            }
        }
        //fmp4
        av_dict_set(&opts, "movflags", "frag_keyframe+empty_moov", 0);
        //Write file header
        if (avformat_write_header(ofmt_ctx_mp4, &opts) < 0) {
            printf( "Error occurred when opening output file\n");
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
                av_log(ifmt_ctx,AV_LOG_ERROR,"ERROR fail read a frame in %s,error is \n",rtsp_url,ret);
                return_error = ERROR_READ_FRAME_FAIL;
                break;
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
                return_error = ERROR_WRITE_PKT_FAIL;
                av_packet_unref(&pkt);
                break;
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
