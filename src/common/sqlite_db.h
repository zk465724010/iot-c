

#ifndef __SQLITE_DB_H__
#define __SQLITE_DB_H__

#include "sqlite3.h"

#ifdef _WIN32
    #define STDCALL __stdcall
#elif __linux__
    #define STDCALL __attribute__((__stdcall__))
#endif


typedef int (*callback)(void *param, int column, char **value, char **field);



class CSqlite
{
public:
    CSqlite();
    ~CSqlite();

    int init();
    void release();


    int open(const char *db_file);
    void close();
    int rebuild(const char *db_file, const char *script_file);

    int exec(const char *sql, callback cbk, void *param);

private:
    //int query_proc(void *param, int column, char **value, char **field);


private:
    sqlite3*    m_db = NULL;

};




#endif