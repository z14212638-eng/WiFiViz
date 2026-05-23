#ifndef MAINWINDOW_H
#define MAINWINDOW_H
#include <QWidget>
#include <QStack>
#include <QStyle>
#include <QApplication>
#include <QScreen>
#include <QGuiApplication>
#include <QStackedWidget>
#include <QMainWindow>
#include <QDebug>
#include <QTimer>

#include "page1_model_chose.h"
#include "greeting.h"
#include "utils.h"
#include "indus_widget.h"
#include "antennas.h"
#include "ap_config.h"
#include "node_config.h"
#include "configgraphicsview.h"
#include "edca_config.h"
#include "JsonHelper.h"
#include "timeline_display.h"
#include "legend_overlay.h"
#include "ppdu_timeline_view.h"
#include "ppdu_detail_window.h"
#include "node_traffic_panel.h"
#include "simu_config.h"
#include "visualizer_mode.h"

QT_BEGIN_NAMESPACE
namespace Ui
{
    class MainWindow;
}
QT_END_NAMESPACE

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(VisualizerMode mode = VisualizerMode::FullGui, QWidget *parent = nullptr);
    ~MainWindow();
    void closeEvent(QCloseEvent *event) override;
    void switchTo(int index);
    void back();
    void resetMain();
    bool isValidNs3Directory(const QString &path);
    void onBrowseNs3Dir();
    void setNs3Path();

private:
    Ui::MainWindow *ui;

    /*Page Management*/
    QStackedWidget *stack;
    QStack<int> history;

    /*Page Widgets*/
    Page1_model_chose *page1;
    Simu_Config *simuConfig;
    node_config *nodeConfigPage;
    Ap_config *apConfigPage;
    Edca_config *edcaConfig;//ui-only
    Antenna *antenna;//ui-only
    Timeline_Display *timelineDisplay;
    Greeting *greetingPage;

    /*Menu_Actions & SideBar_Actions*/
    QAction *homeAction;
    QWidget *rightSidebar = nullptr;
    QStackedWidget *rightSidebarStack = nullptr;
    PpduDetailWindow *ppduInspector = nullptr;
    NodeTrafficPanel *nodeTrafficPanel = nullptr;

    /*Data Structure*/
    //Make sure this data structure is unique,on no account should it be copied.
    Ap ap_now;
    Sta sta_now;
    BuildingConfig buildingConfig;//including ap_list and sta_list
    JsonHelper jsonhelper;

    /*Prequisites*/
    bool ns3PathValid;
    VisualizerMode m_mode = VisualizerMode::FullGui;

private slots:
    void onSaveProject();
    void onLoadProject(const QString &projectPath);
    void updateSidebarVisibility();
};
#endif // MAINWINDOW_H
