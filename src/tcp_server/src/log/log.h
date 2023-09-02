
#ifndef __MY_LOG_H__
#define __MY_LOG_H__

#include <stdio.h>
#include <time.h>

#define USE_LOG_FILE 1

#ifdef _WIN32
	#define short_file_name(x) strrchr(x,'\\') ? strrchr(x,'\\')+1 : x
	#define local_time(m,t) localtime_s(&m, &t)
#elif __linux__
	#include <string.h>
	#include<sys/time.h>	// struct timeval, localtime()
	#define short_file_name(x) strrchr(x,'/') ? strrchr(x,'/')+1 : x
	#define local_time(m,t) localtime_r(&t, &m)
#endif

#ifdef _WIN32
	#define	BLACK		0x0000				//黑色		
	#define	BLUE		0x0001				//蓝色		0x0009: 蓝色[高亮]
	#define	GREEN		0x0002				//绿色		0x000A: 绿色[高亮]
	#define	AZURE		0x0003				//天蓝色	0x000B: 天蓝色[高亮]
	#define	RED			0x0004				//红色		0x000C: 红色[高亮]
	#define	PURPLE		0x0005				//紫色		0x000D: 紫色[高亮]
	#define	YELLOW		0x0006				//黄色		0x000E: 黄色[高亮]
	#define	WHITE		0x0007				//白色		0x000F: 白色[高亮]
	#define	GRAY		0x0008				//灰色
	#define	BLUE_G		0x0009				//蓝色[高亮]
	#define	GREEN_G		0x000A				//绿色[高亮]
	#define	AZURE_G		0x000B				//天蓝色[高亮]
	#define	RED_G		0x000C				//红色[高亮]
	#define	PURPLE_G	0x000D				//紫色[高亮]
	#define	YELLOW_G	0x000E				//黄色[高亮]
	#define	WHITE_G		0x000F				//白色[高亮]
	//#define DEBUG(format, ...) {										\
	//	static HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);		\
	//	SetConsoleTextAttribute(hConsole, YELLOW_G);					\
	//	fprintf(stderr, "[%s() %d]: ", __FUNCTION__, __LINE__);			\
	//	SetConsoleTextAttribute(hConsole, RED_G);						\
	//	fprintf(stderr,format, ##__VA_ARGS__);							\
	//	SetConsoleTextAttribute(hConsole, WHITE | (BLACK << 4));		\
	//}
	
	
#elif __linux__

	// 格式: [INFO][test.cpp main 121]: xxxxxx
	//#define LOG2(level,format,...)  printf("[%s][%s %s %d]: " format, level,short_file_name(__FILE__), __FUNCTION__, __LINE__, ##__VA_ARGS__)

#endif  // _WIN32
	


#ifdef USE_LOG_FILE
	extern FILE* g_log_file;
	//#define LOG(level,format,...)  (printf("[%s][%s %s %d]: "format, level,short_file_name(__FILE__), __FUNCTION__, __LINE__, ##__VA_ARGS__),fprintf(g_log_file,"[%s][%s %s %d]: "format, level,short_file_name(__FILE__), __FUNCTION__, __LINE__, ##__VA_ARGS__),fflush(g_log_file))
	//#define LOG_FATAL(format,...)  (printf("[FATAL][%s %s %d]: "format,short_file_name(__FILE__), __FUNCTION__, __LINE__, ##__VA_ARGS__),fprintf(g_log_file,"[FATAL][%s %s %d]: "format,short_file_name(__FILE__), __FUNCTION__, __LINE__, ##__VA_ARGS__),fflush(g_log_file))
	//#define LOG_ERROR(format,...)  (printf("[ERROR][%s %s %d]: "format,short_file_name(__FILE__), __FUNCTION__, __LINE__, ##__VA_ARGS__),fprintf(g_log_file,"[ERROR][%s %s %d]: "format,short_file_name(__FILE__), __FUNCTION__, __LINE__, ##__VA_ARGS__),fflush(g_log_file))
	//#define LOG_WARN(format,...)  (printf("[WARN][%s %s %d]: "format,short_file_name(__FILE__), __FUNCTION__, __LINE__, ##__VA_ARGS__),fprintf(g_log_file,"[WARN][%s %s %d]: "format,short_file_name(__FILE__), __FUNCTION__, __LINE__, ##__VA_ARGS__),fflush(g_log_file))
	//#define LOG_INFO(format,...)  (printf("[INFO][%s %s %d]: "format,short_file_name(__FILE__), __FUNCTION__, __LINE__, ##__VA_ARGS__),fprintf(g_log_file,"[INFO][%s %s %d]: "format,short_file_name(__FILE__), __FUNCTION__, __LINE__, ##__VA_ARGS__),fflush(g_log_file))
	//#define LOG_DEBUG(format,...)  (printf("[DEBUG][%s %s %d]: "format,short_file_name(__FILE__), __FUNCTION__, __LINE__, ##__VA_ARGS__),fprintf(g_log_file,"[DEBUG][%s %s %d]: "format,short_file_name(__FILE__), __FUNCTION__, __LINE__, ##__VA_ARGS__),fflush(g_log_file))
	//#define LOG_TRACE(format,...)  (printf("[TRACE][%s %s %d]: "format,short_file_name(__FILE__), __FUNCTION__, __LINE__, ##__VA_ARGS__),fprintf(g_log_file,"[TRACE][%s %s %d]: "format,short_file_name(__FILE__), __FUNCTION__, __LINE__, ##__VA_ARGS__),fflush(g_log_file))
	// 格式: [2021-10-28 11:41:34][INFO][test.cpp 121]: xxxxxx
	#define LOG(level,format,...) {	\
		struct tm m;				\
		time_t t = time(NULL);		\
		local_time(m,t);			\
		char szTime[64] = { 0 };	\
		sprintf(szTime, "%4.4d-%2.2d-%2.2d %2.2d:%2.2d:%2.2d", m.tm_year + 1900, m.tm_mon + 1, m.tm_mday,m.tm_hour, m.tm_min, m.tm_sec), \
		printf("[%s][%s][%s %d]: " format, szTime,level,short_file_name(__FILE__),__LINE__,##__VA_ARGS__);\
		if(NULL != g_log_file) {	\
			fprintf(g_log_file,"[%s][%s][%s %d]: " format, szTime,level,short_file_name(__FILE__),__LINE__,##__VA_ARGS__),\
			fflush(g_log_file);		\
		}\
	}
#endif // USE_LOG_FILE

	// 格式2: [INFO][test.cpp main.cpp 121]: xxxxxx
	#define LOG2(level,format,...)  printf("[%s][%s %d]: " format, level,short_file_name(__FILE__), __LINE__,##__VA_ARGS__)
	// 格式3: [2021-10-28 11:41:34][INFO][test.cpp 121]: xxxxxx
	#define LOG3(level,format,...) {	\
			struct tm m;				\
			time_t t = time(NULL);		\
			local_time(m,t);			\
			char szTime[64] = { 0 };	\
			sprintf(szTime, "%4.4d-%2.2d-%2.2d %2.2d:%2.2d:%2.2d", m.tm_year + 1900, m.tm_mon + 1, m.tm_mday,m.tm_hour, m.tm_min, m.tm_sec), \
			printf("[%s][%s][%s %d]: " format, szTime,level,short_file_name(__FILE__),__LINE__,##__VA_ARGS__);\
		}


#endif	// __MY_DEBUG_H__
