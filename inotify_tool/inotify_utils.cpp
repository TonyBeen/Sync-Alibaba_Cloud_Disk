/*************************************************************************
    > File Name: inotify_utils.cpp
    > Author: hsz
    > Brief:
    > Created Time: 2024年04月20日 星期六 16时48分13秒
 ************************************************************************/

#include <algorithm>

#include "inotify_tool/inotify_utils.h"
#include "inotify_utils.h"

#include <utils/sysdef.h>

namespace eular {
namespace utils {

void CorrectionPath(std::string &item)
{
    if (item.empty())
    {
        return;
    }

#ifdef OS_LINUX
    item.push_back('/');
    auto new_end = std::unique(item.begin(), item.end(), [](char a, char b) {
        return a == '/' && b == '/';
    });

    item.erase(new_end, item.end());
#endif
}

void StringReplace(std::string &opStr, const std::string &from, const std::string &to)
{
    size_t pos = 0;
    pos = opStr.find(from);
    if (pos != std::string::npos)
    {
        opStr.replace(pos, from.length(), to);
    }
}

} // namespace utils
} // namespace eular

