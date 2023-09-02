
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


/*
注意事项:

  一.std::thread在以下几种情况下是不可join的:
	1.std::thread默认构造的thread对象,也就是没有指定线程函数的std::thread对象是不可join的.
	2.该std::thread对象被std::move操作.
	3.该std::thread对象已经执行过std::thread::join()方法或者std::thread::detach()方法,此时std::thread对象是不可join的.

  二.使用std::thread::join()/std::thread::detach()方法时需要注意的点:
	1.在执行std::thread::join()/std::thread::detach()方法之前最好先判断该std::thread对象是否可join
	  在调用join之前判断joinable,只有在thread对象是可join的情况下再进行join或者detach操作。
	2.若一个thread对象指定了线程函数时,当程序退出时,若没有调佣join或detach方法,std::thread对象会在析构的时候中断程序,
	  若程序发生异常,也必须在异常处理中调用std::thread对象的join或者detach方法.
	3.若thread对象调用了detach()方法时,当程序退出时


////////////////////////////////////////////////////////////////////////////
// 线程锁:
<mutex>头文件介绍
	Mutex 系列类(四种)
		std::mutex: 最基本的Mutex类,不能递归使用。
		std::time_mutex: 定时互斥锁,不能递归使用,std::time_mutex比std::mutex多了两个成员函数:try_lock_for(),try_lock_until()。
		std::recursive_mutex: 递归互斥锁(可以被同一个线程多次加锁,以获得对互斥锁对象的多层所有权)
		std::recursive_timed_mutex: 带定时的递归互斥锁。

	Lock 类（两种）
		std::lock_guard，与 Mutex RAII 相关，方便线程对互斥量上锁。
		std::unique_lock，与 Mutex RAII 相关，方便线程对互斥量上锁，但提供了更好的上锁和解锁控制。

	其他类型
		std::once_flag
		std::adopt_lock_t
		std::defer_lock_t
		std::try_to_lock_t
	函数
		std::try_lock，尝试同时对多个互斥量上锁。
		std::lock，可以同时对多个互斥量上锁。
		std::call_once，如果多个线程需要同时调用某个函数，call_once 可以保证多个线程对该函数只调
		用一次。
	std::mutex 介绍
		下面以 std::mutex 为例介绍 C++11 中的互斥量用法。
		std::mutex 是C++11 中最基本的互斥量，std::mutex 对象提供了独占所有权的特性,即不支持递
		归地对std::mutex 对象上锁，而 std::recursive_lock 则可以递归地对互斥量对象上锁。

	std::mutex 的成员函数
		1.构造函数，std::mutex不允许拷贝构造，也不允许move拷贝，最初产生的mutex对象是处于unlocked状态的。
		2.lock()，调用线程将锁住该互斥量。线程调用该函数会发生下面 3 种情况：
			(1).如果该互斥量当前没有被锁住，则调用线程将该互斥量锁住，直到调用 unlock之前，该线程一直拥有该锁。
			(2).如果当前互斥量被其他线程锁住，则当前的调用线程被阻塞住。
			(3).如果当前互斥量被当前调用线程锁住，则会产生死锁(deadlock)。
		3.unlock()，解锁，释放对互斥量的所有权。
		4.try_lock()，尝试锁住互斥量，如果互斥量被其他线程占有，则当前线程也不会被阻塞。线程调用该函
		  数也会出现下面 3 种情况，
			(1).如果当前互斥量没有被其他线程占有，则该线程锁住互斥量，直到该线程调用unlock释放互斥量。
			(2).如果当前互斥量被其他线程锁住，则当前调用线程返回 false，而并不会被阻塞掉。
			(3).如果当前互斥量被当前调用线程锁住，则会产生死锁(deadlock)。

	示例:
		#include<mutex>
		std::mutex mut;
		mut.lock();
		std::cout<<"thread_"<<num<<"...temp="<<temp<<std::endl;
		mut.unlock();

		std::mutex
		std::timed_mutex
		std::recursive_mutex
		std::recursive_timed_mutex
	------------------------------------------
		std::mutex g_mtx;
		void func()
		{
			// std::lock_guard,其会在构造的时候提供已锁的互斥量,并在析构的时候进行解锁,从而保证自动管理
			std::lock_guard<std::mutex> auto_lock(g_mtx);
			printf("###\n");
		}
-------------------------------------------


///////////////////////////////////////////////////////////////////////////////

// 调用示例:

#include "Thread11.h"

CThread11 g_thd;
void STDCALL ThreadProc(STATE &state, PAUSE &p, void *pUserData);

int main()
{
	printf("Hello,thread\n\n");

	g_thd.Run(ThreadProc);
	g_thd.Join();

	return 0;
}

void STDCALL ThreadProc(STATE &state, PAUSE &p, void *pUserData)
{
	int n = 0;
	while (TS_STOP != state)
	{
		if (TS_PAUSE == state)
		{
			p.wait();
		}
		printf("# %d\n", n++);
		#ifdef _WIN32
		Sleep(500);
		#elif __linux__
		usleep(500000);
		#endif
	}
}


*/
