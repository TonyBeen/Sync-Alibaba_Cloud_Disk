/*************************************************************************
    > File Name: inotify_tool.cpp
    > Author: hsz
    > Brief:
    > Created Time: 2024年04月13日 星期六 10时17分31秒
 ************************************************************************/

#include <sstream>

#include <utils/sysdef.h>
#include <utils/errors.h>
#include <utils/buffer.h>
#include <log/log.h>

#ifdef OS_LINUX
#include <sys/inotify.h>
#include <sys/types.h>
#include <dirent.h>
#include <sys/ioctl.h>
#include <sys/select.h>

#define INOTIFY_EVENT_SIZE  (sizeof(struct inotify_event))
#define MAX_BUF_SIZE (1024 * INOTIFY_EVENT_SIZE)

#define INOTIFY_PROCDIR "/proc/sys/fs/inotify/"
#define WATCHES_SIZE_PATH INOTIFY_PROCDIR "max_user_watches"
#define QUEUE_SIZE_PATH INOTIFY_PROCDIR "max_queued_watches"
#define INSTANCES_PATH INOTIFY_PROCDIR "max_user_instances"

#endif

#include "inotify_tool/inotify_tool.h"
#include "inotify_tool/inotify_event.h"
#include "inotify_tool/inotify_utils.h"
#include "inotify_tool.h"

#define LOG_TAG "Inotify-tool"

#define INVALID_ID (-1)

namespace eular {

void DumpInotifyEvent(const struct inotify_event *ev);

InotifyTool::InotifyTool() noexcept :
    m_recursion(false),
    m_inotifyFd(INVALID_ID),
    m_errorCode(0),
    m_inotifyBuffer(MAX_BUF_SIZE)
{
}

InotifyTool::~InotifyTool() noexcept
{
    destroyInotify();
}

bool InotifyTool::createInotify() noexcept
{
    setErrorCode(0);
    if (INVALID_ID == m_inotifyFd)
    {
        m_inotifyFd = inotify_init1(IN_NONBLOCK);
    }

    if (INVALID_ID == m_inotifyFd)
    {
        setErrorCode(errno);
        return false;
    }

    return true;
}

void InotifyTool::destroyInotify()
{
    if (m_inotifyFd != INVALID_ID)
    {
        close(m_inotifyFd);
        m_inotifyFd = INVALID_ID;
    }

    setErrorCode(0);
    m_pathWithWd.clear();
}

int32_t InotifyTool::watchFile(const std::string &fileName, uint32_t ev)
{
    std::vector<std::string> paths = { fileName };
    return watchFiles(paths, ev);
}

int32_t InotifyTool::watchFiles(const std::vector<std::string> &fileNames, uint32_t ev)
{
    setErrorCode(0);
    if (INVALID_ID == m_inotifyFd)
    {
        return INVALID_OPERATION;
    }

    uint32_t realEv = inotifyEvent2RealInotifyEv(ev);
    if (realEv == InotifyEvent::EV_IN_NONE)
    {
        return NO_ERROR;
    }

    // 检测参数是否有效
    for (const auto &it : fileNames)
    {
        // 参数为空或非绝对路径
        if (it.empty() || it[0] != '/')
        {
            LOGE("Path(\"%s\") is not an absolute path", it.c_str());
            return INVALID_PARAM;
        }

        // inotify已监视此路径
        if (m_pathWithWd.contains(it))
        {
            LOGE("Path(\"%s\") already exists", it.c_str());
            return ALREADY_EXISTS;
        }
    }

    std::vector<InotifyInfo> wdVec;
    wdVec.reserve(fileNames.size());

    for (int32_t i = 0; i < fileNames.size(); ++i)
    {
        int32_t wd = inotify_add_watch(m_inotifyFd, fileNames[i].c_str(), IN_ALL_EVENTS);
        if (INVALID_ID == wd)
        {
            setErrorCode(errno);
            for (auto it : wdVec)
            {
                inotify_rm_watch(m_inotifyFd, it.wd);
                m_pathWithWd.erase(it);
            }
            return UNKNOWN_ERROR;
        }

        InotifyInfo info = {wd, ev, false};
        wdVec.push_back(info);
        std::string filePath = fileNames[i];
        size_t length = filePath.length();
        if (isDir(filePath) && filePath[length - 1] != '/')
        {
            filePath.append("/");
        }
        m_pathWithWd.insert(info, fileNames[i]);
    }

    return NO_ERROR;
}

int32_t InotifyTool::watchRecursive(const std::string &path, uint32_t ev)
{
    std::vector<std::string> paths = { path };
    return watchRecursive(paths, ev);
}

int32_t InotifyTool::watchRecursive(const std::vector<std::string> &paths, uint32_t ev)
{
    setErrorCode(0);

    if (INVALID_ID == m_inotifyFd)
    {
        return INVALID_OPERATION;
    }

    uint32_t realEv = inotifyEvent2RealInotifyEv(ev);
    if (realEv == InotifyEvent::EV_IN_NONE)
    {
        return NO_ERROR;
    }

    // 检测参数是否有效
    for (const auto &it : paths)
    {
        if (it.empty() || it[0] != '/' || !isDir(it))
        {
            LOGE("Invalid path(\"%s\")", it.c_str());
            return INVALID_PARAM;
        }

        if (m_pathWithWd.contains(it))
        {
            LOGE("Path(\"%s\") already exists", it.c_str());
            return ALREADY_EXISTS;
        }
    }

    std::vector<InotifyInfo> wdVec;
    wdVec.reserve(paths.size());

    for (int32_t i = 0; i < paths.size(); ++i)
    {
        std::list<std::pair<std::string, InotifyInfo>> infoList;
        if (!_watchRecursive(infoList, paths[i], ev))
        {
            return UNKNOWN_ERROR;
        }

        for (const auto &it : infoList)
        {
            m_pathWithWd.insert(it.second, it.first);
            LOGE("wd: %d, path: %s", it.second.wd, it.first.c_str());
        }
    }

    return NO_ERROR;
}

void InotifyTool::removeWatch(const std::string &path)
{
}

int32_t InotifyTool::waitCompleteEvent(uint32_t timeout)
{
    setErrorCode(0);
    if (INVALID_ID == m_inotifyFd)
    {
        return NO_INIT;
    }

    fd_set readFd;
    struct timeval readTimeout;
    struct timeval *pReadTimeout = nullptr;
    if (timeout > 0)
    {
        readTimeout.tv_sec = timeout / 1000;
        readTimeout.tv_usec = timeout & 1000 * 1000;

        pReadTimeout = &readTimeout;
    }

    FD_ZERO(&readFd);
    FD_SET(m_inotifyFd, &readFd);
    int32_t errorCode = select(m_inotifyFd + 1, &readFd, nullptr, nullptr, pReadTimeout);
    if (errorCode < 0)
    {
        setErrorCode(errno);
        return UNKNOWN_ERROR;
    }

    if (errorCode == 0)
    {
        return TIMED_OUT;
    }

    // 等待足够的字节数
    uint32_t bytesToRead = 0;
    do {
        errorCode = ioctl(m_inotifyFd, FIONREAD, &bytesToRead);
    } while (!errorCode && bytesToRead < sizeof(struct inotify_event));
    if (errorCode < 0)
    {
        LOGE("ioctl(FIONREAD) error. [%d, %s]", errno, strerror(errno));
        setErrorCode(errno);
        return UNKNOWN_ERROR;
    }

    char eventBuffer[MAX_BUF_SIZE];
    do {
        ssize_t readSize = ::read(m_inotifyFd, eventBuffer, MAX_BUF_SIZE);
        if (readSize < 0)
        {
            if (errno != EAGAIN)
            {
                setErrorCode(errno);
                LOGE("read(%d) error. [%d, %s]", m_inotifyFd, errno, strerror(errno));
                return UNKNOWN_ERROR;
            }

            break;
        }

        LOGI("read size: %zd", readSize);

        // 读到结尾
        if (readSize == 0)
        {
            break;
        }

        m_inotifyBuffer.append(eventBuffer, readSize);
        _parseEvent(m_inotifyBuffer);
    } while (true);

    return NO_ERROR;
}

void InotifyTool::getEventItem(std::list<InotifyEventItem> &eventItemVec)
{
    eventItemVec = std::move(m_eventItemQueue);
    m_eventItemQueue.clear();
}

int32_t InotifyTool::getLastError()
{
    return m_errorCode;
}

std::string InotifyTool::errorMsg(int32_t code)
{
    return strerror(code);
}

uint32_t InotifyTool::inotifyEvent2RealInotifyEv(uint32_t ev) const noexcept
{
    uint32_t inotifyEv = 0;

#define INOTIFY_MAP(XXX)                                        \
    XXX(InotifyEvent::EV_IN_MODIFY_OVER, IN_MODIFY)             \
    XXX(InotifyEvent::EV_IN_CLOSE_WRITE, IN_CLOSE_WRITE)        \
    XXX(InotifyEvent::EV_IN_MOVED_OUT, IN_MOVED_FROM)           \
    XXX(InotifyEvent::EV_IN_MOVED_IN, IN_MOVED_TO)              \
    XXX(InotifyEvent::EV_IN_CREATE, IN_CREATE)                  \
    XXX(InotifyEvent::EV_IN_DELETE, IN_DELETE)                  \

#define XXX(InoEv, realInoEv)   \
    if ((InoEv) & ev)           \
    {                           \
        inotifyEv |= realInoEv; \
    }                           \

    INOTIFY_MAP(XXX);

#undef XXX
#undef INOTIFY_MAP

    return inotifyEv;
}

uint32_t InotifyTool::realInotifyEv2InotifyEvent(uint32_t ev) const noexcept
{
    uint32_t inotifyEv = InotifyEvent::EV_IN_NONE;

#define INOTIFY_MAP(XXX)                                        \
    XXX(InotifyEvent::EV_IN_MODIFY_OVER, IN_MODIFY)             \
    XXX(InotifyEvent::EV_IN_CLOSE_WRITE, IN_CLOSE_WRITE)        \
    XXX(InotifyEvent::EV_IN_MOVED_OUT, IN_MOVED_FROM)           \
    XXX(InotifyEvent::EV_IN_MOVED_IN, IN_MOVED_TO)              \
    XXX(InotifyEvent::EV_IN_CREATE, IN_CREATE)                  \
    XXX(InotifyEvent::EV_IN_DELETE, IN_DELETE)                  \
    XXX(InotifyEvent::EV_IN_UNMOUNT, IN_UNMOUNT)                \
    XXX(InotifyEvent::EV_IN_IGNORED, IN_IGNORED)                \
    XXX(InotifyEvent::EV_IN_ISDIR, IN_ISDIR)                    \

#define XXX(InoEv, realInoEv)   \
    if ((realInoEv) & ev)       \
    {                           \
        inotifyEv |= InoEv;     \
    }                           \

    INOTIFY_MAP(XXX);

#undef XXX
#undef INOTIFY_MAP

    return 0;
}

bool InotifyTool::isDir(const std::string &path) const
{
    struct stat64 pathStat;

    if (-1 == lstat64(path.c_str(), &pathStat))
    {
        if (errno != ENOENT)
        {
            LOGW("Stat failed on %s: %s\n", path, strerror(errno));
        }

        return false;
    }

    return (S_ISDIR(pathStat.st_mode) && !S_ISLNK(pathStat.st_mode));
}

void InotifyTool::setErrorCode(int32_t code)
{
    m_errorCode = code;
}

bool InotifyTool::_watchRecursive(std::list<std::pair<std::string, InotifyInfo>> &infoList, const std::string &path, uint32_t ev)
{
    std::string fixedPath = path;
    utils::CorrectionPath(fixedPath);

    int32_t wd = inotify_add_watch(m_inotifyFd, fixedPath.c_str(), IN_ALL_EVENTS);
    if (INVALID_ID == wd)
    {
        return false;
    }

    InotifyInfo info = {wd, ev, true};
    infoList.push_back({fixedPath, info});

    DIR *pDir = opendir(fixedPath.c_str());
    if (nullptr == pDir)
    {
        if ( errno == ENOTDIR )
        {
            LOGE("Path: %s is not a directory", fixedPath.c_str());
        }

        setErrorCode(errno);
        return false;
    }

    struct dirent *pDirEntry;
    struct stat64 itemStat;
    while ((pDirEntry = readdir(pDir)) != nullptr) {
        if (std::string(".") == pDirEntry->d_name || std::string("..") == pDirEntry->d_name) {
            continue;
        }

        std::string itemPath = fixedPath + pDirEntry->d_name;
        if (-1 == lstat64(itemPath.c_str(), &itemStat))
        {
            if (errno == EACCES) { // 获取信息没有权限
                LOGW("Path: %s no permission", itemPath.c_str());
                continue;
            } else if (errno == ENOENT) { // 悬空符号链接
                LOGW("Path: %s maybe dangling symbolic link", itemPath.c_str());
                continue;
            } else if (errno == ELOOP) { // 一个文件经过多重符号链接
                LOGW("Path: %s. Too many symbolic links encountered while traversing the path.", itemPath.c_str());
                continue;
            }

            setErrorCode(errno);
            closedir(pDir);
            return false;
        }

        if (S_ISDIR(itemStat.st_mode) && !S_ISLNK(itemStat.st_mode))
        {
            if (!_watchRecursive(infoList, itemPath, ev))
            {
                return false;
            }
        }
    }

    closedir(pDir);
    return true;
}

void InotifyTool::_parseEvent(ByteBuffer &inotifyEventBuf)
{
    // IN_MOVED_FROM事件中的文件/目录名
    const char *pMoveOut = nullptr;
    // IN_MOVED_FROM事件是对目录还是文件的操作
    bool moveOutItemIsDirFlag = false;
    // IN_MOVED_FROM事件的cookie
    uint32_t eventCookie = 0;

    int32_t movedSelfWd = 0;

    const struct inotify_event *pInoEvent = nullptr;

    size_t i = 0;
    // NOTE 未防止因read读取到不完整inotify_event产生越界行为, 需要在for条件中判断是否完整结构体
    for (; i < inotifyEventBuf.size() && (i + INOTIFY_EVENT_SIZE) < inotifyEventBuf.size(); i += (INOTIFY_EVENT_SIZE + pInoEvent->len))
    {
        pInoEvent = (const struct inotify_event *)(inotifyEventBuf.const_data() + i);

        DumpInotifyEvent(pInoEvent);

        InotifyEventItem eventItem;
        InotifyInfo evTemp = {pInoEvent->wd, 0, false};

        if (pInoEvent->mask & IN_Q_OVERFLOW)
        {
            eventItem.cookie = pInoEvent->cookie;
            eventItem.event = pInoEvent->mask;
            m_eventItemQueue.push_back(eventItem);

            continue;
        }

        if (pInoEvent->mask & IN_DELETE_SELF)
        {
            // 监视目录被删除需要解除监视并从映射表中移除
            LOGW("erase wd: %d for IN_DELETE_SELF", pInoEvent->wd);
            inotify_rm_watch(m_inotifyFd, pInoEvent->wd);
            continue;
        }

        // 监视已显式删除(inotify_rm_Watch)或自动删除(文件已删除或文件系统已卸载)
        if (pInoEvent->mask & IN_IGNORED)
        {
            LOGI("wd: %d Trigger IN_IGNORED event", pInoEvent->wd);
            m_pathWithWd.erase(evTemp);
            continue;
        }

        auto it = m_pathWithWd.find(evTemp);
        if (it == m_pathWithWd.end())
        {
            LOGE("-----------------------------------------------------------------");
            for (auto it = m_pathWithWd.begin(); it != m_pathWithWd.end(); ++it)
            {
                LOGD("wd: %d, path: %s", it.key().wd, it.value().c_str());
            }
            LOGE("-----------------------------------------------------------------");
        }
        // TODO 当从双向映射容器找不到wd时, 表示wd未插入到容器或未移除监视的情况下删除
        LOG_ASSERT(it != m_pathWithWd.end(), "wd(%d) not found", pInoEvent->wd);

        // 当前目录是否递归
        bool recursionFlag = it.key().recursion;

        // 当前操作的事件是否目录
        bool isDirFlag = false;

        // 监视目录取消挂载或被删除
        if (pInoEvent->mask & IN_UNMOUNT)
        {
            eventItem.cookie = pInoEvent->cookie;
            eventItem.event = pInoEvent->mask;
            std::string deletedItem;
            if (pInoEvent->len > 0)
            {
                deletedItem = pInoEvent->name;
            }
            eventItem.name = it.value() + deletedItem;
            m_eventItemQueue.push_back(eventItem);

            // 卸载磁盘或自身被删除需要解除监视
            LOGW("erase wd: %d for IN_UNMOUNT", pInoEvent->wd);
            inotify_rm_watch(m_inotifyFd, pInoEvent->wd);
            m_pathWithWd.erase(it);

            continue;
        }

        if (pInoEvent->len == 0)
        {
            // 无意义的事件
            continue;
        }

        // 表示当前是个目录
        if (pInoEvent->mask & IN_ISDIR)
        {
            isDirFlag = true;
            eventItem.event |= EV_IN_ISDIR;
        }

        if (pInoEvent->mask & IN_DELETE)
        {
            eventItem.cookie = pInoEvent->cookie;
            eventItem.event |= EV_IN_DELETE;
            eventItem.name = it.value() + pInoEvent->name;
            m_eventItemQueue.push_back(eventItem);

            continue;
        }

        // 文件/目录被创建
        if (pInoEvent->mask & IN_CREATE)
        {
            // 如果新建文件是目录, 并且当前父目录递归监视, 则将此目录加入到监视中
            if (isDirFlag && it.key().recursion)
            {
                uint32_t mask = EV_IN_ERROR;
                std::list<std::pair<std::string, InotifyInfo>> infoList;
                if (_watchRecursive(infoList, it.value() + pInoEvent->name, it.key().ev))
                {
                    for (const auto &it : infoList)
                    {
                        m_pathWithWd.insert(it.second, it.first);
                    }

                    mask = 0;
                }

                eventItem.event |= mask;
            }

            eventItem.cookie = pInoEvent->cookie;
            eventItem.event |= EV_IN_CREATE;
            eventItem.name = it.value() + pInoEvent->name;
            m_eventItemQueue.push_back(eventItem);

            continue;
        }

        // 文件被修改
        if (pInoEvent->mask & IN_MODIFY)
        {
            std::string filePath = it.value() + pInoEvent->name;
            auto modifyIt = m_fileModifySet.find(filePath);
            if (modifyIt == m_fileModifySet.end())
            {
                // 多次修改只记录一次
                m_fileModifySet.insert(filePath);
            }

            continue;
        }

        // 修改完毕, 将修改事件压入队列
        if ((pInoEvent->mask & IN_CLOSE_WRITE))
        {
            std::string filePath = it.value() + pInoEvent->name;
            auto modifyIt = m_fileModifySet.find(filePath);
            // 如果等于end表示文件以写方式打开, 但是并未修改文件后关闭
            if (modifyIt != m_fileModifySet.end())
            {
                eventItem.cookie = pInoEvent->cookie;
                eventItem.event |= EV_IN_MODIFY_OVER;
                eventItem.name = filePath;
                m_eventItemQueue.push_back(eventItem);

                m_fileModifySet.erase(modifyIt);
            }

            continue;
        }

        // 文件或目录被移动到其他位置
        if (pInoEvent->mask & IN_MOVED_FROM)
        {
            moveOutItemIsDirFlag = isDirFlag;
            pMoveOut = pInoEvent->name;
            eventCookie = pInoEvent->cookie;

            eventItem.cookie = pInoEvent->cookie;
            eventItem.event |= EV_IN_MOVED_OUT;
            eventItem.name = it.value() + pInoEvent->name;
            m_eventItemQueue.push_back(eventItem);

            // NOTE 考虑到对目录重命名会产生IN_MOVED_FROM事件, 故不在此处进行inotify的删除wd操作
            continue;
        }

        // 文件或目录从其他位置移动到被监视目录
        if (pInoEvent->mask & IN_MOVED_TO)
        {
            std::string parentPath = it.value();
            const char *pMoveIn = pInoEvent->name;

            // 对目录的重名操作, 需要更新旧的值
            if (eventCookie == pInoEvent->cookie && pMoveOut != nullptr && moveOutItemIsDirFlag)
            {
                auto tempBiMap = m_pathWithWd;

                for (auto bimapIt = tempBiMap.begin(); bimapIt != tempBiMap.end(); ++bimapIt)
                {
                    std::string moveOutPath = parentPath + pMoveOut;
                    if (std::string::npos != bimapIt.value().find(moveOutPath))
                    {
                        std::string oldPath = bimapIt.value();
                        utils::StringReplace(oldPath, moveOutPath, parentPath + pMoveIn);
                        bimapIt.update(oldPath);
                        continue;
                    }
                }

                m_pathWithWd = std::move(tempBiMap);

                pMoveOut = nullptr;
                moveOutItemIsDirFlag = false;
            }
            else if (isDirFlag)
            {
                InotifyInfo info = it.key();
                // 目录从其他位置移动到此处
                if (info.recursion)
                {
                    std::list<std::pair<std::string, InotifyInfo>> infoList;
                    if (_watchRecursive(infoList, parentPath + pMoveIn, info.ev))
                    {
                        for (const auto &it : infoList)
                        {
                            m_pathWithWd.insert(it.second, it.first);
                        }
                    }
                    else
                    {
                        eventItem.event |= EV_IN_ERROR;
                    }
                }
                else
                {
                    watchFile(parentPath + pMoveIn, info.ev);
                }
            }

            eventItem.cookie = pInoEvent->cookie;
            eventItem.event |= EV_IN_MOVED_IN;
            eventItem.name = parentPath + pMoveIn;
            m_eventItemQueue.push_back(eventItem);
        }

        // NOTE IN_MOVED_FROM和IN_MOVED_TO是连续事件, 中间不会被其他事件隔开
        // 有IN_MOVED_FROM而没有IN_MOVED_TO事件
        if (pMoveOut != nullptr && moveOutItemIsDirFlag)
        {
            auto tempBiMap = m_pathWithWd;

            // 目录被移走, 则停止监视目录
            std::string path = it.value() + pMoveOut;
            for (auto bimapIt = tempBiMap.begin(); bimapIt != tempBiMap.end(); )
            {
                if (bimapIt.value().find(path))
                {
                    inotify_rm_watch(m_inotifyFd, bimapIt.key().wd);
                    bimapIt = tempBiMap.erase(bimapIt);
                    continue;
                }

                ++bimapIt;
            }

            m_pathWithWd = std::move(tempBiMap);
            // NOTE 此时迭代器it已失效, 无法继续使用

            pMoveOut = nullptr;
            moveOutItemIsDirFlag = false;
        }
    }

    // 保留剩余数据
    if (i < inotifyEventBuf.size())
    {
        const uint8_t *pBegin = inotifyEventBuf.const_data() + i;
        size_t copySize = inotifyEventBuf.size() - i;
        inotifyEventBuf.set(pBegin, copySize);
    }
    else
    {
        inotifyEventBuf.clear();
    }
}

void InotifyTool::_disassembleU32(uint32_t flag, uint32_t vec[U32_BITS])
{
    for (int32_t i = 0; i < 32; ++i)
    {
        uint32_t temp = 0x01 << i;
        if (flag & temp)
        {
            vec[i] = temp;
        }
    }
}

void DumpInotifyEvent(const struct inotify_event *ev)
{
    std::string opTotal = "";

    // Supported events suitable for MASK parameter of INOTIFY_ADD_WATCH. 
    if (ev->mask & IN_ACCESS)           opTotal += "IN_ACCESS | ";
    if (ev->mask & IN_MODIFY)           opTotal += "IN_MODIFY | ";
    if (ev->mask & IN_ATTRIB)           opTotal += "IN_ATTRIB | ";
    if (ev->mask & IN_CLOSE_WRITE)      opTotal += "IN_CLOSE_WRITE | ";
    if (ev->mask & IN_CLOSE_NOWRITE)    opTotal += "IN_CLOSE_NOWRITE | ";
    if (ev->mask & IN_OPEN)             opTotal += "IN_OPEN | ";
    if (ev->mask & IN_MOVED_FROM)       opTotal += "IN_MOVED_FROM | ";
    if (ev->mask & IN_MOVED_TO)         opTotal += "IN_MOVED_TO | ";
    if (ev->mask & IN_CREATE)           opTotal += "IN_CREATE | ";
    if (ev->mask & IN_DELETE)           opTotal += "IN_DELETE | ";
    if (ev->mask & IN_DELETE_SELF)      opTotal += "IN_DELETE_SELF | ";
    if (ev->mask & IN_MOVE_SELF)        opTotal += "IN_MOVE_SELF | ";

    // Events sent by the kernel.
    if (ev->mask & IN_UNMOUNT)          opTotal += "IN_UNMOUNT | ";
    if (ev->mask & IN_Q_OVERFLOW)       opTotal += "IN_Q_OVERFLOW | ";
    if (ev->mask & IN_IGNORED)          opTotal += "IN_IGNORED | ";

    // Special flags
    if (ev->mask & IN_ONLYDIR)          opTotal += "IN_ONLYDIR | ";
    if (ev->mask & IN_DONT_FOLLOW)      opTotal += "IN_DONT_FOLLOW | ";
    if (ev->mask & IN_EXCL_UNLINK)      opTotal += "IN_EXCL_UNLINK | ";
    if (ev->mask & IN_MASK_ADD)         opTotal += "IN_MASK_ADD | ";
    if (ev->mask & IN_ISDIR)            opTotal += "IN_ISDIR | ";
    if (ev->mask & IN_ONESHOT)          opTotal += "IN_ONESHOT | ";

    std::string temp(opTotal.c_str(), opTotal.length() - 3);
    LOGD("wd: %d, mask: %#x(%s), cookie: %#x, len: %u, name: %s\n",
        ev->wd, ev->mask, temp.c_str(), ev->cookie, ev->len, ev->len > 0 ? ev->name : "(null)");
}

} // namespace eular

std::string Event2String(uint32_t ev)
{
    std::stringstream ss;

#define INOTIFY_MAP(XXX)                                        \
    XXX(InotifyEvent::EV_IN_MODIFY_OVER, IN_MODIFY)             \
    XXX(InotifyEvent::EV_IN_CLOSE_WRITE, IN_CLOSE_WRITE)        \
    XXX(InotifyEvent::EV_IN_MOVED_OUT, IN_MOVED_FROM)           \
    XXX(InotifyEvent::EV_IN_MOVED_IN, IN_MOVED_TO)              \
    XXX(InotifyEvent::EV_IN_CREATE, IN_CREATE)                  \
    XXX(InotifyEvent::EV_IN_DELETE, IN_DELETE)                  \
    XXX(InotifyEvent::EV_IN_ERROR, IN_ONESHOT)                  \
    XXX(InotifyEvent::EV_IN_UNMOUNT, IN_UNMOUNT)                \
    XXX(InotifyEvent::EV_IN_IGNORED, IN_IGNORED)                \
    XXX(InotifyEvent::EV_IN_ISDIR, IN_ISDIR)                    \

    const char *orStr = " | ";

#define XXX(InoEv, realInoEv)   \
    if ((InoEv) & ev)           \
    {                           \
        ss << (#InoEv);         \
        ss << orStr;            \
    }                           \

    INOTIFY_MAP(XXX);

#undef XXX
#undef INOTIFY_MAP

    std::string temp = ss.str();
    return temp.substr(0, temp.length() - strlen(orStr));
}

