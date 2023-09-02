#if 1

#include <stdio.h>
#include <windows.h>
#include "media.h"


const char *g_id = "1000";
const char *g_url = "C:/Work/Projects/gb28181_server/build/bin/123456.ps";
const char *g_local_ip = "192.168.41.131";

FILE *g_fp = NULL;
FILE *g_mp4 = NULL;

int STDCALL on_stream(void *pData, int nSize, void *pUserData);
int STDCALL on_stream2(void *pData, int nSize, void *pUserData);

int main()
{
	char filename[128] = {0};
	sprintf(filename, "C:/Work/Projects/gb28181_server/build/bin/%lld.ps", time(NULL));
	g_fp = fopen(filename, "wb");
	sprintf(filename, "C:/Work/Projects/gb28181_server/build/bin/%lld.mp4\0", time(NULL));
	g_mp4 = fopen(filename, "wb");

	CMedia media;

#if 1
	int nRet = media.create_channel(g_url, on_stream, on_stream2, NULL, 0);
#else
	int port = media.alloc_port();
	int nRet = media.create_channel(g_id, g_local_ip, port, on_stream, on_stream2, NULL, 0);
#endif
	if (nRet >= 0)
	{
		getchar();

		media.destroy_channel(g_id);
	}

	if (NULL != g_fp) {
		fclose(g_fp);
		g_fp = NULL;
	}
	if (NULL != g_mp4) {
		fclose(g_mp4);
		g_mp4 = NULL;
	}

	system("pause");


	return 0;
}

int STDCALL on_stream(void *pData, int nSize, void *pUserData)
{
	static int n = 0;
	//printf("%d  data:%p, size:%d\n", n++, pData, nSize);
	if (NULL != g_fp)
		fwrite(pData, 1, nSize, g_fp);

	return 0;
}

int STDCALL on_stream2(void *pData, int nSize, void *pUserData)
{
	static int n = 0;
	//printf("%d  data:%p, size:%d\n", n++, pData, nSize);
	if (NULL != g_mp4)
		fwrite(pData, 1, nSize, g_mp4);

	return 0;
}



#else


#include <stdio.h>
#include "av_stream.h"
#include "windows.h"

int STDCALL on_stream(void *pData, int nSize, void *pUserData);
int write_buffer(void *arg, uint8_t *buf, int buf_size);


int main()
{

	CAVStream stream;

	stream.set_callback(on_stream);

	int hhh = 100;
	int nRet = stream.open("C:/Work/Projects/gb28181_server/build/bin/123456.h264", NULL, write_buffer, NULL, &hhh);
	printf("stream.open = %d\n\n", nRet);
	if (0 == nRet)
	{

		getchar();
	}

	stream.close();

	if (NULL != g_fp) {
		fclose(g_fp);
		g_fp = NULL;
	}
	if (NULL != g_mp4) {
		fclose(g_mp4);
		g_mp4 = NULL;
	}
	system("pause");
	return 0;
}

int STDCALL on_stream(void *pData, int nSize, void *pUserData)
{
	static int n = 0;
	//printf("%d  data:%p, size:%d\n", n++, pData, nSize);
	if (NULL != g_fp)
		fwrite(pData, 1, nSize, g_fp);

	return 0;
}

int write_buffer(void *arg, uint8_t *buf, int buf_size)
{
	static int n = 0;
	printf("mp4:  %d  data:%p, size:%d\n", n++, buf, buf_size);
	if (NULL != g_mp4)
		fwrite(buf, 1, buf_size, g_mp4);
	return buf_size;
}

#endif