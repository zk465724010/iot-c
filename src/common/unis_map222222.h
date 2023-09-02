
#ifndef __UNIS_MAP_H__
#define __UNIS_MAP_H__

#include <map>
using namespace std;

#define USE_LOCK					        //是否使用锁

#ifdef _WIN32
    #ifdef USE_LOCK
    #include <windows.h>
    #endif
    #ifdef _DEBUG
        #define USE_UNISMAP_DEBUG_OUTPUT    //调试输出
    #endif
#elif __linux__
    #include <pthread.h>
    #define USE_UNISMAP_DEBUG_OUTPUT
#else
#endif

#define TEMPLATE template<typename KEY_TYPE, typename OBJ_TYPE>


TEMPLATE
class CMap
{
public:
    CMap();
    ~CMap();

    int init();
    void release();

    int add(KEY_TYPE key, OBJ_TYPE *value);
    int del(KEY_TYPE key);
    int size();
	void clear();


    typename map<KEY_TYPE,OBJ_TYPE>::iterator find(KEY_TYPE key);
    typename map<KEY_TYPE,OBJ_TYPE>::iterator begin();
    typename map<KEY_TYPE,OBJ_TYPE>::iterator end();

    typename map<KEY_TYPE,OBJ_TYPE>::iterator erase(typename map<KEY_TYPE,OBJ_TYPE>::iterator it);

public:
#ifdef USE_LOCK
    void lock();
    void unlock();
#endif


private:
#ifdef USE_LOCK
    #ifdef _WIN32
        HANDLE	m_mutex;						//互斥量对象句柄
    #elif __linux__
        pthread_mutex_t		m_mutex;			//互斥锁对象
        //pthread_mutexattr_t	m_mutex_attribute;	//互斥锁属性对象
    #endif
#endif

private:
    map<KEY_TYPE, OBJ_TYPE> m_obj_map;

};


#if 1
    TEMPLATE CMap<KEY_TYPE,OBJ_TYPE>::CMap()
    {
        init();
    }

    TEMPLATE CMap<KEY_TYPE,OBJ_TYPE>::~CMap()
    {
        release();
    }

    TEMPLATE int CMap<KEY_TYPE,OBJ_TYPE>::init()
    {
        int nRet = 0;
        #ifdef USE_LOCK
            #ifdef _WIN32
            	m_mutex = CreateMutex(NULL,	// 互斥量的安全属性,当为NULL表示为默认安全属性
		        false,					// 初始所有权( 当为False表示该互斥量对象不属于当前线程所有,其他线程都可以有效的获得该互斥量对象的所有权...
									    // ...当为True时,表示该互斥量对象属于当前线程所有,在当前线程释放该互斥量对象之前,其他线程都无法获得该互斥量对象的所有权。
		        NULL);					// 创建互斥量对象时,若指定的名称与前面互斥量对象的名称相同,则此时函数会返回前面已经存在的同名互斥量对象的句柄
            #elif __linux__
            nRet = pthread_mutex_init(&m_mutex, NULL);
            #endif
        #endif
        return nRet;
    }

    TEMPLATE void CMap<KEY_TYPE,OBJ_TYPE>::release()
    {
        #ifdef USE_LOCK
            unlock();
            #ifdef _WIN32
            CloseHandle(m_mutex);
            m_mutex = NULL;
            #elif __linux__
            pthread_mutex_destroy(&m_mutex);
            #endif
        #endif
    }

    TEMPLATE int CMap<KEY_TYPE,OBJ_TYPE>::add(KEY_TYPE key, OBJ_TYPE *value)
    {
        if(NULL == value)
            return -1;

        #ifdef USE_LOCK
            #ifdef _WIN32
            WaitForSingleObject(m_mutex, INFINITE);
            #elif __linux__
            pthread_mutex_lock(&m_mutex);
            #endif
        #endif

        //m_obj_map.insert(map<KEY_TYPE, OBJ_TYPE>::value_type(key, *obj));
		auto iter = m_obj_map.find(key);
		if (iter == m_obj_map.end())
			m_obj_map[key] = *value;
		else
			iter->second = *value;

        #ifdef USE_LOCK
            #ifdef _WIN32
            ReleaseMutex(m_mutex);		// 当线程执行完毕后,互斥量会自动释放,也可手动去释放(是多余的)
            #elif __linux__
            pthread_mutex_unlock(&m_mutex);
            #endif
        #endif
        return m_obj_map.size();
    }

    TEMPLATE int CMap<KEY_TYPE,OBJ_TYPE>::del(KEY_TYPE key)
    {
        int nRet = -1;

        #ifdef USE_LOCK
            #ifdef _WIN32
            WaitForSingleObject(m_mutex, INFINITE);
            #elif __linux__
            pthread_mutex_lock(&m_mutex);
            #endif
        #endif

        auto iter = m_obj_map.find(key);
        if(iter != m_obj_map.end())
        {
            m_obj_map.erase(iter);
            nRet = m_obj_map.size();
        }

        #ifdef USE_LOCK
            #ifdef _WIN32
            ReleaseMutex(m_mutex);
            #elif __linux__
            pthread_mutex_unlock(&m_mutex);
            #endif
        #endif
        return nRet;
    }

    TEMPLATE int CMap<KEY_TYPE,OBJ_TYPE>::size()
    {
        #ifdef USE_LOCK
            #ifdef _WIN32
            WaitForSingleObject(m_mutex, INFINITE);
            #elif __linux__
            pthread_mutex_lock(&m_mutex);
            #endif
        #endif

        int size = m_obj_map.size();

        #ifdef USE_LOCK
            #ifdef _WIN32
            ReleaseMutex(m_mutex);
            #elif __linux__
            pthread_mutex_unlock(&m_mutex);
            #endif
        #endif
        return size;
    }

	TEMPLATE void CMap<KEY_TYPE, OBJ_TYPE>::clear()
	{
		#ifdef USE_LOCK
		#ifdef _WIN32
			WaitForSingleObject(m_mutex, INFINITE);
		#elif __linux__
			pthread_mutex_lock(&m_mutex);
		#endif
		#endif

			m_obj_map.clear();

		#ifdef USE_LOCK
		#ifdef _WIN32
			ReleaseMutex(m_mutex);
		#elif __linux__
			pthread_mutex_unlock(&m_mutex);
		#endif
		#endif
	}

    TEMPLATE typename map<KEY_TYPE,OBJ_TYPE>::iterator CMap<KEY_TYPE,OBJ_TYPE>::find(KEY_TYPE key)
    {
        #ifdef USE_LOCK
            #ifdef _WIN32
            WaitForSingleObject(m_mutex, INFINITE);
            #elif __linux__
            pthread_mutex_lock(&m_mutex);
            #endif
        #endif

        auto iter = m_obj_map.find(key);

        #ifdef USE_LOCK
            #ifdef _WIN32
            ReleaseMutex(m_mutex);
            #elif __linux__
            pthread_mutex_unlock(&m_mutex);
            #endif
        #endif
        return iter;
    }

    TEMPLATE typename map<KEY_TYPE,OBJ_TYPE>::iterator CMap<KEY_TYPE,OBJ_TYPE>::begin()
    {
        return m_obj_map.begin();
    }

    TEMPLATE typename map<KEY_TYPE,OBJ_TYPE>::iterator CMap<KEY_TYPE,OBJ_TYPE>::end()
    {
        return m_obj_map.end();
    }

    TEMPLATE typename map<KEY_TYPE,OBJ_TYPE>::iterator CMap<KEY_TYPE,OBJ_TYPE>::erase(typename map<KEY_TYPE,OBJ_TYPE>::iterator it)
    {
        return m_obj_map.erase(it);
    }

#ifdef USE_LOCK
    TEMPLATE void CMap<KEY_TYPE,OBJ_TYPE>::lock()
    {
        #ifdef _WIN32
        WaitForSingleObject(m_mutex, INFINITE);
        #elif __linux__
        pthread_mutex_lock(&m_mutex);
        #endif
    }

    TEMPLATE void CMap<KEY_TYPE,OBJ_TYPE>::unlock()
    {
        #ifdef _WIN32
        ReleaseMutex(m_mutex);
        #elif __linux__
        pthread_mutex_unlock(&m_mutex);
        #endif
    }
#endif  // #ifdef USE_LOCK

#endif


#endif  // #ifndef __UNIS_MAP_H__