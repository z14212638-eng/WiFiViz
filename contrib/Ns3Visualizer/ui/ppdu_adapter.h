// ppdu_adapter.h
#pragma once
#include "shm.h"
#include "ppdu_visual_item.h"

class PpduAdapter
{
public:
    static uint64_t MacToUInt64(const uint8_t mac[6])
    {
        uint64_t value = 0;
        for (int i = 0; i < 6; ++i)
        {
            value = (value << 8) | mac[i];
        }
        return value;
    }

    enum frame_type_t
    {
        WIFI_MAC_CTL_TRIGGER = 0,
        WIFI_MAC_CTL_CTLWRAPPER,
        WIFI_MAC_CTL_PSPOLL,
        WIFI_MAC_CTL_RTS,
        WIFI_MAC_CTL_CTS,
        WIFI_MAC_CTL_ACK,
        WIFI_MAC_CTL_BACKREQ,
        WIFI_MAC_CTL_BACKRESP,
        WIFI_MAC_CTL_END,
        WIFI_MAC_CTL_END_ACK,

        WIFI_MAC_CTL_DMG_POLL,
        WIFI_MAC_CTL_DMG_SPR,
        WIFI_MAC_CTL_DMG_GRANT,
        WIFI_MAC_CTL_DMG_CTS,
        WIFI_MAC_CTL_DMG_DTS,
        WIFI_MAC_CTL_DMG_SSW,
        WIFI_MAC_CTL_DMG_SSW_FBCK,
        WIFI_MAC_CTL_DMG_SSW_ACK,
        WIFI_MAC_CTL_DMG_GRANT_ACK,

        WIFI_MAC_MGT_BEACON,
        WIFI_MAC_MGT_ASSOCIATION_REQUEST,
        WIFI_MAC_MGT_ASSOCIATION_RESPONSE,
        WIFI_MAC_MGT_DISASSOCIATION,
        WIFI_MAC_MGT_REASSOCIATION_REQUEST,
        WIFI_MAC_MGT_REASSOCIATION_RESPONSE,
        WIFI_MAC_MGT_PROBE_REQUEST,
        WIFI_MAC_MGT_PROBE_RESPONSE,
        WIFI_MAC_MGT_AUTHENTICATION,
        WIFI_MAC_MGT_DEAUTHENTICATION,
        WIFI_MAC_MGT_ACTION,
        WIFI_MAC_MGT_ACTION_NO_ACK,
        WIFI_MAC_MGT_MULTIHOP_ACTION,

        WIFI_MAC_DATA,
        WIFI_MAC_DATA_CFACK,
        WIFI_MAC_DATA_CFPOLL,
        WIFI_MAC_DATA_CFACK_CFPOLL,
        WIFI_MAC_DATA_NULL,
        WIFI_MAC_DATA_NULL_CFACK,
        WIFI_MAC_DATA_NULL_CFPOLL,
        WIFI_MAC_DATA_NULL_CFACK_CFPOLL,
        WIFI_MAC_QOSDATA,
        WIFI_MAC_QOSDATA_CFACK,
        WIFI_MAC_QOSDATA_CFPOLL,
        WIFI_MAC_QOSDATA_CFACK_CFPOLL,
        WIFI_MAC_QOSDATA_NULL,
        WIFI_MAC_QOSDATA_NULL_CFPOLL,
        WIFI_MAC_QOSDATA_NULL_CFACK_CFPOLL,

        WIFI_MAC_EXTENSION_DMG_BEACON,
    };

    static std::string FrameTypeToString(frame_type_t frameType)
    {
        switch (frameType)
        {
            case WIFI_MAC_CTL_TRIGGER: return "CTL_Trigger";
            case WIFI_MAC_CTL_CTLWRAPPER: return "CTL_CtlWrapper";
            case WIFI_MAC_CTL_PSPOLL: return "CTL_PSPoll";
            case WIFI_MAC_CTL_RTS: return "CTL_RTS";
            case WIFI_MAC_CTL_CTS: return "CTL_CTS";
            case WIFI_MAC_CTL_ACK: return "CTL_ACK";
            case WIFI_MAC_CTL_BACKREQ: return "CTL_BackReq";
            case WIFI_MAC_CTL_BACKRESP: return "CTL_BackResp";
            case WIFI_MAC_CTL_END: return "CTL_End";
            case WIFI_MAC_CTL_END_ACK: return "CTL_EndAck";

            case WIFI_MAC_CTL_DMG_POLL: return "CTL_DMGPoll";
            case WIFI_MAC_CTL_DMG_SPR: return "CTL_DMGSPR";
            case WIFI_MAC_CTL_DMG_GRANT: return "CTL_DMGGrant";
            case WIFI_MAC_CTL_DMG_CTS: return "CTL_DMGCTS";
            case WIFI_MAC_CTL_DMG_DTS: return "CTL_DMGDTS";
            case WIFI_MAC_CTL_DMG_SSW: return "CTL_DMGSSW";
            case WIFI_MAC_CTL_DMG_SSW_FBCK: return "CTL_DMGSSWFBCK";
            case WIFI_MAC_CTL_DMG_SSW_ACK: return "CTL_DMGSSWACK";
            case WIFI_MAC_CTL_DMG_GRANT_ACK: return "CTL_DMGGrantAck";

            case WIFI_MAC_MGT_BEACON: return "MGT_Beacon";
            case WIFI_MAC_MGT_ASSOCIATION_REQUEST: return "MGT_AssocReq";
            case WIFI_MAC_MGT_ASSOCIATION_RESPONSE: return "MGT_AssocResp";
            case WIFI_MAC_MGT_DISASSOCIATION: return "MGT_Disassoc";
            case WIFI_MAC_MGT_REASSOCIATION_REQUEST: return "MGT_ReassocReq";
            case WIFI_MAC_MGT_REASSOCIATION_RESPONSE: return "MGT_ReassocResp";
            case WIFI_MAC_MGT_PROBE_REQUEST: return "MGT_ProbeReq";
            case WIFI_MAC_MGT_PROBE_RESPONSE: return "MGT_ProbeResp";
            case WIFI_MAC_MGT_AUTHENTICATION: return "MGT_Auth";
            case WIFI_MAC_MGT_DEAUTHENTICATION: return "MGT_Deauth";
            case WIFI_MAC_MGT_ACTION: return "MGT_Action";
            case WIFI_MAC_MGT_ACTION_NO_ACK: return "MGT_ActionNoAck";
            case WIFI_MAC_MGT_MULTIHOP_ACTION: return "MGT_MultihopAction";

            case WIFI_MAC_DATA: return "DATA";
            case WIFI_MAC_DATA_CFACK: return "DATA_CFACK";
            case WIFI_MAC_DATA_CFPOLL: return "DATA_CFPoll";
            case WIFI_MAC_DATA_CFACK_CFPOLL: return "DATA_CFACKCFPoll";
            case WIFI_MAC_DATA_NULL: return "DATA_Null";
            case WIFI_MAC_DATA_NULL_CFACK: return "DATA_NullCFACK";
            case WIFI_MAC_DATA_NULL_CFPOLL: return "DATA_NullCFPoll";
            case WIFI_MAC_DATA_NULL_CFACK_CFPOLL: return "DATA_NullCFACKCFPoll";
            case WIFI_MAC_QOSDATA: return "QoSDATA";
            case WIFI_MAC_QOSDATA_CFACK: return "QoSDATA_CFACK";
            case WIFI_MAC_QOSDATA_CFPOLL: return "QoSDATA_CFPoll";
            case WIFI_MAC_QOSDATA_CFACK_CFPOLL: return "QoSDATA_CFACKCFPoll";
            case WIFI_MAC_QOSDATA_NULL: return "QoSDATA_Null";
            case WIFI_MAC_QOSDATA_NULL_CFPOLL: return "QoSDATA_NullCFPoll";
            case WIFI_MAC_QOSDATA_NULL_CFACK_CFPOLL: return "QoSDATA_NullCFACKCFPoll";

            case WIFI_MAC_EXTENSION_DMG_BEACON: return "DMGBeacon";

            default: return "Unknown";
        }
    }

    static PpduVisualItem FromShm(const PPDU_Meta &m)
    {
        PpduVisualItem v{};
        v.recordType = static_cast<RecordType>(m.record_type);
        v.id = m.id;
        v.nodeId = m.sta_id;
        v.channel_number = m.channel_id;
        v.rxState = static_cast<RxState>(m.rx_state);
        v.failReason = m.rx_fail_reason;
        v.collision = (m.collision != 0);
        v.collisionTimeNs = m.collision_time_ns;
        v.phyState = static_cast<PhyStateKind>(m.phy_state);
        v.phyStateStartNs = m.phy_state_start_ns;
        v.phyStateEndNs = m.phy_state_end_ns;
        v.phyStateDurationNs = m.phy_state_duration_ns;
        v.deviceRole = static_cast<DeviceRole>(m.device_role);
        v.roleMac = MacToUInt64(m.device_mac);
        v.nodeMac = v.roleMac;
        v.delayKind = static_cast<DelayKind>(m.delay_kind);

        if (v.recordType == RecordType::PhyState)
        {
            v.txStartNs = m.phy_state_start_ns;
            v.txEndNs = m.phy_state_end_ns;
            v.durationNs = m.phy_state_duration_ns;
            v.frameType = "PHY_STATE";
            return v;
        }

        if (v.recordType == RecordType::DeviceRole)
        {
            v.sender = v.roleMac;
            v.receiver = 0;
            v.frameType = "DEVICE_ROLE";
            return v;
        }

        if (v.recordType == RecordType::MacDelay)
        {
            v.sender = MacToUInt64(m.sender);
            v.receiver = MacToUInt64(m.receiver);
            v.txStartNs = m.tx_start_ns;
            v.txEndNs = m.tx_end_ns;
            v.durationNs = m.tx_duration_ns;
            v.accessDelayNs = m.access_delay_ns;
            v.macE2eDelayNs = m.mac_e2e_delay_ns;
            v.size = m.size_bytes;
            v.frameType = "MAC_DELAY";
            return v;
        }

        v.sender = MacToUInt64(m.sender);
        v.receiver = MacToUInt64(m.receiver);
        v.txStartNs = m.tx_start_ns;
        v.txEndNs = m.tx_end_ns;
        v.durationNs = m.tx_duration_ns;
        v.accessDelayNs = m.access_delay_ns;
        v.macE2eDelayNs = m.mac_e2e_delay_ns;
        v.successDecodeNs = m.successDecodeTime;
        v.size = m.size_bytes;
        v.throughputMbpsX100 = m.throughput_mbps_x100;

        v.mcs = (m.mcs == 255) ? -1 : m.mcs;
        v.snrDbX10 = (m.snr_margin_db_x10 != 0) ? m.snr_margin_db_x10 : m.snr_gap_db_x10;
        v.mpduAggregation = m.mpdu_aggregation;
        v.frameType = FrameTypeToString(static_cast<frame_type_t>(m.frame_type));
        return v;
    }
};
