/*************************************************************************
    > File Name: http_router.h
    > Author: hsz
    > Brief:
    > Created Time: 2024年05月01日 星期三 18时32分07秒
 ************************************************************************/

#ifndef __HTTP_HTTP_ROUTER_H__
#define __HTTP_HTTP_ROUTER_H__

#include <hv/HttpService.h>

namespace eular {
class HttpRouter
{
public:
    HttpRouter() = default;
    ~HttpRouter() = default;

    static void Register(hv::HttpService &router);
};

} // namespace eular

#endif // __HTTP_HTTP_ROUTER_H__