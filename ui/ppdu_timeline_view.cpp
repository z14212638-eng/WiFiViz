#include "ppdu_timeline_view.h"
#include "visualizer_config.h"

#include <QPainter>
#include <QMouseEvent>
#include <QMap>
#include <QHash>
#include <QTimer>
#include <QWheelEvent>
#include <QToolTip>

#include <algorithm>
#include <cmath>
#include <complex>
#include <cstdlib>
#include <iostream>
#include <limits>

/* ======================== Constants ======================== */

static constexpr int kBottomMargin = 72;
static constexpr int kLanePadding = 2;
static constexpr int kConflictAxisHeight = 10;
static constexpr int kConflictAxisGap = 8;
/* ====== UI Colors ======= */
static const QColor kBgColor(255, 255, 255);
static const QColor kHoverRow(230, 235, 240);
static const QColor kGridLine(200, 205, 210);
static const QColor kPpduBlue(76, 114, 176);
static const QColor kPpduHover(220, 50, 47);
static const QColor kSelectFill(80, 140, 255, 60);
static const QColor kSelectBorder(80, 140, 255, 160);
static const QColor kConflictRed(219, 68, 55);

static constexpr int kRangeBarHeight = 16;
static constexpr int kRangeHandleW = 8;
static constexpr int kRangeBarMargin = 10;
static constexpr int kRangeBarBottomPadding = 10;

static constexpr uint64_t kBroadcastMac = 0xFFFFFFFFFFFFULL;

static inline bool isBroadcastMac(uint64_t addr)
{
    uint64_t low48 = addr & 0xFFFFFFFFFFFFULL;
    uint64_t high48 = (addr >> 16) & 0xFFFFFFFFFFFFULL;
    if (low48 == kBroadcastMac || high48 == kBroadcastMac)
        return true;

    uint8_t bytesLE[6];
    uint8_t bytesBE[6];
    for (int i = 0; i < 6; ++i)
    {
        bytesLE[i] = (addr >> (i * 8)) & 0xFF;
        bytesBE[5 - i] = bytesLE[i];
    }

    auto isAllFF = [](const uint8_t *b) {
        for (int i = 0; i < 6; ++i)
        {
            if (b[i] != 0xFF)
                return false;
        }
        return true;
    };

    if (isAllFF(bytesLE) || isAllFF(bytesBE))
        return true;

    auto isFFWithLeadingZeros = [](const uint8_t *b) {
        return b[0] == 0x00 && b[1] == 0x00 &&
               b[2] == 0xFF && b[3] == 0xFF &&
               b[4] == 0xFF && b[5] == 0xFF;
    };

    auto isFFWithTrailingZeros = [](const uint8_t *b) {
        return b[0] == 0xFF && b[1] == 0xFF &&
               b[2] == 0xFF && b[3] == 0xFF &&
               b[4] == 0x00 && b[5] == 0x00;
    };

    return isFFWithLeadingZeros(bytesBE) || isFFWithTrailingZeros(bytesLE);
}

static inline QString formatMac(uint64_t addr)
{
    if (isBroadcastMac(addr))
        return "FF:FF:FF:FF:FF:FF";

    uint8_t bytesLE[6];
    uint8_t bytesBE[6];

    for (int i = 0; i < 6; ++i)
    {
        bytesLE[i] = (addr >> (i * 8)) & 0xFF;
        bytesBE[5 - i] = bytesLE[i];
    }

    auto countNonZero = [](const uint8_t *b) {
        int c = 0;
        for (int i = 0; i < 6; ++i)
            c += (b[i] != 0);
        return c;
    };

    auto isTrailingSingleByte = [](const uint8_t *b) {
        for (int i = 0; i < 5; ++i)
        {
            if (b[i] != 0)
                return false;
        }
        return b[5] != 0;
    };

    const uint8_t *chosen = bytesBE; // default: standard MAC order

    if (isTrailingSingleByte(bytesLE) || isTrailingSingleByte(bytesBE))
    {
        chosen = isTrailingSingleByte(bytesLE) ? bytesLE : bytesBE;
    }
    else
    {
        int nzLE = countNonZero(bytesLE);
        int nzBE = countNonZero(bytesBE);
        if (nzLE == 1 && nzBE == 1)
        {
            uint8_t value = 0;
            for (int i = 0; i < 6; ++i)
            {
                if (bytesLE[i] != 0)
                {
                    value = bytesLE[i];
                    break;
                }
            }
            bytesBE[0] = 0;
            bytesBE[1] = 0;
            bytesBE[2] = 0;
            bytesBE[3] = 0;
            bytesBE[4] = 0;
            bytesBE[5] = value;
            chosen = bytesBE;
        }
    }

    QStringList parts;
    for (int i = 0; i < 6; ++i)
    {
        parts << QString("%1")
                     .arg(chosen[i], 2, 16, QChar('0'));
    }
    return parts.join(":").toUpper();
}

static inline bool isZeroMac(uint64_t addr)
{
    return (addr & 0xFFFFFFFFFFFFULL) == 0;
}

static inline bool isDisplayedAsBroadcast(uint64_t addr)
{
    return isBroadcastMac(addr) || isZeroMac(addr);
}

static inline QString deviceRoleName(DeviceRole role)
{
    switch (role)
    {
    case DeviceRole::Ap:
        return "AP";
    case DeviceRole::Sta:
        return "STA";
    default:
        return "DEV";
    }
}

static inline QString rxStateName(RxState state)
{
    switch (state)
    {
    case RxState::Success:
        return "SUCCESS";
    case RxState::Collision:
        return "COLLISION";
    case RxState::DecodeFail:
        return "DECODE_FAIL";
    default:
        return "UNKNOWN";
    }
}

static inline QColor rxStateAccent(RxState state)
{
    switch (state)
    {
    case RxState::Success:
        return QColor("#2E8B57");
    case RxState::Collision:
        return QColor("#DB4437");
    case RxState::DecodeFail:
        return QColor("#C7922F");
    default:
        return QColor("#5C6D7E");
    }
}

static inline QColor rxStateFill(RxState state)
{
    switch (state)
    {
    case RxState::Success:
        return QColor(46, 139, 87, 220);
    case RxState::Collision:
        return QColor(219, 68, 55, 225);
    case RxState::DecodeFail:
        return QColor(199, 146, 47, 225);
    default:
        return QColor(92, 109, 126, 165);
    }
}

static inline QString phyStateBadgeName(PhyStateKind state)
{
    switch (state)
    {
    case PhyStateKind::Idle:
        return "IDLE";
    case PhyStateKind::CcaBusy:
        return "CCA_BUSY";
    case PhyStateKind::Tx:
        return "TX";
    case PhyStateKind::Rx:
        return "RX";
    case PhyStateKind::Switching:
        return "SWITCHING";
    case PhyStateKind::Sleep:
        return "SLEEP";
    case PhyStateKind::Off:
        return "OFF";
    default:
        return "UNKNOWN";
    }
}

static inline QColor phyStateBadgeAccent(PhyStateKind state)
{
    switch (state)
    {
    case PhyStateKind::Idle:
        return QColor("#6E7C89");
    case PhyStateKind::CcaBusy:
        return QColor("#C7922F");
    case PhyStateKind::Tx:
        return QColor("#2A72D4");
    case PhyStateKind::Rx:
        return QColor("#2E8B57");
    case PhyStateKind::Switching:
        return QColor("#8A63D2");
    case PhyStateKind::Sleep:
        return QColor("#5C6D7E");
    case PhyStateKind::Off:
        return QColor("#39424E");
    default:
        return QColor("#5C6D7E");
    }
}

static inline bool isDrawablePpdu(const PpduVisualItem &item);

static void applyToolbarButtonStyle(QPushButton* button, bool compact = true)
{
    if (!button)
    {
        return;
    }
    if (compact)
    {
        button->setFixedHeight(32);
    }
    button->setCursor(Qt::PointingHandCursor);
    button->setStyleSheet(
        "QPushButton {"
        "background: rgba(255,255,255,0.94);"
        "border: 1px solid #D5DDE8;"
        "border-radius: 10px;"
        "color: #314154;"
        "padding: 0 12px;"
        "font-weight: 600;"
        "}"
        "QPushButton:hover {"
        "background: #F3F7FC;"
        "border-color: #C7D4E4;"
        "}"
        "QPushButton:pressed {"
        "background: #E8F0FA;"
        "}");
}

struct ConflictSegment
{
    uint64_t startNs = 0;
    uint64_t endNs = 0;
};

struct CollisionSummary
{
    bool hasCollision = false;
    uint64_t collisionTimeNs = 0;
};

static QVector<ConflictSegment>
buildConflictSegments(const QVector<PpduVisualItem> &items)
{
    struct Event
    {
        uint64_t time = 0;
        int delta = 0;
    };

    QMap<int, QVector<Event>> channelEvents;
    for (const auto &item : items)
    {
        if (!isDrawablePpdu(item))
            continue;
        if (item.txStartNs >= item.txEndNs)
            continue;

        channelEvents[item.channel_number].push_back({item.txStartNs, +1});
        channelEvents[item.channel_number].push_back({item.txEndNs, -1});
    }

    QVector<ConflictSegment> segments;
    for (auto it = channelEvents.cbegin(); it != channelEvents.cend(); ++it)
    {
        QVector<Event> events = it.value();
        if (events.isEmpty())
            continue;

        std::sort(events.begin(), events.end(),
                  [](const Event &a, const Event &b) {
                      if (a.time != b.time)
                          return a.time < b.time;
                      return a.delta < b.delta;
                  });

        int activeCount = 0;
        uint64_t lastTime = 0;
        bool hasLastTime = false;

        for (int i = 0; i < events.size();)
        {
            const uint64_t eventTime = events[i].time;

            if (hasLastTime && lastTime < eventTime && activeCount > 1)
            {
                segments.push_back({lastTime, eventTime});
            }

            int deltaSum = 0;
            while (i < events.size() && events[i].time == eventTime)
            {
                deltaSum += events[i].delta;
                ++i;
            }

            activeCount = std::max(0, activeCount + deltaSum);
            lastTime = eventTime;
            hasLastTime = true;
        }
    }

    std::sort(segments.begin(), segments.end(),
              [](const ConflictSegment &a, const ConflictSegment &b) {
                  if (a.startNs != b.startNs)
                      return a.startNs < b.startNs;
                  return a.endNs < b.endNs;
              });

    QVector<ConflictSegment> merged;
    for (const auto &segment : segments)
    {
        if (merged.isEmpty() || segment.startNs > merged.back().endNs)
        {
            merged.push_back(segment);
            continue;
        }
        merged.back().endNs = std::max(merged.back().endNs, segment.endNs);
    }
    return merged;
}

static CollisionSummary
computeCollisionSummary(const QVector<PpduVisualItem> &items, int targetIndex)
{
    if (targetIndex < 0 || targetIndex >= items.size())
    {
        return {};
    }

    const auto &target = items[targetIndex];
    if (!isDrawablePpdu(target) || target.txStartNs >= target.txEndNs)
    {
        return {};
    }

    if (target.collision || target.rxState == RxState::Collision)
    {
        const bool hasExplicitTime =
            (target.collisionTimeNs >= target.txStartNs && target.collisionTimeNs < target.txEndNs);
        return {true, hasExplicitTime ? target.collisionTimeNs : target.txStartNs};
    }

    uint64_t firstOverlapNs = 0;
    bool foundOverlap = false;
    for (int i = 0; i < items.size(); ++i)
    {
        if (i == targetIndex)
        {
            continue;
        }

        const auto &other = items[i];
        if (!isDrawablePpdu(other) || other.channel_number != target.channel_number)
        {
            continue;
        }

        const uint64_t overlapStart = std::max(target.txStartNs, other.txStartNs);
        const uint64_t overlapEnd = std::min(target.txEndNs, other.txEndNs);
        if (overlapStart >= overlapEnd)
        {
            continue;
        }

        if (!foundOverlap || overlapStart < firstOverlapNs)
        {
            firstOverlapNs = overlapStart;
            foundOverlap = true;
        }
    }

    return {foundOverlap, foundOverlap ? firstOverlapNs : 0};
}

enum class ChannelStateKind
{
    Idle,
    Busy,
    Collision
};

struct ChannelStateSegment
{
    uint64_t startNs = 0;
    uint64_t endNs = 0;
    ChannelStateKind state = ChannelStateKind::Idle;
    int activeCount = 0;
};

struct MloChannelRow
{
    uint64_t rowKey = 0;
    uint16_t nodeId = 0;
    uint8_t linkId = 0;
    int channel = 0;
    DeviceRole role = DeviceRole::Unknown;
    QString label;
};

static inline QString channelBand(int ch)
{
    return (ch >= 1 && ch <= 14) ? "2.4G" : "5G";
}

static inline ChannelStateKind stateFromActiveCount(int activeCount)
{
    if (activeCount <= 0)
        return ChannelStateKind::Idle;
    if (activeCount == 1)
        return ChannelStateKind::Busy;
    return ChannelStateKind::Collision;
}

static inline QString channelStateName(ChannelStateKind state)
{
    switch (state)
    {
    case ChannelStateKind::Idle:
        return "IDLE";
    case ChannelStateKind::Busy:
        return "BUSY";
    case ChannelStateKind::Collision:
        return "COLLISION";
    }

    return "UNKNOWN";
}

static inline QColor channelStateColor(ChannelStateKind state)
{
    switch (state)
    {
    case ChannelStateKind::Idle:
        return QColor(150, 164, 180, 150);
    case ChannelStateKind::Busy:
        return QColor(62, 140, 94, 210);
    case ChannelStateKind::Collision:
        return QColor(220, 88, 64, 220);
    }

    return QColor(120, 120, 120, 180);
}

enum class PhyStateKindView
{
    Idle,
    CcaBusy,
    Tx,
    Rx,
    Other
};

struct PhyStateSegment
{
    uint64_t rowKey = 0;
    uint16_t nodeId = 0;
    uint8_t channel = 0;
    uint8_t linkId = 0;
    DeviceRole role = DeviceRole::Unknown;
    uint64_t startNs = 0;
    uint64_t endNs = 0;
    PhyStateKindView state = PhyStateKindView::Idle;
};

static inline uint64_t nodeLinkRowKey(uint16_t nodeId, uint8_t linkId)
{
    return (static_cast<uint64_t>(nodeId) << 32) | linkId;
}

static inline uint64_t nodeLinkChannelRowKey(uint16_t nodeId, uint8_t linkId, uint8_t channel)
{
    return (static_cast<uint64_t>(nodeId) << 40) |
           (static_cast<uint64_t>(linkId) << 32) |
           static_cast<uint64_t>(channel);
}

static inline PhyStateKindView toPhyStateKind(PhyStateKind state)
{
    switch (state)
    {
    case PhyStateKind::Idle:
        return PhyStateKindView::Idle;
    case PhyStateKind::CcaBusy:
        return PhyStateKindView::CcaBusy;
    case PhyStateKind::Tx:
        return PhyStateKindView::Tx;
    case PhyStateKind::Rx:
        return PhyStateKindView::Rx;
    default:
        return PhyStateKindView::Other;
    }
}

static inline QString phyStateName(PhyStateKindView state)
{
    switch (state)
    {
    case PhyStateKindView::Idle:
        return "IDLE";
    case PhyStateKindView::CcaBusy:
        return "CCA_BUSY";
    case PhyStateKindView::Tx:
        return "TX";
    case PhyStateKindView::Rx:
        return "RX";
    case PhyStateKindView::Other:
        return "OTHER";
    }

    return "UNKNOWN";
}

static inline QColor phyStateColor(PhyStateKindView state)
{
    switch (state)
    {
    case PhyStateKindView::Idle:
        return QColor(150, 164, 180, 150);
    case PhyStateKindView::CcaBusy:
        return QColor(223, 170, 58, 220);
    case PhyStateKindView::Tx:
        return QColor(62, 140, 94, 220);
    case PhyStateKindView::Rx:
        return QColor(76, 114, 176, 220);
    case PhyStateKindView::Other:
        return QColor(120, 120, 120, 180);
    }

    return QColor(120, 120, 120, 180);
}

static inline bool isDrawablePpdu(const PpduVisualItem &item)
{
    return item.recordType == RecordType::Ppdu && item.txEndNs > item.txStartNs;
}

static inline bool isDrawablePhyState(const PpduVisualItem &item)
{
    return item.recordType == RecordType::PhyState &&
           item.phyStateDurationNs > 0 &&
           item.phyStateEndNs > item.phyStateStartNs;
}

static QVector<int> collectChannels(const QVector<PpduVisualItem> &items)
{
    QMap<int, bool> channelMap;
    for (const auto &item : items)
    {
        if (!isDrawablePpdu(item))
            continue;
        channelMap[item.channel_number] = true;
    }

    QVector<int> channels;
    channels.reserve(channelMap.size());
    for (auto it = channelMap.cbegin(); it != channelMap.cend(); ++it)
        channels.push_back(it.key());

    return channels;
}

static QVector<MloChannelRow>
collectMloChannelRows(const QVector<PpduVisualItem> &ppduItems,
                      const QVector<PpduVisualItem> &phyStateItems,
                      const QHash<uint16_t, uint64_t> &nodeToMacMap,
                      const QHash<uint64_t, DeviceNodeInfo> &deviceInfoMap)
{
    QMap<uint64_t, MloChannelRow> rowMap;

    auto roleForNode = [&](uint16_t nodeId, DeviceRole fallback) {
        if (fallback != DeviceRole::Unknown)
            return fallback;
        const uint64_t mac = nodeToMacMap.value(nodeId, 0);
        if (mac != 0 && deviceInfoMap.contains(mac))
            return deviceInfoMap.value(mac).role;
        return DeviceRole::Unknown;
    };

    auto addRow = [&](uint16_t nodeId, uint8_t linkId, uint8_t channel, DeviceRole role) {
        if (channel == 0)
            return;
        const uint64_t key = nodeLinkChannelRowKey(nodeId, linkId, channel);
        if (rowMap.contains(key))
            return;
        const DeviceRole resolvedRole = roleForNode(nodeId, role);
        rowMap.insert(key,
                      MloChannelRow{key,
                                    nodeId,
                                    linkId,
                                    channel,
                                    resolvedRole,
                                    QString("%1 %2 / L%3 / CH%4")
                                        .arg(deviceRoleName(resolvedRole))
                                        .arg(nodeId)
                                        .arg(linkId)
                                        .arg(channel)});
    };

    for (const auto &item : ppduItems)
    {
        if (isDrawablePpdu(item))
            addRow(item.nodeId, item.linkId, item.channel_number, item.deviceRole);
    }

    for (const auto &item : phyStateItems)
    {
        if (item.recordType == RecordType::PhyState && item.phyStateDurationNs > 0)
            addRow(item.nodeId, item.linkId, item.channel_number, item.deviceRole);
    }

    QVector<MloChannelRow> rows;
    rows.reserve(rowMap.size());
    for (auto it = rowMap.cbegin(); it != rowMap.cend(); ++it)
        rows.push_back(it.value());
    return rows;
}

static QVector<ChannelStateSegment> buildChannelStateSegments(
    const QVector<PpduVisualItem> &items,
    int channel,
    uint64_t globalStartNs,
    uint64_t globalEndNs)
{
    struct Event
    {
        uint64_t time = 0;
        int delta = 0;
    };

    QVector<Event> events;
    for (const auto &item : items)
    {
        if (!isDrawablePpdu(item))
            continue;
        if (item.channel_number != channel)
            continue;

        uint64_t startNs = std::max(item.txStartNs, globalStartNs);
        uint64_t endNs = std::min(item.txEndNs, globalEndNs);
        if (startNs >= endNs)
            continue;

        events.push_back({startNs, +1});
        events.push_back({endNs, -1});
    }

    std::sort(events.begin(), events.end(),
              [](const Event &a, const Event &b) {
                  if (a.time != b.time)
                      return a.time < b.time;
                  return a.delta < b.delta;
              });

    QVector<ChannelStateSegment> segments;
    int activeCount = 0;
    uint64_t cursorNs = globalStartNs;

    for (int i = 0; i < events.size();)
    {
        const uint64_t eventTime = events[i].time;

        if (cursorNs < eventTime)
        {
            segments.push_back({
                cursorNs,
                eventTime,
                stateFromActiveCount(activeCount),
                activeCount});
        }

        int deltaSum = 0;
        while (i < events.size() && events[i].time == eventTime)
        {
            deltaSum += events[i].delta;
            ++i;
        }

        activeCount = std::max(0, activeCount + deltaSum);
        cursorNs = eventTime;
    }

    if (cursorNs < globalEndNs)
    {
        segments.push_back({
            cursorNs,
            globalEndNs,
            stateFromActiveCount(activeCount),
            activeCount});
    }

    if (segments.isEmpty() && globalStartNs < globalEndNs)
    {
        segments.push_back({
            globalStartNs,
            globalEndNs,
            ChannelStateKind::Idle,
            0});
    }

    return segments;
}

/* ======================== Constructor ======================== */

PpduTimelineView::PpduTimelineView(QWidget *parent)
    : QWidget(parent),
      m_rowMode(TimelineRowMode::BySender),
      m_viewMode(TimelineViewMode::PpduTimeline)
{
    setMouseTracking(true);

    m_overlay = new PpduInfoOverlay(this);
    m_overlay->close();

    m_btnSave = new QPushButton("Save", this);
    m_btnSave->setFixedSize(68, 32);
    m_btnSave->move(8, 8);
    applyToolbarButtonStyle(m_btnSave);
    connect(m_btnSave, &QPushButton::clicked,
            this, &PpduTimelineView::onSaveImage);

    m_btnSetTimeRange = new QPushButton("Time Span", this);
    m_btnSetTimeRange->setFixedSize(96, 32);
    m_btnSetTimeRange->move(84, 8);
    applyToolbarButtonStyle(m_btnSetTimeRange);
    connect(m_btnSetTimeRange, &QPushButton::clicked,
            this, &PpduTimelineView::onSetTimeRange);

    // Channel View Button
    m_btnChannel = new QPushButton("Mode", this);
    m_btnChannel->setFixedSize(76, 32);
    m_btnChannel->move(188, 8);
    applyToolbarButtonStyle(m_btnChannel);
    m_btnChannel->setToolTip("Switch to channel state view");
    connect(m_btnChannel, &QPushButton::clicked,
            this, &PpduTimelineView::onToggleChannelView);

    quitButton = new QPushButton("Quit", this);

    quitButton->setFixedSize(76, 32);
    quitButton->move(272, 8);
    applyToolbarButtonStyle(quitButton);

    connect(quitButton, &QPushButton::clicked,
            this, &PpduTimelineView::quit_simulation);

    m_btnLegend = new QPushButton("Legend", this);
    m_btnLegend->setFixedSize(82, 32);
    applyToolbarButtonStyle(m_btnLegend);
    connect(m_btnLegend, &QPushButton::clicked,
            this, &PpduTimelineView::onToggleLegend);

    m_legendOverlay = new LegendOverlay(this);
    centerWindow(this);
    m_legendOverlay->close();
    updateModeButton();
}

PpduTimelineView::~PpduTimelineView()
{
    // Clean up UI components
    delete m_legendOverlay;
    delete m_overlay;
    delete m_btnSave;
    delete m_btnLegend;
    delete m_btnSetTimeRange;

    // Clear data
    m_ppduItems.clear();
    m_phyStateItems.clear();
}

void PpduTimelineView::enableQuitButton(bool enable)
{
    if (quitButton)
    {
        quitButton->setVisible(enable);
        quitButton->setEnabled(enable);
    }
}

void PpduTimelineView::setDetailWindow(PpduDetailWindow *detailWindow)
{
    m_detailWindow = detailWindow;
    if (m_detailWindow)
    {
        m_detailWindow->clearDetails();
        m_detailWindow->show();
    }
}

/* ======================== Row Key ======================== */
// ★ NEW
uint64_t PpduTimelineView::rowKey(const PpduVisualItem &ppdu) const
{
	    if (m_rowMode == TimelineRowMode::ByReceiver)
	        return ppdu.receiver;
	    if (m_rowMode == TimelineRowMode::ByChannel)
	        return static_cast<uint64_t>(ppdu.channel_number);
	    if (m_rowMode == TimelineRowMode::ByNodeLink)
	        return nodeLinkRowKey(ppdu.nodeId, ppdu.linkId);

    return ppdu.sender;
}

DeviceRole PpduTimelineView::deviceRole(uint64_t mac) const
{
    return m_deviceInfoMap.value(mac, DeviceNodeInfo{}).role;
}

DeviceRole PpduTimelineView::deviceRoleForNode(uint16_t nodeId) const
{
    const uint64_t mac = m_nodeToMacMap.value(nodeId, 0);
    if (mac != 0 && m_deviceInfoMap.contains(mac))
        return m_deviceInfoMap.value(mac).role;
    return DeviceRole::Unknown;
}

QString PpduTimelineView::nodeRoleLabel(uint16_t nodeId) const
{
    DeviceRole role = deviceRoleForNode(nodeId);
    if (role == DeviceRole::Unknown)
    {
        for (const auto &item : m_ppduItems)
        {
            if (item.nodeId == nodeId && item.deviceRole != DeviceRole::Unknown)
            {
                role = item.deviceRole;
                break;
            }
        }
    }
    if (role == DeviceRole::Unknown)
    {
        for (const auto &item : m_phyStateItems)
        {
            if (item.nodeId == nodeId && item.deviceRole != DeviceRole::Unknown)
            {
                role = item.deviceRole;
                break;
            }
        }
    }
    return QString("%1 %2").arg(deviceRoleName(role)).arg(nodeId);
}

uint16_t PpduTimelineView::nodeIdForMac(uint64_t mac) const
{
    return m_deviceInfoMap.value(mac, DeviceNodeInfo{}).nodeId;
}

QString PpduTimelineView::roleLabel(uint64_t mac) const
{
    if (isDisplayedAsBroadcast(mac))
        return "Broadcast";

    int apIndex = 0;
    int staIndex = 0;
    int devIndex = 0;
    for (const auto &row : m_cachedPpduRows)
    {
        if (isDisplayedAsBroadcast(row.rowKey))
            continue;

        const DeviceRole role = deviceRole(row.rowKey);
        if (role == DeviceRole::Ap)
        {
            ++apIndex;
            if (row.rowKey == mac)
                return QString("AP_%1").arg(apIndex);
        }
        else if (role == DeviceRole::Sta)
        {
            ++staIndex;
            if (row.rowKey == mac)
                return QString("STA_%1").arg(staIndex);
        }
        else
        {
            ++devIndex;
            if (row.rowKey == mac)
                return QString("DEV_%1").arg(devIndex);
        }
    }

    const uint16_t mappedNodeId = nodeIdForMac(mac);
    if (m_deviceInfoMap.contains(mac))
    {
        return QString("%1_%2").arg(deviceRoleName(deviceRole(mac))).arg(mappedNodeId);
    }

    return QString("%1_?").arg(deviceRoleName(deviceRole(mac)));
}

QString PpduTimelineView::roleTooltip(uint64_t mac) const
{
    if (isDisplayedAsBroadcast(mac))
        return "Broadcast";

    return QString("%1\n%2")
        .arg(roleLabel(mac))
        .arg(formatMac(mac));
}

bool PpduTimelineView::showRowLabelTooltip(const QPoint &pos)
{
    if (m_viewMode != TimelineViewMode::PpduTimeline &&
        m_viewMode != TimelineViewMode::MloChannelTimeline &&
        m_viewMode != TimelineViewMode::RxTimeline)
    {
        return false;
    }

    ensurePpduLayoutCache();
    const int rowCount = m_cachedPpduRows.size();
    if (rowCount == 0)
        return false;

    const int availH = height() - m_topMargin - kBottomMargin;
    const int rowH = std::clamp(availH / rowCount, 18, 80);
    int topY = m_topMargin;
    if (availH > rowCount * rowH)
        topY += (availH - rowCount * rowH) / 2;

    for (int rowIndex = 0; rowIndex < m_cachedPpduRows.size(); ++rowIndex)
    {
        const auto &rowInfo = m_cachedPpduRows[rowIndex];
        const QRect labelRect(0, topY + rowIndex * rowH, m_leftMargin - 10, rowH);
        if (!labelRect.contains(pos))
            continue;

        QString text;
        if (m_rowMode == TimelineRowMode::ByChannel)
        {
            const int ch = static_cast<int>(rowInfo.rowKey);
            text = QString("Channel %1 (%2)").arg(ch).arg(channelBand(ch));
        }
        else if (m_rowMode == TimelineRowMode::ByNodeLink)
        {
            const int idx = rowInfo.itemIndices.isEmpty() ? -1 : rowInfo.itemIndices.first();
            if (idx >= 0 && idx < m_ppduItems.size())
            {
                const auto &item = m_ppduItems[idx];
                text = QString("%1\nLink %2\nChannel %3 (%4)")
                           .arg(nodeRoleLabel(item.nodeId))
                           .arg(item.linkId)
                           .arg(item.channel_number)
                           .arg(channelBand(item.channel_number));
            }
        }
        else
        {
            text = roleTooltip(rowInfo.rowKey);
        }

        if (text.isEmpty())
            return false;

        const int y = topY + rowIndex * rowH + rowH / 2;
        QToolTip::showText(mapToGlobal(QPoint(8, y)), text, this, labelRect);
        return true;
    }

    return false;
}

void PpduTimelineView::hideHoverUi()
{
    m_hoverIndex = -1;
    if (m_overlay)
        m_overlay->close();
    QToolTip::hideText();
}

QColor PpduTimelineView::senderColor(uint64_t mac) const
{
    const uint hue = qHash(static_cast<quint64>(mac)) % 360;
    return QColor::fromHsv(hue, 150, 205);
}

void PpduTimelineView::updateDetailWindow(int idx)
{
    if (!m_detailWindow || idx < 0 || idx >= m_ppduItems.size())
        return;

    const auto &it = m_ppduItems[idx];
    const CollisionSummary collisionSummary = computeCollisionSummary(m_ppduItems, idx);
    const QString senderTitle = QString("Sender • %1").arg(roleLabel(it.sender));
    const QString receiverTitle = QString("Receiver • %1").arg(roleLabel(it.receiver));
    const QString collisionText =
        collisionSummary.hasCollision
            ? QString("Yes • %1 ms")
                  .arg(collisionSummary.collisionTimeNs /
                           1e6,
                       0,
                       'f',
                       3)
            : "No";

    const QString throughputText =
        it.throughputMbpsX100 > 0
            ? QString("%1 Mbps").arg(it.throughputMbpsX100 / 100.0, 0, 'f', 2)
            : "0.00 Mbps";
    const PhyStateKind detailPhyState =
        (it.phyState == PhyStateKind::Unknown) ? PhyStateKind::Tx : it.phyState;
    const QString snrText =
        it.snrDbX10 > 0
            ? QString("%1 dB").arg(it.snrDbX10 / 10.0, 0, 'f', 1)
            : "Unavailable";
    const QString mcsText = it.mcs >= 0 ? QString::number(it.mcs) : "Legacy";

    m_detailWindow->showDetails(
        senderTitle,
        isDisplayedAsBroadcast(it.sender) ? "Broadcast" : formatMac(it.sender),
        receiverTitle,
        isDisplayedAsBroadcast(it.receiver) ? "Broadcast" : formatMac(it.receiver),
        QString::fromStdString(it.frameType),
        phyStateBadgeName(detailPhyState),
        phyStateBadgeAccent(detailPhyState).name(),
        QString("CH %1 (%2)").arg(it.channel_number).arg(channelBand(it.channel_number)),
        QString::number(it.nodeId),
        QString("%1 ms").arg(it.txStartNs / 1e6, 0, 'f', 3),
        QString("%1 ms").arg(it.txEndNs / 1e6, 0, 'f', 3),
        QString("%1 us").arg((it.txEndNs - it.txStartNs) / 1e3, 0, 'f', 1),
        QString("%1 B").arg(it.size),
        QString::number(it.mpduAggregation),
        mcsText,
        throughputText,
        snrText,
        collisionText);

}

/* ======================== Channel View Toggle ======================== */
// ★ NEW
void PpduTimelineView::onToggleChannelView()
{
    switch (m_viewMode)
    {
	    case TimelineViewMode::PpduTimeline:
	        m_viewMode = TimelineViewMode::MloChannelTimeline;
	        m_rowMode = TimelineRowMode::ByNodeLink;
	        break;
	    case TimelineViewMode::MloChannelTimeline:
	        m_viewMode = TimelineViewMode::MloChannelState;
	        m_rowMode = TimelineRowMode::ByNodeLink;
	        break;
	    case TimelineViewMode::MloChannelState:
	        m_viewMode = TimelineViewMode::ChannelState;
	        m_rowMode = TimelineRowMode::ByChannel;
	        break;
    case TimelineViewMode::ChannelState:
	        m_viewMode = TimelineViewMode::RxTimeline;
        m_rowMode = TimelineRowMode::ByReceiver;
        break;
    case TimelineViewMode::RxTimeline:
	        m_viewMode = TimelineViewMode::PhyStateTimeline;
        m_rowMode = TimelineRowMode::ByNodeLink;
        break;
    case TimelineViewMode::PhyStateTimeline:
        m_viewMode = TimelineViewMode::PpduTimeline;
        m_rowMode = TimelineRowMode::BySender;
        break;
    }

    updateModeButton();
    m_hoverIndex = -1;
    m_showingStats = false;
    m_overlay->close();
    QToolTip::hideText();
    markPpduLayoutDirty();
    update();
}

void PpduTimelineView::updateModeButton()
{
    switch (m_viewMode)
    {
	    case TimelineViewMode::PpduTimeline:
	        m_btnChannel->setText("MLO");
	        m_btnChannel->setToolTip("Switch to MLO channel timeline");
	        break;
	    case TimelineViewMode::MloChannelTimeline:
	        m_btnChannel->setText("MLO CH");
	        m_btnChannel->setToolTip("Switch to MLO channel state view");
	        break;
	    case TimelineViewMode::MloChannelState:
	        m_btnChannel->setText("CH");
	        m_btnChannel->setToolTip("Switch to aggregate channel state view");
	        break;
    case TimelineViewMode::ChannelState:
        m_btnChannel->setText("RX");
        m_btnChannel->setToolTip("Switch to RX result timeline");
        break;
    case TimelineViewMode::RxTimeline:
        m_btnChannel->setText("PHY");
        m_btnChannel->setToolTip("Switch to PHY state view");
        break;
    case TimelineViewMode::PhyStateTimeline:
        m_btnChannel->setText("PPDU");
        m_btnChannel->setToolTip("Back to PPDU timeline");
        break;
    }
}

std::pair<uint64_t, uint64_t> PpduTimelineView::currentTimeBounds() const
{
    const QVector<PpduVisualItem> *items = nullptr;

    if (m_viewMode == TimelineViewMode::PhyStateTimeline)
    {
        ensurePhyStateCache();
        items = &m_phyStateItems;
    }
	    else
	        items = &m_ppduItems;

    if (items->isEmpty())
        return {0, 0};

    if (m_viewMode == TimelineViewMode::PhyStateTimeline && m_phyStateEndNs > m_phyStateStartNs)
        return {m_phyStateStartNs, m_phyStateEndNs};

    uint64_t startNs = std::numeric_limits<uint64_t>::max();
    uint64_t endNs = 0;

    for (const auto &item : *items)
    {
        const uint64_t itemStart =
            item.recordType == RecordType::PhyState ? item.phyStateStartNs : item.txStartNs;
        const uint64_t itemEnd =
            item.recordType == RecordType::PhyState ? item.phyStateEndNs : item.txEndNs;
        if (itemEnd <= itemStart)
            continue;
        startNs = std::min(startNs, itemStart);
        endNs = std::max(endNs, itemEnd);
    }

    return {startNs, endNs};
}

int
PpduTimelineView::usableTimelineWidth() const
{
    return std::max(1, width() - m_leftMargin - 10);
}

int64_t
PpduTimelineView::clampViewStartNs(double requestedStartNs, double nsToPixel) const
{
    const auto [dataStartNs, dataEndNs] = currentTimeBounds();
    if (dataEndNs <= dataStartNs)
    {
        return std::max<int64_t>(0, static_cast<int64_t>(std::llround(requestedStartNs)));
    }

    const double visibleNs = double(usableTimelineWidth()) / std::max(nsToPixel, 1e-12);
    const double minStart = double(dataStartNs);
    const double maxStart = std::max(minStart, double(dataEndNs) - visibleNs);
    const double clamped = std::clamp(requestedStartNs, minStart, maxStart);
    return std::max<int64_t>(0, static_cast<int64_t>(std::llround(clamped)));
}

void
PpduTimelineView::syncRangeSliderToView()
{
    const auto [dataStartNs, dataEndNs] = currentTimeBounds();
    if (dataEndNs <= dataStartNs)
    {
        m_rangeStart = 0.0;
        m_rangeEnd = 1.0;
        return;
    }

    const double spanNs = double(dataEndNs - dataStartNs);
    const double visibleNs = double(usableTimelineWidth()) / std::max(m_nsToPixel, 1e-12);
    if (visibleNs >= spanNs)
    {
        m_rangeStart = 0.0;
        m_rangeEnd = 1.0;
        return;
    }

    const double startRatio = (double(m_viewStartNs) - double(dataStartNs)) / spanNs;
    const double endRatio = (double(m_viewStartNs) + visibleNs - double(dataStartNs)) / spanNs;
    m_rangeStart = std::clamp(startRatio, 0.0, 1.0);
    m_rangeEnd = std::clamp(endRatio, m_rangeStart, 1.0);
}

void PpduTimelineView::scheduleDataUpdate()
{
    if (m_dataUpdateQueued)
        return;

    m_dataUpdateQueued = true;
    QTimer::singleShot(16, this, [this]() {
        m_dataUpdateQueued = false;
        update();
    });
}

void PpduTimelineView::schedulePhyStateDataUpdate()
{
    if (m_phyStateUpdateQueued)
        return;

    m_phyStateUpdateQueued = true;
    QTimer::singleShot(80, this, [this]() {
        m_phyStateUpdateQueued = false;
        if (m_viewMode == TimelineViewMode::PhyStateTimeline)
            update();
    });
}

void PpduTimelineView::markPpduLayoutDirty()
{
    m_ppduLayoutDirty = true;
}

void PpduTimelineView::markPhyStateCacheDirty()
{
    m_phyStateCacheDirty = true;
}

void PpduTimelineView::ensurePpduLayoutCache() const
{
    if (!m_ppduLayoutDirty)
        return;

    const_cast<PpduTimelineView*>(this)->rebuildPpduLayoutCache();
}

void PpduTimelineView::ensurePhyStateCache() const
{
    if (!m_phyStateCacheDirty)
        return;

    const_cast<PpduTimelineView*>(this)->rebuildPhyStateCache();
}

void PpduTimelineView::rebuildPpduLayoutCache()
{
    m_cachedPpduLayout.clear();
    m_cachedPpduRows.clear();
    m_cachedPpduLayout.resize(m_ppduItems.size());

    if (m_ppduItems.isEmpty())
    {
        m_ppduLayoutDirty = false;
        return;
    }

    QHash<uint64_t, int> rowMap;
    for (int i = 0; i < m_ppduItems.size(); ++i)
    {
        if (!isDrawablePpdu(m_ppduItems[i]))
            continue;

        const uint64_t key = rowKey(m_ppduItems[i]);
        int row = rowMap.value(key, -1);
        if (row < 0)
        {
            row = m_cachedPpduRows.size();
            rowMap.insert(key, row);
            m_cachedPpduRows.push_back(CachedPpduRow{key});
        }

        m_cachedPpduLayout[i].rowKey = key;
        m_cachedPpduLayout[i].row = row;
        m_cachedPpduRows[row].itemIndices.push_back(i);
    }

    for (auto& row : m_cachedPpduRows)
    {
	        if (m_rowMode == TimelineRowMode::ByChannel)
	        {
	            row.label = QString("CH %1").arg(row.rowKey);
	        }
	        else if (m_rowMode == TimelineRowMode::ByNodeLink)
	        {
	            const int idx = row.itemIndices.isEmpty() ? -1 : row.itemIndices.first();
	            if (idx >= 0 && idx < m_ppduItems.size())
	            {
	                const auto& item = m_ppduItems[idx];
	                row.label = QString("%1 / L%2 / CH%3")
	                                .arg(nodeRoleLabel(item.nodeId))
	                                .arg(item.linkId)
	                                .arg(item.channel_number);
	            }
	            else
	            {
	                row.label = QString("MLO Link");
	            }
	        }
	        else if (m_rowMode == TimelineRowMode::ByReceiver)
	        {
	            row.label = QString("RX %1").arg(roleLabel(row.rowKey));
	        }
	        else
	        {
	            row.label = roleLabel(row.rowKey);
	            const int idx = row.itemIndices.isEmpty() ? -1 : row.itemIndices.first();
	            if (row.label.startsWith("DEV") && idx >= 0 && idx < m_ppduItems.size())
	            {
	                const auto& item = m_ppduItems[idx];
	                const DeviceRole rowRole = item.deviceRole != DeviceRole::Unknown
	                                               ? item.deviceRole
	                                               : deviceRoleForNode(item.nodeId);
	                row.label = QString("%1 %2")
	                                .arg(deviceRoleName(rowRole))
	                                .arg(item.nodeId);
	            }
	        }

        QVector<int> sorted = row.itemIndices;
        std::sort(sorted.begin(), sorted.end(), [this](int a, int b) {
            if (m_ppduItems[a].txStartNs != m_ppduItems[b].txStartNs)
                return m_ppduItems[a].txStartNs < m_ppduItems[b].txStartNs;
            return m_ppduItems[a].txEndNs < m_ppduItems[b].txEndNs;
        });

        QVector<uint64_t> laneEndNs;
        QVector<int> overlapCluster;
        uint64_t clusterMaxEnd = 0;
        bool clusterActive = false;

        for (int idx : sorted)
        {
            const auto& ppdu = m_ppduItems[idx];

            int lane = -1;
            for (int l = 0; l < laneEndNs.size(); ++l)
            {
                if (ppdu.txStartNs >= laneEndNs[l])
                {
                    lane = l;
                    break;
                }
            }
            if (lane < 0)
            {
                lane = laneEndNs.size();
                laneEndNs.push_back(0);
            }
            laneEndNs[lane] = ppdu.txEndNs;
            m_cachedPpduLayout[idx].lane = lane;

            if (!clusterActive || ppdu.txStartNs >= clusterMaxEnd)
            {
                overlapCluster.clear();
                overlapCluster.push_back(idx);
                clusterMaxEnd = ppdu.txEndNs;
                clusterActive = true;
            }
            else
            {
                m_cachedPpduLayout[idx].overlap = true;
                for (int clusterIdx : overlapCluster)
                    m_cachedPpduLayout[clusterIdx].overlap = true;
                overlapCluster.push_back(idx);
                clusterMaxEnd = std::max(clusterMaxEnd, ppdu.txEndNs);
            }
        }

        row.laneCount = std::max(1, static_cast<int>(laneEndNs.size()));
    }

    m_ppduLayoutDirty = false;
}

void PpduTimelineView::rebuildPhyStateCache()
{
    m_cachedPhyStateRows.clear();
    m_phyStateStartNs = 0;
    m_phyStateEndNs = 0;

    if (m_phyStateItems.isEmpty())
    {
        m_phyStateCacheDirty = false;
        return;
    }

    QMap<uint64_t, int> rowMap;
    uint64_t globalStartNs = std::numeric_limits<uint64_t>::max();
    uint64_t globalEndNs = 0;

    for (int i = 0; i < m_phyStateItems.size(); ++i)
    {
        const auto &item = m_phyStateItems[i];
        if (!isDrawablePhyState(item))
            continue;

        const uint64_t key = nodeLinkRowKey(item.nodeId, item.linkId);
        int row = rowMap.value(key, -1);
        if (row < 0)
        {
            row = m_cachedPhyStateRows.size();
            rowMap.insert(key, row);
            m_cachedPhyStateRows.push_back(
                CachedPhyStateRow{key,
                                  QString("%1 / L%2 / CH%3")
                                      .arg(nodeRoleLabel(item.nodeId))
                                      .arg(item.linkId)
                                      .arg(item.channel_number),
                                  item.nodeId,
                                  item.linkId,
                                  item.channel_number,
                                  {}});
        }

        m_cachedPhyStateRows[row].itemIndices.push_back(i);
        globalStartNs = std::min(globalStartNs, item.phyStateStartNs);
        globalEndNs = std::max(globalEndNs, item.phyStateEndNs);
    }

    for (auto &row : m_cachedPhyStateRows)
    {
        std::sort(row.itemIndices.begin(), row.itemIndices.end(), [this](int a, int b) {
            const auto &lhs = m_phyStateItems[a];
            const auto &rhs = m_phyStateItems[b];
            if (lhs.phyStateStartNs != rhs.phyStateStartNs)
                return lhs.phyStateStartNs < rhs.phyStateStartNs;
            return lhs.phyStateEndNs < rhs.phyStateEndNs;
        });
    }

    if (globalStartNs != std::numeric_limits<uint64_t>::max() && globalStartNs < globalEndNs)
    {
        m_phyStateStartNs = globalStartNs;
        m_phyStateEndNs = globalEndNs;
    }

    m_phyStateCacheDirty = false;
}

/* ======================== Culculate the number of APs =================== */

int PpduTimelineView::apCount() const
{
    return this->Num_ap;
}

/* ======================== Timeline Top ======================== */

int PpduTimelineView::effectiveRowHeight() const
{
    int avail = height() - m_topMargin - kBottomMargin;
    return std::clamp(avail / apCount(), 18, 80);
}

int PpduTimelineView::timelineTopY() const
{
    int total = apCount() * m_rowHeight;
    int avail = height() - m_topMargin - kBottomMargin;

    int y = m_topMargin;
    if (avail > total)
        y += (avail - total) / 2;
    return y;
}

/* ======================== Data ======================== */

void PpduTimelineView::append(const PpduVisualItem &ppdu)
{
    if (ppdu.recordType == RecordType::DeviceRole)
	    {
        if (ppdu.roleMac != 0)
        {
            m_deviceInfoMap.insert(ppdu.roleMac, DeviceNodeInfo{ppdu.nodeId, ppdu.deviceRole});
            m_nodeToMacMap.insert(ppdu.nodeId, ppdu.roleMac);
        }
        else if (ppdu.deviceRole != DeviceRole::Unknown)
        {
            m_nodeToMacMap.insert(ppdu.nodeId, 0);
        }
        markPpduLayoutDirty();
        markPhyStateCacheDirty();
        scheduleDataUpdate();
        return;
    }

    if (ppdu.recordType == RecordType::PhyState)
    {
        PpduVisualItem fixed = ppdu;
        m_phyStateItems.append(fixed);
        markPhyStateCacheDirty();
        if (m_ppduItems.isEmpty() && m_phyStateItems.size() == 1)
            m_viewStartNs = fixed.phyStateStartNs;
        schedulePhyStateDataUpdate();
        return;
    }
	    if (ppdu.recordType == RecordType::PpduUpdate)
    {
        for (int i = m_ppduItems.size() - 1; i >= 0; --i)
        {
            if (m_ppduItems[i].id == ppdu.id)
            {
                PpduVisualItem merged = ppdu;
                merged.recordType = RecordType::Ppdu;
                m_ppduItems[i] = merged;
                markPpduLayoutDirty();
                scheduleDataUpdate();
                return;
            }
        }
        return;
    }

	    if (ppdu.recordType != RecordType::Ppdu)
	        return;

    static uint64_t ppduCount = 0;
    ppduCount++;

    PpduVisualItem fixed = ppdu;
    if (fixed.txEndNs <= fixed.txStartNs)
        return;
    if (fixed.durationNs == 0)
        fixed.durationNs = fixed.txEndNs - fixed.txStartNs;

    const quint64 dedupeKeyA = (static_cast<quint64>(fixed.id) << 32) |
                               static_cast<quint64>(fixed.nodeId);
    const quint64 dedupeKeyB = fixed.txStartNs ^ (fixed.txEndNs << 1);
    const quint64 dedupeKey = dedupeKeyA ^ dedupeKeyB;
    if (m_seenPpduKeys.contains(dedupeKey))
        return;
    m_seenPpduKeys.insert(dedupeKey);

    m_ppduItems.append(fixed);
    markPpduLayoutDirty();

    if (m_ppduItems.size() == 1)
        m_viewStartNs = fixed.txStartNs;

    // only append the ppdu belongs to the current view

    // if (width() > 0)
    // {
    //     double visibleNs = width() / m_nsToPixel;
    //     if (fixed.txEndNs > m_viewStartNs + visibleNs)
    //         m_viewStartNs = fixed.txEndNs - visibleNs;
    // }

    scheduleDataUpdate();
}

void PpduTimelineView::clear()
{
    m_ppduItems.clear();
    m_phyStateItems.clear();
    m_seenPpduKeys.clear();
    m_cachedPpduLayout.clear();
    m_cachedPpduRows.clear();
    m_cachedPhyStateRows.clear();
    m_ppduLayoutDirty = true;
    m_phyStateCacheDirty = true;
    m_phyStateStartNs = 0;
    m_phyStateEndNs = 0;
    m_dataUpdateQueued = false;
    m_phyStateUpdateQueued = false;
    hideHoverUi();
    m_selectedIndex = -1;
    m_pressedIndex = -1;
    m_deviceInfoMap.clear();
    m_nodeToMacMap.clear();
    if (m_detailWindow)
    {
        m_detailWindow->clearDetails();
        m_detailWindow->show();
    }
    update();
}

/* ======================== Conflict detection ======================== */

bool PpduTimelineView::hasOverlap(int idx) const
{
    ensurePpduLayoutCache();
    return idx >= 0 && idx < m_cachedPpduLayout.size() && m_cachedPpduLayout[idx].overlap;
}

/* ======================== Paint ======================== */

TimeRangeStats PpduTimelineView::computeStats(uint64_t startNs, uint64_t endNs) const
{
    TimeRangeStats s;
    s.durationNs = endNs - startNs;

    uint64_t busyNs = 0;
    uint64_t totalBytes = 0;

    for (const auto &it : m_ppduItems)
    {
        uint64_t overlapStart = std::max(it.txStartNs, startNs);
        uint64_t overlapEnd = std::min(it.txEndNs, endNs);

        if (overlapStart < overlapEnd)
        {
            uint64_t overlapNs = overlapEnd - overlapStart;
            busyNs += overlapNs;

            double ratio =
                double(overlapNs) /
                double(it.txEndNs - it.txStartNs);

            totalBytes += uint64_t(it.size * ratio);
        }
    }

    s.busyNs = busyNs;
    s.idleNs = s.durationNs > busyNs ? s.durationNs - busyNs : 0;
    s.totalBytes = totalBytes;

    double sec = s.durationNs / 1e9;
    if (sec > 0)
        s.throughputMbps = (totalBytes * 8.0) / sec / 1e6;

    s.utilization = double(busyNs) / double(s.durationNs);
    return s;
}

void PpduTimelineView::showStatsOverlay(
    const TimeRangeStats &s,
    const QPoint &globalPos)
{
    QString text =
        QString(
            "Selected Range\n"
            "Duration: %1 ms\n"
            "Busy:      %2 ms\n"
            "Idle:      %3 ms\n"
            "Util:      %4 %\n"
            "Throughput: %5 Mbps")
            .arg(s.durationNs / 1e6, 0, 'f', 2)
            .arg(s.busyNs / 1e6, 0, 'f', 2)
            .arg(s.idleNs / 1e6, 0, 'f', 2)
            .arg(s.utilization * 100.0, 0, 'f', 1)
            .arg(s.throughputMbps, 0, 'f', 2);

    m_overlay->setText(text);
    m_overlay->showAt(globalPos + QPoint(8, 8));
}

void PpduTimelineView::paintEvent(QPaintEvent *)
{
    QPainter painter(this);
    painter.fillRect(rect(), kBgColor);

    if (m_viewMode == TimelineViewMode::ChannelState)
    {
        paintChannelStateView(painter);
        return;
    }

    if (m_viewMode == TimelineViewMode::MloChannelState)
    {
        paintMloChannelStateView(painter);
        return;
    }

    if (m_viewMode == TimelineViewMode::PhyStateTimeline)
    {
        paintPhyStateTimeline(painter);
        return;
    }

    if (m_viewMode == TimelineViewMode::RxTimeline)
    {
        paintRxTimeline(painter);
        return;
    }

    paintPpduTimeline(painter);
}

void PpduTimelineView::paintPpduTimeline(QPainter &painter)
{
    ensurePpduLayoutCache();

    const int rowCount = m_cachedPpduRows.size();
    if (rowCount == 0)
        return;

    const int availH = height() - m_topMargin - kBottomMargin;
    const int rowH = std::clamp(availH / rowCount, 18, 80);
    int topY = m_topMargin;
    if (availH > rowCount * rowH)
        topY += (availH - rowCount * rowH) / 2;
    const int rowsBottomY = topY + rowCount * rowH;
    const int conflictTopY = rowsBottomY + kConflictAxisGap;
    const int conflictBottomY = conflictTopY + kConflictAxisHeight;

    if (m_hoverIndex >= 0 && m_hoverIndex < m_cachedPpduLayout.size())
    {
        const int row = m_cachedPpduLayout[m_hoverIndex].row;
        if (row >= 0)
        {
            painter.fillRect(
                QRectF(0, topY + row * rowH, width(), rowH),
                QColor(255, 235, 205, 120));
        }
    }

    painter.setPen(kGridLine);
    for (int r = 0; r <= rowCount; ++r)
    {
        const int y = topY + r * rowH;
        painter.drawLine(m_leftMargin, y, width(), y);
    }

    painter.setPen(QColor(40, 48, 58));
    for (int rowIndex = 0; rowIndex < m_cachedPpduRows.size(); ++rowIndex)
    {
        const auto& rowInfo = m_cachedPpduRows[rowIndex];
        const int row = rowIndex;
        const int y = topY + row * rowH + rowH / 2;
        const QRect labelRect(0, y - rowH / 2, m_leftMargin - 10, rowH);

        painter.setBrush(senderColor(rowInfo.rowKey));
        painter.setPen(Qt::NoPen);
        painter.drawEllipse(QRectF(12, y - 5, 10, 10));

        painter.setPen(QColor(25, 32, 40));
        painter.drawText(labelRect.adjusted(28, 0, -4, 0),
                         Qt::AlignVCenter | Qt::AlignLeft,
                         rowInfo.label);

	    }
    painter.setPen(QColor(180, 180, 180));
    painter.drawLine(m_leftMargin - 8, topY, m_leftMargin - 8, conflictBottomY);

    for (int rowIndex = 0; rowIndex < m_cachedPpduRows.size(); ++rowIndex)
    {
        const auto& rowInfo = m_cachedPpduRows[rowIndex];
        const int row = rowIndex;
        const int laneCount = std::max(1, rowInfo.laneCount);
        const double laneH = (rowH - 2) / double(laneCount);

        for (int idx : rowInfo.itemIndices)
        {
            const auto& ppdu = m_ppduItems[idx];
            const auto& layout = m_cachedPpduLayout[idx];

            QRectF r(
                m_leftMargin + (ppdu.txStartNs - m_viewStartNs) * m_nsToPixel,
                topY + row * rowH + layout.lane * laneH + kLanePadding,
                std::max(1.0, (ppdu.txEndNs - ppdu.txStartNs) * m_nsToPixel),
                laneH - 2 * kLanePadding);

            QColor baseColor = senderColor(ppdu.sender);
            if (idx == m_hoverIndex)
                baseColor = baseColor.lighter(125);

            painter.setPen(Qt::NoPen);
            painter.setBrush(baseColor);
            painter.drawRoundedRect(r, 3, 3);

            painter.setPen(QPen(idx == m_selectedIndex ? QColor(20, 20, 20)
                                                       : QColor(60, 60, 60),
                                idx == m_selectedIndex ? 2 : 1));
            painter.setBrush(Qt::NoBrush);
            painter.drawRoundedRect(r, 3, 3);
        }
    }

    const uint64_t startNs = m_viewStartNs;
    const uint64_t endNs = m_viewStartNs + width() / m_nsToPixel;

    painter.setPen(QColor(90, 90, 90));
    painter.drawText(8, conflictBottomY - 1, "COLL");
    painter.setPen(Qt::NoPen);
    painter.setBrush(QColor(165, 173, 181, 130));
    painter.drawRoundedRect(QRectF(m_leftMargin,
                                   conflictTopY,
                                   std::max(1, width() - m_leftMargin - 5),
                                   kConflictAxisHeight),
                            4,
                            4);

    const QVector<ConflictSegment> conflictSegments = buildConflictSegments(m_ppduItems);
    painter.setBrush(kConflictRed);
    for (const auto &segment : conflictSegments)
    {
        const uint64_t drawStartNs = std::max(segment.startNs, startNs);
        const uint64_t drawEndNs = std::min(segment.endNs, endNs);
        if (drawStartNs >= drawEndNs)
            continue;

        painter.drawRoundedRect(
            QRectF(m_leftMargin + (drawStartNs - startNs) * m_nsToPixel,
                   conflictTopY,
                   std::max(1.0, (drawEndNs - drawStartNs) * m_nsToPixel),
                   kConflictAxisHeight),
            3,
            3);
    }

    QPen gridPen(QColor(180, 180, 180));
    gridPen.setStyle(Qt::DashLine);
    painter.setPen(gridPen);

    for (int i = 0; i <= 10; ++i)
    {
        uint64_t ns = startNs + i * (endNs - startNs) / 10;
        int x = m_leftMargin + (ns - m_viewStartNs) * m_nsToPixel;

        painter.drawLine(x, topY, x, conflictBottomY);
        painter.drawText(x - 20,
                         conflictBottomY + 16,
                         QString::number(ns / 1e6, 'f', 2) + " ms");
    }

    if (m_selecting)
    {
        int x1 = m_selectStart.x();
        int x2 = m_selectEnd.x();

        int left = std::min(x1, x2);
        int right = std::max(x1, x2);

        left = std::clamp(left, m_leftMargin, width() - 5);
        right = std::clamp(right, m_leftMargin, width() - 5);

        if (right > left)
        {
            QRectF selRect(
                left,
                topY,
                right - left,
                rowCount * rowH);

            painter.setPen(QPen(kSelectBorder, 1, Qt::DashLine));
            painter.setBrush(kSelectFill);
            painter.drawRect(selRect);
        }
    }

    /* ================= Time Range Slider ================= */

    int barY = height() - kRangeBarHeight - kRangeBarBottomPadding;
    int barX = m_leftMargin;
    int barW = width() - m_leftMargin - kRangeBarMargin;

    if (barW > 50)
    {
        int leftX = barX + m_rangeStart * barW;
        int rightX = barX + m_rangeEnd * barW;

        // 导轨
        painter.setPen(Qt::NoPen);
        painter.setBrush(QColor(180, 185, 190));
        painter.drawRoundedRect(
            QRectF(barX, barY + kRangeBarHeight / 2 - 2, barW, 4),
            2, 2);

        // 选中窗口
        painter.setBrush(QColor(90, 140, 255, 120));
        painter.drawRoundedRect(
            QRectF(leftX, barY, rightX - leftX, kRangeBarHeight),
            4, 4);

        // 左右手柄
        painter.setBrush(QColor(240, 240, 240));
        painter.drawRoundedRect(
            QRectF(leftX - kRangeHandleW / 2, barY,
                   kRangeHandleW, kRangeBarHeight),
            3, 3);

        painter.drawRoundedRect(
            QRectF(rightX - kRangeHandleW / 2, barY,
                   kRangeHandleW, kRangeBarHeight),
            3, 3);
    }
}

void PpduTimelineView::paintRxTimeline(QPainter &painter)
{
    ensurePpduLayoutCache();

    const int rowCount = m_cachedPpduRows.size();
    if (rowCount == 0)
        return;

    const int availH = height() - m_topMargin - kBottomMargin;
    const int rowH = std::clamp(availH / rowCount, 18, 80);
    int topY = m_topMargin;
    if (availH > rowCount * rowH)
        topY += (availH - rowCount * rowH) / 2;

    painter.setPen(QColor(90, 90, 90));
    painter.drawText(m_leftMargin, 16, "RX Timeline");

    if (m_hoverIndex >= 0 && m_hoverIndex < m_cachedPpduLayout.size())
    {
        const int row = m_cachedPpduLayout[m_hoverIndex].row;
        if (row >= 0)
        {
            painter.fillRect(QRectF(0, topY + row * rowH, width(), rowH),
                             QColor(255, 235, 205, 120));
        }
    }

    painter.setPen(kGridLine);
    for (int r = 0; r <= rowCount; ++r)
    {
        const int y = topY + r * rowH;
        painter.drawLine(m_leftMargin, y, width(), y);
    }

    for (int rowIndex = 0; rowIndex < m_cachedPpduRows.size(); ++rowIndex)
    {
        const auto& rowInfo = m_cachedPpduRows[rowIndex];
        const int y = topY + rowIndex * rowH + rowH / 2;
        const QRect labelRect(0, topY + rowIndex * rowH, m_leftMargin - 10, rowH);

        painter.setBrush(senderColor(rowInfo.rowKey));
        painter.setPen(Qt::NoPen);
        painter.drawEllipse(QRectF(12, y - 5, 10, 10));

        painter.setPen(QColor(25, 32, 40));
        painter.drawText(labelRect.adjusted(28, 0, -4, 0),
                         Qt::AlignVCenter | Qt::AlignLeft,
                         rowInfo.label);

    }

    painter.setPen(QColor(180, 180, 180));
    painter.drawLine(m_leftMargin - 8, topY, m_leftMargin - 8, topY + rowCount * rowH);

    for (int rowIndex = 0; rowIndex < m_cachedPpduRows.size(); ++rowIndex)
    {
        const auto& rowInfo = m_cachedPpduRows[rowIndex];
        const int laneCount = std::max(1, rowInfo.laneCount);
        const double laneH = (rowH - 2) / double(laneCount);

        for (int idx : rowInfo.itemIndices)
        {
            const auto& ppdu = m_ppduItems[idx];
            const auto& layout = m_cachedPpduLayout[idx];
            const QRectF rect(
                m_leftMargin + (ppdu.txStartNs - m_viewStartNs) * m_nsToPixel,
                topY + rowIndex * rowH + layout.lane * laneH + kLanePadding,
                std::max(1.0, (ppdu.txEndNs - ppdu.txStartNs) * m_nsToPixel),
                laneH - 2 * kLanePadding);

            QColor fill = rxStateFill(ppdu.rxState);
            if (idx == m_hoverIndex)
                fill = fill.lighter(120);

            painter.setPen(Qt::NoPen);
            painter.setBrush(fill);
            painter.drawRoundedRect(rect, 3, 3);

            painter.setPen(QPen(idx == m_selectedIndex ? QColor(20, 20, 20)
                                                       : QColor(60, 60, 60),
                                idx == m_selectedIndex ? 2 : 1));
            painter.setBrush(Qt::NoBrush);
            painter.drawRoundedRect(rect, 3, 3);

            if (rect.width() >= 74)
            {
                painter.setPen(ppdu.rxState == RxState::Unknown ? QColor(35, 42, 50) : Qt::white);
                painter.drawText(rect.adjusted(6, 0, -6, 0),
                                 Qt::AlignVCenter | Qt::AlignLeft,
                                 rxStateName(ppdu.rxState));
            }
        }
    }

    const QPen gridPen(QColor(180, 180, 180), 1, Qt::DashLine);
    painter.setPen(gridPen);
    const uint64_t visibleStartNs = std::max<uint64_t>(0, m_viewStartNs);
    const uint64_t visibleEndNs =
        visibleStartNs + std::max<int>(1, width()) / m_nsToPixel;
    for (int i = 0; i <= 10; ++i)
    {
        const uint64_t ns = visibleStartNs + i * (visibleEndNs - visibleStartNs) / 10;
        const int x = m_leftMargin + (ns - visibleStartNs) * m_nsToPixel;
        painter.drawLine(x, topY, x, topY + rowCount * rowH);
        painter.drawText(x - 20,
                         topY + rowCount * rowH + 15,
                         QString::number(ns / 1e6, 'f', 2) + " ms");
    }

    if (m_selecting)
    {
        const int left = std::clamp(std::min(m_selectStart.x(), m_selectEnd.x()),
                                    m_leftMargin,
                                    width() - 5);
        const int right = std::clamp(std::max(m_selectStart.x(), m_selectEnd.x()),
                                     m_leftMargin,
                                     width() - 5);
        if (right > left)
        {
            painter.setPen(QPen(kSelectBorder, 1, Qt::DashLine));
            painter.setBrush(kSelectFill);
            painter.drawRect(QRectF(left, topY, right - left, rowCount * rowH));
        }
    }

    const int barY = height() - kRangeBarHeight - kRangeBarBottomPadding;
    const int barX = m_leftMargin;
    const int barW = width() - m_leftMargin - kRangeBarMargin;
    if (barW > 50)
    {
        const int leftX = barX + m_rangeStart * barW;
        const int rightX = barX + m_rangeEnd * barW;

        painter.setPen(Qt::NoPen);
        painter.setBrush(QColor(180, 185, 190));
        painter.drawRoundedRect(QRectF(barX, barY + kRangeBarHeight / 2 - 2, barW, 4), 2, 2);

        painter.setBrush(QColor(90, 140, 255, 120));
        painter.drawRoundedRect(QRectF(leftX, barY, rightX - leftX, kRangeBarHeight), 4, 4);

        painter.setBrush(QColor(240, 240, 240));
        painter.drawRoundedRect(QRectF(leftX - kRangeHandleW / 2,
                                       barY,
                                       kRangeHandleW,
                                       kRangeBarHeight),
                                3,
                                3);
        painter.drawRoundedRect(QRectF(rightX - kRangeHandleW / 2,
                                       barY,
                                       kRangeHandleW,
                                       kRangeBarHeight),
                                3,
                                3);
    }
}

void PpduTimelineView::paintChannelStateView(QPainter &painter)
{
    if (m_ppduItems.isEmpty())
        return;

    const QVector<int> channels = collectChannels(m_ppduItems);
    if (channels.isEmpty())
        return;

    uint64_t globalStartNs = std::numeric_limits<uint64_t>::max();
    uint64_t globalEndNs = 0;
    for (const auto &item : m_ppduItems)
    {
        if (!isDrawablePpdu(item))
            continue;
        globalStartNs = std::min(globalStartNs, item.txStartNs);
        globalEndNs = std::max(globalEndNs, item.txEndNs);
    }

    if (globalStartNs >= globalEndNs)
        return;

    const int channelCount = channels.size();
    const int availH = height() - m_topMargin - kBottomMargin;
    const int rowH = std::clamp(availH / channelCount, 22, 84);
    int topY = m_topMargin;
    if (availH > channelCount * rowH)
        topY += (availH - channelCount * rowH) / 2;

    painter.setPen(QColor(200, 200, 200));
    for (int r = 0; r <= channelCount; ++r)
    {
        const int y = topY + r * rowH;
        painter.drawLine(m_leftMargin, y, width(), y);
    }

    painter.setPen(QColor(90, 90, 90));
    painter.drawText(m_leftMargin, 16, "Channel State View");

    painter.setPen(Qt::black);
    for (int i = 0; i < channels.size(); ++i)
    {
        const int y = topY + i * rowH + rowH / 2;
        painter.drawText(
            8, y + 5,
            QString("CH %1").arg(channels[i]));
    }

    painter.setPen(QColor(180, 180, 180));
    painter.drawLine(
        m_leftMargin - 8,
        topY,
        m_leftMargin - 8,
        topY + channelCount * rowH);

    const uint64_t visibleStartNs = std::max<uint64_t>(0, m_viewStartNs);
    const uint64_t visibleEndNs = visibleStartNs +
                                  std::max<int>(1, width()) / m_nsToPixel;

    for (int row = 0; row < channels.size(); ++row)
    {
        const int channel = channels[row];
        const QVector<ChannelStateSegment> segments =
            buildChannelStateSegments(
                m_ppduItems,
                channel,
                globalStartNs,
                globalEndNs);

        for (const auto &segment : segments)
        {
            const uint64_t drawStartNs = std::max(segment.startNs, visibleStartNs);
            const uint64_t drawEndNs = std::min(segment.endNs, visibleEndNs);
            if (drawStartNs >= drawEndNs)
                continue;

            QRectF rect(
                m_leftMargin + (drawStartNs - visibleStartNs) * m_nsToPixel,
                topY + row * rowH + kLanePadding,
                std::max(1.0, (drawEndNs - drawStartNs) * m_nsToPixel),
                rowH - 2 * kLanePadding);

            painter.setPen(Qt::NoPen);
            painter.setBrush(channelStateColor(segment.state));
            painter.drawRoundedRect(rect, 3, 3);

            painter.setPen(QColor(70, 70, 70, 100));
            painter.setBrush(Qt::NoBrush);
            painter.drawRoundedRect(rect, 3, 3);

            if (rect.width() >= 52)
            {
                painter.setPen(segment.state == ChannelStateKind::Idle
                                   ? QColor(40, 40, 40)
                                   : Qt::white);
                painter.drawText(
                    rect.adjusted(6, 0, -6, 0),
                    Qt::AlignVCenter | Qt::AlignLeft,
                    channelStateName(segment.state));
            }
        }
    }

    const QPen gridPen(QColor(180, 180, 180), 1, Qt::DashLine);
    painter.setPen(gridPen);
    for (int i = 0; i <= 10; ++i)
    {
        const uint64_t ns =
            visibleStartNs + i * (visibleEndNs - visibleStartNs) / 10;
        const int x = m_leftMargin + (ns - visibleStartNs) * m_nsToPixel;
        painter.drawLine(x, topY, x, topY + channelCount * rowH);
        painter.drawText(
            x - 20,
            topY + channelCount * rowH + 15,
            QString::number(ns / 1e6, 'f', 2) + " ms");
    }

    if (m_selecting)
    {
        int x1 = m_selectStart.x();
        int x2 = m_selectEnd.x();

        const int left = std::clamp(std::min(x1, x2), m_leftMargin, width() - 5);
        const int right = std::clamp(std::max(x1, x2), m_leftMargin, width() - 5);

        if (right > left)
        {
            const QRectF selectionRect(
                left,
                topY,
                right - left,
                channelCount * rowH);

            painter.setPen(QPen(kSelectBorder, 1, Qt::DashLine));
            painter.setBrush(kSelectFill);
            painter.drawRect(selectionRect);
        }
    }

    const int barY = height() - kRangeBarHeight - kRangeBarBottomPadding;
    const int barX = m_leftMargin;
    const int barW = width() - m_leftMargin - kRangeBarMargin;

    if (barW > 50)
    {
        const int leftX = barX + m_rangeStart * barW;
        const int rightX = barX + m_rangeEnd * barW;

        painter.setPen(Qt::NoPen);
        painter.setBrush(QColor(180, 185, 190));
        painter.drawRoundedRect(
            QRectF(barX, barY + kRangeBarHeight / 2 - 2, barW, 4),
            2, 2);

        painter.setBrush(QColor(90, 140, 255, 120));
        painter.drawRoundedRect(
            QRectF(leftX, barY, rightX - leftX, kRangeBarHeight),
            4, 4);

        painter.setBrush(QColor(240, 240, 240));
        painter.drawRoundedRect(
            QRectF(leftX - kRangeHandleW / 2, barY,
                   kRangeHandleW, kRangeBarHeight),
            3, 3);
        painter.drawRoundedRect(
            QRectF(rightX - kRangeHandleW / 2, barY,
                   kRangeHandleW, kRangeBarHeight),
            3, 3);
    }
}

void PpduTimelineView::paintMloChannelStateView(QPainter &painter)
{
    if (m_ppduItems.isEmpty() && m_phyStateItems.isEmpty())
        return;

    const QVector<MloChannelRow> rows =
        collectMloChannelRows(m_ppduItems, m_phyStateItems, m_nodeToMacMap, m_deviceInfoMap);
    if (rows.isEmpty())
        return;

    uint64_t globalStartNs = std::numeric_limits<uint64_t>::max();
    uint64_t globalEndNs = 0;
    for (const auto &item : m_ppduItems)
    {
        if (!isDrawablePpdu(item))
            continue;
        globalStartNs = std::min(globalStartNs, item.txStartNs);
        globalEndNs = std::max(globalEndNs, item.txEndNs);
    }

    if (globalStartNs >= globalEndNs)
        return;

    const int rowCount = rows.size();
    const int availH = height() - m_topMargin - kBottomMargin;
    const int rowH = std::clamp(availH / std::max(1, rowCount), 22, 84);
    int topY = m_topMargin;
    if (availH > rowCount * rowH)
        topY += (availH - rowCount * rowH) / 2;

    painter.setPen(QColor(200, 200, 200));
    for (int r = 0; r <= rowCount; ++r)
    {
        const int y = topY + r * rowH;
        painter.drawLine(m_leftMargin, y, width(), y);
    }

    painter.setPen(QColor(90, 90, 90));
    painter.drawText(m_leftMargin, 16, "MLO Channel State View");

    painter.setPen(Qt::black);
    for (int row = 0; row < rows.size(); ++row)
    {
        const int y = topY + row * rowH + rowH / 2;
        painter.setBrush(senderColor(rows[row].rowKey));
        painter.setPen(Qt::NoPen);
        painter.drawEllipse(QRectF(12, y - 5, 10, 10));
        painter.setPen(QColor(25, 32, 40));
        painter.drawText(QRect(28, topY + row * rowH, m_leftMargin - 36, rowH),
                         Qt::AlignVCenter | Qt::AlignLeft,
                         rows[row].label);
    }

    painter.setPen(QColor(180, 180, 180));
    painter.drawLine(m_leftMargin - 8, topY, m_leftMargin - 8, topY + rowCount * rowH);

    const uint64_t visibleStartNs = std::max<uint64_t>(0, m_viewStartNs);
    const uint64_t visibleEndNs =
        visibleStartNs + std::max<int>(1, width()) / m_nsToPixel;

    for (int row = 0; row < rows.size(); ++row)
    {
        const auto &rowInfo = rows[row];
        const QVector<ChannelStateSegment> segments =
            buildChannelStateSegments(m_ppduItems, rowInfo.channel, globalStartNs, globalEndNs);

        for (const auto &segment : segments)
        {
            const uint64_t drawStartNs = std::max(segment.startNs, visibleStartNs);
            const uint64_t drawEndNs = std::min(segment.endNs, visibleEndNs);
            if (drawStartNs >= drawEndNs)
                continue;

            QRectF rect(
                m_leftMargin + (drawStartNs - visibleStartNs) * m_nsToPixel,
                topY + row * rowH + kLanePadding,
                std::max(1.0, (drawEndNs - drawStartNs) * m_nsToPixel),
                rowH - 2 * kLanePadding);

            painter.setPen(Qt::NoPen);
            painter.setBrush(channelStateColor(segment.state));
            painter.drawRoundedRect(rect, 3, 3);

            painter.setPen(QColor(70, 70, 70, 100));
            painter.setBrush(Qt::NoBrush);
            painter.drawRoundedRect(rect, 3, 3);

            if (rect.width() >= 52)
            {
                painter.setPen(segment.state == ChannelStateKind::Idle
                                   ? QColor(40, 40, 40)
                                   : Qt::white);
                painter.drawText(rect.adjusted(6, 0, -6, 0),
                                 Qt::AlignVCenter | Qt::AlignLeft,
                                 channelStateName(segment.state));
            }
        }
    }

    const QPen gridPen(QColor(180, 180, 180), 1, Qt::DashLine);
    painter.setPen(gridPen);
    for (int i = 0; i <= 10; ++i)
    {
        const uint64_t ns =
            visibleStartNs + i * (visibleEndNs - visibleStartNs) / 10;
        const int x = m_leftMargin + (ns - visibleStartNs) * m_nsToPixel;
        painter.drawLine(x, topY, x, topY + rowCount * rowH);
        painter.drawText(x - 20,
                         topY + rowCount * rowH + 15,
                         QString::number(ns / 1e6, 'f', 2) + " ms");
    }

    if (m_selecting)
    {
        const int left = std::clamp(std::min(m_selectStart.x(), m_selectEnd.x()),
                                    m_leftMargin,
                                    width() - 5);
        const int right = std::clamp(std::max(m_selectStart.x(), m_selectEnd.x()),
                                     m_leftMargin,
                                     width() - 5);

        if (right > left)
        {
            const QRectF selectionRect(left, topY, right - left, rowCount * rowH);
            painter.setPen(QPen(kSelectBorder, 1, Qt::DashLine));
            painter.setBrush(kSelectFill);
            painter.drawRect(selectionRect);
        }
    }

    const int barY = height() - kRangeBarHeight - kRangeBarBottomPadding;
    const int barX = m_leftMargin;
    const int barW = width() - m_leftMargin - kRangeBarMargin;

    if (barW > 50)
    {
        const int leftX = barX + m_rangeStart * barW;
        const int rightX = barX + m_rangeEnd * barW;

        painter.setPen(Qt::NoPen);
        painter.setBrush(QColor(180, 185, 190));
        painter.drawRoundedRect(QRectF(barX, barY + kRangeBarHeight / 2 - 2, barW, 4), 2, 2);

        painter.setBrush(QColor(90, 140, 255, 120));
        painter.drawRoundedRect(QRectF(leftX, barY, rightX - leftX, kRangeBarHeight), 4, 4);

        painter.setBrush(QColor(240, 240, 240));
        painter.drawRoundedRect(QRectF(leftX - kRangeHandleW / 2,
                                       barY,
                                       kRangeHandleW,
                                       kRangeBarHeight),
                                3,
                                3);
        painter.drawRoundedRect(QRectF(rightX - kRangeHandleW / 2,
                                       barY,
                                       kRangeHandleW,
                                       kRangeBarHeight),
                                3,
                                3);
    }
}

void PpduTimelineView::paintPhyStateTimeline(QPainter &painter)
{
    ensurePhyStateCache();
    if (m_cachedPhyStateRows.isEmpty() || m_phyStateStartNs >= m_phyStateEndNs)
        return;

    const int rowCount = m_cachedPhyStateRows.size();
    const int availH = height() - m_topMargin - kBottomMargin;
    const int rowH = std::clamp(availH / std::max(1, rowCount), 22, 84);
    int topY = m_topMargin;
    if (availH > rowCount * rowH)
        topY += (availH - rowCount * rowH) / 2;

    painter.setPen(QColor(200, 200, 200));
    for (int r = 0; r <= rowCount; ++r)
    {
        const int y = topY + r * rowH;
        painter.drawLine(m_leftMargin, y, width(), y);
    }

    painter.setPen(QColor(90, 90, 90));
    painter.drawText(m_leftMargin, 16, "PHY State View");

    painter.setPen(Qt::black);
    for (int row = 0; row < m_cachedPhyStateRows.size(); ++row)
    {
        const int y = topY + row * rowH + rowH / 2;
        painter.drawText(8, y + 5, m_cachedPhyStateRows[row].label);
    }

    painter.setPen(QColor(180, 180, 180));
    painter.drawLine(m_leftMargin - 8, topY, m_leftMargin - 8, topY + rowCount * rowH);

    const uint64_t visibleStartNs = std::max<uint64_t>(0, m_viewStartNs);
    const uint64_t visibleEndNs =
        visibleStartNs + std::max<int>(1, width()) / m_nsToPixel;

    for (int row = 0; row < m_cachedPhyStateRows.size(); ++row)
    {
        const auto &rowInfo = m_cachedPhyStateRows[row];
        const auto &indices = rowInfo.itemIndices;
        if (indices.isEmpty())
            continue;

        const auto firstIt =
            std::lower_bound(indices.cbegin(), indices.cend(), visibleStartNs,
                             [this](int itemIndex, uint64_t value) {
                                 return m_phyStateItems[itemIndex].phyStateStartNs < value;
                             });
        auto drawIt = firstIt;
        if (drawIt != indices.cbegin())
            --drawIt;

        QRectF pendingRect;
        PhyStateKindView pendingState = PhyStateKindView::Other;
        bool hasPending = false;
        auto flushPending = [&]() {
            if (!hasPending)
                return;
            painter.setPen(Qt::NoPen);
            painter.setBrush(phyStateColor(pendingState));
            if (pendingRect.width() < 3.0 || pendingRect.height() < 8.0)
            {
                painter.drawRect(pendingRect);
            }
            else
            {
                painter.drawRoundedRect(pendingRect, 3, 3);
                painter.setPen(QColor(70, 70, 70, 100));
                painter.setBrush(Qt::NoBrush);
                painter.drawRoundedRect(pendingRect, 3, 3);
            }
            if (pendingRect.width() >= 62)
            {
                painter.setPen(pendingState == PhyStateKindView::Idle ? QColor(40, 40, 40) : Qt::white);
                painter.drawText(pendingRect.adjusted(6, 0, -6, 0),
                                 Qt::AlignVCenter | Qt::AlignLeft,
                                 phyStateName(pendingState));
            }
            hasPending = false;
        };

        for (auto it = drawIt; it != indices.cend(); ++it)
        {
            const auto &segment = m_phyStateItems[*it];
            if (segment.phyStateStartNs > visibleEndNs)
                break;

            const uint64_t drawStartNs = std::max(segment.phyStateStartNs, visibleStartNs);
            const uint64_t drawEndNs = std::min(segment.phyStateEndNs, visibleEndNs);
            if (drawStartNs >= drawEndNs)
                continue;

            QRectF rect(
                m_leftMargin + (drawStartNs - visibleStartNs) * m_nsToPixel,
                topY + row * rowH + kLanePadding,
                std::max(1.0, (drawEndNs - drawStartNs) * m_nsToPixel),
                rowH - 2 * kLanePadding);

            const auto state = toPhyStateKind(segment.phyState);
            if (hasPending &&
                pendingState == state &&
                rect.left() <= pendingRect.right() + 0.75 &&
                rect.width() < 3.0 &&
                pendingRect.width() < 24.0)
            {
                pendingRect = pendingRect.united(rect);
                continue;
            }

            flushPending();
            pendingRect = rect;
            pendingState = state;
            hasPending = true;
        }
        flushPending();
    }

    const QPen gridPen(QColor(180, 180, 180), 1, Qt::DashLine);
    painter.setPen(gridPen);
    for (int i = 0; i <= 10; ++i)
    {
        const uint64_t ns =
            visibleStartNs + i * (visibleEndNs - visibleStartNs) / 10;
        const int x = m_leftMargin + (ns - visibleStartNs) * m_nsToPixel;
        painter.drawLine(x, topY, x, topY + rowCount * rowH);
        painter.drawText(x - 20,
                         topY + rowCount * rowH + 15,
                         QString::number(ns / 1e6, 'f', 2) + " ms");
    }

    if (m_selecting)
    {
        const int left = std::clamp(std::min(m_selectStart.x(), m_selectEnd.x()),
                                    m_leftMargin,
                                    width() - 5);
        const int right = std::clamp(std::max(m_selectStart.x(), m_selectEnd.x()),
                                     m_leftMargin,
                                     width() - 5);

        if (right > left)
        {
            const QRectF selectionRect(left, topY, right - left, rowCount * rowH);
            painter.setPen(QPen(kSelectBorder, 1, Qt::DashLine));
            painter.setBrush(kSelectFill);
            painter.drawRect(selectionRect);
        }
    }

    const int barY = height() - kRangeBarHeight - kRangeBarBottomPadding;
    const int barX = m_leftMargin;
    const int barW = width() - m_leftMargin - kRangeBarMargin;

    if (barW > 50)
    {
        const int leftX = barX + m_rangeStart * barW;
        const int rightX = barX + m_rangeEnd * barW;

        painter.setPen(Qt::NoPen);
        painter.setBrush(QColor(180, 185, 190));
        painter.drawRoundedRect(QRectF(barX, barY + kRangeBarHeight / 2 - 2, barW, 4), 2, 2);

        painter.setBrush(QColor(90, 140, 255, 120));
        painter.drawRoundedRect(QRectF(leftX, barY, rightX - leftX, kRangeBarHeight), 4, 4);

        painter.setBrush(QColor(240, 240, 240));
        painter.drawRoundedRect(QRectF(leftX - kRangeHandleW / 2,
                                       barY,
                                       kRangeHandleW,
                                       kRangeBarHeight),
                                3,
                                3);
        painter.drawRoundedRect(QRectF(rightX - kRangeHandleW / 2,
                                       barY,
                                       kRangeHandleW,
                                       kRangeBarHeight),
                                3,
                                3);
    }
}

void PpduTimelineView::resizeEvent(QResizeEvent *)
{
    if (m_btnLegend)
    {
        m_btnLegend->move(width() - m_btnLegend->width() - 8, 8);
    }

    if (m_legendOverlay)
    {
        m_legendOverlay->move(
            width() - m_legendOverlay->width() - 10,
            40);
    }
}

void PpduTimelineView::onSaveImage()
{
    QString file = QFileDialog::getSaveFileName(
        this,
        "Save Timeline",
        QDir::homePath() + "/ppdu_timeline.png",
        "PNG Image (*.png);;JPEG Image (*.jpg)");

    if (file.isEmpty())
    {
        qDebug() << "Save canceled";
        return;
    }

    QImage img(this->size() * devicePixelRatioF(),
               QImage::Format_ARGB32_Premultiplied);
    img.setDevicePixelRatio(devicePixelRatioF());
    img.fill(kBgColor);

    QPainter p(&img);
    this->render(&p);

    if (!img.save(file))
    {
        qDebug() << "Save failed:" << file;
    }
    else
    {
        qDebug() << "Saved to:" << file;
    }
}

void PpduTimelineView::onToggleLegend()
{
    qDebug() << "onToggleLegend triggered";

    if (!m_legendOverlay)
        return;

    if (m_legendOverlay->isVisible())
    {
        m_legendOverlay->close();
        return;
    }

    QPoint globalTopRight = mapToGlobal(rect().topRight());

    QSize sz = m_legendOverlay->sizeHint();

    QPoint pos(
        globalTopRight.x() - sz.width() - 12,
        globalTopRight.y() + 36);

    m_legendOverlay->move(pos);
    m_legendOverlay->show();
    m_legendOverlay->raise();
}

void PpduTimelineView::onSetTimeRange()
{
    const auto [dataStartNs, dataEndNs] = currentTimeBounds();
    if (dataEndNs <= dataStartNs)
        return;

    bool ok1 = false, ok2 = false;

    double startMs = QInputDialog::getDouble(
        this,
        "Set Time Range",
        "Start Time (ms):",
        m_viewStartNs / 1e6,
        0, 1e12, 3,
        &ok1);

    if (!ok1)
        return;

    double endMs = QInputDialog::getDouble(
        this,
        "Set Time Range",
        "End Time (ms):",
        startMs + 1.0,
        startMs, 1e12, 3,
        &ok2);

    if (!ok2 || endMs <= startMs)
        return;

    int64_t startNs = startMs * 1e6;
    int64_t endNs = endMs * 1e6;
    int64_t rangeNs = endNs - startNs;

    int usableWidth = width() - m_leftMargin - 10;
    if (usableWidth <= 0 || rangeNs <= 0)
        return;

    m_viewStartNs = startNs;
    m_nsToPixel = double(usableWidth) / double(rangeNs);

    m_nsToPixel = std::clamp(m_nsToPixel, 1e-9, 1e-4);
    m_viewStartNs = clampViewStartNs(m_viewStartNs, m_nsToPixel);
    syncRangeSliderToView();

    hideHoverUi();
    update();
}

/* ======================== Interaction ======================== */

void PpduTimelineView::wheelEvent(QWheelEvent *event)
{
    const int delta = event->angleDelta().y();
    if (delta == 0)
    {
        event->accept();
        return;
    }

    const int usableWidth = usableTimelineWidth();
    const int mouseX = std::clamp(event->position().toPoint().x(), m_leftMargin, m_leftMargin + usableWidth);
    const double anchorPx = double(mouseX - m_leftMargin);
    const double anchorNs = double(m_viewStartNs) + anchorPx / std::max(m_nsToPixel, 1e-12);

    const double steps = std::max(1.0, std::abs(double(delta)) / 120.0);
    const double factor = std::pow(1.2, steps);
    const double oldScale = m_nsToPixel;
    double newScale = (delta > 0) ? oldScale * factor : oldScale / factor;
    newScale = std::clamp(newScale, 1e-9, 1e-4);

    m_nsToPixel = newScale;
    const double requestedStartNs = anchorNs - anchorPx / m_nsToPixel;
    m_viewStartNs = clampViewStartNs(requestedStartNs, m_nsToPixel);
    syncRangeSliderToView();
    hideHoverUi();
    update();
    event->accept();
}

void PpduTimelineView::mousePressEvent(QMouseEvent *e)
{
    m_showingStats = false;
    hideHoverUi();
    m_mousePos = e->pos();

    int barY = height() - kRangeBarHeight - kRangeBarBottomPadding;
    int barX = m_leftMargin;
    int barW = width() - m_leftMargin - kRangeBarMargin;

    if (e->button() == Qt::LeftButton &&
        e->y() >= barY && e->y() <= barY + kRangeBarHeight)
    {
        int leftX = barX + m_rangeStart * barW;
        int rightX = barX + m_rangeEnd * barW;

        if (qAbs(e->x() - leftX) < 6)
            m_dragLeftHandle = true;
        else if (qAbs(e->x() - rightX) < 6)
            m_dragRightHandle = true;
        else if (e->x() > leftX && e->x() < rightX)
            m_dragRangeBody = true;

        if (m_dragLeftHandle || m_dragRightHandle || m_dragRangeBody)
        {
            m_rangeDragging = true;
            m_lastRangeX = e->x();
            setCursor(Qt::SizeHorCursor);
            return;
        }
    }

    m_showingStats = false;
    hideHoverUi();

    if (childAt(e->pos()) == m_btnLegend ||
        childAt(e->pos()) == m_btnSave ||
        childAt(e->pos()) == m_btnSetTimeRange ||
        childAt(e->pos()) == m_btnChannel ||
        childAt(e->pos()) == quitButton)
    {
        QWidget::mousePressEvent(e);
        return;
    }

    if (e->button() == Qt::LeftButton)
    {
	        m_pressedIndex = (m_viewMode == TimelineViewMode::PpduTimeline ||
	                          m_viewMode == TimelineViewMode::MloChannelTimeline ||
	                          m_viewMode == TimelineViewMode::RxTimeline)
	                             ? hitTest(e->pos())
	                             : -1;
        m_pressMoved = false;
        m_dragging = true;
        m_pressPos = e->pos();
        m_lastMousePos = e->pos();
        setCursor(Qt::ClosedHandCursor);
    }
    else if (e->button() == Qt::RightButton)
    {
        m_selecting = true;
        m_selectStart = m_selectEnd = e->pos();
        setCursor(Qt::CrossCursor);
    }
}

void PpduTimelineView::mouseMoveEvent(QMouseEvent *e)
{
    m_mousePos = e->pos();

    if (m_rangeDragging)
    {
        hideHoverUi();
        const auto [dataStartNs, dataEndNs] = currentTimeBounds();
        if (dataEndNs <= dataStartNs)
            return;

        int barW = width() - m_leftMargin - kRangeBarMargin;
        double dx = double(e->x() - m_lastRangeX) / double(barW);

        if (m_dragLeftHandle)
            m_rangeStart = std::clamp(m_rangeStart + dx, 0.0, m_rangeEnd - 0.01);
        else if (m_dragRightHandle)
            m_rangeEnd = std::clamp(m_rangeEnd + dx, m_rangeStart + 0.01, 1.0);
        else if (m_dragRangeBody)
        {
            double w = m_rangeEnd - m_rangeStart;
            m_rangeStart = std::clamp(m_rangeStart + dx, 0.0, 1.0 - w);
            m_rangeEnd = m_rangeStart + w;
        }

        // 同步时间轴
        uint64_t span = dataEndNs - dataStartNs;
        if (span == 0)
            return;

        m_viewStartNs = dataStartNs + m_rangeStart * span;

        int usableWidth = width() - m_leftMargin - 10;
        m_nsToPixel = double(usableWidth) /
                      double((m_rangeEnd - m_rangeStart) * span);

        m_lastRangeX = e->x();
        update();
        return;
    }

    if (m_showingStats)
        return;

    if (m_selecting)
    {
        if (m_overlay)
            m_overlay->close();
        QToolTip::hideText();
        m_selectEnd = e->pos();
        update();
        return;
    }

    if (m_dragging)
    {
        if ((e->pos() - m_pressPos).manhattanLength() > 3)
            m_pressMoved = true;

        int dx = e->pos().x() - m_lastMousePos.x();
        m_lastMousePos = e->pos();

        if (m_pressMoved && qAbs(dx) > 0)
        {
            m_viewStartNs = clampViewStartNs(
                double(m_viewStartNs) - double(dx) / std::max(m_nsToPixel, 1e-12),
                m_nsToPixel);
            syncRangeSliderToView();
            update();
        }

        hideHoverUi();
        return;
    }
    if (m_viewMode == TimelineViewMode::ChannelState)
    {
        m_hoverIndex = -1;
        QToolTip::hideText();
        if (!showChannelStateHover(e->pos()))
            m_overlay->close();
        update();
        return;
    }

    if (m_viewMode == TimelineViewMode::MloChannelState)
    {
        m_hoverIndex = -1;
        QToolTip::hideText();
        if (!showMloChannelStateHover(e->pos()))
            m_overlay->close();
        update();
        return;
    }

    if (m_viewMode == TimelineViewMode::PhyStateTimeline)
    {
        m_hoverIndex = -1;
        QToolTip::hideText();
        if (!showPhyStateHover(e->pos()))
            m_overlay->close();
        update();
        return;
    }

    /* ===== hover ===== */
    int idx = hitTest(e->pos());

    m_hoverIndex = idx;
    if (m_overlay)
        m_overlay->close();
    if (idx >= 0 || !showRowLabelTooltip(e->pos()))
        QToolTip::hideText();
    update();
}

void PpduTimelineView::mouseReleaseEvent(QMouseEvent *e)
{

    if (e->button() == Qt::LeftButton && m_dragging)
    {
        m_dragging = false;
        setCursor(Qt::ArrowCursor);
	        if (!m_pressMoved && m_pressedIndex >= 0 &&
	            (m_viewMode == TimelineViewMode::PpduTimeline ||
	             m_viewMode == TimelineViewMode::MloChannelTimeline ||
	             m_viewMode == TimelineViewMode::RxTimeline))
        {
            m_selectedIndex = m_pressedIndex;
            updateDetailWindow(m_selectedIndex);
            update();
        }
        m_pressedIndex = -1;
        return;
    }

    if (e->button() == Qt::RightButton && m_selecting)
    {
        m_selecting = false;
        setCursor(Qt::ArrowCursor);

        int x1 = std::min(m_selectStart.x(), m_selectEnd.x());
        int x2 = std::max(m_selectStart.x(), m_selectEnd.x());

        if (x2 - x1 < 10)
        {
            update();
            return;
        }

        int left = m_leftMargin;
        int right = width() - 5;
        x1 = std::clamp(x1, left, right);
        x2 = std::clamp(x2, left, right);

        int64_t startNs =
            m_viewStartNs +
            (x1 - m_leftMargin) / m_nsToPixel;

        int64_t endNs =
            m_viewStartNs +
            (x2 - m_leftMargin) / m_nsToPixel;

        if (endNs > startNs)
        {
	            if (m_viewMode == TimelineViewMode::PpduTimeline ||
	                m_viewMode == TimelineViewMode::MloChannelTimeline ||
	                m_viewMode == TimelineViewMode::RxTimeline)
            {
                auto stats = computeStats(startNs, endNs);

                showStatsOverlay(
                    stats,
                    mapToGlobal(QPoint(x2, m_selectEnd.y())));

                m_showingStats = true;
            }

            int usableWidth = width() - m_leftMargin - 10;
            m_viewStartNs = startNs;
            m_nsToPixel = double(usableWidth) / double(endNs - startNs);

            m_nsToPixel = std::clamp(m_nsToPixel, 1e-9, 1e-4);
            m_viewStartNs = clampViewStartNs(m_viewStartNs, m_nsToPixel);
            syncRangeSliderToView();
        }

        m_hoverIndex = -1;
        QToolTip::hideText();
        update();
    }

    if (m_rangeDragging)
    {
        m_rangeDragging = false;
        m_dragLeftHandle = false;
        m_dragRightHandle = false;
        m_dragRangeBody = false;
        setCursor(Qt::ArrowCursor);
        return;
    }
}

void PpduTimelineView::leaveEvent(QEvent *)
{
    m_showingStats = false;
    hideHoverUi();
    update();
}

void PpduTimelineView::closeEvent(QCloseEvent *e)
{
    emit timelineClosed();
    e->accept();
}

/* ======================== hitTest ======================== */

int PpduTimelineView::hitTest(const QPoint &pos) const
{
    ensurePpduLayoutCache();

    if (m_ppduItems.isEmpty())
        return -1;

    int rowCnt = m_cachedPpduRows.size();
    if (rowCnt == 0)
        return -1;

    int availH = height() - m_topMargin - kBottomMargin;
    int rowH = std::clamp(availH / rowCnt, 18, 80);

    int topY = m_topMargin;
    if (availH > rowCnt * rowH)
        topY += (availH - rowCnt * rowH) / 2;

    for (int idx = m_ppduItems.size() - 1; idx >= 0; --idx)
    {
        const auto& ppdu = m_ppduItems[idx];
        const auto& layout = m_cachedPpduLayout[idx];
        if (layout.row < 0 || layout.row >= m_cachedPpduRows.size())
            continue;

        const int laneCount = std::max(1, m_cachedPpduRows[layout.row].laneCount);
        const double laneH = (rowH - 2) / double(laneCount);

        QRectF r(
            m_leftMargin + (ppdu.txStartNs - m_viewStartNs) * m_nsToPixel,
            topY + layout.row * rowH + layout.lane * laneH + kLanePadding,
            std::max(1.0, (ppdu.txEndNs - ppdu.txStartNs) * m_nsToPixel),
            laneH - 2 * kLanePadding);

        if (r.contains(pos))
            return idx;
    }

    return -1;
}

bool PpduTimelineView::showChannelStateHover(const QPoint &pos)
{
    if (m_ppduItems.isEmpty())
        return false;

    const QVector<int> channels = collectChannels(m_ppduItems);
    if (channels.isEmpty())
        return false;

    uint64_t globalStartNs = std::numeric_limits<uint64_t>::max();
    uint64_t globalEndNs = 0;
    for (const auto &item : m_ppduItems)
    {
        if (!isDrawablePpdu(item))
            continue;
        globalStartNs = std::min(globalStartNs, item.txStartNs);
        globalEndNs = std::max(globalEndNs, item.txEndNs);
    }

    if (globalStartNs >= globalEndNs)
        return false;

    const int channelCount = channels.size();
    const int availH = height() - m_topMargin - kBottomMargin;
    const int rowH = std::clamp(availH / channelCount, 22, 84);
    int topY = m_topMargin;
    if (availH > channelCount * rowH)
        topY += (availH - channelCount * rowH) / 2;

    const uint64_t visibleStartNs = std::max<uint64_t>(0, m_viewStartNs);
    const uint64_t visibleEndNs = visibleStartNs +
                                  std::max<int>(1, width()) / m_nsToPixel;

    for (int row = 0; row < channels.size(); ++row)
    {
        const int channel = channels[row];
        const QRect labelRect(0, topY + row * rowH, m_leftMargin - 10, rowH);
        if (labelRect.contains(pos))
        {
            const QString text =
                QString("Channel %1\nBand: %2")
                    .arg(channel)
                    .arg(channelBand(channel));

            m_overlay->setText(text);
            m_overlay->showAt(mapToGlobal(pos) + QPoint(12, 12));
            return true;
        }

        const QVector<ChannelStateSegment> segments =
            buildChannelStateSegments(
                m_ppduItems,
                channel,
                globalStartNs,
                globalEndNs);

        for (const auto &segment : segments)
        {
            const uint64_t drawStartNs = std::max(segment.startNs, visibleStartNs);
            const uint64_t drawEndNs = std::min(segment.endNs, visibleEndNs);
            if (drawStartNs >= drawEndNs)
                continue;

            const QRectF rect(
                m_leftMargin + (drawStartNs - visibleStartNs) * m_nsToPixel,
                topY + row * rowH + kLanePadding,
                std::max(1.0, (drawEndNs - drawStartNs) * m_nsToPixel),
                rowH - 2 * kLanePadding);

            if (!rect.contains(pos))
                continue;

            QString text =
                QString("Channel %1\n"
                        "Band: %2\n"
                        "State: %3\n"
                        "Start: %4 ms\n"
                        "End: %5 ms\n"
                        "Duration: %6 us")
                    .arg(channel)
                    .arg(channelBand(channel))
                    .arg(channelStateName(segment.state))
                    .arg(segment.startNs / 1e6, 0, 'f', 3)
                    .arg(segment.endNs / 1e6, 0, 'f', 3)
                    .arg((segment.endNs - segment.startNs) / 1e3, 0, 'f', 1);

            if (segment.activeCount > 1)
            {
                text += QString("\nConcurrent PPDU: %1")
                            .arg(segment.activeCount);
            }

            m_overlay->setText(text);
            m_overlay->showAt(mapToGlobal(pos) + QPoint(12, 12));
            return true;
        }
    }

    return false;
}

bool PpduTimelineView::showMloChannelStateHover(const QPoint &pos)
{
    if (m_ppduItems.isEmpty() && m_phyStateItems.isEmpty())
        return false;

    const QVector<MloChannelRow> rows =
        collectMloChannelRows(m_ppduItems, m_phyStateItems, m_nodeToMacMap, m_deviceInfoMap);
    if (rows.isEmpty())
        return false;

    uint64_t globalStartNs = std::numeric_limits<uint64_t>::max();
    uint64_t globalEndNs = 0;
    for (const auto &item : m_ppduItems)
    {
        if (!isDrawablePpdu(item))
            continue;
        globalStartNs = std::min(globalStartNs, item.txStartNs);
        globalEndNs = std::max(globalEndNs, item.txEndNs);
    }

    if (globalStartNs >= globalEndNs)
        return false;

    const int rowCount = rows.size();
    const int availH = height() - m_topMargin - kBottomMargin;
    const int rowH = std::clamp(availH / std::max(1, rowCount), 22, 84);
    int topY = m_topMargin;
    if (availH > rowCount * rowH)
        topY += (availH - rowCount * rowH) / 2;

    const uint64_t visibleStartNs = std::max<uint64_t>(0, m_viewStartNs);
    const uint64_t visibleEndNs =
        visibleStartNs + std::max<int>(1, width()) / m_nsToPixel;

    for (int row = 0; row < rows.size(); ++row)
    {
        const auto &rowInfo = rows[row];
        const QRect labelRect(0, topY + row * rowH, m_leftMargin - 10, rowH);
        if (labelRect.contains(pos))
        {
            const QString text =
                QString("%1\nLink: %2\nChannel: %3 (%4)")
                    .arg(rowInfo.label)
                    .arg(rowInfo.linkId)
                    .arg(rowInfo.channel)
                    .arg(channelBand(rowInfo.channel));

            m_overlay->setText(text);
            m_overlay->showAt(mapToGlobal(pos) + QPoint(12, 12));
            return true;
        }

        const QVector<ChannelStateSegment> segments =
            buildChannelStateSegments(m_ppduItems,
                                      rowInfo.channel,
                                      globalStartNs,
                                      globalEndNs);

        for (const auto &segment : segments)
        {
            const uint64_t drawStartNs = std::max(segment.startNs, visibleStartNs);
            const uint64_t drawEndNs = std::min(segment.endNs, visibleEndNs);
            if (drawStartNs >= drawEndNs)
                continue;

            const QRectF rect(
                m_leftMargin + (drawStartNs - visibleStartNs) * m_nsToPixel,
                topY + row * rowH + kLanePadding,
                std::max(1.0, (drawEndNs - drawStartNs) * m_nsToPixel),
                rowH - 2 * kLanePadding);

            if (!rect.contains(pos))
                continue;

            QString text =
                QString("%1\n"
                        "State: %2\n"
                        "Start: %3 ms\n"
                        "End: %4 ms\n"
                        "Duration: %5 us")
                    .arg(rowInfo.label)
                    .arg(channelStateName(segment.state))
                    .arg(segment.startNs / 1e6, 0, 'f', 3)
                    .arg(segment.endNs / 1e6, 0, 'f', 3)
                    .arg((segment.endNs - segment.startNs) / 1e3, 0, 'f', 1);

            if (segment.activeCount > 1)
            {
                text += QString("\nConcurrent PPDU: %1").arg(segment.activeCount);
            }

            m_overlay->setText(text);
            m_overlay->showAt(mapToGlobal(pos) + QPoint(12, 12));
            return true;
        }
    }

    return false;
}

bool PpduTimelineView::showPhyStateHover(const QPoint &pos)
{
    ensurePhyStateCache();
    if (m_cachedPhyStateRows.isEmpty() || m_phyStateStartNs >= m_phyStateEndNs)
        return false;

    const int rowCount = m_cachedPhyStateRows.size();
    const int availH = height() - m_topMargin - kBottomMargin;
    const int rowH = std::clamp(availH / std::max(1, rowCount), 22, 84);
    int topY = m_topMargin;
    if (availH > rowCount * rowH)
        topY += (availH - rowCount * rowH) / 2;

    const uint64_t visibleStartNs = std::max<uint64_t>(0, m_viewStartNs);
    const uint64_t visibleEndNs =
        visibleStartNs + std::max<int>(1, width()) / m_nsToPixel;

    for (int row = 0; row < m_cachedPhyStateRows.size(); ++row)
    {
        const QRect labelRect(0, topY + row * rowH, m_leftMargin - 10, rowH);
        if (labelRect.contains(pos))
        {
            m_overlay->setText(m_cachedPhyStateRows[row].label);
            m_overlay->showAt(mapToGlobal(pos) + QPoint(12, 12));
            return true;
        }
    }

    const int hoveredRow = (pos.y() - topY) / rowH;
    if (hoveredRow < 0 || hoveredRow >= m_cachedPhyStateRows.size())
        return false;

    const auto &indices = m_cachedPhyStateRows[hoveredRow].itemIndices;
    if (indices.isEmpty())
        return false;

    const auto firstIt =
        std::lower_bound(indices.cbegin(), indices.cend(), visibleStartNs,
                         [this](int itemIndex, uint64_t value) {
                             return m_phyStateItems[itemIndex].phyStateStartNs < value;
                         });
    auto hoverIt = firstIt;
    if (hoverIt != indices.cbegin())
        --hoverIt;

    for (auto it = hoverIt; it != indices.cend(); ++it)
    {
        const auto &segment = m_phyStateItems[*it];
        if (segment.phyStateStartNs > visibleEndNs)
            break;

        const uint64_t drawStartNs = std::max(segment.phyStateStartNs, visibleStartNs);
        const uint64_t drawEndNs = std::min(segment.phyStateEndNs, visibleEndNs);
        if (drawStartNs >= drawEndNs)
            continue;

        const QRectF rect(
            m_leftMargin + (drawStartNs - visibleStartNs) * m_nsToPixel,
            topY + hoveredRow * rowH + kLanePadding,
            std::max(1.0, (drawEndNs - drawStartNs) * m_nsToPixel),
            rowH - 2 * kLanePadding);

        if (!rect.contains(pos))
            continue;

        const QString text =
	            QString("Node: %1\n"
	                    "Link: %2\n"
	                    "Channel: %3\n"
	                    "State: %4\n"
	                    "Start: %5 ms\n"
	                    "End: %6 ms\n"
	                    "Duration: %7 us")
		                .arg(nodeRoleLabel(segment.nodeId))
		                .arg(segment.linkId)
		                .arg(segment.channel_number)
		                .arg(phyStateName(toPhyStateKind(segment.phyState)))
		                .arg(segment.phyStateStartNs / 1e6, 0, 'f', 3)
		                .arg(segment.phyStateEndNs / 1e6, 0, 'f', 3)
	                .arg((segment.phyStateEndNs - segment.phyStateStartNs) / 1e3, 0, 'f', 1);

        m_overlay->setText(text);
        m_overlay->showAt(mapToGlobal(pos) + QPoint(12, 12));
        return true;
    }

    return false;
}


void PpduTimelineView::resetPage()
{
    /* ===== data ===== */
    m_ppduItems.clear();
    m_phyStateItems.clear();
    m_seenPpduKeys.clear();
    m_cachedPpduLayout.clear();
    m_cachedPpduRows.clear();
    m_cachedPhyStateRows.clear();
    m_ppduLayoutDirty = true;
    m_phyStateCacheDirty = true;
    m_phyStateStartNs = 0;
    m_phyStateEndNs = 0;
    m_dataUpdateQueued = false;
    m_phyStateUpdateQueued = false;

    /* ===== view state ===== */
    m_viewStartNs = 0;
    m_nsToPixel = 1e-6;

    Num_ap = 0;
    Num_sta = 0;

    /* ===== hover / selection ===== */
    m_hoverIndex = -1;
    m_selectedIndex = -1;
    m_pressedIndex = -1;
    m_pressMoved = false;
    m_hasSelection = false;
    m_showingStats = false;
    m_deviceInfoMap.clear();
    m_nodeToMacMap.clear();

    m_selecting = false;
    m_dragging = false;

    /* ===== mouse / drag ===== */
    m_dragLeftHandle = false;
    m_dragRightHandle = false;
    m_dragRangeBody = false;
    m_rangeDragging = false;
    m_pressPos = QPoint();

    /* ===== time range slider ===== */
    m_rangeStart = 0.0;
    m_rangeEnd = 1.0;
    m_lastRangeX = 0;

    /* ===== view mode ===== */
    m_rowMode = TimelineRowMode::BySender;
    m_viewMode = TimelineViewMode::PpduTimeline;

    /* ===== overlays ===== */
    if (m_overlay)
        m_overlay->close();

    if (m_legendOverlay)
        m_legendOverlay->close();

    if (m_detailWindow)
    {
        m_detailWindow->clearDetails();
        m_detailWindow->show();
    }

    /* ===== cursor ===== */
    unsetCursor();
    updateModeButton();
    QToolTip::hideText();

    update();

    qDebug() << "[PpduTimelineView] resetPage() done";
}
