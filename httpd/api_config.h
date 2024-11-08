/*************************************************************************
    > File Name: api_config.h
    > Author: hsz
    > Brief:
    > Created Time: 2024年10月28日 星期一 20时02分55秒
 ************************************************************************/

#ifndef __HTTPD_API_CONFIG_H__
#define __HTTPD_API_CONFIG_H__

#define OPENAPI_DOMAIN_NAME     "https://openapi.alipan.com"
#define OPENAPI_ACCESS_TOKEN    "/oauth/access_token"               // POST
#define OPENAPI_USER_INFO       "/oauth/users/info"                 // GET
#define OPENAPI_DRIVE_INFO      "/adrive/v1.0/user/getDriveInfo"    // POST
#define OPENAPI_FILE_LIST       "/adrive/v1.0/openFile/list"        // POST

#define FILE_LIST_API_REQ_LIMIT 250 // 10 秒 40 次

#endif // __HTTPD_API_CONFIG_H__
