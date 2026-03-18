#include "throughput_chart.h"

#include <QPainter>
#include <QPainterPath>
#include <QMouseEvent>
#include <QToolTip>
#include <QEnterEvent>
#include <QTimer>
#include <algorithm>
#include <cmath>

namespace {
static const QColor kChartBg(245, 246, 248);
static const QColor kGrid(210, 215, 220);
static const QColor kLine(80, 140, 255);
static const QColor kAxis(120, 120, 120);
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
    m_updateQueued = false;
    m_hovering = false;
    m_hoverIndex = -1;
    m_viewStartIndex = 0;
    m_viewCount = 0;
    m_yZoom = 1.0;
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

void ThroughputChartWidget::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);
    p.fillRect(rect(), kChartBg);

    const int leftPad = 50;
    const int rightPad = 12;
    const int topPad = 12;
    const int bottomPad = 24;

    if (m_viewCount == 0)
        updateScrollBars();

    QRect plot(leftPad, topPad,
               width() - leftPad - rightPad - (m_vScroll ? m_vScroll->sizeHint().width() : 0),
               height() - topPad - bottomPad - (m_hScroll ? m_hScroll->sizeHint().height() : 0));

    if (plot.width() <= 10 || plot.height() <= 10)
        return;

    int start = m_viewStartIndex;
    int end = viewEndIndex();
    if (start < 0 || end < start)
        return;

    double maxMbps = 0.0;
    for (int i = start; i <= end; ++i)
        maxMbps = std::max(maxMbps, sampleMbps(i));

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
    p.setPen(QPen(kAxis, 1));
    p.drawRect(plot);

    // Labels
    p.setPen(kAxis);
    p.drawText(6, plot.top() + 12, "Throughput (Mbps)");

    // Y-axis labels
    for (int i = 0; i <= hLines; ++i)
    {
        double v = maxMbps * (1.0 - double(i) / hLines);
        int y = plot.top() + (plot.height() * i) / hLines;
        p.drawText(6, y + 4, QString::number(v, 'f', 1));
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
    QRect legendRect(plot.right() - 110, plot.top() + 6, 100, 18);
    p.setPen(QPen(kLine, 2));
    p.drawLine(legendRect.left(), legendRect.center().y(),
               legendRect.left() + 18, legendRect.center().y());
    p.setPen(kAxis);
    p.drawText(legendRect.left() + 24, legendRect.top() + 12, "Throughput");

    // Line
    const int n = std::max(1, end - start + 1);
    if (n >= 2)
    {
        QPainterPath path;
        for (int i = 0; i < n; ++i)
        {
            double v = sampleMbps(start + i);
            double ratio = std::clamp(v / maxMbps, 0.0, 1.0);
            int x = plot.left() + (plot.width() * i) / std::max(1, n - 1);
            int y = plot.bottom() - int(plot.height() * ratio);
            if (i == 0)
                path.moveTo(x, y);
            else
                path.lineTo(x, y);
        }

        p.setPen(QPen(kLine, 2));
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

    // Hover highlight
    if (m_hovering && m_hoverIndex >= 0 && m_hoverIndex < m_sampleTimeNs.size())
    {
        double v = sampleMbps(m_hoverIndex);
        double ratio = std::clamp(v / maxMbps, 0.0, 1.0);
        int rel = m_hoverIndex - start;
        int x = plot.left() + (plot.width() * rel) / std::max(1, n - 1);
        int y = plot.bottom() - int(plot.height() * ratio);

        p.setPen(Qt::NoPen);
        p.setBrush(kLine);
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

        QString tip = QString("Time: %1 ms\nThroughput: %2 Mbps")
                          .arg(tMs)
                          .arg(mbps, 0, 'f', 2);

        if (m_hoverIndex < m_sampleItems.size())
        {
            const auto &s = m_sampleItems[m_hoverIndex];
            if (s.txStartNs != 0 || s.txEndNs != 0)
            {
                tip += QString("\nNode: %1\nRealtime: %2 Mbps\nSender: 0x%3\nReceiver: 0x%4")
                           .arg(s.nodeId)
                           .arg(s.throughputMbpsX100 / 100.0, 0, 'f', 2)
                           .arg(QString::number(s.sender, 16).toUpper())
                           .arg(QString::number(s.receiver, 16).toUpper());
            }
        }

        QToolTip::showText(mapToGlobal(QPoint(px + 10, py - 10)), tip, this);
    }

    scheduleUpdate();
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
    m_viewCount = std::min(count, visibleCountForWidth(plotWidth));

    int maxStart = std::max(0, count - m_viewCount);
    m_hScroll->setRange(0, maxStart);
    m_hScroll->setPageStep(std::max(1, m_viewCount));
    m_hScroll->setSingleStep(1);
    m_viewStartIndex = std::clamp(m_viewStartIndex, 0, maxStart);
    m_hScroll->setValue(m_viewStartIndex);

    m_vScroll->setRange(0, 100);
    m_vScroll->setPageStep(10);
    m_vScroll->setSingleStep(1);
}
