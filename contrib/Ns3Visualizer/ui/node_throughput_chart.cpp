#include "node_throughput_chart.h"

#include <QMouseEvent>
#include <QPainter>
#include <QToolTip>

#include <algorithm>
#include <cmath>

namespace
{
static const QColor kPanelBg(244, 248, 252);
static const QColor kFrame(205, 214, 226);
static const QColor kTextPrimary(34, 43, 56);
static const QColor kTextMuted(103, 116, 131);
static const QVector<QColor> kPalette = {
    QColor(18, 184, 134), QColor(53, 131, 255), QColor(246, 116, 59), QColor(145, 86, 255),
    QColor(255, 184, 0),  QColor(236, 72, 153), QColor(20, 184, 166), QColor(99, 102, 241)};
}

NodeThroughputChartWidget::NodeThroughputChartWidget(QWidget* parent)
    : QWidget(parent)
{
    setMinimumHeight(220);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    setMouseTracking(true);
}

void
NodeThroughputChartWidget::appendPpdu(const PpduVisualItem& ppdu)
{
    if (ppdu.recordType == RecordType::DeviceRole && ppdu.roleMac != 0)
    {
        QString prefix = "Node";
        if (ppdu.deviceRole == DeviceRole::Ap)
        {
            prefix = "AP";
        }
        else if (ppdu.deviceRole == DeviceRole::Sta)
        {
            prefix = "STA";
        }
        m_labelByMac[ppdu.roleMac] = QString("%1_%2").arg(prefix).arg(ppdu.nodeId);
        update();
        return;
    }

    if (ppdu.recordType == RecordType::PpduUpdate)
    {
        const auto it = m_samplesById.find(ppdu.id);
        if (it == m_samplesById.end())
        {
            return;
        }

        const Sample oldSample = it.value();
        const quint64 sender = ppdu.sender != 0 ? ppdu.sender : oldSample.sender;
        m_samplesById.erase(it);
        removeSample(oldSample);
        addSample(ppdu.id, sender, ppdu.throughputMbpsX100);
        update();
        return;
    }

    if (ppdu.recordType != RecordType::Ppdu || ppdu.sender == 0 || ppdu.txEndNs == 0)
    {
        return;
    }

    addSample(ppdu.id, ppdu.sender, ppdu.throughputMbpsX100);
    update();
}

void
NodeThroughputChartWidget::reset()
{
    m_samplesById.clear();
    m_throughputX100SumByMac.clear();
    m_sampleCountByMac.clear();
    m_labelByMac.clear();
    m_totalThroughputX100Sum = 0;
    m_totalSampleCount = 0;
    m_hoverIndex = -1;
    update();
}

QVector<NodeThroughputChartWidget::Slice>
NodeThroughputChartWidget::slices() const
{
    QVector<Slice> result;
    result.reserve(m_throughputX100SumByMac.size());
    int index = 0;
    for (auto it = m_throughputX100SumByMac.constBegin();
         it != m_throughputX100SumByMac.constEnd();
         ++it, ++index)
    {
        result.push_back(Slice{
            labelForMac(it.key()),
            colorForIndex(index),
            it.value(),
            m_sampleCountByMac.value(it.key(), 0),
        });
    }
    std::sort(result.begin(), result.end(), [](const Slice& a, const Slice& b) {
        return a.throughputX100Sum > b.throughputX100Sum;
    });
    return result;
}

void
NodeThroughputChartWidget::addSample(quint32 id, quint64 sender, quint32 throughputX100)
{
    const auto old = m_samplesById.find(id);
    if (old != m_samplesById.end())
    {
        removeSample(old.value());
    }

    m_samplesById[id] = Sample{sender, throughputX100};
    m_throughputX100SumByMac[sender] += throughputX100;
    m_sampleCountByMac[sender] += 1;
    m_totalThroughputX100Sum += throughputX100;
    m_totalSampleCount += 1;
}

void
NodeThroughputChartWidget::removeSample(const Sample& sample)
{
    auto sumIt = m_throughputX100SumByMac.find(sample.sender);
    if (sumIt != m_throughputX100SumByMac.end())
    {
        sumIt.value() =
            sumIt.value() > sample.throughputX100 ? sumIt.value() - sample.throughputX100 : 0;
        if (sumIt.value() == 0)
        {
            m_throughputX100SumByMac.erase(sumIt);
        }
    }

    auto countIt = m_sampleCountByMac.find(sample.sender);
    if (countIt != m_sampleCountByMac.end())
    {
        countIt.value() = countIt.value() > 0 ? countIt.value() - 1 : 0;
        if (countIt.value() == 0)
        {
            m_sampleCountByMac.erase(countIt);
        }
    }

    m_totalThroughputX100Sum = m_totalThroughputX100Sum > sample.throughputX100
                                   ? m_totalThroughputX100Sum - sample.throughputX100
                                   : 0;
    m_totalSampleCount = m_totalSampleCount > 0 ? m_totalSampleCount - 1 : 0;
}

QColor
NodeThroughputChartWidget::colorForIndex(int index) const
{
    return kPalette[index % kPalette.size()];
}

QString
NodeThroughputChartWidget::labelForMac(quint64 mac) const
{
    const auto it = m_labelByMac.find(mac);
    if (it != m_labelByMac.end())
    {
        return it.value();
    }
    return QString("0x%1").arg(QString::number(mac, 16).toUpper());
}

void
NodeThroughputChartWidget::paintEvent(QPaintEvent*)
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
    p.drawText(card.adjusted(16, 12, -16, -12), Qt::AlignLeft | Qt::AlignTop, "Node Throughput");

    QFont bodyFont = font();
    bodyFont.setPointSize(std::max(9, bodyFont.pointSize() - 1));
    p.setFont(bodyFont);
    p.setPen(kTextMuted);
    p.drawText(card.adjusted(16, 36, -16, -12),
               Qt::AlignLeft | Qt::AlignTop,
               "Sender-side throughput grouped by AP / STA.");

    if (m_totalSampleCount == 0)
    {
        p.drawText(card.adjusted(0, 40, 0, 0), Qt::AlignCenter, "Waiting for PPDU throughput samples...");
        return;
    }

    const auto data = slices();
    QRect donutRect = card.adjusted(20, 72, -card.width() / 2 + 10, -24);
    const int diameter = std::min(donutRect.width(), donutRect.height());
    donutRect.setSize(QSize(diameter, diameter));

    QRect legendRect(card.center().x() + 12,
                     card.top() + 74,
                     card.right() - card.center().x() - 24,
                     card.height() - 96);

    int startAngle = 90 * 16;
    const int ringWidth = std::max(24, diameter / 6);
    int index = 0;
    for (const auto& slice : data)
    {
        const double ratio = m_totalThroughputX100Sum == 0
                                 ? double(slice.sampleCount) / double(m_totalSampleCount)
                                 : double(slice.throughputX100Sum) /
                                       double(m_totalThroughputX100Sum);
        const int span = qRound(ratio * 360.0 * 16.0);
        QColor fill = slice.color;
        if (index == m_hoverIndex)
        {
            fill = fill.lighter(115);
        }
        p.setPen(QPen(Qt::white, 2));
        p.setBrush(fill);
        p.drawPie(donutRect, startAngle, -span);
        startAngle -= span;
        ++index;
    }

    QRect innerRect = donutRect.adjusted(ringWidth, ringWidth, -ringWidth, -ringWidth);
    p.setPen(Qt::NoPen);
    p.setBrush(Qt::white);
    p.drawEllipse(innerRect);

    p.setPen(kTextMuted);
    p.setFont(bodyFont);
    p.drawText(innerRect.adjusted(0, -16, 0, 0), Qt::AlignCenter, "Avg");
    p.setPen(kTextPrimary);
    QFont totalFont = titleFont;
    totalFont.setPointSize(totalFont.pointSize() + 2);
    p.setFont(totalFont);
    const double avgMbps = double(m_totalThroughputX100Sum) / double(m_totalSampleCount) / 100.0;
    p.drawText(innerRect.adjusted(0, 16, 0, 0),
               Qt::AlignCenter,
               QString("%1 Mbps").arg(avgMbps, 0, 'f', 2));

    p.setFont(bodyFont);
    int y = legendRect.top();
    index = 0;
    for (const auto& slice : data)
    {
        const QRect row(legendRect.left(), y, legendRect.width(), 24);
        p.setPen(Qt::NoPen);
        p.setBrush(slice.color);
        p.drawRoundedRect(QRect(row.left(), row.top() + 4, 12, 12), 4, 4);

        const double ratio = m_totalThroughputX100Sum == 0
                                 ? double(slice.sampleCount) / double(m_totalSampleCount) * 100.0
                                 : double(slice.throughputX100Sum) /
                                       double(m_totalThroughputX100Sum) * 100.0;
        p.setPen(index == m_hoverIndex ? kTextPrimary : kTextMuted);
        p.drawText(row.adjusted(20, 0, -90, 0), Qt::AlignVCenter | Qt::AlignLeft, slice.label);
        p.drawText(row.adjusted(0, 0, -4, 0),
                   Qt::AlignVCenter | Qt::AlignRight,
                   QString("%1%").arg(ratio, 0, 'f', 1));
        y += 28;
        if (y > legendRect.bottom() - 20)
        {
            break;
        }
        ++index;
    }
}

int
NodeThroughputChartWidget::sliceIndexAt(const QPoint& pos) const
{
    if (m_totalSampleCount == 0)
    {
        return -1;
    }

    const QRect card = rect().adjusted(6, 6, -6, -6);
    QRect donutRect = card.adjusted(20, 72, -card.width() / 2 + 10, -24);
    const int diameter = std::min(donutRect.width(), donutRect.height());
    donutRect.setSize(QSize(diameter, diameter));

    const QPointF center = donutRect.center();
    const QPointF delta = pos - center.toPoint();
    const double radius = std::sqrt(delta.x() * delta.x() + delta.y() * delta.y());
    const double outerRadius = donutRect.width() / 2.0;
    const double innerRadius = outerRadius - std::max(24, donutRect.width() / 6);
    if (radius < innerRadius || radius > outerRadius)
    {
        return -1;
    }

    double angle = std::atan2(-delta.y(), delta.x()) * 180.0 / M_PI;
    angle = 90.0 - angle;
    if (angle < 0.0)
    {
        angle += 360.0;
    }

    const auto data = slices();
    double acc = 0.0;
    for (int i = 0; i < data.size(); ++i)
    {
        const double span = m_totalThroughputX100Sum == 0
                                ? double(data[i].sampleCount) / double(m_totalSampleCount) * 360.0
                                : double(data[i].throughputX100Sum) /
                                      double(m_totalThroughputX100Sum) * 360.0;
        if (angle >= acc && angle < acc + span)
        {
            return i;
        }
        acc += span;
    }
    return -1;
}

void
NodeThroughputChartWidget::mouseMoveEvent(QMouseEvent* event)
{
    m_hoverIndex = sliceIndexAt(event->pos());
    if (m_hoverIndex >= 0)
    {
        const auto data = slices();
        if (m_hoverIndex < data.size())
        {
            const auto& slice = data[m_hoverIndex];
            const double ratio = m_totalThroughputX100Sum == 0
                                     ? double(slice.sampleCount) / double(m_totalSampleCount) *
                                           100.0
                                     : double(slice.throughputX100Sum) /
                                           double(m_totalThroughputX100Sum) * 100.0;
            const double avgMbps = slice.sampleCount == 0
                                       ? 0.0
                                       : double(slice.throughputX100Sum) /
                                             double(slice.sampleCount) / 100.0;
            QToolTip::showText(mapToGlobal(event->pos() + QPoint(12, -10)),
                               QString(
                                   "<div style='min-width:180px;'>"
                                   "<div style='font-size:14px;font-weight:700;color:#223043;'>Node Throughput</div>"
                                   "<div style='margin-top:8px;font-size:15px;font-weight:600;'>%1</div>"
                                   "<table cellspacing='0' cellpadding='0' style='margin-top:8px;color:#314154;'>"
                                   "<tr><td style='padding:0 12px 4px 0;color:#6A7A8C;'>Avg</td><td style='font-weight:600;'>%2 Mbps</td></tr>"
                                   "<tr><td style='padding:0 12px 0 0;color:#6A7A8C;'>Portion</td><td style='font-weight:600;'>%3%</td></tr>"
                                   "</table></div>")
                                   .arg(slice.label)
                                   .arg(avgMbps, 0, 'f', 2)
                                   .arg(ratio, 0, 'f', 1),
                               this);
        }
    }
    else
    {
        QToolTip::hideText();
    }
    update();
}

void
NodeThroughputChartWidget::leaveEvent(QEvent*)
{
    m_hoverIndex = -1;
    QToolTip::hideText();
    update();
}
