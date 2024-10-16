/*************************************************************************
    > File Name: sync_thread.h
    > Author: hsz
    > Brief:
    > Created Time: 2024年10月11日 星期五 20时08分41秒
 ************************************************************************/

#ifndef __HTTPD_SYNC_THREAD_H__
#define __HTTPD_SYNC_THREAD_H__

#include <memory>

#include <hv/json.hpp>

#include <utils/auto_clean.hpp>
#include <utils/thread.h>
#include <utils/string8.h>
#include <utils/buffer.h>

#include "concurrentqueue.h"

namespace eular {

struct FileContentItemNode {
    eular::String8  file_path;
    eular::String8  file_name;
    ByteBuffer      range_buffer;
    uint64_t        range_begin;
    uint64_t        range_end;
};

struct FileDownloadItemNode {
    eular::String8  drive_id;
    eular::String8  file_id;
};

class SyncThread
{
public:
    using SP = std::shared_ptr<SyncThread>;

    SyncThread();
    ~SyncThread();

    void start();
    void stop();

    int32_t enqueue(const FileDownloadItemNode &itemReq);

private:
    Thread::SP  m_httpDownloadThread;
    Thread::SP  m_fileIoThread;

    moodycamel::ConcurrentQueue<FileDownloadItemNode>   m_fileDownloadQueue;    // 文件下载队列
    moodycamel::ConcurrentQueue<FileContentItemNode>    m_fileContentQueue;     // 文件
};

} // namespace eular

#endif // __HTTPD_SYNC_THREAD_H__
