
#include "main.h"
#include "log.h"
#include <stdio.h>
#ifdef _WIN32
#include <Windows.h>
#endif

FILE *g_log_file = fopen("test.log", "w");

int main()
{
	LOG("INFO", "Hello,log!\n\n");
	LOG3("ERROR", "123456\n\n");


	#ifdef _WIN32
	system("pause");
	#endif
    return 0;
}