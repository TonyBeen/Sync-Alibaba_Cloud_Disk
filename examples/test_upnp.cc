/*************************************************************************
    > File Name: test_upnp.cc
    > Author: hsz
    > Brief:
    > Created Time: 2024年05月05日 星期日 13时39分47秒
 ************************************************************************/

#include "upnp/upnp.h"

int main(int argc, char **argv)
{
    eular::UPNPClient upnp;
    upnp.displayUpnp();

    upnp.addUPNP("192.168.3.10", 8888, 8888, PROTO_TCP, 60, "test_upnp");
    upnp.delUPNP(8888, PROTO_TCP);

    return 0;
}
