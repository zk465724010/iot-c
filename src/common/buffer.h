
//////////////////////////////////////////////////////////////////////////////////////////////
// 文件名称：buffer.h 																		//
// 类的名称：CBuffer																		//
// 类的作用：数据缓冲区																		//
// 使用平台：Windows / Linux																//
// 作    者：zk	 																			//
// 编写日期：2022.7.19 17:05 二 															//
//////////////////////////////////////////////////////////////////////////////////////////////

#ifndef __BUFFER_H__
#define __BUFFER_H__

#define DEBUG_OUTPUT					//调试输出
#define USE_LOCK						//是否使用锁
#define	POOL_AUTO_EXTEND_SIZE	100		//自动扩充对象池的步长,若为0表示不自动扩充对象池(即:每次扩充多少个对象节点)
#define	POOL_MAX_SIZE			10000000 //对象池的最大大小(即:池中节点的最大个数)

//覆盖模式 (即:当缓冲区已满时,处理情况如下)
#define RM_OLD			0x01			//覆盖最旧的对象(即:覆盖最先添加到缓冲区的对象)
#define RM_NEW			0x02			//覆盖最新的对象(即:覆盖最后一次添加到缓冲区的对象)
#define RM_DISCARD		0x04			//丢弃待入队的新对象(即:缓冲区已满,不再缓存新的对象了)
#define RM_EXTEND		0x08			//自动扩容缓冲区,以便缓存新的对象(可以与上面3个组合使用)

#ifdef USE_LOCK
#include<mutex>
#endif

// new,bad_alloc 依赖头文件 #include <new>和using namespace std;
#ifdef __linux__
#include <new>
using namespace std;
#endif

typedef int (*create_cbk)(void*& obj, int& obj_size, void* user_data);
typedef void (*destory_cbk)(void*& obj, void* user_data);
typedef void (*print_cbk)(void* obj, int obj_size, int obj_len, void* user_data);

#define TEMPLATE template<typename OBJ_TYPE>

TEMPLATE 
class CBuffer
{
	typedef  struct tag_node {
		OBJ_TYPE* pObject = NULL;	//对象指针
		void* pStart = NULL;		//对象指针
		int nSize = 0;				//缓冲区大小
		int nLen = 0;				//缓冲区数据长度
		tag_node* pNext = NULL;		//下一个节点指针
	}node_t;
public:
	typedef struct tag_buffer {
		int nNodeCount = 0;	//链表的节点个数(链表的大小)
		int nDataNodeCount = 0;	//缓冲区中有数据的节点个数
		int nAutoExtendSize = POOL_AUTO_EXTEND_SIZE;//自动扩充链表的步长,若为0表示不自动扩充链表(即:每次扩充多少个对象节点)
		int nMaxNodeSize = POOL_MAX_SIZE;//链表的最大大小(即:池中节点的最大个数)
		node_t*		  pHeader = NULL;	//头节点(链表的第一个节点)
		node_t*		  pRead = NULL;		//读数据
		node_t*		  pWrite = NULL;	//写数据
	}buffer_t;

	typedef struct IOContext {
		int64_t pos = 0;        // 当前位置
		int64_t filesize = 0;   // 文件大小
		uint8_t* fuzz = NULL;   // 数据起始指针
		int64_t fuzz_size = 0;   // 剩余数据大小
		const char* remote_name = NULL;
	} IOContext;

public:
	CBuffer(void);
	~CBuffer(void);

	//创建缓冲区
	int Create(int nNodeCount, create_cbk create_cbk_, destory_cbk destory_cbk_, void* user_data=NULL,int nMaxNodeSize=POOL_MAX_SIZE, int nAutoExtendSize=POOL_AUTO_EXTEND_SIZE);
	void Destory();
	int Write(OBJ_TYPE* pObject, int size);	// 写缓冲
	int Read(OBJ_TYPE* pBuf, int size);		// 读缓冲
	int64_t Seek(int64_t offset);

	// 获取所有对象所占字节数(单位:字节)
	long long GetDataSize();
	// 获取空闲节点的个数
	inline int GetIdleNodeCount() { return (m_buffer.nNodeCount - m_buffer.nDataNodeCount); }
	// 获取数据节点的个数
	inline int GetDataNodeCount() {return m_buffer.nDataNodeCount;}
	// 清空所有节点中的数据
	void Clean();

	//int m_upload_count = 10;
	bool m_flag = true;

	#ifdef DEBUG_OUTPUT
	void PrintBuffer(print_cbk cbk);//打印
	#endif

private:
	int init();
	void release();
	int InsertNode(node_t *pPosition, int nCount, int nObjectSize);//在指定节点后面插入nNodeCount个新节点
private:
	buffer_t  m_buffer;
	create_cbk m_create_obj_cbk = NULL;
	destory_cbk m_destory_obj_cbk = NULL;
	void*   m_user_data = NULL;
#ifdef USE_LOCK
private:
	std::mutex m_mutex;
#endif
};

#if 1
TEMPLATE long long CBuffer<OBJ_TYPE>::GetDataSize()
{
	#ifdef USE_LOCK
	std::lock_guard<std::mutex> auto_lock(m_mutex);
	#endif
	long long llRet = 0;
	node_t* pNode = m_buffer.pHeader;
	while (NULL != pNode) {
		if (pNode->nLen > 0)
			llRet += pNode->nLen;
		pNode = pNode->pNext;
		if (pNode == m_buffer.pHeader)
			break;
	}
	return llRet;
}
TEMPLATE void CBuffer<OBJ_TYPE>::Clean()
{
	#ifdef USE_LOCK
	std::lock_guard<std::mutex> auto_lock(m_mutex);
	#endif
	node_t* pNode = m_buffer.pHeader;
	while (NULL != pNode) {
		if (pNode->nLen > 0) {
			pNode->nLen = 0;
			m_buffer.nDataNodeCount--;
		}
		pNode = pNode->pNext;
		if (pNode == m_buffer.pHeader) {
			m_buffer.pWrite = m_buffer.pHeader;
			m_buffer.pRead = m_buffer.pHeader;
			break;
		}
	}
}
TEMPLATE CBuffer<OBJ_TYPE>::CBuffer(void)
{
	init();
}

TEMPLATE CBuffer<OBJ_TYPE>::~CBuffer(void)
{
	release();
}
TEMPLATE int CBuffer<OBJ_TYPE>::init()
{
	return 0;
}
TEMPLATE void CBuffer<OBJ_TYPE>::release()
{
}
TEMPLATE int CBuffer<OBJ_TYPE>::Create(int nNodeCount, create_cbk create_cbk_, destory_cbk destory_cbk_, void* user_data/*=NULL*/,int nMaxNodeSize/*=POOL_MAX_SIZE*/, int nAutoExtendSize/*=POOL_AUTO_EXTEND_SIZE*/)
{
	#ifdef USE_LOCK
	std::lock_guard<std::mutex> auto_lock(m_mutex);
	#endif
	m_create_obj_cbk = create_cbk_;
	m_destory_obj_cbk = destory_cbk_;
	m_user_data = user_data;
	m_buffer.nAutoExtendSize = nAutoExtendSize;
	m_buffer.nMaxNodeSize = nMaxNodeSize;
	nNodeCount = (nNodeCount > nMaxNodeSize) ? nMaxNodeSize : nNodeCount;
	if ((nNodeCount <= 0) || (NULL != m_buffer.pHeader)) {
		LOG("ERROR", "Input parameter error [NodeCount:%d,MaxSize:%d,AutoExtendSize:%d,Header=%p]\n", nNodeCount, nMaxNodeSize, nAutoExtendSize, m_buffer.pHeader);
		return -1;
	}
	// 创建头节点
	try {
		m_buffer.pHeader = new node_t();
		m_buffer.nNodeCount++;
	}
	catch (const bad_alloc& e) {
		m_buffer.pHeader = NULL;
		LOG("ERROR", "Allocation memory failed [NodeCount:%d]\n", m_buffer.nNodeCount);
		return -1;
	}
	if (NULL != m_create_obj_cbk) {
		m_create_obj_cbk((void*&)m_buffer.pHeader->pObject, m_buffer.pHeader->nSize,user_data);
		m_buffer.pHeader->pStart = m_buffer.pHeader->pObject;
	}
	///////////////////////////////////////////////////////
	// 创建链表
	node_t* pNode = m_buffer.pHeader;
	for (int i = 0; i < nNodeCount - 1; i++) {
		try {
			pNode->pNext = new node_t();
			m_buffer.nNodeCount++;
		}
		catch (const bad_alloc& e) {
			pNode->pNext = m_buffer.pHeader; // 形成环
			LOG("ERROR", "Allocation memory failed '%s' [NodeCount:%d]\n", e.what(), m_buffer.nNodeCount);
			break;
		}
		pNode = pNode->pNext;
		pNode->pNext = m_buffer.pHeader; // 形成环
		if (NULL != m_create_obj_cbk) {
			int ret = m_create_obj_cbk((void*&)pNode->pObject, pNode->nSize,user_data);
			pNode->pStart = pNode->pObject;
			if (0 != ret)
				break;
		}
	}//for
	//pNode->pNext = m_buffer.pHeader;	//形成环(单向循环链表)
	m_buffer.pRead = m_buffer.pHeader;
	m_buffer.pWrite = m_buffer.pHeader;
	return m_buffer.nNodeCount;
}
TEMPLATE void CBuffer<OBJ_TYPE>::Destory()
{
	#ifdef USE_LOCK
	std::lock_guard<std::mutex> auto_lock(m_mutex);
	#endif
	node_t *pNode = m_buffer.pHeader;
	while (NULL != pNode){
		if (NULL != pNode->pObject){
			if (NULL != m_destory_obj_cbk)
				m_destory_obj_cbk((void*&)pNode->pObject, m_user_data);
			else delete [] pNode->pObject;
			m_buffer.nDataNodeCount--;
		}
		node_t *pDel = pNode;
		pNode = pNode->pNext;
		delete pDel;
		m_buffer.nNodeCount--;
		if (pNode == m_buffer.pHeader) {
			pNode = NULL;
			m_buffer.pHeader = NULL;
			m_buffer.pRead = NULL;
			m_buffer.pWrite = NULL;
		}
	}
}
TEMPLATE int CBuffer<OBJ_TYPE>::Write(OBJ_TYPE* pObject, int size)
{
	int write_bytes = 0;
	if ((NULL == pObject) || (size <= 0) || (NULL == m_buffer.pWrite)) {
		LOG("ERROR", "Input parameter error. [pObject:%p,size:%d,pWrite:%p]\n", pObject, size, m_buffer.pWrite);
		return write_bytes;
	}
	#ifdef USE_LOCK
	std::lock_guard<std::mutex> auto_lock(m_mutex);
	#endif
start:
	if (NULL == m_buffer.pWrite->pObject) { // 当前节点无对象缓冲区
		if (NULL != m_create_obj_cbk) {
			int ret = m_create_obj_cbk((void*&)m_buffer.pWrite->pObject, m_buffer.pWrite->nSize, m_user_data);
			m_buffer.pWrite->pStart = m_buffer.pWrite->pObject;
			if (0 != ret) {
				LOG("ERROR", "Callback error occurred %d\n", ret);
				return write_bytes;
			}
		}
		else {
			try {
				m_buffer.pWrite->pObject = new OBJ_TYPE[size+1];
			}
			catch (const bad_alloc& e) {
				m_buffer.pWrite->pObject = NULL;
				LOG("ERROR", "Allocation memory failed '%s'\n", e.what());
				return write_bytes;
			}
			m_buffer.pWrite->pStart = m_buffer.pWrite->pObject;
			m_buffer.pWrite->nSize = size;
		}
	}
	if (m_buffer.pWrite->nLen > 0) {	// 当前节点是否有数据
		//LOG("WARN", "DataNodeCount:%d, NodeCount:%d\n", m_buffer.nDataNodeCount, m_buffer.nNodeCount);
		node_t* pNode = m_buffer.pWrite->pNext;
		while ((pNode->nLen > 0) && (pNode != m_buffer.pWrite))
			pNode = pNode->pNext;		// 寻找下一个空闲节点
		m_buffer.pWrite = pNode;
		// 缓冲区已满,扩充新空闲节点
		if (m_buffer.pWrite->nLen > 0) {
			LOG("INFO", "Buffer automatic expansion %d node\n", m_buffer.nAutoExtendSize);
			// 在pWrite节点后面插入扩充的新空闲节点
			if (InsertNode(m_buffer.pWrite, m_buffer.nAutoExtendSize, 0) <= 0) {
				LOG("ERROR", "Insert node failed [NodeCount:%d,MaxSize:%d]\n", m_buffer.nNodeCount, m_buffer.nMaxNodeSize);
				return write_bytes;
			}
			m_buffer.pWrite = m_buffer.pWrite->pNext;
		}
	}
	m_buffer.nDataNodeCount++; // 缓冲区中有数据的节点统计
	if (m_buffer.pWrite->nSize >= size) {
		//memcpy(m_buffer.pWrite->pObject, pObject, (sizeof(OBJ_TYPE) * size));
		memcpy(m_buffer.pWrite->pObject, pObject, size);
		m_buffer.pWrite->nLen = size;
		write_bytes += size;
	}
	else {
		//memcpy(m_buffer.pWrite->pObject, pObject, (sizeof(OBJ_TYPE) * m_buffer.pWrite->nSize));
		memcpy(m_buffer.pWrite->pObject, pObject, m_buffer.pWrite->nSize);
		m_buffer.pWrite->nLen = m_buffer.pWrite->nSize;
		write_bytes += m_buffer.pWrite->nSize;

		size -= m_buffer.pWrite->nSize;
		pObject += m_buffer.pWrite->nSize;

		if (0 == m_buffer.pWrite->pNext->nLen)
			m_buffer.pWrite = m_buffer.pWrite->pNext;
		goto start;
	}
	if (0 == m_buffer.pWrite->pNext->nLen)
		m_buffer.pWrite = m_buffer.pWrite->pNext;	
	return write_bytes;
}
TEMPLATE int CBuffer<OBJ_TYPE>::Read(OBJ_TYPE* pBuf, int size)
{
	if ((NULL == pBuf) || (size <= 0) || (NULL == m_buffer.pRead)) {
		LOG("ERROR", "Input parameter error. [pBuf=%p,size=%d,pRead=%p]\n", pBuf, size, m_buffer.pRead);
		return 0;
	}
	#ifdef USE_LOCK
	std::lock_guard<std::mutex> auto_lock(m_mutex);
	#endif
	if ((0 == m_buffer.pRead->nLen) || (NULL == m_buffer.pRead->pObject)) {
		node_t* pNode = m_buffer.pRead->pNext;
		while ((0 == pNode->nLen) && (pNode != m_buffer.pRead))
			pNode = pNode->pNext; // 寻找下一个有数据的节点
		m_buffer.pRead = pNode;
	}
	static long long num = 0; // 测试
	if (m_buffer.pRead->nLen > 0) {
		num = 0;
		int nRet = size;
		if (m_buffer.pRead->nLen > size) {
			//memcpy(pBuf, m_buffer.pRead->pStart, (sizeof(OBJ_TYPE) * size));
			memcpy(pBuf, m_buffer.pRead->pStart, size);
			m_buffer.pRead->pStart = ((OBJ_TYPE*)m_buffer.pRead->pStart) + size;
			m_buffer.pRead->nLen -= size;
		}
		else {
			nRet = m_buffer.pRead->nLen;
			//memcpy(pBuf, m_buffer.pRead->pStart, (sizeof(OBJ_TYPE) * m_buffer.pRead->nLen));
			memcpy(pBuf, m_buffer.pRead->pStart, m_buffer.pRead->nLen);
			m_buffer.pRead->pStart = m_buffer.pRead->pObject;
			m_buffer.pRead->nLen = 0;
			if (m_buffer.pRead->pNext->nLen > 0)
				m_buffer.pRead = m_buffer.pRead->pNext; // 指向下一个有数据的节点
			m_buffer.nDataNodeCount--; // 缓冲区中有数据的节点统计
		}
		return nRet;
	}
	else LOG("WARN", "%lld Buffer no data [DataNodeCount:%d,NodeCount:%d]\n", num++, m_buffer.nDataNodeCount, m_buffer.nNodeCount);
	return 0;
}
TEMPLATE int64_t CBuffer<OBJ_TYPE>::Seek(int64_t offset)
{
	if (offset <= 0) {
		LOG("ERROR", "Input parameter error. [offset=%lld]\n", offset);
		return 0;
	}
	#ifdef USE_LOCK
	std::lock_guard<std::mutex> auto_lock(m_mutex);
	#endif
	if (m_buffer.pRead->nLen > 0) {
		while (offset > 0) {
			if (m_buffer.pRead->nLen > offset) {
				m_buffer.pRead->pStart = ((OBJ_TYPE*)m_buffer.pRead->pStart) + offset;
				m_buffer.pRead->nLen -= offset;
				offset = 0;
			}
			else {
				offset -= m_buffer.pRead->nLen;
				m_buffer.pRead->nLen = 0;
				node_t* pNode = m_buffer.pRead->pNext;
				while ((0 == pNode->nLen) && (pNode != m_buffer.pRead))
					pNode = pNode->pNext; // 寻找下一个有数据的节点
				m_buffer.pRead = pNode;
			}
		}
	}
	else LOG("WARN", "Buffer no data [DataNodeCount:%d,NodeCount:%d]\n", m_buffer.nDataNodeCount, m_buffer.nNodeCount);
	return 0;
}

TEMPLATE int CBuffer<OBJ_TYPE>::InsertNode(node_t *pPosition, int nCount, int nObjectSize)
{
	if ((NULL == pPosition) || (nCount <= 0)) {
		LOG("ERROR", "Input parameter error. [Position=%p,Count=%d]\n", pPosition, nCount);
		return 0;
	}
	if (m_buffer.nMaxNodeSize <= m_buffer.nNodeCount) {
		LOG("WARN","Insert node failed [DataNode:%d,NodeCount:%d,MaxNodeSize:%d]\n", m_buffer.nDataNodeCount,m_buffer.nNodeCount, m_buffer.nMaxNodeSize);
		#if 1
			return 0;
		#else
		//覆盖模式 (即:当缓冲区已满时,处理情况如下)
		//#define RM_OLD			0x01			//覆盖最旧的对象(即:覆盖最先添加到缓冲区的对象)
		//#define RM_NEW			0x02			//覆盖最新的对象(即:覆盖最后一次添加到缓冲区的对象)
		//#define RM_DISCARD		0x04			//丢弃待入队的新对象(即:缓冲区已满,不再缓存新的对象了)
		//#define RM_EXTEND		0x08			//自动扩容缓冲区,以便缓存新的对象(可以与上面3个组合使用)
		#endif
	}
	int nRet = 0;
	int nSurplus = m_buffer.nMaxNodeSize - m_buffer.nNodeCount;
	nCount = nSurplus > nCount ? nCount : nSurplus;
	node_t* pos = pPosition;
	node_t* next = pPosition->pNext;
	node_t* new_node = NULL;
	for (int i = 0; i < nCount; i++) {
		try {
			new_node = new node_t();
		}
		catch (const bad_alloc& e) {
			LOG("ERROR", "Allocation memory failed '%s' [NodeCount:%d]\n", e.what(), m_buffer.nNodeCount);
			break;
		}
		if ((0 == nObjectSize) && (NULL != m_create_obj_cbk)) {
			int ret = m_create_obj_cbk((void*&)new_node->pObject, new_node->nSize, m_user_data);
			new_node->pStart = new_node->pObject;
			if (0 != ret) {
				delete new_node;
				break;
			}
		}
		else {
			try {
				new_node->pObject = new OBJ_TYPE[nObjectSize + 1];
			}
			catch (const bad_alloc& e) {
				LOG("ERROR", "Allocation memory failed '%s'\n", e.what());
				delete new_node;
				break;
			}
			new_node->pStart = new_node->pObject;
			new_node->nSize = nObjectSize;
		}
		pos->pNext = new_node;
		pos = pos->pNext;
		pos->pNext = next;			// 形成环
		m_buffer.nNodeCount++;
		nRet++;
	}//for
	return nRet;
}

#ifdef DEBUG_OUTPUT
TEMPLATE void CBuffer<OBJ_TYPE>::PrintBuffer(print_cbk cbk)
{
	if (NULL == m_buffer.pHeader){
		LOG("WARN","Buffer is not created [NodeCount:%d]\n", m_buffer.nNodeCount);
		return;
	}
	#ifdef USE_LOCK
	std::lock_guard<std::mutex> auto_lock(m_mutex);
	#endif
	printf("\n\n");
#ifdef _WIN32
	static HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
	//SetConsoleTextAttribute(hConsole, F_RED | B_GRAY);
	SetConsoleTextAttribute(hConsole, F_RED_G);
	printf("[%d/%d]:  ", m_buffer.nNodeCount, m_buffer.nMaxNodeSize);
	SetConsoleTextAttribute(hConsole, F_WHITE | B_BLACK);//恢复系统默认的前景背景颜色(黑底白字,无高亮)
	static CONSOLE_SCREEN_BUFFER_INFO console_info;
	node_t *pNode = m_buffer.pHeader;
	while (NULL != pNode){
		GetConsoleScreenBufferInfo(hConsole, &console_info);
		//==================================================================================
		//光标位置上移一行
		SetConsoleCursorPosition(hConsole, { console_info.dwCursorPosition.X, console_info.dwCursorPosition.Y - 1 });
		if ((pNode == m_buffer.pRead) && (pNode == m_buffer.pWrite) && (pNode == m_buffer.pHeader)){
			printf("|");
			SetConsoleTextAttribute(hConsole, F_RED_G);
			printf("|");
			SetConsoleTextAttribute(hConsole, F_GREEN_G);
			printf("|");
		}
		else if ((pNode == m_buffer.pRead) && (pNode == m_buffer.pWrite)){
			SetConsoleTextAttribute(hConsole, F_RED_G);
			printf("| ");
			SetConsoleTextAttribute(hConsole, F_GREEN_G);
			printf("|");
		}
		else if ((pNode == m_buffer.pRead) && (pNode == m_buffer.pHeader)){
			printf("| ");
			SetConsoleTextAttribute(hConsole, F_RED_G);
			printf("|");
		}
		else if ((pNode == m_buffer.pWrite) && (pNode == m_buffer.pHeader)){
			printf("| ");
			SetConsoleTextAttribute(hConsole, F_GREEN_G);
			printf("|");
		}
		else if (pNode == m_buffer.pRead){
			SetConsoleTextAttribute(hConsole, F_RED_G);
			printf(" |");
		}
		else if (pNode == m_buffer.pWrite){
			SetConsoleTextAttribute(hConsole, F_GREEN_G);
			printf(" |");
		}
		else if (pNode == m_buffer.pHeader){
			printf(" |");
		}
		//光标位置下移一行
		SetConsoleCursorPosition(hConsole, { console_info.dwCursorPosition.X, console_info.dwCursorPosition.Y });
		//==================================================================================
		SetConsoleTextAttribute(hConsole, F_AZURE_G | B_GRAY);//设置字体和背景颜色
		if (NULL != cbk) {
			cbk(pNode->pStart, pNode->nSize, pNode->nLen, m_user_data);
		}
		else {
			if ((NULL != pNode->pStart) && (pNode->nLen > 0))
				printf("[%d]", *((OBJ_TYPE*)pNode->pStart));
			else printf("[ ]");
		}
		SetConsoleTextAttribute(hConsole, F_WHITE | B_BLACK);//设置字体和背景颜色
		if (pNode->pNext != m_buffer.pHeader){
			pNode = pNode->pNext;
			printf(" -> ");
		}
		else break;
	} // while
#elif __linux__		
	printf("\033[31m[%d/%d]2: \033[0m", m_buffer.nNodeCount, m_buffer.nMaxNodeSize);
	node_t* pNode = m_buffer.pHeader;
	while (NULL != pNode) {
		if ((pNode == m_buffer.pRead) && (pNode == m_buffer.pWrite) && (pNode == m_buffer.pHeader)) {
			printf("\033[1A");// 光标位置上移一行
			printf("|");
			printf("\033[31m|\033[0m"); // 红
			printf("\033[32m|\033[0m"); // 绿
			printf("\033[1B");// 光标位置下移一行
			printf("\033[3D"); // 光标左移2行
		}
		else if ((pNode == m_buffer.pRead) && (pNode == m_buffer.pWrite)) {
			printf("\033[1A");// 光标位置上移一行
			printf("\033[31m|\033[0m"); // 红
			printf("\033[32m|\033[0m"); // 绿
			printf("\033[1B");// 光标位置下移一行
			printf("\033[3D"); // 光标左移2行
		}
		else if ((pNode == m_buffer.pRead) && (pNode == m_buffer.pHeader)) {
			printf("\033[1A");// 光标位置上移一行
			printf("|");
			printf("\033[31m|\033[0m"); // 红
			printf("\033[1B");// 光标位置下移一行
			printf("\033[3D"); // 光标左移2行
		}
		else if ((pNode == m_buffer.pWrite) && (pNode == m_buffer.pHeader)) {
			printf("\033[1A");// 光标位置上移一行
			printf("|");
			printf("\033[32m|\033[0m"); // 绿
			printf("\033[1B");// 光标位置下移一行
			printf("\033[3D"); // 光标左移2行
		}
		else if (pNode == m_buffer.pRead) {
			printf("\033[1A");// 光标位置上移一行
			printf("\033[31m|\033[0m"); // 红
			printf("\033[1B");// 光标位置下移一行
			printf("\033[3D"); // 光标左移2行
		}
		else if (pNode == m_buffer.pWrite) {
			printf("\033[1A");// 光标位置上移一行
			printf("\033[32m|\033[0m"); // 绿
			printf("\033[1B");// 光标位置下移一行
			printf("\033[3D"); // 光标左移2行
		}
		else if (pNode == m_buffer.pHeader) {
			printf("\033[1A");// 光标位置上移一行
			printf(" |");
			printf("\033[1B");// 光标位置下移一行
			printf("\033[3D"); // 光标左移2行
		}
		if (NULL != cbk) {
			printf("\033[1;34;47m");
			cbk(pNode->pStart, pNode->nSize, pNode->nLen, m_user_data);
			printf("\033[0m");
		}
		else {
			if ((NULL != pNode->pStart) && (pNode->nLen > 0))
				printf("\033[1;34;47m[%d]\033[0m", *((OBJ_TYPE*)pNode->pStart));
			else printf("\033[1;34;47m[ ]\033[0m", *((OBJ_TYPE*)pNode->pStart));
		}
		if (pNode->pNext != m_buffer.pHeader) {
			pNode = pNode->pNext;
			printf(" -> ");
		}
		else break;
	} // while
#else
#endif
	printf("\n");
}
#endif//#ifdef DEBUG_OUTPUT

#endif//#if 1

#endif//#ifndef __BUFFER_H__
































