/*************************************************************************
    > File Name: thread_pool.h
    > Author: hsz
    > Brief:
    > Created Time: 2024年10月11日 星期五 19时25分28秒
 ************************************************************************/

#ifndef __HTTP_THREAD_POOL_H__
#define __HTTP_THREAD_POOL_H__

#include <vector>

#include <SQLiteCpp/SQLiteCpp.h>

#include <utils/consistent_hash.h>
#include <utils/singleton.h>

#include "global_resource_management.h"
#include "sync_thread.h"

namespace eular {
class ThreadPool
{
public:
    ThreadPool();
    ~ThreadPool();

    bool start();
    void stop();

protected:
    void flushSQLite();
    void syncFromCloud(std::string diskPath, std::string parentFileId);

private:
    ConsistentHash<SyncThread>  m_syncThreadHashMap;
    std::vector<SyncThread::SP> m_syncThreadVec;
    Thread::SP  m_syncTh; // 同步线程
};

using ThreadPoolInstance = Singleton<ThreadPool>;
} // namespace eular

#endif // __HTTP_THREAD_POOL_H__
