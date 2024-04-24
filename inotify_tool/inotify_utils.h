/*************************************************************************
    > File Name: inotify_utils.h
    > Author: hsz
    > Brief:
    > Created Time: 2024年04月20日 星期六 16时48分10秒
 ************************************************************************/

#ifndef __INOTIFY_TOOL_UTILS_H__
#define __INOTIFY_TOOL_UTILS_H__

#include <string>

namespace eular {
namespace utils {

void CorrectionPath(std::string &item);

/**
 * @brief 修改opStr, 将from替换为to
 * 
 * @param opStr 
 * @param from 
 * @param to 
 */
void StringReplace(std::string &opStr, const std::string &from, const std::string &to);

} // namespace utils
} // namespace eular

#endif // __INOTIFY_TOOL_UTILS_H__