/*************************************************************************
    > File Name: http_handler.cpp
    > Author: hsz
    > Brief:
    > Created Time: 2024年05月01日 星期三 18时31分22秒
 ************************************************************************/

#include "httpd/http_handler.h"

#include <log/log.h>
#include <hv/requests.h>
#include <hv/axios.h>

#include <config/YamlConfig.h>

#define LOG_TAG "HttpHandler"

#define OPENAPI_DOMAIN_NAME     "https://openapi.alipan.com"
#define OPENAPI_ACCESS_TOKEN    "/oauth/access_token"               // POST
#define OPENAPI_USER_INFO       "/oauth/users/info"                 // GET
#define OPENAPI_DRIVE_INFO      "/adrive/v1.0/user/getDriveInfo"    // POST
#define OPENAPI_FILE_LIST       "/adrive/v1.0/openFile/list"        // POST

int32_t eular::HttpHandler::Auth(HttpRequest *req, HttpResponse *resp)
{
    // 1、根据URL回调获取 code
    std::string code;
    auto it = req->query_params.find("code");
    if (it == req->query_params.end()) {
        resp->SetBody("Your eggs are so big!");
        return HTTP_STATUS_OK;
    }

    code = it->second;
    LOGI("code = %s", code.c_str());

    std::string postJsonBody;
    try {
        hv::Json jsonObj;
        jsonObj["client_id"] = YamlReaderInstance::Get()->lookup<std::string>("http.app_id");
        jsonObj["client_secret"] = YamlReaderInstance::Get()->lookup<std::string>("http.app_secret");
        jsonObj["grant_type"] = "authorization_code";
        jsonObj["code"] = code;

        postJsonBody = jsonObj.dump();
    } catch(const std::exception& e) {
        LOGE("catch exception: %s", e.what());
    }

    if (postJsonBody.empty()) {
        return HTTP_STATUS_INTERNAL_SERVER_ERROR;
    }

    // 2、获取 access_token
    http_headers headers;
    headers["Content-Type"] = "application/json";
    auto postResp = requests::post(OPENAPI_DOMAIN_NAME OPENAPI_ACCESS_TOKEN, postJsonBody, headers);
    if (postResp == nullptr) {
        LOGE("request failed!\n");
        return HTTP_STATUS_INTERNAL_SERVER_ERROR;
    }

    LOGI("POST [" OPENAPI_DOMAIN_NAME OPENAPI_ACCESS_TOKEN "] => Response %d %s\r\n", postResp->status_code, postResp->status_message());
    hv::Json jsonResponse;
    try {
        jsonResponse = hv::Json::parse(postResp->body);
        LOGI("POST " OPENAPI_DOMAIN_NAME OPENAPI_ACCESS_TOKEN "-> Response: %s", postResp->body.c_str());
        if (postResp->status_code == HTTP_STATUS_OK) {
            std::string tokenType = jsonResponse.at("token_type");
            std::string accessToken = jsonResponse.at("access_token");
            std::string refreshToken = jsonResponse.at("refresh_token");
            uint32_t expireTime = jsonResponse.at("expires_in");

            LOGI("tokenType = %s", tokenType.c_str());
            LOGI("access_token = %s", accessToken.c_str());
            LOGI("refreshToken = %s", refreshToken.c_str());
            LOGI("expireTime = %u", expireTime);

            // 3、获取用户信息
            http_headers userInfoReq;
            userInfoReq["Authorization"] = tokenType + " " + accessToken;
            auto userInfoResp = requests::get(OPENAPI_DOMAIN_NAME OPENAPI_USER_INFO, userInfoReq);
            LOGI("GET [" OPENAPI_DOMAIN_NAME OPENAPI_USER_INFO "] => Response %d %s\r\n", userInfoResp->status_code, userInfoResp->status_message());
            if (userInfoResp->status_code == HTTP_STATUS_OK) {
                try {
                    jsonResponse = hv::Json::parse(userInfoResp->body);
                    if (jsonResponse.type() == nlohmann::json::value_t::object) {
                        std::string id = jsonResponse.at("id");
                        std::string name = jsonResponse.at("name");
                        std::string avatar = jsonResponse.at("avatar");
                        std::string phoneNo;
                        if (jsonResponse.at("phone").type() != nlohmann::json::value_t::null) {
                            phoneNo = jsonResponse.at("phone");
                        }
                        LOGI("Object -> id: %s, name: %s, avatar: %s, phone: '%s'", id.c_str(), name.c_str(), avatar.c_str(), phoneNo.c_str());
                    }
                } catch(const std::exception& e) {
                    LOGE("Invalid Json Body: [%s]: %s", userInfoResp->body.c_str(), e.what());
                    return HTTP_STATUS_INTERNAL_SERVER_ERROR;
                }
            }

            // 4、获取 drive 信息
            auto driveInfoResp = requests::post(OPENAPI_DOMAIN_NAME OPENAPI_DRIVE_INFO, NoBody, userInfoReq);
            LOGI("POST [" OPENAPI_DOMAIN_NAME OPENAPI_DRIVE_INFO "] => Response %d %s\r\n", driveInfoResp->status_code, driveInfoResp->status_message());
            if (driveInfoResp->status_code == HTTP_STATUS_OK) {
                std::string default_drive_id;
                try {
                    jsonResponse = hv::Json::parse(driveInfoResp->body);
                    if (jsonResponse.type() == nlohmann::json::value_t::object) {
                        const std::string &user_id = jsonResponse.at("user_id");
                        default_drive_id = jsonResponse.at("default_drive_id");
                        std::string resource_drive_id;
                        std::string backup_drive_id;
                        if (jsonResponse.contains("resource_drive_id")) {
                            resource_drive_id = jsonResponse.at("resource_drive_id");
                        }
                        if (jsonResponse.contains("backup_drive_id")) {
                            backup_drive_id = jsonResponse.at("backup_drive_id");
                        }

                        LOGI("user_id: %s, default_drive_id: %s, resource_drive_id: %s, backup_drive_id: %s",
                            user_id.c_str(), default_drive_id.c_str(), resource_drive_id.c_str(), backup_drive_id.c_str());
                    }
                } catch(const std::exception& e) {
                    LOGE("Invalid Json Body: [%s]: %s", driveInfoResp->body.c_str(), e.what());
                    return HTTP_STATUS_INTERNAL_SERVER_ERROR;
                }

                // 5、获取根目录文件列表
                http_headers fileListReqHeader;
                fileListReqHeader["Authorization"] = tokenType + " " + accessToken;
                fileListReqHeader["Content-Type"] = "application/json";

                nlohmann::json fileListReqBody;
                fileListReqBody["drive_id"] = default_drive_id;
                fileListReqBody["parent_file_id"] = "root";
                auto fileListResp = requests::post(OPENAPI_DOMAIN_NAME OPENAPI_FILE_LIST, fileListReqBody.dump(), fileListReqHeader);
                if (fileListResp->status_code == HTTP_STATUS_OK) {
                    resp->SetBody(fileListResp->body);
                    resp->SetContentType("application/json");
                }
            }

            // resp->SetBody(driveInfoResp->body);
            // resp->SetContentType("application/json");
        } else {
            resp->SetBody(postResp->body);
            std::string code = jsonResponse.at("code");
            std::string message = jsonResponse.at("message");
            LOGW("code: %s, message: %s", code.c_str(), message.c_str());
        }
    } catch(const std::exception& e) {
        LOGE("Invalid Json Body: [%s]: %s", postResp->body.c_str(), e.what());
        return HTTP_STATUS_INTERNAL_SERVER_ERROR;
    }

    return HTTP_STATUS_OK;
}

int32_t eular::HttpHandler::Login(HttpRequest *req, HttpResponse *resp)
{
    return HTTP_STATUS_OK;
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