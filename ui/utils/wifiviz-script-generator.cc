/**
 * NS3 Script Generator (Standalone Version)
 * 根据 JSON 配置文件生成完全独立的 NS3 仿真脚本
 * 生成的脚本不依赖 JSON 文件，所有参数直接硬编码
 * 支持 CBR 和 OnOff 流量类型
 */

#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <filesystem>
#include <algorithm>
#include <map>
#include <set>
#include <stdexcept>
#include "nlohmann/json.hpp"

using json = nlohmann::json;
namespace fs = std::filesystem;

class NS3ScriptGenerator
{
public:
    NS3ScriptGenerator(const std::string& projectPath)
        : m_projectPath(projectPath)
    {
    }

    bool LoadConfigs()
    {
        m_apConfigs.clear();
        m_staConfigs.clear();

        // 加载 General.json
        fs::path generalPath = m_projectPath / "GeneralJson" / "General.json";
        if (!fs::exists(generalPath))
        {
            std::cerr << "Error: General config not found: " << generalPath << std::endl;
            return false;
        }

        std::ifstream generalFile(generalPath);
        if (!generalFile.is_open())
        {
            std::cerr << "Error: Cannot open " << generalPath << std::endl;
            return false;
        }
        m_generalConfig = json::parse(generalFile);

        const auto expectedApIds = CollectNodeIdsFromGeneral("Ap_pos_list");
        const auto expectedStaIds = CollectNodeIdsFromGeneral("Sta_pos_list");

        // 加载 AP 配置文件
        fs::path apDir = m_projectPath / "ApConfigJson";
        if (fs::exists(apDir))
        {
            std::vector<fs::path> apFiles;
            for (const auto& entry : fs::directory_iterator(apDir))
            {
                if (entry.path().extension() == ".json" &&
                    entry.path().filename().string().find("Ap_") == 0)
                {
                    apFiles.push_back(entry.path());
                }
            }
            std::sort(apFiles.begin(), apFiles.end());

            for (const auto& apFile : apFiles)
            {
                std::ifstream file(apFile);
                if (file.is_open())
                {
                    json apConfig = json::parse(file);
                    if (!ShouldKeepNodeConfig(apConfig, expectedApIds, "AP", apFile))
                    {
                        continue;
                    }
                    m_apConfigs.push_back(std::move(apConfig));
                }
            }
        }

        // 加载 STA 配置文件
        fs::path staDir = m_projectPath / "StaConfigJson";
        if (fs::exists(staDir))
        {
            std::vector<fs::path> staFiles;
            for (const auto& entry : fs::directory_iterator(staDir))
            {
                if (entry.path().extension() == ".json" &&
                    entry.path().filename().string().find("Sta_") == 0)
                {
                    staFiles.push_back(entry.path());
                }
            }
            std::sort(staFiles.begin(), staFiles.end());

            for (const auto& staFile : staFiles)
            {
                std::ifstream file(staFile);
                if (file.is_open())
                {
                    json staConfig = json::parse(file);
                    if (!ShouldKeepNodeConfig(staConfig, expectedStaIds, "STA", staFile))
                    {
                        continue;
                    }
                    m_staConfigs.push_back(std::move(staConfig));
                }
            }
        }

        DeduplicateConfigsById(m_apConfigs, "AP");
        DeduplicateConfigsById(m_staConfigs, "STA");
        SortConfigsById(m_apConfigs);
        SortConfigsById(m_staConfigs);

        if (!ValidateNodeCounts(expectedApIds, expectedStaIds))
        {
            return false;
        }

        std::cout << "Loaded " << m_apConfigs.size() << " AP config(s) and "
                  << m_staConfigs.size() << " STA config(s)." << std::endl;

        return true;
    }

    std::string GenerateScript()
    {
        std::ostringstream script;

        script << GenerateHeader();
        script << "\nint\nmain(int argc, char* argv[])\n{\n";
        script << "    bool enableWiFiViz = false;\n";
        script << "    bool precise = true;\n";
        script << "    uint32_t rough = 1;\n";
        script << "    CommandLine cmd(__FILE__);\n";
        script << "    cmd.AddValue(\"enable-wifiviz\", \"Enable WiFiViz timeline capture\", enableWiFiViz);\n";
        script << "    cmd.AddValue(\"precise\", \"Capture every PPDU without down-sampling\", precise);\n";
        script << "    cmd.AddValue(\"rough\", \"Down-sample PPDU timeline to 1/N\", rough);\n";
        script << "    cmd.Parse(argc, argv);\n";
        script << "    NS_LOG_INFO(\"Starting simulation...\");\n\n";
        script << GenerateConfigSection();
        script << "\n" << GenerateBuildingSection();
        script << "\n" << GenerateNodesSection();
        script << "\n" << GenerateWifiSection();
        script << "\n" << GenerateMobilitySection();
        script << "\n" << GenerateBuildingsHelperSection();
        script << "\n" << GenerateInternetSection();
        script << "\n" << GenerateTrafficFlows();
        script << GenerateSniffSection();
        script << GenerateSimulationSection();

        return script.str();
    }

    bool SaveScript(const std::string& outputPath)
    {
        std::string script;
        try
        {
            script = GenerateScript();
        }
        catch (const std::exception& ex)
        {
            std::cerr << "Error: " << ex.what() << std::endl;
            return false;
        }

        fs::path outputFile(outputPath);
        fs::create_directories(outputFile.parent_path());

        std::ofstream file(outputPath);
        if (!file.is_open())
        {
            std::cerr << "Error: Cannot create output file: " << outputPath << std::endl;
            return false;
        }

        file << script;
        file.close();

        std::cout << "Script generated successfully: " << outputPath << std::endl;
        return true;
    }

private:
    fs::path m_projectPath;
    json m_generalConfig;
    std::vector<json> m_apConfigs;
    std::vector<json> m_staConfigs;

    std::set<int> CollectNodeIdsFromGeneral(const char* key) const
    {
        std::set<int> ids;
        if (!m_generalConfig.contains(key) || !m_generalConfig[key].is_array())
        {
            return ids;
        }

        for (const auto& entry : m_generalConfig[key])
        {
            if (!entry.is_object() || !entry.contains("id"))
            {
                continue;
            }
            ids.insert(entry["id"].get<int>());
        }
        return ids;
    }

    bool ShouldKeepNodeConfig(const json& nodeConfig,
                              const std::set<int>& expectedIds,
                              const char* nodeType,
                              const fs::path& sourcePath) const
    {
        if (!nodeConfig.is_object())
        {
            std::cerr << "Warning: skip invalid " << nodeType << " config: " << sourcePath << std::endl;
            return false;
        }

        if (!nodeConfig.contains("Id") || !nodeConfig["Id"].is_number_integer())
        {
            std::cerr << "Warning: skip " << nodeType << " config without integer Id: "
                      << sourcePath << std::endl;
            return false;
        }

        if (!expectedIds.empty() && expectedIds.count(nodeConfig["Id"].get<int>()) == 0)
        {
            std::cerr << "Warning: skip stale " << nodeType << " config not referenced by General.json: "
                      << sourcePath << std::endl;
            return false;
        }

        return true;
    }

    void DeduplicateConfigsById(std::vector<json>& configs, const char* nodeType) const
    {
        std::map<int, json> deduped;
        for (const auto& cfg : configs)
        {
            deduped[cfg["Id"].get<int>()] = cfg;
        }

        if (deduped.size() != configs.size())
        {
            std::cerr << "Info: deduplicated " << nodeType << " configs from "
                      << configs.size() << " to " << deduped.size() << std::endl;
        }

        configs.clear();
        configs.reserve(deduped.size());
        for (auto& [id, cfg] : deduped)
        {
            configs.push_back(std::move(cfg));
        }
    }

    void SortConfigsById(std::vector<json>& configs) const
    {
        std::sort(configs.begin(), configs.end(), [](const json& lhs, const json& rhs) {
            return lhs.value("Id", -1) < rhs.value("Id", -1);
        });
    }

    bool ValidateNodeCounts(const std::set<int>& expectedApIds,
                            const std::set<int>& expectedStaIds) const
    {
        if (!expectedApIds.empty() && expectedApIds.size() != m_apConfigs.size())
        {
            std::cerr << "Error: AP config count mismatch. General.json references "
                      << expectedApIds.size() << " AP(s), but generator loaded "
                      << m_apConfigs.size() << "." << std::endl;
            return false;
        }

        if (!expectedStaIds.empty() && expectedStaIds.size() != m_staConfigs.size())
        {
            std::cerr << "Error: STA config count mismatch. General.json references "
                      << expectedStaIds.size() << " STA(s), but generator loaded "
                      << m_staConfigs.size() << "." << std::endl;
            return false;
        }

        return true;
    }

    std::string GenerateHeader()
    {
        return R"(/**
 * Auto-generated NS3 Simulation Script
 * This script is standalone and does not require JSON configuration files
 */

#include "ns3/applications-module.h"
#include "ns3/buildings-module.h"
#include "ns3/core-module.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/network-module.h"
#include "ns3/wifi-module.h"
#include "ns3/wifiviz.h"
#include <boost/interprocess/shared_memory_object.hpp>

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("GeneratedSimulation");
)";
    }

    std::string GenerateConfigSection()
    {
        std::ostringstream ss;
        
        // 获取仿真时长
        double duration = 10.0;
        if (m_generalConfig.contains("SimulationTime"))
        {
            duration = m_generalConfig["SimulationTime"].get<double>();
        }

        if (duration <= 0.0)
        {
            throw std::runtime_error("SimulationTime must be greater than 0.");
        }
        
        ss << "    // ========= Simulation Parameters =========\n";
        ss << "    double duration = " << duration << ";\n";
        
        return ss.str();
    }

    std::string GenerateBuildingSection()
    {
        std::ostringstream ss;
        
        if (m_generalConfig.contains("Building") && !m_generalConfig["Building"].is_null())
        {
            auto building = m_generalConfig["Building"];
            auto range = building.value("range", std::vector<double>{10, 10, 5});
            
            ss << "    // ========= Building =========\n";
            ss << "    Ptr<Building> building = CreateObject<Building>();\n";
            ss << "    building->SetBoundaries(Box(0, " << range[0] << ", 0, " << range[1] 
               << ", 0, " << range[2] << "));\n";
            ss << "    building->SetBuildingType(Building::Residential);\n";
            ss << "    building->SetExtWallsType(Building::ConcreteWithWindows);\n";
        }
        else
        {
            ss << "    // ========= Building =========\n";
            ss << "    Ptr<Building> building = CreateObject<Building>();\n";
            ss << "    building->SetBoundaries(Box(0, 10, 0, 10, 0, 5));\n";
            ss << "    building->SetBuildingType(Building::Residential);\n";
            ss << "    building->SetExtWallsType(Building::ConcreteWithWindows);\n";
        }
        
        return ss.str();
    }

    std::string GenerateNodesSection()
    {
        std::ostringstream ss;
        ss << "    // ========= Nodes =========\n";
        ss << "    NodeContainer apNodes;\n";
        ss << "    apNodes.Create(" << m_apConfigs.size() << ");\n\n";
        ss << "    NodeContainer staNodes;\n";
        ss << "    staNodes.Create(" << m_staConfigs.size() << ");\n";
        return ss.str();
    }

    std::string GenerateWifiSection()
    {
        std::ostringstream ss;
        const auto staToAp = BuildStaToApAssociation();
        
        ss << "    // ========= Wi-Fi Configuration =========\n";
        ss << "    WifiHelper wifi;\n";
        ss << "    WifiMacHelper mac;\n\n";
        
        ss << "    const uint32_t apCount = " << m_apConfigs.size() << ";\n";
        ss << "    std::vector<YansWifiPhyHelper> phys;\n";
        ss << "    phys.reserve(apCount);\n\n";
        
        // 为每个 AP 创建独立信道
        ss << "    // Create independent channels for each AP\n";
        ss << "    for (uint32_t i = 0; i < apCount; ++i)\n";
        ss << "    {\n";
        ss << "        YansWifiChannelHelper channel = YansWifiChannelHelper::Default();\n";
        ss << "        Ptr<YansWifiChannel> channelPtr = channel.Create();\n\n";
        ss << "        YansWifiPhyHelper phy;\n";
        ss << "        phy.SetChannel(channelPtr);\n";
        ss << "        phys.push_back(phy);\n";
        ss << "    }\n\n";
        
        ss << "    NetDeviceContainer apDevices;\n";
        ss << "    NetDeviceContainer staDevices;\n\n";
        
        // 为每个 AP 配置 WiFi
        ss << "    // Configure APs\n";
        for (size_t i = 0; i < m_apConfigs.size(); ++i)
        {
            const auto& ap = m_apConfigs[i];
            std::string ssid = ap.value("Ssid", "ns3-wifi");
            std::string standard = ap.value("Standard", "802.11ax");
            std::string rateCtrl = ap.value("Rate_ctrl_algo", "Minstrel");
            int txPower = ap.value("Tx_power", 20);
            int bandwidth = ap.value("Bandwidth", 20);
            int apId = GetNodeId(ap, static_cast<int>(i));
            
            std::string wifiStandard = ConvertWifiStandard(standard);
            
            ss << "    // AP " << apId << " (" << ssid << ")\n";
            ss << "    {\n";
            ss << "        wifi.SetStandard(" << wifiStandard << ");\n";
            ss << "        wifi.SetRemoteStationManager(\"ns3::" << rateCtrl << "WifiManager\");\n\n";
            
            ss << "        Ssid ssid(\"" << ssid << "\");\n";
            ss << "        mac.SetType(\"ns3::ApWifiMac\",\n";
            ss << "                    \"Ssid\", SsidValue(ssid),\n";
            ss << "                    \"QosSupported\", BooleanValue(true));\n\n";
            
            ss << "        phys[" << i << "].Set(\"TxPowerStart\", DoubleValue(" << txPower << "));\n";
            ss << "        phys[" << i << "].Set(\"TxPowerEnd\", DoubleValue(" << txPower << "));\n";
            ss << "        phys[" << i << "].Set(\"ChannelSettings\", StringValue(\"{0, " << bandwidth << ", BAND_5GHZ, 0}\"));\n\n";
            
            ss << "        NetDeviceContainer apDev = wifi.Install(phys[" << i << "], mac, apNodes.Get(" << i << "));\n";
            ss << "        apDevices.Add(apDev);\n";
            ss << "    }\n\n";
        }
        
        // 为每个 STA 配置 WiFi
        ss << "    // Configure STAs\n";
        for (size_t i = 0; i < m_staConfigs.size(); ++i)
        {
            const auto& sta = m_staConfigs[i];
            size_t apIdx = 0;
            auto assocIt = staToAp.find(i);
            if (assocIt != staToAp.end())
            {
                apIdx = assocIt->second;
            }
            
            const auto& ap = m_apConfigs[apIdx];
            std::string ssid = ap.value("Ssid", "ns3-wifi");
            std::string standard = sta.value("Standard", ap.value("Standard", "802.11ax"));
            std::string rateCtrl = sta.value("Rate_ctrl_algo", "Minstrel");
            int txPower = sta.value("Tx_power", 20);
            int staId = GetNodeId(sta, static_cast<int>(i));
            
            std::string wifiStandard = ConvertWifiStandard(standard);
            
            ss << "    // STA " << staId << "\n";
            ss << "    {\n";
            ss << "        wifi.SetStandard(" << wifiStandard << ");\n";
            ss << "        wifi.SetRemoteStationManager(\"ns3::" << rateCtrl << "WifiManager\");\n\n";
            
            ss << "        Ssid ssid(\"" << ssid << "\");\n";
            ss << "        mac.SetType(\"ns3::StaWifiMac\",\n";
            ss << "                    \"Ssid\", SsidValue(ssid),\n";
            ss << "                    \"QosSupported\", BooleanValue(true),\n";
            ss << "                    \"ActiveProbing\", BooleanValue(true));\n\n";
            
            ss << "        phys[" << apIdx << "].Set(\"TxPowerStart\", DoubleValue(" << txPower << "));\n";
            ss << "        phys[" << apIdx << "].Set(\"TxPowerEnd\", DoubleValue(" << txPower << "));\n\n";
            
            ss << "        NetDeviceContainer staDev = wifi.Install(phys[" << apIdx << "], mac, staNodes.Get(" << i << "));\n";
            ss << "        staDevices.Add(staDev);\n";
            ss << "    }\n\n";
        }
        
        return ss.str();
    }
    
    std::string ConvertWifiStandard(const std::string& standard)
    {
        if (standard == "802.11a") return "WIFI_STANDARD_80211a";
        if (standard == "802.11b") return "WIFI_STANDARD_80211b";
        if (standard == "802.11g") return "WIFI_STANDARD_80211g";
        if (standard == "802.11n") return "WIFI_STANDARD_80211n";
        if (standard == "802.11ac") return "WIFI_STANDARD_80211ac";
        if (standard == "802.11ax") return "WIFI_STANDARD_80211ax";
        return "WIFI_STANDARD_80211ax";
    }

    int GetNodeId(const json& nodeConfig, int fallback) const
    {
        return nodeConfig.value("Id", fallback);
    }

    std::map<int, size_t> BuildApIdToIndex() const
    {
        std::map<int, size_t> result;
        for (size_t i = 0; i < m_apConfigs.size(); ++i)
        {
            result[GetNodeId(m_apConfigs[i], static_cast<int>(i))] = i;
        }
        return result;
    }

    std::map<int, size_t> BuildStaIdToIndex() const
    {
        std::map<int, size_t> result;
        for (size_t i = 0; i < m_staConfigs.size(); ++i)
        {
            result[GetNodeId(m_staConfigs[i], static_cast<int>(i))] = i;
        }
        return result;
    }

    std::map<size_t, size_t> BuildStaToApAssociation() const
    {
        std::map<size_t, size_t> association;
        std::map<std::string, size_t> ssidToAp;

        for (size_t i = 0; i < m_apConfigs.size(); ++i)
        {
            ssidToAp[m_apConfigs[i].value("Ssid", "")] = i;
        }

        for (size_t i = 0; i < m_staConfigs.size(); ++i)
        {
            const std::string staSsid = m_staConfigs[i].value("Ssid", "");
            auto it = ssidToAp.find(staSsid);
            if (it != ssidToAp.end())
            {
                association[i] = it->second;
            }
            else if (!m_apConfigs.empty())
            {
                association[i] = i % m_apConfigs.size();
            }
        }

        return association;
    }

    std::string GenerateMobilitySection()
    {
        std::ostringstream ss;
        
        ss << "    // ========= Mobility =========\n";
        ss << "    MobilityHelper mobility;\n";
        ss << "    mobility.SetMobilityModel(\"ns3::ConstantPositionMobilityModel\");\n\n";
        
        // AP 位置
        if (!m_apConfigs.empty())
        {
            ss << "    // AP Positions\n";
            ss << "    mobility.Install(apNodes);\n";
            for (size_t i = 0; i < m_apConfigs.size(); ++i)
            {
                if (m_apConfigs[i].contains("Position"))
                {
                    auto pos = m_apConfigs[i]["Position"];
                    double x = pos[0].get<double>();
                    double y = pos[1].get<double>();
                    double z = pos[2].get<double>();
                    
                    ss << "    apNodes.Get(" << i << ")->GetObject<MobilityModel>()"
                       << "->SetPosition(Vector(" << x << ", " << y << ", " << z << "));\n";
                }
            }
            ss << "\n";
        }
        
        // STA 位置
        if (!m_staConfigs.empty())
        {
            ss << "    // STA Positions\n";
            ss << "    mobility.Install(staNodes);\n";
            for (size_t i = 0; i < m_staConfigs.size(); ++i)
            {
                if (m_staConfigs[i].contains("Position"))
                {
                    auto pos = m_staConfigs[i]["Position"];
                    double x = pos[0].get<double>();
                    double y = pos[1].get<double>();
                    double z = pos[2].get<double>();
                    
                    ss << "    staNodes.Get(" << i << ")->GetObject<MobilityModel>()"
                       << "->SetPosition(Vector(" << x << ", " << y << ", " << z << "));\n";
                }
            }
            ss << "\n";
        }
        
        return ss.str();
    }

    std::string GenerateBuildingsHelperSection()
    {
        return R"(    // ========= Buildings =========
    BuildingsHelper::Install(apNodes);
    BuildingsHelper::Install(staNodes);
)";
    }

    std::string GenerateInternetSection()
    {
        std::ostringstream ss;
        const auto staToAp = BuildStaToApAssociation();

        ss << R"(    // ========= Internet =========
    InternetStackHelper stack;
    stack.Install(apNodes);
    stack.Install(staNodes);

    Ipv4AddressHelper address;
    Ipv4InterfaceContainer apIfaces;
    Ipv4InterfaceContainer staIfaces;
    std::vector<Ipv4Address> staAddrs(staNodes.GetN());

    for (uint32_t i = 0; i < apCount; ++i)
    {
        std::string base = "10.1." + std::to_string(i + 1) + ".0";
        address.SetBase(Ipv4Address(base.c_str()), "255.255.255.0");

        NetDeviceContainer devs;
        devs.Add(apDevices.Get(i));

        std::vector<uint32_t> staIndices;
)";

        for (size_t staIdx = 0; staIdx < m_staConfigs.size(); ++staIdx)
        {
            const size_t apIdx = staToAp.count(staIdx) ? staToAp.at(staIdx) : 0;
            ss << "        if (i == " << apIdx << ")\n";
            ss << "        {\n";
            ss << "            devs.Add(staDevices.Get(" << staIdx << "));\n";
            ss << "            staIndices.push_back(" << staIdx << ");\n";
            ss << "        }\n";
        }

        ss << R"(

        auto ifaces = address.Assign(devs);
        apIfaces.Add(ifaces.Get(0));
        for (uint32_t k = 0; k < staIndices.size(); ++k)
        {
            staIfaces.Add(ifaces.Get(k + 1));
            staAddrs[staIndices[k]] = ifaces.GetAddress(k + 1);
        }
    }

    Ipv4GlobalRoutingHelper::PopulateRoutingTables();
)";
        return ss.str();
    }

    std::string GenerateTrafficFlows()
    {
        std::ostringstream ss;
        const auto apIdToIndex = BuildApIdToIndex();
        const auto staIdToIndex = BuildStaIdToIndex();
        
        // 收集所有流量配置
        struct FlowInfo
        {
            std::string sourceType;
            int sourceIdx;
            std::string dest;
            json flow;
        };
        
        std::vector<FlowInfo> flows;
        
        // 收集 AP 流量
        for (size_t i = 0; i < m_apConfigs.size(); ++i)
        {
            if (m_apConfigs[i].contains("Traffic") && m_apConfigs[i]["Traffic"].contains("Flows"))
            {
                for (const auto& flow : m_apConfigs[i]["Traffic"]["Flows"])
                {
                    flows.push_back({"AP", GetNodeId(m_apConfigs[i], static_cast<int>(i)),
                                   flow.value("Destination", ""), flow});
                }
            }
        }
        
        // 收集 STA 流量
        for (size_t i = 0; i < m_staConfigs.size(); ++i)
        {
            if (m_staConfigs[i].contains("Traffic") && m_staConfigs[i]["Traffic"].contains("Flows"))
            {
                for (const auto& flow : m_staConfigs[i]["Traffic"]["Flows"])
                {
                    flows.push_back({"STA", GetNodeId(m_staConfigs[i], static_cast<int>(i)),
                                   flow.value("Destination", ""), flow});
                }
            }
        }
        
        if (flows.empty())
        {
            // 使用默认流量模式
            return GenerateDefaultTraffic();
        }
        
        ss << "    // ========= Applications (Traffic Flows) =========\n";
        ss << "    const uint16_t basePort = 9000;\n\n";
        
        // 为每个唯一目标创建服务器
        std::map<std::string, int> destToPort;
        int portCounter = 0;
        
        for (const auto& flowInfo : flows)
        {
            if (destToPort.find(flowInfo.dest) == destToPort.end())
            {
                destToPort[flowInfo.dest] = portCounter++;
            }
        }
        
        // 创建服务器
        ss << "    // Create packet sinks (servers)\n";
        for (const auto& [dest, portOffset] : destToPort)
        {
            std::string destType, destIdx;
            size_t pos = dest.find('_');
            if (pos != std::string::npos)
            {
                destType = dest.substr(0, pos);
                destIdx = dest.substr(pos + 1);
            }
            
            if (destType == "STA")
            {
                const int staId = std::stoi(destIdx);
                auto it = staIdToIndex.find(staId);
                if (it == staIdToIndex.end())
                {
                    continue;
                }
                ss << "    // Server on " << dest << "\n";
                ss << "    {\n";
                ss << "        PacketSinkHelper sink(\"ns3::UdpSocketFactory\", "
                   << "InetSocketAddress(Ipv4Address::GetAny(), basePort + " << portOffset << "));\n";
                ss << "        ApplicationContainer serverApps = sink.Install(staNodes.Get(" << it->second << "));\n";
                ss << "        serverApps.Start(Seconds(0.5));\n";
                ss << "        serverApps.Stop(Seconds(duration));\n";
                ss << "    }\n\n";
            }
            else if (destType == "AP")
            {
                const int apId = std::stoi(destIdx);
                auto it = apIdToIndex.find(apId);
                if (it == apIdToIndex.end())
                {
                    continue;
                }
                ss << "    // Server on " << dest << "\n";
                ss << "    {\n";
                ss << "        PacketSinkHelper sink(\"ns3::UdpSocketFactory\", "
                   << "InetSocketAddress(Ipv4Address::GetAny(), basePort + " << portOffset << "));\n";
                ss << "        ApplicationContainer serverApps = sink.Install(apNodes.Get(" << it->second << "));\n";
                ss << "        serverApps.Start(Seconds(0.5));\n";
                ss << "        serverApps.Stop(Seconds(duration));\n";
                ss << "    }\n\n";
            }
        }
        
        // 创建客户端（流量发送方）
        ss << "    // Create traffic sources\n";
        int flowCount = 0;
        for (const auto& flowInfo : flows)
        {
            std::string destType, destIdx;
            size_t pos = flowInfo.dest.find('_');
            if (pos != std::string::npos)
            {
                destType = flowInfo.dest.substr(0, pos);
                destIdx = flowInfo.dest.substr(pos + 1);
            }
            
            std::string flowType = flowInfo.flow.value("FlowType", "CBR");
            double startTime = flowInfo.flow.value("StartTime", 1.0);
            
            std::string endTimeStr = "duration";
            if (flowInfo.flow.contains("EndTime") && !flowInfo.flow["EndTime"].is_null())
            {
                if (flowInfo.flow["EndTime"].is_number())
                {
                    endTimeStr = std::to_string(flowInfo.flow["EndTime"].get<double>());
                }
            }
            
            std::string destAddr;
            if (destType == "STA")
            {
                const int staId = std::stoi(destIdx);
                auto it = staIdToIndex.find(staId);
                if (it == staIdToIndex.end())
                {
                    continue;
                }
                destAddr = "staAddrs[" + std::to_string(it->second) + "]";
            }
            else if (destType == "AP")
            {
                const int apId = std::stoi(destIdx);
                auto it = apIdToIndex.find(apId);
                if (it == apIdToIndex.end())
                {
                    continue;
                }
                destAddr = "apIfaces.GetAddress(" + std::to_string(it->second) + ")";
            }
            else
            {
                continue;
            }
            
            std::string sourceNode;
            if (flowInfo.sourceType == "AP")
            {
                auto it = apIdToIndex.find(flowInfo.sourceIdx);
                if (it == apIdToIndex.end())
                {
                    continue;
                }
                sourceNode = "apNodes.Get(" + std::to_string(it->second) + ")";
            }
            else
            {
                auto it = staIdToIndex.find(flowInfo.sourceIdx);
                if (it == staIdToIndex.end())
                {
                    continue;
                }
                sourceNode = "staNodes.Get(" + std::to_string(it->second) + ")";
            }
            
            int portOffset = destToPort[flowInfo.dest];
            
            ss << "    // Flow " << (++flowCount) << ": " << flowInfo.sourceType << "_" 
               << flowInfo.sourceIdx << " -> " << flowInfo.dest << " (" << flowType << ")\n";
            ss << "    {\n";
            
            if (flowType == "CBR")
            {
                // CBR 使用 UdpClient
                int packetSize = flowInfo.flow.value("PacketSize", 1024);
                double interval = flowInfo.flow.value("Interval", 0.01);
                int maxPackets = flowInfo.flow.value("MaxPackets", 0);
                uint32_t maxPacketsVal = maxPackets > 0 ? maxPackets : 0xFFFFFFFF;
                
                ss << "        UdpClientHelper client(" << destAddr << ", basePort + " << portOffset << ");\n";
                ss << "        client.SetAttribute(\"MaxPackets\", UintegerValue(" << maxPacketsVal << "));\n";
                ss << "        client.SetAttribute(\"Interval\", TimeValue(Seconds(" << interval << ")));\n";
                ss << "        client.SetAttribute(\"PacketSize\", UintegerValue(" << packetSize << "));\n";
                ss << "        \n";
                ss << "        ApplicationContainer clientApps = client.Install(" << sourceNode << ");\n";
                ss << "        clientApps.Start(Seconds(" << startTime << "));\n";
                ss << "        clientApps.Stop(Seconds(" << endTimeStr << "));\n";
            }
            else if (flowType == "OnOff")
            {
                // OnOff 使用 OnOffApplication
                int packetSize = flowInfo.flow.value("PacketSize", 1024);
                double dataRate = flowInfo.flow.value("DataRate", 1.0); // Mbps
                int maxBytes = flowInfo.flow.value("MaxBytes", 0);
                
                std::string onTimeType = "ConstantRandomVariable";
                std::string offTimeType = "ConstantRandomVariable";
                std::string onTimeParams = "Constant=1.0";
                std::string offTimeParams = "Constant=0.0"; // 解析 OnTime 参数
                if (flowInfo.flow.contains("OntimeType"))
                {
                    std::string ontimeType = flowInfo.flow["OntimeType"].get<std::string>();
                    if (ontimeType == "Constant")
                    {
                        onTimeType = "ConstantRandomVariable";
                        if (flowInfo.flow.contains("OntimeParams") && 
                            flowInfo.flow["OntimeParams"].contains("Value"))
                        {
                            double mean = flowInfo.flow["OntimeParams"]["Value"].get<double>();
                            onTimeParams = "Constant=" + std::to_string(mean);
                        }
                    }
                    else if (ontimeType == "Exponential")
                    {
                        onTimeType = "ExponentialRandomVariable";
                        if (flowInfo.flow.contains("OntimeParams") && 
                            flowInfo.flow["OntimeParams"].contains("Mean"))
                        {
                            double mean = flowInfo.flow["OntimeParams"]["Mean"].get<double>();
                            onTimeParams = "Mean=" + std::to_string(mean);
                        }
                    }
                    else if (ontimeType == "Uniform")
                    {
                        onTimeType = "UniformRandomVariable";
                        if (flowInfo.flow.contains("OntimeParams"))
                        {
                            double min = flowInfo.flow["OntimeParams"].value("Min", 0.0);
                            double max = flowInfo.flow["OntimeParams"].value("Max", 1.0);
                            onTimeParams = "Min=" + std::to_string(min) + "|Max=" + std::to_string(max);
                        }
                    }
                    else if (ontimeType == "Gaussian" || ontimeType == "Normal")
                    {
                        onTimeType = "NormalRandomVariable";
                        if (flowInfo.flow.contains("OntimeParams"))
                        {
                            double mean = flowInfo.flow["OntimeParams"].value("Mean", 1.0);
                            double variance = flowInfo.flow["OntimeParams"].value("Variance", 0.1);
                            double bound = flowInfo.flow["OntimeParams"].value("Bound", mean * 10);
                            onTimeParams = "Mean=" + std::to_string(mean) + "|Variance=" + 
                                         std::to_string(variance) + "|Bound=" + std::to_string(bound);
                        }
                    }
                }
                
                // 解析 OffTime 参数
                if (flowInfo.flow.contains("OfftimeType"))
                {
                    std::string offtimeType = flowInfo.flow["OfftimeType"].get<std::string>();
                    if (offtimeType == "Constant")
                    {
                        offTimeType = "ConstantRandomVariable";
                        if (flowInfo.flow.contains("OfftimeParams") && 
                            flowInfo.flow["OfftimeParams"].contains("Value"))
                        {
                            double mean = flowInfo.flow["OfftimeParams"]["Value"].get<double>();
                            offTimeParams = "Constant=" + std::to_string(mean);
                        }
                    }
                    else if (offtimeType == "Exponential")
                    {
                        offTimeType = "ExponentialRandomVariable";
                        if (flowInfo.flow.contains("OfftimeParams") && 
                            flowInfo.flow["OfftimeParams"].contains("Mean"))
                        {
                            double mean = flowInfo.flow["OfftimeParams"]["Mean"].get<double>();
                            offTimeParams = "Mean=" + std::to_string(mean);
                        }
                    }
                    else if (offtimeType == "Uniform")
                    {
                        offTimeType = "UniformRandomVariable";
                        if (flowInfo.flow.contains("OfftimeParams"))
                        {
                            double min = flowInfo.flow["OfftimeParams"].value("Min", 0.0);
                            double max = flowInfo.flow["OfftimeParams"].value("Max", 1.0);
                            offTimeParams = "Min=" + std::to_string(min) + "|Max=" + std::to_string(max);
                        }
                    }
                }
                
                ss << "        OnOffHelper onoff(\"ns3::UdpSocketFactory\", "
                   << "Address(InetSocketAddress(" << destAddr << ", basePort + " << portOffset << ")));\n";
                ss << "        onoff.SetAttribute(\"PacketSize\", UintegerValue(" << packetSize << "));\n";
                ss << "        onoff.SetAttribute(\"DataRate\", DataRateValue(DataRate(\"" 
                   << dataRate << "Mbps\")));\n";
                ss << "        onoff.SetAttribute(\"MaxBytes\", UintegerValue(" << maxBytes << "));\n";
                ss << "        onoff.SetAttribute(\"OnTime\", StringValue(\"ns3::" << onTimeType 
                   << "[" << onTimeParams << "]\"));\n";
                ss << "        onoff.SetAttribute(\"OffTime\", StringValue(\"ns3::" << offTimeType 
                   << "[" << offTimeParams << "]\"));\n";
                ss << "        \n";
                ss << "        ApplicationContainer clientApps = onoff.Install(" << sourceNode << ");\n";
                ss << "        clientApps.Start(Seconds(" << startTime << "));\n";
                ss << "        clientApps.Stop(Seconds(" << endTimeStr << "));\n";
            }
            else if (flowType == "Bulk")
            {
                // Bulk 使用 BulkSendApplication
                int maxBytes = flowInfo.flow.value("MaxBytes", 0);
                int packetSize = flowInfo.flow.value("PacketSize", 512);
                
                ss << "        BulkSendHelper bulk(\"ns3::TcpSocketFactory\", "
                   << "Address(InetSocketAddress(" << destAddr << ", basePort + " << portOffset << ")));\n";
                ss << "        bulk.SetAttribute(\"SendSize\", UintegerValue(" << packetSize << "));\n";
                ss << "        bulk.SetAttribute(\"MaxBytes\", UintegerValue(" << maxBytes << "));\n";
                ss << "        \n";
                ss << "        ApplicationContainer clientApps = bulk.Install(" << sourceNode << ");\n";
                ss << "        clientApps.Start(Seconds(" << startTime << "));\n";
                ss << "        clientApps.Stop(Seconds(" << endTimeStr << "));\n";
            }
            
            ss << "    }\n\n";
        }
        
        return ss.str();
    }

    std::string GenerateDefaultTraffic()
    {
        return R"(    // Using default traffic pattern (bidirectional)
    const uint16_t basePortUp = 10000;

    // One server per STA (AP sends downlink to every STA)
    for (uint32_t i = 0; i < staNodes.GetN(); ++i)
    {
        const uint16_t port = basePort + i;
        PacketSinkHelper sink("ns3::UdpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), port));
        ApplicationContainer serverApps = sink.Install(staNodes.Get(i));
        serverApps.Start(Seconds(0.5));
        serverApps.Stop(Seconds(duration));
    }

    // One server per AP (STAs send uplink to APs)
    for (uint32_t i = 0; i < apNodes.GetN(); ++i)
    {
        const uint16_t port = basePortUp + i;
        PacketSinkHelper sink("ns3::UdpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), port));
        ApplicationContainer serverApps = sink.Install(apNodes.Get(i));
        serverApps.Start(Seconds(0.5));
        serverApps.Stop(Seconds(duration));
    }

    // Each AP sends to all its associated STAs (downlink)
    for (uint32_t apIdx = 0; apIdx < apNodes.GetN(); ++apIdx)
    {
        for (uint32_t staIdx = 0; staIdx < staNodes.GetN(); ++staIdx)
        {
            if (apNodes.GetN() != 0 && (staIdx % apNodes.GetN()) != apIdx)
            {
                continue;
            }

            const uint16_t port = basePort + staIdx;
            const Ipv4Address staAddr = staAddrs[staIdx];

            UdpClientHelper client(staAddr, port);
            client.SetAttribute("MaxPackets", UintegerValue(0xFFFFFFFF));
            client.SetAttribute("Interval", TimeValue(MilliSeconds(80)));
            client.SetAttribute("PacketSize", UintegerValue(1200));

            ApplicationContainer clientApps = client.Install(apNodes.Get(apIdx));
            clientApps.Start(Seconds(1.0 + 0.001 * staIdx));
            clientApps.Stop(Seconds(duration));
        }
    }

    // Each STA sends to its associated AP (uplink), creating cross traffic
    for (uint32_t staIdx = 0; staIdx < staNodes.GetN(); ++staIdx)
    {
        const uint32_t apIdx = apNodes.GetN() == 0 ? 0 : (staIdx % apNodes.GetN());
        const uint16_t port = basePortUp + apIdx;
        const Ipv4Address apAddr = apIfaces.GetAddress(apIdx);

        UdpClientHelper client(apAddr, port);
        client.SetAttribute("MaxPackets", UintegerValue(0xFFFFFFFF));
        client.SetAttribute("Interval", TimeValue(MilliSeconds(80)));
        client.SetAttribute("PacketSize", UintegerValue(1000));

        ApplicationContainer clientApps = client.Install(staNodes.Get(staIdx));
        clientApps.Start(Seconds(1.0 + 0.002 * staIdx));
        clientApps.Stop(Seconds(duration));
    }
)";
    }

    std::string GenerateSniffSection()
    {
        return R"(
    // ========= Packet Sniffing =========
    NetDeviceContainer allDevices = WiFiVizHelper::MergeDevices(apDevices, staDevices);
    WiFiVizHelper::ConfigureVisualizerSampling(precise, rough);
    Ptr<SniffUtils> Sniff_Utils = WiFiVizHelper::MaybeEnableVisualizer(enableWiFiViz, allDevices, duration, true);
)";
    }

    std::string GenerateSimulationSection()
    {
        return R"(    // ========= Run Simulation =========
    NS_LOG_INFO("Starting simulation for " << duration << " seconds");
    Simulator::Stop(Seconds(duration));
    Simulator::Run();
    NS_LOG_INFO("Simulation finished");
    Simulator::Destroy();

    return 0;
}
)";
    }
};

int main(int argc, char* argv[])
{
    if (argc < 2)
    {
        std::cout << "Usage: " << argv[0] << " <project_path> [output_script_path]" << std::endl;
        std::cout << "Example: " << argv[0] 
                  << " ./MySimulationProject ./output/my-simulation.cc" << std::endl;
        std::cout << "\nThis generator creates standalone NS3 scripts that don't require JSON files at runtime." << std::endl;
        std::cout << "All configuration is hardcoded into the generated script." << std::endl;
        return 1;
    }

    std::string projectPath = argv[1];
    std::string outputPath;

    if (argc >= 3)
    {
        outputPath = argv[2];
    }
    else
    {
        fs::path projPath(projectPath);
        outputPath = "./generated_" + projPath.filename().string() + ".cc";
    }

    NS3ScriptGenerator generator(projectPath);
    
    std::cout << "Loading configurations from: " << projectPath << std::endl;
    if (!generator.LoadConfigs())
    {
        std::cerr << "Failed to load configurations" << std::endl;
        return 1;
    }

    std::cout << "Generating standalone NS3 script..." << std::endl;
    if (!generator.SaveScript(outputPath))
    {
        std::cerr << "Failed to save script" << std::endl;
        return 1;
    }

    std::cout << "\n=== Generation Complete ===" << std::endl;
    std::cout << "The generated script is standalone and does not require JSON files." << std::endl;
    std::cout << "To compile and run:" << std::endl;
    std::cout << "  cd <ns3-directory>" << std::endl;
    std::cout << "  ./ns3 build" << std::endl;
    std::cout << "  ./ns3 run " << fs::path(outputPath).stem().string() << std::endl;

    return 0;
}
