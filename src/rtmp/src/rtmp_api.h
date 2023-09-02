
#ifndef __RTMP_API_H__
#define __RTMP_API_H__

#include "rtmp_media.h"

int create_channel(const char *id, stream_param_t *param);
int destroy_channel(const char *id, bool is_sync = true);
int destroy_channels(bool is_sync = true);

#endif
