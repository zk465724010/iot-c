
#include "rtmp_api.h"

CRtmpMedia g_rtmp_media;

// 创建通道
int create_channel(const char *id, stream_param_t *param)
{
	return g_rtmp_media.create_channel(id, param);
}

// 销毁通道
int destroy_channel(const char *id, bool is_sync/*=true*/)
{
	return g_rtmp_media.destroy_channel(id, is_sync);
}

// 销毁通道
int destroy_channels(bool is_sync/*=true*/)
{
	return g_rtmp_media.destroy_channels(is_sync);
}
