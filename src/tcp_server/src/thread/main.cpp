
#include "main.h"
#include <stdio.h>
#include "thread11.h"
#include "common_func.h"

#ifdef _WIN32
#include <Windows.h>	// Sleep()
#elif __linux__
#include <unistd.h>		// usleep()
#endif

FILE *g_log_file = fopen("thread.log", "w");

CThread11 g_thd;
void STDCALL thread_proc(STATE &state, PAUSE &p, void *pUserData);

CThread11 g_control_thd;
void STDCALL control_proc(STATE &state, PAUSE &p, void *pUserData);

int main()
{
	printf("Hello,thread.\n\n");

	g_thd.Run(thread_proc);

	g_control_thd.Run(control_proc);
	g_control_thd.Join();

	g_thd.Stop();

	#ifdef _WIN32
	system("pause");
	#else
	printf("\n Program exit.\n");
	#endif
	return 0;
}

void STDCALL thread_proc(STATE &state, PAUSE &p, void *pUserData)
{
	int n = 0;
	while (TS_STOP != state)
	{
		if (TS_PAUSE == state)
		{
			p.wait();
		}
		printf("-- %d --\n", n++);
		if (n >= 20)
			break;
		#ifdef _WIN32
			Sleep(500);
		#elif __linux__
			usleep(500000);
		#endif
	}
}

void STDCALL control_proc(STATE &state, PAUSE &p, void *pUserData)
{
	#ifdef _WIN32
		Sleep(200);
	#elif __linux__
		usleep(200000);
	#endif

	srand(time(NULL));
	char ch = 0;
	while (TS_STOP != state)
	{
		//printf("*\n", ch);
		if (TS_PAUSE == state)
		{
			printf("pause\n");
			p.wait();
		}
		#if 0
			ch = get_char();
		#else
			ch = '0' + (rand() % 7);
			while ('0' == ch) {
				ch = '0' + (rand() % 7);
				//printf("'%c'\n", ch);
			}
		#endif
		//printf("'%c'\n", ch);
		switch (ch) {
			case '0': {	// 退出
				return;
			}break;
			case '1': {	// 暂停
				printf("Pause\n\n");
				g_thd.Pause();
			}break;
			case '2': {	// 继续
				printf("Continue\n\n");
				g_thd.Continue();
			}break;
			case '3': {	// 停止
				printf("Stop 1\n\n");
				//Sleep(500);
				g_thd.Stop();
				printf("Stop 2\n\n");
			}break;
			case '4': {	// 运行
				printf("Run\n\n");
				g_thd.Run(thread_proc);
			}break;
			case '5': {	// Join
				printf("Join\n\n");
				g_thd.Continue();
				g_thd.Join();
			}break;
			case '6': {	// Detach
				printf("Detach\n\n");
				g_thd.Detach();
			}break;
		}
		#ifdef _WIN32
			//Sleep(10);
		#elif __linux__
			usleep(10000);
		#endif
	}
}