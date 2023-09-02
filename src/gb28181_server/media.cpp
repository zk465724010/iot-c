 
#include "media.h"
#include "log.h"
#include "common.h"
#include <sstream>

#define USE_DEBUG_OUTPUT
#define BUFF_SIZE  20480

size_t CMedia::m_num = 0;
CMedia::CMedia(int start_port, int count)
{
	srand(time(NULL));
	init(start_port, count);
}

CMedia::~CMedia()
{
	release();
}

int CMedia::init(int start_port, int count)
{
	int nRet = 0;
	nRet = init_port_map(start_port, count);
	//LOG("INFO","port pool %d ~ %d (%d)\n\n", start_port, (start_port + count), count);
	return nRet;
}

void CMedia::release()
{
	m_port_map.clear();
}

int CMedia::create_channel(const char *id, const char *local_ip, int local_port,
	STREAM_CBK cbk/*=NULL*/, STREAM_CBK cbk2/*=NULL*/, void *arg/*=NULL*/, int arg_size/*=0*/)
{
	destroy_channel_auto();	// �Զ�������Ч��ͨ��
	int nRet = -1;
	if ((NULL != id) && (NULL != local_ip) && (local_port > 0)) {
		auto iter = m_channel_map.find(id);
		if (iter != m_channel_map.end()) {
			LOG("ERROR","Channel '%s' exist, cannot create channel [size:%d]\n\n", id, m_channel_map.size());
			return nRet;
		}
		channel_t *chl = NULL;
		try {
			chl = new channel_t();
			chl->stream = new CAVStream();
		}
		catch (const bad_alloc& e) {
			LOG("ERROR","Memory allocation failed [id:%s]\n\n", id);
			return nRet;
		}
		nRet = chl->stream->open(local_ip, local_port, cbk, cbk2, arg, arg_size);
		if (nRet >= 0) {
			m_channel_map.add(id, &chl);
			LOG("INFO","Create channel successful ^-^ [id:%s ip:%s:%d chl_size:%d]\n\n", id, local_ip, local_port, m_channel_map.size());
		}
		else {
			delete chl->stream;
			delete chl;
			LOG("ERROR","Create channel failed [id:%s ip:%s:%d chl_size:%d]\n\n", id, local_ip, local_port, m_channel_map.size());
		}
	}
	else LOG("ERROR","[id:%p ip:%p:%d chl_size:%d]\n\n", id, local_ip, local_port, m_channel_map.size());
	return nRet;
}

int CMedia::create_channel(const char *id, const char *url, STREAM_CBK cbk/*=NULL*/, STREAM_CBK cbk2/*=NULL*/, void *arg/*=NULL*/, int arg_size/*=0*/)
{
	destroy_channel_auto();	// �Զ�������Ч��ͨ��
	int nRet = -1;
	if ((NULL != id) && (NULL != url)) {
		auto iter = m_channel_map.find(id);
		if (iter != m_channel_map.end()) {
			LOG("ERROR","Channel '%s' exist, cannot create channel [size:%d]\n\n", id, m_channel_map.size());
			return nRet;
		}
		channel_t *chl = NULL;
		try {
			chl = new channel_t();
			chl->stream = new CAVStream();
		}
		catch (const bad_alloc& e) {
			LOG("ERROR","Memory allocation failed [id:%s url:%s]\n\n", id,url);
			return nRet;
		}
		nRet = chl->stream->open(url, cbk, cbk2, arg, arg_size);
		if (nRet >= 0) {
			//chl->stream->set_callback(cbk, arg, arg_size);
			m_channel_map.add(id, &chl);
		}
		else {
			delete chl->stream;
			delete chl;
		}
		LOG("INFO","Create channel %s [id:%s url:%s]\n\n", (nRet >= 0) ? "successful ^-^" : "failed", id, url);
	}
	else LOG("ERROR","[id:%p url:%p]\n\n", id, url);
	return nRet;
}

int CMedia::create_channel(const char *id, const char *filename, udp_addr_t *addr, STREAM_CBK cbk/*=NULL*/, void *arg/*=NULL*/, int arg_size/*=0*/)
{
	destroy_channel_auto();	// �Զ�������Ч��ͨ��
	int nRet = -1;
	if ((NULL != id) && (NULL != filename) && (NULL != addr)) {
		auto iter = m_channel_map.find(id);
		if (iter != m_channel_map.end()) {
			LOG("ERROR","Channel '%s' exist, cannot create channel [size:%d]\n\n", id, m_channel_map.size());
			return nRet;
		}
		channel_t *chl = NULL;
		try {
			chl = new channel_t();
			chl->stream = new CAVStream();
		}
		catch (const bad_alloc& e) {
			LOG("ERROR","Memory allocation failed [id:%s filename:%s]\n\n", id, filename);
			return nRet;
		}
		nRet = chl->stream->open_file(filename, addr, cbk, arg, arg_size);
		if (nRet >= 0) {
			m_channel_map.add(id, &chl);
		}
		else {
			delete chl->stream;
			delete chl;
		}
		LOG("INFO","Create channel %s [id:%s filename:%s]\n\n", (nRet >= 0) ? "successful ^-^" : "failed", id, filename);
	}
	else LOG("ERROR","[id:%p filename:%p addr:%p]\n\n", id, filename, addr);
	return nRet;
}

int CMedia::destroy_channel(const char *id, bool is_sync/*=true*/)
{
	int nRet = -1;
	if (NULL != id) {
		auto iter = m_channel_map.find(id);
		if (iter != m_channel_map.end()) {
			channel_t *chl = (channel_t*)iter->second;
			if (NULL != chl->stream) {
				#ifdef USE_UDP_CLIENT
				if (NULL != chl->stream->m_udp) {
					int port = 0;
					((CUdpClient*)chl->stream->m_udp)->GetAddress(NULL, &port);
					free_port(port);
				}
				#endif
				chl->stream->close();
				delete chl->stream;
			}
			delete chl;
			m_channel_map.erase(iter);
			LOG("INFO","Destroy channel '%s' [chl_size:%d]\n\n", id, m_channel_map.size());
			nRet = 0;
		}
		else LOG("ERROR","Channel does not exist '%s'\n\n", id);
	}
	else LOG("ERROR","[id:%p]\n\n", id);
	return nRet;
}

int CMedia::destroy_channels(bool is_sync/*=true*/)
{
	int nRet = 0;
	auto iter = m_channel_map.begin();
	for (; iter != m_channel_map.end(); ) {
		channel_t *chl = (channel_t*)iter->second;
		if (NULL != chl->stream) {
			#ifdef USE_UDP_CLIENT
			if(NULL != chl->stream->m_udp) {
				int port = 0;
				((CUdpClient*)chl->stream->m_udp)->GetAddress(NULL, &port);
				free_port(port);
			}
			#endif
			chl->stream->close();
			delete chl->stream;
		}
		delete chl;
		iter = m_channel_map.erase(iter);
		++nRet;
	}
	return nRet;
}

int CMedia::destroy_channel_auto(bool is_sync/*=true*/)
{
	int nRet = -1;
	auto iter = m_channel_map.begin();
	for (iter; iter != m_channel_map.end(); ) {
		channel_t *chl = (channel_t*)iter->second;
		if (TS_STOP == chl->stream->status()) {
			#ifdef USE_UDP_CLIENT
			if (NULL != chl->stream->m_udp) {
				int port = 0;
				((CUdpClient*)chl->stream->m_udp)->GetAddress(NULL, &port);
				free_port(port);
			}
			#endif
			chl->stream->close();
			delete chl->stream;
			delete chl;
			char id[64] = { 0 }; // ����
			strcpy(id, iter->first.c_str());// ����
			iter = m_channel_map.erase(iter);
			LOG("INFO","Destroy channel '%s' [chl_size:%d]\n\n", id, m_channel_map.size());
			nRet = 0;
		}
		else ++iter;
	}
	return nRet;
}

const channel_t* CMedia::get_channel(const char *id)
{
	if (NULL != id) {
		auto iter = m_channel_map.find(id);
		if (iter != m_channel_map.end()) {
			channel_t *chl = (channel_t*)iter->second;
			return chl;
		}
		else LOG("WARN","Channel does not exist [id:%s]\n\n", id);
	}
	else LOG("ERROR","id=%p\n\n", id);
	return NULL;
}

#if 1
void CMedia::join_channel(const char *id)
{
	if (NULL != id) {
		auto iter = m_channel_map.find(id);
		if (iter != m_channel_map.end()) {
			channel_t *chl = (channel_t*)iter->second;
			if ((NULL != chl) && (NULL != chl->stream)) {
				LOG("INFO","Join channel '%s'\n\n", id);
				chl->stream->join();
			}
		}
		else LOG("WARN","Channel does not exist [id:%s]\n\n", id);
	}
	else LOG("ERROR","id=%p\n\n", id);
}

void CMedia::detach_channel(const char *id)
{
	if (NULL != id) {
		auto iter = m_channel_map.find(id);
		if (iter != m_channel_map.end()) {
			channel_t *chl = (channel_t*)iter->second;
			if ((NULL != chl) && (NULL != chl->stream)) {
				LOG("INFO","Detach channel '%s'\n\n", id);
				chl->stream->detach();
			}
		}
		else LOG("WARN","Channel does not exist [id:%s]\n\n", id);
	}
	else LOG("ERROR","id=%p\n\n", id);
}
#endif

int CMedia::init_port_map(int start_port, int count)
{
	//srand(time(NULL));// ��ʼ�����������
	if ((start_port > 0) && (count > 0)) {
		int max = start_port + count;
		for (int i = start_port; i < max; ++i) {
			m_port_map.insert(map<int, char>::value_type(i, 0));
		}
		return 0;
	}
	return -1;
}

int CMedia::alloc_port()
{
#if 1
	//srand(time(NULL));
	int random = rand() % m_port_map.size();
	auto iter = m_port_map.begin();
	for (int i = 0; i < random; i++)
		iter++;
	if (iter == m_port_map.end())
		iter = m_port_map.begin();
#else
	auto iter = m_port_map.begin();
#endif
	for (; iter != m_port_map.end(); ++iter) {
		if (0 == iter->second) {   // �˿�ʹ��״̬[0:δʹ��,1:��ʹ��]
			iter->second = 1;
			return iter->first;
		}
	}
	#ifdef USE_DEBUG_OUTPUT
	LOG("ERROR","�Զ�����˿�ʧ��,����ԭ��:�˿���ʹ����!\n\n\n\n\n\n");
	#endif
	return -1;
}

void CMedia::free_port(int port)
{
	auto iter = m_port_map.find(port);
	if (iter != m_port_map.end()) {
		iter->second = 0;       // �˿�ʹ��״̬[0:δʹ��,1:��ʹ��]
		#ifdef USE_DEBUG_OUTPUT
		LOG("INFO","Free port '%d'\n\n", port);
		#endif
	}
	#ifdef USE_DEBUG_OUTPUT
	else LOG("WARN","Port does not exist '%d'\n\n", port);
	#endif
}
string CMedia::generate_random_string(int length)
{
	//static char str[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_+=<>,./?~`!@#$%^&*(){}[]|\\";
	static char str[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789!@#$%&*+=?";
	static int len = strlen(str);
	//static bool b = true;
	//if (b) {
	//	b = false;
	//	srand(time(NULL));				// ��ʼ�����������
	//}
	std::stringstream str_random;		// #include <sstream>
	for (int i = 0; i < length; i++)
		str_random << str[rand() % len];
	str_random << m_num++;
	return str_random.str();
}
int CMedia::timestamp_to_string(time_t tick, char *buf, int buf_size, char ch/*='-'*/)
{
	#define buff_size 64
	char buff[buff_size] = { 0 };
	//time_t tick = time(NULL);
#if 0
	// localtime��ȡϵͳʱ�䲢�����̰߳�ȫ��(�����ڲ�����ռ�,����ʱ��ָ��)
	struct tm *pTime = localtime(&tick);
	if (ch == '-')
		strftime(buff, buff_size, "%Y-%m-%dT%H:%M:%S", pTime);
	else if (ch == '/')
		strftime(buff, buff_size, "%Y/%m/%d %H:%M:%S", pTime);
	else if (ch == '.')
		strftime(buff, buff_size, "%Y.%m.%d_%H.%M.%S", pTime);
	else strftime(buff, buff_size, "%Y%m%d%H%M%S", pTime);
#else
	#ifdef _WIN32
		// localtime_sҲ��������ȡϵͳʱ��,������windowsƽ̨��,��localtime_rֻ�в���˳��һ��
		struct tm t;
		localtime_s(&t, &tick);
		sprintf(buff, "%4.4d%c%2.2d%c%2.2dT%2.2d%c%2.2d%c%2.2d", t.tm_year + 1900, ch, t.tm_mon + 1, ch, t.tm_mday, t.tm_hour, ch, t.tm_min, ch, t.tm_sec);
	#elif __linux__
		// localtime_rҲ��������ȡϵͳʱ��,������linuxƽ̨��,֧���̰߳�ȫ
		struct tm t;
		localtime_r(&tick, &t);
		sprintf(buff, "%4.4d%c%2.2d%c%2.2dT%2.2d%c%2.2d%c%2.2d", t.tm_year + 1900, ch, t.tm_mon + 1, ch, t.tm_mday, t.tm_hour, ch, t.tm_min, ch, t.tm_sec);
	#endif
#endif
	int len = strlen(buff);
	int min = (buf_size > len) ? len : buf_size;
	if (NULL != buf)
		strncpy(buf, buff, min);
	return len;
}

time_t CMedia::string_to_timestamp(const char *time_str)
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
	// �ַ���תʱ���
	struct tm tm_time;
	strptime("2010-11-15 10:39:30", "%Y-%m-%d %H:%M:%S", &time);
	return mktime(&time);
#endif
	return -1;
}

int CMedia::snapshoot(const char* ip, int port, const char* filename)
{
	CAVStream stream;
	return stream.snapshoot(ip, port, filename);
}
