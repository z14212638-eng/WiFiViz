#pragma once

#include <QColor>
#include <QHash>
#include <QVector>
#include <QWidget>

#include "ppdu_visual_item.h"

class PpduCompositionChartWidget : public QWidget
{
    Q_OBJECT
  public:
    explicit PpduCompositionChartWidget(QWidget* parent = nullptr);

    void appendPpdu(const PpduVisualItem& ppdu);
    void reset();

  protected:
    void paintEvent(QPaintEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void leaveEvent(QEvent* event) override;

  private:
    struct Slice
    {
        QString label;
        QColor color;
        int count = 0;
    };

    QVector<Slice> slices() const;
    int sliceIndexAt(const QPoint& pos) const;
    QColor colorForIndex(int index) const;

    QHash<QString, int> m_counts;
    int m_totalCount = 0;
    int m_hoverIndex = -1;
};
