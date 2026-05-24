#include "timeline_window.h"

#include "ppdu_timeline_view.h"

#include <QCloseEvent>
#include <QHBoxLayout>
#include <QStatusBar>
#include <QTimer>
#include <QWidget>

TimelineWindow::TimelineWindow(QWidget* parent)
    : QMainWindow(parent)
{
    auto* central = new QWidget(this);
    auto* layout = new QHBoxLayout(central);
    layout->setContentsMargins(0, 0, 0, 0);

    m_timelineDisplay = new Timeline_Display(central);
    m_detailWindow = new PpduDetailWindow(central);
    m_detailWindow->setMinimumWidth(360);
    m_detailWindow->setMaximumWidth(420);
    m_timelineDisplay->timelineView()->setDetailWindow(m_detailWindow);
    m_timelineDisplay->timelineView()->enableQuitButton(false);

    layout->addWidget(m_timelineDisplay, 1);
    layout->addWidget(m_detailWindow);

    setCentralWidget(central);
    setWindowTitle("Ns3 Visualizer Timeline");
    resize(1600, 900);

    statusBar()->showMessage("Timeline mode");
}

TimelineWindow::~TimelineWindow()
{
    stopRuntime();
}

void TimelineWindow::attachSimulationProcess(QProcess* process)
{
    m_simulationProcess = process;
    if (!m_simulationProcess)
    {
        return;
    }

    connect(m_simulationProcess, &QProcess::readyReadStandardOutput, this, [this]() {
        m_timelineDisplay->appendOutput(QString::fromUtf8(m_simulationProcess->readAllStandardOutput()));
    });

    connect(m_simulationProcess, &QProcess::readyReadStandardError, this, [this]() {
        m_timelineDisplay->appendOutput(QString::fromUtf8(m_simulationProcess->readAllStandardError()));
    });

    connect(
        m_simulationProcess,
        QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
        this,
        [this](int, QProcess::ExitStatus) {
            QTimer::singleShot(400, this, [this]() {
                if (!m_hasPpdu)
                {
                    m_timelineDisplay->showSniffFail();
                }
                if (m_ppduReader)
                {
                    m_ppduReader->stop();
                }
            });
        });

    m_timelineDisplay->attachProcess(m_simulationProcess);
}

void TimelineWindow::startReader()
{
    if (m_ppduThread || m_ppduReader)
    {
        return;
    }

    m_ppduThread = new QThread(this);
    m_ppduReader = new QtPpduReader;
    m_ppduReader->moveToThread(m_ppduThread);

    connect(m_ppduThread, &QThread::started, m_ppduReader, &QtPpduReader::run);
    connect(m_ppduReader, &QtPpduReader::finished, this, [this]() {
        if (m_ppduThread)
        {
            m_ppduThread->quit();
        }
    });

    connect(m_ppduReader, &QtPpduReader::ppduReady, this, [this](const PpduVisualItem& item) {
        if (item.recordType == RecordType::Ppdu)
        {
            m_hasPpdu = true;
        }
        m_timelineDisplay->timelineView()->append(item);
        m_timelineDisplay->appendPpdu(item);
    });

    m_ppduThread->start();
}

void TimelineWindow::closeEvent(QCloseEvent* event)
{
    stopRuntime();
    event->accept();
}

void TimelineWindow::stopRuntime()
{
    if (m_ppduReader)
    {
        m_ppduReader->stop();
        m_ppduReader->deleteLater();
        m_ppduReader = nullptr;
    }

    if (m_ppduThread)
    {
        m_ppduThread->quit();
        m_ppduThread->wait();
        m_ppduThread->deleteLater();
        m_ppduThread = nullptr;
    }

    if (m_simulationProcess && m_simulationProcess->state() != QProcess::NotRunning)
    {
        m_simulationProcess->kill();
        m_simulationProcess->waitForFinished(1000);
    }
}
