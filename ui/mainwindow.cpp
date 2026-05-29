#include "mainwindow.h"
#include "ui_mainwindow.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QStackedWidget>
#include <QStatusBar>
#include <QToolBar>
#include <QVBoxLayout>
#include <qcombobox.h>
#include <qthreadpool.h>
#include <QFileDialog>
#include <QMessageBox>
#include <QInputDialog>
#include <QDir>
#include <QDateTime>

MainWindow::MainWindow(VisualizerMode mode, QWidget *parent)
    : QMainWindow(parent), ui(new Ui::MainWindow), m_mode(mode) {
  ui->setupUi(this);

  /*ToolBar*/
  QToolBar *topToolBar = new QToolBar("Top Toolbar", this);
  addToolBar(Qt::TopToolBarArea, topToolBar);
  homeAction = topToolBar->addAction("Home(don't click me[doge])");
  // connect(homeAction, &QAction::triggered, this, [=]() { switchTo(0); });
  QAction *pathAction =
      new QAction(QIcon(":/icons/folder.svg"), tr("NS-3 Path"), this);

  pathAction->setToolTip(tr("Set NS-3 working directory"));
  topToolBar->addAction(pathAction);

  connect(pathAction, &QAction::triggered, this, &MainWindow::onBrowseNs3Dir);
  topToolBar->setIconSize(QSize(24, 24));
  topToolBar->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);

  /*Central Widget*/
  QWidget *central = new QWidget(this);
  setCentralWidget(central);

  QHBoxLayout *mainLayout = new QHBoxLayout(central);
  mainLayout->setContentsMargins(0, 0, 0, 0);

  /*Right Sidebar*/
  rightSidebar = new QWidget(central);
  rightSidebar->setObjectName("ppduInspectorSidebar");
  rightSidebar->setMinimumWidth(360);
  rightSidebar->setMaximumWidth(420);
  rightSidebar->setAttribute(Qt::WA_StyledBackground, true);
  rightSidebar->setAutoFillBackground(true);
  rightSidebar->setStyleSheet(
      "QWidget#ppduInspectorSidebar {"
      "  background: #F3F6FA;"
      "  border-left: 1px solid #2F3B49;"
      "}");

  QVBoxLayout *rightLayout = new QVBoxLayout(rightSidebar);
  rightLayout->setContentsMargins(12, 12, 12, 12);
  rightLayout->setSpacing(10);

  rightSidebarStack = new QStackedWidget(rightSidebar);
  rightSidebarStack->setObjectName("rightSidebarStack");
  rightSidebarStack->setAttribute(Qt::WA_StyledBackground, true);
  rightSidebarStack->setAutoFillBackground(true);
  rightSidebarStack->setStyleSheet(
      "QStackedWidget#rightSidebarStack {"
      "  background: #F3F6FA;"
      "}"
  );
  ppduInspector = new PpduDetailWindow(rightSidebarStack);
  nodeTrafficPanel = new NodeTrafficPanel(rightSidebarStack);
  nodeTrafficPanel->setJsonHelper(&jsonhelper);
  rightSidebarStack->addWidget(nodeTrafficPanel);
  rightSidebarStack->addWidget(ppduInspector);
  rightLayout->addWidget(rightSidebarStack, 1);

  stack = new QStackedWidget(central);

  /*Pages*/
  page1 = new Page1_model_chose(this);
  simuConfig = new Simu_Config(this);
  nodeConfigPage = new node_config(this);
  apConfigPage = new Ap_config(this);
  edcaConfig = new Edca_config(this);
  antenna = new Antenna(this);
  timelineDisplay = new Timeline_Display(this);
  greetingPage = new Greeting(this);

  if (simuConfig)
    simuConfig->setJsonHelper(&jsonhelper);
  if (page1)
    page1->setJsonHelper(&jsonhelper);

  stack->addWidget(page1);           // index 0
  stack->addWidget(simuConfig);      // index 1
  stack->addWidget(nodeConfigPage);  // index 2
  stack->addWidget(apConfigPage);    // index 3
  stack->addWidget(edcaConfig);      // index 4
  stack->addWidget(antenna);         // index 5
  stack->addWidget(timelineDisplay); // index 6
  stack->addWidget(greetingPage);

  switchTo(stack->indexOf(greetingPage));

  /*Layout Assemble*/
  mainLayout->addWidget(stack, 1);
  mainLayout->addWidget(rightSidebar);

  timelineDisplay->timelineView()->setDetailWindow(ppduInspector);
  timelineDisplay->timelineView()->enableQuitButton(m_mode == VisualizerMode::FullGui);

  /*Signal & Slot*/

  /*greetingPage*/
  connect(greetingPage, &Greeting::nextPage, this, [=]() {
    const QString currentPath = greetingPage ? greetingPage->ns3Path : QString();
    if (currentPath.isEmpty() || !isValidNs3Directory(currentPath)) {
      QMessageBox::warning(
          this, tr("NS-3"),
          tr("The selected directory is not a valid NS-3 directory."));
      ns3PathValid = false;
      return;
    }
    ns3PathValid = true;
    if (page1)
      page1->ns3Path = currentPath;
    if (page1)
      page1->refreshModelLists();
    if (simuConfig)
      simuConfig->setNs3Path(currentPath);
    if (greetingPage)
      greetingPage->setNs3Path(currentPath);
    switchTo(0);
  });
  /*Page_1_model_chose*/
  connect(page1, &Page1_model_chose::BackToMain, this, [=]() { switchTo(stack->indexOf(greetingPage)); });

  connect(page1, &Page1_model_chose::ConfigSimulation, this,
          [=]() {
            if (simuConfig)
              simuConfig->setSelectedScene(QString());
            simuConfig->resetSimuScene();
            switchTo(1);
          });

  connect(page1, &Page1_model_chose::RunSelectedSimulation, this,
          [=]() {
            if (!ns3PathValid) {
              QMessageBox::warning(
                  this,
                  tr("NS-3 not configured"),
                  tr("Please set the NS-3 directory before starting a simulation."));
              return;
            }
            const QString sceneName = page1->GetSceneName();
            if (sceneName.isEmpty()) {
              QMessageBox::warning(this, tr("No Scene"),
                                   tr("Please select a scene first."));
              return;
            }
            if (simuConfig) {
              simuConfig->setSelectedScene(sceneName);
            }
            if (timelineDisplay)
              timelineDisplay->resetPage();
            simuConfig->Create_And_StartThread();
            switchTo(6);
          });
  /*Simu_Config*/
  connect(simuConfig, &Simu_Config::BackToLastPage, this,
          [=]() { switchTo(0); });

  connect(simuConfig, &Simu_Config::Building_Set, this, [=]() {
    simuConfig->Write_building_into_config(buildingConfig, *apConfigPage,
                                           *nodeConfigPage);
  });

  connect(simuConfig, &Simu_Config::update, this,
          [=]() { simuConfig->Update_json_map(jsonhelper); });

  connect(simuConfig, &Simu_Config::LoadGeneralConfig, this,
          [=]() { simuConfig->Load_General_Json(jsonhelper); });

  connect(simuConfig, &Simu_Config::ns3OutputReady, timelineDisplay,
          &Timeline_Display::appendOutput);
  connect(simuConfig, &Simu_Config::simulationProcessCreated, timelineDisplay,
          &Timeline_Display::attachProcess, Qt::QueuedConnection);
  connect(simuConfig, &Simu_Config::CreateAndStartThread, this, [=]() {
    auto *tv = timelineDisplay->timelineView();
    tv->Num_ap = simuConfig->Num_ap;
    tv->Num_sta = simuConfig->Num_sta;
    if (timelineDisplay)
      timelineDisplay->resetPage();
    if (timelineDisplay)
      timelineDisplay->clearOutput();
    simuConfig->Generate_And_Build();
  });

  connect(simuConfig, &Simu_Config::generationReady, this, [=]() {
    auto *tv = timelineDisplay->timelineView();
    tv->Num_ap = simuConfig->Num_ap;
    tv->Num_sta = simuConfig->Num_sta;
    if (timelineDisplay)
      timelineDisplay->resetPage();
    simuConfig->RunGeneratedSimulation();
    switchTo(6);
  });

  connect(simuConfig, &Simu_Config::AddAp, this, [=]() {
    apConfigPage->setAvailableTargets(jsonhelper.GetAvailableNodeTargets());
    switchTo(3);
  });

  connect(simuConfig, &Simu_Config::AddSta, this, [=]() {
    nodeConfigPage->setAvailableTargets(jsonhelper.GetAvailableNodeTargets());
    switchTo(2);
  });

  connect(simuConfig, &Simu_Config::NodeContextRequested, this, [=](const QString &nodeType, int nodeId) {
    if (nodeTrafficPanel) {
      nodeTrafficPanel->showNode(nodeType, nodeId);
    }
    if (rightSidebarStack) {
      rightSidebarStack->setCurrentWidget(nodeTrafficPanel);
    }
  });

  connect(simuConfig, &Simu_Config::NodeTrafficSelectionCleared, this, [=]() {
    if (nodeTrafficPanel) {
      nodeTrafficPanel->clearSelection();
    }
    if (stack && stack->currentWidget() == simuConfig && rightSidebarStack) {
      rightSidebarStack->setCurrentWidget(nodeTrafficPanel);
    }
  });

  /*Ap_Config*/
  connect(apConfigPage, &Ap_config::Finish_setting_ap, this,
          [=]() { switchTo(1); });

  connect(apConfigPage, &Ap_config::Page1, this,
          [=]() { apConfigPage->setPosition(ap_now); });
  connect(apConfigPage, &Ap_config::Page2, this,
          [=]() { apConfigPage->setMobility(ap_now); });
  connect(apConfigPage, &Ap_config::Edca_to_config, this,
          [=]() { switchTo(4); });

  connect(apConfigPage, &Ap_config::Antenna_to_config, this,
          [=]() { switchTo(5); });

  connect(apConfigPage, &Ap_config::LoadOneApConfig, this, [=]() {
    apConfigPage->Load_One_Config(this->ap_now);
    apConfigPage->Get_Edca_Config(ap_now, *(edcaConfig));
    apConfigPage->Get_Antenna_Config(ap_now, *(antenna));
    apConfigPage->Get_Traffic_Config(ap_now);  // 转换所有流量配置
    jsonhelper.SetApToJson(&ap_now, apConfigPage->ApIndex);
    
    // 添加到表格
    simuConfig->addApToTable(ap_now.Id, ap_now.m_position, ap_now.Mobility);
    
    ap_now.Traffic_list.clear();
    ap_now.Antenna_list.clear();
    emit simuConfig->update();
  });

  /*Sta_Config*/
  connect(nodeConfigPage, &node_config::Finish_setting_sta, this,
          [=]() { switchTo(1); });
  connect(nodeConfigPage, &node_config::Page1, this,
          [=]() { nodeConfigPage->setPosition(sta_now); });
  connect(nodeConfigPage, &node_config::Page2, this,
          [=]() { nodeConfigPage->setMobility(sta_now); });
  connect(nodeConfigPage, &node_config::Edca_to_config, this,
          [=]() { switchTo(4); });
  connect(nodeConfigPage, &node_config::Antenna_to_config, this,
          [=]() { switchTo(5); });

  connect(nodeConfigPage, &node_config::LoadOneStaConfig, this, [=]() {
    nodeConfigPage->Load_One_Config(sta_now);
    nodeConfigPage->Get_Edca_Config(sta_now, *edcaConfig);
    nodeConfigPage->Get_Antenna_Config(sta_now, *antenna);
    nodeConfigPage->Get_Traffic_Config(sta_now);  // 转换所有流量配置
    jsonhelper.SetStaToJson(&sta_now, nodeConfigPage->StaIndex);
    
    // 添加到表格
    simuConfig->addStaToTable(sta_now.Id, sta_now.m_position, sta_now.Mobility);
    
    sta_now.Traffic_list.clear();
    sta_now.Antenna_list.clear();
    emit simuConfig->update();
  });

  /*Edca_Config*/
  connect(edcaConfig, &Edca_config::BackToLastPage, this, [=]() { back(); });

  /*Antenna_Config*/
  connect(antenna, &Antenna::BackToLastPage, this, [=]() { back(); });

  /*Timeline_Display*/
  connect(simuConfig, &Simu_Config::ppduReady, timelineDisplay->timelineView(),
          &PpduTimelineView::append, Qt::QueuedConnection);
  connect(simuConfig, &Simu_Config::ppduReady, timelineDisplay,
          &Timeline_Display::appendPpdu, Qt::QueuedConnection);
    connect(simuConfig, &Simu_Config::sniffFailed, timelineDisplay,
      &Timeline_Display::showSniffFail, Qt::QueuedConnection);
  /*Timeline_Display*/




  connect(timelineDisplay->timelineView(), &PpduTimelineView::quit_simulation,
          this, [=]() {
            qDebug() << "[MainWindow] quit_simulation received";

            if (m_mode == VisualizerMode::TimelineOnly) {
              close();
              return;
            }

            if (simuConfig) {
              simuConfig->requestCleanup();
            }
            switchTo(stack->indexOf(greetingPage));
            resetMain();
          });




          
  /*Save/Load Project*/
  connect(simuConfig, &Simu_Config::SaveProjectRequested, this, &MainWindow::onSaveProject);
  connect(page1, &Page1_model_chose::LoadProjectRequested, this, &MainWindow::onLoadProject);

  /*Status Bar*/
  QStatusBar *status = new QStatusBar(this);
  setStatusBar(status);
  status->showMessage("Ready");

  /*Default NS-3 path (injected at build time)*/
#ifdef NS3_SOURCE_DIR
  {
    const QString defaultNs3Path = QString(NS3_SOURCE_DIR);
    jsonhelper.Base_dir = defaultNs3Path + "/contrib/wifiviz/Simulation/Designed/";
    if (simuConfig)
      simuConfig->setNs3Path(defaultNs3Path);
    if (page1) {
      page1->ns3Path = defaultNs3Path;
      page1->refreshModelLists();
    }
    ns3PathValid = true;
  }
#endif

  if (m_mode == VisualizerMode::TimelineOnly) {
    switchTo(stack->indexOf(timelineDisplay));
    if (rightSidebarStack) {
      rightSidebarStack->setCurrentWidget(ppduInspector);
    }
  }

  showMaximized();
  updateSidebarVisibility();
}
MainWindow::~MainWindow() {
  qDebug() << "MainWindow destroyed";
  QThreadPool::globalInstance()->clear(); // 取消未开始任务
}

void MainWindow::switchTo(int index) {
  history.push(stack->currentIndex());
  stack->setCurrentIndex(index);
  updateSidebarVisibility();
}

void MainWindow::back() {
  if (!history.isEmpty())
    stack->setCurrentIndex(history.pop());
  updateSidebarVisibility();
}

void MainWindow::closeEvent(QCloseEvent *event) {
  qDebug() << "MainWindow closeEvent";

  // 取消未开始的线程任务
  QThreadPool::globalInstance()->clear();

  event->accept();
}

void MainWindow::resetMain() {
  qDebug() << "[MainWindow] resetMain()";

  // ===============================
  // 1️⃣ 清空导航历史
  // ===============================ap
  history.clear();

  // ===============================
  // 2️⃣ 停止 & reset 仿真核心页
  // ===============================
  if (simuConfig) {
    simuConfig->resetPage();
  }

  // ===============================
  // 3️⃣ reset 所有页面（UI 状态）
  // ===============================
  if (page1)
    page1->resetPage();
  if (nodeConfigPage)
    nodeConfigPage->resetPage();
  if (apConfigPage)
    apConfigPage->resetPage();
  if (edcaConfig)
    edcaConfig->resetPage();
  if (antenna)
    antenna->resetPage();
  if (timelineDisplay)
    timelineDisplay->resetPage();
  if (greetingPage)
    greetingPage->resetPage();

  // ===============================
  // 4️⃣ 清空运行期数据结构
  // ===============================
  ap_now = Ap{};
  sta_now = Sta{};
  buildingConfig = BuildingConfig{};
  jsonhelper.reset();
  // ===============================
  // 5️⃣ 回到首页
  // ===============================
  stack->setCurrentIndex(stack->indexOf(greetingPage));
  updateSidebarVisibility();

  qDebug() << "[MainWindow] resetAll() done";
}

void MainWindow::updateSidebarVisibility() {
  if (!rightSidebar || !stack || !timelineDisplay || !rightSidebarStack || !nodeTrafficPanel || !ppduInspector) {
    return;
  }

  const QWidget *current = stack->currentWidget();
  const bool showSidebar = (current == timelineDisplay || (m_mode == VisualizerMode::FullGui && current == simuConfig));
  rightSidebar->setVisible(showSidebar);

  if (current == timelineDisplay) {
    rightSidebarStack->setCurrentWidget(ppduInspector);
  } else if (current == simuConfig) {
    rightSidebarStack->setCurrentWidget(nodeTrafficPanel);
  }
}

void MainWindow::onBrowseNs3Dir() {
  QString dir = QFileDialog::getExistingDirectory(
      this, tr("Select NS-3 Directory"), QDir::homePath(),
      QFileDialog::ShowDirsOnly | QFileDialog::DontResolveSymlinks);

  if (dir.isEmpty())
    return;

  if (isValidNs3Directory(dir)) {
    ns3PathValid = true;
    this->greetingPage->ns3Path = dir;
    this->page1->ns3Path = dir;
    this->page1->refreshModelLists();
    //fix: update simuConfig ns3Path
    jsonhelper.Base_dir = dir + "/contrib/wifiviz/Simulation/Designed/";
    if (simuConfig)
      simuConfig->setNs3Path(dir);
    greetingPage->setNs3Path(dir);
    // QMessageBox::information(this, tr("NS-3"),
    //                          tr("Valid NS-3 directory selected."));
  } else {
    QMessageBox::warning(
        this, tr("NS-3"),
        tr("The selected directory is not a valid NS-3 directory."));
        ns3PathValid = false;
  }
}

bool MainWindow::isValidNs3Directory(const QString &path) {
  QDir dir(path);

  if (!dir.exists())
    return false;

  return dir.exists("src") && dir.exists("scratch");
}

void MainWindow::onSaveProject() {
  qDebug() << "[MainWindow] onSaveProject()";

  // Make sure configuration is up to date
  if (simuConfig) {
    simuConfig->updateCurrentSceneJson(jsonhelper.m_building_config);
  }

  // Ask user for project name
  bool ok;
  QString projectName = QInputDialog::getText(this, tr("Save Project"),
                                              tr("Enter project name:"),
                                              QLineEdit::Normal,
                                              "MySimulationProject", &ok);
  if (!ok || projectName.isEmpty()) {
    return;  // User cancelled or entered empty name
  }

  // Ask user to select a directory where to save the project folder
  QString baseDir = QFileDialog::getExistingDirectory(
      this,
      tr("Select Directory to Save Project"),
      QDir::homePath(),
      QFileDialog::ShowDirsOnly | QFileDialog::DontResolveSymlinks);

  if (baseDir.isEmpty()) {
    return;  // User cancelled
  }

  // Create project folder
  QString projectDir = baseDir + "/" + projectName;
  QDir dir;
  if (dir.exists(projectDir)) {
    auto reply = QMessageBox::question(
        this, tr("Folder Exists"),
        tr("The folder '%1' already exists. Do you want to overwrite it?").arg(projectName),
        QMessageBox::Yes | QMessageBox::No);
    if (reply == QMessageBox::No) {
      return;
    }
    // Remove existing folder
    QDir(projectDir).removeRecursively();
  }

  if (!dir.mkpath(projectDir)) {
    QMessageBox::critical(this, tr("Error"),
                         tr("Failed to create project directory:\n%1").arg(projectDir));
    return;
  }

  // Create subdirectories
  QString apConfigDir = projectDir + "/ApConfigJson";
  QString staConfigDir = projectDir + "/StaConfigJson";
  QString generalConfigDir = projectDir + "/GeneralJson";

  if (!dir.mkpath(apConfigDir) || !dir.mkpath(staConfigDir) || !dir.mkpath(generalConfigDir)) {
    QMessageBox::critical(this, tr("Error"),
                         tr("Failed to create configuration directories."));
    return;
  }

  // Save General Config
  QJsonObject generalConfig = jsonhelper.m_building_config;
  QString generalFilePath = generalConfigDir + "/General.json";
  if (!jsonhelper.SaveJsonObjToRoute(generalConfig, generalFilePath)) {
    QMessageBox::critical(this, tr("Error"),
                         tr("Failed to save general configuration."));
    return;
  }

  // Save AP Configs
  QStringList apFiles;
  for (int i = 0; i < jsonhelper.m_ap_config_list.size(); ++i) {
    QString apFileName = QString("Ap_%1.json").arg(i);
    QString apFilePath = apConfigDir + "/" + apFileName;
    if (!jsonhelper.SaveJsonObjToRoute(jsonhelper.m_ap_config_list[i], apFilePath)) {
      QMessageBox::critical(this, tr("Error"),
                           tr("Failed to save AP configuration %1.").arg(i));
      return;
    }
    apFiles.append(apFileName);
  }

  // Save STA Configs
  QStringList staFiles;
  for (int i = 0; i < jsonhelper.m_sta_config_list.size(); ++i) {
    QString staFileName = QString("Sta_%1.json").arg(i);
    QString staFilePath = staConfigDir + "/" + staFileName;
    if (!jsonhelper.SaveJsonObjToRoute(jsonhelper.m_sta_config_list[i], staFilePath)) {
      QMessageBox::critical(this, tr("Error"),
                           tr("Failed to save STA configuration %1.").arg(i));
      return;
    }
    staFiles.append(staFileName);
  }

  // Save project file with relative paths
  QString projectFilePath = projectDir + "/" + projectName + ".nsproj";
  
  QJsonObject projectObj;
  projectObj["project_name"] = projectName;
  projectObj["work_directory"] = projectDir;
  projectObj["ns3_directory"] = jsonhelper.Base_dir;
  
  // Use relative paths for config folders
  projectObj["ap_config_folder"] = "ApConfigJson";
  projectObj["sta_config_folder"] = "StaConfigJson";
  projectObj["general_config_folder"] = "GeneralJson";
  
  // Config file names (relative to their folders)
  QJsonArray apFilesArray;
  for (const QString &fileName : apFiles) {
    apFilesArray.append(fileName);
  }
  projectObj["ap_config_files"] = apFilesArray;
  
  QJsonArray staFilesArray;
  for (const QString &fileName : staFiles) {
    staFilesArray.append(fileName);
  }
  projectObj["sta_config_files"] = staFilesArray;
  
  projectObj["general_config_file"] = "General.json";
  
  // Timestamps
  QString currentTime = QDateTime::currentDateTime().toString("yyyy-MM-dd HH:mm:ss");
  projectObj["created_time"] = currentTime;
  projectObj["last_modified"] = currentTime;
  
  // Statistics
  projectObj["ap_count"] = jsonhelper.m_ap_config_list.size();
  projectObj["sta_count"] = jsonhelper.m_sta_config_list.size();
  
  // Save project file
  if (!jsonhelper.SaveJsonObjToRoute(projectObj, projectFilePath)) {
    QMessageBox::critical(this, tr("Error"),
                         tr("Failed to save project file."));
    return;
  }

  // Update jsonhelper's project config to reflect the saved state
  jsonhelper.m_project_config.project_name = projectName;
  jsonhelper.m_project_config.work_directory = projectDir;
  jsonhelper.m_project_config.ap_config_folder = apConfigDir;
  jsonhelper.m_project_config.sta_config_folder = staConfigDir;
  jsonhelper.m_project_config.general_config_folder = generalConfigDir;
  jsonhelper.m_project_config.ap_config_files = apFiles;
  jsonhelper.m_project_config.sta_config_files = staFiles;
  jsonhelper.m_project_config.general_config_file = "General.json";
  jsonhelper.m_project_config.created_time = currentTime;
  jsonhelper.m_project_config.last_modified = currentTime;

  QMessageBox::information(this, tr("Success"),
                          tr("Project saved successfully to:\n%1\n\nProject folder contains:\n- %2 AP configurations\n- %3 STA configurations\n- General configuration")
                          .arg(projectDir)
                          .arg(jsonhelper.m_ap_config_list.size())
                          .arg(jsonhelper.m_sta_config_list.size()));
  qDebug() << "Project saved to:" << projectDir;
}

void MainWindow::onLoadProject(const QString &projectPath) {
  qDebug() << "[MainWindow] onLoadProject:" << projectPath;

  // Load the project configuration
  bool success = jsonhelper.LoadProjectConfig(projectPath);

  if (success) {
    qDebug() << "Project loaded from:" << projectPath;
    qDebug() << "  Project name:" << jsonhelper.m_project_config.project_name;
    qDebug() << "  APs:" << jsonhelper.m_ap_config_list.size();
    qDebug() << "  STAs:" << jsonhelper.m_sta_config_list.size();
    
    // Update UI with loaded configuration (follow the original flow)
    if (simuConfig) {
      // Ensure loaded-project flow uses DIY generation path
      simuConfig->resetSimuScene();

      // 1. Load building configuration to simu_config UI (spinbox, combobox, etc.)
      simuConfig->loadConfigFromJson(jsonhelper.m_building_config);
      
      // 2. Write building config and draw the map (same as "Check" button flow)
      //    This calls Draw_the_config_map() which draws the building frame and axes
      simuConfig->Write_building_into_config(buildingConfig, *apConfigPage, *nodeConfigPage);
      
      // 3. Set button state to "Checked" and switch to Node Configuration tab
      simuConfig->setButtonChecked();
      simuConfig->switchToTab(1);
      
      // 4. Update the scene with loaded AP/STA data (adds nodes and updates tables)
      //    This calls addApToTable/addStaToTable for each node
      simuConfig->Update_json_map(jsonhelper);
      
      // 5. Switch to simulation config page in main window
      switchTo(1);
    }
    
    QMessageBox::information(this, tr("Success"),
                           tr("Project loaded successfully!\n\n"
                              "Project: %1\n"
                              "Building: %2 x %3 x %4 m\n"
                              "APs: %5\n"
                              "STAs: %6")
                           .arg(jsonhelper.m_project_config.project_name)
                           .arg(simuConfig->building_range[0], 0, 'f', 1)
                           .arg(simuConfig->building_range[1], 0, 'f', 1)
                           .arg(simuConfig->building_range[2], 0, 'f', 1)
                           .arg(jsonhelper.m_ap_config_list.size())
                           .arg(jsonhelper.m_sta_config_list.size()));
  } else {
    QMessageBox::critical(this, tr("Error"),
                        tr("Failed to load project from:\n%1").arg(projectPath));
    qWarning() << "Failed to load project from:" << projectPath;
  }
}
