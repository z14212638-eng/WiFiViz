#pragma once

#include <QRect>
#include <QScrollBar>
#include <QVector>
#include <QWidget>

#include "ppdu_visual_item.h"

class QPainter;

enum class LatencyMetric
{
    QueueingDelay,
    MacEndToEndDelay
};

class LatencyChartWidget : public QWidget
{
    Q_OBJECT
  public:
    explicit LatencyChartWidget(LatencyMetric metric, QWidget* parent = nullptr);

    void appendPpdu(const PpduVisualItem& ppdu);
    void reset();
    void setCdfMode(bool enabled);
    bool cdfMode() const;

  protected:
    void paintEvent(QPaintEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void wheelEvent(QWheelEvent* event) override;
    void enterEvent(QEnterEvent* event) override;
    void leaveEvent(QEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;

  private:
    void scheduleUpdate();
    double sampleUs(int index) const;
    double averageUs() const;
    int indexFromX(int x, int plotLeft, int plotWidth) const;
    uint64_t sampleTimeNs(int index) const;
    int visibleCountForWidth(int plotWidth) const;
    int viewEndIndex() const;
    void updateScrollBars();
    QString titleText() const;
    QString subtitleText() const;
    QString yAxisText() const;
    QString tooltipTitleText() const;
    uint64_t metricValueNs(const PpduVisualItem& ppdu) const;
    QVector<double> sortedSampleMs() const;
    QRect plotRectForCard(const QRect& card) const;
    void paintTimeSeries(QPainter& p, const QRect& card, const QRect& plot);
    void paintCdf(QPainter& p, const QRect& card, const QRect& plot);
    void updateScrollBarVisibility();
    int cdfIndexFromX(int x, const QRect& plot, const QVector<double>& sortedMs) const;

    QVector<uint64_t> m_sampleTimeNs;
    QVector<uint32_t> m_sampleLatencyUsX10;
    QVector<PpduVisualItem> m_sampleItems;
    bool m_updateQueued = false;
    LatencyMetric m_metric;

    bool m_hovering = false;
    int m_hoverIndex = -1;
    int m_cdfHoverIndex = -1;
    bool m_cdfMode = false;

    QScrollBar* m_hScroll = nullptr;
    QScrollBar* m_vScroll = nullptr;

    int m_viewStartIndex = 0;
    int m_viewCount = 0;
    double m_xZoom = 1.0;
    double m_yZoom = 1.0;

    uint64_t m_totalLatencyUsX10Sum = 0;
    uint64_t m_totalSampleCount = 0;
};
