#include "mcs_distribution_chart.h"

#include <QMouseEvent>
#include <QPainter>
#include <QToolTip>

#include <algorithm>

namespace
{
static const QColor kPanelBg(244, 248, 252);
static const QColor kPlotBg(250, 252, 255);
static const QColor kFrame(205, 214, 226);
static const QColor kGrid(222, 229, 238);
static const QColor kBar(53, 131, 255);
static const QColor kBarHover(80, 156, 255);
static const QColor kTextPrimary(34, 43, 56);
static const QColor kTextMuted(103, 116, 131);
}

McsDistributionChartWidget::McsDistributionChartWidget(QWidget* parent)
    : QWidget(parent)
{
    setMinimumHeight(220);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    setMouseTracking(true);
}

void
McsDistributionChartWidget::appendPpdu(const PpduVisualItem& ppdu)
{
    if (ppdu.recordType != RecordType::Ppdu)
    {
        return;
    }
    m_mcsCounts[ppdu.mcs] += 1;
    ++m_totalCount;
    update();
}

void
McsDistributionChartWidget::reset()
{
    m_mcsCounts.clear();
    m_totalCount = 0;
    m_hoverIndex = -1;
    update();
}

void
McsDistributionChartWidget::paintEvent(QPaintEvent*)
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
    p.drawText(card.adjusted(16, 12, -16, -12), Qt::AlignLeft | Qt::AlignTop, "MCS Distribution");

    QFont bodyFont = font();
    bodyFont.setPointSize(std::max(9, bodyFont.pointSize() - 1));
    p.setFont(bodyFont);
    p.setPen(kTextMuted);
    p.drawText(card.adjusted(16, 36, -16, -12),
               Qt::AlignLeft | Qt::AlignTop,
               "Captured PPDUs grouped by MCS index.");

    if (m_totalCount == 0)
    {
        p.drawText(card.adjusted(0, 40, 0, 0), Qt::AlignCenter, "Waiting for MCS samples...");
        return;
    }

    const int leftPad = card.left() + 46;
    const int rightPad = 18;
    const int topPad = card.top() + 74;
    const int bottomPad = 34;
    QRect plot(leftPad, topPad, card.width() - (leftPad - card.left()) - rightPad, card.height() - (topPad - card.top()) - bottomPad);

    p.setPen(Qt::NoPen);
    p.setBrush(kPlotBg);
    p.drawRoundedRect(plot, 12, 12);

    QVector<int> keys;
    keys.reserve(m_mcsCounts.size());
    int maxCount = 0;
    for (auto it = m_mcsCounts.constBegin(); it != m_mcsCounts.constEnd(); ++it)
    {
        keys.push_back(it.key());
        maxCount = std::max(maxCount, it.value());
    }
    if (keys.isEmpty() || maxCount <= 0)
    {
        p.setPen(kTextMuted);
        p.drawText(plot, Qt::AlignCenter, "No valid MCS buckets.");
        return;
    }

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
    p.drawText(card.left() + 10, plot.top() - 6, "Count");
    for (int i = 0; i <= hLines; ++i)
    {
        const int value = qRound(double(maxCount) * (1.0 - double(i) / hLines));
        const int y = plot.top() + (plot.height() * i) / hLines;
        p.drawText(card.left() + 8, y + 4, QString::number(value));
    }

    const int count = int(keys.size());
    const int gap = 10;
    const int barWidth = std::max(18, (plot.width() - gap * (count + 1)) / std::max(1, count));
    int x = plot.left() + gap;
    for (int i = 0; i < count; ++i)
    {
        const int key = keys[i];
        const int bucketCount = m_mcsCounts.value(key);
        const double ratio = double(bucketCount) / double(maxCount);
        const int barHeight = std::max(2, int(plot.height() * ratio));
        QRect barRect(x, plot.bottom() - barHeight, barWidth, barHeight);
        p.setPen(Qt::NoPen);
        p.setBrush(i == m_hoverIndex ? kBarHover : kBar);
        p.drawRoundedRect(barRect, 8, 8);

        p.setPen(kTextMuted);
        p.drawText(QRect(x - 4, plot.bottom() + 6, barWidth + 8, 18),
                   Qt::AlignHCenter | Qt::AlignTop,
                   key < 0 ? QString("L") : QString::number(key));
        x += barWidth + gap;
    }
}

int
McsDistributionChartWidget::barIndexAt(const QPoint& pos, const QRect& plot, const QVector<int>& keys) const
{
    if (!plot.contains(pos) || keys.isEmpty())
    {
        return -1;
    }
    const int gap = 10;
    const int count = int(keys.size());
    const int barWidth = std::max(18, (plot.width() - gap * (count + 1)) / std::max(1, count));
    int x = plot.left() + gap;
    for (int i = 0; i < keys.size(); ++i)
    {
        QRect barArea(x, plot.top(), barWidth, plot.height());
        if (barArea.contains(pos))
        {
            return i;
        }
        x += barWidth + gap;
    }
    return -1;
}

void
McsDistributionChartWidget::mouseMoveEvent(QMouseEvent* event)
{
    const QRect card = rect().adjusted(6, 6, -6, -6);
    const int leftPad = card.left() + 46;
    const int rightPad = 18;
    const int topPad = card.top() + 74;
    const int bottomPad = 34;
    QRect plot(leftPad, topPad, card.width() - (leftPad - card.left()) - rightPad, card.height() - (topPad - card.top()) - bottomPad);

    QVector<int> keys;
    for (auto it = m_mcsCounts.constBegin(); it != m_mcsCounts.constEnd(); ++it)
    {
        keys.push_back(it.key());
    }

    m_hoverIndex = barIndexAt(event->pos(), plot, keys);
    if (m_hoverIndex >= 0 && m_hoverIndex < keys.size())
    {
        const int key = keys[m_hoverIndex];
        const int count = m_mcsCounts.value(key);
        const double ratio = m_totalCount > 0 ? double(count) / double(m_totalCount) * 100.0 : 0.0;
        QToolTip::showText(mapToGlobal(event->pos() + QPoint(12, -10)),
                           QString(
                               "<div style='min-width:170px;'>"
                               "<div style='font-size:14px;font-weight:700;color:#223043;'>MCS Distribution</div>"
                               "<div style='margin-top:8px;font-size:15px;font-weight:600;'>%1</div>"
                               "<table cellspacing='0' cellpadding='0' style='margin-top:8px;color:#314154;'>"
                               "<tr><td style='padding:0 12px 4px 0;color:#6A7A8C;'>Count</td><td style='font-weight:600;'>%2</td></tr>"
                               "<tr><td style='padding:0 12px 0 0;color:#6A7A8C;'>Share</td><td style='font-weight:600;'>%3%</td></tr>"
                               "</table></div>")
                               .arg(key < 0 ? QString("Legacy") : QString("MCS %1").arg(key))
                               .arg(count)
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
McsDistributionChartWidget::leaveEvent(QEvent*)
{
    m_hoverIndex = -1;
    QToolTip::hideText();
    update();
}
