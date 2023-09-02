//////////////////////////////////////////////////////////////////////////////////////////////
// 文件名称：cmap.h															                //
// 类的名称：CMap																			//
// 类的作用：Map容器类														                //
// 作    者：zk	 																			//
// 编写日期：2022.02.11 21:32 五 															//
//////////////////////////////////////////////////////////////////////////////////////////////

#ifndef __UNIS_MAP_H__
#define __UNIS_MAP_H__

#include <map>
using namespace std;

#define USE_LOCK			//是否使用锁

#ifdef USE_LOCK
#include<mutex>
#endif

#ifdef _WIN32
	#define STDCALL __stdcall
#elif __linux__
	#define STDCALL __attribute__((__stdcall__))
#else
#endif

#define TEMPLATE template<typename KEY_TYPE, typename OBJ_TYPE>

TEMPLATE
class CMap
{
public:
    CMap();
    ~CMap();

    int add(KEY_TYPE key, OBJ_TYPE *value);
    int del(KEY_TYPE key);
    int move(KEY_TYPE key, OBJ_TYPE* dst); // 根据key移走一个对象
    int copy(KEY_TYPE key, OBJ_TYPE* dst);  // 根据key拷贝一个对象
    int get(OBJ_TYPE* out, int mode=0);    // 获取一个对象(mode=0为取走,mode=1为拷贝)
    int size();
	void clear();
	void print(void(STDCALL *cbk)(KEY_TYPE key, OBJ_TYPE *value));

    typename map<KEY_TYPE,OBJ_TYPE>::iterator find(KEY_TYPE key);
    typename map<KEY_TYPE,OBJ_TYPE>::iterator begin();
    typename map<KEY_TYPE,OBJ_TYPE>::iterator end();
    typename map<KEY_TYPE,OBJ_TYPE>::iterator erase(typename map<KEY_TYPE,OBJ_TYPE>::iterator it);

private:
	int init();
	void release();

private:
	map<KEY_TYPE, OBJ_TYPE> m_obj_map;

#ifdef USE_LOCK
private:
    std::mutex m_mutex;
#endif
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
        return nRet;
    }
    TEMPLATE void CMap<KEY_TYPE,OBJ_TYPE>::release()
    {

    }

    TEMPLATE int CMap<KEY_TYPE,OBJ_TYPE>::add(KEY_TYPE key, OBJ_TYPE *value)
    {
        if(NULL == value)
            return -1;

        #ifdef USE_LOCK
        std::lock_guard<std::mutex> auto_lock(m_mutex);
        #endif

        m_obj_map[key] = *value;    // 若不存在则添加,若存在则更新

		//auto iter = m_obj_map.find(key);
		//if (iter == m_obj_map.end())
		//	m_obj_map[key] = *value;
		//else
		//	iter->second = *value;
        return m_obj_map.size();
    }

    TEMPLATE int CMap<KEY_TYPE,OBJ_TYPE>::del(KEY_TYPE key)
    {
        #ifdef USE_LOCK
        std::lock_guard<std::mutex> auto_lock(m_mutex);
        #endif

        auto iter = m_obj_map.find(key);
        if(iter != m_obj_map.end())
        {
            m_obj_map.erase(iter);
            return m_obj_map.size();
        }
		return -1;
    }
    TEMPLATE int CMap<KEY_TYPE, OBJ_TYPE>::move(KEY_TYPE key, OBJ_TYPE* dst)
    {
        #ifdef USE_LOCK
        std::lock_guard<std::mutex> auto_lock(m_mutex);
        #endif
        auto iter = m_obj_map.find(key);
        if (iter != m_obj_map.end()) {
            memcpy(dst, &iter->second);
            m_obj_map.erase(iter);
            return 0;
        }
        return -1;
    }
    TEMPLATE int CMap<KEY_TYPE, OBJ_TYPE>::copy(KEY_TYPE key, OBJ_TYPE* dst)
    {
        #ifdef USE_LOCK
        std::lock_guard<std::mutex> auto_lock(m_mutex);
        #endif
        auto iter = m_obj_map.find(key);
        if (iter != m_obj_map.end()) {
            memcpy(dst, &iter->second);
            return 0;
        }
        return -1;
    }
    // 获取一个对象 (mode=0为取走,mode=1为拷贝)
    TEMPLATE int CMap<KEY_TYPE, OBJ_TYPE>::get(OBJ_TYPE* out, int mode/*=0*/)
    {
        #ifdef USE_LOCK
        std::lock_guard<std::mutex> auto_lock(m_mutex);
        #endif
        auto iter = m_obj_map.begin();
        if (iter != m_obj_map.end()) {
            memcpy(out, &iter->second, sizeof(OBJ_TYPE));
            if(0 == mode)
                m_obj_map.erase(iter);
            return 0;
        }
        return -1;
    }
    TEMPLATE int CMap<KEY_TYPE,OBJ_TYPE>::size()
    {
        #ifdef USE_LOCK
        std::lock_guard<std::mutex> auto_lock(m_mutex);
        #endif
		return m_obj_map.size();
    }

	TEMPLATE void CMap<KEY_TYPE, OBJ_TYPE>::clear()
	{
        #ifdef USE_LOCK
        std::lock_guard<std::mutex> auto_lock(m_mutex);
        #endif
		m_obj_map.clear();
	}

    TEMPLATE typename map<KEY_TYPE,OBJ_TYPE>::iterator CMap<KEY_TYPE,OBJ_TYPE>::find(KEY_TYPE key)
    {
        #ifdef USE_LOCK
        std::lock_guard<std::mutex> auto_lock(m_mutex);
        #endif

        return m_obj_map.find(key);
    }

    TEMPLATE typename map<KEY_TYPE,OBJ_TYPE>::iterator CMap<KEY_TYPE,OBJ_TYPE>::begin()
    {
        #ifdef USE_LOCK
        std::lock_guard<std::mutex> auto_lock(m_mutex);
        #endif

        return m_obj_map.begin();
    }

    TEMPLATE typename map<KEY_TYPE,OBJ_TYPE>::iterator CMap<KEY_TYPE,OBJ_TYPE>::end()
    {
        #ifdef USE_LOCK
        std::lock_guard<std::mutex> auto_lock(m_mutex);
        #endif

        return m_obj_map.end();
    }

    TEMPLATE typename map<KEY_TYPE,OBJ_TYPE>::iterator CMap<KEY_TYPE,OBJ_TYPE>::erase(typename map<KEY_TYPE,OBJ_TYPE>::iterator it)
    {
        #ifdef USE_LOCK
        std::lock_guard<std::mutex> auto_lock(m_mutex);
        #endif

        return m_obj_map.erase(it);
    }

	TEMPLATE void CMap<KEY_TYPE, OBJ_TYPE>::print(void(STDCALL *cbk)(KEY_TYPE key,OBJ_TYPE *value))
	{
		if (NULL != cbk)
		{
            #ifdef USE_LOCK
            std::lock_guard<std::mutex> auto_lock(m_mutex);
            #endif
            auto iter = m_obj_map.begin();
			for (; iter != m_obj_map.end(); iter++)
			{
				cbk(iter->first, &iter->second);
			}
		}
	}
#endif  // if 1

#endif  // #ifndef __UNIS_MAP_H__