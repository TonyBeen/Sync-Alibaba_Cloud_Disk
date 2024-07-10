/*************************************************************************
    > File Name: console_config.h
    > Author: hsz
    > Brief:
    > Created Time: 2024年05月07日 星期二 10时31分45秒
 ************************************************************************/

#ifndef __HTTPD_CONSOLE_CONFIG_H__
#define __HTTPD_CONSOLE_CONFIG_H__

#include <stdio.h>
#include <stdint.h>

#include <memory>

#include <hv/hmain.h>

namespace eular {
class ConsoleConfig
{
public:
    typedef std::unique_ptr<ConsoleConfig> Ptr;
    typedef std::shared_ptr<ConsoleConfig> SP;

    ConsoleConfig();
    ~ConsoleConfig();

    /**
     * @brief 设置要解析的命令
     * 
     * @param cmd 
     */
    void setOption(const option_t *pOption, uint32_t optionSize);

    /**
     * @brief 解析命令行参数
     * 
     * @param argc 参数个数
     * @param argv 参数数组
     * @return true 解析成功
     * @return false 解析失败
     */
    bool parse(int32_t argc, char *argv[]);

    const char *getArg(const char *key);

private:
    const option_t* m_pOption;
    uint32_t        m_optionSize;
};

} // namespace eular

#endif // __HTTPD_CONSOLE_CONFIG_H__
