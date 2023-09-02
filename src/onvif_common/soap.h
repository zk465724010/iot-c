
#ifndef __SOAP_H__
#define __SOAP_H__

//#include <stdio.h>
//#include <stdlib.h>
//#include <assert.h>
//#include "soapH.h"
//#include "wsaapi.h"
#include "stdsoap2.h"

#ifdef __cplusplus
extern "C" {
#endif

void* onvif_soap_malloc(struct soap* soap, size_t size);

struct soap* onvif_soap_new(int timeout);
void onvif_soap_free(struct soap* soap);
// 设置认证信息
int onvif_set_auth_info(struct soap* soap, const char* username, const char* password);
void onvif_init_header(struct soap* soap);

#ifdef __cplusplus
}
#endif

#endif