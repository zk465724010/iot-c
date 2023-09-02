
#ifndef __MAIN_H__
#define __MAIN_H__

#include <stdio.h>

int main();
int init();
void release();
int main_write_packet(void* opaque, unsigned char* buf, int size);


#endif
