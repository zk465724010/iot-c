
#ifndef __MY_MEDIA_H__
#define __MY_MEDIA_H__

#include "udp_client.h"
#include "cmap.h"
#include "av_stream.h"

typedef struct channel_t
{
	CAVStream *stream = NULL;
}channel_t;

class CMedia
{
public:
	CMedia(int start_port, int count);
	~CMedia();
	// ����ͨ��(�㲥)
	int create_channel(const char *id, const char *local_ip, int local_port, \
		STREAM_CBK cbk = NULL, STREAM_CBK cbk2 = NULL, void *arg = NULL, int arg_size = 0);

	// ����ͨ��(�ط�)
	int create_channel(const char *id, const char *url, STREAM_CBK cbk = NULL, STREAM_CBK cbk2 = NULL, void *arg = NULL, int arg_size = 0);
	int create_channel(const char *id, const char *filename, udp_addr_t *addr, STREAM_CBK cbk = NULL, void *arg = NULL, int arg_size = 0);

	// ����ͨ��
	int destroy_channel(const char *id, bool is_sync = true);
	int destroy_channel_auto(bool is_sync = true);
	int destroy_channels(bool is_sync=true);

	void join_channel(const char *id);
	void detach_channel(const char *id);

	const channel_t* get_channel(const char *id);

	string generate_random_string(int length);
	int timestamp_to_string(time_t tick, char *buf, int buf_size, char ch='-');
	time_t string_to_timestamp(const char *time_str);

	int alloc_port();
	void free_port(int port);

	// ����
	int snapshoot(const char* ip, int port, const char* filename);


private:
	int init(int start_port, int count);
	void release();
	// �˿ڸ��� 
	int init_port_map(int start_port, int count);
	map<int, char> m_port_map;	// �˿ڼ��� (keyΪ�˿ں�,valueΪʹ��״̬[0:δʹ��,1:��ʹ��])

	static size_t m_num;

private:
	CMap<string, channel_t*>	m_channel_map;
};


#endif
