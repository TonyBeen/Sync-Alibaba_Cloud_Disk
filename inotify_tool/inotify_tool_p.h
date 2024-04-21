/*************************************************************************
    > File Name: inotify_p.h
    > Author: hsz
    > Brief:
    > Created Time: 2024年04月21日 星期日 13时22分06秒
 ************************************************************************/

#ifndef __INOTIFY_TOOL_P_H__
#define __INOTIFY_TOOL_P_H__

#include <stdint.h>
#include <unordered_map>

struct InotifyInfo
{
    int32_t wd;     // 监视描述符
    uint32_t ev;    // 事件
    bool recursion; // 是否递归监视子目录, 只对目录有效

    bool operator==(const InotifyInfo& other) const
    {
        return (wd == other.wd);
    }
};

template<>
struct std::hash<InotifyInfo> : public std::__hash_base<size_t, InotifyInfo>
{
    size_t operator()(InotifyInfo val) const noexcept
    {
        return static_cast<size_t>(val.wd);
    }
};

#endif // __INOTIFY_TOOL_P_H__
