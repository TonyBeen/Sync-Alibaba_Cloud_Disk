/*************************************************************************
    > File Name: upnp.cpp
    > Author: hsz
    > Brief:
    > Created Time: 2024年05月04日 星期六 20时25分46秒
 ************************************************************************/

#include "upnp/upnp.h"

#include "miniupnpc.h"
#include "upnpcommands.h"
#include "portlistingparse.h"
#include "upnperrors.h"

#include <log/log.h>
#include "upnp.h"

#define LOG_TAG "UPNP"

struct UPNPDeviceInfo
{
    UPNPDev*    pDeviceList;
    UPNPUrls    urls;
    IGDdatas    data;
    char        lanAddr[64];

    UPNPDeviceInfo() :
        pDeviceList(nullptr)
    {
        memset(&urls, 0, sizeof(UPNPUrls));
        memset(&data, 0, sizeof(IGDdatas));
        memset(lanAddr, 0, sizeof(lanAddr));
    }

    ~UPNPDeviceInfo()
    {
        FreeUPNPUrls(&urls);
        freeUPNPDevlist(pDeviceList);
    }
};

static const char g_protoTCP[4] = {'T', 'C', 'P', 0};
static const char g_protoUDP[4] = {'U', 'D', 'P', 0};

namespace eular {
static void DisplayInfos(struct UPNPUrls *urls, struct IGDdatas *data)
{
    char externalIPAddress[40];
    char connectionType[64];
    char status[64];
    char lastconnerr[64];
    uint32_t uptime = 0;
    uint32_t brUp, brDown;
    time_t timenow, timestarted;
    int32_t r;
    if (UPNP_GetConnectionTypeInfo(urls->controlURL,
                                   data->first.servicetype,
                                   connectionType) != UPNPCOMMAND_SUCCESS)
        LOGE("GetConnectionTypeInfo failed.\n");
    else
        LOGI("Connection Type : %s\n", connectionType);
    if (UPNP_GetStatusInfo(urls->controlURL, data->first.servicetype,
                           status, &uptime, lastconnerr) != UPNPCOMMAND_SUCCESS)
        LOGE("GetStatusInfo failed.\n");
    else
        LOGI("Status : %s, uptime=%us, LastConnectionError : %s\n", status, uptime, lastconnerr);
    if (uptime > 0)
    {
        timenow = time(NULL);
        timestarted = timenow - uptime;
        LOGI("  Time started : %s", ctime(&timestarted));
    }
    if (UPNP_GetLinkLayerMaxBitRates(urls->controlURL_CIF, data->CIF.servicetype,
                                     &brDown, &brUp) != UPNPCOMMAND_SUCCESS)
    {
        LOGE("GetLinkLayerMaxBitRates failed.\n");
    }
    else
    {
        LOGI("MaxBitRateDown : %u bps", brDown);
        if (brDown >= 1000000)
        {
            LOGI("  (%u.%u Mbps)", brDown / 1000000, (brDown / 100000) % 10);
        }
        else if (brDown >= 1000)
        {
            LOGI("  (%u Kbps)", brDown / 1000);
        }

        LOGI("   MaxBitRateUp %u bps", brUp);
        if (brUp >= 1000000)
        {
            LOGI("  (%u.%u Mbps)", brUp / 1000000, (brUp / 100000) % 10);
        }
        else if (brUp >= 1000)
        {
            LOGI("  (%u Kbps)", brUp / 1000);
        }
        LOGI("\n");
    }
    r = UPNP_GetExternalIPAddress(urls->controlURL,
                                  data->first.servicetype,
                                  externalIPAddress);
    if (r != UPNPCOMMAND_SUCCESS)
    {
        LOGE("GetExternalIPAddress failed. (errorcode=%d)\n", r);
    }
    else if (!externalIPAddress[0])
    {
        LOGE("GetExternalIPAddress failed. (empty string)\n");
    }
    else
    {
        LOGI("ExternalIPAddress = %s\n", externalIPAddress);
    }
}

static void Format(uint16_t index, const char *protocol, const char *exPort,
                   const char *inAddr, const char *inPort, const char *description,
                   const char *remoteHost, const char *leaseTime)
{
    LOGI("| %hu | %-8s | %5s->%s:%-5s | '%s' | '%s' | %s |\n",
         index, protocol, exPort, inAddr, inPort, description, remoteHost, leaseTime);
}

static void ListRedirections(struct UPNPUrls *urls, struct IGDdatas *data)
{
    int r;
    unsigned short i = 0;
    char index[6];
    char intClient[40];
    char intPort[6];
    char extPort[6];
    char protocol[4];
    char desc[80];
    char enabled[6];
    char rHost[64];
    char duration[16];

    LOGI("| i | protocol | exPort->inAddr:inPort | description | remoteHost | leaseTime |\n");
    do {
        snprintf(index, 6, "%hu", i);
        rHost[0] = '\0';
        enabled[0] = '\0';
        duration[0] = '\0';
        desc[0] = '\0';
        extPort[0] = '\0';
        intPort[0] = '\0';
        intClient[0] = '\0';
        r = UPNP_GetGenericPortMappingEntry(urls->controlURL,
                                            data->first.servicetype,
                                            index,
                                            extPort, intClient, intPort,
                                            protocol, desc, enabled,
                                            rHost, duration);
        if (r == 0) {
            Format(i, protocol, extPort, intClient, intPort, desc, rHost, duration);
        } else {
            if (r != 713) {
                LOGE("GetGenericPortMappingEntry() returned %d (%s)\n", r, strupnperror(r));
            }
        }
    } while (r == 0 && i++ < 65535);
}

bool UPNPClient::hasValidIGD()
{
    m_hasValidIGD = false;
    m_spDevice.reset();
    m_spDevice = std::make_shared<UPNPDeviceInfo>();

    struct UPNPDev *devList = nullptr;
    int32_t error = 0;
    devList = upnpDiscover(100, nullptr, nullptr, 0, 0, 2, &error);
    if (nullptr == devList) {
        LOGW("No IGD UPnP Device found on the network!\n");
        return m_hasValidIGD;
    }

    m_spDevice->pDeviceList = devList;

    int32_t value = 0;
    value = UPNP_GetValidIGD(devList, &m_spDevice->urls, &m_spDevice->data,
                             m_spDevice->lanAddr, sizeof(m_spDevice->lanAddr));
    if (value < 0) {
        LOGW("Error calling interface UPNP_GetValidIGD!");
        return m_hasValidIGD;
    }

    if (value != 1) {
        LOGW("No valid IGD device found!");
        return m_hasValidIGD;
    }

    m_hasValidIGD = true;
    return m_hasValidIGD;
}

void UPNPClient::displayUpnp() const
{
    int32_t error = 0;
    struct UPNPDev* pDevice;
    struct UPNPUrls urls;
    struct IGDdatas data;
    char lanaddr[64] = {0};

    struct UPNPDev *upnpDevList = upnpDiscover(100, nullptr, nullptr, UPNP_LOCAL_PORT_ANY, 0, 2, &error);
    if (upnpDevList == nullptr) {
        LOGW("No IGD UPnP Device found on the network!");
        return;
    }

    LOGI("List of UPNP devices found on the network:\n");
    for (pDevice = upnpDevList; nullptr != pDevice; pDevice = pDevice->pNext) {
        LOGI("    xml url: %s", pDevice->descURL);
        LOGI("    device type: %s", pDevice->st);
        LOGI("");
    }

    int32_t index = 0;
    index = UPNP_GetValidIGD(upnpDevList, &urls, &data, lanaddr, sizeof(lanaddr));
    switch (index) {
    case 1:
        LOGI("Found valid IGD : %s\n", urls.controlURL);
        break;
    case 2:
        LOGI("Found a (not connected?) IGD : %s\n", urls.controlURL);
        break;
    case 3:
        LOGI("UPnP device found. Is it an IGD ? : %s\n", urls.controlURL);
        break;
    default:
        LOGI("Found device (igd ?) : %s\n", urls.controlURL);
        break;
    }

    LOGI("Local LAN ip address : %s\n", lanaddr);
    DisplayInfos(&urls, &data);
    ListRedirections(&urls, &data);

    FreeUPNPUrls(&urls);
    freeUPNPDevlist(upnpDevList);
}

bool UPNPClient::addUPNP(const char *localIp, uint16_t localPort,
                         uint16_t externalPort, uint32_t proto,
                         uint32_t leaseDuration, const char *description)
{
    if (!hasValidIGD()) {
        LOGE("No valid IGD device found!");
        return false;
    }

    if (localIp == nullptr) {
        return false;
    }

    const char *pProto = nullptr;
    if (proto == PROTO_TCP) {
        pProto = g_protoTCP;
    } else if (proto == PROTO_UDP) {
        pProto = g_protoUDP;
    } else {
        return false;
    }

    char externalIPAddress[40] = {0};
    int32_t errorCode = UPNP_GetExternalIPAddress(m_spDevice->urls.controlURL,
                                                  m_spDevice->data.first.servicetype,
                                                  externalIPAddress);
    if (errorCode != UPNPCOMMAND_SUCCESS) {
        LOGE("Error calling interface UPNP_GetExternalIPAddress!");
        return false;
    }
    LOGI("External IP Address = %s\n", externalIPAddress);

    char exPort[16] = {0};
    snprintf(exPort, sizeof(exPort), "%u", externalPort);
    char lPort[16] = {0};
    snprintf(lPort, sizeof(lPort), "%u", localPort);
    char duration[16] = {0};
    snprintf(duration, sizeof(duration), "%u", leaseDuration);

    errorCode = UPNP_AddPortMapping(m_spDevice->urls.controlURL, m_spDevice->data.first.servicetype,
                                    exPort, lPort, localIp, description,
                                    pProto, nullptr, duration);
    if (errorCode != UPNPCOMMAND_SUCCESS) {
        LOGE("UPNP_AddPortMapping(%s, %s, %s) failed: %s\n",
             exPort, lPort, localIp, strupnperror(errorCode));
        return false;
    }

    char intClient[40] = {0};
    char intPort[6] = {0};
    errorCode = UPNP_GetSpecificPortMappingEntry(m_spDevice->urls.controlURL,
                                                 m_spDevice->data.first.servicetype,
                                                 exPort, pProto, nullptr,
                                                 intClient, intPort, nullptr,
                                                 nullptr, duration);
    if (errorCode != UPNPCOMMAND_SUCCESS) {
        LOGE("UPNP_GetSpecificPortMappingEntry() failed: %s", strupnperror(errorCode));
        return false;
    }

    LOGI("InternalIP:Port = %s:%s\n", intClient, intPort);
    LOGI("external %s:%s %s is redirected to internal %s:%s (duration = %s)",
         externalIPAddress, exPort, pProto, intClient, intPort, duration);

    return true;
}

void UPNPClient::delUPNP(uint16_t externalPort, uint32_t proto)
{
    if (!hasValidIGD()) {
        LOGE("No valid IGD device found!");
        return;
    }

    const char *pProto = nullptr;
    if (proto == PROTO_TCP) {
        pProto = g_protoTCP;
    } else if (proto == PROTO_UDP) {
        pProto = g_protoUDP;
    } else {
        return;
    }

    char exPort[8] = {0};
    snprintf(exPort, sizeof(exPort), "%u", externalPort);
    int32_t errorCode = UPNP_DeletePortMapping(m_spDevice->urls.controlURL, m_spDevice->data.first.servicetype,
                                               exPort, pProto, nullptr);
    if (errorCode != UPNPCOMMAND_SUCCESS) {
        LOGE("UPNP_DeletePortMapping() failed: %s", strupnperror(errorCode));
    }

    char externalIPAddress[40] = {0};
    errorCode = UPNP_GetExternalIPAddress(m_spDevice->urls.controlURL,
                                          m_spDevice->data.first.servicetype,
                                          externalIPAddress);
    if (errorCode != UPNPCOMMAND_SUCCESS) {
        return;
    }
    LOGI("external %s:%s %s is removed.", externalIPAddress, exPort, pProto);
}

} // namespace eular
