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
#include <complex>
#include <cstdlib>
#include <iostream>
#include <limits>

/* ======================== Constants ======================== */

static constexpr int kBottomMargin = 40;
static constexpr int kLanePadding = 2;
/* ====== UI Colors ======= */
static const QColor kBgColor(245, 246, 248);
static const QColor kHoverRow(230, 235, 240);
static const QColor kGridLine(200, 205, 210);
static const QColor kPpduBlue(76, 114, 176);
static const QColor kPpduHover(220, 50, 47);
static const QColor kSelectFill(80, 140, 255, 60);
static const QColor kSelectBorder(80, 140, 255, 160);

static constexpr int kRangeBarHeight = 16;
static constexpr int kRangeHandleW = 8;
static constexpr int kRangeBarMargin = 10;

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
    uint64_t startNs = 0;
    uint64_t endNs = 0;
    PhyStateKindView state = PhyStateKindView::Idle;
};

static inline uint64_t phyRowKey(uint16_t nodeId, uint8_t channel)
{
    return (static_cast<uint64_t>(nodeId) << 32) | channel;
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

static QVector<int> collectChannels(const QVector<PpduVisualItem> &items)
{
    QMap<int, bool> channelMap;
    for (const auto &item : items)
        channelMap[item.channel_number] = true;

    QVector<int> channels;
    channels.reserve(channelMap.size());
    for (auto it = channelMap.cbegin(); it != channelMap.cend(); ++it)
        channels.push_back(it.key());

    return channels;
}

static QVector<PhyStateSegment> collectPhyStateSegments(const QVector<PpduVisualItem> &items)
{
    QVector<PhyStateSegment> segments;
    segments.reserve(items.size());

    for (const auto &item : items)
    {
        if (item.recordType != RecordType::PhyState || item.phyStateDurationNs == 0)
            continue;

        segments.push_back({
            phyRowKey(item.nodeId, item.channel_number),
            item.nodeId,
            item.channel_number,
            item.phyStateStartNs,
            item.phyStateEndNs,
            toPhyStateKind(item.phyState)});
    }

    std::sort(segments.begin(), segments.end(),
              [](const PhyStateSegment &a, const PhyStateSegment &b) {
                  if (a.rowKey != b.rowKey)
                      return a.rowKey < b.rowKey;
                  if (a.startNs != b.startNs)
                      return a.startNs < b.startNs;
                  return a.endNs < b.endNs;
              });
    return segments;
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
      m_rowMode(TimelineRowMode::ByAp),
      m_viewMode(TimelineViewMode::PpduTimeline)
{
    setMouseTracking(true);

    m_overlay = new PpduInfoOverlay(this);
    m_overlay->close();

    m_btnSave = new QPushButton("~", this);
    m_btnSave->setFixedSize(26, 26);
    m_btnSave->move(8, 8);
    connect(m_btnSave, &QPushButton::clicked,
            this, &PpduTimelineView::onSaveImage);

    m_btnSetTimeRange = new QPushButton("⏱", this);
    m_btnSetTimeRange->setFixedSize(26, 26);
    m_btnSetTimeRange->move(42, 8);
    connect(m_btnSetTimeRange, &QPushButton::clicked,
            this, &PpduTimelineView::onSetTimeRange);

    // Channel View Button
    m_btnChannel = new QPushButton("CH", this);
    m_btnChannel->setFixedSize(50, 50);
    m_btnChannel->move(76, 8);
    m_btnChannel->setToolTip("Switch to channel state view");
    connect(m_btnChannel, &QPushButton::clicked,
            this, &PpduTimelineView::onToggleChannelView);

    quitButton = new QPushButton("quit", this);

    quitButton->setFixedSize(120, 50);
    quitButton->move(142, 8);

    connect(quitButton, &QPushButton::clicked,
            this, &PpduTimelineView::quit_simulation);

    m_btnLegend = new QPushButton("ⓘ", this);
    m_btnLegend->setFixedSize(26, 26);
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

/* ======================== Row Key ======================== */
// ★ NEW
uint64_t PpduTimelineView::rowKey(const PpduVisualItem &ppdu) const
{
    if (m_rowMode == TimelineRowMode::ByChannel)
        return static_cast<uint64_t>(ppdu.channel_number);
    if (m_rowMode == TimelineRowMode::ByNodeLink)
        return phyRowKey(ppdu.nodeId, ppdu.channel_number);

    // default: AP view
    return ppdu.receiver;
}

/* ======================== Channel View Toggle ======================== */
// ★ NEW
void PpduTimelineView::onToggleChannelView()
{
    switch (m_viewMode)
    {
    case TimelineViewMode::PpduTimeline:
        m_viewMode = TimelineViewMode::ChannelState;
        m_rowMode = TimelineRowMode::ByChannel;
        break;
    case TimelineViewMode::ChannelState:
        m_viewMode = TimelineViewMode::PhyStateTimeline;
        m_rowMode = TimelineRowMode::ByNodeLink;
        break;
    case TimelineViewMode::PhyStateTimeline:
        m_viewMode = TimelineViewMode::PpduTimeline;
        m_rowMode = TimelineRowMode::ByAp;
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
        m_btnChannel->setText("CH");
        m_btnChannel->setToolTip("Switch to channel PPDU view");
        break;
    case TimelineViewMode::ChannelState:
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
        items = &m_phyStateItems;
    else
        items = &m_ppduItems;

    if (items->isEmpty())
        return {0, 0};

    uint64_t startNs = std::numeric_limits<uint64_t>::max();
    uint64_t endNs = 0;

    for (const auto &item : *items)
    {
        const uint64_t itemStart =
            item.recordType == RecordType::PhyState ? item.phyStateStartNs : item.txStartNs;
        const uint64_t itemEnd =
            item.recordType == RecordType::PhyState ? item.phyStateEndNs : item.txEndNs;
        startNs = std::min(startNs, itemStart);
        endNs = std::max(endNs, itemEnd);
    }

    return {startNs, endNs};
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

void PpduTimelineView::markPpduLayoutDirty()
{
    m_ppduLayoutDirty = true;
}

void PpduTimelineView::ensurePpduLayoutCache() const
{
    if (!m_ppduLayoutDirty)
        return;

    const_cast<PpduTimelineView*>(this)->rebuildPpduLayoutCache();
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

    int nodeCounter = 0;
    QHash<uint64_t, int> nodeIndexMap;
    for (const auto& row : m_cachedPpduRows)
    {
        if (!isBroadcastMac(row.rowKey))
            nodeIndexMap.insert(row.rowKey, ++nodeCounter);
    }

    for (auto& row : m_cachedPpduRows)
    {
        if (m_rowMode == TimelineRowMode::ByChannel)
        {
            row.label = QString("CH %1").arg(row.rowKey);
        }
        else if (isBroadcastMac(row.rowKey))
        {
            row.label = "Broadcast";
        }
        else
        {
            row.label = QString("NODE %1").arg(nodeIndexMap.value(row.rowKey));
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
    if (ppdu.recordType == RecordType::PhyState)
    {
        PpduVisualItem fixed = ppdu;
        if (fixed.nodeId <= 0)
            fixed.nodeId = 1;
        m_phyStateItems.append(fixed);
        if (m_ppduItems.isEmpty() && m_phyStateItems.size() == 1)
            m_viewStartNs = fixed.phyStateStartNs;
        scheduleDataUpdate();
        return;
    }

    static uint64_t ppduCount = 0;
    ppduCount++;

    if (!g_ppduViewConfig.preciseMode.load() && !g_ppduViewConfig.absoluteRate.load())
    {
        int rate = g_ppduViewConfig.sampleRate.load();
        if (rate > 1 && (ppduCount % rate != 0))
            return;
    }

    PpduVisualItem fixed = ppdu;
    if (fixed.nodeId <= 0)
        fixed.nodeId = 1;

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
    m_cachedPpduLayout.clear();
    m_cachedPpduRows.clear();
    m_ppduLayoutDirty = true;
    m_hoverIndex = -1;
    m_overlay->close();
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
    painter.fillRect(rect(), QColor(220, 225, 232));

    if (m_viewMode == TimelineViewMode::PhyStateTimeline)
    {
        paintPhyStateTimeline(painter);
        return;
    }

    paintPpduTimeline(painter);
}

void PpduTimelineView::paintPpduTimeline(QPainter &painter)
{
    ensurePpduLayoutCache();

    int apCnt = m_cachedPpduRows.size();
    if (apCnt == 0)
        return;

    int availH = height() - m_topMargin - kBottomMargin;
    int rowH = std::clamp(availH / apCnt, 18, 80);
    int topY = m_topMargin;
    if (availH > apCnt * rowH)
        topY += (availH - apCnt * rowH) / 2;

    // ==================== Step 2: 绘制背景 hover ====================
    if (m_hoverIndex >= 0 && m_hoverIndex < m_cachedPpduLayout.size())
    {
        int row = m_cachedPpduLayout[m_hoverIndex].row;
        if (row >= 0)
        {
            painter.fillRect(
                QRectF(0, topY + row * rowH, width(), rowH),
                QColor(255, 235, 205, 120));
        }
    }

    // ==================== Step 3: 绘制网格线 ====================
    painter.setPen(QColor(200, 200, 200));
    for (int r = 0; r <= apCnt; ++r)
    {
        int y = topY + r * rowH;
        painter.drawLine(m_leftMargin, y, width(), y);
    }

    painter.setPen(Qt::black);
    for (int rowIndex = 0; rowIndex < m_cachedPpduRows.size(); ++rowIndex)
    {
        const auto& rowInfo = m_cachedPpduRows[rowIndex];
        int row = rowIndex;
        int y = topY + row * rowH + rowH / 2;

        /* Draw AP/Channel labels */
        painter.drawText(8, y + 5, rowInfo.label);

        // -------- hover 显示信息（AP: MAC；Channel: 频段） --------
        QRect labelRect(0, y - rowH / 2, m_leftMargin - 10, rowH);

        if (labelRect.contains(m_mousePos)) // m_mousePos 在 mouseMoveEvent 里更新
        {
            if (m_rowMode == TimelineRowMode::ByChannel)
            {
                // 按信道视图：根据信道号显示 2.4G / 5G
                int ch = static_cast<int>(rowInfo.rowKey);

                QString band;
                if (ch >= 1 && ch <= 14)
                    band = "2.4G";
                else
                    band = "5G";

                QString tip = QString("Channel %1 (%2)").arg(ch).arg(band);

                QToolTip::showText(
                    mapToGlobal(QPoint(8, y)),
                    tip,
                    this);
            }
            else
            {
                // 按 AP 视图：显示 MAC 地址（广播显示 Broadcast）
                QString mac = isBroadcastMac(rowInfo.rowKey) ? "Broadcast" : formatMac(rowInfo.rowKey);

                QToolTip::showText(
                    mapToGlobal(QPoint(8, y)),
                    mac,
                    this);
            }
        }
    }
    painter.setPen(QColor(180, 180, 180));
    painter.drawLine(m_leftMargin - 8, topY, m_leftMargin - 8, topY + apCnt * rowH);

    // ==================== Step 4: 绘制 PPDU ====================
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

            QColor baseColor = QColor("#1f77b4");
            if (idx == m_hoverIndex)
                baseColor = Qt::red;

            painter.setPen(Qt::NoPen);
            painter.setBrush(baseColor);
            painter.drawRoundedRect(r, 3, 3);

            if (layout.overlap)
            {
                painter.setBrush(QColor(255, 127, 14, 180));
                painter.drawRoundedRect(r, 3, 3);
            }

            painter.setPen(QColor(60, 60, 60));
            painter.setBrush(Qt::NoBrush);
            painter.drawRoundedRect(r, 3, 3);
        }
    }

    uint64_t startNs = m_viewStartNs;
    uint64_t endNs = m_viewStartNs + width() / m_nsToPixel;

    QPen gridPen(QColor(180, 180, 180));
    gridPen.setStyle(Qt::DashLine);
    painter.setPen(gridPen);

    for (int i = 0; i <= 10; ++i)
    {
        uint64_t ns = startNs + i * (endNs - startNs) / 10;
        int x = m_leftMargin + (ns - m_viewStartNs) * m_nsToPixel;

        painter.drawLine(x, topY, x, topY + apCnt * rowH);
        painter.drawText(x - 20,
                         topY + apCnt * rowH + 15,
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
                apCnt * rowH);

            painter.setPen(QPen(kSelectBorder, 1, Qt::DashLine));
            painter.setBrush(kSelectFill);
            painter.drawRect(selRect);
        }
    }

    /* ================= Time Range Slider ================= */

    int barY = height() - kBottomMargin + 10;
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

    const int barY = height() - kBottomMargin + 10;
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

void PpduTimelineView::paintPhyStateTimeline(QPainter &painter)
{
    if (m_phyStateItems.isEmpty())
        return;

    const QVector<PhyStateSegment> segments = collectPhyStateSegments(m_phyStateItems);
    if (segments.isEmpty())
        return;

    QMap<uint64_t, int> rowMap;
    QMap<uint64_t, QString> rowLabelMap;
    uint64_t globalStartNs = std::numeric_limits<uint64_t>::max();
    uint64_t globalEndNs = 0;

    for (const auto &segment : segments)
    {
        if (!rowMap.contains(segment.rowKey))
        {
            rowMap[segment.rowKey] = rowMap.size();
            rowLabelMap[segment.rowKey] =
                QString("N%1 / CH%2").arg(segment.nodeId).arg(segment.channel);
        }
        globalStartNs = std::min(globalStartNs, segment.startNs);
        globalEndNs = std::max(globalEndNs, segment.endNs);
    }

    if (globalStartNs >= globalEndNs)
        return;

    const int rowCount = rowMap.size();
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
    for (auto it = rowMap.cbegin(); it != rowMap.cend(); ++it)
    {
        const int row = it.value();
        const int y = topY + row * rowH + rowH / 2;
        painter.drawText(8, y + 5, rowLabelMap.value(it.key()));
    }

    painter.setPen(QColor(180, 180, 180));
    painter.drawLine(m_leftMargin - 8, topY, m_leftMargin - 8, topY + rowCount * rowH);

    const uint64_t visibleStartNs = std::max<uint64_t>(0, m_viewStartNs);
    const uint64_t visibleEndNs =
        visibleStartNs + std::max<int>(1, width()) / m_nsToPixel;

    for (const auto &segment : segments)
    {
        const uint64_t drawStartNs = std::max(segment.startNs, visibleStartNs);
        const uint64_t drawEndNs = std::min(segment.endNs, visibleEndNs);
        if (drawStartNs >= drawEndNs)
            continue;

        const int row = rowMap.value(segment.rowKey, -1);
        if (row < 0)
            continue;

        QRectF rect(
            m_leftMargin + (drawStartNs - visibleStartNs) * m_nsToPixel,
            topY + row * rowH + kLanePadding,
            std::max(1.0, (drawEndNs - drawStartNs) * m_nsToPixel),
            rowH - 2 * kLanePadding);

        const auto state = segment.state;
        painter.setPen(Qt::NoPen);
        painter.setBrush(phyStateColor(state));
        painter.drawRoundedRect(rect, 3, 3);

        painter.setPen(QColor(70, 70, 70, 100));
        painter.setBrush(Qt::NoBrush);
        painter.drawRoundedRect(rect, 3, 3);

        if (rect.width() >= 62)
        {
            painter.setPen(state == PhyStateKindView::Idle ? QColor(40, 40, 40) : Qt::white);
            painter.drawText(rect.adjusted(6, 0, -6, 0),
                             Qt::AlignVCenter | Qt::AlignLeft,
                             phyStateName(state));
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

    const int barY = height() - kBottomMargin + 10;
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
    m_btnLegend->move(width() - 34, 8);

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

    m_hoverIndex = -1;
    m_overlay->close();
    update();
}

/* ======================== Interaction ======================== */

void PpduTimelineView::wheelEvent(QWheelEvent *event)
{
    double factor = (event->angleDelta().y() > 0) ? 1.2 : 0.8;
    m_nsToPixel = std::clamp(m_nsToPixel * factor, 1e-9, 1e-4);
    update();
}

void PpduTimelineView::mousePressEvent(QMouseEvent *e)
{
    m_showingStats = false;
    m_overlay->close();
    QToolTip::hideText();

    int barY = height() - kBottomMargin + 10;
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
    m_overlay->close();

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
        m_dragging = true;
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
    if (m_rangeDragging)
    {
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
        m_selectEnd = e->pos();
        update();
        return;
    }

    if (m_dragging)
    {
        int dx = e->pos().x() - m_lastMousePos.x();
        m_lastMousePos = e->pos();

        if (qAbs((int)dx) > 0)
        {
            m_viewStartNs = std::max<int64_t>(
                0, m_viewStartNs - dx / m_nsToPixel);
            update();
        }

        m_overlay->close();
        return;
    }

    m_mousePos = e->pos();

    if (m_viewMode == TimelineViewMode::PhyStateTimeline)
    {
        m_hoverIndex = -1;
        if (!showPhyStateHover(e->pos()))
            m_overlay->close();
        update();
        return;
    }

    /* ===== hover ===== */
    int idx = hitTest(e->pos());

    m_hoverIndex = idx;
    update();

    if (idx >= 0)
    {
        const auto &it = m_ppduItems[idx];

        QString senderMac = formatMac(it.sender);
        QString receiverMac = isBroadcastMac(it.receiver) ? "Broadcast" : formatMac(it.receiver);

        QString text =
            QString("Node ID: %1\n"
                    "Start:   %2 ms\n"
                    "End:     %3 ms\n"
                    "Duration: %4 us\n"
                    "MPDU:    %5\n"
                    "Type:    %6\n"
                    "Flow:    %7 -> %8")
                .arg(it.nodeId)
                .arg(it.txStartNs / 1e6, 0, 'f', 3)
                .arg(it.txEndNs / 1e6, 0, 'f', 3)
                .arg((it.txEndNs - it.txStartNs) / 1e3, 0, 'f', 1)
                .arg(it.mpduAggregation)
                .arg(QString::fromStdString(it.frameType))
                .arg(senderMac)
                .arg(receiverMac);

        m_overlay->setText(text);
        m_overlay->showAt(mapToGlobal(e->pos()) + QPoint(12, 12));
    }
    else
    {
        m_overlay->close();
    }
    update();
}

void PpduTimelineView::mouseReleaseEvent(QMouseEvent *e)
{

    if (e->button() == Qt::LeftButton && m_dragging)
    {
        m_dragging = false;
        setCursor(Qt::ArrowCursor);
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
            if (m_viewMode == TimelineViewMode::PpduTimeline)
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
        }

        m_hoverIndex = -1;
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
    m_hoverIndex = -1;
    m_overlay->close();
    QToolTip::hideText();
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

bool PpduTimelineView::showPhyStateHover(const QPoint &pos)
{
    if (m_phyStateItems.isEmpty())
        return false;

    const QVector<PhyStateSegment> segments = collectPhyStateSegments(m_phyStateItems);
    if (segments.isEmpty())
        return false;

    QMap<uint64_t, int> rowMap;
    QMap<uint64_t, QString> rowLabelMap;
    uint64_t globalStartNs = std::numeric_limits<uint64_t>::max();
    uint64_t globalEndNs = 0;

    for (const auto &segment : segments)
    {
        if (!rowMap.contains(segment.rowKey))
        {
            rowMap[segment.rowKey] = rowMap.size();
            rowLabelMap[segment.rowKey] =
                QString("Node %1 / Link CH %2").arg(segment.nodeId).arg(segment.channel);
        }
        globalStartNs = std::min(globalStartNs, segment.startNs);
        globalEndNs = std::max(globalEndNs, segment.endNs);
    }

    if (globalStartNs >= globalEndNs)
        return false;

    const int rowCount = rowMap.size();
    const int availH = height() - m_topMargin - kBottomMargin;
    const int rowH = std::clamp(availH / std::max(1, rowCount), 22, 84);
    int topY = m_topMargin;
    if (availH > rowCount * rowH)
        topY += (availH - rowCount * rowH) / 2;

    const uint64_t visibleStartNs = std::max<uint64_t>(0, m_viewStartNs);
    const uint64_t visibleEndNs =
        visibleStartNs + std::max<int>(1, width()) / m_nsToPixel;

    for (auto it = rowMap.cbegin(); it != rowMap.cend(); ++it)
    {
        const int row = it.value();
        const QRect labelRect(0, topY + row * rowH, m_leftMargin - 10, rowH);
        if (labelRect.contains(pos))
        {
            m_overlay->setText(rowLabelMap.value(it.key()));
            m_overlay->showAt(mapToGlobal(pos) + QPoint(12, 12));
            return true;
        }
    }

    for (const auto &segment : segments)
    {
        const uint64_t drawStartNs = std::max(segment.startNs, visibleStartNs);
        const uint64_t drawEndNs = std::min(segment.endNs, visibleEndNs);
        if (drawStartNs >= drawEndNs)
            continue;

        const int row = rowMap.value(segment.rowKey, -1);
        if (row < 0)
            continue;

        const QRectF rect(
            m_leftMargin + (drawStartNs - visibleStartNs) * m_nsToPixel,
            topY + row * rowH + kLanePadding,
            std::max(1.0, (drawEndNs - drawStartNs) * m_nsToPixel),
            rowH - 2 * kLanePadding);

        if (!rect.contains(pos))
            continue;

        const QString text =
            QString("Node: %1\n"
                    "Link: CH %2\n"
                    "State: %3\n"
                    "Start: %4 ms\n"
                    "End: %5 ms\n"
                    "Duration: %6 us")
                .arg(segment.nodeId)
                .arg(segment.channel)
                .arg(phyStateName(segment.state))
                .arg(segment.startNs / 1e6, 0, 'f', 3)
                .arg(segment.endNs / 1e6, 0, 'f', 3)
                .arg((segment.endNs - segment.startNs) / 1e3, 0, 'f', 1);

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
    m_cachedPpduLayout.clear();
    m_cachedPpduRows.clear();
    m_ppduLayoutDirty = true;
    m_dataUpdateQueued = false;

    /* ===== view state ===== */
    m_viewStartNs = 0;
    m_nsToPixel = 1e-6;

    Num_ap = 0;
    Num_sta = 0;

    /* ===== hover / selection ===== */
    m_hoverIndex = -1;
    m_hasSelection = false;
    m_showingStats = false;

    m_selecting = false;
    m_dragging = false;

    /* ===== mouse / drag ===== */
    m_dragLeftHandle = false;
    m_dragRightHandle = false;
    m_dragRangeBody = false;
    m_rangeDragging = false;

    /* ===== time range slider ===== */
    m_rangeStart = 0.0;
    m_rangeEnd = 1.0;
    m_lastRangeX = 0;

    /* ===== view mode ===== */
    m_rowMode = TimelineRowMode::ByAp;
    m_viewMode = TimelineViewMode::PpduTimeline;

    /* ===== overlays ===== */
    if (m_overlay)
        m_overlay->close();

    if (m_legendOverlay)
        m_legendOverlay->close();

    /* ===== cursor ===== */
    unsetCursor();
    updateModeButton();
    QToolTip::hideText();

    update();

    qDebug() << "[PpduTimelineView] resetPage() done";
}
