#pragma once

#include <QColor>
#include <QHash>
#include <QVector>
#include <QWidget>

#include "ppdu_visual_item.h"

class NodeThroughputChartWidget : public QWidget
{
    Q_OBJECT
  public:
    explicit NodeThroughputChartWidget(QWidget* parent = nullptr);

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
        quint64 throughputX100Sum = 0;
        quint64 sampleCount = 0;
    };

    struct Sample
    {
        quint64 sender = 0;
        quint32 throughputX100 = 0;
    };

    QVector<Slice> slices() const;
    void addSample(quint32 id, quint64 sender, quint32 throughputX100);
    void removeSample(const Sample& sample);
    QColor colorForIndex(int index) const;
    int sliceIndexAt(const QPoint& pos) const;
    QString labelForMac(quint64 mac) const;

    QHash<quint32, Sample> m_samplesById;
    QHash<quint64, quint64> m_throughputX100SumByMac;
    QHash<quint64, quint64> m_sampleCountByMac;
    QHash<quint64, QString> m_labelByMac;
    quint64 m_totalThroughputX100Sum = 0;
    quint64 m_totalSampleCount = 0;
    int m_hoverIndex = -1;
};
