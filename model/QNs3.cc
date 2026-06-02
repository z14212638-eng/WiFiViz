#include "QNs3.h"

#include "nlohmann/json.hpp"

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <type_traits>

#define CHECK_ARRAY(j, key)                                                                        \
    if (!(j).contains(key) || !(j).at(key).is_array())                                             \
    throw std::runtime_error(std::string(key) + " is not array")

#define CHECK_OBJECT(j, key)                                                                       \
    if (!(j).contains(key) || !(j).at(key).is_object())                                            \
    throw std::runtime_error(std::string(key) + " is not object")

namespace fs = std::filesystem;

namespace ns3
{
using json = nlohmann::json;
using namespace ns3;

// Turns a string to lowercase in a copy form
static std::string
ToLowerCopy(std::string value)
{
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char c) {
        return std::tolower(c);
    });
    return value;
}

// Indentation
static std::string
Indent(int level)
{
    return std::string(static_cast<std::size_t>(level) * 2, ' ');
}

// boolean to string
static std::string
BoolToString(bool value)
{
    return value ? "true" : "false";
}

// value to string
template <typename T>
std::string
ValueToString(const T& value)
{
    if constexpr (std::is_same<T, bool>::value)
    {
        return value ? "true" : "false";
    }
    else if constexpr (std::is_same<T, std::string>::value)
    {
        return value;
    }
    else
    {
        return std::to_string(value);
    }
}

// Optional value to string
template <typename T>
static std::string
OptionalValueToString(const std::optional<T>& value)
{
    if (!value.has_value())
    {
        return "<unset>";
    }
    return ValueToString(*value);
}

// check a general formed bool value (also used in optional bool parsing)
static bool
ParseBoolToken(const json& value)
{
    if (value.is_boolean())
    {
        return value.get<bool>();
    }
    if (value.is_string())
    {
        const auto lowered = ToLowerCopy(value.get<std::string>());
        if (lowered == "true" || lowered == "yes" || lowered == "on")
        {
            return true;
        }
        if (lowered == "false" || lowered == "no" || lowered == "off")
        {
            return false;
        }
        throw std::runtime_error(std::string("Unexpected boolean string :  ") + lowered);
    }
    if (value.is_number_integer())
    {
        return value.get<int>() != 0;
    }
    throw std::runtime_error(std::string("Unexpected boolean value type : ") + value.type_name());
}

// Get a general optional value
template <typename T>
static std::optional<T>
GetOptionalValue(const json& j, const char* key)
{
    if (!j.contains(key))
    {
        return std::nullopt;
    }
    else if (j.at(key).is_null())
    {
        return std::nullopt;
    }
    return j.at(key).get<T>();
}

// Join values in a string with a seprator
template <typename Container>
std::string
JoinValues(const Container& c, char sep = ',', bool space = true)
{
    std::ostringstream oss;
    bool first = true;

    for (const auto& value : c)
    {
        if (!first)
        {
            oss << sep;
            if (space)
            {
                oss << ' ';
            }
        }
        first = false;

        oss << value; 
    }
    return oss.str();
}

// Mobility Adapter
void
from_json(const json& j, Mobility& m)
{
    j.at("set").get_to(m.set);
    j.at("boundaries").get_to(m.boundaries);
    j.at("distance_change_interval").get_to(m.distance_change_interval);
    j.at("time_change_interval").get_to(m.time_change_interval);
    j.at("mode").get_to(m.mode);
    j.at("model_type").get_to(m.model_type);
    j.at("velocity").get_to(m.velocity);
}

// RtsCts Adapter
void
from_json(const json& j, RtsCts& rtsCts)
{
    j.at("set").get_to(rtsCts.set);
    j.at("Threshold").get_to(rtsCts.Threshold);
}

// EdcaParam Adapter
void
from_json(const json& j, EdcaParams& edcaParam)
{
    j.at("CWmin").get_to(edcaParam.CWmin);
    j.at("CWmax").get_to(edcaParam.CWmax);
    j.at("AIFSN").get_to(edcaParam.AIFSN);
    j.at("TXOPLimit").get_to(edcaParam.TXOPLimit);
}

// MsduAggregation Adapter
void
from_json(const json& j, MsduAggregation& msduAggregation)
{
    j.at("set").get_to(msduAggregation.set);
    j.at("type").get_to(msduAggregation.type);
    j.at("MaxAmsduSize").get_to(msduAggregation.MaxAmsduSize);
}

// MpduAggregation Adapter
void
from_json(const json& j, MpduAggregation& mpduAggregation)
{
    j.at("set").get_to(mpduAggregation.set);
    j.at("type").get_to(mpduAggregation.type);
    j.at("MaxAmpduSize").get_to(mpduAggregation.MaxAmpduSize);
    j.at("Density").get_to(mpduAggregation.Density);
}

// Qos Adapter
void
from_json(const json& j, Qos& qos)
{
    j.at("Qos_support").get_to(qos.Qos_support);
    j.at("Edca_choose").get_to(qos.Edca_choose);
    j.at("Edca_params").get_to(qos.Edca_params);
    j.at("Msdu_Aggregation").get_to(qos.Msdu_Aggregation);
    j.at("Mpdu_Aggregation").get_to(qos.Mpdu_Aggregation);
}

// AntennaElement Adapter
void
from_json(const nlohmann::json& j, AntennaElement& element)
{
    if (j.contains("type") && !j.at("type").is_null())
    {
        element.type = j.at("type").get<std::string>();
    }

    if (j.contains("MaxGain") && !j.at("MaxGain").is_null())
    {
        element.MaxGain = j.at("MaxGain").get<int>();
    }

    if (j.contains("Beamwidth") && !j.at("Beamwidth").is_null())
    {
        element.Beamwidth = j.at("Beamwidth").get<int>();
    }
}

// Antenna Adapter
void
from_json(const nlohmann::json& j, Antenna& a)
{
    j.at("set").get_to(a.set);
    if (j.contains("Antennas") && !j.at("Antennas").is_null())
    {
        CHECK_ARRAY(j, "Antennas"); //[CHECK]
        a.Antennas = j.at("Antennas").get<std::vector<AntennaElement>>();
    }
    else
    {
        a.Antennas.clear();
    }
}

// Beacons Adapter
void
from_json(const nlohmann::json& j, Beacon& b)
{
    b.set = ParseBoolToken(j.at("set"));
    b.BeaconInterval = j.at("BeaconInterval").get<int>();
    b.Beacon_Rate = j.at("Beacon_Rate").get<int>();
    b.EnableBeaconJitter = ParseBoolToken(j.at("EnableBeaconJitter"));
}

// AC_VI
void
from_json(const json& j, Struct_AC_VI& ac_vi)
{
    j.at("enabled").get_to(ac_vi.enabled);
    j.at("packetSize").get_to(ac_vi.packetSize);
    j.at("trafficType").get_to(ac_vi.trafficType);
    j.at("meanDataRate").get_to(ac_vi.meanDataRate);
    j.at("peakDataRate").get_to(ac_vi.peakDataRate);
}

// AC_VO
void
from_json(const json& j, Struct_AC_VO& ac_vo)
{
    j.at("enabled").get_to(ac_vo.enabled);
    j.at("packetSize").get_to(ac_vo.packetSize);
    j.at("trafficType").get_to(ac_vo.trafficType);
    j.at("interval").get_to(ac_vo.interval);
}

// AC_BE
void
from_json(const json& j, Struct_AC_BE& ac_be)
{
    j.at("enabled").get_to(ac_be.enabled);
    j.at("packetSize").get_to(ac_be.packetSize);
    j.at("trafficType").get_to(ac_be.trafficType);
    j.at("DataRate").get_to(ac_be.DataRate);
}

// AC_BK
void
from_json(const json& j, Struct_AC_BK& ac_bk)
{
    j.at("enabled").get_to(ac_bk.enabled);
    j.at("packetSize").get_to(ac_bk.packetSize);
    j.at("trafficType").get_to(ac_bk.trafficType);
    j.at("DataRate").get_to(ac_bk.DataRate);
}

// Flow Adapter
void
from_json(const json& j, Flow& f)
{
    j.at("FlowId").get_to(f.FlowId);
    j.at("Protocol").get_to(f.Protocol);
    j.at("StartTime").get_to(f.StartTime);
    j.at("EndTime").get_to(f.EndTime);
    j.at("Direction").get_to(f.Direction);

    if (j.contains("EDCA_Traffic"))
    {
        for (const auto& ac : j.at("EDCA_Traffic"))
        {
            std::string type = ac.at("type").get<std::string>();
            if (type == "AC_VI")
            {
                from_json(ac, f.AC_VI_flow);
            }

            else if (type == "AC_VO")
            {
                from_json(ac, f.AC_VO_flow);
            }

            else if (type == "AC_BE")
            {
                from_json(ac, f.AC_BE_flow);
            }

            else if (type == "AC_BK")
            {
                from_json(ac, f.AC_BK_flow);
            }
        }
    }
}

// NodeConfig Adapter
void
from_json(const json& j, NodeConfig& nodeConfig)
{
    if (j.contains("Beacon"))
    {
        nodeConfig.NodeType = "AP";
    }
    else
    {
        nodeConfig.NodeType = "STA";
    }

    j.at("Position").get_to(nodeConfig.Position);
    j.at("Standard").get_to(nodeConfig.Standard);
    j.at("Guard_interval").get_to(nodeConfig.Guard_interval);
    j.at("Mobility").get_to(nodeConfig.mobility);
    j.at("Id").get_to(nodeConfig.Id);
    j.at("Channel_number").get_to(nodeConfig.Channel_number);
    j.at("Frequency").get_to(nodeConfig.Frequency);
    j.at("Qos").get_to(nodeConfig.qos);
    j.at("Antenna").get_to(nodeConfig.antenna);
    j.at("Bandwidth").get_to(nodeConfig.Bandwidth);
    j.at("Tx_power").get_to(nodeConfig.Tx_power);
    j.at("TxQueue").get_to(nodeConfig.TxQueue);
    j.at("Rts_Cts").get_to(nodeConfig.Rts_Cts);
    j.at("Ssid").get_to(nodeConfig.Ssid);
    j.at("Phy_model").get_to(nodeConfig.Phy_model);
    for (const auto& flowJson : j.at("Traffic").at("Flows"))
    {
        Flow f;
        from_json(flowJson, f);
        nodeConfig.flows.push_back(f);
    }

    if (j.contains("RxSensitivity"))
    {
        nodeConfig.RxSensitivity = GetOptionalValue<int>(j, "RxSensitivity");
    }

    else
    {
        nodeConfig.RxSensitivity = std::nullopt;
    }

    if (j.contains("Rate_ctrl_algo"))
    {
        nodeConfig.Rate_ctrl_algo = GetOptionalValue<std::string>(j, "Rate_ctrl_algo");
    }
    else
    {
        nodeConfig.Rate_ctrl_algo = std::nullopt;
    }

    if (j.contains("CcaEdThreshold"))
    {
        nodeConfig.CcaEdThreshold = GetOptionalValue<int>(j, "CcaEdThreshold");
    }
    else
    {
        nodeConfig.CcaEdThreshold = std::nullopt;
    }

    if (j.contains("CcaSensitivity"))
    {
        nodeConfig.CcaSensitivity = GetOptionalValue<int>(j, "CcaSensitivity");
    }
    else
    {
        nodeConfig.CcaSensitivity = std::nullopt;
    }

    if (j.contains("Beacon"))
    {
        nodeConfig.beacon = GetOptionalValue<Beacon>(j, "Beacon");
    }
    else
    {
        nodeConfig.beacon = std::nullopt;
    }

    if (j.contains("Sifs"))
    {
        nodeConfig.Sifs = GetOptionalValue<int>(j, "Sifs");
    }
    else
    {
        nodeConfig.Sifs = std::nullopt;
    }

    if (j.contains("Slot"))
    {
        nodeConfig.Slot = GetOptionalValue<int>(j, "Slot");
    }
    else
    {
        nodeConfig.Slot = std::nullopt;
    }

    if(j.contains("EnableAssocFailRetry"))
    {
        nodeConfig.EnableAssocFailRetry = GetOptionalValue<bool>(j, "EnableAssocFailRetry");
    }
    else
    {
        nodeConfig.EnableAssocFailRetry = std::nullopt; 
    }

    if(j.contains("MaxMissedBeacons"))
    {
        nodeConfig.MaxMissedBeacons = GetOptionalValue<int>(j, "MaxMissedBeacons");
    }
    else
    {
        nodeConfig.MaxMissedBeacons = std::nullopt;
    }

    if(j.contains("ProbeRequestTimeout"))
    {
        nodeConfig.ProbeRequestTimeout = GetOptionalValue<int>(j, "ProbeRequestTimeout");
    }
    else
    {
        nodeConfig.ProbeRequestTimeout = std::nullopt;
    }
}

NodeConfig
LoadNodeConfigsFromFile(const std::string& path)
{
    std::ifstream in(path);
    if (!in.is_open())
    {
        throw std::runtime_error("Failed to open JSON file: " + path);
    }

    nlohmann::json data;
    in >> data;

    if (!data.is_object())
    {
        throw std::runtime_error("Expected a JSON object for NodeConfig");
    }

    return data.get<NodeConfig>();
}

void from_json(const json& j, GeneralConfig& generalConfig)   
{
    j.at("Building").at("building_type").get_to(generalConfig.BuildingType);
    j.at("Building").at("range").get_to(generalConfig.range);
    j.at("Building").at("wall_type").get_to(generalConfig.WallType);
    j.at("SimulationTime").get_to(generalConfig.SimulationTime);
}   

GeneralConfig
LoadGeneralConfigFromFile(const std::string& path)
{
    std::ifstream in(path);
    if (!in.is_open())
    {
        throw std::runtime_error("Failed to open JSON file: " + path);  
    }

    nlohmann::json data;
    in >> data;

    if (!data.is_object())
    {
        throw std::runtime_error("Expected a JSON object for BuildingConfig");
    }

    return data.get<GeneralConfig>();
}

/**
 * @brief Print Helper
 *
 */
std::ostream&
PrintMobility(const Mobility& mobility, std::ostream& os, int indent)
{
    const auto pad = Indent(indent);
    os << pad << "Mobility:\n";
    os << pad << "  set: " << BoolToString(mobility.set) << '\n';
    os << pad << "  model_type: " << mobility.model_type << '\n';
    os << pad << "  velocity: " << mobility.velocity << '\n';
    os << pad << "  distance_change_interval: " << mobility.distance_change_interval << '\n';
    os << pad << "  time_change_interval: " << mobility.time_change_interval << '\n';
    os << pad << "  boundaries: [" << JoinValues(mobility.boundaries) << "]\n";
    os << pad << "  mode: " << mobility.mode << '\n';
    return os;
}

std::ostream&
PrintRtsCts(const RtsCts& rts, std::ostream& os, int indent)
{
    const auto pad = Indent(indent);
    os << pad << "Rts_Cts:\n";
    os << pad << "  set: " << BoolToString(rts.set) << '\n';
    os << pad << "  Threshold: " << rts.Threshold << '\n';
    return os;
}

std::ostream&
PrintEdcaParams(const EdcaParams& params, std::ostream& os, int indent)
{
    const auto pad = Indent(indent);
    os << pad << "CWmin: " << params.CWmin << '\n';
    os << pad << "CWmax: " << params.CWmax << '\n';
    os << pad << "AIFSN: " << params.AIFSN << '\n';
    os << pad << "TXOPLimit: " << params.TXOPLimit << '\n';
    return os;
}

std::ostream&
PrintAggregation(const MsduAggregation& msdu,
                 const MpduAggregation& mpdu,
                 std::ostream& os,
                 int indent)
{
    const auto pad = Indent(indent);

    os << pad << "Msdu_Aggregation:\n";
    os << pad << "  set: " << BoolToString(msdu.set) << '\n';
    os << pad << "  type: " << msdu.type << '\n';
    os << pad << "  MaxAmsduSize: " << msdu.MaxAmsduSize << '\n';

    os << pad << "Mpdu_Aggregation:\n";
    os << pad << "  set: " << BoolToString(mpdu.set) << '\n';
    os << pad << "  type: " << mpdu.type << '\n';
    os << pad << "  MpduDensity: " << mpdu.Density << '\n';
    os << pad << "  MaxAmpduSize: " << mpdu.MaxAmpduSize << '\n';

    return os;
}

std::ostream&
PrintQos(const Qos& qos, std::ostream& os, int indent)
{
    const auto pad = Indent(indent);
    os << pad << "Qos:\n";
    os << pad << "  Qos_support: " << BoolToString(qos.Qos_support) << '\n';
    os << pad << "  Edca_choose: " << qos.Edca_choose << '\n';
    os << pad << "  Edca_params:\n";

    for (const auto& [accessClass, params] : qos.Edca_params)
    {
        os << pad << "    [" << accessClass << "]\n";
        PrintEdcaParams(params, os, indent + 3);
    }

    PrintAggregation(qos.Msdu_Aggregation, qos.Mpdu_Aggregation, os, indent + 1);
    return os;
}

std::ostream&
PrintAntennaElement(const AntennaElement& element, std::ostream& os, int indent, std::size_t index)
{
    const auto pad = Indent(indent);
    os << pad << "Antenna[" << index << "]:\n";
    os << pad << "  type: " << element.type << '\n';
    os << pad << "  MaxGain: " << element.MaxGain << '\n';
    os << pad << "  Beamwidth: " << element.Beamwidth << '\n';
    return os;
}

std::ostream&
PrintAntenna(const Antenna& antenna, std::ostream& os, int indent)
{
    const auto pad = Indent(indent);
    os << pad << "Antenna:\n";
    os << pad << "  set: " << BoolToString(antenna.set) << '\n';
    os << pad << "  Antennas_count: " << antenna.Antennas.size() << '\n';

    for (std::size_t idx = 0; idx < antenna.Antennas.size(); ++idx)
    {
        PrintAntennaElement(antenna.Antennas[idx], os, indent + 2, idx);
    }
    return os;
}

std::ostream&
PrintBeacon(const Beacon& beacon, std::ostream& os, int indent)
{
    const auto pad = Indent(indent);
    os << pad << "Beacon:\n";
    os << pad << "  set: " << BoolToString(beacon.set) << '\n';
    os << pad << "  BeaconInterval: " << beacon.BeaconInterval << '\n';
    os << pad << "  Beacon_Rate: " << beacon.Beacon_Rate << '\n';
    os << pad << "  EnableBeaconJitter: " << BoolToString(beacon.EnableBeaconJitter) << '\n';
    return os;
}

std::ostream&
PrintFlow(const Flow& f, std::ostream& os, int indent)
{
    const auto pad = Indent(indent);
    os << pad << "FlowId: " << f.FlowId << '\n';
    os << pad << "Protocol: " << f.Protocol << '\n';
    os << pad << "StartTime: " << f.StartTime << '\n';
    os << pad << "EndTime: " << f.EndTime << '\n';
    os << pad << "Direction: " << f.Direction << '\n';

    os << pad << "AC_VI:\n"
       << pad << "  enabled: " << BoolToString(f.AC_VI_flow.enabled)
       << "  packetSize: " << f.AC_VI_flow.packetSize
       << "  trafficType: " << f.AC_VI_flow.trafficType
       << "  meanDataRate: " << f.AC_VI_flow.meanDataRate
       << "  peakDataRate: " << f.AC_VI_flow.peakDataRate << '\n';

    os << pad << "AC_VO:\n"
       << pad << "  enabled: " << BoolToString(f.AC_VO_flow.enabled)
       << "  packetSize: " << f.AC_VO_flow.packetSize
       << "  trafficType: " << f.AC_VO_flow.trafficType << "  interval: " << f.AC_VO_flow.interval
       << '\n';

    os << pad << "AC_BE:\n"
       << pad << "  enabled: " << BoolToString(f.AC_BE_flow.enabled)
       << "  packetSize: " << f.AC_BE_flow.packetSize
       << "  trafficType: " << f.AC_BE_flow.trafficType << "  DataRate: " << f.AC_BE_flow.DataRate
       << '\n';

    os << pad << "AC_BK:\n"
       << pad << "  enabled: " << BoolToString(f.AC_BK_flow.enabled)
       << "  packetSize: " << f.AC_BK_flow.packetSize
       << "  trafficType: " << f.AC_BK_flow.trafficType << "  DataRate: " << f.AC_BK_flow.DataRate
       << '\n';

    return os;
}

std::ostream&
PrintNodeConfig(const NodeConfig& n, std::ostream& os)
{
    const int indent = 1;

    os << "================ NodeConfig ================\n";

    os << Indent(indent) << "Id: " << n.Id << '\n';
    os << Indent(indent) << "Ssid: " << n.Ssid << '\n';
    os << Indent(indent) << "Standard: " << n.Standard << '\n';
    os << Indent(indent) << "Phy_model: " << n.Phy_model << '\n';
    os << Indent(indent) << "Bandwidth: " << n.Bandwidth << '\n';
    os << Indent(indent) << "Frequency: " << n.Frequency << '\n';
    os << Indent(indent) << "Channel_number: " << n.Channel_number << '\n';
    os << Indent(indent) << "Tx_power: " << n.Tx_power << '\n';
    os << Indent(indent) << "TxQueue: " << n.TxQueue << '\n';
    os << Indent(indent) << "Position: [" << JoinValues(n.Position) << "]\n";
    PrintMobility(n.mobility, os, indent);
    PrintAntenna(n.antenna, os, indent);
    PrintRtsCts(n.Rts_Cts, os, indent);
    PrintQos(n.qos, os, indent);

    if (n.Rate_ctrl_algo)
    {
        os << Indent(indent) << "Rate_ctrl_algo: " << *n.Rate_ctrl_algo << '\n';
    }
    if (n.RxSensitivity)
    {
        os << Indent(indent) << "RxSensitivity: " << *n.RxSensitivity << '\n';
    }

    if (n.CcaSensitivity)
    {
        os << Indent(indent) << "CcaSensitivity: " << *n.CcaSensitivity << '\n';
    }

    if (n.CcaEdThreshold)
    {
        os << Indent(indent) << "CcaEdThreshold: " << *n.CcaEdThreshold << '\n';
    }

    if (n.Sifs)
    {
        os << Indent(indent) << "Sifs: " << *n.Sifs << '\n';
    }

    if (n.Slot)
    {
        os << Indent(indent) << "Slot: " << *n.Slot << '\n';
    }

    if (n.beacon)
    {
        PrintBeacon(*n.beacon, os, indent);
    }

    if (!n.flows.empty())
    {
        os << Indent(indent) << "Flows:\n";
        for (const auto& f : n.flows)
        {
            PrintFlow(f, os, indent + 1);
        }
    }

    os << "============================================\n";
    return os;
}

/**
 * @brief FileSystem
 *
 */

std::vector<std::string>
GetFilesInFolder(const std::string& folderPath, const std::string& extension)
{
    std::vector<std::string> files;

    auto resolveLatestDesigned = [](const fs::path& input) -> fs::path {
        if (fs::exists(input) && fs::is_directory(input))
        {
            return input;
        }

        fs::path base;
        for (fs::path p = input; !p.empty(); p = p.parent_path())
        {
            if (p.filename() == "Designed")
            {
                base = p;
                break;
            }
        }

        if (base.empty() || !fs::exists(base) || !fs::is_directory(base))
        {
            return input;
        }

        fs::path latest;
        std::optional<fs::file_time_type> latestTime;
        for (const auto& entry : fs::directory_iterator(base))
        {
            if (!entry.is_directory())
            {
                continue;
            }

            const auto name = entry.path().filename().string();
            if (name.rfind("Designed_", 0) != 0)
            {
                continue;
            }

            std::error_code ec;
            const auto t = fs::last_write_time(entry.path(), ec);
            if (ec)
            {
                continue;
            }

            if (!latestTime.has_value() || t > *latestTime)
            {
                latestTime = t;
                latest = entry.path();
            }
        }

        if (latest.empty())
        {
            return input;
        }

        std::string tail = input.filename().string();
        if (tail.empty())
        {
            tail = input.parent_path().filename().string();
        }
        if (tail == "GeneralJson" || tail == "StaConfigJson" || tail == "ApConfigJson")
        {
            return latest / tail;
        }

        return latest;
    };

    const fs::path resolvedPath = resolveLatestDesigned(folderPath);

    if (!fs::exists(resolvedPath) || !fs::is_directory(resolvedPath))
    {
        std::cerr << "Route does not exist or is not a directory: " << resolvedPath.string()
                  << std::endl;
        return files;
    }

    for (const auto& entry : fs::directory_iterator(resolvedPath))
    {
        if (fs::is_regular_file(entry.status()))
        {
            if (extension.empty() || entry.path().extension() == extension)
            {
                files.push_back(entry.path().string());
            }
        }
    }

    return files;
}

std::vector<NodeConfig>
GetApConfigs(const std::string& folderPath)
{
    std::vector<NodeConfig> apConfigs;
    std::vector<std::string> files = GetFilesInFolder(folderPath, ".json");

    for (const auto& file : files)
    {
        NodeConfig apConfig = LoadNodeConfigsFromFile(file);
        if (apConfig.NodeType == "AP")
        {
            apConfigs.push_back(apConfig);
        }
    }
    return apConfigs;
} 

std::vector<NodeConfig>
GetStaConfigs(const std::string& folderPath)
{
    std::vector<NodeConfig> staConfigs;
    std::vector<std::string> files = GetFilesInFolder(folderPath, ".json");

    for (const auto& file : files)
    {
        NodeConfig staConfig = LoadNodeConfigsFromFile(file);
        if (staConfig.NodeType == "STA")
        {
            staConfigs.push_back(staConfig);
        }
    }
    return staConfigs;
} 

GeneralConfig
LoadGeneralConfig(const std::string& path)
{
    GeneralConfig generalConfig;
    std::vector<std::string> files = GetFilesInFolder(path, ".json");
    if (files.empty())
    {
        throw std::runtime_error("No general config JSON found in folder: " + path);
    }
    generalConfig = LoadGeneralConfigFromFile(files[0]);
    return generalConfig;
}

std::ostream& PrintGeneralConfig(const GeneralConfig& generalConfig,std::ostream& os)
{
    os << "================ BuildingConfig ================\n";
    os << "Building Type: " << generalConfig.BuildingType << '\n';
    os << "Range: [" << JoinValues(generalConfig.range) << "]\n";
    os << "Wall Type: " << generalConfig.WallType << '\n';
    os <<"Simulation Time: "<< generalConfig.SimulationTime << '\n';
    os << "==========\n";

    return os;
}

GeneralConfig GetGeneralConfig(const std::string& path)
{
    return LoadGeneralConfig(path);
}
}// namespace ns3
