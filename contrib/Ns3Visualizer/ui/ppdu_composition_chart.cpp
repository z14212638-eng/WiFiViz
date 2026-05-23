#include "ppdu_composition_chart.h"

#include <QMouseEvent>
#include <QPainter>
#include <QToolTip>
#include <QtMath>

#include <algorithm>

namespace
{
static const QColor kPanelBg(244, 248, 252);
static const QColor kFrame(205, 214, 226);
static const QColor kTextPrimary(34, 43, 56);
static const QColor kTextMuted(103, 116, 131);
static const QVector<QColor> kPalette = {
    QColor(53, 131, 255), QColor(246, 116, 59), QColor(18, 184, 134), QColor(145, 86, 255),
    QColor(255, 184, 0),  QColor(236, 72, 153), QColor(20, 184, 166), QColor(99, 102, 241)};
}

PpduCompositionChartWidget::PpduCompositionChartWidget(QWidget* parent)
    : QWidget(parent)
{
    setMinimumHeight(220);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    setMouseTracking(true);
}

void
PpduCompositionChartWidget::appendPpdu(const PpduVisualItem& ppdu)
{
    if (ppdu.recordType != RecordType::Ppdu)
    {
        return;
    }

    QString key = QString::fromStdString(ppdu.frameType);
    if (key.isEmpty())
    {
        key = "Unknown";
    }
    m_counts[key] += 1;
    ++m_totalCount;
    update();
}

void
PpduCompositionChartWidget::reset()
{
    m_counts.clear();
    m_totalCount = 0;
    m_hoverIndex = -1;
    update();
}

QVector<PpduCompositionChartWidget::Slice>
PpduCompositionChartWidget::slices() const
{
    QVector<Slice> result;
    result.reserve(m_counts.size());
    int index = 0;
    for (auto it = m_counts.constBegin(); it != m_counts.constEnd(); ++it, ++index)
    {
        result.push_back(Slice{it.key(), colorForIndex(index), it.value()});
    }
    std::sort(result.begin(), result.end(), [](const Slice& a, const Slice& b) {
        return a.count > b.count;
    });
    return result;
}

QColor
PpduCompositionChartWidget::colorForIndex(int index) const
{
    return kPalette[index % kPalette.size()];
}

void
PpduCompositionChartWidget::paintEvent(QPaintEvent*)
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
    p.drawText(card.adjusted(16, 12, -16, -12), Qt::AlignLeft | Qt::AlignTop, "PPDU Composition");

    QFont bodyFont = font();
    bodyFont.setPointSize(std::max(9, bodyFont.pointSize() - 1));
    p.setFont(bodyFont);
    p.setPen(kTextMuted);
    p.drawText(card.adjusted(16, 36, -16, -12),
               Qt::AlignLeft | Qt::AlignTop,
               "Frame-type share across captured PPDUs.");

    if (m_totalCount <= 0)
    {
        p.drawText(card.adjusted(0, 40, 0, 0), Qt::AlignCenter, "Waiting for PPDU samples...");
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
        const double ratio = double(slice.count) / double(m_totalCount);
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
    p.drawText(innerRect.adjusted(0, -16, 0, 0), Qt::AlignCenter, "Total");
    p.setPen(kTextPrimary);
    QFont totalFont = titleFont;
    totalFont.setPointSize(totalFont.pointSize() + 4);
    p.setFont(totalFont);
    p.drawText(innerRect.adjusted(0, 16, 0, 0), Qt::AlignCenter, QString::number(m_totalCount));

    p.setFont(bodyFont);
    int y = legendRect.top();
    index = 0;
    for (const auto& slice : data)
    {
        const QRect row(legendRect.left(), y, legendRect.width(), 24);
        p.setPen(Qt::NoPen);
        p.setBrush(slice.color);
        p.drawRoundedRect(QRect(row.left(), row.top() + 4, 12, 12), 4, 4);

        const double ratio = double(slice.count) / double(m_totalCount) * 100.0;
        p.setPen(index == m_hoverIndex ? kTextPrimary : kTextMuted);
        p.drawText(row.adjusted(20, 0, -80, 0), Qt::AlignVCenter | Qt::AlignLeft, slice.label);
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
PpduCompositionChartWidget::sliceIndexAt(const QPoint& pos) const
{
    if (m_totalCount <= 0)
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
        const double span = double(data[i].count) / double(m_totalCount) * 360.0;
        if (angle >= acc && angle < acc + span)
        {
            return i;
        }
        acc += span;
    }
    return -1;
}

void
PpduCompositionChartWidget::mouseMoveEvent(QMouseEvent* event)
{
    m_hoverIndex = sliceIndexAt(event->pos());
    if (m_hoverIndex >= 0)
    {
        const auto data = slices();
        if (m_hoverIndex < data.size())
        {
            const auto& slice = data[m_hoverIndex];
            const double ratio = double(slice.count) / double(m_totalCount) * 100.0;
            QToolTip::showText(mapToGlobal(event->pos() + QPoint(12, -10)),
                               QString(
                                   "<div style='min-width:170px;'>"
                                   "<div style='font-size:14px;font-weight:700;color:#223043;'>Frame Mix</div>"
                                   "<div style='margin-top:8px;font-size:15px;font-weight:600;'>%1</div>"
                                   "<table cellspacing='0' cellpadding='0' style='margin-top:8px;color:#314154;'>"
                                   "<tr><td style='padding:0 12px 4px 0;color:#6A7A8C;'>Count</td><td style='font-weight:600;'>%2</td></tr>"
                                   "<tr><td style='padding:0 12px 0 0;color:#6A7A8C;'>Share</td><td style='font-weight:600;'>%3%</td></tr>"
                                   "</table></div>")
                                   .arg(slice.label)
                                   .arg(slice.count)
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
PpduCompositionChartWidget::leaveEvent(QEvent*)
{
    m_hoverIndex = -1;
    QToolTip::hideText();
    update();
}
