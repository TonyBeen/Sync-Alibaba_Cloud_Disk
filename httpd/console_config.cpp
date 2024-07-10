/*************************************************************************
    > File Name: console_config.cpp
    > Author: hsz
    > Brief:
    > Created Time: 2024年05月07日 星期二 10时31分48秒
 ************************************************************************/

#include <getopt.h>

#include <log/log.h>

#include "httpd/console_config.h"
#include "console_config.h"

#define LOG_TAG "ConsoleConfig"

namespace eular {
ConsoleConfig::ConsoleConfig() :
    m_pOption(nullptr),
    m_optionSize(0)
{
}

ConsoleConfig::~ConsoleConfig()
{
}

void ConsoleConfig::setOption(const option_t *pOption, uint32_t optionSize)
{
    m_pOption = pOption;
    m_optionSize = optionSize;
}

bool ConsoleConfig::parse(int32_t argc, char *argv[])
{
    if (m_pOption == nullptr || m_optionSize == 0) {
        return false;
    }

    int ret = parse_opt_long(argc, argv, m_pOption, m_optionSize);
    if (ret != 0) {
        LOGE("Parsing failed");
        return false;
    }

    return true;
}

const char *ConsoleConfig::getArg(const char *key)
{
    return get_arg(key);
}

} // namespace eular

