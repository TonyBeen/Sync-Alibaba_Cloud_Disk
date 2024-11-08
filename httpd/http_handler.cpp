 /*************************************************************************
    > File Name: http_handler.cpp
    > Author: hsz
    > Brief:
    > Created Time: 2024年05月01日 星期三 18时31分22秒
 ************************************************************************/

#include "httpd/http_handler.h"

#include <filesystem>

#include <hv/requests.h>
#include <hv/axios.h>
#include <hv/hv.h>

#include <config/YamlConfig.h>
#include <log/log.h>

#include "global_resource_management.h"
#include "http_handler.h"
#include "application.h"
#include "api_config.h"
#include "thread_pool.h"

#define LOG_TAG "HttpHandler"

#define EXPIRE_TIME(x) ((x) * 7 / 8)

int32_t eular::HttpHandler::Auth(HttpRequest *req, HttpResponse *resp)
{
    // 已登录后无需重复登录
    if (GlobalResourceInstance::Get()->logged_in) {
        std::string html;
        if (!Login(html)) {
            return HTTP_STATUS_NOT_FOUND;
        }

        resp->SetContentType(http_content_type_str(http_content_type::TEXT_HTML));
        resp->SetBody(html);
        return HTTP_STATUS_OK;
    }

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

            GlobalResourceInstance::Get()->refresh_token = refreshToken;
            GlobalResourceInstance::Get()->token_type = tokenType;
            GlobalResourceInstance::Get()->token = accessToken;
            GlobalResourceInstance::Get()->token_expire = expireTime;

            AppInstance::Get()->m_hvLoop->setTimeout(EXPIRE_TIME(expireTime) * 1000, [] (hv::TimerID) {
                HttpHandler::UpdateToken();
            });

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
                        GlobalResourceInstance::Get()->image_url = avatar;
                        GlobalResourceInstance::Get()->name = name;
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
                            GlobalResourceInstance::Get()->resource_drive_id = resource_drive_id;
                        }
                        if (jsonResponse.contains("backup_drive_id")) {
                            backup_drive_id = jsonResponse.at("backup_drive_id");
                            GlobalResourceInstance::Get()->backup_drive_id = backup_drive_id;
                        }

                        GlobalResourceInstance::Get()->default_drive_id = default_drive_id;

                        LOGI("user_id: %s, default_drive_id: %s, resource_drive_id: %s, backup_drive_id: %s",
                            user_id.c_str(), default_drive_id.c_str(), resource_drive_id.c_str(), backup_drive_id.c_str());
                    }
                } catch(const std::exception& e) {
                    String8 msg = String8::format("Invalid Json Body: %s, \n%s", e.what(), driveInfoResp->body.c_str());
                    resp->SetBody(msg.toStdString());
                    return HTTP_STATUS_INTERNAL_SERVER_ERROR;
                }

            //     // 5、获取根目录文件列表
            //     http_headers fileListReqHeader;
            //     fileListReqHeader["Authorization"] = tokenType + " " + accessToken;
            //     fileListReqHeader["Content-Type"] = "application/json";

            //     nlohmann::json fileListReqBody;
            //     fileListReqBody["drive_id"] = default_drive_id;
            //     fileListReqBody["limit"] = 100;
            //     fileListReqBody["parent_file_id"] = "root";
            //     auto fileListResp = requests::post(OPENAPI_DOMAIN_NAME OPENAPI_FILE_LIST, fileListReqBody.dump(), fileListReqHeader);
            //     LOGI("POST [" OPENAPI_DOMAIN_NAME OPENAPI_FILE_LIST "] => Response %d %s\r\n", fileListResp->status_code, fileListResp->status_message());
            //     if (fileListResp->status_code == HTTP_STATUS_OK) {
            //         try
            //         {
            //             nlohmann::json fileJson = nlohmann::json::parse(fileListResp->body);
            //             if (fileJson.contains("items")) {
            //                 nlohmann::json fileItemJsonArray = fileJson.at("items");
            //                 LOG_ASSERT(fileItemJsonArray.is_array(), "fileItemJsonArray.type = %s", fileItemJsonArray.type_name());
            //                 for (const auto &fileItem : fileItemJsonArray) {
            //                     if (fileItem.at("type") == "folder") {
            //                         fileListReqBody["parent_file_id"] = fileItem["file_id"];
            //                         break;
            //                     }
            //                 }

            //                 fileListResp = requests::post(OPENAPI_DOMAIN_NAME OPENAPI_FILE_LIST, fileListReqBody.dump(), fileListReqHeader);
            //                 LOGI("POST [" OPENAPI_DOMAIN_NAME OPENAPI_FILE_LIST "] => Response %d %s\r\n", fileListResp->status_code, fileListResp->status_message());
            //             }
            //         }
            //         catch(const std::exception& e)
            //         {
            //             LOGE("************************** %s", e.what());
            //         }
            //     }

            //     resp->SetBody(fileListResp->body);
            //     resp->SetContentType("application/json");
            }

            GlobalResourceInstance::Get()->root_path = YamlReaderInstance::Get()->lookup<std::string>("storage.path", "/null");
            if (!std::filesystem::exists(GlobalResourceInstance::Get()->root_path)) {
                return HTTP_STATUS_INTERNAL_SERVER_ERROR;
            }

            std::string html;
            if (!Login(html)) {
                return HTTP_STATUS_NOT_FOUND;
            }

            // 启动下载线程池
            ThreadPoolInstance::Get()->start();
            GlobalResourceInstance::Get()->logged_in = true;

            resp->SetContentType(http_content_type_str(http_content_type::TEXT_HTML));
            resp->SetBody(html);
            return HTTP_STATUS_OK;
        } else {
            resp->SetBody(postResp->body);
            std::string code = jsonResponse.at("code");
            std::string message = jsonResponse.at("message");
            LOGW("code: %s, message: %s", code.c_str(), message.c_str());
        }
    } catch(const std::exception& e) {
        String8 msg = String8::format("Invalid Json Body: %s, \n%s", e.what(), postResp->body.c_str());
        resp->SetBody(msg.toStdString());
        return HTTP_STATUS_INTERNAL_SERVER_ERROR;
    }

    return HTTP_STATUS_OK;
}

int32_t eular::HttpHandler::Index(HttpRequest *req, HttpResponse *resp)
{
    static const std::string rootPath = YamlReaderInstance::Get()->lookup<std::string>("http.root", DEFAULT_DOCUMENT_ROOT);
    static const std::string indexHtml = YamlReaderInstance::Get()->lookup<std::string>("http.index", "index.html");
    static const std::string redirectHost = YamlReaderInstance::Get()->lookup<std::string>("http.redirect", indexHtml);
    static const std::string path = rootPath + "/" + indexHtml;

    if (GlobalResourceInstance::Get()->logged_in) {
        // HFile file;
        
        // if (file.open(path.c_str(), "r") != 0) {
        //     LOGI("[%s] not found", path.c_str());
        //     return HTTP_STATUS_NOT_FOUND;
        // }

        // std::string fileContent;
        // file.readall(fileContent);
        // resp->SetBody(fileContent);
        // resp->SetContentType(http_content_type_str(TEXT_HTML));
        std::string html;
        if (!Login(html)) {
            return HTTP_STATUS_NOT_FOUND;
        }

        resp->SetContentType(http_content_type_str(http_content_type::TEXT_HTML));
        resp->SetBody(html);
        return HTTP_STATUS_OK;
    }

    return resp->Redirect(redirectHost);
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

int32_t eular::HttpHandler::Logout(HttpRequest *req, HttpResponse *resp)
{
    ThreadPoolInstance::Get()->stop();
    return HTTP_STATUS_OK;
}

bool eular::HttpHandler::Login(std::string &html)
{
    HFile file;
    std::string rootPath = YamlReaderInstance::Get()->lookup<std::string>("http.root", "/www/html") + "/home.html";
    if (file.open(rootPath.c_str(), "r") != 0) {
        LOGE("%s: %s Not Found", __PRETTY_FUNCTION__, rootPath.c_str());
        return false;
    }

    std::string fileContent;
    file.readall(fileContent);

    html.resize(fileContent.size() + GlobalResourceInstance::Get()->image_url.size() + GlobalResourceInstance::Get()->name.size());
    snprintf((char *)html.data(), html.capacity(), fileContent.c_str(),
        GlobalResourceInstance::Get()->image_url.c_str(), GlobalResourceInstance::Get()->name.c_str());

    return true;
}

void eular::HttpHandler::UpdateToken()
{
    std::string postJsonBody;
    try {
        hv::Json refreshTokenJson;
        refreshTokenJson["client_id"] = YamlReaderInstance::Get()->lookup<std::string>("http.app_id");
        refreshTokenJson["client_secret"] = YamlReaderInstance::Get()->lookup<std::string>("http.app_secret");
        refreshTokenJson["grant_type"] = "refresh_token";
        refreshTokenJson["code"] = GlobalResourceInstance::Get()->refresh_token;

        postJsonBody = refreshTokenJson.dump();
    } catch(const std::exception& e) {
        postJsonBody.clear();
        LOGE("UpdateToken() catch exception: %s", e.what());
    }

    http_headers headers;
    headers["Content-Type"] = "application/json";
    auto postResp = requests::post(OPENAPI_DOMAIN_NAME OPENAPI_ACCESS_TOKEN, postJsonBody, headers);
    if (postResp == nullptr) {
        LOGE("UpdateToken() post '" OPENAPI_DOMAIN_NAME OPENAPI_ACCESS_TOKEN "' failed!\n");
        return;
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

            // 刷新 refresh_token 和 token
            GlobalResourceInstance::Get()->refresh_token = refreshToken;
            GlobalResourceInstance::Get()->token_type = tokenType;
            GlobalResourceInstance::Get()->token = accessToken;
            GlobalResourceInstance::Get()->token_expire = expireTime;

            LOGD("tokenType = %s", tokenType.c_str());
            LOGD("access_token = %s", accessToken.c_str());
            LOGD("refreshToken = %s", refreshToken.c_str());
            LOGD("expireTime = %u", expireTime);
        }
    } catch(const std::exception& e) {
        postJsonBody.clear();
        LOGE("UpdateToken() catch exception: %s", e.what());
    }

    // 下次触发时间
    uint32_t expireTime = GlobalResourceInstance::Get()->token_expire;
    AppInstance::Get()->m_hvLoop->setTimeout(EXPIRE_TIME(expireTime) * 1000, [] (hv::TimerID) {
        HttpHandler::UpdateToken();
    });
}
