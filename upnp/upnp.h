/*************************************************************************
    > File Name: upnpc.h
    > Author: hsz
    > Brief:
    > Created Time: 2024年05月04日 星期六 20时25分42秒
 ************************************************************************/

#ifndef __UPNP_UPNP_H_
#define __UPNP_UPNP_H_

#include <stdint.h>
#include <memory>

namespace eular {
class UpnpClient
{
public:
    UpnpClient() = default;
    ~UpnpClient() = default;

    void displayUpnp() const;
};

} // namespace eular

#endif // __UPNP_UPNP_H_
