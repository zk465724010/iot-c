#include <assert.h>
#include "thread11.h"
#include "log.h"

#ifdef _WIN32
#include <Windows.h>	// Sleep()
#elif __linux__
#include <unistd.h>		// usleep()
#endif

CThread11::CThread11(void)
{
	Init();
}

CThread11::~CThread11(void)
{
	Release();
}

int CThread11::Init()
{
	return 0;
}

void CThread11::Release()
{
	m_pCallback = NULL;
	if ((NULL != m_pUserData) && (m_nUserDataSize > 0))
	{
		free(m_pUserData);
	}
	m_pUserData = NULL;
	m_nUserDataSize = 0;
}

int CThread11::Run(USER_CBK pCallback, void *pUserData/*=NULL*/, int nUserDataSize/*=0*/)
{
	//assert(NULL != pCallback);
	int nRet = -1;
	if(TS_STOP == m_State)
	{
		#ifdef USE_LOCK
		std::lock_guard<std::mutex> auto_lock(m_mutex);
		#endif
		m_pCallback	= pCallback;
		if ((NULL != m_pUserData) && (m_nUserDataSize > 0)) {
			free(m_pUserData);
			m_pUserData = NULL;
			m_nUserDataSize = 0;
		}
		m_pUserData = pUserData;
		if ((NULL != pUserData) && (nUserDataSize > 0)) {
			m_pUserData = malloc(nUserDataSize+1);
			if(NULL != m_pUserData) {
				memset(m_pUserData, 0, nUserDataSize + 1);
				memcpy(m_pUserData, pUserData, nUserDataSize);
				m_nUserDataSize = nUserDataSize;
			}
		}
		//m_pThread = new std::thread(&CThread11::ThreadProc, this, m_pUserParam1, m_pUserParam2, m_pUserParam3);
		//m_pThread = new std::thread(std::bind(&CThread11::ThreadProc, this, m_pUserParam1, m_pUserParam2, m_pUserParam3));	
		m_bJoinable = true;
		m_State = TS_RUNING;
		m_thread = std::thread(&CThread11::ThreadProc, this);
		nRet = 0;
		#ifdef _WIN32
		Sleep(50);	// 休眠一段时间是为了等待线程函数运行起来
		#elif __linux__
		usleep(50000);
		#endif
	}
	#ifdef USE_EXTEND_PAUSE
	else if (TS_PAUSE == m_State) {
		Continue();
	}
	#endif
	else LOG3("WARN", "Thread is already running.\n\n");
	return nRet;
}

void CThread11::Stop(bool bSync/*=true*/)
{
#ifdef USE_LOCK
	std::lock_guard<std::mutex> auto_lock(m_mutex);
#endif
#ifdef USE_EXTEND_PAUSE
	if (TS_PAUSE == m_State)
		m_pause_cv.notify_all();
#endif
	m_State = TS_STOP;
	//if (bSync && m_thread.joinable()) {	// 检测线程是否有效
	if (bSync && m_thread.joinable() && m_bJoinable) {
		m_bJoinable = false;
		m_thread.join();
	}
	else LOG3("WARN","bSync=%s,joinable()=%s,joinable=%s\n\n", bSync ? "true" : "false", m_thread.joinable() ? "true" : "false", m_bJoinable ? "true" : "false");
}

void CThread11::Join()
{
	//#ifdef USE_EXTEND_PAUSE
	//if (TS_PAUSE == m_State)
	//	m_pause_cv.notify_all();
	//#endif
	//if (m_thread.joinable()) { // 检测线程是否有效
	if (m_thread.joinable() && m_bJoinable) {
		m_bJoinable = false;
		m_thread.join();
	}
	else LOG3("WARN", "joinable()=%s,joinable=%s, cannot join.\n\n", m_thread.joinable()?"true":"false", m_bJoinable ? "true" : "false");
}

void CThread11::Detach()
{
	//if (m_thread.joinable()) { // 检测线程是否有效
	if (m_thread.joinable() && m_bJoinable) {
		m_bJoinable = false;
		m_thread.detach();
	}
	else LOG3("WARN", "joinable()=%s,joinable=%s, cannot detach.\n\n", m_thread.joinable() ? "true" : "false", m_bJoinable ? "true" : "false");
}

unsigned int CThread11::HardwareConcurrency()
{
	return m_thread.hardware_concurrency();
}

void CThread11::Swap(CThread11& other)
{
	m_thread.swap(other.m_thread);
}

void CThread11::Move(CThread11& other)
{
	m_thread = std::move(other.m_thread);
}

#ifdef _WIN32
void* CThread11::NativeHandle()
{
	return m_thread.native_handle();
}
#endif

//void CThread11::ThreadProc(void *pParam1, void *pParam2, void *pParam3)
void CThread11::ThreadProc()
{
	//LOG3("INFO", "Thread running ...\n\n");
	if( NULL != m_pCallback )								
	{
		//m_State = TS_RUNING;
		#ifdef USE_EXTEND_PAUSE
			unique_lock<mutex> lock(m_pause_mtx);
			PAUSE p(lock , m_pause_cv);
			m_pCallback(m_State, p, m_pUserData);
		#else
			m_pCallback(m_State, m_pUserData);
		#endif
		m_State = TS_STOP;
	}
	LOG3("INFO", "Thread exit.\n\n");
}

void CThread11::SetCallback(USER_CBK pCallback, void *pUserData/*=NULL*/, int nUserDataSize/*=0*/)
{
	m_pCallback = pCallback;
	if ((NULL != m_pUserData) && (m_nUserDataSize > 0)) {
		free(m_pUserData);
		m_pUserData = NULL;
		m_nUserDataSize = 0;
	}
	m_pUserData = pUserData;
	if ((NULL != pUserData) && (nUserDataSize > 0)) {
		m_pUserData = malloc(nUserDataSize + 1);
		if (NULL != m_pUserData) {
			memset(m_pUserData, 0, nUserDataSize + 1);
			memcpy(m_pUserData, pUserData, nUserDataSize);
			m_nUserDataSize = nUserDataSize;
		}
	}
}

unsigned CThread11::GetId()
{
	// std::this_thread::get_id();
	std::thread::id tid = m_thread.get_id();
	// 类只有一个数据成员且没有虚函数表,利用C++指针获取private数据成员
	//int id = *(int*)(char*)&tid;
	return *((unsigned*)&tid);
}

STATE CThread11::GetState()
{
	return m_State;
}

#ifdef USE_EXTEND_PAUSE
void CThread11::Pause()
{
	if (TS_RUNING == m_State)
	{
		m_State = TS_PAUSE;
	}
}
void CThread11::Continue()
{
	if (TS_PAUSE == m_State)
	{
		m_State = TS_RUNING;
		m_pause_cv.notify_all();
	}
}
#endif