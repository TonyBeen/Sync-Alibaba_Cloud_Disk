/*************************************************************************
    > File Name: application.cpp
    > Author: hsz
    > Brief:
    > Created Time: 2024年05月07日 星期二 15时13分10秒
 ************************************************************************/

#include "httpd/application.h"

#include <log/log.h>
#include <log/callstack.h>
#include <config/YamlConfig.h>

#include <hv/hmain.h>
#include <hv/hsocket.h>
#include <hv/hloop.h>
#include <hv/hlog.h>

#include "httpd/console_config.h"
#include "httpd/http_router.h"
#include "httpd/http_handler.h"
#include "application.h"
#include "thread_pool.h"

#define LOG_TAG     "Application"
#define HV_LOG_TAG  "libhv"

// short options
static const char options[] = "hc:d";
// long options
static const option_t long_options[] = {
    {'h', "help",       NO_ARGUMENT},
    {'c', "confile",    REQUIRED_ARGUMENT},
    {'d', "daemon",     NO_ARGUMENT}
};
static const char detail_options[] = R"(
  -h|--help                 Print this information
  -c|--confile <confile>    Set configure file
  -d|--daemon               Daemonize
)";

namespace eular {
void Application::init(int32_t argc, char *argv[])
{
    m_daemon = false;
    YamlReaderInstance::Get();
    GlobalResourceInstance::Get();
    ThreadPoolInstance::Get();

    if (Socketpair(AF_LOCAL, SOCK_STREAM, 0, m_sock) != 0) {
        perror("socketpair failed");
        return;
    }

    eular::ConsoleConfig::Ptr pConsoleConfig(new eular::ConsoleConfig());
    pConsoleConfig->setOption(long_options, ARRAY_SIZE(long_options));
    if (!pConsoleConfig->parse(argc, argv)) {
        printf("parse failed\n");
        abort();
    }

    if (pConsoleConfig->getArg("h")) {
        printHelp();
        exit(0);
    }

    const char *yamlPath = pConsoleConfig->getArg("c");
    if (yamlPath != nullptr) {
        YamlReaderInstance::Get()->loadYaml(yamlPath);
        if (!YamlReaderInstance::Get()->valid()) {
            printf("invalid config: %s\n", yamlPath);
            abort();
        }
    }

    // 自定义日志处理句柄
    logger_t *logger = hv_default_logger();
    logger_set_handler(logger, &Application::HvLoggerHandler);

    main_ctx_init(argc, argv);
    m_upnp = std::unique_ptr<UPNPClient>(new UPNPClient());
    m_httpServer = std::make_unique<hv::HttpServer>();
    m_httpServer->onWorkerStart = [this] () {
        this->m_hvLoop = hv::tlsEventLoop();
        hio_t *io = hio_get(m_hvLoop->loop(), m_sock[0]);
        // 设置读事件回调
        hio_setcb_read(io, [] (hio_t *io, void *buf, int readbytes) {

        });
        // 添加读事件
        hio_read(io);
    };

    m_idleLoop = std::make_unique<hv::EventLoop>();
    uint32_t leaseDuration = YamlReaderInstance::Get()->lookup<uint32_t>("upnp.mapping.lease_time", 24 * 60 * 60);
    uint32_t upnpProbe = YamlReaderInstance::Get()->lookup<uint32_t>("upnp.upnp_probe", 30000);
    m_idleLoop->setInterval(leaseDuration * 1000, [this, upnpProbe, leaseDuration] (hv::TimerID timerId) {
        if (m_upnp->hasValidIGD()) {
            std::string localHost = YamlReaderInstance::Get()->lookup<std::string>("upnp.mapping.local_host", "0.0.0.0");
            uint16_t localPort = YamlReaderInstance::Get()->lookup<uint16_t>("upnp.mapping.local_port", 8080);
            uint16_t externalPort = YamlReaderInstance::Get()->lookup<uint16_t>("upnp.mapping.external_port", 8080);
            m_upnp->addUPNP(localHost.c_str(), localPort, externalPort, PROTO_TCP, leaseDuration, "sync_aliyun");
        } else {
            m_idleLoop->setInterval(upnpProbe, [this, leaseDuration] (hv::TimerID timerId) {
                if (m_upnp->hasValidIGD()) {
                    std::string localHost = YamlReaderInstance::Get()->lookup<std::string>("upnp.mapping.local_host", "0.0.0.0");
                    uint16_t localPort = YamlReaderInstance::Get()->lookup<uint16_t>("upnp.mapping.local_port", 8080);
                    uint16_t externalPort = YamlReaderInstance::Get()->lookup<uint16_t>("upnp.mapping.external_port", 8080);
                    m_upnp->addUPNP(localHost.c_str(), localPort, externalPort, PROTO_TCP, leaseDuration, "sync_aliyun");

                    m_idleLoop->killTimer(timerId);
                }
            });

           m_idleLoop->killTimer(timerId);
        }
    });
}

void Application::run()
{
    signal(SIGABRT, Signalcatch);
    signal(SIGINT, Signalcatch);
    signal(SIGSEGV, Signalcatch);
    signal(SIGPIPE, SIG_IGN);

    // 注册UPNP
    if (m_upnp->hasValidIGD()) {
        std::string localHost = YamlReaderInstance::Get()->lookup<std::string>("upnp.mapping.local_host", "0.0.0.0");
        uint16_t localPort = YamlReaderInstance::Get()->lookup<uint16_t>("upnp.mapping.local_port", 8080);
        uint16_t externalPort = YamlReaderInstance::Get()->lookup<uint16_t>("upnp.mapping.external_port", 8080);
        uint32_t leaseDuration = YamlReaderInstance::Get()->lookup<uint32_t>("upnp.mapping.lease_time", 24 * 60 * 60);
        m_upnp->addUPNP(localHost.c_str(), localPort, externalPort, PROTO_TCP, leaseDuration, "sync_aliyun");
    } else {
        uint32_t upnpProbe = YamlReaderInstance::Get()->lookup<uint32_t>("upnp.upnp_probe", 5000);
        m_idleLoop->setInterval(upnpProbe, [this] (hv::TimerID timerId) {
            if (m_upnp->hasValidIGD()) {
                std::string localHost = YamlReaderInstance::Get()->lookup<std::string>("upnp.mapping.local_host", "0.0.0.0");
                uint16_t localPort = YamlReaderInstance::Get()->lookup<uint16_t>("upnp.mapping.local_port", 8080);
                uint16_t externalPort = YamlReaderInstance::Get()->lookup<uint16_t>("upnp.mapping.external_port", 8080);
                uint32_t leaseDuration = YamlReaderInstance::Get()->lookup<uint32_t>("upnp.mapping.lease_time", 24 * 60 * 60);
                m_upnp->addUPNP(localHost.c_str(), localPort, externalPort, PROTO_TCP, leaseDuration, "sync_aliyun");

                m_idleLoop->killTimer(timerId);
            }
        });
    }

    hv::HttpService httpService;
    httpService.document_root = YamlReaderInstance::Get()->lookup<std::string>("http.root", DEFAULT_DOCUMENT_ROOT);
    eular::HttpRouter::Register(httpService);
    m_httpServer->registerHttpService(&httpService);
    m_httpServer->setThreadNum(1);
    std::string bindHost;
    bindHost = YamlReaderInstance::Get()->lookup<std::string>("http.ip", "0.0.0.0");
    bindHost += ':';
    bindHost += std::to_string(YamlReaderInstance::Get()->lookup<uint32_t>("http.port", 8080));

    LOGD("Server bind to %s", bindHost.c_str());
    m_httpServer->run(bindHost.c_str());

    main_ctx_free();
}

void Application::stop()
{
    ThreadPoolInstance::Get()->stop();
    m_httpServer->stop();
    if (m_upnp->hasValidIGD()) {
        uint16_t externalPort = YamlReaderInstance::Get()->lookup<uint16_t>("upnp.mapping.external_port", 8080);
        m_upnp->delUPNP(externalPort, PROTO_TCP);
    }
}

void Application::HvLoggerHandler(int32_t loglevel, const char *buf, int32_t len)
{
    switch (loglevel) {
    case log_level_e::LOG_LEVEL_DEBUG:
        eular::log_write(eular::LogLevel::Level::LEVEL_DEBUG, HV_LOG_TAG, "%.*s", len, buf);
        break;
    case log_level_e::LOG_LEVEL_INFO:
        eular::log_write(eular::LogLevel::Level::LEVEL_INFO, HV_LOG_TAG, "%.*s", len, buf);
        break;
    case log_level_e::LOG_LEVEL_WARN:
        eular::log_write(eular::LogLevel::Level::LEVEL_WARN, HV_LOG_TAG, "%.*s", len, buf);
        break;
    case log_level_e::LOG_LEVEL_ERROR:
        eular::log_write(eular::LogLevel::Level::LEVEL_ERROR, HV_LOG_TAG, "%.*s", len, buf);
        break;
    case log_level_e::LOG_LEVEL_FATAL:
        eular::log_write(eular::LogLevel::Level::LEVEL_FATAL, HV_LOG_TAG, "%.*s", len, buf);
        break;
    default:
        eular::log_write(eular::LogLevel::Level::LEVEL_WARN, HV_LOG_TAG, "unknow level %d: %.*s", loglevel, len, buf);
        break;
    }
}

void Application::printHelp() const
{
    printf("Usage: %s [%s]\n", g_main_ctx.program_name, options);
    printf("Options:\n%s\n", detail_options);
}

void Application::Signalcatch(int sig)
{
    AppInstance::Get()->stop();
    if (sig == SIGSEGV) {
        LOGI("catch signal SIGSEGV");
        // 产生堆栈信息;
        eular::CallStack stack;
        stack.update();
        stack.log("SIGSEGV", LogLevel::LEVEL_FATAL);
        exit(0);
    }

    if (sig == SIGABRT) {
        LOGI("catch signal SIGABRT");
        // 产生堆栈信息;
        eular::CallStack stack;
        stack.update();
        stack.log("SIGABRT", LogLevel::LEVEL_FATAL);
        exit(0);
    }

    if (sig == SIGINT) {
        LOGI("catch signal SIGINT");
        exit(0);
    }
}

} // namespace eular
