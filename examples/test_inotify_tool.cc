/*************************************************************************
    > File Name: test_inotify_tool.cc
    > Author: hsz
    > Brief:
    > Created Time: 2024年04月21日 星期日 12时52分34秒
 ************************************************************************/

#include "inotify_tool/inotify_event.h"
#include "inotify_tool/inotify_tool.h"

#include <utils/errors.h>
#include <log/log.h>

#define LOG_TAG "Test-Inotify"

int main(int argc, char *argv[])
{
    if (argc != 2)
    {
        printf("usage: %s /path/to/watch/\n", argv[0]);
        return 0;
    }

    eular::log::InitLog();
    eular::log::EnableLogColor(true);
    eular::log::SetPath("/home/eular/VSCode/Sync-Alibaba_Cloud_Disk/build/examples/");
    eular::log::addOutputNode(eular::LogWrite::FILEOUT);

    eular::InotifyTool::SP spInotifyTool = std::make_shared<eular::InotifyTool>();

    spInotifyTool->createInotify();

    spInotifyTool->watchRecursive(argv[1], EV_IN_ALL);

    while (true)
    {
        int32_t errorCode = spInotifyTool->waitCompleteEvent(0);
        if (errorCode != NO_ERROR)
        {
            perror("waitCompleteEvent error");
            continue;
        }

        std::list<InotifyEventItem> eventItemVec;
        spInotifyTool->getEventItem(eventItemVec);

        for (const auto &it : eventItemVec)
        {
            const std::string &event = Event2String(it.event);
            LOGI("%s: %s\n", it.name.c_str(), event.c_str());
        }
    }

    return 0;
}