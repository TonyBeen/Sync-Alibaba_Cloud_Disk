/*************************************************************************
    > File Name: http_handler.h
    > Author: hsz
    > Brief:
    > Created Time: 2024年05月01日 星期三 18时31分19秒
 ************************************************************************/

#ifndef __HTTP_HTTP_HANDLER_H__
#define __HTTP_HTTP_HANDLER_H__

#include <stdint.h>

namespace eular {
class HttpHandler
{
public:
    HttpHandler() = default;
    ~HttpHandler() = default;

    static int32_t GetRoot();
};

} // namespace eular

#endif // __HTTP_HTTP_HANDLER_H__
