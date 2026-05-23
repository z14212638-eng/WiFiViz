#include "rx_outcome_chart.h"

#include <QMouseEvent>
#include <QPainter>
#include <QToolTip>

#include <cmath>

namespace
{
static const QColor kPanelBg(244, 248, 252);
static const QColor kFrame(205, 214, 226);
static const QColor kTextPrimary(34, 43, 56);
static const QColor kTextMuted(103, 116, 131);
static const QColor kSuccess(18, 184, 134);
static const QColor kCollision(246, 116, 59);
static const QColor kDecodeFail(145, 86, 255);
static const QColor kUnknown(148, 163, 184);
}

RxOutcomeChartWidget::RxOutcomeChartWidget(QWidget* parent)
    : QWidget(parent)
{
    setMinimumHeight(220);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    setMouseTracking(true);
}

void
RxOutcomeChartWidget::appendPpdu(const PpduVisualItem& ppdu)
{
    auto addState = [this](RxState state) {
        switch (state)
        {
        case RxState::Success:
            ++m_successCount;
            break;
        case RxState::Collision:
            ++m_collisionCount;
            break;
        case RxState::DecodeFail:
            ++m_decodeFailCount;
            break;
        default:
            ++m_unknownCount;
            break;
        }
    };

    auto removeState = [this](RxState state) {
        switch (state)
        {
        case RxState::Success:
            --m_successCount;
            break;
        case RxState::Collision:
            --m_collisionCount;
            break;
        case RxState::DecodeFail:
            --m_decodeFailCount;
            break;
        default:
            --m_unknownCount;
            break;
        }
    };

    if (ppdu.recordType == RecordType::PpduUpdate)
    {
        for (int i = m_samples.size() - 1; i >= 0; --i)
        {
            if (m_samples[i].id == ppdu.id)
            {
                removeState(m_samples[i].rxState);
                PpduVisualItem merged = ppdu;
                merged.recordType = RecordType::Ppdu;
                m_samples[i] = merged;
                addState(merged.rxState);
                update();
                return;
            }
        }
        return;
    }

    if (ppdu.recordType != RecordType::Ppdu)
        return;

    m_samples.append(ppdu);
    addState(ppdu.rxState);
    update();
}

void
RxOutcomeChartWidget::reset()
{
    m_successCount = 0;
    m_collisionCount = 0;
    m_decodeFailCount = 0;
    m_unknownCount = 0;
    m_samples.clear();
    m_hoverIndex = -1;
    update();
}

QVector<RxOutcomeChartWidget::Slice>
RxOutcomeChartWidget::slices() const
{
    return {
        {"Success", kSuccess, m_successCount},
        {"Collision", kCollision, m_collisionCount},
        {"Decode Fail", kDecodeFail, m_decodeFailCount},
        {"Unknown", kUnknown, m_unknownCount},
    };
}

void
RxOutcomeChartWidget::paintEvent(QPaintEvent*)
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
    p.drawText(card.adjusted(16, 12, -16, -12), Qt::AlignLeft | Qt::AlignTop, "RX Outcome");

    QFont bodyFont = font();
    bodyFont.setPointSize(std::max(9, bodyFont.pointSize() - 1));
    p.setFont(bodyFont);
    p.setPen(kTextMuted);
    p.drawText(card.adjusted(16, 36, -16, -12),
               Qt::AlignLeft | Qt::AlignTop,
               "Receive result distribution for captured PPDUs.");

    const auto data = slices();
    int total = 0;
    for (const auto& s : data)
    {
        total += s.count;
    }
    if (total == 0)
    {
        p.drawText(card.adjusted(0, 40, 0, 0), Qt::AlignCenter, "Waiting for RX outcomes...");
        return;
    }

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
        if (slice.count <= 0)
        {
            continue;
        }
        const int span = qRound((double(slice.count) / double(total)) * 360.0 * 16.0);
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

    p.setPen(kTextMuted);
    p.setFont(bodyFont);
    p.drawText(innerRect.adjusted(0, -16, 0, 0), Qt::AlignCenter, "PPDUs");
    p.setPen(kTextPrimary);
    QFont totalFont = titleFont;
    totalFont.setPointSize(totalFont.pointSize() + 4);
    p.setFont(totalFont);
    p.drawText(innerRect.adjusted(0, 16, 0, 0), Qt::AlignCenter, QString::number(total));

    p.setFont(bodyFont);
    int y = legendRect.top();
    for (int i = 0; i < data.size(); ++i)
    {
        const auto& slice = data[i];
        const QRect row(legendRect.left(), y, legendRect.width(), 24);
        p.setPen(Qt::NoPen);
        p.setBrush(slice.color);
        p.drawRoundedRect(QRect(row.left(), row.top() + 4, 12, 12), 4, 4);

        const double ratio = total > 0 ? double(slice.count) / double(total) * 100.0 : 0.0;
        p.setPen(i == m_hoverIndex ? kTextPrimary : kTextMuted);
        p.drawText(row.adjusted(20, 0, -80, 0), Qt::AlignVCenter | Qt::AlignLeft, slice.label);
        p.drawText(row.adjusted(0, 0, -4, 0),
                   Qt::AlignVCenter | Qt::AlignRight,
                   QString("%1%").arg(ratio, 0, 'f', 1));
        y += 28;
    }
}

int
RxOutcomeChartWidget::sliceIndexAt(const QPoint& pos) const
{
    const auto data = slices();
    int total = 0;
    for (const auto& s : data)
    {
        total += s.count;
    }
    if (total == 0)
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

    double acc = 0.0;
    for (int i = 0; i < data.size(); ++i)
    {
        if (data[i].count <= 0)
        {
            continue;
        }
        const double span = double(data[i].count) / double(total) * 360.0;
        if (angle >= acc && angle < acc + span)
        {
            return i;
        }
        acc += span;
    }
    return -1;
}

void
RxOutcomeChartWidget::mouseMoveEvent(QMouseEvent* event)
{
    m_hoverIndex = sliceIndexAt(event->pos());
    const auto data = slices();
    int total = 0;
    for (const auto& s : data)
    {
        total += s.count;
    }
    if (m_hoverIndex >= 0 && m_hoverIndex < data.size() && total > 0)
    {
        const auto& slice = data[m_hoverIndex];
        const double ratio = double(slice.count) / double(total) * 100.0;
        QToolTip::showText(mapToGlobal(event->pos() + QPoint(12, -10)),
                           QString(
                               "<div style='min-width:170px;'>"
                               "<div style='font-size:14px;font-weight:700;color:#223043;'>RX Outcome</div>"
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
    else
    {
        QToolTip::hideText();
    }
    update();
}

void
RxOutcomeChartWidget::leaveEvent(QEvent*)
{
    m_hoverIndex = -1;
    QToolTip::hideText();
    update();
}
