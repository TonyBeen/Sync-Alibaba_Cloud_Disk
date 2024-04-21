/*************************************************************************
    > File Name: test_inotify_tool.cc
    > Author: hsz
    > Brief:
    > Created Time: 2024年04月21日 星期日 12时52分34秒
 ************************************************************************/

#include "inotify_tool/inotify_event.h"
#include "inotify_tool/inotify_tool.h"

int main(int argc, char *argv[])
{
    eular::InotifyTool::SP spInotifyTool = std::make_shared<eular::InotifyTool>();

    spInotifyTool->createInotify();

    spInotifyTool->watchRecursive("", EV_IN_ALL);

    while (true)
    {
        spInotifyTool->waitCompleteEvent(0);
    }

    return 0;
}