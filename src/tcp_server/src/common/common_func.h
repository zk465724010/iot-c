
#ifndef __COMMON_FUNC_H__
#define __COMMON_FUNC_H__

#include <time.h>
#include <string>
using namespace std;


char get_char();
time_t string_to_timestamp(const char *time_str);
string timestamp_to_string(time_t t, char ch = '-');
int timestamp_to_string(time_t t, char *buff, int buf_size,char ch = '-');
void init_rand();
string random_string(int length);






#endif
