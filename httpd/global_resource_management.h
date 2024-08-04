/*************************************************************************
    > File Name: global_resource_management.h
    > Author: hsz
    > Brief:
    > Created Time: 2024年07月31日 星期三 16时46分55秒
 ************************************************************************/

#ifndef __HTTPD_GLOBAL_RESOURCE_MANAGEMENT_H__
#define __HTTPD_GLOBAL_RESOURCE_MANAGEMENT_H__

#include <utils/singleton.h>

namespace eular {
class GlobalResourceManagement
{
public:
    GlobalResourceManagement();
    ~GlobalResourceManagement();

private:

};

using GlobalResourceInstance = Singleton<GlobalResourceManagement>;
} // namespace eular

#endif // __HTTPD_GLOBAL_RESOURCE_MANAGEMENT_H__
