#pragma once

#include <QMap>
#include <QVector>
#include <QWidget>

#include "ppdu_visual_item.h"

class McsDistributionChartWidget : public QWidget
{
    Q_OBJECT
  public:
    explicit McsDistributionChartWidget(QWidget* parent = nullptr);

    void appendPpdu(const PpduVisualItem& ppdu);
    void reset();

  protected:
    void paintEvent(QPaintEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void leaveEvent(QEvent* event) override;

  private:
    int barIndexAt(const QPoint& pos, const QRect& plot, const QVector<int>& keys) const;

    QMap<int, int> m_mcsCounts;
    int m_totalCount = 0;
    int m_hoverIndex = -1;
};
