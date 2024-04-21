/*************************************************************************
    > File Name: inotify_tool.cpp
    > Author: hsz
    > Brief:
    > Created Time: 2024年04月13日 星期六 10时17分31秒
 ************************************************************************/

#include <sstream>

#include <utils/sysdef.h>
#include <utils/errors.h>
#include <log/log.h>

#ifdef OS_LINUX
#include <sys/inotify.h>
#include <sys/types.h>
#include <dirent.h>
#include <sys/ioctl.h>
#include <sys/select.h>

#define INOTIFY_EVENT_SIZE  (sizeof(struct inotify_event))
#define MAX_BUF_SIZE 8192
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
    m_errorCode(0)
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
        int32_t wd = inotify_add_watch(m_inotifyFd, fileNames[i].c_str(), realEv);
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
        m_pathWithWd.insert(fileNames[i], info);
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
        if (!_watchRecursive(infoList, paths[i], realEv))
        {
            return UNKNOWN_ERROR;
        }

        for (const auto &it : infoList)
        {
            m_pathWithWd.insert(it.first, it.second);
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
    do
    {
        errorCode = ioctl(m_inotifyFd, FIONREAD, &bytesToRead);
    } while (!errorCode && bytesToRead < sizeof(struct inotify_event));
    if (errorCode < 0)
    {
        setErrorCode(errno);
        return UNKNOWN_ERROR;
    }

    char eventBuffer[MAX_BUF_SIZE];
    do {
        ssize_t readSize = ::read(m_inotifyFd, eventBuffer, MAX_BUF_SIZE);
        if (readSize < 0)
        {
            setErrorCode(errno);
            return UNKNOWN_ERROR;
        }

        // 读到结尾
        if (readSize == 0)
        {
            break;
        }

        ssize_t i = 0;
        while (i < readSize)
        {
            struct inotify_event *event = (struct inotify_event *)&eventBuffer[i];
            DumpInotifyEvent(event);

            i += (INOTIFY_EVENT_SIZE + event->len);
        }
    } while (true);

    return NO_ERROR;
}

void InotifyTool::getEventItem(std::vector<InotifyEventItem> &eventItemVec)
{
    eventItemVec = std::move(m_eventItemVec);
    m_eventItemVec.clear();
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
    XXX(InotifyEvent::EV_IN_ACCESS, IN_ACCESS)                  \
    XXX(InotifyEvent::EV_IN_MODIFY, IN_MODIFY)                  \
    XXX(InotifyEvent::EV_IN_ATTRIB, IN_ATTRIB)                  \
    XXX(InotifyEvent::EV_IN_CLOSE_WRITE, IN_CLOSE_WRITE)        \
    XXX(InotifyEvent::EV_IN_CLOSE_NOWRITE, IN_CLOSE_NOWRITE)    \
    XXX(InotifyEvent::EV_IN_OPEN, IN_OPEN)                      \
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
    XXX(InotifyEvent::EV_IN_ACCESS, IN_ACCESS)                  \
    XXX(InotifyEvent::EV_IN_MODIFY, IN_MODIFY)                  \
    XXX(InotifyEvent::EV_IN_ATTRIB, IN_ATTRIB)                  \
    XXX(InotifyEvent::EV_IN_CLOSE_WRITE, IN_CLOSE_WRITE)        \
    XXX(InotifyEvent::EV_IN_CLOSE_NOWRITE, IN_CLOSE_NOWRITE)    \
    XXX(InotifyEvent::EV_IN_OPEN, IN_OPEN)                      \
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
            LOGD("Stat failed on %s: %s\n", path, strerror(errno));
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

    int32_t wd = inotify_add_watch(m_inotifyFd, fixedPath.c_str(), ev);
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
    if (ev->len > 0)
    {
        LOGD("wd: %d, mask: %#x(%s), cookie: %#x, len: %u, name: %s\n",
            ev->wd, ev->mask, temp.c_str(), ev->cookie, ev->len, ev->len > 0 ? ev->name : "(null)");
    }
}

void InotifyTool::_parseEvent(inotify_event *pInotifyEv)
{

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

} // namespace eular

std::string Event2String(uint32_t ev)
{
    std::stringstream ss;

#define INOTIFY_MAP(XXX)                                        \
    XXX(InotifyEvent::EV_IN_ACCESS, IN_ACCESS)                  \
    XXX(InotifyEvent::EV_IN_MODIFY, IN_MODIFY)                  \
    XXX(InotifyEvent::EV_IN_ATTRIB, IN_ATTRIB)                  \
    XXX(InotifyEvent::EV_IN_CLOSE_WRITE, IN_CLOSE_WRITE)        \
    XXX(InotifyEvent::EV_IN_CLOSE_NOWRITE, IN_CLOSE_NOWRITE)    \
    XXX(InotifyEvent::EV_IN_OPEN, IN_OPEN)                      \
    XXX(InotifyEvent::EV_IN_MOVED_OUT, IN_MOVED_FROM)           \
    XXX(InotifyEvent::EV_IN_MOVED_IN, IN_MOVED_TO)              \
    XXX(InotifyEvent::EV_IN_CREATE, IN_CREATE)                  \
    XXX(InotifyEvent::EV_IN_DELETE, IN_DELETE)                  \
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

