
//////////////////////////////////////////////////////////////////////////////////////////////
// 文件名称：log.h																			//
// 作    用：日志																			//
// 使用平台：Windows / Linux																//
// 作    者：zk	 																			//
// 编写日期：2018.12.15 16:58 六 															//
//////////////////////////////////////////////////////////////////////////////////////////////
// 日志格式: 应包含: 日期、时间、日志级别、代码位置、日志内容、错误码等信息
// [2021-09-28 16:49:50][ERROR][main.cpp main 43]: 222222

#ifndef __MY_LOG_H__
#define __MY_LOG_H__

#include <stdio.h>
#include <time.h>

//#define USE_LOG_FILE

#ifdef _WIN32
	#include <Windows.h>
	// 截取短文件名称
	//#define f_name(x) strrchr((x),'\\') ? strrchr((x),'\\')+1 : (x)
	#define f_name(s,ch) strrchr((s),ch) ? strrchr((s),ch)+1 : (s)
	// localtime_s是windows平台下用来获取系统时间的,与localtime_r(linux版)只有参数顺序不一样
	#define local_time(m,t) localtime_s(&m, &t)
#elif __linux__
	#include <string.h>
	#include<sys/time.h>	// struct timeval, localtime()
	//#define f_name(x) strrchr((x),'/') ? strrchr((x),'/')+1 : (x)
	#define f_name(s,ch) strrchr((s),ch) ? strrchr((s),ch)+1 : (s)
	#define local_time(m,t) localtime_r(&t, &m)
#endif

//static char err[6][8] = {"FATAL","ERROR","WARN","INFO","DEBUG","TRACE"};

#ifdef _WIN32
#if 1
	//------------ 控制台颜色宏定义 -------------
	// 前景颜色
	#define	F_BLACK		0x0000				//黑色		
	#define	F_BLUE		0x0001				//蓝色		0x0009: 蓝色[高亮]
	#define	F_GREEN		0x0002				//绿色		0x000A: 绿色[高亮]
	#define	F_AZURE		0x0003				//天蓝色	0x000B: 天蓝色[高亮]
	#define	F_RED		0x0004				//红色		0x000C: 红色[高亮]
	#define	F_PURPLE	0x0005				//紫色		0x000D: 紫色[高亮]
	#define	F_YELLOW	0x0006				//黄色		0x000E: 黄色[高亮]
	#define	F_WHITE		0x0007				//白色		0x000F: 白色[高亮]
	#define	F_GRAY		0x0008				//灰色
	#define	F_BLUE_G	0x0009				//蓝色[高亮]
	#define	F_GREEN_G	0x000A				//绿色[高亮]
	#define	F_AZURE_G	0x000B				//天蓝色[高亮]
	#define	F_RED_G		0x000C				//红色[高亮]
	#define	F_PURPLE_G	0x000D				//紫色[高亮]
	#define	F_YELLOW_G	0x000E				//黄色[高亮]
	#define	F_WHITE_G	0x000F				//白色[高亮]
	// 背景颜色
	#define	B_BLACK		0x0000				//黑色		
	#define	B_BLUE		0x0010				//蓝色		0x0009: 蓝色[高亮]
	#define	B_GREEN		0x0020				//绿色		0x000A: 绿色[高亮]
	#define	B_AZURE		0x0030				//天蓝色	0x000B: 天蓝色[高亮]
	#define	B_RED		0x0040				//红色		0x000C: 红色[高亮]
	#define	B_PURPLE	0x0050				//紫色		0x000D: 紫色[高亮]
	#define	B_YELLOW	0x0060				//黄色		0x000E: 黄色[高亮]
	#define	B_WHITE		0x0070				//白色		0x000F: 白色[高亮]
	#define	B_GRAY		0x0080				//灰色
	#define	B_BLUE_G	0x0090				//蓝色[高亮]
	#define	B_GREEN_G	0x00A0				//绿色[高亮]
	#define	B_AZURE_G	0x00B0				//天蓝色[高亮]
	#define	B_RED_G		0x00C0				//红色[高亮]
	#define	B_PURPLE_G	0x00D0				//紫色[高亮]
	#define	B_YELLOW_G	0x00E0				//黄色[高亮]
	#define	B_WHITE_G	0x00F0				//白色[高亮]
	// stdin,stdout,stderr
	#ifdef USE_LOG_FILE
		extern FILE* g_log_file;
		#define LOG(level,format,...) {								\
			static HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);\
			static CONSOLE_SCREEN_BUFFER_INFO old_info;				\
			static struct tm m;			\
			static time_t t = 0;		\
			t = time(NULL);				\
			local_time(m,t);			\
			char szTime[64] = { 0 };	\
			sprintf(szTime, "%4.4d-%2.2d-%2.2d %2.2d:%2.2d:%2.2d", m.tm_year + 1900, m.tm_mon + 1, m.tm_mday,m.tm_hour, m.tm_min, m.tm_sec);\
			if(0 == stricmp("ERROR", (level)))	{						\
				GetConsoleScreenBufferInfo(hConsole, &old_info);		\
				SetConsoleTextAttribute(hConsole, F_RED_G);				\
				fprintf(stdout, "[%s][%s][%s %d]: " format, szTime, level, f_name(__FILE__,'\\'), __LINE__, ##__VA_ARGS__);\
				SetConsoleTextAttribute(hConsole, old_info.wAttributes);\
			}															\
			else if(0 == stricmp("WARN", (level)))	{					\
				GetConsoleScreenBufferInfo(hConsole, &old_info);		\
				SetConsoleTextAttribute(hConsole, F_YELLOW);			\
				fprintf(stdout, "[%s][%s][%s %d]: " format, szTime, level, f_name(__FILE__,'\\'), __LINE__, ##__VA_ARGS__);\
				SetConsoleTextAttribute(hConsole, old_info.wAttributes);\
			}															\
			else fprintf(stdout, "[%s][%s][%s %d]: " format, szTime, level, f_name(__FILE__), __LINE__, ##__VA_ARGS__);\
			if(NULL != g_log_file) {	\
				fprintf(g_log_file,"[%s][%s][%s %d]: " format, szTime,level,f_name(__FILE__,'\\'),__LINE__,##__VA_ARGS__),\
				fflush(g_log_file);		\
			}							\
		}
	#else
		#define LOG(level,format,...) {								\
			static HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);\
			static CONSOLE_SCREEN_BUFFER_INFO old_info;				\
			static struct tm m;			\
			static time_t t = 0;		\
			t = time(NULL);				\
			local_time(m,t);			\
			char szTime[64] = { 0 };	\
			sprintf_s(szTime,64, "%4.4d-%2.2d-%2.2d %2.2d:%2.2d:%2.2d", m.tm_year + 1900, m.tm_mon + 1, m.tm_mday,m.tm_hour, m.tm_min, m.tm_sec);\
			if(0 == stricmp("ERROR", (level)))	{						\
				GetConsoleScreenBufferInfo(hConsole, &old_info);		\
				SetConsoleTextAttribute(hConsole, F_RED_G);				\
				fprintf(stdout, "[%s][%s][%s %d]: " format, szTime, level, f_name(__FILE__,'\\'), __LINE__, ##__VA_ARGS__);\
				SetConsoleTextAttribute(hConsole, old_info.wAttributes);\
			}															\
			else if(0 == stricmp("WARN", (level)))	{					\
				GetConsoleScreenBufferInfo(hConsole, &old_info);		\
				SetConsoleTextAttribute(hConsole, F_YELLOW);			\
				fprintf(stdout, "[%s][%s][%s %d]: " format, szTime, level, f_name(__FILE__,'\\'), __LINE__, ##__VA_ARGS__);\
				SetConsoleTextAttribute(hConsole, old_info.wAttributes);\
			}															\
			else fprintf(stdout, "[%s][%s][%s %d]: " format, szTime, level, f_name(__FILE__,'\\'), __LINE__, ##__VA_ARGS__);\
		}
	#endif // USE_LOG_FILE
#endif // #if 0

#elif __linux__

	#ifdef USE_LOG_FILE
		extern FILE* g_log_file;
		//#define LOG(level,format,...)  (printf("[%s][%s %s %d]: "format, level,f_name(__FILE__,'/'), __FUNCTION__, __LINE__, ##__VA_ARGS__),fprintf(g_log_file,"[%s][%s %s %d]: "format, level,short_file_name(__FILE__), __FUNCTION__, __LINE__, ##__VA_ARGS__),fflush(g_log_file))
		//#define LOG_FATAL(format,...)  (printf("[FATAL][%s %s %d]: "format,f_name(__FILE__,'/'), __FUNCTION__, __LINE__, ##__VA_ARGS__),fprintf(g_log_file,"[FATAL][%s %s %d]: "format,short_file_name(__FILE__), __FUNCTION__, __LINE__, ##__VA_ARGS__),fflush(g_log_file))
		//#define LOG_ERROR(format,...)  (printf("[ERROR][%s %s %d]: "format,f_name(__FILE__,'/'), __FUNCTION__, __LINE__, ##__VA_ARGS__),fprintf(g_log_file,"[ERROR][%s %s %d]: "format,short_file_name(__FILE__), __FUNCTION__, __LINE__, ##__VA_ARGS__),fflush(g_log_file))
		//#define LOG_WARN(format,...)  (printf("[WARN][%s %s %d]: "format,f_name(__FILE__,'/'), __FUNCTION__, __LINE__, ##__VA_ARGS__),fprintf(g_log_file,"[WARN][%s %s %d]: "format,short_file_name(__FILE__), __FUNCTION__, __LINE__, ##__VA_ARGS__),fflush(g_log_file))
		//#define LOG_INFO(format,...)  (printf("[INFO][%s %s %d]: "format,f_name(__FILE__,'/'), __FUNCTION__, __LINE__, ##__VA_ARGS__),fprintf(g_log_file,"[INFO][%s %s %d]: "format,short_file_name(__FILE__), __FUNCTION__, __LINE__, ##__VA_ARGS__),fflush(g_log_file))
		//#define LOG_DEBUG(format,...)  (printf("[DEBUG][%s %s %d]: "format,f_name(__FILE__,'/'), __FUNCTION__, __LINE__, ##__VA_ARGS__),fprintf(g_log_file,"[DEBUG][%s %s %d]: "format,short_file_name(__FILE__), __FUNCTION__, __LINE__, ##__VA_ARGS__),fflush(g_log_file))
		//#define LOG_TRACE(format,...)  (printf("[TRACE][%s %s %d]: "format,f_name(__FILE__,'/'), __FUNCTION__, __LINE__, ##__VA_ARGS__),fprintf(g_log_file,"[TRACE][%s %s %d]: "format,short_file_name(__FILE__), __FUNCTION__, __LINE__, ##__VA_ARGS__),fflush(g_log_file))
		// 格式: [2021-10-28 11:41:34][INFO][test.cpp 121]: xxxxxx
		#define LOG(level,format,...) {	\
			static struct tm m;			\
			static time_t t = 0;		\
			t = time(NULL);				\
			local_time(m,t);			\
			char szTime[64] = { 0 };	\
			sprintf(szTime, "%4.4d-%2.2d-%2.2d %2.2d:%2.2d:%2.2d", m.tm_year + 1900, m.tm_mon + 1, m.tm_mday,m.tm_hour, m.tm_min, m.tm_sec);\
			if(0 == strcmp("ERROR", (level)))	\
				fprintf(stdout,"\033[31m[%s][%s][%s %d]: " format"\033[0m", szTime,level,f_name(__FILE__,'/'),__LINE__,##__VA_ARGS__);\
			else if(0 == strcmp("WARN",(level)))\
				fprintf(stdout, "\033[33m[%s][%s][%s %d]: " format"\033[0m", szTime, level, f_name(__FILE__,'/'), __LINE__, ##__VA_ARGS__);\
			else fprintf(stdout, "[%s][%s][%s %d]: " format, szTime, level, f_name(__FILE__,'/'), __LINE__, ##__VA_ARGS__);\
			if(NULL != g_log_file) {	\
				fprintf(g_log_file,"[%s][%s][%s %d]: " format, szTime,level,f_name(__FILE__,'/'),__LINE__,##__VA_ARGS__),\
				fflush(g_log_file);		\
			}							\
		}
	#else
		// stdin,stdout,stderr
		// 格式3: [2021-10-28 11:41:34][INFO][test.cpp 121]: xxxxxx
		#define LOG(level,format,...) {	\
			static struct tm m;			\
			static time_t t = 0;		\
			t = time(NULL);				\
			local_time(m,t);			\
			char szTime[64] = { 0 };	\
			sprintf(szTime, "%4.4d-%2.2d-%2.2d %2.2d:%2.2d:%2.2d", m.tm_year + 1900, m.tm_mon + 1, m.tm_mday,m.tm_hour, m.tm_min, m.tm_sec);\
			if(0 == strcmp("ERROR", (level)))	\
				fprintf(stdout,"\033[31m[%s][%s][%s %d]: " format"\033[0m", szTime,level,f_name(__FILE__,'/'),__LINE__,##__VA_ARGS__);\
			else if(0 == strcmp("WARN", (level)))\
				fprintf(stdout, "\033[33m[%s][%s][%s %d]: " format"\033[0m", szTime, level, f_name(__FILE__,'/'), __LINE__, ##__VA_ARGS__);\
			else fprintf(stdout, "[%s][%s][%s %d]: " format, szTime, level, f_name(__FILE__,'/'), __LINE__, ##__VA_ARGS__); \
		}
	#endif // USE_LOG_FILE

#endif  // __linux__
	
#endif	// __MY_LOG_H__

//// 两个宏都存在时编译代码A，否则编译代码B
//#if (defined (__WIN32) && defined (_DEBUG))	// #ifdef可以认为是 #if defined 的缩写。
//    printf("WIN32 | Debug");
//#elif
//	printf("Warn\n");
//#else
//    printf("Error\n");
//#endif
//
//
//#if (defined(__GNUC__) && ((__GNUC__ > 4) || (__GNUC__ == 4)))
//#else
//#endif