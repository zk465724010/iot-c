
#ifndef __PARSE_XML_H__
#define __PARSE_XML_H__

#include "common.h"
#include "cmap.h"

 #define USE_TEST_XML

int parse_device_info(const char *xml);

#if 1
int parse_sip_body(const char *xml,  dev_info_t *body, const dev_info_t *dev);
#else
int parse_sip_body(const char *xml, const addr_info_t *dev,  sip_body_t *sip);
#endif


//string make_response(const char *cmd_type, sip_body_t *resp_body);


//time_t string_to_timestamp(const char *pTime);
string timestamp_to_string(time_t time, char *buf, int buf_size, char ch='-');


#ifdef USE_TEST_XML
void print_device_info(dev_info_t *dev);
void print_device_map(CMap<string, dev_info_t> *dev_map);

void print_access_map(CMap<string, access_info_t> *access_map);


void print_channel_info(chl_info_t *chl);
void print_channel_map(map<string, chl_info_t> *chl_map);

void print_platform_info(pfm_info_t *pfm);
void print_platform_map(map<string, pfm_info_t> *pfm_map);

void print_local_info(local_cfg_t *local_cfg);

void print_record_map(map<string, record_info_t> *rec_map);
void print_record_info(record_info_t *rec);

#endif




#endif
