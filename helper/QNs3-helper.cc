#include "QNs3-helper.h"
#include "ns3/wifi-phy-operating-channel.h"

#include <boost/interprocess/shared_memory_object.hpp>
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <string>
#include <unordered_set>
#include <vector>

#ifdef _WIN32
#include <windows.h>
#else
#include <sys/wait.h>
#include <unistd.h>
#endif

namespace ns3
{

namespace
{

void
SetEnvironmentVariableValue(const char* name, const std::string& value)
{
#ifdef _WIN32
    _putenv_s(name, value.c_str());
#else
    setenv(name, value.c_str(), 1);
#endif
}

void
ClearEnvironmentVariableValue(const char* name)
{
#ifdef _WIN32
    _putenv_s(name, "");
#else
    unsetenv(name);
#endif
}

std::filesystem::path
WiFiVizExecutableName()
{
#ifdef _WIN32
    return "WiFiVizApp.exe";
#else
    return "WiFiVizApp";
#endif
}

std::filesystem::path
FindWiFiVizApp()
{
    namespace fs = std::filesystem;
    const fs::path cwd = fs::current_path();
    const fs::path appName = WiFiVizExecutableName();
    const std::vector<fs::path> candidates = {
        cwd / "build" / appName,
        cwd / appName,
#ifdef __APPLE__
        cwd / "build" / "WiFiVizApp.app" / "Contents" / "MacOS" / "WiFiVizApp",
        cwd / "WiFiVizApp.app" / "Contents" / "MacOS" / "WiFiVizApp",
#endif
    };

    for (const auto& candidate : candidates)
    {
        if (fs::exists(candidate))
        {
            return candidate;
        }
    }
    return {};
}

bool
LaunchProcessDetached(const std::filesystem::path& executable)
{
#ifdef _WIN32
    std::wstring commandLine = L"\"" + executable.wstring() + L"\" --timeline-only";
    STARTUPINFOW startupInfo{};
    startupInfo.cb = sizeof(startupInfo);
    PROCESS_INFORMATION processInfo{};
    const BOOL ok = CreateProcessW(nullptr,
                                   commandLine.data(),
                                   nullptr,
                                   nullptr,
                                   FALSE,
                                   0,
                                   nullptr,
                                   nullptr,
                                   &startupInfo,
                                   &processInfo);
    if (!ok)
    {
        return false;
    }
    CloseHandle(processInfo.hThread);
    CloseHandle(processInfo.hProcess);
    return true;
#else
    const pid_t pid = fork();
    if (pid < 0)
    {
        return false;
    }
    if (pid == 0)
    {
        const pid_t childPid = fork();
        if (childPid < 0)
        {
            _exit(127);
        }
        if (childPid > 0)
        {
            _exit(0);
        }

        const std::string path = executable.string();
        execl(path.c_str(), path.c_str(), "--timeline-only", static_cast<char*>(nullptr));
        _exit(127);
    }
    int status = 0;
    if (waitpid(pid, &status, 0) < 0)
    {
        return false;
    }
    if (!WIFEXITED(status) || WEXITSTATUS(status) != 0)
    {
        return false;
    }
    return true;
#endif
}

} // namespace

static const std::unordered_map<std::string, WifiStandard> standard_map = {
    {"802.11n", WIFI_STANDARD_80211n},
    {"802.11b", WIFI_STANDARD_80211b},
    {"802.11ac", WIFI_STANDARD_80211ac},
    {"802.11ax", WIFI_STANDARD_80211ax},
    {"802.11g", WIFI_STANDARD_80211g},
    {"802.11a", WIFI_STANDARD_80211a},
};

WifiStandard
QNs3Helper::Configure_WifiStandard(const std::string& standard)
{
    const auto it = standard_map.find(standard);
    if (it == standard_map.end())
    {
        NS_FATAL_ERROR("Unsupported Wifi standard: " << standard);
        return WIFI_STANDARD_80211n;
    }
    return it->second;
}

void
QNs3Helper::Configure_GI(const Ptr<WifiPhy>& phy, const int gi, const std::string& standard)
{
    auto it = standard_map.find(standard);
    auto standard_enum = WIFI_STANDARD_80211n;
    if (it != standard_map.end())
    {
        standard_enum = it->second;
    }

    const int giValue = gi;

    switch (standard_enum)
    {
    case WIFI_STANDARD_80211a:
    case WIFI_STANDARD_80211b:
    case WIFI_STANDARD_80211g:
        break;
    case WIFI_STANDARD_80211n: {
        if (giValue == 400 && phy && phy->GetDevice() && phy->GetDevice()->GetHtConfiguration())
        {
            Config::SetDefault("ns3::HtConfiguration::ShortGuardIntervalSupported",
                               BooleanValue(true));
        }
        break;
    }
    case WIFI_STANDARD_80211ac: {
        if (giValue == 400 && phy && phy->GetDevice() && phy->GetDevice()->GetVhtConfiguration())
        {
            Config::SetDefault("ns3::VhtConfiguration::ShortGuardIntervalSupported",
                               BooleanValue(true));
        }
        break;
    }
    case WIFI_STANDARD_80211ax: {
        if (phy && phy->GetDevice())
        {
            auto HE = phy->GetDevice()->GetHeConfiguration();
            if (HE)
            {
                int heGi = giValue;
                if (heGi != 800 && heGi != 1600 && heGi != 3200)
                {
                    heGi = 800;
                }
                HE->SetGuardInterval(NanoSeconds(heGi));
            }
        }
        break;
    }
    default:
        break;
    }
}

std::string
QNs3Helper::Configure_RateCtrlManager(std::optional<std::string> rate_ctrl_manager,
                                      std::optional<std::string> standard)
{
    static const std::unordered_map<std::string, std::string> manager_map = {
        {"Aarf", "ns3::AarfWifiManager"},
        {"Aarfcd", "ns3::AarfcdWifiManager"},
        {"Amrr", "ns3::AmrrWifiManager"},
        {"Arf", "ns3::ArfWifiManager"},
        {"Cara", "ns3::CaraWifiManager"},
        {"Rraa", "ns3::RraaWifiManager"},
        {"Onoe", "ns3::OnoeWifiManager"},
        {"Minstrel", "ns3::MinstrelWifiManager"},
        {"MinstrelHt", "ns3::MinstrelHtWifiManager"},
        {"Ideal", "ns3::IdealWifiManager"},
        {"Constant", "ns3::ConstantRateWifiManager"},
        {"ConstantRate", "ns3::ConstantRateWifiManager"},
        {"ThompsonSampling", "ns3::ThompsonSamplingWifiManager"},
        {"ThomsonSampling", "ns3::ThompsonSamplingWifiManager"},
    };

    auto is_ht_like = [](const std::optional<std::string>& stdName) {
        if (!stdName.has_value())
        {
            return false;
        }
        return *stdName == "802.11n" || *stdName == "802.11ac" || *stdName == "802.11ax";
    };

    if (rate_ctrl_manager.has_value())
    {
        const auto it = manager_map.find(*rate_ctrl_manager);
        if (it == manager_map.end())
        {
            NS_FATAL_ERROR("Unsupported rate control manager: " << *rate_ctrl_manager
                                                                << ".Defaulting to Aarf");
            return "ns3::AarfWifiManager";
        }
        if (*rate_ctrl_manager == "Minstrel" && is_ht_like(standard))
        {
            return "ns3::MinstrelHtWifiManager";
        }
        return it->second;
    }
    else
    {
        return is_ht_like(standard) ? "ns3::MinstrelHtWifiManager"
                                    : "ns3::AarfWifiManager";
    }
}

std::string
QNs3Helper::BuildChannelSettings(const NodeConfig& cfg)
{
    const auto band = (cfg.Frequency <= 3.0) ? "BAND_2_4GHZ" : "BAND_5GHZ";
    std::cout << (cfg.Frequency >= 3.0) << std::endl;
    std::ostringstream oss;
    oss << "{" << cfg.Channel_number << ", " << cfg.Bandwidth << ", " << band << ", 0}";
    std::cout << oss.str() << std::endl;
    return oss.str();
}

WifiPhyBand
Determine_Band(const double& frequency)
{
    if (frequency >= 5925 && frequency <= 7125)
    {
        return WIFI_PHY_BAND_6GHZ;
    }
    else if (frequency >= 4900 && frequency < 5925)
    {
        return WIFI_PHY_BAND_5GHZ;
    }
    else if (frequency >= 2400 && frequency <= 2500)
    {
        return WIFI_PHY_BAND_2_4GHZ;
    }
    return WIFI_PHY_BAND_UNSPECIFIED;
}

uint8_t CalcPrimary20Index(uint32_t bandwidthMHz)
{
    if (bandwidthMHz < 20 || bandwidthMHz % 20 != 0)
        throw std::runtime_error("Invalid channel bandwidth");

    // default: lowest 20 MHz as primary
    return 0;
}


WifiChannelConfig
QNs3Helper::Configure_BandChannel(const NodeConfig& nodeconfig)
{
    auto standardFromString = [](const std::string& standard) {
        const auto it = standard_map.find(standard);
        if (it != standard_map.end())
        {
            return it->second;
        }
        return WIFI_STANDARD_80211n;
    };

    auto collectChannels = [](WifiPhyBand band, uint32_t bw) {
        std::vector<uint8_t> channels;
        const auto targetWidth = MHz_u{static_cast<double>(bw)};
        for (const auto& info : WifiPhyOperatingChannel::GetFrequencyChannels())
        {
            if (info.band != band)
            {
                continue;
            }
            if (info.width != targetWidth)
            {
                continue;
            }
            channels.push_back(info.number);
        }
        std::sort(channels.begin(), channels.end());
        channels.erase(std::unique(channels.begin(), channels.end()), channels.end());
        return channels;
    };

    auto pickNearestChannel = [](const std::vector<uint8_t>& channels, int channelNumber) -> uint8_t {
        if (channels.empty())
        {
            return 0;
        }
        uint8_t best = channels.front();
        int bestDiff = std::abs(channelNumber - static_cast<int>(best));
        for (auto ch : channels)
        {
            const int diff = std::abs(channelNumber - static_cast<int>(ch));
            if (diff < bestDiff)
            {
                best = ch;
                bestDiff = diff;
            }
        }
        return best;
    };

    const auto standardEnum = standardFromString(nodeconfig.Standard);

    uint32_t bw = static_cast<uint32_t>(nodeconfig.Bandwidth);
    if (bw != 20 && bw != 40 && bw != 80 && bw != 160)
    {
        bw = 20;
    }

    WifiPhyBand band = Determine_Band(nodeconfig.Frequency);

    // If band is unspecified or mismatched, infer from channel number.
    const int channelNumber = nodeconfig.Channel_number;
    if (band == WIFI_PHY_BAND_UNSPECIFIED)
    {
        const auto ch24 = collectChannels(WIFI_PHY_BAND_2_4GHZ, bw);
        const auto ch5 = collectChannels(WIFI_PHY_BAND_5GHZ, bw);
        const auto ch6 = collectChannels(WIFI_PHY_BAND_6GHZ, bw);

        if (std::find(ch24.begin(), ch24.end(), channelNumber) != ch24.end())
        {
            band = WIFI_PHY_BAND_2_4GHZ;
        }
        else if (std::find(ch5.begin(), ch5.end(), channelNumber) != ch5.end())
        {
            band = WIFI_PHY_BAND_5GHZ;
        }
        else if (std::find(ch6.begin(), ch6.end(), channelNumber) != ch6.end())
        {
            band = WIFI_PHY_BAND_6GHZ;
        }
    }

    if (band == WIFI_PHY_BAND_UNSPECIFIED)
    {
        band = WIFI_PHY_BAND_5GHZ;
    }

    // Validate bandwidth for band; fallback to a supported width
    auto validChannels = collectChannels(band, bw);
    if (validChannels.empty())
    {
        if (band == WIFI_PHY_BAND_2_4GHZ)
        {
            bw = (bw == 40) ? 40 : 20;
        }
        else
        {
            bw = 20;
        }
        validChannels = collectChannels(band, bw);
    }

    uint8_t chosenChannel = 0;
    if (!validChannels.empty())
    {
        if (std::find(validChannels.begin(), validChannels.end(), channelNumber) != validChannels.end())
        {
            chosenChannel = static_cast<uint8_t>(channelNumber);
        }
        else
        {
            chosenChannel = pickNearestChannel(validChannels, channelNumber);
        }
    }
    else
    {
        try
        {
            chosenChannel = WifiPhyOperatingChannel::GetDefaultChannelNumber(
                MHz_u{static_cast<double>(bw)}, standardEnum, band);
        }
        catch (const std::exception&)
        {
            chosenChannel = (band == WIFI_PHY_BAND_2_4GHZ) ? 1 : 36;
        }
    }

    uint8_t p20Index = CalcPrimary20Index(bw);
    WifiChannelConfig::Segment segment(static_cast<uint8_t>(chosenChannel),
                                       MHz_u{static_cast<double>(bw)},
                                       band,
                                       p20Index);
    return WifiChannelConfig(segment);
}

AcIndex
ToAcIndex(const std::string& ac)
{
    static const std::unordered_map<std::string, AcIndex> kAcMap{
        {"AC_BE", AC_BE},
        {"AC_BK", AC_BK},
        {"AC_VI", AC_VI},
        {"AC_VO", AC_VO},
    };
    const auto it = kAcMap.find(ac);
    if (it == kAcMap.end())
    {
        NS_ABORT_MSG("Unknown access class: " << ac);
    }
    return it->second;
}

void
QNs3Helper::ConfigurePhy(const NodeConfig& cfg, const Ptr<WifiNetDevice>& device)
{
    auto phy = device->GetPhy();
    phy->SetOperatingChannel(Configure_BandChannel(cfg));
    phy->SetTxPowerStart(cfg.Tx_power);
    phy->SetTxPowerEnd(cfg.Tx_power);
    Configure_GI(phy, cfg.Guard_interval, cfg.Standard);
    //Not recommended to set slot and sifs
    if (cfg.Slot)
    {
        phy->SetSlot(MicroSeconds(*cfg.Slot));
    }
    if (cfg.Sifs)
    {
        phy->SetSifs(MicroSeconds(*cfg.Sifs));
    }
    if (cfg.RxSensitivity)
    {
        phy->SetRxSensitivity(*cfg.RxSensitivity);
    }
    if (cfg.CcaEdThreshold)
    {
        phy->SetCcaEdThreshold(*cfg.CcaEdThreshold);
    }
    if (cfg.CcaSensitivity)
    {
        phy->SetCcaSensitivityThreshold(*cfg.CcaSensitivity);
    }
}

void
QNs3Helper::ConfigureRtsCts(const NodeConfig& cfg, const Ptr<WifiNetDevice>& device)
{
    auto manager = device->GetRemoteStationManager();
    manager->SetRtsCtsThreshold(cfg.Rts_Cts.Threshold);
    manager->SetFragmentationThreshold(2200);
}

void
QNs3Helper::ConfigureQos(const NodeConfig& cfg, Ptr<WifiMac> mac)
{
    if (!mac || !mac->GetQosSupported())
    {
        return;
    }

    for (const auto& kv : cfg.qos.Edca_params)
    {
        AcIndex ac = ToAcIndex(kv.first);

        Ptr<QosTxop> txop = mac->GetQosTxop(ac);
        if (!txop)
        {
            continue;
        }

        const auto& params = kv.second;
        txop->SetMinCw(params.CWmin);
        txop->SetMaxCw(params.CWmax);
        txop->SetAifsn(params.AIFSN);

        uint32_t txop32 = (params.TXOPLimit + 31) / 32 * 32;
        txop->SetTxopLimit(MicroSeconds(txop32));
    }
}

void
QNs3Helper::ConfigureApMac(const NodeConfig& cfg, const Ptr<WifiNetDevice>& device)
{
    auto mac = DynamicCast<ApWifiMac>(device->GetMac());
    if (!mac)
    {
        return;
    }

    ConfigureQos(cfg, device->GetMac());

    if (cfg.beacon)
    {
        const auto& beacon = *cfg.beacon;

        uint32_t interval_tu = static_cast<uint32_t>(beacon.BeaconInterval * 1000.0 / 1024.0 + 0.5);
        interval_tu = std::max(1u, interval_tu);
        mac->SetBeaconInterval(MicroSeconds(interval_tu * 1024));

        mac->SetAttribute("BeaconGeneration", BooleanValue(beacon.set));
        mac->SetAttribute("EnableBeaconJitter", BooleanValue(beacon.EnableBeaconJitter));
    }
}

void
QNs3Helper::ConfigureStaMac(const NodeConfig& cfg, const Ptr<WifiNetDevice>& device)
{
    auto mac = DynamicCast<StaWifiMac>(device->GetMac());
    if (!mac)
    {
        return;
    }
    ConfigureQos(cfg, DynamicCast<WifiMac>(mac));
    if (cfg.ActiveProbing)
    {
        mac->SetAttribute("ActiveProbing", BooleanValue(*cfg.ActiveProbing));
        // mac->SetActiveProbing(*cfg.ActiveProbing);
    }
    if (cfg.MaxMissedBeacons)
    {
        mac->SetAttribute("MaxMissedBeacons", UintegerValue(*cfg.MaxMissedBeacons));
        // mac->SetMaxMissedBeacons(*cfg.MaxMissedBeacons);
    }
    if (cfg.ProbeRequestTimeout)
    {
        mac->SetAttribute("ProbeRequestTimeout", TimeValue(MilliSeconds(*cfg.ProbeRequestTimeout)));
        // mac->SetProbeRequestTimeout(MilliSeconds(*cfg.ProbeRequestTimeout));
    }
}

void
QNs3Helper::ConfigureMobility(const std::vector<NodeConfig>& configs,
                              const NodeContainer& container)
{
    Ptr<ListPositionAllocator> allocator = CreateObject<ListPositionAllocator>();
    for (const auto& cfg : configs)
    {
        const double x = cfg.Position.size() > 0 ? cfg.Position[0] : 0.0;
        const double y = cfg.Position.size() > 1 ? cfg.Position[1] : 0.0;
        const double z = cfg.Position.size() > 2 ? cfg.Position[2] : 0.0;
        allocator->Add(Vector(x, y, z));
    }
    MobilityHelper mobility;
    mobility.SetPositionAllocator(allocator);
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(container);
}

void
QNs3Helper::ApplyDeviceConfigs(const std::vector<NodeConfig>& configs,
                               const NetDeviceContainer& devices)
{
    const std::size_t count = std::min<std::size_t>(configs.size(), devices.GetN());
    for (std::size_t i = 0; i < count; ++i)
    {
        auto wifiDev = DynamicCast<WifiNetDevice>(devices.Get(i));
        if (!wifiDev)
        {
            continue;
        }
        ConfigurePhy(configs[i], wifiDev);
        ConfigureRtsCts(configs[i], wifiDev);
        if (configs[i].NodeType == "AP")
        {
            ConfigureApMac(configs[i], wifiDev);
        }
        else
        {
            ConfigureStaMac(configs[i], wifiDev);
        }
    }
}

NetDeviceContainer
QNs3Helper::MergeDevices(const NetDeviceContainer& first, const NetDeviceContainer& second)
{
    NetDeviceContainer merged;
    std::unordered_set<const NetDevice*> seen;

    auto appendUnique = [&](const NetDeviceContainer& source) {
        for (auto it = source.Begin(); it != source.End(); ++it)
        {
            Ptr<NetDevice> device = *it;
            if (!device)
            {
                continue;
            }
            if (!seen.insert(PeekPointer(device)).second)
            {
                continue;
            }
            merged.Add(device);
        }
    };

    appendUnique(first);
    appendUnique(second);
    return merged;
}

void
QNs3Helper::ConfigureVisualizerSampling(bool precise, uint32_t rough)
{
    if (precise)
    {
        SetEnvironmentVariableValue("WIFIVIZ_PRECISE", "1");
        SetEnvironmentVariableValue("WIFIVIZ_SAMPLE_RATE", "1");
        return;
    }

    ClearEnvironmentVariableValue("WIFIVIZ_PRECISE");
    if (rough > 1)
    {
        SetEnvironmentVariableValue("WIFIVIZ_SAMPLE_RATE", std::to_string(rough));
        return;
    }
    ClearEnvironmentVariableValue("WIFIVIZ_SAMPLE_RATE");
}

bool
QNs3Helper::LaunchTimelineViewerAsync()
{
    const auto appPath = FindWiFiVizApp();
    if (appPath.empty())
    {
        std::cerr << "WiFiViz: cannot find WiFiVizApp in the current ns-3 directory" << std::endl;
        return false;
    }
    if (!LaunchProcessDetached(appPath))
    {
        std::cerr << "WiFiViz: failed to launch " << appPath << std::endl;
        return false;
    }
    return true;
}

Ptr<SniffUtils>
QNs3Helper::EnableVisualizer(const NetDeviceContainer& devices,
                             double simulationTime,
                             bool launchViewer)
{
    using namespace boost::interprocess;
    shared_memory_object::remove("Ns3PpduSharedMemory");

    const char* disableViewerEnv = std::getenv("WIFIVIZ_DISABLE_VIEWER");
    const bool viewerDisabledByEnv =
        (disableViewerEnv != nullptr && std::string(disableViewerEnv) == "1");

    if (launchViewer && !viewerDisabledByEnv)
    {
        LaunchTimelineViewerAsync();
    }

    Ptr<SniffUtils> sniffUtils = CreateObject<SniffUtils>();
    if (!sniffUtils->Initialize(devices, simulationTime))
    {
        return nullptr;
    }
    return sniffUtils;
}

Ptr<SniffUtils>
QNs3Helper::MaybeEnableVisualizer(bool enabled,
                                  const NetDeviceContainer& devices,
                                  double simulationTime,
                                  bool launchViewer)
{
    if (!enabled)
    {
        return nullptr;
    }
    return EnableVisualizer(devices, simulationTime, launchViewer);
}

Building::BuildingType_t
QNs3Helper::GetBuildingType(const GeneralConfig& cfg)
{
    if (cfg.BuildingType == "Residential")
    {
        return Building::Residential;
    }
    else if (cfg.BuildingType == "Office")
    {
        return Building::Office;
    }
    else if (cfg.BuildingType == "Commercial")
    {
        return Building::Commercial;
    }
    else
    {
        return Building::Residential;
    }
}

Building::ExtWallsType_t
QNs3Helper::GetBuildingExtWallsType(const GeneralConfig& cfg)
{
    if (cfg.WallType == "Wood")
    {
        return Building::Wood;
    }
    else if (cfg.WallType == "ConcreteWithWindows")
    {
        return Building::ConcreteWithWindows;
    }
    else if (cfg.WallType == "ConcreteWithoutWindows")
    {
        return Building::ConcreteWithoutWindows;
    }
    else if (cfg.WallType == "StoneBlocks")
    {
        return Building::StoneBlocks;
    }
    else
    {
        return Building::Wood;
    }
}

} // namespace ns3
