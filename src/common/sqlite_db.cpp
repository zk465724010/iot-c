#include <stdio.h>
#include "sqlite_db.h"
#include "debug.h"
#include <stdlib.h>

#ifdef _WIN32
    //#include <windows.h>
    #ifdef _DEBUG
        #define USE_SQLITE_DEBUG_OUTPUT
    #endif
    #define STDCALL __stdcall
#elif __linux__
    #include <unistd.h>
    #define USE_SQLITE_DEBUG_OUTPUT
    #define STDCALL __attribute__((__stdcall__))
#else
#endif


CSqlite::CSqlite()
{
    init();
}

CSqlite::~CSqlite()
{
    release();
}

int CSqlite::init()
{
    int nRet = 0;

    return nRet;
}

void CSqlite::release()
{

}

int CSqlite::open(const char *db_file)
{
    int nRet = -1;
    if((NULL == m_db) && (NULL != db_file))
    {
        nRet = sqlite3_open(db_file, &m_db);
        #ifdef USE_SQLITE_DEBUG_OUTPUT
        if(SQLITE_OK != nRet)
        {
            DEBUG("Can't open database:%s\n",sqlite3_errmsg(m_db));
            sqlite3_close(m_db);
            m_db = NULL;
        }
        #endif
    }
    return nRet;
}

int CSqlite::rebuild(const char *db_file, const char *script_file)
{
    if((NULL == db_file) || (NULL == script_file))
        return -1;

    char cmd[256] = {0};
    sprintf(cmd, "sqlite3 %s < %s", db_file, script_file);
    return system(cmd);
}

void CSqlite::close()
{
    if(NULL != m_db)
    {
        sqlite3_close(m_db);
        m_db = NULL;
    }
}

int CSqlite::exec(const char *sql, callback cbk, void *param)
{
    int nRet = -1;
    if((NULL != m_db) && (NULL != sql))
    {
        char *pErrMsg = NULL;
        nRet = sqlite3_exec(m_db, sql, cbk, param, &pErrMsg);
        if(SQLITE_OK != nRet)
        {
            DEBUG("sqlite3_exec eror:'%s'\n sql:'%s'\n ret:%d\n", pErrMsg, sql, nRet);
            if(SQLITE_BUSY == nRet)
            {
                #ifdef __linux__
                    usleep(100);
                #endif
                if(NULL != pErrMsg)
                    sqlite3_free(pErrMsg);
                nRet = sqlite3_exec(m_db, sql, cbk, param, &pErrMsg);
                if(SQLITE_OK != nRet)
                {
                    DEBUG("retry error: \n sqlite3_exec sql:'%s' \neror:'%s' \nret:%d\n",sql, pErrMsg, nRet);
                    if(NULL != pErrMsg)
                    sqlite3_free(pErrMsg);
                }
            }
        }
    }
    return nRet;
}

#if 0

void inquire_nocb(sqlite3 *db)
{
    int nrow,ncolumn;
	char **azresult;
	char *sql;
	char *errmsg;
	int ret;
	int i;
 
	sql = "select * from mytable;";
 
    ret = sqlite3_get_table(db,sql,&azresult,&nrow,&ncolumn,&errmsg);
 
    if(ret != SQLITE_OK)
	{
		printf("get table error:%s",errmsg);
		exit(-1);
	}
 
	printf("nrow = %d,column = %d\n",nrow,ncolumn);
	sqlite3_free_table(azresult);
}

#endif