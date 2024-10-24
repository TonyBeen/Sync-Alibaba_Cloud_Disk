/*************************************************************************
    > File Name: sql_config.h
    > Author: hsz
    > Brief:
    > Created Time: 2024年10月16日 星期三 15时52分31秒
 ************************************************************************/

#ifndef __HTTPD_SQL_CONFIG_H__
#define __HTTPD_SQL_CONFIG_H__

#define SQL_STORAGE_DISK            "sync.db"
#define SQL_STORAGE_DISK_BAK        "sync_bak.db"
#define SQL_STORAGE_MEMORY          ":memory:"

#define SQL_DB_MEMORY               "memory"
#define SQL_DB_SYNC                 "sync"

#define SQL_TABLE_INFO              "info"              // 已更新文件信息表
#define TABLE_INFO_DRIVE_ID         "drive_id"          // TEXT
#define TABLE_INFO_FILE_ID          "file_id"           // TEXT
#define TABLE_INFO_PARENT_FILE_ID   "parent_file_id"    // TEXT
#define TABLE_INFO_FILE_NAME        "file_name"         // TEXT
#define TABLE_INFO_FILE_PATH        "file_path"         // TEXT
#define TABLE_INFO_HASH             "hash"              // TEXT
#define TABLE_INFO_DATE             "date"              // INTEGER 从 1970-01-01 00:00:00 UTC 算起的秒数
#define TABLE_INFO_IS_DIR           "is_dir"            // INTEGER

// TODO 注意的点
// 1、校验临时文件是否存在, 是否正确, 不存在或格式不正确时删除此条记录
// 2、下载前校验文件是否存在 POST /adrive/v1.0/openFile/get
// 3、存在则校验文件名是否一致, 不一致时更新文件名
#define SQL_TABLE_DOWNLOAD          "download"          // 下载表 -> 记录分片下载
#define TABLE_DOWNLOAD_FILE_ID      TABLE_INFO_FILE_ID
#define TABLE_DOWNLOAD_DRIVE_ID     TABLE_INFO_DRIVE_ID
#define TABLE_DOWNLOAD_FILE_NAME    TABLE_INFO_FILE_NAME
#define TABLE_DOWNLOAD_FILE_PATH    TABLE_INFO_FILE_PATH // 相对 storage.path 路径
#define TABLE_DOWNLOAD_HASH         TABLE_INFO_HASH
#define TABLE_DOWNLOAD_TEMP_NAME    "temp_file_name"    // TEXT 临时文件名称

// TODO 注意的点
// 尤其注意
// 1、更新时应先上传新的文件并将check_name_mode设置为auto_rename
// 2、上传完毕后检验返回的结果, 如果名称一致则表示未被重命名, 删除记录并返回, 否则删除旧的文件
// 3、更改新文件的名字为旧名
// 4、删除记录
// TODO 检验上传后的文件ID和driv id与记录的是否一致
#define SQL_TABLE_UPLOAD            "upload"            // 上传表 -> 记录未更新的文件
#define TABLE_UPLOAD_FILE_ID        TABLE_INFO_FILE_ID
#define TABLE_UPLOAD_DRIVE_ID       TABLE_INFO_DRIVE_ID
#define TABLE_UPLOAD_FILE_NAME      TABLE_INFO_FILE_NAME
#define TABLE_UPLOAD_FILE_PATH      TABLE_INFO_FILE_PATH // 相对 storage.path 路径
#define TABLE_UPLOAD_TEMP_NAME      "temp_clound_nam"   // 云上的文件名, 当修改文件并上传时会出现重名情况

#define SQL_CREATE_TABLE_INFO(dbName)                                   \
    "CREATE TABLE IF NOT EXISTS " dbName "." SQL_TABLE_INFO " ("        \
        TABLE_INFO_FILE_ID " TEXT NOT NULL,"                            \
        TABLE_INFO_FILE_NAME " TEXT NOT NULL,"                          \
        TABLE_INFO_FILE_PATH " TEXT NOT NULL"                           \
        TABLE_INFO_PARENT_FILE_ID " TEXT NOT NULL,"                     \
        TABLE_INFO_DRIVE_ID " TEXT NOT NULL,"                           \
        TABLE_INFO_HASH " TEXT NOT NULL,"                               \
        TABLE_INFO_DATE " INTEGER,"                                     \
        TABLE_INFO_IS_DIR " INTEGER,"                                   \
        "UNIQUE(" TABLE_INFO_FILE_ID ")"                                \
    ");"

#define SQL_CREATE_TABLE_DOWNLOAD(dbName)                               \
    "CREATE TABLE IF NOT EXISTS " dbName "." SQL_TABLE_DOWNLOAD " ("    \
        TABLE_DOWNLOAD_FILE_ID " TEXT NOT NULL,"                        \
        TABLE_DOWNLOAD_DRIVE_ID " TEXT NOT NULL,"                       \
        TABLE_DOWNLOAD_FILE_NAME " TEXT NOT NULL,"                      \
        TABLE_DOWNLOAD_HASH " TEXT NOT NULL,"                           \
        TABLE_DOWNLOAD_TEMP_NAME " TEXT NOT NULL,"                      \
        "UNIQUE(" TABLE_DOWNLOAD_FILE_ID ")"                            \
    ");"

#define SQL_CREATE_TABLE_UPLOAD(dbName)                                 \
    "CREATE TABLE IF NOT EXISTS " dbName "." SQL_TABLE_UPLOAD " ("      \
        TABLE_UPLOAD_FILE_ID " TEXT NOT NULL,"                          \
        TABLE_UPLOAD_DRIVE_ID " TEXT NOT NULL,"                         \
        TABLE_UPLOAD_FILE_NAME " TEXT NOT NULL,"                        \
        TABLE_UPLOAD_FILE_PATH " TEXT NOT NULL,"                        \
        TABLE_UPLOAD_TEMP_NAME " TEXT NOT NULL,"                        \
        "UNIQUE(" TABLE_UPLOAD_FILE_ID ")"                              \
    ");"

#define SQL_DROP_TABLE(dbName, tableName)  \
    "DROP TABLE IF EXISTS " dbName "." tableName ";"

#define SQL_DELETE_ALL_RECORD(dbName, tableName)  \
    "DELETE FROM " dbName "." tableName ";"

#define SQL_ATTACH_DB(dbName, alias)    \
    "ATTACH '" dbName "' AS " alias ";"

#endif // __HTTPD_SQL_CONFIG_H__
