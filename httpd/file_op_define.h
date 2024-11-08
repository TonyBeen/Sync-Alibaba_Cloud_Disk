/*************************************************************************
    > File Name: file_op_define.h
    > Author: hsz
    > Brief:
    > Created Time: 2024年10月16日 星期三 10时41分05秒
 ************************************************************************/

#ifndef __HTTPD_FILE_OP_H__
#define __HTTPD_FILE_OP_H__

#include <stdint.h>
#include <string>

namespace eular {

enum class FileOp : uint32_t {
    OP_MOVE,
    OP_COPY,
    OP_DELETE,
    OP_CREATE
};

static inline uint32_t TranslateFileOp(FileOp op)
{
    return static_cast<uint32_t>(op);
}

static inline FileOp TranslateFileOp(uint32_t op)
{
    return static_cast<FileOp>(op);
}

struct FileOpMove
{
    FileOp op;
    std::string file_name;// 文件名
    std::string src_path; // 源路径
    std::string dst_path; // 目的路径
};

struct FileOpCopy
{
    FileOp op;
    std::string file_name;// 文件名
    std::string dst_path; // 目的路径
};

} // namespace eular

#endif // __HTTPD_FILE_OP_H__
