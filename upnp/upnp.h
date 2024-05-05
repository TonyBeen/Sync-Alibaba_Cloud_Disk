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

struct UPNPDeviceInfo;

#define PROTO_TCP   0x1
#define PROTO_UDP   0x2

namespace eular {
class UPNPClient
{
public:
    typedef std::shared_ptr<UPNPClient> SP;
    typedef std::shared_ptr<UPNPClient> WP;
    typedef std::unique_ptr<UPNPClient> Ptr;

    UPNPClient() = default;
    ~UPNPClient() = default;

    /**
     * @brief 检测当前路由器是否支持UPNP
     * 
     * @return true 支持
     * @return false 不支持
     */
    bool hasValidIGD();

    /**
     * @brief 添加一条UPNP
     * 
     * @param localIp 本地IP
     * @param localPort 本地端口
     * @param externalPort 外部端口
     * @param proto 协议
     * @param leaseDuration 租期
     * @param description 描述
     * @return true 
     * @return false 
     */
    bool addUPNP(const char *localIp, uint16_t localPort,
                 uint16_t externalPort, uint32_t proto,
                 uint32_t leaseDuration, const char *description = nullptr);

    // 删除一条UPNP
    void delUPNP(uint16_t externalPort, uint32_t proto);

    // 显示UPNP信息
    void displayUpnp() const;

private:
    std::shared_ptr<UPNPDeviceInfo> m_spDevice;
    bool    m_hasValidIGD = false;
};

} // namespace eular

#endif // __UPNP_UPNP_H_
