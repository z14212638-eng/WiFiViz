#pragma once

#include <QColor>
#include <QHash>
#include <QVector>
#include <QWidget>

#include "ppdu_visual_item.h"

class PhyStatePieChartWidget : public QWidget
{
    Q_OBJECT
  public:
    explicit PhyStatePieChartWidget(QWidget* parent = nullptr);

    void appendPpdu(const PpduVisualItem& ppdu);
    void reset();

  protected:
    void paintEvent(QPaintEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void leaveEvent(QEvent* event) override;

  private:
    struct Slice
    {
        PhyStateKind state = PhyStateKind::Unknown;
        QString label;
        QColor color;
        uint64_t durationNs = 0;
    };

    QVector<Slice> slices() const;
    int sliceIndexAt(const QPoint& pos) const;
    QString stateName(PhyStateKind state) const;
    QColor stateColor(PhyStateKind state) const;

    QHash<int, uint64_t> m_durationNsByState;
    uint64_t m_totalDurationNs = 0;
    int m_hoverIndex = -1;
};
