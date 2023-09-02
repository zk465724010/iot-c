
#include "rtmp_api.h"

CRtmpMedia g_rtmp_media;

// ����ͨ��
int create_channel(const char *id, stream_param_t *param)
{
	return g_rtmp_media.create_channel(id, param);
}

// ����ͨ��
int destroy_channel(const char *id, bool is_sync/*=true*/)
{
	return g_rtmp_media.destroy_channel(id, is_sync);
}

// ����ͨ��
int destroy_channels(bool is_sync/*=true*/)
{
	return g_rtmp_media.destroy_channels(is_sync);
}
