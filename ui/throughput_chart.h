#pragma once

#include <QHash>
#include <QString>
#include <QWidget>
#include <QVector>
#include <QScrollBar>

#include "ppdu_visual_item.h"

class ThroughputChartWidget : public QWidget
{
    Q_OBJECT
public:
    explicit ThroughputChartWidget(QWidget *parent = nullptr);

    void appendPpdu(const PpduVisualItem &ppdu);
    void reset();

    void setBucketDurationNs(uint64_t bucketNs);
    void setMaxBuckets(int maxBuckets);

protected:
    void paintEvent(QPaintEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void wheelEvent(QWheelEvent *event) override;
    void enterEvent(QEnterEvent *event) override;
    void leaveEvent(QEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;

private:
    void scheduleUpdate();
    double sampleMbps(int index) const;
    double totalAverageMbps() const;
    int indexFromX(int x, int plotLeft, int plotWidth) const;
    uint64_t sampleTimeNs(int index) const;
    int visibleCountForWidth(int plotWidth) const;
    int viewEndIndex() const;
    void updateScrollBars();
    QString labelForMac(quint64 mac) const;

private:
    uint64_t m_bucketNs = 100000000; // kept for API compatibility
    int m_maxBuckets = 200000;

    QVector<uint64_t> m_sampleTimeNs;
    QVector<uint32_t> m_sampleThroughputX100;
    QVector<PpduVisualItem> m_sampleItems;
    bool m_updateQueued = false;

    bool m_hovering = false;
    int m_hoverIndex = -1;

    QScrollBar *m_hScroll = nullptr;
    QScrollBar *m_vScroll = nullptr;

    int m_viewStartIndex = 0;
    int m_viewCount = 0;
    double m_xZoom = 1.0;
    double m_yZoom = 1.0;

    uint64_t m_totalThroughputX100Sum = 0;
    uint64_t m_totalSampleCount = 0;
    QHash<quint64, DeviceRole> m_roleByMac;
    QVector<quint64> m_apOrder;
    QVector<quint64> m_staOrder;
    QVector<quint64> m_devOrder;
};
