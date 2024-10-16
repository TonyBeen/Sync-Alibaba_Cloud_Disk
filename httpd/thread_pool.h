/*************************************************************************
    > File Name: thread_pool.h
    > Author: hsz
    > Brief:
    > Created Time: 2024年10月11日 星期五 19时25分28秒
 ************************************************************************/

#ifndef __HTTP_THREAD_POOL_H__
#define __HTTP_THREAD_POOL_H__

#include "sync_thread.h"

#include <utils/consistent_hash.h>
#include <utils/singleton.h>

namespace eular {
class ThreadPool
{
public:
    ThreadPool();
    ~ThreadPool();

private:
    ConsistentHash<SyncThread::SP> m_conHash;
};

using ThreadPoolInstance = Singleton<ThreadPool>;
} // namespace eular

#endif // __HTTP_THREAD_POOL_H__
