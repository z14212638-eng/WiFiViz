#include "phy_state_pie_chart.h"

#include <QMouseEvent>
#include <QPainter>
#include <QToolTip>
#include <QtMath>

#include <algorithm>
#include <cmath>

namespace
{
static const QColor kPanelBg(244, 248, 252);
static const QColor kFrame(205, 214, 226);
static const QColor kTextPrimary(34, 43, 56);
static const QColor kTextMuted(103, 116, 131);
}

PhyStatePieChartWidget::PhyStatePieChartWidget(QWidget* parent)
    : QWidget(parent)
{
    setMinimumHeight(220);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    setMouseTracking(true);
}

void
PhyStatePieChartWidget::appendPpdu(const PpduVisualItem& ppdu)
{
    if (ppdu.recordType != RecordType::PhyState || ppdu.phyStateDurationNs == 0)
    {
        return;
    }

    const int key = static_cast<int>(ppdu.phyState);
    m_durationNsByState[key] += ppdu.phyStateDurationNs;
    m_seenPhyKeys.insert((static_cast<quint64>(ppdu.nodeId) << 32) |
                         static_cast<quint64>(ppdu.channel_number));
    m_totalDurationNs += ppdu.phyStateDurationNs;
    update();
}

void
PhyStatePieChartWidget::reset()
{
    m_durationNsByState.clear();
    m_seenPhyKeys.clear();
    m_totalDurationNs = 0;
    m_hoverIndex = -1;
    update();
}

QVector<PhyStatePieChartWidget::Slice>
PhyStatePieChartWidget::slices() const
{
    QVector<Slice> result;
    result.reserve(m_durationNsByState.size());
    for (auto it = m_durationNsByState.constBegin(); it != m_durationNsByState.constEnd(); ++it)
    {
        const auto state = static_cast<PhyStateKind>(it.key());
        result.push_back(Slice{state, stateName(state), stateColor(state), it.value()});
    }
    std::sort(result.begin(), result.end(), [](const Slice& a, const Slice& b) {
        return a.durationNs > b.durationNs;
    });
    return result;
}

QString
PhyStatePieChartWidget::stateName(PhyStateKind state) const
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
    case PhyStateKind::Unknown:
    default:
        return "UNKNOWN";
    }
}

QColor
PhyStatePieChartWidget::stateColor(PhyStateKind state) const
{
    switch (state)
    {
    case PhyStateKind::Idle:
        return QColor(18, 184, 134);
    case PhyStateKind::CcaBusy:
        return QColor(255, 184, 0);
    case PhyStateKind::Tx:
        return QColor(246, 116, 59);
    case PhyStateKind::Rx:
        return QColor(53, 131, 255);
    case PhyStateKind::Switching:
        return QColor(145, 86, 255);
    case PhyStateKind::Sleep:
        return QColor(20, 184, 166);
    case PhyStateKind::Off:
        return QColor(100, 116, 139);
    case PhyStateKind::Unknown:
    default:
        return QColor(148, 163, 184);
    }
}

int
PhyStatePieChartWidget::observedPhyCount() const
{
    return std::max(1, static_cast<int>(m_seenPhyKeys.size()));
}

void
PhyStatePieChartWidget::paintEvent(QPaintEvent*)
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
    p.drawText(card.adjusted(16, 12, -16, -12), Qt::AlignLeft | Qt::AlignTop, "PHY State Share");

    QFont bodyFont = font();
    bodyFont.setPointSize(std::max(9, bodyFont.pointSize() - 1));
    p.setFont(bodyFont);
    p.setPen(kTextMuted);
    p.drawText(card.adjusted(16, 36, -16, -12),
               Qt::AlignLeft | Qt::AlignTop,
               "PHY-layer state duration distribution across observed radios.");

    if (m_totalDurationNs == 0)
    {
        p.drawText(card.adjusted(0, 40, 0, 0), Qt::AlignCenter, "Waiting for PHY state samples...");
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
    for (int i = 0; i < data.size(); ++i)
    {
        const auto& slice = data[i];
        if (slice.durationNs == 0)
        {
            continue;
        }
        const int span = qRound((double(slice.durationNs) / double(m_totalDurationNs)) * 360.0 * 16.0);
        QColor fill = i == m_hoverIndex ? slice.color.lighter(115) : slice.color;
        p.setPen(QPen(Qt::white, 2));
        p.setBrush(fill);
        p.drawPie(donutRect, startAngle, -span);
        startAngle -= span;
    }

    QRect innerRect = donutRect.adjusted(ringWidth, ringWidth, -ringWidth, -ringWidth);
    p.setPen(Qt::NoPen);
    p.setBrush(Qt::white);
    p.drawEllipse(innerRect);

    const double avgPerPhyMs = double(m_totalDurationNs) /
                               double(observedPhyCount()) /
                               1000000.0;
    p.setPen(kTextMuted);
    p.setFont(bodyFont);
    p.drawText(innerRect.adjusted(0, -16, 0, 0), Qt::AlignCenter, "Avg/PHY");
    p.setPen(kTextPrimary);
    QFont totalFont = titleFont;
    totalFont.setPointSize(totalFont.pointSize() + 2);
    p.setFont(totalFont);
    p.drawText(innerRect.adjusted(0, 16, 0, 0),
               Qt::AlignCenter,
               QString("%1 ms").arg(avgPerPhyMs, 0, 'f', avgPerPhyMs < 10.0 ? 2 : 1));

    p.setFont(bodyFont);
    int y = legendRect.top();
    for (int i = 0; i < data.size(); ++i)
    {
        const auto& slice = data[i];
        const QRect row(legendRect.left(), y, legendRect.width(), 24);
        p.setPen(Qt::NoPen);
        p.setBrush(slice.color);
        p.drawRoundedRect(QRect(row.left(), row.top() + 4, 12, 12), 4, 4);

        const double ratio = double(slice.durationNs) / double(m_totalDurationNs) * 100.0;
        p.setPen(i == m_hoverIndex ? kTextPrimary : kTextMuted);
        p.drawText(row.adjusted(20, 0, -86, 0), Qt::AlignVCenter | Qt::AlignLeft, slice.label);
        p.drawText(row.adjusted(0, 0, -4, 0),
                   Qt::AlignVCenter | Qt::AlignRight,
                   QString("%1%").arg(ratio, 0, 'f', 1));
        y += 28;
        if (y > legendRect.bottom() - 20)
        {
            break;
        }
    }
}

int
PhyStatePieChartWidget::sliceIndexAt(const QPoint& pos) const
{
    if (m_totalDurationNs == 0)
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
        const double span = double(data[i].durationNs) / double(m_totalDurationNs) * 360.0;
        if (angle >= acc && angle < acc + span)
        {
            return i;
        }
        acc += span;
    }
    return -1;
}

void
PhyStatePieChartWidget::mouseMoveEvent(QMouseEvent* event)
{
    m_hoverIndex = sliceIndexAt(event->pos());
    const auto data = slices();
    if (m_hoverIndex >= 0 && m_hoverIndex < data.size())
    {
        const auto& slice = data[m_hoverIndex];
        const double ratio = double(slice.durationNs) / double(m_totalDurationNs) * 100.0;
        const double totalDurationMs = double(slice.durationNs) / 1000000.0;
        const double avgDurationMs = totalDurationMs / double(observedPhyCount());
        QToolTip::showText(mapToGlobal(event->pos() + QPoint(12, -10)),
                           QString(
                               "<div style='min-width:190px;'>"
                               "<div style='font-size:14px;font-weight:700;color:#223043;'>PHY State Share</div>"
                               "<div style='margin-top:8px;font-size:15px;font-weight:600;'>%1</div>"
                               "<table cellspacing='0' cellpadding='0' style='margin-top:8px;color:#314154;'>"
                               "<tr><td style='padding:0 12px 4px 0;color:#6A7A8C;'>Total</td><td style='font-weight:600;'>%2 ms</td></tr>"
                               "<tr><td style='padding:0 12px 4px 0;color:#6A7A8C;'>Avg/PHY</td><td style='font-weight:600;'>%3 ms</td></tr>"
                               "<tr><td style='padding:0 12px 0 0;color:#6A7A8C;'>Share</td><td style='font-weight:600;'>%4%</td></tr>"
                               "</table></div>")
                               .arg(slice.label)
                               .arg(totalDurationMs, 0, 'f', totalDurationMs < 10.0 ? 2 : 1)
                               .arg(avgDurationMs, 0, 'f', avgDurationMs < 10.0 ? 2 : 1)
                               .arg(ratio, 0, 'f', 1),
                           this);
    }
    else
    {
        QToolTip::hideText();
    }
    update();
}

void
PhyStatePieChartWidget::leaveEvent(QEvent*)
{
    m_hoverIndex = -1;
    QToolTip::hideText();
    update();
}
