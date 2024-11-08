/*************************************************************************
    > File Name: global_resource_management.h
    > Author: hsz
    > Brief:
    > Created Time: 2024年07月31日 星期三 16时46分55秒
 ************************************************************************/

#ifndef __HTTPD_GLOBAL_RESOURCE_MANAGEMENT_H__
#define __HTTPD_GLOBAL_RESOURCE_MANAGEMENT_H__

#include <string>

#include <SQLiteCpp/SQLiteCpp.h>

#include <utils/mutex.h>
#include <utils/singleton.h>

#define DEFAULT_NAME        "null"
#define DEFAULT_IMAGE_URL   "default-avatar.png"

namespace eular {
class GlobalResourceManagement
{
public:
    GlobalResourceManagement()
    {
        refresh_token.reserve(512);
        token.reserve(512);
    }

    // user
    bool            logged_in = false; // 已登录
    std::string     name = DEFAULT_NAME; // 用户名
    std::string     image_url = DEFAULT_IMAGE_URL; // 头像地址

    // disk
    std::string     root_path;

    // http cookie
    std::string     refresh_token;
    std::string     token_type;
    std::string     token;
    uint32_t        token_expire = 600; // 秒
    std::string     default_drive_id;
    std::string     resource_drive_id;
    std::string     backup_drive_id;

    // sql
    std::shared_ptr<SQLite::Database>   sqlite_handle;
};

using GlobalResourceInstance = Singleton<GlobalResourceManagement>;
} // namespace eular

#endif // __HTTPD_GLOBAL_RESOURCE_MANAGEMENT_H__
