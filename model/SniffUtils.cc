#include "SniffUtils.h"

#include "ns3/ampdu-subframe-header.h"
#include "ns3/ap-wifi-mac.h"
#include "ns3/applications-module.h"
#include "ns3/core-module.h"
#include "ns3/csma-module.h"
#include "ns3/internet-module.h"
#include "ns3/mac48-address.h"
#include "ns3/mobility-module.h"
#include "ns3/msdu-aggregator.h"
#include "ns3/net-device-container.h"
#include "ns3/network-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/ptr.h"
#include "ns3/ssid.h"
#include "ns3/wifi-mac-header.h"
#include "ns3/wifi-phy.h"
#include "ns3/wifi-ppdu.h"
#include "ns3/yans-wifi-helper.h"
#include "ns3/packet-sink.h"
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
#include <ns3/sta-wifi-mac.h>

#include <boost/interprocess/managed_shared_memory.hpp>
#include <boost/interprocess/sync/scoped_lock.hpp>
#include <deque>
#include <unordered_set>

using namespace boost::interprocess;
boost::interprocess::managed_shared_memory* m_shm = nullptr;

namespace ns3
{

namespace
{

constexpr sim_time_ns_t THROUGHPUT_WINDOW_NS = 100000000ULL; // 100 ms
std::deque<std::pair<sim_time_ns_t, uint32_t>> g_throughputWindow;
uint64_t g_throughputBytesInWindow = 0;
uint64_t g_finalizeCount = 0;
uint64_t g_finalizeSuccessCount = 0;
uint64_t g_finalizeNonZeroThroughputCount = 0;
uint64_t g_beginCount = 0;
uint64_t g_beginNonZeroThroughputCount = 0;
uint64_t g_rxOutcomeCount = 0;
uint64_t g_rxOutcomeSuccessCount = 0;
uint64_t g_rxOutcomeMapMissCount = 0;
uint64_t g_rxOutcomePtrHitCount = 0;
uint64_t g_rxOutcomeUidHitCount = 0;
uint64_t g_rxOutcomeDebugPrintCount = 0;

uint8_t
ToSharedPhyState(WifiPhyState state)
{
    switch (state)
    {
    case WifiPhyState::IDLE:
        return SHM_PHY_STATE_IDLE;
    case WifiPhyState::CCA_BUSY:
        return SHM_PHY_STATE_CCA_BUSY;
    case WifiPhyState::TX:
        return SHM_PHY_STATE_TX;
    case WifiPhyState::RX:
        return SHM_PHY_STATE_RX;
    case WifiPhyState::SWITCHING:
        return SHM_PHY_STATE_SWITCHING;
    case WifiPhyState::SLEEP:
        return SHM_PHY_STATE_SLEEP;
    case WifiPhyState::OFF:
        return SHM_PHY_STATE_OFF;
    default:
        return SHM_PHY_STATE_UNKNOWN;
    }
}

uint8_t
DetectDeviceRole(const Ptr<WifiNetDevice>& device)
{
    if (!device)
    {
        return SHM_DEVICE_UNKNOWN;
    }

    Ptr<WifiMac> mac = device->GetMac();
    if (DynamicCast<ApWifiMac>(mac))
    {
        return SHM_DEVICE_AP;
    }
    if (DynamicCast<StaWifiMac>(mac))
    {
        return SHM_DEVICE_STA;
    }
    return SHM_DEVICE_UNKNOWN;
}

bool
IsCollisionLikeFailure(WifiPhyRxfailureReason reason)
{
    switch (reason)
    {
    case BUSY_DECODING_PREAMBLE:
    case PREAMBLE_DETECTION_PACKET_SWITCH:
    case FRAME_CAPTURE_PACKET_SWITCH:
        return true;
    default:
        return false;
    }
}

uint64_t
MacAddressToUint64(const Mac48Address& address)
{
    uint8_t bytes[6] = {0};
    address.CopyTo(bytes);

    uint64_t value = 0;
    for (int i = 0; i < 6; ++i)
    {
        value = (value << 8) | bytes[i];
    }
    return value;
}

uint64_t
NodeLinkKey(uint16_t nodeId, uint8_t linkId)
{
    return (static_cast<uint64_t>(nodeId) << 8) | linkId;
}

uint64_t
PacketUid(const Ptr<const Packet>& packet)
{
    return packet ? packet->GetUid() : 0;
}

uint64_t
MpduSeqKey(const WifiMacHeader& header)
{
    if (!header.IsData())
    {
        return 0;
    }

    uint8_t receiver[6] = {0};
    uint8_t sender[6] = {0};
    header.GetAddr1().CopyTo(receiver);
    header.GetAddr2().CopyTo(sender);

    uint64_t key = static_cast<uint64_t>(header.GetSequenceNumber());
    for (int i = 0; i < 6; ++i)
    {
        key = (key << 5) ^ receiver[i];
        key = (key << 5) ^ sender[i];
    }
    return key;
}

WifiMode
GetPayloadMode(const Ptr<const WifiPpdu>& ppdu, const WifiTxVector& txVector)
{
    if (!txVector.IsMu())
    {
        return txVector.GetMode();
    }

    const auto& userInfos = txVector.GetHeMuUserInfoMap();
    NS_ABORT_MSG_IF(userInfos.empty(), "MU WifiTxVector has no per-user info");

    const uint16_t ppduStaId = ppdu ? ppdu->GetStaId() : SU_STA_ID;
    auto userIt = userInfos.find(ppduStaId);
    if (userIt != userInfos.end())
    {
        return txVector.GetMode(ppduStaId);
    }

    return txVector.GetMode(userInfos.cbegin()->first);
}

void
EmitDeviceRoleRecord(RingBuffer* ring, const Ptr<WifiNetDevice>& device)
{
    if (!ring || !device || !device->GetMac())
    {
        return;
    }

    PPDU_Meta meta{};
    meta.record_type = SHM_RECORD_DEVICE_ROLE;
    meta.sta_id = static_cast<uint16_t>(device->GetNode()->GetId());
    meta.channel_id = device->GetPhy() ? device->GetPhy()->GetChannelNumber() : 0;
    meta.link_id = 0;
    meta.device_role = DetectDeviceRole(device);

    const Mac48Address address = Mac48Address::ConvertFrom(device->GetMac()->GetAddress());
    address.CopyTo(meta.device_mac);
    AppendPpdu(ring, meta);
}

void
EmitMacDelayRecord(RingBuffer* ring,
                   uint16_t nodeId,
                   uint8_t channelId,
                   uint8_t linkId,
                   uint8_t deviceRole,
                   const WifiMacHeader& header,
                   uint32_t sizeBytes,
                   sim_time_ns_t eventTimeNs,
                   sim_time_ns_t delayNs,
                   uint8_t delayKind)
{
    if (!ring || delayNs == 0)
    {
        return;
    }

    PPDU_Meta meta{};
    meta.record_type = SHM_RECORD_MAC_DELAY;
    meta.sta_id = nodeId;
    meta.channel_id = channelId;
    meta.link_id = linkId;
    meta.device_role = deviceRole;
    meta.frame_type = static_cast<uint8_t>(header.GetType());
    meta.size_bytes = sizeBytes;
    meta.tx_end_ns = eventTimeNs;
    meta.delay_kind = delayKind;
    if (delayKind == SHM_DELAY_QUEUEING)
    {
        meta.access_delay_ns = delayNs;
    }
    else if (delayKind == SHM_DELAY_MAC_E2E)
    {
        meta.mac_e2e_delay_ns = delayNs;
    }
    header.GetAddr2().CopyTo(meta.sender);
    header.GetAddr1().CopyTo(meta.receiver);
    AppendPpdu(ring, meta);
}

NetDeviceContainer
MergeDeviceContainers(const NetDeviceContainer& lhs, const NetDeviceContainer& rhs)
{
    NetDeviceContainer merged;
    std::unordered_set<const NetDevice*> seen;

    auto addUnique = [&](const NetDeviceContainer& source) {
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

    addUnique(lhs);
    addUnique(rhs);
    return merged;
}

void
UpdateRealtimeThroughput(PPDU_Meta& meta, uint32_t newlyReceivedBytes)
{
    const sim_time_ns_t nowNs = meta.tx_end_ns;
    const sim_time_ns_t windowStartNs =
        (nowNs > THROUGHPUT_WINDOW_NS) ? (nowNs - THROUGHPUT_WINDOW_NS) : 0;

    while (!g_throughputWindow.empty() && g_throughputWindow.front().first < windowStartNs)
    {
        g_throughputBytesInWindow -= g_throughputWindow.front().second;
        g_throughputWindow.pop_front();
    }

    if (newlyReceivedBytes > 0)
    {
        g_throughputWindow.emplace_back(nowNs, newlyReceivedBytes);
        g_throughputBytesInWindow += newlyReceivedBytes;
    }

    const double throughputMbps =
        (static_cast<double>(g_throughputBytesInWindow) * 8.0) /
        (static_cast<double>(THROUGHPUT_WINDOW_NS) / 1e9) / 1e6;

    meta.throughput_mbps_x100 =
        static_cast<uint32_t>(std::llround(std::max(0.0, throughputMbps) * 100.0));
}

} // namespace

std::unordered_map<ppdu_id_t, PpduRuntime> m_ppdu_runtime;

constexpr const char* SHM_NAME = "Ns3PpduSharedMemory";
static const double SimulationTime = 0.0;
NS_LOG_COMPONENT_DEFINE("SniffUtils");
NS_OBJECT_ENSURE_REGISTERED(SniffUtils);

TypeId
SniffUtils::GetTypeId()
{
    static TypeId tid = TypeId("ns3::SniffUtils")
                            .SetParent<ns3::Object>()
                            .SetGroupName("Wifi")
                            .AddConstructor<SniffUtils>();
    return tid;
}

SniffUtils::SniffUtils()
{
    NS_LOG_FUNCTION(this);
    delete m_shm;
    m_shm = nullptr;
    m_initialized = false;
}

SniffUtils::~SniffUtils()
{
    // std::cout << "[THR-SUMMARY] finalize=" << g_finalizeCount
    //           << " finalize_success=" << g_finalizeSuccessCount
    //           << " finalize_thr_nonzero=" << g_finalizeNonZeroThroughputCount
    //           << " begin=" << g_beginCount
    //           << " begin_thr_nonzero=" << g_beginNonZeroThroughputCount
    //           << " rx_outcome=" << g_rxOutcomeCount
    //           << " rx_outcome_success=" << g_rxOutcomeSuccessCount
    //           << " rx_outcome_map_miss=" << g_rxOutcomeMapMissCount
    //           << " rx_outcome_ptr_hit=" << g_rxOutcomePtrHitCount
    //           << " rx_outcome_uid_hit=" << g_rxOutcomeUidHitCount
    //           << " window_bytes=" << g_throughputBytesInWindow << std::endl;
}

bool
SniffUtils::Initialize(const NetDeviceContainer& devices, double simulationTime)
{
    this->Set_simulation_time(simulationTime);

    m_shm = new boost::interprocess::managed_shared_memory(boost::interprocess::open_or_create,
                                                           SHM_NAME,
                                                           1024UL * 1024 * 1024); // 1GB
    m_ring = m_shm->find_or_construct<RingBuffer>("PpduRing")();
    m_ring->write_index = 0;
    m_ring->read_index = 0;
    g_throughputWindow.clear();
    g_throughputBytesInWindow = 0;
    g_finalizeCount = 0;
    g_finalizeSuccessCount = 0;
    g_finalizeNonZeroThroughputCount = 0;
    g_beginCount = 0;
    g_beginNonZeroThroughputCount = 0;
    g_rxOutcomeCount = 0;
    g_rxOutcomeSuccessCount = 0;
    g_rxOutcomeMapMissCount = 0;
    g_rxOutcomePtrHitCount = 0;
    g_rxOutcomeUidHitCount = 0;
    g_rxOutcomeDebugPrintCount = 0;
    m_ppdu_uid_to_id.clear();
    m_packet_uid_to_ppdu_id.clear();
    m_seq_key_to_ppdu_id.clear();
    m_mpdu_enqueue_time_ns.clear();
    m_node_link_channel_id.clear();
    m_node_roles.clear();
    m_registeredNodeIds.clear();
    m_registeredMacs.clear();

    for (auto deviceIt = devices.Begin(); deviceIt != devices.End(); ++deviceIt)
    {
        Ptr<WifiNetDevice> wifiDevice = DynamicCast<WifiNetDevice>(*deviceIt);
        if (!wifiDevice || !wifiDevice->GetPhy() || !wifiDevice->GetMac())
        {
            continue;
        }

        const auto nodeId = static_cast<uint16_t>(wifiDevice->GetNode()->GetId());
        const Mac48Address address = Mac48Address::ConvertFrom(wifiDevice->GetMac()->GetAddress());
        const uint64_t macKey = MacAddressToUint64(address);
        const uint8_t deviceRole = DetectDeviceRole(wifiDevice);
        if (deviceRole != SHM_DEVICE_UNKNOWN)
        {
            m_node_roles[nodeId] = deviceRole;
        }

        if (m_registeredNodeIds.insert(nodeId).second || m_registeredMacs.insert(macKey).second)
        {
            EmitDeviceRoleRecord(m_ring, wifiDevice);
        }

        const auto deviceId = static_cast<uint32_t>(wifiDevice->GetIfIndex());
        const uint8_t nPhys = wifiDevice->GetNPhys();
        const uint8_t phyCount = nPhys > 0 ? nPhys : 1;

        if (wifiDevice->GetMac()->GetQosSupported())
        {
            for (const auto& [ac, wifiAc] : wifiAcList)
            {
                (void)wifiAc;
                Ptr<WifiMacQueue> queue = wifiDevice->GetMac()->GetTxopQueue(ac);
                if (queue)
                {
                    queue->TraceConnectWithoutContext(
                        "Enqueue",
                        MakeCallback(&SniffUtils::NotifyMacEnqueue, this).Bind(nodeId, deviceId));
                }
            }
        }
        else
        {
            Ptr<WifiMacQueue> queue = wifiDevice->GetMac()->GetTxopQueue(AC_BE_NQOS);
            if (queue)
            {
                queue->TraceConnectWithoutContext(
                    "Enqueue",
                    MakeCallback(&SniffUtils::NotifyMacEnqueue, this).Bind(nodeId, deviceId));
            }
        }

        wifiDevice->GetMac()->TraceConnectWithoutContext(
            "AckedMpdu",
            MakeCallback(&SniffUtils::NotifyAckedMpdu, this).Bind(nodeId, deviceId));

        for (uint8_t phyId = 0; phyId < phyCount; ++phyId)
        {
            Ptr<WifiPhy> phy = wifiDevice->GetPhy(phyId);
            if (!phy)
            {
                continue;
            }

            uint8_t linkId = phyId;
            if (const auto maybeLinkId = wifiDevice->GetMac()->GetLinkForPhy(phyId))
            {
                linkId = *maybeLinkId;
            }

            const auto channelId = phy->GetChannelNumber();
            m_node_link_channel_id[NodeLinkKey(nodeId, linkId)] = channelId;

            phy->TraceConnectWithoutContext(
                "SignalTransmission",
                MakeCallback(&SniffUtils::Sniff_ppdu_begin, this, nodeId, linkId));

            phy->TraceConnectWithoutContext("MonitorSnifferRx",
                                            MakeCallback(&SniffUtils::Sniff_rx_packet_begin, this));

            phy->TraceConnectWithoutContext("PhyTxEnd",
                                            MakeCallback(&SniffUtils::Sniff_tx_packet_end, this));

            phy->TraceConnectWithoutContext("MonitorSnifferTx",
                                            MakeCallback(&SniffUtils::Sniff_tx_packet_begin, this));

            phy->TraceConnectWithoutContext("PhyTxDrop",
                                            MakeCallback(&SniffUtils::Sniff_drop_packet_phy, this));

            phy->TraceConnectWithoutContext("PhyRxPpduDrop",
                                            MakeCallback(&SniffUtils::Sniff_drop_ppdu_phy, this));

            auto state_helper = phy->GetState();
            state_helper->TraceConnectWithoutContext("State",
                                                     MakeCallback(&SniffUtils::NotifyStateChange,
                                                                  this,
                                                                  nodeId,
                                                                  channelId,
                                                                  linkId));
            state_helper->TraceConnectWithoutContext("RxOutcome",
                                                     MakeCallback(&SniffUtils::Sniff_rx_ppdu_outcome,
                                                                  this));
        }
    }

    m_initialized = true;
    return true;
}

void
SniffUtils::NotifyMacEnqueue(uint16_t nodeId, uint32_t deviceId, Ptr<const WifiMpdu> mpdu)
{
    (void)nodeId;
    (void)deviceId;
    if (!mpdu || !mpdu->GetPacket())
    {
        return;
    }

    const auto nowNs = static_cast<sim_time_ns_t>(Simulator::Now().GetNanoSeconds());
    const uint64_t packetUid = PacketUid(mpdu->GetPacket());
    if (packetUid != 0)
    {
        m_mpdu_enqueue_time_ns[packetUid] = nowNs;
    }
}

void
SniffUtils::NotifyAckedMpdu(uint16_t nodeId, uint32_t deviceId, Ptr<const WifiMpdu> mpdu)
{
    (void)nodeId;
    (void)deviceId;
    if (!mpdu || !mpdu->GetPacket())
    {
        return;
    }

    const auto ackTimeNs = static_cast<sim_time_ns_t>(Simulator::Now().GetNanoSeconds());
    const uint64_t packetUid = PacketUid(mpdu->GetPacket());
    const uint64_t seqKey = MpduSeqKey(mpdu->GetHeader());
    ppdu_id_t ppduId = 0;
    auto idIt = m_packet_uid_to_ppdu_id.find(packetUid);
    if (idIt != m_packet_uid_to_ppdu_id.end())
    {
        ppduId = idIt->second;
    }
    else if (seqKey != 0)
    {
        auto seqIt = m_seq_key_to_ppdu_id.find(seqKey);
        if (seqIt != m_seq_key_to_ppdu_id.end())
        {
            ppduId = seqIt->second;
        }
    }

    auto enqueueIt = m_mpdu_enqueue_time_ns.find(packetUid);
    if (enqueueIt != m_mpdu_enqueue_time_ns.end())
    {
        const sim_time_ns_t enqueueTimeNs = enqueueIt->second;
        if (ppduId != 0)
        {
            auto rtIt = m_ppdu_runtime.find(ppduId);
            if (rtIt != m_ppdu_runtime.end())
            {
                rtIt->second.mac_e2e_delay_ns = (ackTimeNs > enqueueTimeNs) ? (ackTimeNs - enqueueTimeNs) : 0;

                PPDU_Meta update{};
                {
                    scoped_lock<interprocess_mutex> lock(m_ring->mutex);
                    update = m_ring->records[rtIt->second.ring_index];
                }
                update.record_type = SHM_RECORD_PPDU_UPDATE;
                update.mac_e2e_delay_ns = rtIt->second.mac_e2e_delay_ns;
                AppendPpdu(m_ring, update);
            }
        }

        uint8_t linkId = 0;
        if (!mpdu->GetInFlightLinkIds().empty())
        {
            linkId = *mpdu->GetInFlightLinkIds().begin();
        }

        const auto channelIt = m_node_link_channel_id.find(NodeLinkKey(nodeId, linkId));
        const auto roleIt = m_node_roles.find(nodeId);
        EmitMacDelayRecord(m_ring,
                           nodeId,
                           channelIt != m_node_link_channel_id.end() ? channelIt->second : 0,
                           linkId,
                           roleIt != m_node_roles.end() ? roleIt->second : SHM_DEVICE_UNKNOWN,
                           mpdu->GetHeader(),
                           mpdu->GetSize(),
                           ackTimeNs,
                           (ackTimeNs > enqueueTimeNs) ? (ackTimeNs - enqueueTimeNs) : 0,
                           SHM_DELAY_MAC_E2E);
        m_mpdu_enqueue_time_ns.erase(enqueueIt);
    }
}

bool
SniffUtils::Initialize(NetDeviceContainer sender,
                       NetDeviceContainer receiver,
                       double simulationTime)
{
    return Initialize(MergeDeviceContainers(sender, receiver), simulationTime);
}

uint8_t
FreqToChannel(double freqMHz)
{
    if (freqMHz >= 2412 && freqMHz <= 2484)
    {
        return static_cast<uint8_t>((freqMHz - 2407) / 5);
    }
    else if (freqMHz >= 5180 && freqMHz <= 5825)
    {
        return static_cast<uint8_t>((freqMHz - 5000) / 5);
    }
    else if (freqMHz >= 5955 && freqMHz <= 7115)
    {
        return static_cast<uint8_t>((freqMHz - 5955) / 5);
    }
    return 0;
}

uint8_t
GetPpduPrimaryChannel(const Ptr<const WifiPpdu>& ppdu)
{
    std::vector<MHz_u> freqs = ppdu->GetTxCenterFreqs();
    if (freqs.empty())
    {
        return 0;
    }

    double centerFreq = freqs[0];
    return FreqToChannel(centerFreq);
}

/**
 * @brief Finally tag the PPDU with the result of the sniffing.
 * @attention We can not deduce the result of the rx when we start sniffing,so
 * waiting until the end of the PPDU to tag it.
 *
 * @param id find the PPDU by its id.
 */
void
SniffUtils::Finalize_Tag_PPDU(ppdu_id_t id)
{
    auto it = m_ppdu_runtime.find(id);
    if (it == m_ppdu_runtime.end())
    {
        return;
    }

    auto& rt = it->second;
    uint32_t idx = rt.ring_index;

    PPDU_Meta meta{};
    {
        scoped_lock<interprocess_mutex> lock(m_ring->mutex);
        meta = m_ring->records[idx];
    }

    if (rt.rx_success)
    {
        meta.rx_state = 1; // success
        meta.successDecodeTime = rt.success_decode_ns;
    }
    else if (rt.collision)
    {
        meta.rx_state = 2; // collision
        meta.collision = 1;
        meta.collision_time_ns = rt.collision_time_ns;
        meta.rx_fail_reason = BUSY_DECODING_PREAMBLE;
    }
    else if (rt.rx_drop)
    {
        meta.rx_state = 3; // decode fail
        meta.rx_fail_reason = rt.drop_reason;
    }
    else
    {
        meta.rx_state = 0; // unknown / not received
    }

    // Throughput is based on successfully received PPDUs only.
    UpdateRealtimeThroughput(meta, rt.rx_success ? meta.size_bytes : 0);
    g_finalizeCount++;
    if (rt.rx_success)
    {
        g_finalizeSuccessCount++;
    }
    if (meta.throughput_mbps_x100 > 0)
    {
        g_finalizeNonZeroThroughputCount++;
    }

    meta.snr_margin_db_x10 = 0;
    meta.snr_gap_db_x10 = 0;
    meta.access_delay_ns = rt.queue_delay_ns;
    meta.mac_e2e_delay_ns = rt.mac_e2e_delay_ns;
    if (rt.snr_db_x10 > 0)
    {
        meta.snr_margin_db_x10 = rt.snr_db_x10;
    }
    meta.tx_time_ns = meta.tx_end_ns;
    //std::cout << "PPDU[COMPLETED] " << std::endl;
    meta.record_type = SHM_RECORD_PPDU_UPDATE;
    AppendPpdu(m_ring, meta);

    m_ppdu_uid_to_id.erase(rt.ppdu_uid);
    if (rt.packet_uid != 0)
    {
        m_packet_uid_to_ppdu_id.erase(rt.packet_uid);
    }
    if (rt.seq_key != 0)
    {
        m_seq_key_to_ppdu_id.erase(rt.seq_key);
    }
    m_ppdu_runtime.erase(it);
    //PrintPpduMeta(idx);
}

void
SniffUtils::Sniff_tx_packet_begin(Ptr<const Packet> packet,
                                  uint16_t frequency,
                                  WifiTxVector txvector,
                                  MpduInfo mpdu_info,
                                  uint16_t sta_id)
{
    ;
}

void
SniffUtils::Sniff_tx_packet_end(Ptr<const Packet> packet)
{
    ;
}

void
SniffUtils::Sniff_rx_packet_begin(Ptr<const Packet> packet,
                                  uint16_t frequency,
                                  WifiTxVector txvector,
                                  MpduInfo mpdu_info,
                                  SignalNoiseDbm noise,
                                  uint16_t sta_id)
{
    (void)packet;
    (void)frequency;
    (void)txvector;
    (void)mpdu_info;
    (void)noise;
    (void)sta_id;
}

void
SniffUtils::Sniff_rx_ppdu_outcome(Ptr<const WifiPpdu> ppdu,
                                  RxSignalInfo rxSignalInfo,
                                  const WifiTxVector& txVector,
                                  const std::vector<bool>& outcomes)
{
    (void)txVector;
    g_rxOutcomeCount++;
    ppdu_id_t id = 0;
    bool found = false;

    auto ptrIt = m_ppdu_map.find(ppdu);
    if (ptrIt != m_ppdu_map.end())
    {
        const uint32_t ring_idx = ptrIt->second;
        scoped_lock<interprocess_mutex> lock(m_ring->mutex);
        id = m_ring->records[ring_idx].id;
        found = true;
        g_rxOutcomePtrHitCount++;
    }
    else
    {
        const auto uid = ppdu->GetUid();
        auto uidIt = m_ppdu_uid_to_id.find(uid);
        if (uidIt != m_ppdu_uid_to_id.end())
        {
            id = uidIt->second;
            found = true;
            g_rxOutcomeUidHitCount++;
        }
    }

    if (!found)
    {
        g_rxOutcomeMapMissCount++;
        return;
    }

    if (rxSignalInfo.snr > 0.0)
    {
        const double snrDb = 10.0 * std::log10(rxSignalInfo.snr);
        auto rt = m_ppdu_runtime.find(id);
        if (rt != m_ppdu_runtime.end())
        {
            rt->second.snr_db_x10 =
                static_cast<uint16_t>(std::max(0.0, std::round(snrDb * 10.0)));
        }
    }

    const bool hasSuccessfulMpdu =
        std::any_of(outcomes.begin(), outcomes.end(), [](bool ok) { return ok; });
    if (g_rxOutcomeDebugPrintCount < 120)
    {
        const auto trueCount = std::count(outcomes.begin(), outcomes.end(), true);
        // std::cout << "[THR-RXOUTCOME] id=" << id << " uid=" << ppdu->GetUid()
        //           << " outcomes=" << outcomes.size()
        //           << " true_count=" << trueCount
        //           << " has_success=" << (hasSuccessfulMpdu ? 1 : 0) << std::endl;
        g_rxOutcomeDebugPrintCount++;
    }
    if (!hasSuccessfulMpdu)
    {
        auto rt = m_ppdu_runtime.find(id);
        if (rt != m_ppdu_runtime.end())
        {
            rt->second.rx_drop = true;
        }
        return;
    }

    auto rt = m_ppdu_runtime.find(id);
    if (rt != m_ppdu_runtime.end())
    {
        rt->second.rx_success = true;
        rt->second.success_decode_ns = Simulator::Now().GetNanoSeconds();
        g_rxOutcomeSuccessCount++;
    }
}

void
SniffUtils::Sniff_drop_packet_phy(Ptr<const Packet> packet)
{
    ;
}

void
SniffUtils::Sniff_drop_ppdu_phy(Ptr<const WifiPpdu> ppdu, WifiPhyRxfailureReason drop_reason)
{
    ppdu_id_t id = 0;
    bool found = false;

    auto ptrIt = m_ppdu_map.find(ppdu);
    if (ptrIt != m_ppdu_map.end())
    {
        const uint32_t ring_idx = ptrIt->second;
        id = m_ring->records[ring_idx].id;
        found = true;
    }
    else
    {
        const auto uid = ppdu->GetUid();
        auto uidIt = m_ppdu_uid_to_id.find(uid);
        if (uidIt != m_ppdu_uid_to_id.end())
        {
            id = uidIt->second;
            found = true;
        }
    }
    if (!found)
    {
        return;
    }

    auto& rt = m_ppdu_runtime[id];
    rt.rx_drop = true;
    rt.drop_reason = drop_reason;
    rt.collision = IsCollisionLikeFailure(drop_reason);
    if (rt.collision)
    {
        rt.collision_time_ns = Simulator::Now().GetNanoSeconds();
    }
}

void
SniffUtils::Sniff_tx_psdu_begin(WifiConstPsduMap psdu_map, WifiTxVector txvector, double tx_power)
{
    ;
}

void
SniffUtils::Sniff_mac_header(const WifiMacHeader& wifi_mac_header,
                             const WifiTxVector& tx_vector,
                             Time time)
{
    ;
}

void
SniffUtils::Sniff_tx_all_packets(Ptr<const Packet> packet,
                                 uint16_t frequency,
                                 WifiTxVector txvector,
                                 MpduInfo mpdu_info,
                                 uint16_t sta_id)
{
    ;
}

void
SniffUtils::Sniff_ppdu_begin(uint16_t nodeId,
                             uint8_t linkId,
                             Ptr<const WifiPpdu> ppdu,
                             const WifiTxVector& tx_vector)
{
    /*Initialize Check*/
    if (!m_initialized)
    {
        std::cout << "SniffUtils not initialized" << std::endl;
        return;
    }

    /*Initalize & Separate the PPDU*/
    PPDU_Meta meta{};

    /*Get Psdu*/
    Ptr<const WifiPsdu> psdu_sample = ppdu->GetPsdu();

    /*Get Mode*/
    const WifiTxVector& tx_vector_sample = tx_vector;
    WifiMode mode = GetPayloadMode(ppdu, tx_vector_sample);

    /*Get ModulationClass*/
    WifiModulationClass modClass = mode.GetModulationClass();

    /*PPDU ID*/
    meta.id = m_next_ppdu_id++;
    meta.record_type = SHM_RECORD_PPDU;

    /*Channel ID*/
    uint8_t channels = GetPpduPrimaryChannel(ppdu);
    meta.channel_id = channels;
    meta.link_id = linkId;

    /* 确定 STA ID */
    meta.sta_id = nodeId;
    const auto roleIt = m_node_roles.find(nodeId);
    meta.device_role = roleIt != m_node_roles.end() ? roleIt->second : SHM_DEVICE_UNKNOWN;

    /*Frame_Type*/
    meta.frame_type = static_cast<uint8_t>(psdu_sample->GetHeader(0).GetType());

    /*MCS*/
    if (modClass == WIFI_MOD_CLASS_HT || modClass == WIFI_MOD_CLASS_VHT ||
        modClass == WIFI_MOD_CLASS_HE || modClass == WIFI_MOD_CLASS_EHT)
    {
        meta.mcs = mode.GetMcsValue();
    }
    else
    {
        meta.mcs = -1; // legacy / control / management
    }

    /*MPDU_AGGRE*/
    meta.mpdu_aggregation = psdu_sample->GetNMpdus();
    /*Size*/
    meta.size_bytes = psdu_sample->GetSize() + psdu_sample->GetHeader(0).GetSize();

    /*Address*/
    psdu_sample->GetHeader(0).GetAddr2().CopyTo(meta.sender);
    psdu_sample->GetHeader(0).GetAddr1().CopyTo(meta.receiver);

    /*TIME*/
    meta.tx_start_ns = Simulator::Now().GetNanoSeconds();
    Time duration = ppdu->GetTxDuration();
    meta.tx_duration_ns = duration.GetNanoSeconds();
    meta.tx_end_ns = meta.tx_start_ns + meta.tx_duration_ns;

    const uint64_t headPacketUid = PacketUid(psdu_sample->GetPayload(0));
    const uint64_t headSeqKey = MpduSeqKey(psdu_sample->GetHeader(0));
    if (headPacketUid != 0)
    {
        auto enqueueIt = m_mpdu_enqueue_time_ns.find(headPacketUid);
        if (enqueueIt != m_mpdu_enqueue_time_ns.end() && meta.tx_start_ns >= enqueueIt->second)
        {
            meta.access_delay_ns = meta.tx_start_ns - enqueueIt->second;
            EmitMacDelayRecord(m_ring,
                               nodeId,
                               meta.channel_id,
                               linkId,
                               meta.device_role,
                               psdu_sample->GetHeader(0),
                               meta.size_bytes,
                               meta.tx_start_ns,
                               meta.access_delay_ns,
                               SHM_DELAY_QUEUEING);
        }
    }

    // Snapshot current successful-throughput window without counting this PPDU yet.
    UpdateRealtimeThroughput(meta, 0);
    g_beginCount++;
    if (meta.throughput_mbps_x100 > 0)
    {
        g_beginNonZeroThroughputCount++;
    }

    /*RX_state*/
    meta.rx_state = 0;       // not received
    meta.rx_fail_reason = 0; // no failure
    meta.phy_state = SHM_PHY_STATE_TX;

    //std::cout << "PPDU[UNCOMPLETED] " << meta.id << " written to shared memory" << std::endl;

    // Write it (uncomplished) into the ring buffer
    const uint32_t ring_idx = AppendPpdu(m_ring, meta);

    // save the ppdu ptr
    m_ppdu_map[ppdu] = ring_idx;
    m_ppdu_uid_to_id[ppdu->GetUid()] = meta.id;
    if (headPacketUid != 0)
    {
        m_packet_uid_to_ppdu_id[headPacketUid] = meta.id;
    }
    if (headSeqKey != 0)
    {
        m_seq_key_to_ppdu_id[headSeqKey] = meta.id;
    }

    m_ppdu_runtime[meta.id] = {.ring_index = ring_idx,
                               .ppdu_uid = ppdu->GetUid(),
                               .packet_uid = headPacketUid,
                               .seq_key = headSeqKey,
                               .queue_delay_ns = meta.access_delay_ns};

    Simulator::Schedule(ppdu->GetTxDuration() + MicroSeconds(200),
                        &SniffUtils::Finalize_Tag_PPDU,
                        this,
                        meta.id);
}

void
SniffUtils::NotifyStateChange(uint16_t nodeId,
                              uint8_t channelId,
                              uint8_t linkId,
                              Time start,
                              Time duration,
                              WifiPhyState state)
{
    if (!m_initialized || !m_ring || !duration.IsStrictlyPositive())
    {
        return;
    }

    PPDU_Meta meta{};
    meta.record_type = SHM_RECORD_PHY_STATE;
    meta.sta_id = nodeId;
    meta.channel_id = channelId;
    meta.link_id = linkId;
    const auto roleIt = m_node_roles.find(nodeId);
    meta.device_role = roleIt != m_node_roles.end() ? roleIt->second : SHM_DEVICE_UNKNOWN;
    meta.phy_state = ToSharedPhyState(state);
    meta.phy_state_start_ns = start.GetNanoSeconds();
    meta.phy_state_duration_ns = duration.GetNanoSeconds();
    meta.phy_state_end_ns = meta.phy_state_start_ns + meta.phy_state_duration_ns;

    AppendPpdu(m_ring, meta);
}
void
SniffUtils::Set_simulation_time(double simulation_time)
{
    ;
}

using namespace boost::interprocess;

uint32_t
AppendPpdu(RingBuffer* buffer, const PPDU_Meta& ppdu)
{
    scoped_lock<interprocess_mutex> lock(buffer->mutex);

    uint32_t idx = buffer->write_index % MAX_PPDU_NUM;
    buffer->records[idx] = ppdu;
    buffer->write_index++;

    buffer->cond.notify_one(); // inform the reader in Qt
    return idx;
}

void
ShmExample()
{
    managed_shared_memory shm(open_or_create, SHM_NAME,
                              1024UL * 1024 * 1024); // 1GB

    RingBuffer* ring = shm.find_or_construct<RingBuffer>("PpduRing")();
    ring->write_index = 0;
    ring->read_index = 0;

    PPDU_Meta ppdu{};
    ppdu.id = 1;
    ppdu.sta_id = 1;
    ppdu.frame_type = 1;
    ppdu.mcs = 7;
    ppdu.size_bytes = 1024;
    uint8_t tmp_sender[6] = {1, 0, 0, 0, 0, 0};
    std::copy(std::begin(tmp_sender), std::end(tmp_sender), std::begin(ppdu.sender));

    uint8_t tmp_receiver[6] = {0, 0, 0, 0, 0, 0};
    std::copy(std::begin(tmp_receiver), std::end(tmp_receiver), std::begin(ppdu.receiver));

    ppdu.tx_start_ns = 1000;
    ppdu.tx_end_ns = 1200;
    ppdu.tx_duration_ns = ppdu.tx_end_ns - ppdu.tx_start_ns;
    ppdu.rx_state = 1; // success

    AppendPpdu(ring, ppdu);

    //std::cout << "PPDU written to shared memory" << std::endl;
}

void
SniffUtils::PrintPpduMeta(uint32_t ring_index) const
{
    if (!m_ring)
    {
        std::cout << "[PrintPpduMeta] RingBuffer not initialized\n";
        return;
    }

    if (ring_index >= MAX_PPDU_NUM)
    {
        std::cout << "[PrintPpduMeta] Invalid ring index\n";
        return;
    }

    const PPDU_Meta& m = m_ring->records[ring_index];

    std::cout << "\n========== PPDU META ==========\n";
    std::cout << "ID            : " << m.id << "\n";
    std::cout << "STA ID        : " << m.sta_id << "\n";
    std::cout << "Channel ID    : " << static_cast<int>(m.channel_id) << "\n";

    std::cout << "Frame Type    : " << static_cast<int>(m.frame_type) << "\n";
    std::cout << "MCS           : " << static_cast<int>(m.mcs) << "\n";
    std::cout << "MPDUs         : " << m.mpdu_aggregation << "\n";
    std::cout << "Size (bytes)  : " << m.size_bytes << "\n";
    std::cout << "Throughput    : " << m.throughput_mbps_x100 / 100.0 << " Mbps\n";

    std::cout << "Sender        : ";
    for (int i = 0; i < 6; ++i)
    {
        std::cout << std::hex << static_cast<int>(m.sender[i]) << (i < 5 ? ":" : "");
    }
    std::cout << std::dec << "\n";

    std::cout << "Receiver      : ";
    for (int i = 0; i < 6; ++i)
    {
        std::cout << std::hex << static_cast<int>(m.receiver[i]) << (i < 5 ? ":" : "");
    }
    std::cout << std::dec << "\n";

    std::cout << "TX start (ns) : " << m.tx_start_ns << "\n";
    std::cout << "TX end   (ns) : " << m.tx_end_ns << "\n";
    std::cout << "TX dur   (ns) : " << m.tx_duration_ns << "\n";

    std::cout << "RX state      : " << static_cast<int>(m.rx_state) << "\n";
    std::cout << "Fail reason   : " << static_cast<int>(m.rx_fail_reason) << "\n";

    if (m.rx_state == 1)
    {
        std::cout << "Decode time   : " << m.successDecodeTime << " ns\n";
    }

    if (m.collision)
    {
        std::cout << "Collision @   : " << m.collision_time_ns << " ns\n";
    }

    if (m.snr_margin_db_x10 != 0)
    {
        std::cout << "SNR        : " << m.snr_margin_db_x10 / 10.0 << " dB\n";
    }

    if (m.snr_gap_db_x10 != 0)
    {
        std::cout << "SNR gap       : " << m.snr_gap_db_x10 / 10.0 << " dB\n";
    }

    else
    {
        std::cout << "SNR gap       : N/A\n";
    }

    std::cout << "================================\n";
}

void
SniffUtils::PrintLastPpdu() const
{
    if (!m_ring || m_ring->write_index == 0)
    {
        std::cout << "[PrintLastPpdu] No PPDU available\n";
        return;
    }

    uint32_t last = (m_ring->write_index - 1) % MAX_PPDU_NUM;

    PrintPpduMeta(last);
}

} // namespace ns3
