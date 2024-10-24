/*************************************************************************
    > File Name: main.cpp
    > Author: hsz
    > Brief:
    > Created Time: 2024年05月01日 星期三 16时47分44秒
 ************************************************************************/

#include <utils/singleton.h>

#include "httpd/application.h"

#include <sqlite3.h>

int main(int argc, char **argv)
{
    if (!sqlite3_threadsafe()) {
        std::cout << "SQLite is NOT thread-safe" << std::endl;
        return 0;
    }

    auto appInstance = eular::AppInstance::Get();
    appInstance->init(argc, argv);

    appInstance->run();
    return 0;
}
