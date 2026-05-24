#pragma once

#include <QColor>
#include <QVector>
#include <QWidget>

#include "ppdu_visual_item.h"

class RxOutcomeChartWidget : public QWidget
{
    Q_OBJECT
  public:
    explicit RxOutcomeChartWidget(QWidget* parent = nullptr);

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

    int m_successCount = 0;
    int m_collisionCount = 0;
    int m_decodeFailCount = 0;
    int m_unknownCount = 0;
    QVector<PpduVisualItem> m_samples;
    int m_hoverIndex = -1;
};
