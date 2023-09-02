
#ifndef __TEST__H__
#define __TEST__H__

#include "common.h"
#include "cmap.h"
#include "parse_xml.h"

int load_gb28181client_config(const char *filename, local_cfg_t *cfg, CMap<string, dev_info_t> *dev_map);
int load_gb28181server_config(const char *filename, local_cfg_t *local_cfg, CMap<string, dev_info_t> *dev_map, CMap<string, pfm_info_t> *pfm_map, CMap<string, access_info_t> *access_map);

char get_char();
//time_t string_to_timestamp(const char *pTime);

#endif