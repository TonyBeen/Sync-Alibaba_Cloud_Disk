/*************************************************************************
    > File Name: inotify_tool.h
    > Author: hsz
    > Brief:
    > Created Time: 2024年04月13日 星期六 10时17分28秒
 ************************************************************************/

#ifndef __INOTIFY_TOOL_H__
#define __INOTIFY_TOOL_H__

#include <string>
#include <list>
#include <vector>
#include <map>
#include <memory>

#include <utils/bimap.h>
#include <utils/buffer.h>

#define U32_BITS (sizeof(uint32_t) * __CHAR_BIT__)

struct inotify_event;
struct InotifyEventItem;
struct InotifyInfo;

#include "inotify_tool/inotify_tool_p.h"

namespace eular {

class InotifyTool
{
public:
    typedef std::shared_ptr<InotifyTool> SP;
    typedef std::unique_ptr<InotifyTool> Ptr;

    InotifyTool() noexcept;
    ~InotifyTool() noexcept;

    /**
     * @brief 创建一个inotify句柄
     * 
     * @return true 成功
     * @return false 失败
     */
    bool createInotify() noexcept;

    /**
     * @brief 销毁Inotify
     * 
     */
    void destroyInotify();

    /**
     * @brief 监控一个文件/目录
     * 
     * @param fileName 绝对路径的文件/目录
     * @param ev 事件
     * @return int32_t 成功返回0, 失败返回负值
     */
    int32_t watchFile(const std::string &fileName, uint32_t ev);

    /**
     * @brief 监控多个文件/目录
     * 
     * @param fileNames 绝对路径文件/目录的数组
     * @param ev 事件
     * @return int32_t 成功返回0, 失败返回负值
     */
    int32_t watchFiles(const std::vector<std::string> &fileNames, uint32_t ev);

    /**
     * @brief 递归监视子目录
     * 
     * @param path 目录路径(绝对路径)
     * @param ev 事件
     * @return int32_t 成功返回0, 失败返回负值
     */
    int32_t watchRecursive(const std::string &path, uint32_t ev);

    /**
     * @brief 递归监视子目录
     * 
     * @param paths 目录路径(绝对路径)数组
     * @param ev 事件
     * @return int32_t 成功返回0, 失败返回负值
     */
    int32_t watchRecursive(const std::vector<std::string> &paths, uint32_t ev);

    /**
     * @brief 
     * 
     * @param path 
     */
    void removeWatch(const std::string &path);

    /**
     * @brief 等待一个完整事件
     * 
     * @param timeout 超时时间
     * @return int32_t 成功返回0, 超时返回TIMED_OUT, 失败返回其他负值
     */
    int32_t waitCompleteEvent(uint32_t timeout);

    /**
     * @brief 获取产生的事件
     * 
     * @param eventItemVec 
     */
    void getEventItem(std::list<InotifyEventItem> &eventItemVec);

    /**
     * @brief 获取错误码
     * 
     * @return int32_t 
     */
    int32_t getLastError();

    /**
     * @brief 错误码转成字符串
     * 
     * @param code 
     * @return std::string 
     */
    std::string errorMsg(int32_t code);

protected:
    /**
     * @brief InotifyEvent事件转成inotify 识别的事件
     * 
     * @param ev 
     * @return uint32_t 
     */
    uint32_t inotifyEvent2RealInotifyEv(uint32_t ev) const noexcept;

    /**
     * @brief inotify事件转成InotifyEvent
     * 
     * @param ev 
     * @return uint32_t 
     */
    uint32_t realInotifyEv2InotifyEvent(uint32_t ev) const noexcept;

    /**
     * @brief 是否目录
     * 
     * @param path 路径
     * @return true 目录
     * @return false 非目录
     */
    bool isDir(const std::string &path) const;

    /**
     * @brief 设置错误码
     * 
     * @param code 错误码
     */
    void setErrorCode(int32_t code = 0);

    /**
     * @brief 递归监视目录
     * 
     * @param infoList 信息
     * @param path 监控路径
     * @param ev 事件
     * @return true 成功
     * @return false 失败
     */
    bool _watchRecursive(std::list<std::pair<std::string, InotifyInfo>> &infoList,
                         const std::string &path, uint32_t ev);

    void _parseEvent(ByteBuffer &inotifyEventBuf);

    void _disassembleU32(uint32_t flag, uint32_t vec[U32_BITS]);

private:
    bool            m_recursion;     // 是否递归监控子目录
    int32_t         m_inotifyFd;     // Inotify文件描述符
    int32_t         m_errorCode;     // 错误码
    ByteBuffer      m_inotifyBuffer; // inotify缓冲区
    std::list<InotifyEventItem>     m_eventItemVec; // 事件队列
    BiMap<InotifyInfo, std::string> m_pathWithWd;   // 目录/文件和wd句柄的双向映射
};

} // namespace eular

#endif // __INOTIFY_TOOL_H__
