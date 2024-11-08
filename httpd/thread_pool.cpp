/*************************************************************************
    > File Name: thread_pool.cpp
    > Author: hsz
    > Brief:
    > Created Time: 2024年10月11日 星期五 19时25分31秒
 ************************************************************************/

#include "thread_pool.h"

#include <filesystem>

#include <hv/requests.h>
#include <hv/axios.h>
#include <hv/hv.h>

#include <config/YamlConfig.h>
#include <log/log.h>

#include "sql_config.h"
#include "api_config.h"

#define LOG_TAG "ThreadPool"

namespace eular {
ThreadPool::ThreadPool()
{
}

ThreadPool::~ThreadPool()
{
}

bool ThreadPool::start()
{
    // 1、打开数据库
    try {
        auto &sqliteHandle = GlobalResourceInstance::Get()->sqlite_handle;
        const std::string &storagePath = GlobalResourceInstance::Get()->root_path;
        sqliteHandle = std::make_shared<SQLite::Database>(storagePath + "/" SQL_STORAGE_DISK, SQLite::OPEN_READWRITE | SQLite::OPEN_CREATE);
        // (1)、给磁盘数据库起个别名
        sqliteHandle->exec(SQL_ATTACH_DB(SQL_STORAGE_DISK, SQL_DB_SYNC));
        // 为磁盘数据库创建表, 存在表则不会创建
        sqliteHandle->exec(SQL_CREATE_TABLE_INFO(SQL_DB_SYNC));
        sqliteHandle->exec(SQL_CREATE_TABLE_DOWNLOAD(SQL_DB_SYNC));
        sqliteHandle->exec(SQL_CREATE_TABLE_UPLOAD(SQL_DB_SYNC));

        // (2)、附加内存数据库
        sqliteHandle->exec(SQL_ATTACH_DB(SQL_STORAGE_MEMORY, SQL_DB_MEMORY));
        // 为内存数据库创建表
        sqliteHandle->exec(SQL_CREATE_TABLE_INFO(SQL_DB_MEMORY));
        sqliteHandle->exec(SQL_CREATE_TABLE_DOWNLOAD(SQL_DB_MEMORY));
        sqliteHandle->exec(SQL_CREATE_TABLE_UPLOAD(SQL_DB_MEMORY));

        // (3)、将磁盘数据库的数据加载到内存中
        sqliteHandle->exec("INSERT INTO " SQL_DB_MEMORY "." SQL_TABLE_INFO " SELECT * FROM " SQL_DB_SYNC "." SQL_TABLE_INFO ";");
        sqliteHandle->exec("INSERT INTO " SQL_DB_MEMORY "." SQL_TABLE_DOWNLOAD " SELECT * FROM " SQL_DB_SYNC "." SQL_TABLE_DOWNLOAD ";");
        sqliteHandle->exec("INSERT INTO " SQL_DB_MEMORY "." SQL_TABLE_UPLOAD " SELECT * FROM " SQL_DB_SYNC "." SQL_TABLE_UPLOAD ";");
    } catch(const std::exception& e) {
        LOGE("catch exception: %s", e.what());
        return false;
    }

    try {
        // 2、创建下载线程
        uint16_t cpuCores = eular::YamlReaderInstance::Get()->lookup("thread.cpu_cores", 4);
        m_syncThreadVec.resize(cpuCores);
        for (uint16_t i = 0; i < cpuCores; ++i) {
            m_syncThreadVec[i] = std::make_shared<SyncThread>();
            m_syncThreadHashMap.insertNode(std::to_string(i), m_syncThreadVec[i].get());
        }

        // 3、创建线程执行执行
        m_syncTh = std::make_shared<Thread>([this] () {
            this->syncFromCloud(GlobalResourceInstance::Get()->root_path, "root");
            // TODO 开启inotify
        }, "SYNC");
    } catch(const std::exception& e) {
        LOGE("create thread exception: %s", e.what());
        return false;
    }

    return true;
}

void ThreadPool::stop()
{
    if (!GlobalResourceInstance::Get()->logged_in) {
        return;
    }

    flushSQLite();
}

void ThreadPool::flushSQLite()
{
    try {
        auto sqliteHandle = GlobalResourceInstance::Get()->sqlite_handle;
        if (sqliteHandle == nullptr) {
            return;
        }

        // 将 内存数据库 刷新到 磁盘数据库
        sqliteHandle->exec("BEGIN TRANSACTION;");
        sqliteHandle->exec(SQL_DELETE_ALL_RECORD(SQL_DB_SYNC, SQL_TABLE_INFO));
        sqliteHandle->exec(SQL_DELETE_ALL_RECORD(SQL_DB_SYNC, SQL_TABLE_DOWNLOAD));
        sqliteHandle->exec(SQL_DELETE_ALL_RECORD(SQL_DB_SYNC, SQL_TABLE_UPLOAD));
        sqliteHandle->exec("INSERT INTO " SQL_DB_SYNC "." SQL_TABLE_INFO " SELECT * FROM " SQL_DB_MEMORY "." SQL_TABLE_INFO ";");
        sqliteHandle->exec("INSERT INTO " SQL_DB_SYNC "." SQL_TABLE_DOWNLOAD " SELECT * FROM " SQL_DB_MEMORY "." SQL_TABLE_DOWNLOAD ";");
        sqliteHandle->exec("INSERT INTO " SQL_DB_SYNC "." SQL_TABLE_UPLOAD " SELECT * FROM " SQL_DB_MEMORY "." SQL_TABLE_UPLOAD ";");
        sqliteHandle->exec("COMMIT;");

        // 分离
        sqliteHandle->exec("DETACH " SQL_DB_MEMORY ";");
    } catch(const std::exception& e) {
        LOGE("catch exception: %s", e.what());
    }
}

void ThreadPool::syncFromCloud(std::string diskPath, std::string parentFileId)
{
    if (diskPath.back() != '/') {
        diskPath.push_back('/');
    }

    // 递归获取文件列表
    const std::string &tokenType = GlobalResourceInstance::Get()->token_type;
    const std::string &accessToken = GlobalResourceInstance::Get()->token;
    const std::string &default_drive_id = GlobalResourceInstance::Get()->resource_drive_id;
    auto &sqlHandle = GlobalResourceInstance::Get()->sqlite_handle;

    http_headers fileListReqHeader;
    fileListReqHeader["Authorization"] = tokenType + " " + accessToken;
    fileListReqHeader["Content-Type"] = "application/json";

    nlohmann::json fileListReqBody;
    fileListReqBody["drive_id"] = default_drive_id;
    fileListReqBody["limit"] = 100;
    fileListReqBody["parent_file_id"] = parentFileId;
    std::this_thread::sleep_for(std::chrono::milliseconds(FILE_LIST_API_REQ_LIMIT)); // API限流
    auto fileListResp = requests::post(OPENAPI_DOMAIN_NAME OPENAPI_FILE_LIST, fileListReqBody.dump(), fileListReqHeader);
    LOGI("POST [" OPENAPI_DOMAIN_NAME OPENAPI_FILE_LIST "] => Response %d %s\r\n", fileListResp->status_code, fileListResp->status_message());
    if (fileListResp->status_code == HTTP_STATUS_OK) {
        try {
            nlohmann::json fileJson = nlohmann::json::parse(fileListResp->body);
            if (!fileJson.contains("items")) {
                return;
            }
            nlohmann::json fileItemJsonArray = fileJson.at("items");
            if (!fileItemJsonArray.is_array()) {
                LOGW("post " OPENAPI_FILE_LIST " response error. type(%s) != array", fileItemJsonArray.type_name());
                return;
            }

            for (const auto &fileItem : fileItemJsonArray) {
                const auto &fileType = fileItem.at("type");
                const std::string &fileItemName = fileItem.at("name").get<std::string>();
                const std::string &fileId = fileItem.at("file_id").get<std::string>();
                nlohmann::json sha1Hash = fileItem.at("content_hash"); // 文件夹为null

                if (fileType == "folder") { // 文件夹
                    // std::filesystem::create_directories(diskPath + fileItemName);
                    // 加入info表
                    // try {
                    //     SQLite::Statement query(*sqlHandle.get(), "INSERT INTO " SQL_TABLE_INFO " VALUES (?, ?, ?, ?, ?, ?, ?, ?)");
                    //     query.bind(1, default_drive_id);
                    //     query.bind(2, fileId);
                    //     query.bind(3, parentFileId);
                    //     query.bind(4, fileItemName);
                    //     query.bind(5, diskPath);
                    //     query.bind(6, "");
                    //     query.bind(7, std::time(NULL));
                    //     query.bind(8, 1);
                    //     query.exec();
                    // } catch (const std::exception& e) {
                    //     LOGE("insert into info error. %s", e.what());
                    // }

                    LOGI("DIR: %s", std::string(diskPath + fileItemName).c_str());

                    syncFromCloud(diskPath + fileItemName, fileId);
                } else if (fileType == "file") { // 文件
                    // 查询info表
                    // try {
                    //     SQLite::Statement queryInfo(*sqlHandle.get(), "SELECT " TABLE_INFO_FILE_ID " FROM " SQL_TABLE_INFO " WHERE " TABLE_INFO_FILE_ID " = ?");
                    //     queryInfo.bind(1, fileId);
                    //     if (!queryInfo.executeStep()) { // 表中不存在数据
                    //         FileDownloadItemNode node = {
                    //             .drive_id = default_drive_id,
                    //             .file_id = fileId,
                    //             .file_path = diskPath,
                    //             .file_name = fileItemName,
                    //         };

                    //         m_syncThreadHashMap.getNode(diskPath + fileItemName)->enqueue(node);
                    //     }
                    // } catch (const std::exception& e) {
                    //     LOGE("insert into info error. %s", e.what());
                    // }
                    LOGI("File: %s", std::string(diskPath + fileItemName).c_str());
                } else {

                }
            }
        } catch(const std::exception& e) {
            LOGE("************************** %s", e.what());
        }
    }
}

} // namespace eular

