/***********************************
 * @file QNs3.h
 * @brief This file contains the declaration of the QNs3 class.
 *
 * The QNs3 class contains the implementation of the QNs3 model.
 * The detailed information and discription of this file remains to be updated.
 *
 * @author Kai Zhang <2747752379@hust.edu.cn>
 * @version 2.0.0
 * @date 2025.11.3
 *
 ***********************************/
#ifndef QNS3_H
#define QNS3_H

#include "ns3/ampdu-subframe-header.h"
#include "ns3/mac48-address.h"
#include "ns3/msdu-aggregator.h"
#include "ns3/net-device-container.h"
#include "ns3/ptr.h"
#include "ns3/wifi-mac-header.h"
#include "ns3/wifi-phy.h"
#include "ns3/building.h"
#include "ns3/buildings-helper.h"
#include "ns3/wifi-ppdu.h"
#include <ns3/callback.h>
#include <ns3/log-macros-disabled.h>
#include <ns3/log.h>
#include <ns3/object-base.h>
#include <ns3/qos-txop.h>
#include <ns3/simulator.h>
#include <ns3/trace-source-accessor.h>
#include <ns3/wifi-mac-queue.h>
#include <ns3/wifi-mac.h>
#include <ns3/wifi-net-device.h>

#include <iostream>
#include <optional>
#include <string>
#include <iterator>
#include <map>
#include <memory>
#include <new>
#include <vector>

namespace ns3
{
/**
 * @brief This struct is to store the info of the Mobility part of ONE node
 * @param set whether the mobility is been set
 * @param boundaries the boundaries of where the node can move
 * @param mode the mode of the mobility(Time,Speed)
 * @param model_type like "RandomWalk2dMobilityModel"
 * @param time_change_interval the time interval between two changes of position
 * @param distance_change_interval the distance interval between two changes of position
 * @param velocity the velocity of the node (m/s)
 *
 */


struct Mobility
{
    bool set;
    std::vector<double> boundaries;
    std::string mode;
    std::string model_type;
    double time_change_interval;     // in seconds
    double distance_change_interval; // in meters
    double velocity;                    // in meters/second
};

/**
 * @brief This struct is to store the info of the Rts/Cts part of ONE node
 * @param set whether the Rts/Cts is been set
 * @param Threshold the threshold of the Rts/Cts
 *
 */
struct RtsCts

{
    bool set;
    int Threshold; // in bytes
};

/**
 * @brief This struct is to store the info of the EdcaParams
 * @param CWmin the minimum contention window size
 * @param CWmax the maximum contention window size
 * @param AIFSN the AIFSN value,in us
 * @param TXOPLimit the TXOP limit value,in us
 *
 */
struct EdcaParams
{
    uint32_t CWmin;
    uint32_t CWmax;
    uint32_t AIFSN;
    uint16_t TXOPLimit;
};

/**
 * @brief This struct is to store the info of the MsduAggregation
 * @param set whether the MsduAggregation is been set
 * @param type the type of the MsduAggregation, like "AC_VO"
 * @param MaxAmsduSize the maximum A-MSDU size
 *
 */
struct MsduAggregation
{
    bool set;
    std::string type;
    uint16_t MaxAmsduSize; // in bytes
};

/**
 * @brief This struct is to store the info of the MpduAggregation
 * @param set whether the MpduAggregation is been set
 * @param type the type of the MpduAggregation, like "AC_VO"
 * @param MaxAmpduSize the maximum A-MPDU size
 * @param Density the density of the A-MPDU
 *
 */
struct MpduAggregation
{
    bool set;
    std::string type;
    uint16_t MaxAmpduSize; // in bytes
    uint8_t Density;       // in us
};

/**
 * @brief This struct is to store the info of the Qos
 * @param Qos_support whether the Qos is been supported
 * @param Edca_choose the type of the EDCA, like "AC_BE"
 * @param Edca_params the parameters of the EDCA,presented by a list
 * @param Msde_Aggregation the parameters of the MsduAggregation,presented by a struct
 * @param Mpdu_Aggregation the parameters of the MpduAggregation,presented by a struct
 *
 */
struct Qos
{
    bool Qos_support;
    std::string Edca_choose;
    std::map<std::string, EdcaParams> Edca_params;
    MsduAggregation Msdu_Aggregation;
    MpduAggregation Mpdu_Aggregation;
};

/**
 * @brief This struct is to store the info of the AntennaElement
 * @param type the type of the antenna, like "Omni"
 * @param MaxGain the maximum gain of the antenna
 * @param Beamwidth the beamwidth of the antenna
 * 
 */
struct AntennaElement
{
    std::string type;
    int MaxGain;
    int Beamwidth;
};

/**
 * @brief This struct is to store the info of the Antenna
 * @param set whether the Antenna is been set
 * @param Antennas the list of the AntennaElements
 * 
 */
struct Antenna
{
    bool set;
    std::vector<AntennaElement> Antennas;
};

/**
 * @brief This struct is to store the info of the Beacon
 * @param set whether the Beacon is been set
 * @param BeaconInterval the interval of the Beacon
 * @param Beacon_Rate the rate of the Beacon
 * @param EnableBeaconJitter whether the Beacon has jitter or not
 * 
 */
struct Beacon
{
    bool set;
    int BeaconInterval;
    int Beacon_Rate;
    bool EnableBeaconJitter;
};

/**
 * @brief EDCA_traffic_type
 * 
 */
struct Struct_AC_VI{bool enabled;uint32_t packetSize;std::string trafficType;uint32_t meanDataRate;uint32_t peakDataRate;};
struct Struct_AC_VO{bool enabled;uint32_t packetSize;std::string trafficType;uint32_t interval;};
struct Struct_AC_BE{bool enabled;uint32_t packetSize;std::string trafficType;uint32_t DataRate;};
struct Struct_AC_BK{bool enabled;uint32_t packetSize;std::string trafficType;uint32_t DataRate;};

/**
 * @brief Flow
 * @param FlowId the id of the flow
 * @param Protocol the protocol of the flow
 * @param StartTime the start time of the flow
 * @param EndTime the end time of the flow
 * @param Direction the direction of the flow
 * 
 */
// Flow 的 EDCA 参数
struct Flow {
    std::string FlowId;
    std::string Protocol;
    double StartTime;
    double EndTime;
    std::string Direction;

    Struct_AC_VI AC_VI_flow;
    Struct_AC_VO AC_VO_flow;
    Struct_AC_BE AC_BE_flow;
    Struct_AC_BK AC_BK_flow;
};
 
/**
 * @brief This struct is to store the info of the NodeConfig
 * @param Position the position of the node
 * @param Id the id of the node
 * @param mobility the Mobility part of the node
 * @param Active_probing whether the node is active probing or not
 * @param Channel_number the channel number of the node
 * @param Frequency the frequency of the node
 * @param Bandwidth the bandwidth of the node
 * @param Tx_power the tx_power of the node
 * @param Ssid the ssid of the node
 * @param Phy_model the phy_model of the node    
 * @param Standard the standard of the node
 * @param Rts_Cts the Rts/Cts part of the node
 * @param Rate_ctr_algo the rate_ctr_algo of the node
 * @param TxQueue the txqueue of the node
 * @param qos the Qos part of the node
 * @param antenna the Antenna part of the node
 * @param beacon the Beacon part of the node
 * 
 */
struct NodeConfig
{
    std::string NodeType;
    std::vector<double> Position;
    int Id;
    Mobility mobility;
    int Channel_number;
    double Frequency;
    int Bandwidth;
    int Tx_power;
    std::string Ssid;
    std::string Phy_model;
    std::string Standard;
    int Guard_interval;
    RtsCts Rts_Cts;
    Qos qos;
    Antenna antenna;
    std::vector<Flow> flows;
    std::string TxQueue;
    std::optional<std::string> Rate_ctrl_algo;
    std::optional<int> CcaEdThreshold;
    std::optional<int> CcaSensitivity;
    std::optional<int> RxSensitivity;
    std::optional<int> Sifs;
    std::optional<int> Slot;
    std::optional<Beacon> beacon;

    std::optional<bool> EnableAssocFailRetry;
    std::optional<int> MaxMissedBeacons;
    std::optional<int> ProbeRequestTimeout;
    std::optional<bool> ActiveProbing;

};

struct GeneralConfig
{
    std::string BuildingType;
    std::string WallType;
    std::vector<double> range;
    double SimulationTime;
    std::string ns3Path;
    std::string jsonPath = "contrib/WiFiViz/Simulation/Designed/";
    std::string path = ns3Path + jsonPath;
};
    


std::ostream& PrintGeneralConfig(const GeneralConfig& , std::ostream&);
std::ostream& PrintNodeConfig(const NodeConfig&, std::ostream&);
GeneralConfig GetGeneralConfig(const std::string& folderPath);
std::vector<NodeConfig> GetApConfigs(const std::string& folderPath);
std::vector<NodeConfig> GetStaConfigs(const std::string& folderPath);
} // namespace ns3

#endif // QNS3_H
