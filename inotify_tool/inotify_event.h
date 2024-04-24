/*************************************************************************
    > File Name: inotify_event.h
    > Author: hsz
    > Brief:
    > Created Time: 2024年04月13日 星期六 10时51分26秒
 ************************************************************************/

#ifndef __INOTIFY_EVENT_H__
#define __INOTIFY_EVENT_H__

#include <stdint.h>
#include <string>

#define EV_IN_MOVE (InotifyEvent::EV_IN_MOVED_OUT | InotifyEvent::EV_IN_MOVED_IN)
#define EV_IN_ALL (                     \
    InotifyEvent::EV_IN_MODIFY_OVER |   \
    InotifyEvent::EV_IN_CLOSE_WRITE |   \
    InotifyEvent::EV_IN_MOVED_OUT |     \
    InotifyEvent::EV_IN_MOVED_IN |      \
    InotifyEvent::EV_IN_CREATE |        \
    InotifyEvent::EV_IN_DELETE)


enum InotifyEvent {
    EV_IN_NONE              = 0x00,         // 无事件
    EV_IN_MODIFY_OVER       = 0x02,         // 文件被修改, 例如write, truncate
    EV_IN_CLOSE_WRITE       = 0x08,         // 关闭以写方式打开的文件
    EV_IN_MOVED_OUT         = 0x40,         // 文件或目录从监视的目录中移走, 例如移动文件/目录到非监视目录, 重命名文件/目录
    EV_IN_MOVED_IN          = 0x80,         // 文件/目录从其他目录移动到监视目录
    EV_IN_CREATE            = 0x100,        // 在监视目录中创建文件或目录, 例如open(O_CREAT), mkdir, link, symlink, bind on a UNIX domain socket
    EV_IN_DELETE            = 0x200,        // 文件或目录从监视目录中删除

    // 特殊标志, 无需主动带上
    EV_IN_ERROR             = 0x1000,       // 处理事件过程中出现问题
    EV_IN_UNMOUNT           = 0x2000,       // 监视的目录被卸载
    EV_IN_IGNORED           = 0x8000,       // 监视的文件/目录已被删除
    EV_IN_ISDIR             = 0x40000000,   // 操作的是个目录
};

struct InotifyEventItem
{
    uint32_t        event = 0;  // 产生的事件
    uint32_t        cookie = 0; // 关联两个事件
    std::string     name;       // 发生事件的名称
};


std::string Event2String(uint32_t ev);

#endif // __INOTIFY_EVENT_H__
