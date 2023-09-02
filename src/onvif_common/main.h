
#ifndef __MAIN_H__
#define __MAIN_H__

#include "stdsoap2.h"

int image_test(const char* device_service);
int ptz_test1(const char* device_service);
int ptz_test2(const char* device_service);
int osd_test(const char* device_service);
int video_get_range1(const char *device_service);
int video_get_range2(const char* device_service);

void ONVIF_DetectDevice(); // …Ë±∏ÃΩ≤‚

#endif