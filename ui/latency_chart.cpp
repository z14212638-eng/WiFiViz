#include "latency_chart.h"

#include <QEnterEvent>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QTimer>
#include <QToolTip>
#include <QWheelEvent>
#include <algorithm>
#include <cmath>

namespace
{
static const QColor kPanelBg(244, 248, 252);
static const QColor kPlotBg(250, 252, 255);
static const QColor kFrame(205, 214, 226);
static const QColor kAxis(84, 96, 113);
static const QColor kGrid(222, 229, 238);
static const QColor kLine(246, 116, 59);
static const QColor kFillTop(246, 116, 59, 115);
static const QColor kFillBottom(246, 116, 59, 10);
static const QColor kAvgLine(145, 86, 255);
static const QColor kTextPrimary(34, 43, 56);
static const QColor kTextMuted(103, 116, 131);
}

LatencyChartWidget::LatencyChartWidget(LatencyMetric metric, QWidget* parent)
    : QWidget(parent),
      m_metric(metric)
{
    setMinimumHeight(220);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    setMouseTracking(true);
    setFocusPolicy(Qt::StrongFocus);
    setAttribute(Qt::WA_StyledBackground, true);

    m_hScroll = new QScrollBar(Qt::Horizontal, this);
    m_vScroll = new QScrollBar(Qt::Vertical, this);

    connect(m_hScroll, &QScrollBar::valueChanged, this, [this](int value) {
        m_viewStartIndex = value;
        update();
    });

    connect(m_vScroll, &QScrollBar::valueChanged, this, [this](int value) {
        m_yZoom = 1.0 + (double(value) / 100.0) * 4.0;
        update();
    });
}

void
LatencyChartWidget::appendPpdu(const PpduVisualItem& ppdu)
{
    if (ppdu.recordType != RecordType::MacDelay)
    {
        return;
    }

    if ((m_metric == LatencyMetric::QueueingDelay && ppdu.delayKind != DelayKind::Queueing) ||
        (m_metric == LatencyMetric::MacEndToEndDelay && ppdu.delayKind != DelayKind::MacE2e))
    {
        return;
    }

    const uint64_t ns = ppdu.txEndNs;
    if (ns == 0)
    {
        return;
    }

    const uint64_t latencyNs = metricValueNs(ppdu);
    if (latencyNs == 0)
    {
        return;
    }

    bool wasAtEnd = false;
    if (m_hScroll)
    {
        wasAtEnd = (m_hScroll->value() == m_hScroll->maximum());
    }

    const uint32_t latencyUsX10 = static_cast<uint32_t>(std::max<uint64_t>(1, latencyNs / 100));
    m_sampleTimeNs.append(ns);
    m_sampleLatencyUsX10.append(latencyUsX10);
    m_sampleItems.append(ppdu);
    m_totalLatencyUsX10Sum += latencyUsX10;
    m_totalSampleCount++;

    while (m_sampleTimeNs.size() > 200000)
    {
        m_sampleTimeNs.removeFirst();
        m_sampleLatencyUsX10.removeFirst();
        m_sampleItems.removeFirst();
        m_viewStartIndex = std::max(0, m_viewStartIndex - 1);
        if (m_hoverIndex >= 0)
        {
            m_hoverIndex = std::max(-1, m_hoverIndex - 1);
        }
    }

    updateScrollBars();
    if (wasAtEnd && m_hScroll)
    {
        m_hScroll->setValue(m_hScroll->maximum());
    }
    scheduleUpdate();
}

void
LatencyChartWidget::reset()
{
    m_sampleTimeNs.clear();
    m_sampleLatencyUsX10.clear();
    m_sampleItems.clear();
    m_updateQueued = false;
    m_hovering = false;
    m_hoverIndex = -1;
    m_cdfHoverIndex = -1;
    m_viewStartIndex = 0;
    m_viewCount = 0;
    m_xZoom = 1.0;
    m_yZoom = 1.0;
    m_totalLatencyUsX10Sum = 0;
    m_totalSampleCount = 0;
    if (m_hScroll)
    {
        m_hScroll->setValue(0);
    }
    if (m_vScroll)
    {
        m_vScroll->setValue(0);
    }
    update();
}

void
LatencyChartWidget::setCdfMode(bool enabled)
{
    if (m_cdfMode == enabled)
    {
        return;
    }
    m_cdfMode = enabled;
    m_hovering = false;
    m_hoverIndex = -1;
    m_cdfHoverIndex = -1;
    updateScrollBarVisibility();
    update();
}

bool
LatencyChartWidget::cdfMode() const
{
    return m_cdfMode;
}

void
LatencyChartWidget::scheduleUpdate()
{
    if (m_updateQueued)
    {
        return;
    }
    m_updateQueued = true;
    QTimer::singleShot(16, this, [this]() {
        m_updateQueued = false;
        update();
    });
}

double
LatencyChartWidget::sampleUs(int index) const
{
    if (index < 0 || index >= m_sampleLatencyUsX10.size())
    {
        return 0.0;
    }
    return m_sampleLatencyUsX10[index] / 10.0;
}

double
LatencyChartWidget::averageUs() const
{
    if (m_totalSampleCount == 0)
    {
        return 0.0;
    }
    return (double(m_totalLatencyUsX10Sum) / double(m_totalSampleCount)) / 10.0;
}

void
LatencyChartWidget::paintEvent(QPaintEvent*)
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
    p.drawText(card.adjusted(16, 12, -16, -12), Qt::AlignLeft | Qt::AlignTop, titleText());

    QFont bodyFont = font();
    bodyFont.setPointSize(std::max(9, bodyFont.pointSize() - 1));
    p.setFont(bodyFont);
    p.setPen(kTextMuted);
    p.drawText(card.adjusted(16, 36, -16, -12),
               Qt::AlignLeft | Qt::AlignTop,
               subtitleText());

    if (m_viewCount == 0)
    {
        updateScrollBars();
    }

    updateScrollBarVisibility();
    const QRect plot = plotRectForCard(card);

    if (plot.width() <= 10 || plot.height() <= 10)
    {
        return;
    }

    p.setPen(Qt::NoPen);
    p.setBrush(kPlotBg);
    p.drawRoundedRect(plot.adjusted(0, 0, 0, 0), 12, 12);

    if (m_sampleTimeNs.isEmpty())
    {
        p.setPen(kTextMuted);
        p.drawText(plot, Qt::AlignCenter, "Waiting for PPDU delay samples...");
        return;
    }

    if (m_cdfMode)
    {
        paintCdf(p, card, plot);
        return;
    }

    paintTimeSeries(p, card, plot);
}

QRect
LatencyChartWidget::plotRectForCard(const QRect& card) const
{
    const int leftPad = card.left() + 58;
    const int rightPad = 18;
    const int topPad = card.top() + 74;
    const int bottomPad = 34;

    const int vScrollWidth = (!m_cdfMode && m_vScroll) ? m_vScroll->sizeHint().width() : 0;
    const int hScrollHeight = (!m_cdfMode && m_hScroll) ? m_hScroll->sizeHint().height() : 0;

    return QRect(leftPad,
                 topPad,
                 card.width() - (leftPad - card.left()) - rightPad - vScrollWidth,
                 card.height() - (topPad - card.top()) - bottomPad - hScrollHeight);
}

void
LatencyChartWidget::paintTimeSeries(QPainter& p, const QRect& card, const QRect& plot)
{
    const int start = m_viewStartIndex;
    const int end = viewEndIndex();
    if (start < 0 || end < start)
    {
        return;
    }

    double maxUs = 0.0;
    for (int i = start; i <= end; ++i)
    {
        maxUs = std::max(maxUs, sampleUs(i));
    }
    const double avgUs = averageUs();
    maxUs = std::max(maxUs, avgUs);
    if (maxUs <= 0.01)
    {
        maxUs = 1.0;
    }
    maxUs /= std::max(1.0, m_yZoom);

    const int hLines = 4;
    p.setPen(QPen(kGrid, 1));
    for (int i = 0; i <= hLines; ++i)
    {
        const int y = plot.top() + (plot.height() * i) / hLines;
        p.drawLine(plot.left(), y, plot.right(), y);
    }

    p.setPen(QPen(kFrame, 1));
    p.drawRoundedRect(plot, 12, 12);

    p.setPen(kTextMuted);
    p.drawText(card.left() + 16, plot.top() - 12, yAxisText());
    for (int i = 0; i <= hLines; ++i)
    {
        const double v = maxUs * (1.0 - double(i) / hLines);
        const int y = plot.top() + (plot.height() * i) / hLines;
        p.drawText(card.left() + 10, y + 4, QString::number(v, 'f', v < 10.0 ? 2 : 1));
    }

    const int n = std::max(1, end - start + 1);
    if (!m_sampleTimeNs.isEmpty())
    {
        const int step = std::max(1, n / 4);
        for (int i = 0; i < n; i += step)
        {
            const int idx = start + i;
            const uint64_t tMs = m_sampleTimeNs[idx] / 1000000;
            const int x = plot.left() + (plot.width() * i) / std::max(1, n - 1);
            p.drawText(x - 16, plot.bottom() + 18, QString::number(tMs) + " ms");
        }
    }

    if (n >= 2)
    {
        QPainterPath linePath;
        QPainterPath fillPath;

        for (int i = 0; i < n; ++i)
        {
            const double v = sampleUs(start + i);
            const double ratio = std::clamp(v / maxUs, 0.0, 1.0);
            const int x = plot.left() + (plot.width() * i) / std::max(1, n - 1);
            const int y = plot.bottom() - int(plot.height() * ratio);

            if (i == 0)
            {
                linePath.moveTo(x, y);
                fillPath.moveTo(x, plot.bottom());
                fillPath.lineTo(x, y);
            }
            else
            {
                linePath.lineTo(x, y);
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
        p.drawPath(linePath);
    }

    if (avgUs > 0.0)
    {
        const double avgRatio = std::clamp(avgUs / maxUs, 0.0, 1.0);
        const int avgY = plot.bottom() - int(plot.height() * avgRatio);
        p.setPen(QPen(kAvgLine, 1.5, Qt::DashLine));
        p.drawLine(plot.left(), avgY, plot.right(), avgY);
        p.setPen(kAvgLine);
        p.drawText(plot.left() + 8,
                   std::max(plot.top() + 16, avgY - 6),
                   QString("Avg %1 us").arg(avgUs, 0, 'f', avgUs < 10.0 ? 2 : 1));
    }

    if (m_hovering && m_hoverIndex >= 0 && m_hoverIndex < m_sampleTimeNs.size())
    {
        const double v = sampleUs(m_hoverIndex);
        const double ratio = std::clamp(v / maxUs, 0.0, 1.0);
        const int rel = m_hoverIndex - start;
        const int x = plot.left() + (plot.width() * rel) / std::max(1, n - 1);
        const int y = plot.bottom() - int(plot.height() * ratio);

        p.setPen(Qt::NoPen);
        p.setBrush(Qt::white);
        p.drawEllipse(QPoint(x, y), 5, 5);
        p.setPen(QPen(kLine, 2));
        p.setBrush(Qt::NoBrush);
        p.drawEllipse(QPoint(x, y), 8, 8);
    }
}

void
LatencyChartWidget::paintCdf(QPainter& p, const QRect& card, const QRect& plot)
{
    const QVector<double> sortedMs = sortedSampleMs();
    if (sortedMs.isEmpty())
    {
        p.setPen(kTextMuted);
        p.drawText(plot, Qt::AlignCenter, "Waiting for delay samples...");
        return;
    }

    const double maxMs = std::max(0.001, sortedMs.back());
    const int hLines = 4;
    p.setPen(QPen(kGrid, 1));
    for (int i = 0; i <= hLines; ++i)
    {
        const int y = plot.top() + (plot.height() * i) / hLines;
        p.drawLine(plot.left(), y, plot.right(), y);
    }
    for (int i = 0; i <= hLines; ++i)
    {
        const int x = plot.left() + (plot.width() * i) / hLines;
        p.drawLine(x, plot.top(), x, plot.bottom());
    }

    p.setPen(QPen(kFrame, 1));
    p.drawRoundedRect(plot, 12, 12);

    p.setPen(kTextMuted);
    p.drawText(card.left() + 12, plot.top() - 12, "CDF");
    for (int i = 0; i <= hLines; ++i)
    {
        const double probability = 1.0 - double(i) / hLines;
        const int y = plot.top() + (plot.height() * i) / hLines;
        p.drawText(card.left() + 12, y + 4, QString::number(probability, 'f', 2));
    }

    for (int i = 0; i <= hLines; ++i)
    {
        const double value = maxMs * double(i) / hLines;
        const int x = plot.left() + (plot.width() * i) / hLines;
        p.drawText(x - 18, plot.bottom() + 18, QString::number(value, 'f', value < 10.0 ? 2 : 1));
    }
    p.drawText(QRect(plot.left(), plot.bottom() + 26, plot.width(), 18),
               Qt::AlignHCenter | Qt::AlignTop,
               "Delay (ms)");

    QPainterPath linePath;
    QPainterPath fillPath;
    for (int i = 0; i < sortedMs.size(); ++i)
    {
        const double xRatio = std::clamp(sortedMs[i] / maxMs, 0.0, 1.0);
        const double probability = double(i + 1) / double(sortedMs.size());
        const int x = plot.left() + int(plot.width() * xRatio);
        const int y = plot.bottom() - int(plot.height() * probability);

        if (i == 0)
        {
            linePath.moveTo(plot.left(), plot.bottom());
            linePath.lineTo(x, y);
            fillPath.moveTo(plot.left(), plot.bottom());
            fillPath.lineTo(x, y);
        }
        else
        {
            linePath.lineTo(x, y);
            fillPath.lineTo(x, y);
        }
    }
    fillPath.lineTo(plot.right(), plot.top());
    fillPath.lineTo(plot.right(), plot.bottom());
    fillPath.closeSubpath();

    QLinearGradient fillGradient(plot.topLeft(), plot.bottomLeft());
    fillGradient.setColorAt(0.0, QColor(18, 184, 134, 90));
    fillGradient.setColorAt(1.0, QColor(18, 184, 134, 8));
    p.setPen(Qt::NoPen);
    p.setBrush(fillGradient);
    p.drawPath(fillPath);

    p.setPen(QPen(QColor(18, 184, 134), 2.6, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
    p.setBrush(Qt::NoBrush);
    p.drawPath(linePath);

    if (m_cdfHoverIndex >= 0 && m_cdfHoverIndex < sortedMs.size())
    {
        const double ms = sortedMs[m_cdfHoverIndex];
        const double probability = double(m_cdfHoverIndex + 1) / double(sortedMs.size());
        const int x = plot.left() + int(plot.width() * std::clamp(ms / maxMs, 0.0, 1.0));
        const int y = plot.bottom() - int(plot.height() * probability);
        p.setPen(QPen(QColor(18, 184, 134, 140), 1.2, Qt::DashLine));
        p.drawLine(x, plot.top(), x, plot.bottom());
        p.drawLine(plot.left(), y, plot.right(), y);
        p.setPen(Qt::NoPen);
        p.setBrush(Qt::white);
        p.drawEllipse(QPoint(x, y), 5, 5);
        p.setPen(QPen(QColor(18, 184, 134), 2));
        p.setBrush(Qt::NoBrush);
        p.drawEllipse(QPoint(x, y), 8, 8);
    }
}

int
LatencyChartWidget::indexFromX(int x, int plotLeft, int plotWidth) const
{
    if (m_sampleTimeNs.isEmpty() || plotWidth <= 0)
    {
        return -1;
    }
    double ratio = double(x - plotLeft) / double(plotWidth);
    ratio = std::clamp(ratio, 0.0, 1.0);
    const int n = std::max(1, viewEndIndex() - m_viewStartIndex + 1);
    const int idx = int(std::round(ratio * (n - 1)));
    const int count = int(m_sampleTimeNs.size());
    return std::clamp(m_viewStartIndex + idx, 0, count - 1);
}

uint64_t
LatencyChartWidget::sampleTimeNs(int index) const
{
    if (index < 0 || index >= m_sampleTimeNs.size())
    {
        return 0;
    }
    return m_sampleTimeNs[index];
}

void
LatencyChartWidget::mouseMoveEvent(QMouseEvent* event)
{
    const QRect card = rect().adjusted(6, 6, -6, -6);
    const QRect plot = plotRectForCard(card);

    if (!plot.contains(event->pos()))
    {
        m_hovering = false;
        m_hoverIndex = -1;
        m_cdfHoverIndex = -1;
        scheduleUpdate();
        return;
    }

    setFocus();
    if (m_cdfMode)
    {
        const QVector<double> sortedMs = sortedSampleMs();
        m_cdfHoverIndex = cdfIndexFromX(event->pos().x(), plot, sortedMs);
        if (m_cdfHoverIndex >= 0 && m_cdfHoverIndex < sortedMs.size())
        {
            const double probability = double(m_cdfHoverIndex + 1) / double(sortedMs.size());
            QToolTip::showText(mapToGlobal(event->pos() + QPoint(14, -12)),
                               QString(
                                   "<div style='min-width:180px;'>"
                                   "<div style='font-size:14px;font-weight:700;color:#223043;'>%1 CDF</div>"
                                   "<div style='margin-top:6px;color:#5D6B7B;'>Delay</div>"
                                   "<div style='font-size:15px;font-weight:600;color:#12B886;'>%2 ms</div>"
                                   "<div style='margin-top:6px;color:#5D6B7B;'>Probability</div>"
                                   "<div style='font-size:15px;font-weight:600;'>%3</div>"
                                   "</div>")
                                   .arg(titleText())
                                   .arg(sortedMs[m_cdfHoverIndex], 0, 'f', sortedMs[m_cdfHoverIndex] < 10.0 ? 3 : 2)
                                   .arg(probability, 0, 'f', 3),
                               this);
        }
        scheduleUpdate();
        return;
    }

    m_hovering = true;
    m_hoverIndex = indexFromX(event->pos().x(), plot.left(), plot.width());
    if (m_hoverIndex >= 0)
    {
        const double us = sampleUs(m_hoverIndex);
        const uint64_t tMs = sampleTimeNs(m_hoverIndex) / 1000000;
        QString tip = QString(
                          "<div style='min-width:180px;'>"
                          "<div style='font-size:14px;font-weight:700;color:#223043;'>%1</div>"
                          "<div style='margin-top:6px;color:#5D6B7B;'>Time</div>"
                          "<div style='font-size:15px;font-weight:600;'>%2 ms</div>"
                          "<div style='margin-top:6px;color:#5D6B7B;'>Delay</div>"
                          "<div style='font-size:15px;font-weight:600;color:#F5743B;'>%3 us</div>")
                          .arg(tooltipTitleText())
                          .arg(tMs)
                          .arg(us, 0, 'f', us < 10.0 ? 2 : 1);
        if (m_hoverIndex < m_sampleItems.size())
        {
            const auto& s = m_sampleItems[m_hoverIndex];
            tip += QString(
                       "<div style='margin-top:8px;border-top:1px solid #E7EDF3;padding-top:8px;'>"
                       "<table cellspacing='0' cellpadding='0' style='color:#314154;'>"
                       "<tr><td style='padding:0 12px 4px 0;color:#6A7A8C;'>Node</td><td style='font-weight:600;'>%1</td></tr>"
                       "<tr><td style='padding:0 12px 0 0;color:#6A7A8C;'>Frame</td><td>%2</td></tr>"
                       "</table></div>")
                       .arg(s.nodeId)
                       .arg(QString::fromStdString(s.frameType));
        }
        tip += "</div>";
        QToolTip::showText(mapToGlobal(event->pos() + QPoint(14, -12)), tip, this);
    }
    scheduleUpdate();
}

void
LatencyChartWidget::wheelEvent(QWheelEvent* event)
{
    if (m_sampleTimeNs.isEmpty() || m_cdfMode)
    {
        event->ignore();
        return;
    }

    const QRect card = rect().adjusted(6, 6, -6, -6);
    const QRect plot = plotRectForCard(card);

    const QPoint cursorPos = event->position().toPoint();
    if (!plot.contains(cursorPos) || plot.width() <= 0)
    {
        event->ignore();
        return;
    }

    int delta = event->angleDelta().y();
    if (std::abs(event->angleDelta().x()) > std::abs(delta))
    {
        delta = event->angleDelta().x();
    }
    if (delta == 0)
    {
        event->accept();
        return;
    }

    const int count = int(m_sampleTimeNs.size());
    const int oldViewCount = std::max(1, m_viewCount);
    const int oldStart = m_viewStartIndex;

    double mouseRatio = double(cursorPos.x() - plot.left()) / double(plot.width());
    mouseRatio = std::clamp(mouseRatio, 0.0, 1.0);
    const int anchorIndex =
        std::clamp(oldStart + int(std::round(mouseRatio * (oldViewCount - 1))), 0, count - 1);

    const double stepCount = std::max(1.0, std::abs(double(delta)) / 120.0);
    const double factor = std::pow(1.2, stepCount);
    if (delta > 0)
    {
        m_xZoom *= factor;
    }
    else
    {
        m_xZoom /= factor;
    }

    const int plotWidth = plot.width();
    const int baseVisible = std::max(1, std::min(count, visibleCountForWidth(plotWidth)));
    const int minVisible = 8;
    const double minZoom = std::min(1.0, double(baseVisible) / double(std::max(1, count)));
    const double maxZoom = std::max(1.0,
                                    double(baseVisible) /
                                        double(std::max(1, std::min(count, minVisible))));
    m_xZoom = std::clamp(m_xZoom, minZoom, maxZoom);

    updateScrollBars();

    const int maxStart = std::max(0, count - m_viewCount);
    const int anchorOffset =
        int(std::round(mouseRatio * (std::max(1, m_viewCount) - 1)));
    m_viewStartIndex = std::clamp(anchorIndex - anchorOffset, 0, maxStart);
    if (m_hScroll)
    {
        m_hScroll->setValue(m_viewStartIndex);
    }

    scheduleUpdate();
    event->accept();
}

void
LatencyChartWidget::enterEvent(QEnterEvent*)
{
    setFocus();
}

void
LatencyChartWidget::leaveEvent(QEvent*)
{
    m_hovering = false;
    m_hoverIndex = -1;
    m_cdfHoverIndex = -1;
    QToolTip::hideText();
    update();
}

void
LatencyChartWidget::resizeEvent(QResizeEvent*)
{
    if (!m_hScroll || !m_vScroll)
    {
        return;
    }
    const int h = m_hScroll->sizeHint().height();
    const int w = m_vScroll->sizeHint().width();
    m_hScroll->setGeometry(0, height() - h, width() - w, h);
    m_vScroll->setGeometry(width() - w, 0, w, height() - h);
    updateScrollBarVisibility();
    updateScrollBars();
}

int
LatencyChartWidget::visibleCountForWidth(int plotWidth) const
{
    const int approx = std::max(64, plotWidth / 2);
    return std::max(1, approx);
}

int
LatencyChartWidget::viewEndIndex() const
{
    if (m_sampleTimeNs.isEmpty())
    {
        return -1;
    }
    const int end = m_viewStartIndex + m_viewCount - 1;
    return std::clamp(end, 0, int(m_sampleTimeNs.size()) - 1);
}

void
LatencyChartWidget::updateScrollBars()
{
    if (!m_hScroll || !m_vScroll)
    {
        return;
    }

    const int plotWidth = width() - 80 - 16 - m_vScroll->sizeHint().width();
    const int count = int(m_sampleTimeNs.size());
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
        const int baseVisible = std::max(1, std::min(count, visibleCountForWidth(plotWidth)));
        const int minVisible = 8;
        const double minZoom = std::min(1.0, double(baseVisible) / double(count));
        const double maxZoom = std::max(1.0,
                                        double(baseVisible) /
                                            double(std::max(1, std::min(count, minVisible))));
        m_xZoom = std::clamp(m_xZoom, minZoom, maxZoom);

        const int targetViewCount = int(std::round(double(baseVisible) / m_xZoom));
        m_viewCount = std::clamp(targetViewCount, 1, count);

        const int maxStart = std::max(0, count - m_viewCount);
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

void
LatencyChartWidget::updateScrollBarVisibility()
{
    if (m_hScroll)
    {
        m_hScroll->setVisible(!m_cdfMode);
    }
    if (m_vScroll)
    {
        m_vScroll->setVisible(!m_cdfMode);
    }
}

QString
LatencyChartWidget::titleText() const
{
    switch (m_metric)
    {
    case LatencyMetric::QueueingDelay:
        return "Queueing Delay";
    case LatencyMetric::MacEndToEndDelay:
        return "MAC End-to-End Delay";
    }
    return "Delay";
}

QString
LatencyChartWidget::subtitleText() const
{
    switch (m_metric)
    {
    case LatencyMetric::QueueingDelay:
        return "Per-packet MAC queueing delay: enqueue to first transmission.";
    case LatencyMetric::MacEndToEndDelay:
        return "Per-packet MAC service delay: enqueue to ACK reception.";
    }
    return "Delay sample.";
}

QString
LatencyChartWidget::yAxisText() const
{
    return "Delay (us)";
}

QString
LatencyChartWidget::tooltipTitleText() const
{
    switch (m_metric)
    {
    case LatencyMetric::QueueingDelay:
        return "Queueing Delay Sample";
    case LatencyMetric::MacEndToEndDelay:
        return "MAC End-to-End Delay Sample";
    }
    return "Delay Sample";
}

uint64_t
LatencyChartWidget::metricValueNs(const PpduVisualItem& ppdu) const
{
    switch (m_metric)
    {
    case LatencyMetric::QueueingDelay:
        return ppdu.accessDelayNs;
    case LatencyMetric::MacEndToEndDelay:
        return ppdu.macE2eDelayNs;
    }
    return 0;
}

QVector<double>
LatencyChartWidget::sortedSampleMs() const
{
    QVector<double> values;
    values.reserve(m_sampleLatencyUsX10.size());
    for (const uint32_t usX10 : m_sampleLatencyUsX10)
    {
        values.append(double(usX10) / 10000.0);
    }
    std::sort(values.begin(), values.end());
    return values;
}

int
LatencyChartWidget::cdfIndexFromX(int x, const QRect& plot, const QVector<double>& sortedMs) const
{
    if (sortedMs.isEmpty() || plot.width() <= 0)
    {
        return -1;
    }
    const double maxMs = std::max(0.001, sortedMs.back());
    double ratio = double(x - plot.left()) / double(plot.width());
    ratio = std::clamp(ratio, 0.0, 1.0);
    const double targetMs = ratio * maxMs;
    auto it = std::lower_bound(sortedMs.constBegin(), sortedMs.constEnd(), targetMs);
    if (it == sortedMs.constEnd())
    {
        return sortedMs.size() - 1;
    }
    return int(std::distance(sortedMs.constBegin(), it));
}
