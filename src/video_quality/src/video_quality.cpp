#include <stdio.h>
#include<string.h>
#ifdef _WIN32
//Windows
extern "C"
{
#include "libavformat/avformat.h"
#include  "libavformat/avio.h"
#include  "libavcodec/avcodec.h"
#include  "libavutil/avutil.h"
#include "libswscale/swscale.h"
#include  "libavutil/time.h"
#include  "libavutil/log.h"
#include "libavutil/opt.h"
#include "libavutil/imgutils.h"
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
#include <libswscale/swscale.h>
#include  <libavutil/avutil.h>
#include  <libavutil/time.h>
#include  <libavutil/log.h>
#include <libavutil/opt.h>
#include <libavutil/imgutils.h>
#ifdef __cplusplus
};
#endif
#endif

#include <opencv2/opencv.hpp>
#include <opencv2/core.hpp>
#include <opencv2/imgcodecs.hpp>
#include <opencv2/highgui.hpp>
#include <opencv2/imgproc.hpp>
#include <iostream>
#include "video_quality.h"

using namespace cv;


//draw hist
void hist(Mat& img)
{
    Mat hsv;
    cvtColor(img,hsv,COLOR_BGR2HSV);
    float h_ranges[]={0,100};
    const float* ranges[]={h_ranges};
    int histSize[]={100},ch[]={2};
    Mat hist;
    calcHist(&hsv,1,ch,noArray(),hist,1,histSize,ranges,true);
    normalize(hist,hist,0,1000 ,NORM_MINMAX);
    Mat hist_img(1000,1000,CV_8UC3);
    for(int h=0;h<histSize[0];h++)
    {
        int hval=hist.at<float>(h);
        std::cout<<hval<<std::endl;
        rectangle(hist_img,Rect(h*10,1000-hval,10,hval),Scalar(255,255,255),-1);
    }
    imshow("hist",hist_img);
    waitKey(0);

}

//compute Chroma contrast brightness value
void hsv_compute(Mat& img)
{
#if 0
    float range[]={0,255};
    const float* ranges[]={range};
    int histSize[]={20},v_ch[]={2};
    Mat v_hist;
    calcHist(&img,1,v_ch,noArray(),v_hist,1,histSize,ranges,true);
    normalize(v_hist,v_hist,1,0 ,NORM_L1);
    printf("v start:\n");
    for(int h=0;h<histSize[0];h++)
    {
        float hval=v_hist.at<float>(h)*100;
        printf("%2.2f ",hval);
    }
    printf("\ns start:\n");

    Mat s_hist;
    float s_range[]={0,100};
    const float* s_ranges[]={s_range};
    int s_ch[]={1};
    calcHist(&img,1,s_ch,noArray(),s_hist,1,histSize,s_ranges,true);
    normalize(s_hist,s_hist,1,0 ,NORM_L1);

    for(int h=0;h<histSize[0];h++)
    {
        float hval=s_hist.at<float>(h)*100;
        printf("%2.2f ",hval);
    }
    printf("\n");
#endif
    Scalar mean;
    Scalar std;
    meanStdDev(img,mean,std);
    printf("v s mean v std  %2.2f %2.2f  %2.2f \n",mean[2],mean[1],std[2]);

}

// check shelter area
double check(Mat gray)
{
    Mat edge;
    // drop far away data
    threshold(gray,edge,80,255,0);
    // close compute ,drop noise
    Mat element = getStructuringElement(MORPH_RECT, Size(15, 15));
    morphologyEx(edge, edge, MORPH_CLOSE, element);

    //black or white
    threshold(edge, edge, 20, 255, THRESH_BINARY);

    //not compute
    bitwise_not(edge,edge);


    //get outline
    std::vector<std::vector<Point> > contours;
    std::vector<Vec4i>  hierarchy;
    findContours(edge,contours,hierarchy,RETR_LIST,CHAIN_APPROX_SIMPLE,Point(0,0));
    // imshow("edge",edge);
    //waitKey();

    //get max outline area
    double max=0;
    for (int i = 0; i < contours.size(); i++)
    {
        double area=contourArea(contours[i]);
        // printf(" %4f \n",area);
        if(area>max)
            max=area;
    }
    return max;
}

//compute mohu value
double mohu(Mat& gray)
{
    Mat  iSobel;
    //qiu dao
    Sobel(gray, iSobel, CV_16U, 1, 1);
    double iimeanValue = 0.0;
    iimeanValue = mean(iSobel)[0];
    return iimeanValue;

}

// add num noise point int a picture
void addSaltNoise(Mat &m, int num)
{
    // 随机数产生器
    std::random_device rd; //种子
    std::mt19937 gen(rd()); // 随机数引擎

    auto cols = m.cols * m.channels();

    for (int i = 0; i < num; i++)
    {
        auto row = static_cast<int>(gen() % m.rows);
        auto col = static_cast<int>(gen() % cols);

        auto p = m.ptr<uchar>(row);
        p[col++] = 255;
        p[col++] = 255;
        p[col] = 255;
    }
}

//get psnr value
double getPSNR(const Mat& I1, const Mat& I2)
{
    Mat s1;
    absdiff(I1, I2, s1);       // |I1 - I2|
    s1.convertTo(s1, CV_32F);  // cannot make a square on 8 bits
    s1 = s1.mul(s1);           // |I1 - I2|^2
    Scalar s = sum(s1);         // sum elements per channel
    double sse = s.val[0] + s.val[1] + s.val[2]; // sum channels
    if( sse <= 1e-10) // for small values return zero
        return 0;
    else
    {
        double  mse =sse /(double)(I1.channels() * I1.total());
        double psnr = 10.0*log10((255*255)/mse);
        return psnr;
    }
}

//get 4 area and comp
double forgroudComp(Mat& curr,Mat& last)
{
    Mat mask;
    int value=300;
    int heigth=curr.rows;
    int width=curr.cols;
    //get 4 rect
    Rect r1(0, 0, value, value);
    Rect r2(width-value, 0, value, value);
    Rect r3(0,heigth-value,value,value);
    Rect r4(width-value,heigth-value,value,value);
    mask = Mat::zeros(curr.size(), CV_8UC1);
    // only comp 4 area
    mask(r1).setTo(255);
    mask(r2).setTo(255);
    mask(r3).setTo(255);
    mask(r4).setTo(255);
    Mat curr_mask,last_mask;
    curr.copyTo(curr_mask, mask);
    last.copyTo(last_mask, mask);
    //imshow("same",curr_mask);
    //waitKey();
    //get two value
    return getPSNR(curr_mask,last_mask);

}

//get zaosheng value
double zaosheng(Mat& curr)
{
    Mat median,noise;
    //median blur
    medianBlur(curr,median,3);
    return getPSNR(curr,median);
} 

/*
 * recv stream from rtsp or file get quality value
 *url rtsp url or file path
*/
int video_quality(char* url)
{
    Mat last;
    int interval=0;
    int ret,index,video_index=-1,audio_index=-1,width=0,height=0;
    AVPacket pkt;
    AVFrame *frame=NULL;
    AVFormatContext *ifmt_ctx = NULL;
    AVCodecContext	*pCodecCtx=NULL;
    AVCodec			*pCodec=NULL;

    if(url==NULL)
        return ERROR_ARG;

    av_log_set_level(AV_LOG_ERROR);
    //open url or file
    ret = avformat_open_input(&ifmt_ctx, url, NULL, NULL);
    if(ret<0)
    {
        ret =ERROR_OPEN_URL_FAIL;
        goto end;
    }

    //find stream information
    if ((ret = avformat_find_stream_info(ifmt_ctx, 0)) < 0) {
        ret=ERROR_STREAM_INFO_FAIL;
        goto end;
    }

    //av_dump_format(ifmt_ctx, 0, 0, 0);
    //get video and audio index
    for(index=0;index<ifmt_ctx->nb_streams;index++)
    {
        if(ifmt_ctx->streams[index]->codecpar->codec_type==AVMEDIA_TYPE_VIDEO)
            video_index=index;
        if(ifmt_ctx->streams[index]->codecpar->codec_type==AVMEDIA_TYPE_AUDIO)
            audio_index=index;
    }
    // not video ,quit
    if(video_index==-1)
    {
        ret=ERROR_NO_VIDEO_STREAM;
        goto end;
    }

    // find video code
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


    //get code information
    if(avcodec_parameters_to_context(pCodecCtx, ifmt_ctx->streams[video_index]->codecpar)<0)
    {
        ret=ERROR_CREATE_AVSTREAM_FAIL;
        goto end;
    }
    //open coder
    if(avcodec_open2(pCodecCtx, pCodec,NULL)<0)
    {
        printf("Could not open codec.\n");
        ret=ERROR_OPEN_CODE_FAIL;
        goto end;
    }

    frame=av_frame_alloc();

    //video height and width
    height = pCodecCtx->height;
    width = pCodecCtx->width;


    while (1)
    {
        //read a packet
        if ((ret=av_read_frame(ifmt_ctx, &pkt))< 0){
            ret =ERROR_READ_FRAME_FAIL;
            goto  end;
        }

        //only deal video stream
        if(pkt.stream_index!=video_index)
        {
            av_packet_unref(&pkt);
            continue;
        }

        //send packet to decode
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
            //get decoded frame
            ret=avcodec_receive_frame(pCodecCtx,frame);
            //0 sucess, EAGAIN need input,AVERROR_EOF no frame
            if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
                break;
            } else if (ret < 0) {
                printf("decode  pack fail\n");
                goto end;
            }
            interval++;
            //video quality every 25 frame
            if(interval%25==0)
            {
                //copy frame data to opencv mat
                Mat tmp_img = cv::Mat::zeros( height*3/2, width, CV_8UC1 );
                memcpy( tmp_img.data, frame->data[0], width*height );
                memcpy( tmp_img.data + width*height, frame->data[1], width*height/4 );
                memcpy( tmp_img.data + width*height*5/4, frame->data[2], width*height/4 );

                Mat bgr;
                Mat hsv;
                Mat gray;
                Mat same;
                //bgr and hsv and gray image
                cvtColor( tmp_img, bgr,COLOR_YUV2BGR_I420);
                cvtColor(bgr,hsv,COLOR_BGR2HSV);
                cvtColor(bgr,gray,COLOR_BGR2GRAY);

                //get Chroma contrast brightness compute
                hsv_compute(hsv);
                // Scalar mean1=mean(hsv);
                // std::cout<<mean1<<std::endl;
                // waitKey(1)
                if(!last.empty())
                {
                    //Mat same=bgr!=last;
                    //imshow("gray",gray);
                    //imshow("last",last);
                    // last=gray.clone();
                    subtract(gray,last,same);
                    //imshow("same",same);
                    // waitKey;
                    int count=countNonZero(same);
                    // imshow("same",same);
                    // waitKey();
                    printf("same comp:%d\n",count);
                    //printf("same psnr %f\n",getPSNR(gray,last));
                    double comvalue=forgroudComp(gray,last);
                    printf("forground com: %f\n",comvalue);
                }
                //gray.copyTo(last);
                last=gray.clone();
                double max;
                max=check(gray);
                double per=max*100/(height*width);
                printf("zhdang %f %f\n",max,per);
                // printf("%s\n",CV_VERSION);
                //foregroundComp(gray);
                double imohu=mohu(gray);
                printf("mohu %f\n",imohu);
                double psnr=zaosheng(gray);
                printf("nomal psnr:  %f",psnr);
                addSaltNoise(gray,10000);
                //imshow("same",gray);
                //waitKey();
                psnr=zaosheng(gray);
                printf("noise psnr :%f\n",psnr);
                printf("================================================================\n\n");
            }
        }
    }
end:
    avformat_close_input(&ifmt_ctx);
    avcodec_free_context(&pCodecCtx);
    return ret;
}

/*create same video mp4 file to test
 * rstp_url rtsp url
 * out_file same video file
*/
int create_same_video(const char* rtsp_url,const char* out_file)
{
    AVFormatContext *ifmt_ctx = NULL,*ofmt_ctx_mp4=NULL;
    int return_error,ret,index,video_index=-1,getframe_return=0,getpacket_return=0;
    AVCodec* inCode_Video=NULL,*outCode_Video=NULL;
    AVCodecContext* inCode_VideoCTX=NULL,*outCode_VideoCTX=NULL;
    int frist_out_packe=-1;
    AVStream* out_video_stream=NULL;
    AVDictionary *opts = NULL;
    int64_t play_time=0;
    AVPacket in_pkt;
    AVPacket out_pkt;
    AVFrame *pFrame = av_frame_alloc();
    AVFrame *startFrame=NULL;


    av_init_packet(&in_pkt);
    av_init_packet(&out_pkt);


    if ((ret = avformat_open_input(&ifmt_ctx,rtsp_url,0, 0)) < 0) {
        av_log(ifmt_ctx,AV_LOG_ERROR,"ERROR Fail to open slice file %s\n",rtsp_url);
        return_error = ERROR_OPEN_FILE_FAIL;
        goto end;
    }
    //find stream information in input
    if ((ret = avformat_find_stream_info(ifmt_ctx, 0)) < 0) {
        av_log(ifmt_ctx,AV_LOG_ERROR,"ERROR Fail to get slice information %s\n",rtsp_url);
        return_error = ERROR_STREAM_INFO_FAIL;
        goto end;
    }
    for(index=0; index<ifmt_ctx->nb_streams; index++) {
        if(ifmt_ctx->streams[index]->codecpar->codec_type==AVMEDIA_TYPE_VIDEO){
            video_index=index;
        }
    }
    av_dump_format(ifmt_ctx, 0,0, 0);
    if(video_index<0){
        av_log(ifmt_ctx,AV_LOG_ERROR,"ERROR no video stream in %s\n",rtsp_url);
        return_error = ERROR_NO_VIDEO_STREAM;
        goto end;
    }

    inCode_Video=avcodec_find_decoder(ifmt_ctx->streams[video_index]->codecpar->codec_id);
    if(inCode_Video==NULL)
    {
        av_log(ifmt_ctx,AV_LOG_ERROR,"ERROR in video stream code not see\n",rtsp_url);
        return_error = ERROR_FIND_CODE_FAIL;
        goto end;
    }
    inCode_VideoCTX=avcodec_alloc_context3(inCode_Video);
    ret=avcodec_parameters_to_context(inCode_VideoCTX,ifmt_ctx->streams[video_index]->codecpar);
    printf("input bitrate is %lld,timebase is %d %d\n",inCode_VideoCTX->bit_rate,ifmt_ctx->streams[video_index]->time_base.den,ifmt_ctx->streams[video_index]->time_base.num);
    if(ret<0)
    {
        av_log(ifmt_ctx,AV_LOG_ERROR,"ERROR copy in code parameters to codecontext fail\n",rtsp_url);
        return_error = ERROR_CREATE_CODECOTENT_FAIL;
        goto end;
    }
    if(avcodec_open2(inCode_VideoCTX, inCode_Video, NULL) < 0)
    {
        av_log(ifmt_ctx,AV_LOG_ERROR,"open in code fail\n",rtsp_url);
        return_error = ERROR_OPEN_CODE_FAIL;
        goto end;
    }

    //Output
    avformat_alloc_output_context2(&ofmt_ctx_mp4, NULL, "mp4", out_file);
    if (!ofmt_ctx_mp4) {
        av_log(ifmt_ctx,AV_LOG_ERROR,"ERROR create output fail\n");
        return_error = ERROR_OPEN_OUTSTREAM_FAIL;
        goto end;
    }


    outCode_Video=avcodec_find_encoder(AV_CODEC_ID_H264);
    if(outCode_Video==NULL){
        av_log(ifmt_ctx,AV_LOG_ERROR,"ERROR out video stream code not see\n",rtsp_url);
        return_error = ERROR_FIND_CODE_FAIL;
        goto end;
    }

    outCode_VideoCTX=avcodec_alloc_context3(outCode_Video);

    outCode_VideoCTX->height=inCode_VideoCTX->height;
    outCode_VideoCTX->width=inCode_VideoCTX->width;
    outCode_VideoCTX->pix_fmt=AV_PIX_FMT_YUV420P;
    // outCode_VideoCTX->time_base=inCode_VideoCTX->time_base;
    outCode_VideoCTX->time_base=(AVRational){1,25};
    outCode_VideoCTX->framerate=(AVRational){25,1};
    outCode_VideoCTX->codec_id=outCode_Video->id;
    outCode_VideoCTX->codec_type=AVMEDIA_TYPE_VIDEO;
    outCode_VideoCTX->gop_size=50;//i frame interval
    outCode_VideoCTX->keyint_min=50;
    outCode_VideoCTX->bit_rate=inCode_VideoCTX->bit_rate;

    if(avcodec_open2(outCode_VideoCTX, outCode_Video,NULL) < 0)
    {
        av_log(ifmt_ctx,AV_LOG_ERROR,"open out code fail\n",rtsp_url);
        return_error = ERROR_OPEN_CODE_FAIL;
        goto end;
    }

    out_video_stream=avformat_new_stream(ofmt_ctx_mp4,outCode_Video);
    if (!out_video_stream) {
        av_log(ifmt_ctx,AV_LOG_ERROR,"ERROR Fail to create out stream in %s\n",rtsp_url);
        ret = ERROR_CREATE_AVSTREAM_FAIL;
        goto end;
    }
    ret=avcodec_parameters_from_context(out_video_stream->codecpar,outCode_VideoCTX);

    if(ret<0)
    {
        av_log(ifmt_ctx,AV_LOG_ERROR,"ERROR copy out code parameters from codecontext to stream fail\n",rtsp_url);
        return_error = ERROR_CREATE_CODECOTENT_FAIL;
        goto end;
    }
    if (ofmt_ctx_mp4->oformat->flags & AVFMT_GLOBALHEADER) {
        ofmt_ctx_mp4->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;
    }

    av_dump_format(ofmt_ctx_mp4, 0, 0, 1);

    //Open output file
    if (!(ofmt_ctx_mp4->flags & AVFMT_NOFILE)) {
        if (avio_open(&ofmt_ctx_mp4->pb, out_file, AVIO_FLAG_WRITE) < 0) {
            av_log( ofmt_ctx_mp4,AV_LOG_ERROR,"Could not open output file '%s'", out_file);
            ret = ERROR_OPEN_OUTSTREAM_FAIL;
            goto end;
        }
    }
    av_dict_set(&opts, "movflags", "frag_keyframe+empty_moov", 0);

    if (avformat_write_header(ofmt_ctx_mp4, &opts) < 0) {
        printf( "Error occurred when write head to  output file\n");
        ret = ERROR_OPEN_OUTSTREAM_FAIL;
        goto end;
    }
    av_dict_free(&opts);

    //read an write
    play_time=av_gettime();
    while(1)
    {
        ret = av_read_frame(ifmt_ctx, &in_pkt);
        if(ret<0)
        {
            printf("end\n");
            break;
        }
        if(in_pkt.stream_index!=video_index)
        {
            av_packet_unref(&in_pkt);
            continue;
        }
        // printf("get an inpacket\n");
        ret=avcodec_send_packet(inCode_VideoCTX,&in_pkt);
        if(ret<0)
        {
            av_packet_unref(&in_pkt);
            printf("send packet fail\n");
            break;
        }
        getframe_return=0;
        while(getframe_return>=0)
        {
            getframe_return=avcodec_receive_frame(inCode_VideoCTX,pFrame);
            if (getframe_return == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
                break;
            } else if (getframe_return < 0) {
                printf("decode  pack fail\n");
                goto end;
            }
            else
            {
                if(startFrame==NULL)
                {
                    startFrame=av_frame_clone(pFrame);
                }
#if 0
                else
                {
                    memcpy( pFrame->data[0], startFrame->data[0], inCode_VideoCTX->width*inCode_VideoCTX->height);
                    memcpy( pFrame->data[1] , startFrame->data[1], inCode_VideoCTX->width*inCode_VideoCTX->height/4 );
                    memcpy( pFrame->data[2], startFrame->data[2], inCode_VideoCTX->width*inCode_VideoCTX->height/4 );
                }

                //  printf("gframe ,send frame\n");
                startFrame->pts=pFrame->pts;
                startFrame->pkt_dts=pFrame->pts;
                startFrame->key_frame=pFrame->key_frame;
#endif
                else
                  av_frame_copy(pFrame,startFrame);
                ret = avcodec_send_frame(outCode_VideoCTX,pFrame);
                if(ret < 0)
                {
                    printf("avcodec_send_frame fail.\n");
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
                        printf("avcodec_receive_packet fail.\n");
                        break;
                    }
                    // printf("get out packet,write\n");
                    if(frist_out_packe<0)
                    {
                        play_time=av_gettime();
                        //printf("tanscode time is %lld\n",play_time);
                        frist_out_packe=1;
                    }
                    AVRational time_base=out_video_stream->time_base;
                    AVRational time_base_q={1,AV_TIME_BASE};

                    out_pkt.pts = av_rescale_q_rnd(out_pkt.pts, ifmt_ctx->streams[video_index]->time_base, out_video_stream->time_base, AV_ROUND_NEAR_INF);
                    out_pkt.dts = av_rescale_q_rnd(out_pkt.dts, ifmt_ctx->streams[video_index]->time_base, out_video_stream->time_base, AV_ROUND_NEAR_INF);
                    out_pkt.duration = av_rescale_q(out_pkt.duration, ifmt_ctx->streams[video_index]->time_base, out_video_stream->time_base);
                    out_pkt.pos=-1;

                    int64_t now_pts = av_rescale_q((out_pkt.pts), time_base, time_base_q);
                    //printf("pkt pts is %lld,time is %lld\n",now_pts,av_gettime()-play_time);
                    if (now_pts > (av_gettime()-play_time))
                        av_usleep(now_pts - (av_gettime()-play_time));
                    ret = av_interleaved_write_frame(ofmt_ctx_mp4, &out_pkt);
                    if (ret < 0)
                    {
                        printf( "Error write packet\n");
                        break;
                    }
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
    avio_close(ofmt_ctx_mp4->pb);
    avformat_free_context(ofmt_ctx_mp4);
    return ret;
}
