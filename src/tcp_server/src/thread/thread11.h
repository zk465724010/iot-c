
#ifndef __THREAD11_H__
#define __THREAD11_H__

#include <thread>
#include <condition_variable>
using namespace std;

//#define USE_LOCK			// 是否使用线程锁(支持多线程)
#define USE_EXTEND_PAUSE	// 是否使用扩展功能'暂停'

#ifdef _WIN32
    #ifdef _DEBUG
        #define USE_DEBUG_OUTPUT
    #endif
    #define STDCALL __stdcall
#elif __linux__
    //#define USE_DEBUG_OUTPUT
    #define STDCALL __attribute__((__stdcall__))
#endif

typedef enum THRERAD_STATE
{
	TS_RUNING,	// 运行中
	TS_STOP,	// 停止
	TS_PAUSE,	// 暂停
}STATE;

#ifdef USE_EXTEND_PAUSE
	#include<mutex>
	struct PAUSE {
	public:
		PAUSE(unique_lock<mutex> &lck, condition_variable &c) : lock(lck),cv(c){ }
		void wait() { cv.wait(lock); }
	private:
		unique_lock<mutex> &lock;
		condition_variable &cv;
	};
	typedef void(STDCALL *USER_CBK)(STATE &state, PAUSE &p, void *pUserData);
#else
	typedef void(STDCALL *USER_CBK)(STATE &state, void *pUserData);
#endif

class CThread11
{
public:
	CThread11(void);
	~CThread11(void);

	int Run(USER_CBK pCallback, void *pUserData=NULL, int nUserDataSize=0);
 	void Stop(bool bSync=true);
	void Join();
	void Detach();
	void Swap(CThread11& other);
	void Move(CThread11& other);

 	unsigned GetId();
 	STATE GetState();
	void SetCallback(USER_CBK pCallback, void *pUserData=NULL, int nUserDataSize = 0);
	
	unsigned int HardwareConcurrency();
	#ifdef _WIN32
	void* NativeHandle();
	#elif __linux__
	#endif

#ifdef USE_EXTEND_PAUSE
	public:
		void Pause();
		void Continue();
	private:
		std::mutex m_pause_mtx;
		std::condition_variable m_pause_cv;	// 条件变量,用于实现暂停功能
#endif

#ifdef USE_LOCK
private:
	std::mutex m_mutex;
	//std::condition_variable m_cv;	// 条件变量
#endif

private:
	int Init();
	void Release();
	void ThreadProc();

private:
	std::thread	m_thread;
	USER_CBK	m_pCallback = NULL;
	void*		m_pUserData = NULL;
	int			m_nUserDataSize = 0;
	STATE		m_State = TS_STOP;
	bool		m_bJoinable = true;
};
#endif