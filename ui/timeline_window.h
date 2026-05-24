#pragma once

#include <QMainWindow>
#include <QProcess>
#include <QThread>

#include "ppdu_detail_window.h"
#include "qt_ppdu_reader.h"
#include "timeline_display.h"

class TimelineWindow : public QMainWindow
{
    Q_OBJECT

  public:
    explicit TimelineWindow(QWidget* parent = nullptr);
    ~TimelineWindow() override;

    void attachSimulationProcess(QProcess* process);
    void startReader();

  protected:
    void closeEvent(QCloseEvent* event) override;

  private:
    void stopRuntime();

    Timeline_Display* m_timelineDisplay = nullptr;
    PpduDetailWindow* m_detailWindow = nullptr;
    QThread* m_ppduThread = nullptr;
    QtPpduReader* m_ppduReader = nullptr;
    QProcess* m_simulationProcess = nullptr;
    bool m_hasPpdu = false;
};
