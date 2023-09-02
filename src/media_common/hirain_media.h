/**
 *   File: hirain_media.h & hirain_media.cpp
 *  Usage: Streaming media processing
 * Author: zk
 *   Date: 2022.8.11
 */
#ifndef __HIRAIN_MEDIA_H__
#define __HIRAIN_MEDIA_H__

#include <stdio.h>
#include "Thread11.h"
#include "minio.h"

#ifdef __cplusplus
extern "C" {
#endif
#include "libavutil/mem.h"  // av_free()
#ifdef __cplusplus
};
#endif

#define BUFF_SIZE  1048576       // 1M

 ///////////////////////////////////////////////////////////////////////////////////////////
 // 2022.3.21 zk
typedef struct tag_stream_param {
    // Input
    const char* in_url = NULL;     // ����URL(��:rtsp,�����ļ�·����)
    int (*read_packet)(void* opaque, uint8_t* buf, int buf_size) = NULL;// �Զ�������ص�
    int64_t (*seek_packet)(void* opaque, int64_t offset, int whence) = NULL;
    void* in_opaque = NULL;
    int64_t start_time = 0;         // ¼����ʼʱ��(���ű���¼���ļ�ʱ��Ч)
    float* ratio = NULL;            // ���ű���,�ط�ʱ��Ч (�������)
    /// /////////////////////////////
    // Output
    const char* out_url = NULL;    // ����ļ�����
    int (*write_packet)(void* opaque, uint8_t* buf, int buf_size) = NULL;// �Զ�������ص�
    void* out_opaque = NULL;
    // test
    const char* chl = NULL;         // ͨ��ID
}stream_param_t;

typedef struct IOContext {
    int64_t pos = 0;        // ��ǰλ��
    int64_t filesize = 0;   // �ļ���С
    uint8_t* fuzz = NULL;   // ������ʼָ��
    int64_t fuzz_size = 0;   // ʣ�����ݴ�С
    const char* remote_name = NULL;
    bool enable = true;
} IOContext;
typedef struct stream_buffer
{
    unsigned char* buffer = NULL;
    int64_t buf_size = 1024 * 1024 * 16;  // 16M
    unsigned char* pos = NULL;
    unsigned char* limit = NULL;
    int write_size = 0;
    bool video_head = false;    // �Ƿ�����Ƶͷ֡
    bool reconnet = false;      // �Ƿ��Ƕ������������Ƶͷ֡
    ~stream_buffer() {
        if (NULL != buffer)
            av_free(buffer);
    }
} stream_buffer_t;

typedef int(*img_data_cbk)(void* opaque, uint8_t* buf, int buf_size);

enum media_error_code {
    NO_ERRORS = 0,            //�޴���
    ERROR_ARG,              //��������
    ERROR_OPEN_URL_FAIL,    //��rtsp url ʧ��
    ERROR_OPEN_FILE_FAIL,   //�򿪷�Ƭ�ļ�ʧ��
    ERROR_STREAM_INFO_FAIL,//��������Ϣʧ��
    ERROR_OPEN_OUTSTREAM_FAIL,//�����ʧ��
    ERROR_CREATE_AVSTREAM_FAIL,//���������ʧ��
    ERROR_WRITE_PKT_FAIL,   //д��ʧ��
    ERROR_NO_VIDEO_STREAM,  //û����Ƶ��
    ERROR_SEEK_FAIL,        //�϶�ʧ��
    ERROR_READ_FRAME_FAIL,  //����ʧ��
    ERROR_MEM_FAIL,         //�ڴ�������
    ERROR_IOCONTENT_FAIL,   //�������IOʧ��
    ERROR_FIND_CODE_FAIL,   //���ұ����ʽʧ��
    ERROR_CREATE_CODECOTENT_FAIL, //�������������ʧ��
    ERROR_OPEN_CODE_FAIL,  //�򿪽���ʧ��
    ERROR_DECODE_PACKET_FAIL,//����ʧ��
    ERROR_ENCODE_PACKET_FAIL,//����ʧ��
    VIDEO_ONLY,             //ֻ����Ƶ��
    VIDEO_AND_AUDIO,        //����Ƶһ��
    FILE_EOF                //�ļ�����
};


int media_init(minio_info_t *cfg);
void media_release();

int media_video_to_image(stream_param_t* params);
int image_write_packet(void* opaque, uint8_t* buf, int buf_size);

int media_video_stream(char* control_cmd, stream_param_t* params);

int media_read_packet(void* opaque, uint8_t* buf, int buf_size);
int media_write_packet(void* opaque, unsigned char* buf, int buf_size);
int64_t media_seek_packet(void* opaque, int64_t offset, int whence);
void STDCALL media_download_thdproc(STATE& state, PAUSE& p, void* pUserData);
int STDCALL media_minio_callback(void* data, size_t size, void* arg);
int media_create_object_cbk(void*& obj, int& obj_size, void* user_data);
void media_destory_object_cbk(void*& obj, void* user_data);
int media_interrupt_cbk(void* opaque);

#endif