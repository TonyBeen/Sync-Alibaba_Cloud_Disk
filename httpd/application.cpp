/*************************************************************************
    > File Name: application.cpp
    > Author: hsz
    > Brief:
    > Created Time: 2024年05月07日 星期二 15时13分10秒
 ************************************************************************/

#include "httpd/application.h"

#include <log/log.h>
#include <config/YamlConfig.h>

#include <hv/HttpServer.h>
#include <hv/hmain.h>
#include <hv/hlog.h>

#include "httpd/console_config.h"
#include "httpd/http_router.h"
#include "httpd/http_handler.h"
#include "application.h"

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
}

void Application::run()
{
    hv::HttpServer  httpServer;
    hv::HttpService httpService;

    eular::HttpRouter::Register(httpService);
    httpServer.registerHttpService(&httpService);
    std::string bindHost;
    bindHost = YamlReaderInstance::Get()->lookup<std::string>("http.ip", "0.0.0.0");
    bindHost += ':';
    bindHost += std::to_string(YamlReaderInstance::Get()->lookup<uint32_t>("http.port", 8080));

    LOGD("Server bind to %s", bindHost.c_str());
    httpServer.run(bindHost.c_str());

    main_ctx_free();
}

void Application::HvLoggerHandler(int32_t loglevel, const char* buf, int32_t len)
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

} // namespace eular
