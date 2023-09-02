
#include "common_func.h"
#include <stdio.h>
#include <sstream>
#ifdef _WIN32
	#include <conio.h>		// _getch();
#elif __linux__
	#include <termios.h>	// tcgetattr
	#include <unistd.h>
	#include <string.h>		//	strlen()
#endif


char get_char()
{
#ifdef _WIN32
	return _getch();
#elif __linux__
	//#include <termios.h>  // tcgetattr
	//#include <unistd.h>
	struct termios stored_settings;
	struct termios new_settings;
	tcgetattr(0, &stored_settings);
	new_settings = stored_settings;
	new_settings.c_lflag &= (~ICANON);
	new_settings.c_cc[VTIME] = 0;
	new_settings.c_cc[VMIN] = 1;
	tcsetattr(0, TCSANOW, &new_settings);
	char c = getchar();
	putchar('\b'); // 删除回显
	//printf("input:  [%c]\n", c);
	tcsetattr(0, TCSANOW, &stored_settings); // 恢复终端参数
	return c;
#endif
}

time_t string_to_timestamp(const char *time_str)
{
	struct tm time;
	#if 1
		if (time_str[4] == '-' || time_str[4] == '.' || time_str[4] == '/')
		{
			char szFormat[30] = { 0 };				//"%u-%u-%u","%u.%u.%u","%u/%u/%u")
			sprintf(szFormat, "%%d%c%%d%c%%dT%%d:%%d:%%d", time_str[4], time_str[4]);
			sscanf(time_str, szFormat, &time.tm_year, &time.tm_mon, &time.tm_mday, &time.tm_hour, &time.tm_min, &time.tm_sec);
			time.tm_year -= 1900;
			time.tm_mon -= 1;
			return mktime(&time);
		}
	#else
		// 字符串转时间戳
		struct tm tm_time;
		strptime("2010-11-15 10:39:30", "%Y-%m-%d %H:%M:%S", &time);
		return mktime(&time);
	#endif
	return -1;
}

string timestamp_to_string(time_t t, char ch/*='-'*/)
{
	char buff[64] = { 0 };
	#if 0
		// localtime获取系统时间并不是线程安全的(函数内部申请空间,返回时间指针)
		//time_t tick = time(NULL);
		struct tm *pTime = localtime(&t);
		if (ch == '-')
			strftime(buff, sizeof(buff), "%Y-%m-%d %H:%M:%S", pTime);
		else if (ch == '/')
			strftime(buff, sizeof(buff), "%Y/%m/%d %H:%M:%S", pTime);
		else if (ch == '.')
			strftime(buff, sizeof(buff), "%Y.%m.%d %H:%M:%S", pTime);
		else strftime(buff, sizeof(buff), "%Y%m%d%H%M%S", pTime);
	#else
		#ifdef _WIN32
			// localtime_s也是用来获取系统时间,运行于windows平台下,与localtime_r只有参数顺序不一样
			struct tm now_time;
			localtime_s(&now_time, &t);
			sprintf(buff, "%4.4d%c%2.2d%c%2.2d %2.2d:%2.2d:%2.2d", now_time.tm_year + 1900, ch, now_time.tm_mon + 1, ch, now_time.tm_mday,
				now_time.tm_hour, now_time.tm_min, now_time.tm_sec);
		#elif __linux__
			// localtime_r也是用来获取系统时间,运行于linux平台下,支持线程安全
			//struct tm* localtime_r(const time_t * timep, struct tm* result);
			struct tm tm1;
			localtime_r(&t, &tm1);
			sprintf(buff, "%4.4d%c%2.2d%c%2.2d %2.2d:%2.2d:%2.2d", tm1.tm_year + 1900, ch, tm1.tm_mon + 1, ch, tm1.tm_mday, tm1.tm_hour, tm1.tm_min, tm1.tm_sec);
		#endif
	#endif
	return string(buff);
}
int timestamp_to_string(time_t t, char* buff, int buf_size, char ch/*='-'*/)
{
	int nRet = -1;
	if ((NULL == buff) || (buf_size <= 0))
		return nRet;

	//char buff[64] = { 0 };
	#if 0
		// localtime获取系统时间并不是线程安全的(函数内部申请空间,返回时间指针)
		//time_t tick = time(NULL);
		struct tm* pTime = localtime(&t);
		if (ch == '-')
			strftime(buff, buf_size, "%Y-%m-%d %H:%M:%S", pTime);
		else if (ch == '/')
			strftime(buff, buf_size, "%Y/%m/%d %H:%M:%S", pTime);
		else if (ch == '.')
			strftime(buff, buf_size, "%Y.%m.%d %H:%M:%S", pTime);
		else strftime(buff, buf_size, "%Y%m%d%H%M%S", pTime);
	#else
		#ifdef _WIN32
			// localtime_s也是用来获取系统时间,运行于windows平台下,与localtime_r只有参数顺序不一样
			struct tm now_time;
			nRet = localtime_s(&now_time, &t);
			sprintf_s(buff, buf_size,"%4.4d%c%2.2d%c%2.2d_%2.2d_%2.2d_%2.2d", now_time.tm_year + 1900, ch, now_time.tm_mon + 1, ch, now_time.tm_mday,
				now_time.tm_hour, now_time.tm_min, now_time.tm_sec);
		#elif __linux__
			// localtime_r也是用来获取系统时间,运行于linux平台下,支持线程安全
			//struct tm* localtime_r(const time_t * timep, struct tm* result);
			struct tm tm1;
			struct tm* result = localtime_r(&t, &tm1);
			nRet = (NULL != result) ? 0 : -1;
			sprintf(buff, "%4.4d%c%2.2d%c%2.2d_%2.2d_%2.2d_%2.2d", tm1.tm_year + 1900, ch, tm1.tm_mon + 1, ch, tm1.tm_mday, 
				tm1.tm_hour, tm1.tm_min, tm1.tm_sec);
		#endif
	#endif
	return nRet;
}
void init_rand()
{
	srand(time(NULL));		// 初始化随机数种子
}
std::string random_string(int length)
{
	static char str[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-=[],.~@#$%^&*)(_+><?/|\{}";
	static int len = strlen(str);
	//srand(time(NULL));				// 初始化随机数种子
	std::stringstream str_random;		// #include <sstream>
	for (int i = 0; i < length; i++)
	{
		str_random << str[rand() % len];
	}
	//static int min = 1000000;
	//static int max = 9999999999;
	//srand(time(NULL));
	//unsigned int nRand = rand() % (max - min + 1) + min;
	return str_random.str();
}
