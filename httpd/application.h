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

#include <hv/HttpServer.h>
#include <hv/EventLoop.h>

#include <utils/singleton.h>

#include "upnp/upnp.h"

namespace eular {
class YamlReader;
class Application
{
public:
    Application() = default;
    ~Application() = default;

    void init(int32_t argc, char *argv[]);

    void run();

    void stop();

    static void HvLoggerHandler(int32_t loglevel, const char* buf, int32_t len);

    void printHelp() const;

public:
    hv::EventLoop*  m_hvLoop;
    int32_t         m_sock[2];

protected:
    static void Signalcatch(int sig);

private:
    bool    m_daemon;

    std::unique_ptr<hv::EventLoop>  m_idleLoop;
    std::unique_ptr<hv::HttpServer> m_httpServer;
    std::unique_ptr<UPNPClient>     m_upnp;
};

using AppInstance = Singleton<Application>;

} // namespace eular

#endif // __HTTPD_APPLICATION_H__
