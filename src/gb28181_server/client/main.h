
#ifndef __MAIN_H__
#define __MAIN_H__
#include "common.h"
#include "cmap.h"
#include "Thread11.h"

typedef struct user_data2_t {
	addr_info_t dev;
	unis_record_t rec;
}user_data2_t;

int main(int argc, char* argv[]);
int load_config(const char* filename, local_cfg_t* cfg, CMap<string, dev_info_t>* dev_map);
int record(cmd_e cmd, addr_info_t* dev, unis_record_t* rec);
int on_gb_record(void* data, int size, void* arg);
int on_play_media(void* data, int size, void* arg);
int do_main(int autom = 0);
void STDCALL on_record_proc(STATE& state, PAUSE& p, void* pUserData);
char get_char();


#endif