/*************************************************************************
    > File Name: http_handler.h
    > Author: hsz
    > Brief:
    > Created Time: 2024年05月01日 星期三 18时31分19秒
 ************************************************************************/

#ifndef __HTTP_HTTP_HANDLER_H__
#define __HTTP_HTTP_HANDLER_H__

#include <stdint.h>

#include <hv/HttpService.h>

namespace eular {
class HttpHandler
{
public:
    HttpHandler() = default;
    ~HttpHandler() = default;

    /**
     * @brief 校验操作
     * 
     * @param req 
     * @param resp 
     * @return int32_t 
     */
    static int32_t Auth(HttpRequest* req, HttpResponse* resp);

    static int32_t Index(HttpRequest* req, HttpResponse* resp);

    static int32_t Makefile(HttpRequest *req, HttpResponse *resp);

    static int32_t Logout(HttpRequest *req, HttpResponse *resp);

protected:
    // 构造登录成功页面
    static bool Login(std::string &html);
    // 刷新 token
    static void UpdateToken();
};

} // namespace eular

#endif // __HTTP_HTTP_HANDLER_H__
