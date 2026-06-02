#include "throughput_chart.h"

#include <QPainter>
#include <QPainterPath>
#include <QMouseEvent>
#include <QToolTip>
#include <QEnterEvent>
#include <QWheelEvent>
#include <QTimer>
#include <algorithm>
#include <cmath>

namespace {
static const QColor kPanelBg(244, 248, 252);
static const QColor kPlotBg(250, 252, 255);
static const QColor kFrame(205, 214, 226);
static const QColor kGrid(222, 229, 238);
static const QColor kLine(53, 131, 255);
static const QColor kFillTop(53, 131, 255, 120);
static const QColor kFillBottom(53, 131, 255, 8);
static const QColor kAvgLine(18, 184, 134);
static const QColor kAxis(84, 96, 113);
static const QColor kTextPrimary(34, 43, 56);
static const QColor kTextMuted(103, 116, 131);
static constexpr quint64 kBroadcastMac = 0xFFFFFFFFFFFFULL;

static bool isBroadcastLikeMac(quint64 mac)
{
    const quint64 low48 = mac & 0xFFFFFFFFFFFFULL;
    return low48 == 0 || low48 == kBroadcastMac;
}
}

ThroughputChartWidget::ThroughputChartWidget(QWidget *parent)
    : QWidget(parent)
{
    setMinimumHeight(160);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    setMouseTracking(true);
    setFocusPolicy(Qt::StrongFocus);

    m_hScroll = new QScrollBar(Qt::Horizontal, this);
    m_vScroll = new QScrollBar(Qt::Vertical, this);

    connect(m_hScroll, &QScrollBar::valueChanged, this, [this](int value) {
        m_viewStartIndex = value;
        update();
    });

    connect(m_vScroll, &QScrollBar::valueChanged, this, [this](int value) {
        m_yZoom = 1.0 + (double(value) / 100.0) * 4.0; // 1x ~ 5x
        update();
    });
}

void ThroughputChartWidget::setBucketDurationNs(uint64_t bucketNs)
{
    if (bucketNs != 0)
        m_bucketNs = bucketNs;
}

void ThroughputChartWidget::setMaxBuckets(int maxBuckets)
{
    if (maxBuckets <= 0 || maxBuckets == m_maxBuckets)
        return;
    m_maxBuckets = maxBuckets;

    while (m_sampleTimeNs.size() > m_maxBuckets)
    {
        m_sampleTimeNs.removeFirst();
        m_sampleThroughputX100.removeFirst();
        m_sampleItems.removeFirst();
    }

    if (m_hoverIndex >= m_sampleTimeNs.size())
        m_hoverIndex = -1;

    updateScrollBars();
    update();
}

void ThroughputChartWidget::reset()
{
    m_sampleTimeNs.clear();
    m_sampleThroughputX100.clear();
    m_sampleItems.clear();
    m_roleByMac.clear();
    m_apOrder.clear();
    m_staOrder.clear();
    m_devOrder.clear();
    m_updateQueued = false;
    m_hovering = false;
    m_hoverIndex = -1;
    m_viewStartIndex = 0;
    m_viewCount = 0;
    m_xZoom = 1.0;
    m_yZoom = 1.0;
    m_totalThroughputX100Sum = 0;
    m_totalSampleCount = 0;
    if (m_hScroll)
        m_hScroll->setValue(0);
    if (m_vScroll)
        m_vScroll->setValue(0);
    update();
}

void ThroughputChartWidget::scheduleUpdate()
{
    if (m_updateQueued)
        return;

    m_updateQueued = true;
    QTimer::singleShot(16, this, [this]() {
        m_updateQueued = false;
        update();
    });
}

void ThroughputChartWidget::appendPpdu(const PpduVisualItem &ppdu)
{
    if (ppdu.recordType == RecordType::DeviceRole && ppdu.roleMac != 0)
    {
        if (isBroadcastLikeMac(ppdu.roleMac))
            return;

        m_roleByMac[ppdu.roleMac] = ppdu.deviceRole;
        auto appendUnique = [](QVector<quint64>& items, quint64 mac) {
            if (!items.contains(mac))
                items.push_back(mac);
        };
        if (ppdu.deviceRole == DeviceRole::Ap)
            appendUnique(m_apOrder, ppdu.roleMac);
        else if (ppdu.deviceRole == DeviceRole::Sta)
            appendUnique(m_staOrder, ppdu.roleMac);
        else
            appendUnique(m_devOrder, ppdu.roleMac);
        return;
    }

    if (ppdu.recordType == RecordType::PpduUpdate)
    {
        for (int i = m_sampleItems.size() - 1; i >= 0; --i)
        {
            if (m_sampleItems[i].id == ppdu.id)
            {
                m_totalThroughputX100Sum -= m_sampleThroughputX100[i];
                PpduVisualItem merged = ppdu;
                merged.recordType = RecordType::Ppdu;
                m_sampleItems[i] = merged;
                m_sampleTimeNs[i] = ppdu.txEndNs;
                m_sampleThroughputX100[i] = ppdu.throughputMbpsX100;
                m_totalThroughputX100Sum += m_sampleThroughputX100[i];
                scheduleUpdate();
                return;
            }
        }
        return;
    }

    if (ppdu.recordType != RecordType::Ppdu)
        return;

    uint64_t ns = ppdu.txEndNs;
    if (ns == 0)
        return;

    bool wasAtEnd = false;
    if (m_hScroll)
        wasAtEnd = (m_hScroll->value() == m_hScroll->maximum());

    m_sampleTimeNs.append(ns);
    m_sampleThroughputX100.append(ppdu.throughputMbpsX100);
    m_sampleItems.append(ppdu);
    m_totalThroughputX100Sum += ppdu.throughputMbpsX100;
    m_totalSampleCount++;

    while (m_sampleTimeNs.size() > m_maxBuckets)
    {
        m_sampleTimeNs.removeFirst();
        m_sampleThroughputX100.removeFirst();
        m_sampleItems.removeFirst();
        m_viewStartIndex = std::max(0, m_viewStartIndex - 1);
        if (m_hoverIndex >= 0)
            m_hoverIndex = std::max(-1, m_hoverIndex - 1);
    }

    updateScrollBars();

    if (wasAtEnd && m_hScroll)
        m_hScroll->setValue(m_hScroll->maximum());

    scheduleUpdate();
}

double ThroughputChartWidget::sampleMbps(int index) const
{
    if (index < 0 || index >= m_sampleThroughputX100.size())
        return 0.0;
    return m_sampleThroughputX100[index] / 100.0;
}

double ThroughputChartWidget::totalAverageMbps() const
{
    if (m_totalSampleCount == 0)
        return 0.0;
    return (double(m_totalThroughputX100Sum) / double(m_totalSampleCount)) / 100.0;
}

void ThroughputChartWidget::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);
    p.fillRect(rect(), kPanelBg);

    const QRect card = rect().adjusted(6, 6, -6, -6);
    p.setPen(Qt::NoPen);
    p.setBrush(Qt::white);
    p.drawRoundedRect(card, 18, 18);
    p.setPen(QPen(kFrame, 1));
    p.setBrush(Qt::NoBrush);
    p.drawRoundedRect(card, 18, 18);

    p.setPen(kTextPrimary);
    QFont titleFont = font();
    titleFont.setPointSize(std::max(11, titleFont.pointSize() + 1));
    titleFont.setBold(true);
    p.setFont(titleFont);
    p.drawText(card.adjusted(16, 12, -16, -12), Qt::AlignLeft | Qt::AlignTop, "Throughput");

    QFont bodyFont = font();
    bodyFont.setPointSize(std::max(9, bodyFont.pointSize() - 1));
    p.setFont(bodyFont);
    p.setPen(kTextMuted);
    p.drawText(card.adjusted(16, 36, -16, -12),
               Qt::AlignLeft | Qt::AlignTop,
               "Realtime per-PPDU throughput with zoom and hover details.");

    const int leftPad = card.left() + 52;
    const int rightPad = 16;
    const int topPad = card.top() + 74;
    const int bottomPad = 34;

    if (m_viewCount == 0)
        updateScrollBars();

    QRect plot(leftPad, topPad,
               card.width() - (leftPad - card.left()) - rightPad -
                   (m_vScroll ? m_vScroll->sizeHint().width() : 0),
               card.height() - (topPad - card.top()) - bottomPad -
                   (m_hScroll ? m_hScroll->sizeHint().height() : 0));

    if (plot.width() <= 10 || plot.height() <= 10)
        return;

    p.setPen(Qt::NoPen);
    p.setBrush(kPlotBg);
    p.drawRoundedRect(plot.adjusted(0, 0, 0, 0), 12, 12);

    if (m_sampleTimeNs.isEmpty())
    {
        p.setPen(kTextMuted);
        p.drawText(plot, Qt::AlignCenter, "Waiting for PPDU throughput samples...");
        return;
    }

    int start = m_viewStartIndex;
    int end = viewEndIndex();
    if (start < 0 || end < start)
        return;

    double maxMbps = 0.0;
    for (int i = start; i <= end; ++i)
        maxMbps = std::max(maxMbps, sampleMbps(i));
    const double avgMbps = totalAverageMbps();
    maxMbps = std::max(maxMbps, avgMbps);

    if (maxMbps <= 0.01)
        maxMbps = 1.0;

    maxMbps /= std::max(1.0, m_yZoom);

    // Grid
    p.setPen(QPen(kGrid, 1));
    const int hLines = 4;
    for (int i = 0; i <= hLines; ++i)
    {
        int y = plot.top() + (plot.height() * i) / hLines;
        p.drawLine(plot.left(), y, plot.right(), y);
    }

    // Axis
    p.setPen(QPen(kFrame, 1));
    p.drawRoundedRect(plot, 12, 12);

    // Labels
    p.setPen(kTextMuted);
    p.drawText(card.left() + 12, plot.top() - 6, "Throughput (Mbps)");

    // Y-axis labels
    for (int i = 0; i <= hLines; ++i)
    {
        double v = maxMbps * (1.0 - double(i) / hLines);
        int y = plot.top() + (plot.height() * i) / hLines;
        p.drawText(card.left() + 8, y + 4, QString::number(v, 'f', 1));
    }

    // X-axis labels (time)
    if (!m_sampleTimeNs.isEmpty())
    {
        int n = std::max(1, end - start + 1);
        int step = std::max(1, n / 4);
        for (int i = 0; i < n; i += step)
        {
            int idx = start + i;
            uint64_t tMs = m_sampleTimeNs[idx] / 1000000;
            int x = plot.left() + (plot.width() * i) / std::max(1, n - 1);
            p.drawText(x - 12, plot.bottom() + 16, QString::number(tMs) + " ms");
        }
    }

    // Legend
    QRect legendRect(plot.right() - 136, plot.top() + 10, 126, 36);
    p.setPen(Qt::NoPen);
    p.setBrush(QColor(255, 255, 255, 220));
    p.drawRoundedRect(legendRect, 10, 10);
    p.setPen(QPen(kFrame, 1));
    p.drawRoundedRect(legendRect, 10, 10);
    p.setPen(QPen(kLine, 2));
    p.drawLine(legendRect.left() + 10, legendRect.top() + 13,
               legendRect.left() + 28, legendRect.top() + 13);
    p.setPen(kTextMuted);
    p.drawText(legendRect.left() + 34, legendRect.top() + 18, "Throughput");
    p.setPen(QPen(kAvgLine, 2, Qt::DashLine));
    p.drawLine(legendRect.left() + 10, legendRect.top() + 26,
               legendRect.left() + 28, legendRect.top() + 26);
    p.setPen(kTextMuted);
    p.drawText(legendRect.left() + 34, legendRect.top() + 31, "Average");

    // Line
    const int n = std::max(1, end - start + 1);
    if (n >= 2)
    {
        QPainterPath path;
        QPainterPath fillPath;
        for (int i = 0; i < n; ++i)
        {
            double v = sampleMbps(start + i);
            double ratio = std::clamp(v / maxMbps, 0.0, 1.0);
            int x = plot.left() + (plot.width() * i) / std::max(1, n - 1);
            int y = plot.bottom() - int(plot.height() * ratio);
            if (i == 0)
            {
                path.moveTo(x, y);
                fillPath.moveTo(x, plot.bottom());
                fillPath.lineTo(x, y);
            }
            else
            {
                path.lineTo(x, y);
                fillPath.lineTo(x, y);
            }
        }
        fillPath.lineTo(plot.right(), plot.bottom());
        fillPath.closeSubpath();

        QLinearGradient fillGradient(plot.topLeft(), plot.bottomLeft());
        fillGradient.setColorAt(0.0, kFillTop);
        fillGradient.setColorAt(1.0, kFillBottom);
        p.setPen(Qt::NoPen);
        p.setBrush(fillGradient);
        p.drawPath(fillPath);

        p.setPen(QPen(kLine, 2.4, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
        p.setBrush(Qt::NoBrush);
        p.drawPath(path);
    }
    else if (n == 1)
    {
        double v = sampleMbps(start);
        double ratio = std::clamp(v / maxMbps, 0.0, 1.0);
        int x = plot.left();
        int y = plot.bottom() - int(plot.height() * ratio);
        p.setPen(Qt::NoPen);
        p.setBrush(kLine);
        p.drawEllipse(QPoint(x, y), 4, 4);
    }

    // Global average throughput reference line
    if (avgMbps > 0.0)
    {
        const double avgRatio = std::clamp(avgMbps / maxMbps, 0.0, 1.0);
        const int avgY = plot.bottom() - int(plot.height() * avgRatio);
        p.setPen(QPen(kAvgLine, 1.5, Qt::DashLine));
        p.drawLine(plot.left(), avgY, plot.right(), avgY);
        p.setPen(kAvgLine);
        p.drawText(plot.left() + 8, std::max(plot.top() + 16, avgY - 6),
                   QString("Avg: %1 Mbps").arg(avgMbps, 0, 'f', 2));
    }

    // Hover highlight
    if (m_hovering && m_hoverIndex >= 0 && m_hoverIndex < m_sampleTimeNs.size())
    {
        double v = sampleMbps(m_hoverIndex);
        double ratio = std::clamp(v / maxMbps, 0.0, 1.0);
        int rel = m_hoverIndex - start;
        int x = plot.left() + (plot.width() * rel) / std::max(1, n - 1);
        int y = plot.bottom() - int(plot.height() * ratio);

        p.setPen(Qt::NoPen);
        p.setBrush(Qt::white);
        p.drawEllipse(QPoint(x, y), 4, 4);

        p.setBrush(Qt::NoBrush);
        p.setPen(QPen(kLine, 2));
        p.drawEllipse(QPoint(x, y), 8, 8);
    }
}

int ThroughputChartWidget::indexFromX(int x, int plotLeft, int plotWidth) const
{
    if (m_sampleTimeNs.isEmpty() || plotWidth <= 0)
        return -1;

    double ratio = double(x - plotLeft) / double(plotWidth);
    ratio = std::clamp(ratio, 0.0, 1.0);
    int n = std::max(1, viewEndIndex() - m_viewStartIndex + 1);
    int idx = int(std::round(ratio * (n - 1)));
    int count = int(m_sampleTimeNs.size());
    return std::clamp(m_viewStartIndex + idx, 0, count - 1);
}

uint64_t ThroughputChartWidget::sampleTimeNs(int index) const
{
    if (index < 0 || index >= m_sampleTimeNs.size())
        return 0;
    return m_sampleTimeNs[index];
}

QString ThroughputChartWidget::labelForMac(quint64 mac) const
{
    if (isBroadcastLikeMac(mac))
        return "Broadcast";

    const DeviceRole role = m_roleByMac.value(mac, DeviceRole::Unknown);
    if (role == DeviceRole::Ap)
    {
        const int index = m_apOrder.indexOf(mac);
        if (index >= 0)
            return QString("AP_%1").arg(index + 1);
    }
    else if (role == DeviceRole::Sta)
    {
        const int index = m_staOrder.indexOf(mac);
        if (index >= 0)
            return QString("STA_%1").arg(index + 1);
    }
    else
    {
        const int index = m_devOrder.indexOf(mac);
        if (index >= 0)
            return QString("DEV_%1").arg(index + 1);
    }
    return QString("0x%1").arg(QString::number(mac, 16).toUpper());
}

void ThroughputChartWidget::mouseMoveEvent(QMouseEvent *event)
{
    const int leftPad = 50;
    const int rightPad = 12;
    const int topPad = 12;
    const int bottomPad = 24;

    QRect plot(leftPad, topPad,
               width() - leftPad - rightPad - (m_vScroll ? m_vScroll->sizeHint().width() : 0),
               height() - topPad - bottomPad - (m_hScroll ? m_hScroll->sizeHint().height() : 0));

    if (!plot.contains(event->pos()))
    {
        m_hovering = false;
        m_hoverIndex = -1;
        scheduleUpdate();
        return;
    }

    setFocus();
    m_hovering = true;
    m_hoverIndex = indexFromX(event->pos().x(), plot.left(), plot.width());

    if (m_hoverIndex >= 0)
    {
        double mbps = sampleMbps(m_hoverIndex);
        uint64_t tMs = sampleTimeNs(m_hoverIndex) / 1000000;

        // compute point position for tooltip
        double maxMbps = 0.0;
        int start = m_viewStartIndex;
        int end = viewEndIndex();
        for (int i = start; i <= end; ++i)
            maxMbps = std::max(maxMbps, sampleMbps(i));
        if (maxMbps <= 0.01)
            maxMbps = 1.0;
        maxMbps /= std::max(1.0, m_yZoom);
        double ratio = std::clamp(mbps / maxMbps, 0.0, 1.0);
        int n = std::max(1, end - start + 1);
        int rel = m_hoverIndex - start;
        int px = plot.left() + (plot.width() * rel) / std::max(1, n - 1);
        int py = plot.bottom() - int(plot.height() * ratio);

        QString tip = QString(
                          "<div style='min-width:190px;'>"
                          "<div style='font-size:14px;font-weight:700;color:#223043;'>Throughput Sample</div>"
                          "<div style='margin-top:6px;color:#5D6B7B;'>Time</div>"
                          "<div style='font-size:15px;font-weight:600;'>%1 ms</div>"
                          "<div style='margin-top:6px;color:#5D6B7B;'>Throughput</div>"
                          "<div style='font-size:15px;font-weight:600;color:#2E7BFF;'>%2 Mbps</div>")
                          .arg(tMs)
                          .arg(mbps, 0, 'f', 2);

        if (m_hoverIndex < m_sampleItems.size())
        {
            const auto &s = m_sampleItems[m_hoverIndex];
            if (s.txStartNs != 0 || s.txEndNs != 0)
            {
                tip += QString(
                           "<div style='margin-top:8px;border-top:1px solid #E7EDF3;padding-top:8px;'>"
                           "<table cellspacing='0' cellpadding='0' style='color:#314154;'>"
                           "<tr><td style='padding:0 12px 4px 0;color:#6A7A8C;'>Node</td><td style='font-weight:600;'>%1</td></tr>"
                           "<tr><td style='padding:0 12px 4px 0;color:#6A7A8C;'>Realtime</td><td style='font-weight:600;'>%2 Mbps</td></tr>"
                           "<tr><td style='padding:0 12px 4px 0;color:#6A7A8C;'>Sender</td><td>%3</td></tr>"
                           "<tr><td style='padding:0 12px 0 0;color:#6A7A8C;'>Receiver</td><td>%4</td></tr>"
                           "</table></div>")
                           .arg(s.nodeId)
                           .arg(s.throughputMbpsX100 / 100.0, 0, 'f', 2)
                           .arg(labelForMac(s.sender))
                           .arg(labelForMac(s.receiver));
            }
        }

        tip += "</div>";

        QToolTip::showText(mapToGlobal(QPoint(px + 10, py - 10)), tip, this);
    }

    scheduleUpdate();
}

void ThroughputChartWidget::wheelEvent(QWheelEvent *event)
{
    if (m_sampleTimeNs.isEmpty())
    {
        event->ignore();
        return;
    }

    const int leftPad = 50;
    const int rightPad = 12;
    const int topPad = 12;
    const int bottomPad = 24;

    QRect plot(leftPad, topPad,
               width() - leftPad - rightPad - (m_vScroll ? m_vScroll->sizeHint().width() : 0),
               height() - topPad - bottomPad - (m_hScroll ? m_hScroll->sizeHint().height() : 0));

    QPoint cursorPos = event->position().toPoint();
    if (!plot.contains(cursorPos) || plot.width() <= 0)
    {
        event->ignore();
        return;
    }

    int delta = event->angleDelta().y();
    if (std::abs(event->angleDelta().x()) > std::abs(delta))
        delta = event->angleDelta().x();
    if (delta == 0)
    {
        event->accept();
        return;
    }

    int count = int(m_sampleTimeNs.size());
    int oldViewCount = std::max(1, m_viewCount);
    int oldStart = m_viewStartIndex;

    double mouseRatio = double(cursorPos.x() - plot.left()) / double(plot.width());
    mouseRatio = std::clamp(mouseRatio, 0.0, 1.0);

    int anchorIndex =
        std::clamp(oldStart + int(std::round(mouseRatio * (oldViewCount - 1))), 0, count - 1);

    double stepCount = std::max(1.0, std::abs(double(delta)) / 120.0);
    double factor = std::pow(1.2, stepCount);
    if (delta > 0)
        m_xZoom *= factor;
    else
        m_xZoom /= factor;

    int plotWidth = width() - 50 - 12 - (m_vScroll ? m_vScroll->sizeHint().width() : 0);
    int baseVisible = std::max(1, std::min(count, visibleCountForWidth(plotWidth)));
    const int minVisible = 8;
    double minZoom = std::min(1.0, double(baseVisible) / double(std::max(1, count)));
    double maxZoom = std::max(1.0,
                              double(baseVisible) /
                                  double(std::max(1, std::min(count, minVisible))));
    m_xZoom = std::clamp(m_xZoom, minZoom, maxZoom);

    updateScrollBars();

    int maxStart = std::max(0, count - m_viewCount);
    int anchorOffset = int(std::round(mouseRatio * (std::max(1, m_viewCount) - 1)));
    m_viewStartIndex = std::clamp(anchorIndex - anchorOffset, 0, maxStart);

    if (m_hScroll)
        m_hScroll->setValue(m_viewStartIndex);

    scheduleUpdate();
    event->accept();
}

void ThroughputChartWidget::enterEvent(QEnterEvent *)
{
    setFocus();
}

void ThroughputChartWidget::leaveEvent(QEvent *)
{
    m_hovering = false;
    m_hoverIndex = -1;
    QToolTip::hideText();
    update();
}

void ThroughputChartWidget::resizeEvent(QResizeEvent *)
{
    if (!m_hScroll || !m_vScroll)
        return;

    int h = m_hScroll->sizeHint().height();
    int w = m_vScroll->sizeHint().width();

    m_hScroll->setGeometry(0, height() - h, width() - w, h);
    m_vScroll->setGeometry(width() - w, 0, w, height() - h);

    updateScrollBars();
}

int ThroughputChartWidget::visibleCountForWidth(int plotWidth) const
{
    int approx = std::max(64, plotWidth / 2);
    return std::max(1, approx);
}

int ThroughputChartWidget::viewEndIndex() const
{
    if (m_sampleTimeNs.isEmpty())
        return -1;
    int end = m_viewStartIndex + m_viewCount - 1;
    return std::clamp(end, 0, int(m_sampleTimeNs.size()) - 1);
}

void ThroughputChartWidget::updateScrollBars()
{
    if (!m_hScroll || !m_vScroll)
        return;

    int plotWidth = width() - 50 - 12 - m_vScroll->sizeHint().width();
    int count = int(m_sampleTimeNs.size());
    if (count <= 0)
    {
        m_viewCount = 0;
        m_viewStartIndex = 0;
        m_hScroll->setRange(0, 0);
        m_hScroll->setPageStep(1);
        m_hScroll->setSingleStep(1);
        m_hScroll->setValue(0);
    }
    else
    {
        int baseVisible = std::max(1, std::min(count, visibleCountForWidth(plotWidth)));
        const int minVisible = 8;
        double minZoom = std::min(1.0, double(baseVisible) / double(count));
        double maxZoom = std::max(1.0,
                                  double(baseVisible) /
                                      double(std::max(1, std::min(count, minVisible))));
        m_xZoom = std::clamp(m_xZoom, minZoom, maxZoom);

        int targetViewCount = int(std::round(double(baseVisible) / m_xZoom));
        m_viewCount = std::clamp(targetViewCount, 1, count);

        int maxStart = std::max(0, count - m_viewCount);
        m_hScroll->setRange(0, maxStart);
        m_hScroll->setPageStep(std::max(1, m_viewCount));
        m_hScroll->setSingleStep(1);
        m_viewStartIndex = std::clamp(m_viewStartIndex, 0, maxStart);
        m_hScroll->setValue(m_viewStartIndex);
    }

    m_vScroll->setRange(0, 100);
    m_vScroll->setPageStep(10);
    m_vScroll->setSingleStep(1);
}
