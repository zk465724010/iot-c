
#include <stdio.h>
#include<string.h>
#include <emscripten.h>
//Linux...
#ifdef __cplusplus
extern "C"
{
#endif
#include <libavformat/avformat.h>
#include <libavformat/avio.h>
#include  <libavcodec/avcodec.h>
#include <libswscale/swscale.h>
#include  <libavutil/avutil.h>
#include  <libavutil/time.h>
#include <libavutil/opt.h>
#include <libavutil/imgutils.h>
#ifdef __cplusplus
};
#endif

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

int fill_iobuffer(void * opaque,uint8_t *buf, int bufsize){
    FILE* fp_open=(FILE*)opaque;
    if(!feof(fp_open)){
        int true_size=fread(buf,1,bufsize,fp_open);
        return true_size;
    }else{
        return -1;
    }
}


int decodeh265(FILE* inputh265,FILE* outyuv,FILE* outrgb)
{
    int ret,index,video_index=-1,audio_index=-1,width,height;
    AVPacket pkt;
    AVFrame *frame=NULL;
    AVFormatContext *ifmt_ctx = NULL;
    AVCodecContext	*pCodecCtx=NULL;
    AVCodec			*pCodec=NULL;
    struct SwsContext *img_convert_ctx=NULL;
    uint8_t *dst_data[4];
    int dst_linesize[4];
    uint8_t *yuv_buff=NULL;


    ifmt_ctx = avformat_alloc_context();
    unsigned char * iobuffer=(unsigned char *)av_malloc(32768);
    AVIOContext *avio =avio_alloc_context(iobuffer, 32768,0,inputh265,fill_iobuffer,NULL,NULL);
    ifmt_ctx->pb=avio;
    ifmt_ctx->flags = AVFMT_FLAG_CUSTOM_IO;
    AVInputFormat* mp4_input=av_find_input_format("mp4");
    ret = avformat_open_input(&ifmt_ctx, NULL, mp4_input, NULL);
    if(ret<0)
    {
        ret =ERROR_OPEN_URL_FAIL;
        goto end;
    }

    if ((ret = avformat_find_stream_info(ifmt_ctx, 0)) < 0) {
        ret=ERROR_STREAM_INFO_FAIL;
        goto end;
    }

     av_dump_format(ifmt_ctx, 0, 0, 0);
    for(index=0;index<ifmt_ctx->nb_streams;index++)
    {
        if(ifmt_ctx->streams[index]->codecpar->codec_type==AVMEDIA_TYPE_VIDEO)
            video_index=index;
        if(ifmt_ctx->streams[index]->codecpar->codec_type==AVMEDIA_TYPE_AUDIO)
            audio_index=index;
    }
    if(video_index==-1)
    {
        ret=ERROR_NO_VIDEO_STREAM;
        goto end;
    }
    pCodec=avcodec_find_decoder(ifmt_ctx->streams[video_index]->codecpar->codec_id);
    if(pCodec==NULL)
    {
        printf("Codec not found.\n");
        ret=ERROR_FIND_CODE_FAIL;
        goto end;
    }
    pCodecCtx = avcodec_alloc_context3(pCodec);

    if(pCodecCtx==NULL)
    {
        printf("code context alloc fail\n");
        ret=ERROR_FIND_CODE_FAIL;
        goto end;
    }


    if(avcodec_parameters_to_context(pCodecCtx, ifmt_ctx->streams[video_index]->codecpar)<0)
    {
        ret=ERROR_CREATE_AVSTREAM_FAIL;
        goto end;
    }
    if(avcodec_open2(pCodecCtx, pCodec,NULL)<0)
    {
        printf("Could not open codec.\n");
        ret=ERROR_OPEN_CODE_FAIL;
        goto end;
    }
     frame=av_frame_alloc();
     height = pCodecCtx->height;
     width = pCodecCtx->width;
     printf("%d x %d /n",width,height);
#if 0

    img_convert_ctx =sws_alloc_context();
    av_opt_set_int(img_convert_ctx,"sws_flags",SWS_FAST_BILINEAR|SWS_PRINT_INFO,0);
    av_opt_set_int(img_convert_ctx,"srcw",pCodecCtx->width,0);
    av_opt_set_int(img_convert_ctx,"srch",pCodecCtx->height,0);
    av_opt_set_int(img_convert_ctx,"src_format",AV_PIX_FMT_YUV420P,0);
    av_opt_set_int(img_convert_ctx,"src_range",1,0);

    av_opt_set_int(img_convert_ctx,"dstw",pCodecCtx->width,0);
    av_opt_set_int(img_convert_ctx,"dsth",pCodecCtx->height,0);
    av_opt_set_int(img_convert_ctx,"dst_format",AV_PIX_FMT_RGB24,0);
    av_opt_set_int(img_convert_ctx,"dst_range",1,0);

    sws_init_context(img_convert_ctx,NULL,NULL);
#endif
    img_convert_ctx = sws_getContext(width, height,AV_PIX_FMT_YUV420P, width,height, AV_PIX_FMT_RGB24,SWS_FAST_BILINEAR, NULL,NULL, NULL);
    if(img_convert_ctx==NULL)
        printf("init sws fail\n");

    ret = av_image_alloc(dst_data, dst_linesize,width, height, AV_PIX_FMT_RGB24, 1);
    if (ret< 0) {
        printf( "Could not allocate destination image\n");
        goto  end;
    }

    yuv_buff=(uint8_t*)av_malloc(pCodecCtx->width*pCodecCtx->height*3/2);
    memset(yuv_buff,0,sizeof(yuv_buff));
    while (1) {
        if ((ret=av_read_frame(ifmt_ctx, &pkt))< 0){
            ret =ERROR_READ_FRAME_FAIL;
            goto  end;
        }

        if(pkt.stream_index!=video_index)
        {
            av_packet_unref(&pkt);
            continue;
        }

        ret = avcodec_send_packet(pCodecCtx, &pkt);

        if(ret<0)
        {
            av_packet_unref(&pkt);
            printf("send pack fail\n");
            goto end;
        }
        av_packet_unref(&pkt);

        while(ret>=0)
        {
            ret=avcodec_receive_frame(pCodecCtx,frame);
            //0 sucess, EAGAIN need input,AVERROR_EOF no frame
            if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
                break;
            } else if (ret < 0) {
                printf("decode  pack fail\n");
                goto end;
            }


            int a = 0, i;
            for (i = 0; i<height; i++)
            {
                memcpy(yuv_buff + a, frame->data[0] + i * frame->linesize[0], width);
                a += width;
            }
            for (i = 0; i<height / 2; i++)
            {
                memcpy(yuv_buff + a, frame->data[1] + i * frame->linesize[1], width / 2);
                a += width / 2;
            }
            for (i = 0; i<height / 2; i++)
            {
                memcpy(yuv_buff + a, frame->data[2] + i * frame->linesize[2], width / 2);
                a += width / 2;
            }
            fwrite(yuv_buff, 1, height *width* 3 / 2,outyuv);
            sws_scale(img_convert_ctx,(const uint8_t **) frame->data, frame->linesize, 0, height, dst_data, dst_linesize);


            fwrite(dst_data[0],1,width* height*3,outrgb);
        }
    }
end:
    avformat_close_input(&ifmt_ctx);
    avcodec_free_context(&pCodecCtx);
    sws_freeContext(img_convert_ctx);
    av_freep(&dst_data[0]);
    av_free(yuv_buff);
    return ret;
}

EMSCRIPTEN_KEEPALIVE  int decodeh265_real(char* rtsp_url,FILE* outrgb)
{
    int ret,index,video_index=-1,audio_index=-1,width,height;
    AVPacket pkt;
    AVFrame *frame=NULL;
    AVFormatContext *ifmt_ctx = NULL;
    AVCodecContext	*pCodecCtx=NULL;
    AVCodec			*pCodec=NULL;
    struct SwsContext *img_convert_ctx=NULL;
    uint8_t *dst_data[4];
    int dst_linesize[4];


    ret = avformat_open_input(&ifmt_ctx, rtsp_url, NULL, NULL);
    if(ret<0)
    {
        ret =ERROR_OPEN_URL_FAIL;
        goto end;
    }

    if ((ret = avformat_find_stream_info(ifmt_ctx, 0)) < 0) {
        ret=ERROR_STREAM_INFO_FAIL;
        goto end;
    }

     av_dump_format(ifmt_ctx, 0, 0, 0);
    for(index=0;index<ifmt_ctx->nb_streams;index++)
    {
        if(ifmt_ctx->streams[index]->codecpar->codec_type==AVMEDIA_TYPE_VIDEO)
            video_index=index;
        if(ifmt_ctx->streams[index]->codecpar->codec_type==AVMEDIA_TYPE_AUDIO)
            audio_index=index;
    }
    if(video_index==-1)
    {
        ret=ERROR_NO_VIDEO_STREAM;
        goto end;
    }
    pCodec=avcodec_find_decoder(ifmt_ctx->streams[video_index]->codecpar->codec_id);
    if(pCodec==NULL)
    {
        printf("Codec not found.\n");
        ret=ERROR_FIND_CODE_FAIL;
        goto end;
    }
    pCodecCtx = avcodec_alloc_context3(pCodec);

    if(pCodecCtx==NULL)
    {
        printf("code context alloc fail\n");
        ret=ERROR_FIND_CODE_FAIL;
        goto end;
    }


    if(avcodec_parameters_to_context(pCodecCtx, ifmt_ctx->streams[video_index]->codecpar)<0)
    {
        ret=ERROR_CREATE_AVSTREAM_FAIL;
        goto end;
    }
    if(avcodec_open2(pCodecCtx, pCodec,NULL)<0)
    {
        printf("Could not open codec.\n");
        ret=ERROR_OPEN_CODE_FAIL;
        goto end;
    }
     frame=av_frame_alloc();
     height = pCodecCtx->height;
     width = pCodecCtx->width;

    img_convert_ctx = sws_getContext(width, height,AV_PIX_FMT_YUV420P, width,height, AV_PIX_FMT_RGB24,SWS_FAST_BILINEAR, NULL,NULL, NULL);
    if(img_convert_ctx==NULL)
        printf("init sws fail\n");

    ret = av_image_alloc(dst_data, dst_linesize,width, height, AV_PIX_FMT_RGB24, 1);
    if (ret< 0) {
        printf( "Could not allocate destination image\n");
        goto  end;
    }

    while (1) {
        if ((ret=av_read_frame(ifmt_ctx, &pkt))< 0){
            ret =ERROR_READ_FRAME_FAIL;
            goto  end;
        }

        if(pkt.stream_index!=video_index)
        {
            av_packet_unref(&pkt);
            continue;
        }

        ret = avcodec_send_packet(pCodecCtx, &pkt);

        if(ret<0)
        {
            av_packet_unref(&pkt);
            printf("send pack fail\n");
            goto end;
        }
        av_packet_unref(&pkt);

        while(ret>=0)
        {
            ret=avcodec_receive_frame(pCodecCtx,frame);
            //0 sucess, EAGAIN need input,AVERROR_EOF no frame
            if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
                break;
            } else if (ret < 0) {
                printf("decode  pack fail\n");
                goto end;
            }
            sws_scale(img_convert_ctx,(const uint8_t **) frame->data, frame->linesize, 0, height, dst_data, dst_linesize);
            fwrite(dst_data[0],1,width* height*3,outrgb);
        }
    }
end:
    avformat_close_input(&ifmt_ctx);
    avcodec_free_context(&pCodecCtx);
    sws_freeContext(img_convert_ctx);
    av_freep(&dst_data[0]);
    return ret;
}
