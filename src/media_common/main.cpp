
//#define HIRAIN_MEDIA_H

#include "main.h"
#include "log.h"
#ifdef HIRAIN_MEDIA_H
    #include "hirain_media.h"

    extern CMinIO* g_file_io;
#else
    #include "media.h"
    extern CMinIO* g_minio;
#endif

int main()
{
    init();

	char control_cmd[] = {"rr"};
#if 1
    // 点播与回放
	stream_param_t params;
    float playspeed = 2.0;
    params.ratio = &playspeed;
    #if 0       // 点播
        #ifdef _WIN32
        params.in_url = "C:/Users/zhangkun/Desktop/2022080217081800003.mp4";
        //params.in_url = "rtsp://admin:abcd1234@192.168.1.64:554/Streaming/Channels/101";
        #else
        params.in_url = "/home/zk/minio_root/video-bucket/2022080217081800003.mp4";
        #endif
    #else       // 回放
        //params.in_url = "2022080217081800003.mp4";
        params.in_url = "/VideoRecord/ONVIF/2022052310043300575/2022-08-02/2022080217081800003.mp4";
        params.in_custom = read_packet;
        params.start_time = 30;
        //params.duration = 249;  //录像时长(单位:秒) 255
        printf("[Playback] start_time:%lld, ratio:%0.3f, filename:'%s'\n", params.start_time,*params.ratio, params.in_url);
    #endif
        //params.out_opaque = "C:/Users/zhangkun/Desktop/output2.jpg";

    #ifdef HIRAIN_MEDIA_H
        params.write_packet = media_write_packet;
	    int ret = media_video_stream(control_cmd, &params);

        //params.write_packet = image_write_packet;
        //int ret = media_video_to_image(&params);
        //image_write_packet(params.out_opaque, NULL, 0);
    #else
        params.out_custom = main_write_packet;
        params.out_opaque = "C:/Users/zhangkun/Desktop/output.mp4";
        //int ret = video_stream(control_cmd, &params);
        int ret = playback_stream(control_cmd, &params);

    #endif


#else
    // 录像
    const char *url  = "rtsp://admin:abcd1234@192.168.1.64:554/Streaming/Channels/101";
    const char *output = "C:/Users/zhangkun/Desktop/output.mp4";
    int video_type = 0;
    int recordtime = 0;
    int stopsecond = 60;
    int nRet = rtsp_slice_autostop(url, output, control_cmd, stopsecond, &video_type, &recordtime);
#endif

    release();

    #ifdef _WIN32
    system("pause");
    #else
    printf("program exit.\n");
    #endif
    return 0;
}
int init()
{
#ifdef HIRAIN_MEDIA_H
    if (NULL == g_file_io) {
        minio_info_t cfg;
        strcpy(cfg.host, "127.0.0.1");
        //strcpy(cfg.host, "10.180.16.60");
        //strcpy(cfg.host, "172.17.0.12"); // 腾讯云服务器(内网IP)
        cfg.port = 9002;
        strcpy(cfg.username, "admin");
        strcpy(cfg.password, "12345678");
        strcpy(cfg.bucket, "video-bucket");
        media_init(&cfg);
    }
#else
    if (NULL == g_minio) {
        minio_info_t cfg;
        //strcpy(cfg.host, "127.0.0.1");
        //strcpy(cfg.host, "10.180.16.60");
        //strcpy(cfg.host, "172.17.0.12"); // 腾讯云服务器(内网IP)
        //cfg.port = 9002;
        strcpy(cfg.host, "10.10.2.41");     // 外网MinIO
        cfg.port = 9000;
        strcpy(cfg.username, "admin");
        strcpy(cfg.password, "12345678");
        strcpy(cfg.bucket, "video-bucket");
        g_minio = new CMinIO();
        g_minio->print_info(&cfg);
        g_minio->init(&cfg);
    }
#endif
    return 0;
}
void release()
{
#ifdef HIRAIN_MEDIA_H
    media_release();
#endif
}

int main_write_packet(void* opaque, unsigned char* buf, int size)
{
    const char* filename = (const char*)opaque;
    //printf("# write_packet  data:%p size:%d  time:%lld\n", buf, size, time(NULL));
#if 1
    static FILE* fp = fopen(filename, "wb");
    if (NULL != fp) {
        if ((NULL != buf) && (size > 0)) {
            fwrite(buf, 1, size, fp);
        }
        else {
            fclose(fp);
            fp = NULL;
            LOG("ERROR", "File closed '%s'\n", filename);
        }
    }
    else LOG("ERROR","Open file failed '%s'\n", filename);
#endif
    return size;
}

