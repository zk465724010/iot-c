#include <stdio.h>
#include<string.h>
#include <emscripten.h>

#include <libavformat/avformat.h>
#include <libavformat/avio.h>
#include  <libavcodec/avcodec.h>
#include  <libavutil/avutil.h>
#include  <libavutil/time.h>
#include <libavutil/opt.h>
#include <libavutil/fifo.h>


const int CustomIoBufferSize = 32 * 1024;
const int DefaultFifoSize = 4 * 1024 * 1024;
const int MaxFifoSize = 16 * 1024 * 1024;

#define MIN(X, Y)  ((X) < (Y) ? (X) : (Y))

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
    ERROR_OPEN_CODE_FAIL ,//打开解码失败
    ERROR_SEND_PACKET_FAIL,//发送解码失败
    ERROR_DECODE_FAIL//解码失败
};

typedef void(*VideoCallback)(unsigned char *buff, int size, double timestamp);

typedef struct _player
{
AVFormatContext *ifmt_ctx;
AVCodecContext	*pCodecCtx;
VideoCallback videoFUN;
AVFifoBuffer *fifo;
int fifoSize;
unsigned char *customIoBuffer;
AVIOContext* ioContext; 
int	audio_index;
int video_index;
int width;
int height;
uint8_t *yuv_buff;
AVFrame *avFrame;	
}player;
	

player curr_player;


EMSCRIPTEN_KEEPALIVE void init(long videoCallback_js)
{
	memset(&curr_player,0,sizeof(curr_player));
	curr_player.fifo=av_fifo_alloc(DefaultFifoSize);
	curr_player.fifoSize=DefaultFifoSize;
	curr_player.videoFUN = (VideoCallback)videoCallback_js;
	curr_player.video_index=-1;
	curr_player.audio_index=-1;
}

EMSCRIPTEN_KEEPALIVE void uninit()
{
	 if (curr_player.fifo != NULL) {
             av_fifo_freep(&(curr_player.fifo));
        }
	 
	avformat_close_input(&(curr_player.ifmt_ctx));
	avcodec_free_context(&(curr_player.pCodecCtx));
	if(curr_player.yuv_buff!=NULL)
		av_free(curr_player.yuv_buff);
	avio_context_free(&(curr_player.ioContext));
	if(curr_player.customIoBuffer!=NULL)
		av_free(curr_player.customIoBuffer);
	 
    if (curr_player.avFrame != NULL) {
            av_freep(&(curr_player.avFrame));
        }
}

EMSCRIPTEN_KEEPALIVE int writeToFifo(unsigned char *buff, int size)
{
	int ret = 0;
	if(curr_player.fifo==NULL)
		return -1;
	int64_t leftSpace = av_fifo_space(curr_player.fifo);
	//printf("leftspace is %ld\n",leftSpace);
	 if (leftSpace < size) {
            int growSize = 0;
            do {	
                leftSpace += curr_player.fifoSize;
                growSize += curr_player.fifoSize;
                curr_player.fifoSize += curr_player.fifoSize;
				printf("add fifo,leftspace is %lld,data size is %d growsize is %d\n",leftSpace,size,growSize);
            } while (leftSpace < size);
            av_fifo_grow(curr_player.fifo, growSize);

            printf("Fifo size growed to %d\n", curr_player.fifoSize);
            if (curr_player.fifoSize >= MaxFifoSize) {
                printf(" Fifo size larger than %d\n", MaxFifoSize);
            }
	 }
	// printf("write %d to fifo\n",size);
	 ret = av_fifo_generic_write(curr_player.fifo, buff, size, NULL);
	 return ret;
}


int readfifo(void *opaque, uint8_t *data, int len) 
{
	if(data==NULL||len<=0)
		return -1;
	//printf("read from fifo\n");
	int availableBytes = av_fifo_size(curr_player.fifo);
	//printf("availableBytes is %d\n",availableBytes);
	if (availableBytes <= 0) {
		   printf("no data in fifo\n");
           return AVERROR_EOF;
        }
	int canReadLen = MIN(availableBytes, len);
    av_fifo_generic_read(curr_player.fifo, data, canReadLen, NULL);
	//printf("read info %d \n",canReadLen);
    return canReadLen;
}

EMSCRIPTEN_KEEPALIVE  int open_stream()
{
    int ret,index;  
	AVCodec			*pCodec=NULL;
	
    curr_player.ifmt_ctx=avformat_alloc_context();
	printf("start\n");
	curr_player.customIoBuffer=(unsigned char*)av_mallocz(CustomIoBufferSize);
	if(curr_player.customIoBuffer==NULL)
	{
	 	ret =ERROR_MEM_FAIL;
		printf("alloc custom buffer fail\n");
        return ret;
	}
	curr_player.ioContext = avio_alloc_context(curr_player.customIoBuffer,CustomIoBufferSize,0,NULL,readfifo,NULL,NULL);
	if(curr_player.ioContext==NULL)
	{
		ret =ERROR_IOCONTENT_FAIL;
		printf("alloc io content fail\n");
        return ret;
	}
	curr_player.ifmt_ctx->pb =curr_player.ioContext;
    curr_player.ifmt_ctx->flags = AVFMT_FLAG_CUSTOM_IO;
	
    ret = avformat_open_input(&(curr_player.ifmt_ctx), NULL, NULL, NULL);
	printf("open input\n");
    
    if(ret<0)
    {
        ret =ERROR_OPEN_URL_FAIL;
		printf("open avformat fail,ret is %d\n",ret);
        return ret;
    }

    if ((ret = avformat_find_stream_info(curr_player.ifmt_ctx, 0)) < 0) {
        ret=ERROR_STREAM_INFO_FAIL;
		printf("find stream fail,ret is %d\n",ret);
        return ret;
    }
	
    av_dump_format(curr_player.ifmt_ctx, 0, 0, 0);
    for(index=0;index<curr_player.ifmt_ctx->nb_streams;index++)
    {
        if(curr_player.ifmt_ctx->streams[index]->codecpar->codec_type==AVMEDIA_TYPE_VIDEO)
            curr_player.video_index=index;
        if(curr_player.ifmt_ctx->streams[index]->codecpar->codec_type==AVMEDIA_TYPE_AUDIO)
            curr_player.audio_index=index;
    }
	printf("find video\n");
    if(curr_player.video_index==-1)
    {
        ret=ERROR_NO_VIDEO_STREAM;
		printf("not video stream\n");
        return ret;
    }
    pCodec=avcodec_find_decoder(curr_player.ifmt_ctx->streams[curr_player.video_index]->codecpar->codec_id);
    if(pCodec==NULL)
    {
        printf("find decode fail\n");
        ret=ERROR_FIND_CODE_FAIL;
        return ret;
    }
    curr_player.pCodecCtx = avcodec_alloc_context3(pCodec);

    if(curr_player.pCodecCtx==NULL)
    {
        printf("alloc decode content fail\n");
        ret=ERROR_FIND_CODE_FAIL;
        return ret;
    }


    if(avcodec_parameters_to_context(curr_player.pCodecCtx, curr_player.ifmt_ctx->streams[curr_player.video_index]->codecpar)<0)
    {
        ret=ERROR_CREATE_AVSTREAM_FAIL;
		printf("copy decode content fail\n");
        return ret;
    }
	printf("open decode\n");
    if(avcodec_open2(curr_player.pCodecCtx, pCodec,NULL)<0)
    {
        printf("open decode fail\n");
        ret=ERROR_OPEN_CODE_FAIL;
        return ret;
    }

	curr_player.height=curr_player.pCodecCtx->height;
	curr_player.width=curr_player.pCodecCtx->width;
	curr_player.yuv_buff=(uint8_t*)av_malloc(curr_player.width*curr_player.height*3/2);
    memset(curr_player.yuv_buff,0,curr_player.width*curr_player.height*3/2);

	curr_player.avFrame=av_frame_alloc();
	return NO_ERROR;
}

EMSCRIPTEN_KEEPALIVE int decodeOnePacket()
{
	int ret;
	double timestamp = 0.0f;
	int yuv_index=-1,line_index=-1;
    AVPacket pkt;
	
    av_init_packet(&pkt);
	pkt.data = NULL;
    pkt.size = 0;
	printf("read one video packet\n");

    int availableBytes = av_fifo_size(curr_player.fifo);
	printf("availableBytes in fifo is %d\n",availableBytes);
	if(availableBytes<=0)
	{
		printf("no data,stop read packet\n");
		return NO_ERROR;
	}
	while(1)
	{
    if ((ret=av_read_frame(curr_player.ifmt_ctx, &pkt))< 0){
			if(ret==AVERROR_EOF)
			{
				printf("no data\n");
				return NO_ERROR;
			}
           
			printf("read packet fail,ret is %d\n",ret);
			ret =ERROR_READ_FRAME_FAIL;
            return ret;
        }
	
    if(pkt.stream_index!=curr_player.video_index)
        {
            av_packet_unref(&pkt);
       		continue;
        }
	else
		break;
	}

	
   		 ret = avcodec_send_packet(curr_player.pCodecCtx, &pkt);

        if(ret<0)
        {
            av_packet_unref(&pkt);
            printf("send packet fail,ret is %d\n",ret);
			printf("clear fifo \n");
			curr_player.ioContext->buf_ptr=curr_player.ioContext->buffer;
			curr_player.ioContext->buf_end=curr_player.ioContext->buf_ptr;
			av_fifo_reset(curr_player.fifo);
            return ERROR_SEND_PACKET_FAIL;
        }
        //av_packet_unref(&pkt);

        while(ret>=0)
        {
           // printf("start decode\n");
            ret=avcodec_receive_frame(curr_player.pCodecCtx,curr_player.avFrame);
            //0 sucess, EAGAIN need input,AVERROR_EOF no frame
            if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
                break;
            } else if (ret < 0) {
                printf("decode packet fail,ret is %d\n",ret);
                return ERROR_DECODE_FAIL;
            }

			//printf("write yuv\n");

            yuv_index=0;
			int height=curr_player.height;
			int width=curr_player.width;
            for (line_index = 0; line_index<height; line_index++)
            {
                memcpy(curr_player.yuv_buff + yuv_index, curr_player.avFrame->data[0] +  line_index* curr_player.avFrame->linesize[0], width);
                yuv_index += width;
            }
            for (line_index = 0; line_index<height / 2; line_index++)
            {
                memcpy(curr_player.yuv_buff + yuv_index, curr_player.avFrame->data[1] + line_index * curr_player.avFrame->linesize[1], width / 2);
                yuv_index += width / 2;
            }
            for (line_index = 0; line_index<height / 2; line_index++)
            {
                memcpy(curr_player.yuv_buff + yuv_index, curr_player.avFrame->data[2] + line_index * curr_player.avFrame->linesize[2], width / 2);
                yuv_index += width / 2;
            }
            
			timestamp = (double)(curr_player.avFrame->pts) * av_q2d(curr_player.ifmt_ctx->streams[curr_player.video_index]->time_base);
			//printf("call funtion\n");
			curr_player.videoFUN(curr_player.yuv_buff,width*height*3/2,timestamp);
			av_frame_unref(curr_player.avFrame);
        }
		
		av_packet_unref(&pkt);
		return NO_ERROR;
}



