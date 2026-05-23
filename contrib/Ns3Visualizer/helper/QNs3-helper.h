#ifndef QNS3_HELPER_H
#define QNS3_HELPER_H

#include "ns3/QNs3.h"
#include "ns3/ap-wifi-mac.h"
#include "ns3/applications-module.h"
#include "ns3/core-module.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/network-module.h"
#include "ns3/qos-txop.h"
#include "ns3/sta-wifi-mac.h"
#include "ns3/wifi-mac.h"
#include "ns3/wifi-module.h"
#include "ns3/wifi-net-device.h"
#include "ns3/wifi-phy-band.h"
#include "ns3/wifi-phy.h"
#include "ns3/SniffUtils.h"
#include "ns3/wifi-remote-station-manager.h"
#include "ns3/wifi-types.h"
#include "ns3/wifi-units.h"

#include <algorithm>
#include <iomanip>
#include <iostream>
#include <map>
#include <stdexcept>
#include <string>
#include <unordered_map>

namespace ns3
{

class QNs3Helper
{
  public:
    QNs3Helper() = default;
    ~QNs3Helper() = default;
    static WifiStandard Configure_WifiStandard(const std::string& standard);
    static void Configure_GI(const Ptr<WifiPhy>& phy, const int gi, const std::string& standard);
    static std::string BuildChannelSettings(const NodeConfig& cfg);
    static std::string Configure_RateCtrlManager(std::optional<std::string> rate_ctrl_manager,
                           std::optional<std::string> standard = std::nullopt);
    static WifiChannelConfig Configure_BandChannel(const NodeConfig& node);
    static void ConfigurePhy(const NodeConfig& cfg, const Ptr<WifiNetDevice>& device);
    static void ConfigureRtsCts(const NodeConfig& cfg, const Ptr<WifiNetDevice>& device);
    static void ConfigureStaMac(const NodeConfig& cfg, const Ptr<WifiNetDevice>& device);
    static void ConfigureQos(const NodeConfig& cfg, Ptr<WifiMac> qosMac);
    static void ConfigureApMac(const NodeConfig& cfg, const Ptr<WifiNetDevice>& device);
    static void ConfigureMobility(const std::vector<NodeConfig>& configs,
                                  const NodeContainer& container);
    static void ApplyDeviceConfigs(const std::vector<NodeConfig>& configs,
                                   const NetDeviceContainer& devices);
    static NetDeviceContainer MergeDevices(const NetDeviceContainer& first,
                                           const NetDeviceContainer& second);
    static void ConfigureVisualizerSampling(bool precise, uint32_t rough);
    static bool LaunchTimelineViewerAsync();
    static Ptr<SniffUtils> EnableVisualizer(const NetDeviceContainer& devices,
                                            double simulationTime,
                                            bool launchViewer = false);
    static Ptr<SniffUtils> MaybeEnableVisualizer(bool enabled,
                                                 const NetDeviceContainer& devices,
                                                 double simulationTime,
                                                 bool launchViewer = false);
    static Building::BuildingType_t GetBuildingType(const GeneralConfig& cfg);
    static Building::ExtWallsType_t GetBuildingExtWallsType(const GeneralConfig& cfg);
};

} // namespace ns3

#endif // QNS3_HELPER_H
