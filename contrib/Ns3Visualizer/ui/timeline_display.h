// timeline_display.h
#pragma once

#include <QPointer>
#include <QWidget>
#include <QVector>

#include <QFileDialog>
#include <QInputDialog>

#include "ppdu_visual_item.h"
#include "utils.h"

class PpduTimelineView;
class ThroughputChartWidget;
class LatencyChartWidget;
class PpduCompositionChartWidget;
class NodeThroughputChartWidget;
class RxOutcomeChartWidget;
class McsDistributionChartWidget;
class PhyStatePieChartWidget;
class QProcess;
class QLabel;
class QStackedWidget;
class QPlainTextEdit;
class QPushButton;

namespace Ui {
class Timeline_Display;
}

class Timeline_Display : public QWidget,public ResettableBase
{
    Q_OBJECT
public:
    explicit Timeline_Display(QWidget *parent = nullptr);
    ~Timeline_Display();


    PpduTimelineView* timelineView() const;
    void resetPage() override;
    void clearOutput();
    void attachProcess(QProcess* process);

    void appendOutput(const QString &text);
    void appendPpdu(const PpduVisualItem &ppdu);
    void showSniffFail();

private:
    void setMetricsPage(int index);
    void setLatencyPage(int index);
    void setLatencyCdfMode(bool enabled);
    void syncLatencyCdfButton();
    void showOutputWindow();

    Ui::Timeline_Display *ui;
    PpduTimelineView *m_timelineView = nullptr;
    ThroughputChartWidget *m_throughputChart = nullptr;
    LatencyChartWidget *m_queueingDelayChart = nullptr;
    LatencyChartWidget *m_macE2eDelayChart = nullptr;
    PpduCompositionChartWidget *m_compositionChart = nullptr;
    NodeThroughputChartWidget *m_nodeThroughputChart = nullptr;
    RxOutcomeChartWidget *m_rxOutcomeChart = nullptr;
    McsDistributionChartWidget *m_mcsDistributionChart = nullptr;
    PhyStatePieChartWidget *m_phyStatePieChart = nullptr;
    QPlainTextEdit *m_outputView = nullptr;
    QStackedWidget *m_latencyStack = nullptr;
    QLabel *m_latencyTitle = nullptr;
    QPushButton *m_latencyCdfButton = nullptr;
    QVector<QString> m_latencyTitles;
    int m_currentLatencyPage = 0;
    QStackedWidget *m_metricsStack = nullptr;
    QLabel *m_metricsTitle = nullptr;
    QVector<QString> m_metricTitles;
    int m_currentMetricsPage = 0;
    QPointer<QProcess> m_process;
    QString m_outputText;
};
