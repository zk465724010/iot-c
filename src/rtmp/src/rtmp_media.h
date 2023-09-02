
#ifndef __RTMP_MEDIA_H__
#define __RTMP_MEDIA_H__

#include "rtmp_stream.h"
#include "cmap.h"


typedef struct rtmp_chl_t
{
	//char id[32] = {0};			// ͨ��ID
	CRtmpStream *stream = NULL;
	~rtmp_chl_t() {
	}
}rtmp_chl_t;

class CRtmpMedia
{
public:
	CRtmpMedia();
	~CRtmpMedia();

	int create_channel(const char *id, stream_param_t *param);

	int destroy_channel(const char *id, bool is_sync = true);
	int destroy_channels(bool is_sync = true);

	const rtmp_chl_t* get_channel(const char *id);

	string random_string(int length);


private:
	CMap<string, rtmp_chl_t*> m_chl_map;

#ifdef __THREAD11_H__
public:
	void pause_channel(const char *id);		// ��ͣ ����/����
	void continue_channel(const char *id);	// ����
	void join_channel(const char *id);		// 
#endif

};

#endif
