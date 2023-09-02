
#include "rtmp_media.h"
#include "log.h"
#include <sstream>

CRtmpMedia::CRtmpMedia()
{
}

CRtmpMedia::~CRtmpMedia()
{
}

int CRtmpMedia::create_channel(const char *id, stream_param_t *param)
{
	int nRet = -1;
	if ((NULL != id) && (NULL != param)) {
		do {
			rtmp_chl_t *chl = NULL;
			try {
				chl = new rtmp_chl_t();
				chl->stream = new CRtmpStream();
			}
			catch (const bad_alloc& e) {
				LOG("ERROR", "Allocation memory failed [chl_size:%d]\n", m_chl_map.size());
				break;
			}
			nRet = chl->stream->open(param);
			//nRet = chl->stream->open2(param);
			if (nRet >= 0) {
				m_chl_map.add(id, &chl);
				LOG("INFO","Create channel '%s' succeed [chl_size:%d] ^-^\n", id, m_chl_map.size());
			}
			else {
				LOG("ERROR", "Create channel '%s' failed [chl_size:%d]\n", id, m_chl_map.size());
				delete chl->stream;
				delete chl;
			}
		} while (false);
	}
	else LOG("ERROR", "Parameter error id=%p param=%p\n", id, param);
	return nRet;
}
int CRtmpMedia::destroy_channel(const char *id, bool is_sync/*=true*/)
{
	int nRet = -1;
	if (NULL != id) {
		auto iter = m_chl_map.find(id);
		if (iter != m_chl_map.end()) {
			iter->second->stream->close();
			delete iter->second->stream;
			delete iter->second;
			m_chl_map.erase(iter);
			LOG("INFO", "Destroy channel '%s' succeed [chl_size:%d]\n", id, m_chl_map.size());
			nRet = 0;
		}
		else LOG("ERROR", "Destroy channel failed, channel '%s' not exist [chl_size:%d]\n", id, m_chl_map.size());
	}
	else LOG("ERROR", "Parameter error id=%p\n", id);
	return nRet;
}
int CRtmpMedia::destroy_channels(bool is_sync/*=true*/)
{
	int count = 0;
	auto iter = m_chl_map.begin();
	for (; iter != m_chl_map.end();) {
		iter->second->stream->close();
		delete iter->second->stream;
		delete iter->second;
		string id = iter->first;
		m_chl_map.erase(iter);
		LOG("INFO", "Destroy channel '%s' succeed [chl_size:%d]\n", id.c_str(), m_chl_map.size());
		count++;
	}
	return count;
}
const rtmp_chl_t* CRtmpMedia::get_channel(const char *id)
{
	if (NULL != id) {
		auto iter = m_chl_map.find(id);
		if (iter != m_chl_map.end())
			return iter->second;
		else 
			LOG("ERROR", "Get channel failed, channel '%s' not exist [chl_size:%d]\n", id, m_chl_map.size());
	}
	else LOG("ERROR", "Parameter error id=%p\n", id);
	return NULL;
}

std::string CRtmpMedia::random_string(int length)
{
	static char str[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-=[],.~@#$%^&*)(_+><?/|\{}";
	static int len = strlen(str);
	static bool init = true;
	if (init) {
		init = false;
		srand(time(NULL));				// 初始化随机数种子
	}
	std::stringstream str_random;		// #include <sstream>
	for (int i = 0; i < length; i++) {
		str_random << str[rand() % len];
	}
	return str_random.str();
}



#ifdef __THREAD11_H__
void CRtmpMedia::pause_channel(const char *id)
{
	if (NULL != id) {
		auto iter = m_chl_map.find(id);
		if (iter != m_chl_map.end()) {
			iter->second->stream->pause();
		}
		else LOG("ERROR", "Pause channel failed, channel '%s' not exist [chl_size:%d]\n", id, m_chl_map.size());
	}
	else LOG("ERROR", "Parameter error id=%p\n", id);
}
void CRtmpMedia::continue_channel(const char *id)
{
	if (NULL != id) {
		auto iter = m_chl_map.find(id);
		if (iter != m_chl_map.end()) {
			iter->second->stream->continu();
		}
		else LOG("ERROR", "Continue channel failed, channel '%s' not exist [chl_size:%d]\n", id, m_chl_map.size());
	}
	else LOG("ERROR", "Parameter error id=%p\n", id);
}
void CRtmpMedia::join_channel(const char *id)
{
	if (NULL != id) {
		auto iter = m_chl_map.find(id);
		if (iter != m_chl_map.end()) {
			iter->second->stream->join();
		}
		else LOG("ERROR", "Join channel failed, channel '%s' not exist [chl_size:%d]\n", id, m_chl_map.size());
	}
	else LOG("ERROR", "Parameter error id=%p\n", id);
}
#endif