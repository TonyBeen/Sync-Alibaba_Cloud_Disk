/*************************************************************************
    > File Name: main.cpp
    > Author: hsz
    > Brief:
    > Created Time: 2024年05月01日 星期三 16时47分44秒
 ************************************************************************/

#include "httpd/application.h"

int main(int argc, char **argv)
{
    eular::Application app;
    app.init(argc, argv);

    app.run();
    return 0;
}
