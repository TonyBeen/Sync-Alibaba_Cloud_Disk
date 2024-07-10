/*************************************************************************
    > File Name: http_handler.cpp
    > Author: hsz
    > Brief:
    > Created Time: 2024年05月01日 星期三 18时31分22秒
 ************************************************************************/

#include "httpd/http_handler.h"

#include <log/log.h>
#include "http_handler.h"

#define LOG_TAG "HttpHandler"

int32_t eular::HttpHandler::Auth(HttpRequest *req, HttpResponse *resp)
{
    resp->SetBody(req->Dump());

    return HTTP_STATUS_OK;
}

int32_t eular::HttpHandler::Login(HttpRequest *req, HttpResponse *resp)
{
    return 0;
}

int32_t eular::HttpHandler::Makefile(HttpRequest *req, HttpResponse *resp)
{
    HFile file;
    if (file.open("./Makefile", "r") != 0) {
        LOGI("Makefile not found");
        return HTTP_STATUS_NOT_FOUND;
    }

    std::string fileContent;
    file.readall(fileContent);
    resp->SetBody(fileContent);

    return HTTP_STATUS_OK;
}