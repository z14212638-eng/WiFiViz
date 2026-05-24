#include "timeline_display.h"
#include "ui_timeline_display.h"
#include "latency_chart.h"
#include "mcs_distribution_chart.h"
#include "node_throughput_chart.h"
#include "ppdu_timeline_view.h"
#include "ppdu_composition_chart.h"
#include "phy_state_pie_chart.h"
#include "rx_outcome_chart.h"
#include "throughput_chart.h"

#include <QDialog>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QPlainTextEdit>
#include <QProcess>
#include <QPushButton>
#include <QSignalBlocker>
#include <QStackedWidget>
#include <QTextCursor>
#include <QVBoxLayout>

#include <algorithm>

Timeline_Display::Timeline_Display(QWidget *parent)
    : QWidget(parent)
    , ui(new Ui::Timeline_Display)
{
    ui->setupUi(this);

    m_timelineView = new PpduTimelineView(ui->frame_3);
    m_timelineView->setSizePolicy(
        QSizePolicy::Expanding,
        QSizePolicy::Expanding
    );

    auto *layout = new QVBoxLayout(ui->frame_3);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->addWidget(m_timelineView);

    m_throughputChart = new ThroughputChartWidget(ui->frame);
    auto *chartLayout = new QVBoxLayout(ui->frame);
    chartLayout->setContentsMargins(0, 0, 0, 0);
    chartLayout->addWidget(m_throughputChart);

    auto *rightLayout = qobject_cast<QHBoxLayout*>(ui->widget_3->layout());
    if (rightLayout)
    {
        rightLayout->setContentsMargins(0, 0, 0, 0);
        rightLayout->setSpacing(14);
        auto *outputButton = new QPushButton("Output", ui->widget_3);
        outputButton->setCursor(Qt::PointingHandCursor);
        outputButton->setFixedHeight(32);
        outputButton->setStyleSheet(
            "QPushButton {"
            "background: #E1E6EC;"
            "border: 1px solid #697481;"
            "border-radius: 4px;"
            "padding: 0 12px;"
            "color: #27313D;"
            "font-weight: 700;"
            "}"
            "QPushButton:hover { background: #D4DBE3; }");
        rightLayout->addWidget(outputButton, 0, Qt::AlignTop);
        connect(outputButton, &QPushButton::clicked, this, &Timeline_Display::showOutputWindow);
    }

    delete ui->frame_2->layout();
    if (auto *legacyTextEdit = ui->frame_2->findChild<QWidget*>("textEdit"))
    {
        delete legacyTextEdit;
    }
    auto *terminalSplit = new QHBoxLayout(ui->frame_2);
    terminalSplit->setContentsMargins(0, 0, 0, 0);
    terminalSplit->setSpacing(14);

    auto *latencyFrame = new QFrame(ui->frame_2);
    latencyFrame->setFrameShape(QFrame::StyledPanel);
    latencyFrame->setFrameShadow(QFrame::Raised);
    auto *latencyLayout = new QVBoxLayout(latencyFrame);
    latencyLayout->setContentsMargins(12, 12, 12, 12);
    latencyLayout->setSpacing(8);

    auto *latencyHeaderLayout = new QHBoxLayout();
    latencyHeaderLayout->setContentsMargins(0, 0, 0, 0);
    latencyHeaderLayout->setSpacing(8);

    m_latencyTitle = new QLabel("Queueing Delay", latencyFrame);
    m_latencyTitle->setStyleSheet("font-size: 18px; font-weight: 700; color: #243447;");
    latencyHeaderLayout->addWidget(m_latencyTitle, 1);

    auto *metricsFrame = new QFrame(ui->frame_2);
    metricsFrame->setFrameShape(QFrame::StyledPanel);
    metricsFrame->setFrameShadow(QFrame::Raised);
    auto *metricsLayout = new QVBoxLayout(metricsFrame);
    metricsLayout->setContentsMargins(12, 12, 12, 12);
    metricsLayout->setSpacing(8);

    auto *headerLayout = new QHBoxLayout();
    headerLayout->setContentsMargins(0, 0, 0, 0);
    headerLayout->setSpacing(8);

    m_metricsTitle = new QLabel("Frame Mix", metricsFrame);
    m_metricsTitle->setStyleSheet("font-size: 18px; font-weight: 700; color: #243447;");
    headerLayout->addWidget(m_metricsTitle, 1);

    auto makePageButton = [&](const QString& text) {
        auto *button = new QPushButton(text, metricsFrame);
        button->setCursor(Qt::PointingHandCursor);
        button->setFixedSize(34, 34);
        button->setStyleSheet(
            "QPushButton {"
            "background: #F5F8FC;"
            "border: 1px solid #D5DDE8;"
            "border-radius: 17px;"
            "color: #344255;"
            "font-weight: 700;"
            "}"
            "QPushButton:hover { background: #E7EEF8; }");
        return button;
    };

    m_latencyCdfButton = new QPushButton("CDF View", latencyFrame);
    m_latencyCdfButton->setCursor(Qt::PointingHandCursor);
    m_latencyCdfButton->setCheckable(true);
    m_latencyCdfButton->setToolTip("Toggle CDF view for the current delay chart");
    m_latencyCdfButton->setFixedHeight(34);
    m_latencyCdfButton->setMinimumWidth(86);
    m_latencyCdfButton->setStyleSheet(
        "QPushButton {"
        "background: #F5F8FC;"
        "border: 1px solid #D5DDE8;"
        "border-radius: 6px;"
        "padding: 0 12px;"
        "color: #344255;"
        "font-weight: 700;"
        "}"
        "QPushButton:hover { background: #E7EEF8; }"
        "QPushButton:checked {"
        "background: #DDF6EC;"
        "border-color: #12B886;"
        "color: #087F5B;"
        "}");
    auto *latencyPrevButton = makePageButton("‹");
    auto *latencyNextButton = makePageButton("›");
    latencyHeaderLayout->addWidget(m_latencyCdfButton);
    latencyHeaderLayout->addWidget(latencyPrevButton);
    latencyHeaderLayout->addWidget(latencyNextButton);
    latencyLayout->addLayout(latencyHeaderLayout);

    m_latencyStack = new QStackedWidget(latencyFrame);
    m_queueingDelayChart = new LatencyChartWidget(LatencyMetric::QueueingDelay, m_latencyStack);
    m_macE2eDelayChart = new LatencyChartWidget(LatencyMetric::MacEndToEndDelay, m_latencyStack);
    m_latencyStack->addWidget(m_queueingDelayChart);
    m_latencyStack->addWidget(m_macE2eDelayChart);
    latencyLayout->addWidget(m_latencyStack, 1);

    m_latencyTitles = {"Queueing Delay", "MAC End-to-End Delay"};
    connect(latencyPrevButton, &QPushButton::clicked, this, [this]() {
        const int count = m_latencyStack ? m_latencyStack->count() : 0;
        if (count <= 0)
            return;
        setLatencyPage((m_currentLatencyPage - 1 + count) % count);
    });
    connect(latencyNextButton, &QPushButton::clicked, this, [this]() {
        const int count = m_latencyStack ? m_latencyStack->count() : 0;
        if (count <= 0)
            return;
        setLatencyPage((m_currentLatencyPage + 1) % count);
    });
    connect(m_latencyCdfButton, &QPushButton::clicked, this, [this](bool checked) {
        setLatencyCdfMode(checked);
    });
    setLatencyPage(0);

    auto *prevButton = makePageButton("‹");
    auto *nextButton = makePageButton("›");
    headerLayout->addWidget(prevButton);
    headerLayout->addWidget(nextButton);
    metricsLayout->addLayout(headerLayout);

    m_metricsStack = new QStackedWidget(metricsFrame);
    m_compositionChart = new PpduCompositionChartWidget(m_metricsStack);
    m_nodeThroughputChart = new NodeThroughputChartWidget(m_metricsStack);
    m_rxOutcomeChart = new RxOutcomeChartWidget(m_metricsStack);
    m_mcsDistributionChart = new McsDistributionChartWidget(m_metricsStack);
    m_phyStatePieChart = new PhyStatePieChartWidget(m_metricsStack);
    m_metricsStack->addWidget(m_compositionChart);
    m_metricsStack->addWidget(m_nodeThroughputChart);
    m_metricsStack->addWidget(m_rxOutcomeChart);
    m_metricsStack->addWidget(m_mcsDistributionChart);
    m_metricsStack->addWidget(m_phyStatePieChart);
    metricsLayout->addWidget(m_metricsStack, 1);

    m_metricTitles = {"Frame Mix", "Node Throughput", "RX Outcome", "MCS Distribution", "PHY State Share"};
    connect(prevButton, &QPushButton::clicked, this, [this]() {
        const int count = m_metricsStack ? m_metricsStack->count() : 0;
        if (count <= 0)
            return;
        setMetricsPage((m_currentMetricsPage - 1 + count) % count);
    });
    connect(nextButton, &QPushButton::clicked, this, [this]() {
        const int count = m_metricsStack ? m_metricsStack->count() : 0;
        if (count <= 0)
            return;
        setMetricsPage((m_currentMetricsPage + 1) % count);
    });
    setMetricsPage(0);

    terminalSplit->addWidget(latencyFrame, 1);
    terminalSplit->addWidget(metricsFrame, 1);
}

PpduTimelineView* Timeline_Display::timelineView() const
{
    return m_timelineView;
}

Timeline_Display::~Timeline_Display()
{
    delete ui;
}

void Timeline_Display::resetPage()
{
    if (!m_timelineView)
        return;

    m_timelineView->resetPage();
    if (m_throughputChart)
        m_throughputChart->reset();
    if (m_queueingDelayChart)
        m_queueingDelayChart->reset();
    if (m_macE2eDelayChart)
        m_macE2eDelayChart->reset();
    if (m_compositionChart)
        m_compositionChart->reset();
    if (m_nodeThroughputChart)
        m_nodeThroughputChart->reset();
    if (m_rxOutcomeChart)
        m_rxOutcomeChart->reset();
    if (m_mcsDistributionChart)
        m_mcsDistributionChart->reset();
    if (m_phyStatePieChart)
        m_phyStatePieChart->reset();
    setLatencyPage(0);
    setMetricsPage(0);

    qDebug() << "[Timeline_Display] resetPage() done";
}

void Timeline_Display::clearOutput()
{
    m_outputText.clear();
    if (m_outputView)
        m_outputView->clear();
}

void Timeline_Display::attachProcess(QProcess *process)
{
    m_process = process;
}

void Timeline_Display::appendOutput(const QString &text)
{
    m_outputText.append(text);
    if (m_outputView)
    {
        m_outputView->setPlainText(m_outputText);
        m_outputView->moveCursor(QTextCursor::End);
    }
}

void Timeline_Display::appendPpdu(const PpduVisualItem &ppdu)
{
    if (m_throughputChart)
        m_throughputChart->appendPpdu(ppdu);
    if (m_queueingDelayChart)
        m_queueingDelayChart->appendPpdu(ppdu);
    if (m_macE2eDelayChart)
        m_macE2eDelayChart->appendPpdu(ppdu);
    if (m_compositionChart)
        m_compositionChart->appendPpdu(ppdu);
    if (m_nodeThroughputChart)
        m_nodeThroughputChart->appendPpdu(ppdu);
    if (m_rxOutcomeChart)
        m_rxOutcomeChart->appendPpdu(ppdu);
    if (m_mcsDistributionChart)
        m_mcsDistributionChart->appendPpdu(ppdu);
    if (m_phyStatePieChart)
        m_phyStatePieChart->appendPpdu(ppdu);
}

void Timeline_Display::showSniffFail()
{
    if (m_timelineView)
        m_timelineView->resetPage();
    if (m_throughputChart)
        m_throughputChart->reset();
    if (m_queueingDelayChart)
        m_queueingDelayChart->reset();
    if (m_macE2eDelayChart)
        m_macE2eDelayChart->reset();
    if (m_compositionChart)
        m_compositionChart->reset();
    if (m_nodeThroughputChart)
        m_nodeThroughputChart->reset();
    if (m_rxOutcomeChart)
        m_rxOutcomeChart->reset();
    if (m_mcsDistributionChart)
        m_mcsDistributionChart->reset();
    if (m_phyStatePieChart)
        m_phyStatePieChart->reset();
    setLatencyPage(0);
    setMetricsPage(0);
}

void
Timeline_Display::setLatencyPage(int index)
{
    if (!m_latencyStack || m_latencyStack->count() == 0)
    {
        return;
    }
    m_currentLatencyPage = std::clamp(index, 0, m_latencyStack->count() - 1);
    m_latencyStack->setCurrentIndex(m_currentLatencyPage);
    if (m_latencyTitle && m_currentLatencyPage < m_latencyTitles.size())
    {
        m_latencyTitle->setText(m_latencyTitles[m_currentLatencyPage]);
    }
    syncLatencyCdfButton();
}

void
Timeline_Display::setLatencyCdfMode(bool enabled)
{
    LatencyChartWidget *chart = nullptr;
    if (m_currentLatencyPage == 0)
    {
        chart = m_queueingDelayChart;
    }
    else if (m_currentLatencyPage == 1)
    {
        chart = m_macE2eDelayChart;
    }
    if (!chart)
    {
        return;
    }
    chart->setCdfMode(enabled);
    syncLatencyCdfButton();
}

void
Timeline_Display::syncLatencyCdfButton()
{
    if (!m_latencyCdfButton)
    {
        return;
    }

    bool enabled = false;
    if (m_currentLatencyPage == 0 && m_queueingDelayChart)
    {
        enabled = m_queueingDelayChart->cdfMode();
    }
    else if (m_currentLatencyPage == 1 && m_macE2eDelayChart)
    {
        enabled = m_macE2eDelayChart->cdfMode();
    }

    QSignalBlocker blocker(m_latencyCdfButton);
    m_latencyCdfButton->setChecked(enabled);
    m_latencyCdfButton->setText("CDF View");
}

void
Timeline_Display::setMetricsPage(int index)
{
    if (!m_metricsStack || m_metricsStack->count() == 0)
    {
        return;
    }
    m_currentMetricsPage = std::clamp(index, 0, m_metricsStack->count() - 1);
    m_metricsStack->setCurrentIndex(m_currentMetricsPage);
    if (m_metricsTitle && m_currentMetricsPage < m_metricTitles.size())
    {
        m_metricsTitle->setText(m_metricTitles[m_currentMetricsPage]);
    }
}

void
Timeline_Display::showOutputWindow()
{
    auto *dialog = new QDialog(this);
    dialog->setAttribute(Qt::WA_DeleteOnClose);
    dialog->setWindowTitle("Simulation Output");
    dialog->resize(900, 560);

    auto *layout = new QVBoxLayout(dialog);
    layout->setContentsMargins(14, 14, 14, 14);
    layout->setSpacing(10);

    auto *header = new QLabel("Output", dialog);
    header->setStyleSheet("font-size: 18px; font-weight: 700; color: #26313D;");
    layout->addWidget(header);

    m_outputView = new QPlainTextEdit(dialog);
    m_outputView->setReadOnly(true);
    m_outputView->setPlainText(m_outputText);
    m_outputView->setStyleSheet(
        "QPlainTextEdit {"
        "background: #E8ECEF;"
        "color: #24303B;"
        "border: 1px solid #7A848F;"
        "border-radius: 5px;"
        "padding: 10px;"
        "selection-background-color: #B9C3CE;"
        "}");
    layout->addWidget(m_outputView, 1);
    m_outputView->moveCursor(QTextCursor::End);

    connect(dialog, &QDialog::destroyed, this, [this]() {
        m_outputView = nullptr;
    });
    dialog->show();
}
