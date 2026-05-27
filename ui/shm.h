//shm.h
#pragma once

#include <boost/interprocess/managed_shared_memory.hpp>
#include <boost/interprocess/sync/interprocess_mutex.hpp>
#include <boost/interprocess/sync/interprocess_condition.hpp>
#include <iostream>
#include <thread>
#include <chrono>

using sim_time_ns_t = uint64_t;
using ppdu_id_t = uint32_t;
constexpr std::size_t MAX_PPDU_NUM = 1 << 20; // about 1 million PPDUs

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
    sim_time_ns_t access_delay_ns;   // Queueing delay: enqueue -> first PPDU TX begin
    sim_time_ns_t mac_e2e_delay_ns;  // MAC end-to-end delay: enqueue -> ACK

    uint8_t rx_state;       // 0=unknown 1=success 2=collision 3=decode_fail
    uint8_t rx_fail_reason; // WifiPhyfailureReason
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
    uint8_t link_id;
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
