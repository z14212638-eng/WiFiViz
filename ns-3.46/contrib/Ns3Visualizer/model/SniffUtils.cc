#include "SniffUtils.h"

#include "ns3/ampdu-subframe-header.h"
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

#include <boost/interprocess/managed_shared_memory.hpp>
#include <boost/interprocess/sync/scoped_lock.hpp>
#include <deque>

using namespace boost::interprocess;
boost::interprocess::managed_shared_memory* m_shm = nullptr;

namespace ns3
{

namespace
{

constexpr sim_time_ns_t THROUGHPUT_WINDOW_NS = 100000000ULL; // 100 ms
std::deque<std::pair<sim_time_ns_t, uint32_t>> g_throughputWindow;
uint64_t g_throughputBytesInWindow = 0;

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

void
UpdateRealtimeThroughput(PPDU_Meta& meta)
{
    const sim_time_ns_t nowNs = meta.tx_end_ns;
    const sim_time_ns_t windowStartNs =
        (nowNs > THROUGHPUT_WINDOW_NS) ? (nowNs - THROUGHPUT_WINDOW_NS) : 0;

    while (!g_throughputWindow.empty() && g_throughputWindow.front().first < windowStartNs)
    {
        g_throughputBytesInWindow -= g_throughputWindow.front().second;
        g_throughputWindow.pop_front();
    }

    g_throughputWindow.emplace_back(nowNs, meta.size_bytes);
    g_throughputBytesInWindow += meta.size_bytes;

    const double throughputMbps =
        (static_cast<double>(g_throughputBytesInWindow) * 8.0) /
        (static_cast<double>(THROUGHPUT_WINDOW_NS) / 1e9) / 1e6;

    meta.throughput_mbps_x100 =
        static_cast<uint32_t>(std::llround(std::max(0.0, throughputMbps) * 100.0));
}

} // namespace

std::unordered_map<ppdu_id_t, PpduRuntime> m_ppdu_runtime;
std::optional<ActivePpdu> m_active_ppdu;

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

bool
SniffUtils::Initialize(NetDeviceContainer sender,
                       NetDeviceContainer receiver,
                       double simulationTime)
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

    for (auto senderNetDevice = sender.Begin(); senderNetDevice != sender.End(); senderNetDevice++)
    {
        Ptr<WifiNetDevice> senderDevice = DynamicCast<WifiNetDevice>(*senderNetDevice);
        const auto nodeId = static_cast<uint16_t>(senderDevice->GetNode()->GetId());
        Ptr<WifiPhy> phy = senderDevice->GetPhy();
        const auto channelId = phy->GetChannelNumber();
        phy->TraceConnectWithoutContext("SignalTransmission",
                                        MakeCallback(&SniffUtils::Sniff_ppdu_begin, this, nodeId));

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
                                                              channelId));
    }

    m_initialized = true;
    return true;
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

    double centerFreq = freqs[0]; // 主中心频率
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

    scoped_lock<interprocess_mutex> lock(m_ring->mutex);
    PPDU_Meta& meta = m_ring->records[idx];

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

    if (m_active_ppdu.has_value() && m_active_ppdu->snr_cnt > 0)
    {
        meta.snr_gap_db_x10 = m_active_ppdu->snr_sum / m_active_ppdu->snr_cnt;
    }
    meta.tx_time_ns = meta.tx_end_ns;
    std::cout << "PPDU[COMPLETED] " << std::endl;
    m_active_ppdu.reset();
    m_ppdu_runtime.erase(it);
    PrintPpduMeta(idx);
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
    if (!m_active_ppdu.has_value())
    {
        return;
    }

    auto& current_ppdu = *m_active_ppdu;
    // PPDU_Meta& meta = m_ring->records[current_ppdu.ring_index];

    uint16_t snr_db_x10 = static_cast<uint16_t>(std::round((noise.signal - noise.noise) * 10));

    current_ppdu.snr_sum += snr_db_x10;
    current_ppdu.snr_cnt++;
}

void
SniffUtils::Sniff_drop_packet_phy(Ptr<const Packet> packet)
{
    ;
}

void
SniffUtils::Sniff_drop_ppdu_phy(Ptr<const WifiPpdu> ppdu, WifiPhyRxfailureReason drop_reason)
{
    auto it = m_ppdu_map.find(ppdu);
    if (it == m_ppdu_map.end())
    {
        return;
    }

    uint32_t ring_idx = it->second;
    ppdu_id_t id = m_ring->records[ring_idx].id;

    auto& rt = m_ppdu_runtime[id];
    rt.rx_drop = true;
    rt.drop_reason = drop_reason;
    rt.collision = (drop_reason == BUSY_DECODING_PREAMBLE);
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

    /*Get TxVector*/
    const WifiTxVector tx_vector_sample = ppdu->GetTxVector();

    /*Get Mode*/
    WifiMode mode = tx_vector_sample.GetMode();

    /*Get ModulationClass*/
    WifiModulationClass modClass = mode.GetModulationClass();

    /*PPDU ID*/
    meta.id = m_next_ppdu_id++;
    meta.record_type = SHM_RECORD_PPDU;

    /*Channel ID*/
    uint8_t channels = GetPpduPrimaryChannel(ppdu);
    meta.channel_id = channels;

    /* 确定 STA ID */
    meta.sta_id = nodeId;

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
    UpdateRealtimeThroughput(meta);

    /*RX_state*/
    meta.rx_state = 0;       // not received
    meta.rx_fail_reason = 0; // no failure

    std::cout << "PPDU[UNCOMPLETED] " << meta.id << " written to shared memory" << std::endl;

    // Write it (uncomplished) into the ring buffer
    const uint32_t ring_idx = AppendPpdu(m_ring, meta);

    // save the ppdu ptr
    m_ppdu_map[ppdu] = ring_idx;

    // add to Active ppdu(current ppdu)
    m_active_ppdu = ActivePpdu{ppdu, meta.id, ring_idx, 0, 0};

    m_ppdu_runtime[meta.id] = {.ring_index = ring_idx};

    Simulator::Schedule(ppdu->GetTxDuration(), &SniffUtils::Finalize_Tag_PPDU, this, meta.id);
}

void
SniffUtils::NotifyStateChange(uint16_t nodeId,
                              uint8_t channelId,
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

    std::cout << "PPDU written to shared memory" << std::endl;
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
