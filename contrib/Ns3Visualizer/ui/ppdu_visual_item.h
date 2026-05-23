// ppdu_visual_item.h
#pragma once
#include <cstdint>
#include <string>

enum class RxState : uint8_t
{
    Unknown = 0,
    Success,
    Collision,
    DecodeFail
};

enum class RecordType : uint8_t
{
    Ppdu = 0,
    PhyState = 1,
    DeviceRole = 2,
    PpduUpdate = 3,
    MacDelay = 4
};

enum class DelayKind : uint8_t
{
    Unknown = 0,
    Queueing = 1,
    MacE2e = 2
};

enum class DeviceRole : uint8_t
{
    Unknown = 0,
    Ap = 1,
    Sta = 2
};

enum class PhyStateKind : uint8_t
{
    Unknown = 0,
    Idle = 1,
    CcaBusy = 2,
    Tx = 3,
    Rx = 4,
    Switching = 5,
    Sleep = 6,
    Off = 7
};

struct PpduVisualItem
{
    RecordType recordType = RecordType::Ppdu;

    // identity
    uint32_t id;
    uint16_t nodeId;
    uint8_t channel_number;
    uint64_t sender;
    uint64_t receiver;

    // time
    uint64_t txStartNs;
    uint64_t txEndNs;
    uint64_t durationNs;
    uint64_t accessDelayNs = 0;
    uint64_t macE2eDelayNs = 0;
    uint64_t successDecodeNs = 0;
    DelayKind delayKind = DelayKind::Unknown;

    // logical
    int16_t  mcs;
    uint16_t snrDbX10;
    uint16_t mpduAggregation;
    std::string frameType;
    uint32_t size;
    uint32_t throughputMbpsX100 = 0;

    RxState  rxState;
    uint8_t  failReason;
    bool collision = false;
    uint64_t collisionTimeNs = 0;

    PhyStateKind phyState = PhyStateKind::Unknown;
    uint64_t phyStateStartNs = 0;
    uint64_t phyStateEndNs = 0;
    uint64_t phyStateDurationNs = 0;

    DeviceRole deviceRole = DeviceRole::Unknown;
    uint64_t roleMac = 0;
    uint64_t nodeMac = 0;

    // UI-only (IMPORTANT)
    bool hovered = false;
};
