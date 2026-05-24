/***********************************
 * @file SniffUtils.h
 * @brief This file contains the declaration of the SniffUtils class.
 *
 * The SniffUtils class contains utilities to sniff the information while transmitting and receiving
 * packets. The detailed information and discription of this file remains to be updated.
 *
 * @author Kai Zhang <2747752379@hust.edu.cn>
 * @version 2.0.0
 * @date 2025.11.3
 *
 ***********************************/
#ifndef SNIFF_UTILS_H
#define SNIFF_UTILS_H

#include "ns3/ampdu-subframe-header.h"
#include "ns3/he-ppdu.h"
#include "ns3/mac48-address.h"
#include "ns3/msdu-aggregator.h"
#include "ns3/net-device-container.h"
#include "ns3/ptr.h"
#include "ns3/wifi-mac-header.h"
#include "ns3/wifi-phy.h"
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

#include <boost/interprocess/allocators/allocator.hpp>
#include <boost/interprocess/containers/vector.hpp>
#include <boost/interprocess/managed_shared_memory.hpp>
#include <boost/interprocess/sync/interprocess_condition.hpp>
#include <boost/interprocess/sync/interprocess_mutex.hpp>
#include <cstdint>
#include <iostream>
#include <iterator>
#include <map>
#include <memory>
#include <new>
#include <vector>

enum SharedRecordType : uint8_t
{
    SHM_RECORD_PPDU = 0,
    SHM_RECORD_PHY_STATE = 1,
    SHM_RECORD_DEVICE_ROLE = 2,
    SHM_RECORD_PPDU_UPDATE = 3,
    SHM_RECORD_MAC_DELAY = 4
};

enum SharedDelayKind : uint8_t
{
    SHM_DELAY_UNKNOWN = 0,
    SHM_DELAY_QUEUEING = 1,
    SHM_DELAY_MAC_E2E = 2
};

enum SharedDeviceRole : uint8_t
{
    SHM_DEVICE_UNKNOWN = 0,
    SHM_DEVICE_AP = 1,
    SHM_DEVICE_STA = 2
};

enum SharedPhyState : uint8_t
{
    SHM_PHY_STATE_UNKNOWN = 0,
    SHM_PHY_STATE_IDLE = 1,
    SHM_PHY_STATE_CCA_BUSY = 2,
    SHM_PHY_STATE_TX = 3,
    SHM_PHY_STATE_RX = 4,
    SHM_PHY_STATE_SWITCHING = 5,
    SHM_PHY_STATE_SLEEP = 6,
    SHM_PHY_STATE_OFF = 7
};

/**
 * @brief This struct contains the information of a PPDU that should be transmitted by Ns3 and
 * received by Qt
 *
 */

using sim_time_ns_t = uint64_t;
using ppdu_id_t = uint32_t;
constexpr std::size_t MAX_PPDU_NUM = 1 << 20; // about 1 million PPDUs

/**
 * @brief This struct contains the information of a MPDU that should be transmitted by Ns3 and
 *
 */
struct PPDU_Meta
{
    ppdu_id_t id;
    uint8_t record_type;
    uint16_t sta_id;
    uint8_t channel_id;

    uint8_t frame_type;
    uint8_t mcs;
    uint16_t mpdu_aggregation;
    uint32_t size_bytes;
    uint32_t throughput_mbps_x100;

    uint8_t sender[6];
    uint8_t receiver[6];

    sim_time_ns_t tx_start_ns;
    sim_time_ns_t tx_end_ns;
    sim_time_ns_t successDecodeTime;
    sim_time_ns_t tx_duration_ns;
    sim_time_ns_t access_delay_ns;
    sim_time_ns_t mac_e2e_delay_ns;

    uint8_t rx_state; // 0=unknown 1=success 2=collision 3=decode_fail
    uint8_t rx_fail_reason;
    sim_time_ns_t tx_time_ns;

    uint8_t collision;
    sim_time_ns_t collision_time_ns;

    uint16_t snr_margin_db_x10; // compatibility field, stores SNR x10
    uint16_t snr_gap_db_x10;    // deprecated, kept for shm layout compatibility

    uint8_t phy_state;
    sim_time_ns_t phy_state_start_ns;
    sim_time_ns_t phy_state_end_ns;
    sim_time_ns_t phy_state_duration_ns;

    uint8_t device_role;
    uint8_t device_mac[6];
    uint8_t delay_kind;
    uint8_t reserved[1];
};

/**
 * @brief RingBuffer
 *
 */

struct RingBuffer
{
    uint32_t write_index;
    uint32_t read_index;

    boost::interprocess::interprocess_mutex mutex;
    boost::interprocess::interprocess_condition cond;

    PPDU_Meta records[MAX_PPDU_NUM];
};

namespace ns3
{
/**
 * @brief Struct to store information of a single PPDU
 * @param frame_type type of the frame ,including CONTROL , MANAGEMENT ,DATA
 * @param sender MAC address of the sender
 * @param receiver MAC address of the receiver
 * @param aggregate number of subframes aggregated in this PPDU
 * @param size_bytes size of the PPDU in bytes
 * @param AccessChannelTime time of the PPDU in the access channel
 * @param StartTxTime start time of the PPDU transmission
 * @param EndTxTime end time of the PPDU transmission
 * @param collision indicates if the PPDU was lost due to collision
 * @param CollisionTime time of the collision
 * @param SNRmargin SNR margin of the PPDU
 * @param SNRgap SNR gap of the PPDU
 * @param decodeState indicates if the PPDU was successfully decoded
 * @param ReasonForDecodeFail reason for the failure of decoding the PPDU
 * @param SuccessDecodeTime time of successful decoding of the PPDU
 * @param TxDuration duration of the PPDU transmission
 * @param mcs modulation and coding scheme of the PPDU
 */

struct Ppdu_info;

struct Node_info
{
    Mac48Address node_address;
    std::vector<std::shared_ptr<Ppdu_info>> Ppdu_info_list;
    std::map<uint8_t /*mcs*/, std::vector<std::shared_ptr<Ppdu_info>>> mcs_map;
    std::map<WifiMacType, std::vector<std::shared_ptr<Ppdu_info>>> frame_type_map;
};

struct Ppdu_info
{
    WifiMacType frame_type;
    Mac48Address sender;
    Mac48Address receiver;
    size_t aggregate;
    uint32_t size_bytes;
    double AccessChannelTime;
    Time StartTxTime;
    Time EndTxTime;
    bool collision;
    Time CollisionTime;
    double SNRmargin;
    double SNRgap;
    bool decodeState;
    uint8_t ReasonForDecodeFail;
    Time SuccessDecodeTime;
    Time TxDuration;
    uint8_t mcs;
};

class SniffUtils : public Object
{
  public:
    SniffUtils();
    ~SniffUtils() override;

    static TypeId GetTypeId();
    /**
     * @brief Register all Wi-Fi devices that should be traced.
     * @param[in] devices all Wi-Fi net devices to monitor
     * @param[in] SimulationTime simulation time in double
     */
    bool Initialize(const NetDeviceContainer& devices, double SimulationTime);

    /**
     * @brief Compatibility overload. Internally merges and deduplicates devices.
     * @param[in] sender legacy device container
     * @param[in] receiver legacy device container
     * @param[in] SimulationTime simulation time in double
     */
    bool Initialize(NetDeviceContainer sender, NetDeviceContainer receiver, double SimulationTime);

    void Finalize_Tag_PPDU(ppdu_id_t id);

    /**
     * @brief Sniff all the frames been transmitted
     * @param[in] packet the packet
     * @param[in] frequency frequency of the channel
     * @param[in] txvector txvector of the PPDU
     * @param[in] mpdu_info the information of the PPDU
     * @param[in] sta_id the id of the station
     *
     */
    void Sniff_tx_packet_begin(Ptr<const Packet> packet,
                               uint16_t frequency,
                               WifiTxVector txvector,
                               MpduInfo mpdu_info,
                               uint16_t sta_id);

    /**
     * @brief Sniff all the frames been received
     *
     * @param packet
     */
    void Sniff_tx_packet_end(Ptr<const Packet> packet);

    /**
     * @brief Sniff all the frames been received
     * @param[in] packet the packet
     * @param[in] frequency of the channel
     * @param[in] txvector txvector of the PPDU
     * @param[in] mpdu_info the information of the PPDU
     * @param[in] noise the noise of the receiver
     * @param[in] sta_id the id of the station
     */
    void Sniff_rx_packet_begin(Ptr<const Packet> packet,
                               uint16_t frequency,
                               WifiTxVector txvector,
                               MpduInfo mpdu_info,
                               SignalNoiseDbm noise,
                               uint16_t sta_id);

    /**
     * @brief Sniff the PPDU decode outcome to determine whether reception succeeded.
     *
     * @param[in] ppdu the received PPDU
     * @param[in] rxSignalInfo received signal information
     * @param[in] txVector tx parameters
     * @param[in] outcomes per-MPDU decode outcomes
     */
    void Sniff_rx_ppdu_outcome(Ptr<const WifiPpdu> ppdu,
                               RxSignalInfo rxSignalInfo,
                               const WifiTxVector& txVector,
                               const std::vector<bool>& outcomes);

    /**
     * @brief Sniff all the frames been dropped
     *
     * @param[in] packet the packet
     * @param[in] drop_reason the reason of the drop during the reception
     */
    void Sniff_drop_packet_phy(Ptr<const Packet> packet);
    /**
     * @brief Sniff all the frames been dropped
     * @param[in] ppdu the WifiPpdu
     * @param[in] drop_reason the reason of the drop during the reception
     */
    void Sniff_drop_ppdu_phy(Ptr<const WifiPpdu> ppdu, WifiPhyRxfailureReason drop_reason);

    /**
     * @brief Sniff all the frames been transmitted
     *
     * @param[in] psdu_map the Psdu information is included
     * @param[in] txvector the information of tx
     * @param[in] tx_power the tx power
     */
    void Sniff_tx_psdu_begin(WifiConstPsduMap psdu_map, WifiTxVector txvector, double tx_power);

    /**
     * @brief Sniff the MAC header of the PPDU
     *
     * @param[in] wifi_mac_header the MAC header of the PPDU
     * @param[in] tx_vector the tx vector of the PPDU
     * @param[in] time ?
     */
    void Sniff_mac_header(const WifiMacHeader& wifi_mac_header,
                          const WifiTxVector& tx_vector,
                          Time time);

    /**
     * @brief Sniff all the frames been transmitted
     * @param[in] packet the packet
     * @param[in] frequency of the channel
     * @param[in] txvector txvector of the PPDU
     * @param[in] mpdu_info the information of the PPDU
     * @param[in] sta_id the id of the station
     */
    void Sniff_tx_all_packets(Ptr<const Packet> packet,
                              uint16_t frequency,
                              WifiTxVector txvector,
                              MpduInfo mpdu_info,
                              uint16_t sta_id);

    /**
     * @brief Sniff all the ppdu been sent
     *
     * @param[in] ppdu
     * @param[in] tx_vector
     */
    void Sniff_ppdu_begin(uint16_t nodeId, Ptr<const WifiPpdu> ppdu, const WifiTxVector& tx_vector);

    void NotifyMacEnqueue(uint16_t nodeId, uint32_t deviceId, Ptr<const WifiMpdu> mpdu);

    void NotifyAckedMpdu(uint16_t nodeId, uint32_t deviceId, Ptr<const WifiMpdu> mpdu);

    /**
     * @brief Notify when the state changes
     * 
     */
    void NotifyStateChange(uint16_t nodeId,
                           uint8_t channelId,
                           Time start,
                           Time duration,
                           WifiPhyState state);
    /**
     * @brief set the simulation time
     *
     * @param[in] simulation_time
     */
    void Set_simulation_time(double simulation_time);

    void PrintPpduMeta(uint32_t ring_index) const;

    void PrintLastPpdu() const;

  private:
    bool m_initialized;
    double m_simulation_time;
    uint32_t m_next_ppdu_id = 0;
    std::unordered_map<Ptr<const WifiPpdu>, uint32_t> m_ppdu_map;
    std::unordered_map<uint64_t, ppdu_id_t> m_ppdu_uid_to_id;
    std::unordered_map<uint64_t, ppdu_id_t> m_packet_uid_to_ppdu_id;
    std::unordered_map<uint64_t, ppdu_id_t> m_seq_key_to_ppdu_id;
    std::unordered_map<uint64_t, sim_time_ns_t> m_mpdu_enqueue_time_ns;
    std::unordered_map<uint16_t, uint8_t> m_node_channel_id;
    RingBuffer* m_ring = nullptr;
    std::unordered_set<uint32_t> m_registeredNodeIds;
    std::unordered_set<uint64_t> m_registeredMacs;
};

/**
 * @brief The information of a PPDU that is freshly sniffed by SniffUtils
 *
 */
struct PpduRuntime
{
    uint32_t ring_index;
    uint64_t ppdu_uid = 0;
    uint64_t packet_uid = 0;
    uint64_t seq_key = 0;
    bool rx_success = false;
    bool rx_drop = false;
    bool collision = false;
    WifiPhyRxfailureReason drop_reason{};
    int64_t success_decode_ns = 0;
    int64_t collision_time_ns = 0;
    uint16_t snr_db_x10 = 0;
    sim_time_ns_t queue_delay_ns = 0;
    sim_time_ns_t mac_e2e_delay_ns = 0;
};

extern std::unordered_map<ppdu_id_t, PpduRuntime> m_ppdu_runtime;

/**
 * @brief
 *
 */
//this function is used to append a PPDU to the ring buffer
uint32_t AppendPpdu(RingBuffer* buffer, const PPDU_Meta& ppdu);
void ShmExample();
} // namespace ns3

#endif
