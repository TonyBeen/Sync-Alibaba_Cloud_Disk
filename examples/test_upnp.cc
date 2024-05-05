/*************************************************************************
    > File Name: test_upnp.cc
    > Author: hsz
    > Brief:
    > Created Time: 2024年05月05日 星期日 13时39分47秒
 ************************************************************************/

#include "upnp/upnp.h"

int main(int argc, char **argv)
{
    eular::UpnpClient upnp;
    upnp.displayUpnp();
   
    return 0;
}
