
#ifndef __THREAD11_H__
#define __THREAD11_H__

#include <thread>
#include <condition_variable>
using namespace std;

#define USE_EXTEND_PAUSE	// ʹ����չ����'��ͣ'

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
	TS_RUNING,	// ����
	TS_STOP,	// ֹͣ
	TS_PAUSE,	// ��ͣ
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
		std::mutex m_mutex;
		std::condition_variable m_cv;	// ��������,����ʵ����ͣ����
	#endif

private:
	int Init();
	void Release();
	void ThreadProc();
	// std::mutex m_mutex;

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
ע������:

  һ.std::thread�����¼���������ǲ���join��:
    1.std::threadĬ�Ϲ����thread����,Ҳ����û��ָ���̺߳�����std::thread�����ǲ���join��.
    2.��std::thread����std::move����.
    3.��std::thread�����Ѿ�ִ�й�std::thread::join()��������std::thread::detach()����,��ʱstd::thread�����ǲ���join��.
  
  ��.ʹ��std::thread::join()/std::thread::detach()����ʱ��Ҫע��ĵ�:
    1.��ִ��std::thread::join()/std::thread::detach()����֮ǰ������жϸ�std::thread�����Ƿ��join
      �ڵ���join֮ǰ�ж�joinable,ֻ����thread�����ǿ�join��������ٽ���join����detach������
	2.��һ��thread����ָ�����̺߳���ʱ,�������˳�ʱ,��û�е�Ӷjoin��detach����,std::thread�������������ʱ���жϳ���,
	  ���������쳣,Ҳ�������쳣�����е���std::thread�����join����detach����.
	3.��thread���������detach()����ʱ,�������˳�ʱ



////////////////////////////////////////////////////////////////////////////
// �߳���:
<mutex>ͷ�ļ�����
	Mutex ϵ����(����)
		std::mutex��������� Mutex �ࡣ
		std::recursive_mutex���ݹ� Mutex �ࡣ
		std::time_mutex����ʱ Mutex �ࡣ
		std::recursive_timed_mutex����ʱ�ݹ� Mutex �ࡣ
	Lock �ࣨ���֣�
		std::lock_guard���� Mutex RAII ��أ������̶߳Ի�����������
		std::unique_lock���� Mutex RAII ��أ������̶߳Ի��������������ṩ�˸��õ������ͽ������ơ�
	��������
		std::once_flag
		std::adopt_lock_t
		std::defer_lock_t
		std::try_to_lock_t
	����
		std::try_lock������ͬʱ�Զ��������������
		std::lock������ͬʱ�Զ��������������
		std::call_once���������߳���Ҫͬʱ����ĳ��������call_once ���Ա�֤����̶߳Ըú���ֻ��
		��һ�Ρ�
	std::mutex ����
		������ std::mutex Ϊ������ C++11 �еĻ������÷���
		std::mutex ��C++11 ��������Ļ�������std::mutex �����ṩ�˶�ռ����Ȩ������,����֧�ֵ�
		��ض�std::mutex ������������ std::recursive_lock ����Եݹ�ضԻ���������������

	std::mutex �ĳ�Ա����
		1.���캯����std::mutex�����������죬Ҳ������move���������������mutex�����Ǵ���unlocked״̬�ġ�
		2.lock()�������߳̽���ס�û��������̵߳��øú����ᷢ������ 3 �������
			(1).����û�������ǰû�б���ס��������߳̽��û�������ס��ֱ������ unlock֮ǰ�����߳�һֱӵ�и�����
			(2).�����ǰ�������������߳���ס����ǰ�ĵ����̱߳�����ס��
			(3).�����ǰ����������ǰ�����߳���ס������������(deadlock)��
		3.unlock()���������ͷŶԻ�����������Ȩ��
		4.try_lock()��������ס������������������������߳�ռ�У���ǰ�߳�Ҳ���ᱻ�������̵߳��øú�
		  ��Ҳ��������� 3 �������
			(1).�����ǰ������û�б������߳�ռ�У�����߳���ס��������ֱ�����̵߳���unlock�ͷŻ�������
			(2).�����ǰ�������������߳���ס����ǰ�����̷߳��� false���������ᱻ��������
			(3).�����ǰ����������ǰ�����߳���ס������������(deadlock)��

	ʾ��:
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
			// std::lock_guard,����ڹ����ʱ���ṩ�����Ļ�����,����������ʱ����н���,�Ӷ���֤�Զ�����
			std::lock_guard<std::mutex> auto_lock(g_mtx);
			printf("###\n");
		}
-------------------------------------------


///////////////////////////////////////////////////////////////////////////////

// ����ʾ��:

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
