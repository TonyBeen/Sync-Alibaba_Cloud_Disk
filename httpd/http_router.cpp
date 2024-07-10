/*************************************************************************
    > File Name: http_router.cpp
    > Author: hsz
    > Brief:
    > Created Time: 2024年05月01日 星期三 18时32分10秒
 ************************************************************************/

#include "httpd/http_router.h"
#include "httpd/http_handler.h"

namespace eular {
void HttpRouter::Register(hv::HttpService &router)
{
    router.GET("/auth", &HttpHandler::Auth);
    router.GET("/Makefile", &HttpHandler::Makefile);
}
} // namespace eular
