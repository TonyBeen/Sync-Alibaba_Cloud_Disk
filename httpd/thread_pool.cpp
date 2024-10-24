/*************************************************************************
    > File Name: thread_pool.cpp
    > Author: hsz
    > Brief:
    > Created Time: 2024年10月11日 星期五 19时25分31秒
 ************************************************************************/

#include "thread_pool.h"

#include "sql_config.h"

#include <log/log.h>

#define LOG_TAG "ThreadPool"

namespace eular {
ThreadPool::ThreadPool()
{
}

ThreadPool::~ThreadPool()
{
}

bool ThreadPool::start()
{
    if (GlobalResourceInstance::Get()->logged_in) {
        return true;
    }

    // 1、打开数据库
    try {
        auto &g_sqliteHandle = GlobalResourceInstance::Get()->m_sqliteHandle;
        g_sqliteHandle = std::make_shared<SQLite::Database>(SQL_STORAGE_DISK, SQLite::OPEN_READWRITE | SQLite::OPEN_CREATE);
        // (1)、给磁盘数据库起个别名
        g_sqliteHandle->exec(SQL_ATTACH_DB(SQL_STORAGE_DISK, SQL_DB_SYNC));
        // 为磁盘数据库创建表, 存在表则不会创建
        g_sqliteHandle->exec(SQL_CREATE_TABLE_INFO(SQL_DB_SYNC));
        g_sqliteHandle->exec(SQL_CREATE_TABLE_DOWNLOAD(SQL_DB_SYNC));
        g_sqliteHandle->exec(SQL_CREATE_TABLE_UPLOAD(SQL_DB_SYNC));

        // (2)、附加内存数据库
        g_sqliteHandle->exec(SQL_ATTACH_DB(SQL_STORAGE_MEMORY, SQL_DB_MEMORY));
        // 为内存数据库创建表
        g_sqliteHandle->exec(SQL_CREATE_TABLE_INFO(SQL_DB_MEMORY));
        g_sqliteHandle->exec(SQL_CREATE_TABLE_DOWNLOAD(SQL_DB_MEMORY));
        g_sqliteHandle->exec(SQL_CREATE_TABLE_UPLOAD(SQL_DB_MEMORY));

        // (3)、将磁盘数据库的数据加载到内存中
        g_sqliteHandle->exec("INSERT INTO " SQL_DB_MEMORY "." SQL_TABLE_INFO " SELECT * FROM " SQL_DB_SYNC "." SQL_TABLE_INFO ";");
        g_sqliteHandle->exec("INSERT INTO " SQL_DB_MEMORY "." SQL_TABLE_DOWNLOAD " SELECT * FROM " SQL_DB_SYNC "." SQL_TABLE_DOWNLOAD ";");
        g_sqliteHandle->exec("INSERT INTO " SQL_DB_MEMORY "." SQL_TABLE_UPLOAD " SELECT * FROM " SQL_DB_SYNC "." SQL_TABLE_UPLOAD ";");
    } catch(const std::exception& e) {
        LOGE("catch exception: %s", e.what());
        return false;
    }

    try {
        // 2、创建线程执行执行
        m_syncTh = std::make_shared<Thread>([this](){
            this->syncFromCloud();
        }, "SYNC");
    } catch(const std::exception& e) {
        LOGE("create thread exception: %s", e.what());
        return false;
    }

    return true;
}

void ThreadPool::stop()
{
    if (!GlobalResourceInstance::Get()->logged_in) {
        return;
    }
}

void ThreadPool::flushSQLite()
{
    try {
        auto g_sqliteHandle = GlobalResourceInstance::Get()->m_sqliteHandle;
        LOG_ASSERT(g_sqliteHandle != nullptr, "g_sqliteHandle can't be null");

        // 将 内存数据库 刷新到 磁盘数据库
        g_sqliteHandle->exec("BEGIN TRANSACTION;");
        g_sqliteHandle->exec(SQL_DELETE_ALL_RECORD(SQL_DB_SYNC, SQL_TABLE_INFO));
        g_sqliteHandle->exec(SQL_DELETE_ALL_RECORD(SQL_DB_SYNC, SQL_TABLE_DOWNLOAD));
        g_sqliteHandle->exec(SQL_DELETE_ALL_RECORD(SQL_DB_SYNC, SQL_TABLE_UPLOAD));
        g_sqliteHandle->exec("INSERT INTO " SQL_DB_SYNC "." SQL_TABLE_INFO " SELECT * FROM " SQL_DB_MEMORY "." SQL_TABLE_INFO ";");
        g_sqliteHandle->exec("INSERT INTO " SQL_DB_SYNC "." SQL_TABLE_DOWNLOAD " SELECT * FROM " SQL_DB_MEMORY "." SQL_TABLE_DOWNLOAD ";");
        g_sqliteHandle->exec("INSERT INTO " SQL_DB_SYNC "." SQL_TABLE_UPLOAD " SELECT * FROM " SQL_DB_MEMORY "." SQL_TABLE_UPLOAD ";");
        g_sqliteHandle->exec("COMMIT;");

        // 分离
        g_sqliteHandle->exec("DETACH " SQL_DB_MEMORY ";");
    } catch(const std::exception& e) {
        LOGE("catch exception: %s", e.what());
    }
}

void ThreadPool::syncFromCloud()
{
}

} // namespace eular

