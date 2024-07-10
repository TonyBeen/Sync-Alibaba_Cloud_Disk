/*************************************************************************
    > File Name: application.h
    > Author: hsz
    > Brief:
    > Created Time: 2024年05月07日 星期二 15时13分07秒
 ************************************************************************/

#ifndef __HTTPD_APPLICATION_H__
#define __HTTPD_APPLICATION_H__

#include <stdint.h>
#include <memory>

namespace eular {
class YamlReader;
class Application
{
public:
    Application() = default;
    ~Application() = default;

    void init(int32_t argc, char *argv[]);

    void run();

    static void HvLoggerHandler(int32_t loglevel, const char* buf, int32_t len);
    void printHelp() const;

private:
    bool  m_daemon;
    std::shared_ptr<YamlReader> m_spYamlReader;
};

} // namespace eular

#endif // __HTTPD_APPLICATION_H__
