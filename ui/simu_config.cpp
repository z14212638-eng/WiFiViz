#include "simu_config.h"
#include "config_ui_style.h"
#include "configgraphicsview.h"
#include "ui_simu_config.h"
#include "visualizer_config.h"

#include <QCheckBox>
#include <QCoreApplication>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QFont>
#include <QFormLayout>
#include <QGraphicsView>
#include <QHBoxLayout>
#include <QInputDialog>
#include <QLabel>
#include <QMessageBox>
#include <QPushButton>
#include <QRegularExpression>
#include <QSpinBox>
#include <QTimer>
#include <QVBoxLayout>
#include <boost/interprocess/shared_memory_object.hpp>

Simu_Config::Simu_Config(QWidget *parent)
    : QWidget(parent), ui(new Ui::Simu_Config) {
  ui->setupUi(this);
  ConfigUiStyle::ApplyConfigTheme(this, ui->tabWidget);
  ui->pushButton_9->setText("Generate");

  centerWindow(this);

  ui->tabWidget->setCurrentIndex(0);
  scene = new QGraphicsScene(this);
  ui->graphicsView->setScene(scene);
  ui->frame->setObjectName("myFrame");
  QString frameQss = R"(
QFrame#myFrame {
    border: 1px solid #BFBFBF;
    border-radius: 4px;
    background-color: #FFFFFF;
}
    
)";

  // Create enlarge button overlay on graphicsView
  m_enlargeButton = new QPushButton("Enlarge Map", ui->graphicsView);
  m_enlargeButton->setStyleSheet("QPushButton {"
                                 "  background-color: rgba(255, 255, 255, 230);"
                                 "  border: 1px solid #2F3B49;"
                                 "  border-radius: 4px;"
                                 "  padding: 8px 15px;"
                                 "  font-weight: bold;"
                                 "  font-size: 11pt;"
                                 "  color: #263442;"
                                 "}"
                                 "QPushButton:hover {"
                                 "  background-color: rgba(221, 235, 255, 240);"
                                 "}");
  m_enlargeButton->setCursor(Qt::PointingHandCursor);
  m_enlargeButton->setFixedSize(140, 40);
  m_enlargeButton->move(10, 10);
  m_enlargeButton->raise();
  connect(m_enlargeButton, &QPushButton::clicked, this,
          &Simu_Config::showEnlargedMap);
}

Simu_Config::~Simu_Config() {
  qDebug() << "Destroying Simu_Config";
  stopPpduReader();

  if (ns3Process) {
    if (ns3Process->state() != QProcess::NotRunning) {
      ns3Process->kill();
      ns3Process->waitForFinished(1000);
    }
    ns3Process->deleteLater();
    ns3Process = nullptr;
  }

  delete ui; // 最后 delete UI
}

void Simu_Config::refreshFlowTargetSources() {
  // Target source propagation is handled by MainWindow when switching to
  // AP/STA configuration pages. Keep this hook so delete/clear paths can stay
  // symmetric without coupling Simu_Config to other page instances.
}

// Add a Sta
void Simu_Config::on_pushButton_clicked() { emit AddSta(); }

// Add an AP
void Simu_Config::on_pushButton_4_clicked() { emit AddAp(); }

// Draw the frame of the building
void Simu_Config::Write_building_into_config(BuildingConfig &building,
                                             Ap_config &ap, node_config &sta) {
  /*Fill the local data*/
  building_range = {ui->doubleSpinBox->value(), ui->doubleSpinBox_2->value(),
                    ui->doubleSpinBox_3->value()};

  /*load the data in mainwindow*/
  building.m_range[0] = ui->doubleSpinBox->value();
  building.m_range[1] = ui->doubleSpinBox_2->value();
  building.m_range[2] = ui->doubleSpinBox_3->value();
  ap.Building_range = building.m_range;
  sta.Building_range = building.m_range;
  building.m_building_type = ui->comboBox->currentText();
  building.m_wall_type = ui->comboBox_2->currentText();

  /*Draw the building frame*/
  Draw_the_config_map();
}

// Building config checked [Tab 1 Finished]
void Simu_Config::on_pushButton_7_clicked() {
  /*debug*/
  qDebug() << "Scene pointer:" << scene;
  qDebug() << "Scene item count:" << (scene ? scene->items().size() : -1);

  ui->pushButton_7->setEnabled(false);
  ui->pushButton_7->setText("Checked");
  ui->tabWidget->setCurrentIndex(1);

  emit Building_Set();
}

// Draw the building frame
void Simu_Config::Draw_the_config_map() {
  if (!scene) {
    this->scene = new QGraphicsScene(this);
  }
  const auto items = scene->items();
  for (QGraphicsItem *item : items) {
    if (auto *node = dynamic_cast<NodeItem *>(item)) {
      scene->removeItem(node);
      delete node;
    }
  }
  ui->graphicsView->setScene(scene);
  ui->graphicsView->resetTransform();
  // set the axis direction
  ui->graphicsView->scale(1, -1);
  ui->graphicsView->setRenderHint(QPainter::Antialiasing);

  // restrict the range and set the scale
  double width = this->building_range[0];
  double height = this->building_range[1];
  double scale = 40;
  // draw the building
  QRect sceneRect(0, 0, width * scale, height * scale);
  scene->setSceneRect(sceneRect);
  QGraphicsRectItem *building = scene->addRect(sceneRect, QPen(Qt::black, 2));
  building->setZValue(0);

  // Draw coordinate axes
  drawCoordinateAxes(scene, width, height, scale);

  // Fit the view to show the entire building
  ui->graphicsView->fitInView(sceneRect.adjusted(-80, -20, 20, 20),
                              Qt::KeepAspectRatio);
}

// Enable the json updated
void Simu_Config::update_json(QGraphicsScene *scene,
                              QJsonObject &building_config) {
  qDebug() << "updating json";
  if (!scene)
    return;

  QJsonArray apList = building_config["Ap_pos_list"].toArray();
  QJsonArray staList = building_config["Sta_pos_list"].toArray();

  for (QGraphicsItem *item : scene->items()) {
    auto *node = dynamic_cast<NodeItem *>(item);
    if (!node)
      continue;

    double x = node->pos().x() / 40.0;
    double y = node->pos().y() / 40.0;
    double z = node->z_sim;

    // update the corresponding ap
    if (node->Type == "AP") {
      for (int i = 0; i < apList.size(); ++i) {
        QJsonObject obj = apList[i].toObject();
        if (obj["id"].toInt() == node->m_id) {
          obj["pos"] = QJsonArray{x, y, z};
          apList[i] = obj;
          break;
        }
      }
    }

    // update the corresponding sta
    else if (node->Type == "STA") {
      for (int i = 0; i < staList.size(); ++i) {
        QJsonObject obj = staList[i].toObject();
        if (obj["id"].toInt() == node->m_id) {
          obj["pos"] = QJsonArray{x, y, z};
          staList[i] = obj;
          break;
        }
      }
    }
  }

  building_config["Ap_pos_list"] = apList;
  building_config["Sta_pos_list"] = staList;
}

// update the config_map
void Simu_Config::on_pushButton_8_clicked() { emit update(); }

bool Simu_Config::removeNodeFromScene(const QString &type, int id) {
  if (!scene) {
    return false;
  }

  const auto items = scene->items();
  for (QGraphicsItem *item : items) {
    auto *node = dynamic_cast<NodeItem *>(item);
    if (!node) {
      continue;
    }
    if (node->Type == type && node->m_id == id) {
      scene->removeItem(node);
      delete node;
      return true;
    }
  }

  return false;
}

bool Simu_Config::deleteSelectedNode(const QString &type) {
  if (!m_jsonHelper) {
    return false;
  }

  QTableWidget *table = nullptr;
  if (type == QLatin1String("AP")) {
    table = ui->tableWidget_2;
  } else if (type == QLatin1String("STA")) {
    table = ui->tableWidget;
  } else {
    return false;
  }

  const int row = table->currentRow();
  if (row < 0) {
    QMessageBox::warning(this, tr("Delete Node"),
                         tr("Please select one %1 node first.")
                             .arg(type == QLatin1String("AP") ? tr("AP") : tr("STA")));
    return false;
  }

  auto *idItem = table->item(row, 0);
  if (!idItem) {
    return false;
  }

  bool ok = false;
  const int id = idItem->text().toInt(&ok);
  if (!ok) {
    return false;
  }

  if (!m_jsonHelper->RemoveNodeConfig(type, id)) {
    QMessageBox::warning(this, tr("Delete Node"),
                         tr("Failed to remove %1 %2 from JSON.")
                             .arg(type)
                             .arg(id));
    return false;
  }

  table->removeRow(row);
  removeNodeFromScene(type, id);
  updateLcdNumbers();
  refreshFlowTargetSources();
  emit NodeTrafficSelectionCleared();
  return true;
}

void Simu_Config::clearNodes(const QString &type) {
  if (!m_jsonHelper) {
    return;
  }

  m_jsonHelper->ClearNodeConfigs(type);

  QTableWidget *table = type == QLatin1String("AP") ? ui->tableWidget_2 : ui->tableWidget;
  table->setRowCount(0);

  const auto items = scene ? scene->items() : QList<QGraphicsItem *>{};
  for (QGraphicsItem *item : items) {
    auto *node = dynamic_cast<NodeItem *>(item);
    if (!node) {
      continue;
    }
    if (node->Type == type) {
      scene->removeItem(node);
      delete node;
    }
  }

  updateLcdNumbers();
  refreshFlowTargetSources();
  emit NodeTrafficSelectionCleared();
}

void Simu_Config::Update_json_map(JsonHelper &json_helper) {
  json_helper.ensureRunDirectories();

  update_json(scene, json_helper.m_building_config);

  // Clear the scene
  if (scene) {
    const auto items = scene->items(); // 拷贝，避免遍历时修改
    for (QGraphicsItem *item : items) {
      scene->removeItem(item);
      delete item;
    }
  } else {
    scene = new QGraphicsScene(this);
    ui->graphicsView->setScene(scene);
    ui->graphicsView->resetTransform();
    ui->graphicsView->scale(1, -1);
    ui->graphicsView->setRenderHint(QPainter::Antialiasing);
  }

  // Clear tables
  ui->tableWidget->setRowCount(0);   // STA table
  ui->tableWidget_2->setRowCount(0); // AP table

  double width = building_range[0];
  double height = building_range[1];
  double scale = 40.0;
  QRectF sceneRect(0, 0, width * scale, height * scale);
  scene->setSceneRect(sceneRect);

  // Draw building frame
  QGraphicsRectItem *building = scene->addRect(sceneRect, QPen(Qt::black, 2));
  building->setZValue(0);

  // Draw coordinate axes
  drawCoordinateAxes(scene, width, height, scale);

  // ---------- AP ----------
  QJsonArray apList = json_helper.m_building_config["Ap_pos_list"].toArray();

  for (const auto &val : apList) {
    QJsonObject ap_pos = val.toObject();
    QJsonArray pos = ap_pos["pos"].toArray();
    if (pos.size() < 3)
      continue;

    double x = pos[0].toDouble();
    double y = pos[1].toDouble();
    double z = pos[2].toDouble();
    int id = ap_pos["id"].toInt();

    // Add to scene
    auto *ap = new NodeItem(sceneRect, scale);
    ap->m_id = id;
    ap->Type = "AP";
    ap->x_sim = x;
    ap->y_sim = y;
    ap->z_sim = z;

    ap->setBrush(Qt::red);
    ap->setZValue(10);
    ap->setPos(x * scale, y * scale);
    ap->finishInit();

    ap->onPositionCommitted = [this](int id, double nx, double ny, double nz) {
      updateNodePositionInTable(QStringLiteral("AP"), id, nx, ny, nz);
    };
    ap->onContextRequested = [this](const QString &type, int nodeId) {
      emit NodeContextRequested(type, nodeId);
    };

    scene->addItem(ap);

    // Add to table
    bool mobility =
        ap_pos.contains("mobility") ? ap_pos["mobility"].toBool() : false;
    addApToTable(id, QVector<double>{x, y, z}, mobility);

    // 同步 ap_config_list
    for (auto &cfg : json_helper.m_ap_config_list) {
      if (cfg["Id"].toInt() == id) {
        cfg["Position"] = QJsonArray{x, y, z};
        break;
      }
    }
  }

  // ---------- STA ----------
  QJsonArray staList = json_helper.m_building_config["Sta_pos_list"].toArray();

  for (const auto &val : staList) {
    QJsonObject sta_pos = val.toObject();
    QJsonArray pos = sta_pos["pos"].toArray();
    if (pos.size() < 3)
      continue;

    double x = pos[0].toDouble();
    double y = pos[1].toDouble();
    double z = pos[2].toDouble();
    int id = sta_pos["id"].toInt();

    // Add to scene
    auto *sta = new NodeItem(sceneRect, scale);
    sta->m_id = id;
    sta->Type = "STA";
    sta->x_sim = x;
    sta->y_sim = y;
    sta->z_sim = z;

    sta->setBrush(Qt::blue);
    sta->setZValue(10);
    sta->setPos(x * scale, y * scale);
    sta->finishInit();

    sta->onPositionCommitted = [this](int id, double nx, double ny, double nz) {
      updateNodePositionInTable(QStringLiteral("STA"), id, nx, ny, nz);
    };
    sta->onContextRequested = [this](const QString &type, int nodeId) {
      emit NodeContextRequested(type, nodeId);
    };

    scene->addItem(sta);

    // Add to table
    bool mobility =
        sta_pos.contains("mobility") ? sta_pos["mobility"].toBool() : false;
    addStaToTable(id, QVector<double>{x, y, z}, mobility);

    // 同步 sta_config_list
    for (auto &cfg : json_helper.m_sta_config_list) {
      if (cfg["Id"].toInt() == id) {
        cfg["Position"] = QJsonArray{x, y, z};
        break;
      }
    }
  }

  // Update LCD displays (already done by addApToTable/addStaToTable)
  // ui->lcdNumber->display(static_cast<int>(apList.size()));
  // ui->lcdNumber_2->display(static_cast<int>(staList.size()));

  // Fit the view
  ui->graphicsView->fitInView(sceneRect.adjusted(-90, -50, 50, 50),
                              Qt::KeepAspectRatio);

  // Save configs - use original filenames from project config if available
  for (int i = 0; i < json_helper.m_ap_config_list.size(); ++i) {
    const auto &item = json_helper.m_ap_config_list[i];
    QString path;

    // Check if we have project config with original filenames
    if (!json_helper.m_project_config.ap_config_folder.isEmpty() &&
        i < json_helper.m_project_config.ap_config_files.size()) {
      // Use original filename and folder from loaded project
      QString filename = json_helper.m_project_config.ap_config_files[i];
      path = json_helper.m_project_config.ap_config_folder + "/" + filename;
    } else {
      // Use default naming scheme for new projects
      path = json_helper.Ap_file_path +
             QString::number(item["Id"].toInt() + 1) + ".json";
    }

    json_helper.SaveJsonObjToRoute(item, path);
  }

  for (int i = 0; i < json_helper.m_sta_config_list.size(); ++i) {
    const auto &item = json_helper.m_sta_config_list[i];
    QString path;

    // Check if we have project config with original filenames
    if (!json_helper.m_project_config.sta_config_folder.isEmpty() &&
        i < json_helper.m_project_config.sta_config_files.size()) {
      // Use original filename and folder from loaded project
      QString filename = json_helper.m_project_config.sta_config_files[i];
      path = json_helper.m_project_config.sta_config_folder + "/" + filename;
    } else {
      // Use default naming scheme for new projects
      path = json_helper.Sta_file_path +
             QString::number(item["Id"].toInt() + 1) + ".json";
    }

    json_helper.SaveJsonObjToRoute(item, path);
  }

  refreshFlowTargetSources();
}

void Simu_Config::updateNodePositionInTable(const QString &type, int id,
                                            double x, double y, double z) {
  QTableWidget *table = nullptr;
  if (type == QLatin1String("AP")) {
    table = ui->tableWidget_2;
  } else if (type == QLatin1String("STA")) {
    table = ui->tableWidget;
  } else {
    return;
  }

  const QString posText = QString("(%1, %2, %3)")
                              .arg(x, 0, 'f', 2)
                              .arg(y, 0, 'f', 2)
                              .arg(z, 0, 'f', 2);

  for (int row = 0; row < table->rowCount(); ++row) {
    auto *idItem = table->item(row, 0);
    if (!idItem)
      continue;

    bool ok = false;
    const int rowId = idItem->text().toInt(&ok);
    if (!ok || rowId != id)
      continue;

    auto *posItem = table->item(row, 1);
    if (!posItem) {
      posItem = new QTableWidgetItem();
      table->setItem(row, 1, posItem);
    }
    posItem->setText(posText);
    break;
  }
}

void Simu_Config::cleanupAndExit() {
  static bool cleaning = false;
  if (cleaning)
    return;
  cleaning = true;

  qDebug() << "cleanupAndExit()";

  // 停止 Reader

  stopPpduReader();

  // 停止 ns-3（如果还活着）
  if (ns3Process) {
    qDebug() << "Killing ns-3";
    ns3Process->kill();
    ns3Process->waitForFinished(1000);
    ns3Process->deleteLater();
    ns3Process = nullptr;
  }

  if (!m_copiedScratchPath.isEmpty()) {
      // 保留 scratch 下生成/复制的 .cc 文件，不再自动清理
      m_copiedScratchPath.clear();
  }

  close();
  cleaning = false;
}

void Simu_Config::stopPpduReader() {
  QThread *thread = m_ppduThread;
  QtPpduReader *reader = m_ppduReader;

  m_ppduThread = nullptr;
  m_ppduReader = nullptr;

  if (reader) {
    reader->stop();
  }

  if (thread) {
    thread->quit();
    if (!thread->wait(3000)) {
      qWarning() << "PPDU reader thread did not stop in time; terminating";
      thread->terminate();
      thread->wait();
    }
    if (reader) {
      delete reader;
    }
    thread->deleteLater();
  } else if (reader) {
    delete reader;
  }
}

void Simu_Config::resetPage() {
  qDebug() << "[Simu_Config] resetPage()";

  if (m_ppduReader || m_ppduThread) {
    stopPpduReader();
  }

  if (ns3Process) {
    ns3Process->kill();
    ns3Process->waitForFinished(1000);
    ns3Process->deleteLater();
    ns3Process = nullptr;
  }

  if (scene) {
    const auto items = scene->items();
    for (QGraphicsItem *item : items) {
      scene->removeItem(item);
      delete item;
    }
    scene->clear();
  }

  ui->tabWidget->setCurrentIndex(0);

  ui->doubleSpinBox->setValue(10);
  ui->doubleSpinBox_2->setValue(10);
  ui->doubleSpinBox_3->setValue(5);
  ui->doubleSpinBox_4->setValue(10);

  ui->comboBox->setCurrentIndex(0);
  ui->comboBox_2->setCurrentIndex(0);

  ui->tableWidget->setRowCount(0);
  ui->tableWidget_2->setRowCount(0);
  ui->lcdNumber->display(0);
  ui->lcdNumber_2->display(0);

  ui->pushButton_7->setEnabled(true);
  ui->pushButton_7->setText("Check");

  Num_ap = 0;
  Num_sta = 0;
  building_range = {0, 0, 0};
  m_hasPpdu.store(false);
  m_selectedScene.clear();

  ui->graphicsView->resetTransform();
  ui->graphicsView->setScene(scene);

  // rebuildScene();
  qDebug() << "[Simu_Config] resetPage() done";
}

void Simu_Config::setNs3Path(const QString &path) { m_ns3Path = path; }

void Simu_Config::setSelectedScene(const QString &sceneName) {
  m_selectedScene = sceneName;
}

void Simu_Config::resetSimuScene() { m_selectedScene.clear(); }

void Simu_Config::rebuildScene() {
  if (!scene)
    scene = new QGraphicsScene(this);

  scene->clear();

  if (building_range[0] <= 0 || building_range[1] <= 0) {
    qDebug() << "[Simu_Config] skip draw building (invalid range)";
    return;
  }

  Draw_the_config_map();
}

void Simu_Config::requestCleanup() {
  QTimer::singleShot(0, this, &Simu_Config::cleanupAndExit);
}

void Simu_Config::Load_General_Json(JsonHelper &json_helper) {
  json_helper.ensureRunDirectories();
  QJsonObject Building;
  double x_range = ui->doubleSpinBox->value();
  double y_range = ui->doubleSpinBox_2->value();
  double z_range = ui->doubleSpinBox_3->value();
  QJsonArray range = {x_range, y_range, z_range};

  json_helper.m_building_config["Building"] =
      QJsonObject{{"range", range},
                  {"building_type", ui->comboBox->currentText()},
                  {"wall_type", ui->comboBox_2->currentText()}};
  json_helper.m_building_config["Ap_num"] = ui->lcdNumber->intValue();
  json_helper.m_building_config["Sta_num"] = ui->lcdNumber_2->intValue();
  json_helper.m_building_config["SimulationTime"] =
      ui->doubleSpinBox_4->value();

  QString filepath = json_helper.General_file_path + "General.json";
  json_helper.SaveJsonObjToRoute(json_helper.m_building_config, filepath);
  qDebug() << "Save general config to " << filepath;

  this->Num_ap = ui->lcdNumber->intValue();
  this->Num_sta = ui->lcdNumber_2->intValue();
}

bool Simu_Config::prepareSceneScript(QString &outSceneFileName) {
  if (!m_selectedScene.isEmpty()) {
    const QString scratchPath =
        m_ns3Path + "/scratch/" + m_selectedScene + ".cc";
    if (!QFile::exists(scratchPath)) {
      const QString simplePath =
          m_ns3Path + "/contrib/wifiviz/Simulation/Default/Simple/" +
          m_selectedScene + "/" + m_selectedScene + ".cc";
      const QString complexPath =
          m_ns3Path + "/contrib/wifiviz/Simulation/Default/Complex/" +
          m_selectedScene + "/" + m_selectedScene + ".cc";

      QString sourcePath;
      if (QFile::exists(simplePath)) {
        sourcePath = simplePath;
      } else if (QFile::exists(complexPath)) {
        sourcePath = complexPath;
      }

      if (sourcePath.isEmpty()) {
        qWarning() << "Scene source not found in Default folders:"
                   << m_selectedScene;
        return false;
      }

      if (!QFile::copy(sourcePath, scratchPath)) {
        qWarning() << "Failed to copy scene to scratch:" << scratchPath;
        return false;
      }

      m_copiedScratchPath = scratchPath;
    }
    outSceneFileName = m_selectedScene + ".cc";
    return true;
  }

  if (!generateStandaloneScript(outSceneFileName)) {
    qWarning() << "Failed to generate standalone script";
    return false;
  }

  return true;
}

void Simu_Config::Generate_And_Build() {
  if (m_ppduReader || m_ppduThread) {
    stopPpduReader();
  }

  if (ns3Process) {
    qWarning() << "Generation/build already running";
    return;
  }

  if (m_ns3Path.isEmpty()) {
    qWarning() << "NS-3 path not set";
    emit ns3OutputReady("NS-3 path not set.\n");
    return;
  }

  m_hasPpdu.store(false);
  m_copiedScratchPath.clear();

  QString sceneToRun;
  if (!prepareSceneScript(sceneToRun)) {
    emit ns3OutputReady("Failed to generate simulation script.\n");
    return;
  }
  m_lastGeneratedSceneFileName = sceneToRun;

  emit ns3OutputReady(QString("Generated scratch script: %1/scratch/%2\n")
                          .arg(m_ns3Path, sceneToRun));

  ns3Process = new QProcess(this);
  ns3Process->setWorkingDirectory(m_ns3Path);

  connect(ns3Process, &QProcess::started, this, [] {
    qDebug() << "ns-3 build started";
  });

  connect(ns3Process, &QProcess::readyReadStandardOutput, this, [this]() {
    emit ns3OutputReady(QString::fromUtf8(ns3Process->readAllStandardOutput()));
  });

  connect(ns3Process, &QProcess::readyReadStandardError, this, [this]() {
    emit ns3OutputReady(QString::fromUtf8(ns3Process->readAllStandardError()));
  });

  connect(ns3Process, &QProcess::errorOccurred, this,
          [this](QProcess::ProcessError error) {
            emit ns3OutputReady(QString("[ns3 build error] code=%1\n")
                                    .arg(static_cast<int>(error)));
          });

  connect(ns3Process,
          QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished), this,
          [this](int code, QProcess::ExitStatus status) {
            qDebug() << "ns-3 build finished:" << code << status;
            emit ns3OutputReady(QString("ns-3 build finished: exitCode=%1 status=%2\n")
                                    .arg(code)
                                    .arg(status == QProcess::NormalExit
                                             ? "NormalExit"
                                             : "CrashExit"));
            ns3Process->deleteLater();
            ns3Process = nullptr;

            if (code == 0 && status == QProcess::NormalExit) {
              emit generationReady();
            }
          });

  emit ns3OutputReady("Running ./ns3 build\n");
  ns3Process->start("./ns3", {"build"});
}

void Simu_Config::StartVisualizationReader() {
  if (m_ppduReader || m_ppduThread) {
    stopPpduReader();
  }

  if (m_ppduThread || m_ppduReader) {
    qWarning() << "Visualization reader already running";
    return;
  }

  m_hasPpdu.store(false);
  boost::interprocess::shared_memory_object::remove("Ns3PpduSharedMemory");

  m_ppduThread = new QThread(this);
  m_ppduReader = new QtPpduReader;

  m_ppduReader->moveToThread(m_ppduThread);

  connect(m_ppduThread, &QThread::started, m_ppduReader, &QtPpduReader::run);

  connect(
      m_ppduReader, &QtPpduReader::ppduReady, this,
      [this](const PpduVisualItem &item) {
        if (item.recordType == RecordType::Ppdu) {
          m_hasPpdu.store(true);
        }
        emit ppduReady(item);
      },
      Qt::DirectConnection);

  // reader 自己结束时，线程退出；不要依赖成员指针，避免同步清理时竞态。
  connect(m_ppduReader, &QtPpduReader::finished, this,
          [thread = m_ppduThread] { thread->quit(); }, Qt::DirectConnection);

  m_ppduThread->start();
}

void Simu_Config::Create_And_StartThread() {

  if (m_ppduReader || m_ppduThread) {
    stopPpduReader();
  }

  if (ns3Process || m_ppduThread || m_ppduReader) {
    qWarning() << "Simulation already running";
    return;
  }

  if (m_ns3Path.isEmpty()) {
    qWarning() << "NS-3 path not set";
    return;
  }

  m_copiedScratchPath.clear();

  QString sceneToRun;
  if (!prepareSceneScript(sceneToRun)) {
    stopPpduReader();
    return;
  }

  ns3Process = new QProcess(this);
  ns3Process->setWorkingDirectory(m_ns3Path);
  {
    QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
    env.insert("WIFIVIZ_DISABLE_VIEWER", "1");
    ns3Process->setProcessEnvironment(env);
  }

  connect(ns3Process, &QProcess::started, this,
          [] { qDebug() << "ns-3 started"; });
  emit simulationProcessCreated(ns3Process);

  connect(ns3Process,
          QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished), this,
          [this](int code, QProcess::ExitStatus status) {
            qDebug() << "ns-3 finished:" << code << status;
            ns3Process->deleteLater();
            ns3Process = nullptr;

            QTimer::singleShot(400, this, [this]() {
              if (!m_copiedScratchPath.isEmpty()) {
                m_copiedScratchPath.clear();
              }
              if (!m_hasPpdu.load()) {
                emit sniffFailed();
              }
              stopPpduReader();
            });
          });

  StartVisualizationReader();

  const QString sceneTarget = QFileInfo(sceneToRun).completeBaseName();
  const QString runTarget = QStringLiteral("%1 --enable-wifiviz=1 --precise=%2 --rough=%3")
                                .arg(sceneTarget)
                                .arg(g_ppduViewConfig.preciseMode.load() ? 1 : 0)
                                .arg(std::max(1, g_ppduViewConfig.sampleRate.load()));
  {
    QProcess buildProcess;
    buildProcess.setWorkingDirectory(m_ns3Path);
    QStringList buildArgs = {"build"};

    emit ns3OutputReady(QString("Building project before run: ./ns3 %1").arg(buildArgs.join(' ')));
    buildProcess.start("./ns3", buildArgs);
    if (!buildProcess.waitForFinished(-1)) {
      qWarning() << "ns-3 build did not finish";
      stopPpduReader();
      return;
    }

    const QByteArray buildOut = buildProcess.readAllStandardOutput();
    if (!buildOut.isEmpty()) {
      emit ns3OutputReady(QString::fromUtf8(buildOut));
    }

    const QByteArray buildErr = buildProcess.readAllStandardError();
    if (!buildErr.isEmpty()) {
      emit ns3OutputReady(QString::fromUtf8(buildErr));
    }

    const int exitCode = buildProcess.exitCode();
    if (exitCode != 0) {
      qWarning() << "ns-3 build failed with code" << exitCode;
      stopPpduReader();
      return;
    }
  }

  /*LINK the Terminal */
  connect(ns3Process, &QProcess::readyReadStandardOutput, this, [this]() {
    QByteArray out = ns3Process->readAllStandardOutput();
    emit ns3OutputReady(QString::fromUtf8(out));
  });

  connect(ns3Process, &QProcess::readyReadStandardError, this, [this]() {
    QByteArray out = ns3Process->readAllStandardError();
    emit ns3OutputReady(QString::fromUtf8(out));
  });

  connect(ns3Process, &QProcess::errorOccurred, this,
          [this](QProcess::ProcessError error) {
            emit ns3OutputReady(QString("[ns3 run error] code=%1").arg(static_cast<int>(error)));
          });

  const QString runCommand = QStringLiteral("./ns3 run \"%1\"").arg(runTarget);
  emit ns3OutputReady(QString("Running command: %1").arg(runCommand));
  ns3Process->start("/bin/bash", {"-lc", runCommand});

  // ===============================
  // 2. Timeline View
  // ===============================
  // if (!m_timelineView) {
  //   m_timelineView = new PpduTimelineView(this);
  //   m_timelineView->Num_ap = this->Num_ap;
  //   m_timelineView->Num_sta = this->Num_sta;
  //   m_timelineView->setWindowTitle("PPDU Timeline");
  //   m_timelineView->resize(1200, 400);
  //   m_timelineView->show();

  //   connect(m_timelineView, &PpduTimelineView::timelineClosed,
  //           this, &Simu_Config::requestCleanup);
}

void Simu_Config::RunGeneratedSimulation() {
  if (m_ppduReader || m_ppduThread) {
    stopPpduReader();
  }

  if (ns3Process || m_ppduThread || m_ppduReader) {
    qWarning() << "Simulation already running";
    return;
  }

  if (m_ns3Path.isEmpty()) {
    qWarning() << "NS-3 path not set";
    emit ns3OutputReady("NS-3 path not set.\n");
    return;
  }

  QString sceneToRun = m_lastGeneratedSceneFileName;
  if (sceneToRun.isEmpty() && !prepareSceneScript(sceneToRun)) {
    emit ns3OutputReady("Failed to prepare simulation script.\n");
    return;
  }
  m_lastGeneratedSceneFileName = sceneToRun;

  ns3Process = new QProcess(this);
  ns3Process->setWorkingDirectory(m_ns3Path);
  {
    QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
    env.insert("WIFIVIZ_DISABLE_VIEWER", "1");
    ns3Process->setProcessEnvironment(env);
  }

  connect(ns3Process, &QProcess::started, this,
          [] { qDebug() << "ns-3 started"; });
  emit simulationProcessCreated(ns3Process);

  connect(ns3Process,
          QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished), this,
          [this](int code, QProcess::ExitStatus status) {
            qDebug() << "ns-3 finished:" << code << status;
            emit ns3OutputReady(QString("ns-3 run finished: exitCode=%1 status=%2\n")
                                    .arg(code)
                                    .arg(status == QProcess::NormalExit
                                             ? "NormalExit"
                                             : "CrashExit"));
            ns3Process->deleteLater();
            ns3Process = nullptr;

            QTimer::singleShot(400, this, [this]() {
              if (!m_copiedScratchPath.isEmpty()) {
                m_copiedScratchPath.clear();
              }
              if (!m_hasPpdu.load()) {
                emit sniffFailed();
              }
              stopPpduReader();
            });
          });

  StartVisualizationReader();

  const QString sceneTarget = QFileInfo(sceneToRun).completeBaseName();
  const QString runTarget = QStringLiteral("%1 --enable-wifiviz=1 --precise=%2 --rough=%3")
                                .arg(sceneTarget)
                                .arg(g_ppduViewConfig.preciseMode.load() ? 1 : 0)
                                .arg(std::max(1, g_ppduViewConfig.sampleRate.load()));

  connect(ns3Process, &QProcess::readyReadStandardOutput, this, [this]() {
    emit ns3OutputReady(QString::fromUtf8(ns3Process->readAllStandardOutput()));
  });

  connect(ns3Process, &QProcess::readyReadStandardError, this, [this]() {
    emit ns3OutputReady(QString::fromUtf8(ns3Process->readAllStandardError()));
  });

  connect(ns3Process, &QProcess::errorOccurred, this,
          [this](QProcess::ProcessError error) {
            emit ns3OutputReady(QString("[ns3 run error] code=%1\n")
                                    .arg(static_cast<int>(error)));
          });

  const QString runCommand = QStringLiteral("./ns3 run \"%1\"").arg(runTarget);
  emit ns3OutputReady(QString("Running command: %1\n").arg(runCommand));
  ns3Process->start("/bin/bash", {"-lc", runCommand});
}

QString Simu_Config::resolveUtilsBuildScriptPath() const {
  // QDir dir(QCoreApplication::applicationDirPath());
  QDir dir(m_ns3Path);
  for (int i = 0; i < 5; ++i) {
    const QString candidate = dir.absoluteFilePath("contrib/wifiviz/ui/utils/build.sh");
    if (QFileInfo::exists(candidate)) {
      return candidate;
    }
    if (!dir.cdUp()) {
      break;
    }
  }
  return {};
}

QString
Simu_Config::resolveScriptGeneratorPath(const QString &buildScriptPath) const {
  if (buildScriptPath.isEmpty()) {
    return {};
  }
  const QDir utilsDir = QFileInfo(buildScriptPath).dir();
  return utilsDir.absoluteFilePath("build/wifiviz-script-generator");
}

bool Simu_Config::ensureScriptGeneratorBuilt(QString &outGeneratorPath) {
  // const QString buildScript = resolveUtilsBuildScriptPath();
  // if (buildScript.isEmpty()) {
  //   const QString msg =
  //       tr("Cannot locate utils/build.sh. Please check installation.");
  //   emit ns3OutputReady(msg);
  //   qWarning() << msg;
  //   return false;
  // }

  // const QDir utilsDir = QFileInfo(buildScript).dir();
  // QProcess buildProcess;
  // buildProcess.setWorkingDirectory(utilsDir.path());
  // buildProcess.start("bash", {buildScript, m_ns3Path});
  // if (!buildProcess.waitForFinished(-1)) {
  //   qWarning() << "Generator build did not finish";
  //   return false;
  // }

  // const QByteArray buildOut = buildProcess.readAllStandardOutput();
  // if (!buildOut.isEmpty()) {
  //   emit ns3OutputReady(QString::fromUtf8(buildOut));
  // }

  // const QByteArray buildErr = buildProcess.readAllStandardError();
  // if (!buildErr.isEmpty()) {
  //   emit ns3OutputReady(QString::fromUtf8(buildErr));
  // }

  // if (buildProcess.exitCode() != 0) {
  //   qWarning() << "Generator build failed with code" << buildProcess.exitCode();
  //   return false;
  // }

  const QString generatorPath = m_ns3Path + "/build/wifiviz-script-generator";

  if (!QFileInfo::exists(generatorPath)) {
    qWarning() << "Generator executable not found:" << generatorPath;
    return false;
  }

  outGeneratorPath = generatorPath;
  return true;
}

QString Simu_Config::generatedScratchScriptPath(const QString &projectName) const {
  QString safeName = projectName;
  safeName.replace(QRegularExpression(QStringLiteral("[^A-Za-z0-9_-]")), QStringLiteral("_"));
  if (safeName.isEmpty()) {
    safeName = QStringLiteral("ns3visualizer");
  }

  const QRegularExpression designedNamePattern(QStringLiteral("^Designed_\\d{2}_\\d{1,2}_\\d{1,2}_\\d{3,6}$"));
  if (!designedNamePattern.match(safeName).hasMatch()) {
    const QString stamp = QDateTime::currentDateTime().toString(QStringLiteral("yyyy_MMdd_HHmmss"));
    safeName = QStringLiteral("%1_%2").arg(safeName, stamp);
  }

  return QStringLiteral("%1/scratch/%2.cc").arg(m_ns3Path, safeName);
}

void Simu_Config::cleanupOldGeneratedScratchScripts(const QString &keepFilePath) const {
  if (m_ns3Path.isEmpty()) {
    return;
  }

  QDir scratchDir(m_ns3Path + "/scratch");
  if (!scratchDir.exists()) {
    return;
  }

  const QString keepName = QFileInfo(keepFilePath).fileName();
  const QStringList files = scratchDir.entryList(QStringList() << "generated_*.cc"
                                                             << "ns3visualizer-generated.cc"
                                                             << "*_????_????_??????.cc",
                                                 QDir::Files);
  for (const QString &fileName : files) {
    if (fileName == keepName) {
      continue;
    }
    scratchDir.remove(fileName);
  }
}

bool Simu_Config::generateStandaloneScript(QString &outSceneFileName) {
  if (!m_jsonHelper) {
    qWarning() << "JsonHelper not set";
    return false;
  }

  m_jsonHelper->ensureRunDirectories();
  const QString projectPath = m_jsonHelper->Run_dir;
  if (projectPath.isEmpty()) {
    qWarning() << "Project path is empty";
    return false;
  }

  QString generatorPath;
  if (!ensureScriptGeneratorBuilt(generatorPath)) {
    return false;
  }

  QString projectName = QDir(projectPath).dirName();
  if (projectName.isEmpty()) {
    projectName = "GeneratedProject";
  }

  if (!m_jsonHelper->PersistAllJson()) {
    emit ns3OutputReady(tr("Failed to persist JSON before script generation."));
    qWarning() << "Failed to persist JSON before generation";
    return false;
  }

  const QString outputPath = generatedScratchScriptPath(projectName);
  cleanupOldGeneratedScratchScripts(outputPath);
  QFile::remove(outputPath);

  QProcess genProcess;
  genProcess.setWorkingDirectory(QFileInfo(generatorPath).dir().path());
  std::cout<< "Running generator: " << generatorPath.toStdString() <<" "<< projectPath.toStdString() <<" "<< outputPath.toStdString() << std::endl;
  genProcess.start(generatorPath, {projectPath, outputPath});
  if (!genProcess.waitForFinished(-1)) {
    qWarning() << "Script generation did not finish";
    return false;
  }

  const QByteArray genOut = genProcess.readAllStandardOutput();
  if (!genOut.isEmpty()) {
    emit ns3OutputReady(QString::fromUtf8(genOut));
  }

  const QByteArray genErr = genProcess.readAllStandardError();
  if (!genErr.isEmpty()) {
    emit ns3OutputReady(QString::fromUtf8(genErr));
  }

  if (genProcess.exitCode() != 0) {
    qWarning() << "Script generator failed with code" << genProcess.exitCode();
    return false;
  }

  if (!QFileInfo::exists(outputPath)) {
    qWarning() << "Generated script not found:" << outputPath;
    return false;
  }

  outSceneFileName = QFileInfo(outputPath).fileName();
  m_copiedScratchPath = outputPath;
  return true;
}

// Finnal Check , Run the Simulation , Load the General Json
void Simu_Config::on_pushButton_9_clicked() {
  if (ui->lcdNumber->intValue() != 0 && ui->lcdNumber_2->intValue() != 0) {
    if (ui->doubleSpinBox_4->value() <= 0) {
      QMessageBox::critical(this, "Error",
                            "Simulation time must be greater than 0 seconds.",
                            QMessageBox::Ok);
      return;
    }

    auto *msgBox = new QMessageBox(QMessageBox::Information, "Attention",
                                   "Running ./ns3 build\nWaiting for visualization",
                                   QMessageBox::NoButton, this);
    msgBox->setAttribute(Qt::WA_DeleteOnClose);
    msgBox->setWindowModality(Qt::WindowModal);

    // 在窗口真正 show 之后再 move
    QTimer::singleShot(0, this, [this, msgBox]() {
      QRect parentRect = this->frameGeometry();
      QRect boxRect = msgBox->frameGeometry();

      QPoint center = parentRect.center() - boxRect.center();

      msgBox->move(center);
    });
    QTimer::singleShot(1200, msgBox, &QMessageBox::close);
    msgBox->show();
    QCoreApplication::processEvents();
  } else {
    QMessageBox msgBox(QMessageBox::Critical, "Error",
                       "At least one AP and one STA is needed", QMessageBox::Ok,
                       this);

    msgBox.setWindowModality(Qt::WindowModal);

    QTimer::singleShot(0, this, [this, &msgBox]() {
      QRect parentRect = this->frameGeometry();
      QRect boxRect = msgBox.frameGeometry();

      QPoint center = parentRect.center() - boxRect.center();

      msgBox.move(center);
    });

    msgBox.exec();
    return;
  }

  g_ppduViewConfig.preciseMode.store(true);
  g_ppduViewConfig.absoluteRate.store(true);
  g_ppduViewConfig.sampleRate.store(1);

  emit LoadGeneralConfig();
  emit CreateAndStartThread();
}

void Simu_Config::on_pushButton_5_clicked() {
  deleteSelectedNode(QStringLiteral("AP"));
}

void Simu_Config::on_pushButton_6_clicked() {
  clearNodes(QStringLiteral("AP"));
}

void Simu_Config::on_pushButton_2_clicked() {
  deleteSelectedNode(QStringLiteral("STA"));
}

void Simu_Config::on_pushButton_3_clicked() {
  clearNodes(QStringLiteral("STA"));
}

void Simu_Config::addApToTable(int id, const QVector<double> &pos,
                               bool mobility) {
  int row = ui->tableWidget_2->rowCount();
  ui->tableWidget_2->insertRow(row);

  ui->tableWidget_2->setItem(row, 0, new QTableWidgetItem(QString::number(id)));
  ui->tableWidget_2->setItem(row, 1,
                             new QTableWidgetItem(QString("(%1, %2, %3)")
                                                      .arg(pos[0], 0, 'f', 2)
                                                      .arg(pos[1], 0, 'f', 2)
                                                      .arg(pos[2], 0, 'f', 2)));
  ui->tableWidget_2->setItem(row, 2,
                             new QTableWidgetItem(mobility ? "Yes" : "No"));

  updateLcdNumbers();
}

void Simu_Config::addStaToTable(int id, const QVector<double> &pos,
                                bool mobility) {
  int row = ui->tableWidget->rowCount();
  ui->tableWidget->insertRow(row);

  ui->tableWidget->setItem(row, 0, new QTableWidgetItem(QString::number(id)));
  ui->tableWidget->setItem(row, 1,
                           new QTableWidgetItem(QString("(%1, %2, %3)")
                                                    .arg(pos[0], 0, 'f', 2)
                                                    .arg(pos[1], 0, 'f', 2)
                                                    .arg(pos[2], 0, 'f', 2)));
  ui->tableWidget->setItem(row, 2,
                           new QTableWidgetItem(mobility ? "Yes" : "No"));

  updateLcdNumbers();
}

void Simu_Config::updateLcdNumbers() {
  ui->lcdNumber->display(ui->tableWidget_2->rowCount());
  ui->lcdNumber_2->display(ui->tableWidget->rowCount());
}

void Simu_Config::drawCoordinateAxes(QGraphicsScene *targetScene, double width,
                                     double height, double scale) {
  if (!targetScene)
    return;

  // Draw coordinate axes
  QPen axisPen(QColor(40, 40, 40), 3.0);
  QPen tickPen(QColor(60, 60, 60), 2.0);
  QPen gridPen(QColor(200, 200, 200), 1.0, Qt::DashLine);

  // X axis
  auto *xAxis = targetScene->addLine(0, 0, width * scale, 0, axisPen);
  xAxis->setZValue(5);

  // Y axis
  auto *yAxis = targetScene->addLine(0, 0, 0, height * scale, axisPen);
  yAxis->setZValue(5);

  // Draw tick marks and labels
  double tickInterval = 1.0; // 1 meter interval
  if (width > 20)
    tickInterval = 2.0; // Use 2m for larger areas
  if (width > 50)
    tickInterval = 5.0; // Use 5m for very large areas

  const double tickLength = 8;

  // X axis ticks and labels
  for (double x = 0; x <= width; x += tickInterval) {
    double screenX = x * scale;

    // Tick mark
    auto *tick =
        targetScene->addLine(screenX, 0, screenX, -tickLength, tickPen);
    tick->setZValue(5);

    // Grid line (optional)
    if (x > 0 && x < width) {
      auto *gridLine =
          targetScene->addLine(screenX, 0, screenX, height * scale, gridPen);
      gridLine->setZValue(-1);
    }

    // Label
    auto *label = targetScene->addText(QString::number(x, 'f', 1));
    label->setDefaultTextColor(QColor(20, 20, 20));
    label->setFont(QFont("Arial", 10, QFont::Bold));
    label->setZValue(10);
    label->setFlag(QGraphicsItem::ItemIgnoresTransformations);
    label->setPos(screenX - 10, -15);
  }

  // Y axis ticks and labels
  for (double y = 0; y <= height; y += tickInterval) {
    double screenY = y * scale;

    // Tick mark
    auto *tick =
        targetScene->addLine(0, screenY, -tickLength, screenY, tickPen);
    tick->setZValue(5);

    // Grid line (optional)
    if (y > 0 && y < height) {
      auto *gridLine =
          targetScene->addLine(0, screenY, width * scale, screenY, gridPen);
      gridLine->setZValue(-1);
    }

    // Label
    auto *label = targetScene->addText(QString::number(y, 'f', 1));
    label->setDefaultTextColor(QColor(20, 20, 20));
    label->setFont(QFont("Arial", 10, QFont::Bold));
    label->setZValue(10);
    label->setFlag(QGraphicsItem::ItemIgnoresTransformations);
    label->setPos(-30, screenY - 10);
  }

  // Axis labels
  auto *xAxisLabel = targetScene->addText("X (m)");
  xAxisLabel->setDefaultTextColor(QColor(0, 0, 0));
  xAxisLabel->setFont(QFont("Arial", 12, QFont::Bold));
  xAxisLabel->setZValue(10);
  xAxisLabel->setFlag(QGraphicsItem::ItemIgnoresTransformations);
  xAxisLabel->setPos(width * scale / 2 - 20, -30);

  auto *yAxisLabel = targetScene->addText("Y (m)");
  yAxisLabel->setDefaultTextColor(QColor(0, 0, 0));
  yAxisLabel->setFont(QFont("Arial", 12, QFont::Bold));
  yAxisLabel->setZValue(10);
  yAxisLabel->setFlag(QGraphicsItem::ItemIgnoresTransformations);
  yAxisLabel->setPos(-70, height * scale / 2 - 10);
}

void Simu_Config::showEnlargedMap() {
  if (!scene) {
    QMessageBox::information(this, tr("Info"),
                             tr("Please configure the building first."));
    return;
  }

  // Create a separate scene for the enlarged map with axes
  auto *enlargedScene = new QGraphicsScene();

  double width = building_range[0];
  double height = building_range[1];
  double scale = 40.0;

  // Extend scene to include axis space
  const double axisMargin = 80;
  QRectF buildingRect(0, 0, width * scale, height * scale);
  enlargedScene->setSceneRect(-axisMargin, -axisMargin,
                              width * scale + axisMargin * 2,
                              height * scale + axisMargin * 2);

  // Draw the building frame
  QGraphicsRectItem *building =
      enlargedScene->addRect(buildingRect, QPen(Qt::black, 2));
  building->setZValue(0);

  // Draw coordinate axes on the enlarged scene
  drawCoordinateAxes(enlargedScene, width, height, scale);

  // Copy all nodes from the original scene to the enlarged scene
  for (QGraphicsItem *item : scene->items()) {
    if (auto *node = dynamic_cast<NodeItem *>(item)) {
      auto *newNode = new NodeItem(buildingRect, scale);
      newNode->m_id = node->m_id;
      newNode->Type = node->Type;
      newNode->x_sim = node->x_sim;
      newNode->y_sim = node->y_sim;
      newNode->z_sim = node->z_sim;
      newNode->setBrush(node->brush());
      newNode->setZValue(10);
      newNode->setPos(node->pos());
      newNode->finishInit();

      // Update original scene when position changes in enlarged map
      newNode->onPositionCommitted = [this, node](int id, double nx, double ny,
                                                  double nz) {
        node->x_sim = nx;
        node->y_sim = ny;
        node->z_sim = nz;
        node->setPos(nx * 40, ny * 40);
        updateNodePositionInTable(node->Type, id, nx, ny, nz);
      };
      newNode->onContextRequested = [this](const QString &type, int nodeId) {
        emit NodeContextRequested(type, nodeId);
      };

      enlargedScene->addItem(newNode);
    }
  }

  // Create a dialog for the enlarged map
  auto *dialog = new QDialog(this);
  dialog->setWindowTitle(tr("Enlarged Map View - Interactive Mode"));
  dialog->resize(1400, 1000);
  dialog->setAttribute(Qt::WA_DeleteOnClose);

  auto *layout = new QVBoxLayout(dialog);
  layout->setContentsMargins(10, 10, 10, 10);

  // Create a new graphics view for the enlarged map with zoom support
  auto *enlargedView = new ConfigGraphicsView(dialog);
  enlargedView->setScene(enlargedScene);

  // Apply the same transformation (Y-axis flip)
  enlargedView->scale(1, -1);

  // Fit the view to show all content including axes
  enlargedView->fitInView(enlargedScene->sceneRect(), Qt::KeepAspectRatio);

  layout->addWidget(enlargedView);

  // Add info label
  auto *infoLabel =
      new QLabel(tr("🖱️ Mouse Wheel: Zoom | Middle/Right Click: Pan | Drag "
                    "Nodes: Reposition | All changes sync with main view"),
                 dialog);
  infoLabel->setStyleSheet(
      "QLabel { color: #526071; font-style: italic; padding: 8px; "
      "background-color: #FAFBFD; border: 1px solid #2F3B49; border-radius: 4px; }");
  infoLabel->setAlignment(Qt::AlignCenter);
  layout->addWidget(infoLabel);

  // Add button row
  auto *buttonLayout = new QHBoxLayout();

  // Reset view button
  auto *resetViewButton = new QPushButton(tr("Reset View"), dialog);
  resetViewButton->setMaximumWidth(150);
  connect(resetViewButton, &QPushButton::clicked,
          [enlargedView, enlargedScene]() {
            enlargedView->resetTransform();
            enlargedView->scale(1, -1);
            enlargedView->fitInView(enlargedScene->sceneRect(),
                                    Qt::KeepAspectRatio);
          });

  // Close button
  auto *closeButton = new QPushButton(tr("Close"), dialog);
  closeButton->setMaximumWidth(150);
  connect(closeButton, &QPushButton::clicked, dialog, &QDialog::accept);

  buttonLayout->addStretch();
  buttonLayout->addWidget(resetViewButton);
  buttonLayout->addSpacing(10);
  buttonLayout->addWidget(closeButton);
  buttonLayout->addStretch();
  layout->addLayout(buttonLayout);

  // Clean up the enlarged scene when dialog closes
  connect(dialog, &QDialog::finished,
          [enlargedScene]() { delete enlargedScene; });

  dialog->exec();
}

void Simu_Config::setJsonHelper(JsonHelper *helper) { m_jsonHelper = helper; }

void Simu_Config::updateCurrentSceneJson(QJsonObject &buildingConfig) {
  if (scene) {
    update_json(scene, buildingConfig);
  }
}

void Simu_Config::loadConfigFromJson(const QJsonObject &buildingConfig) {
  qDebug() << "[Simu_Config] loadConfigFromJson()";

  // Load Building configuration
  if (buildingConfig.contains("Building") &&
      buildingConfig["Building"].isObject()) {
    QJsonObject building = buildingConfig["Building"].toObject();

    // Load range (x, y, z)
    if (building.contains("range") && building["range"].isArray()) {
      QJsonArray rangeArray = building["range"].toArray();
      if (rangeArray.size() >= 3) {
        ui->doubleSpinBox->setValue(rangeArray[0].toDouble());
        ui->doubleSpinBox_2->setValue(rangeArray[1].toDouble());
        ui->doubleSpinBox_3->setValue(rangeArray[2].toDouble());

        // Update building_range
        building_range[0] = rangeArray[0].toDouble();
        building_range[1] = rangeArray[1].toDouble();
        building_range[2] = rangeArray[2].toDouble();
      }
    }

    // Load building type
    if (building.contains("building_type")) {
      QString buildingType = building["building_type"].toString();
      int index = ui->comboBox->findText(buildingType);
      if (index >= 0) {
        ui->comboBox->setCurrentIndex(index);
      }
    }

    // Load wall type
    if (building.contains("wall_type")) {
      QString wallType = building["wall_type"].toString();
      int index = ui->comboBox_2->findText(wallType);
      if (index >= 0) {
        ui->comboBox_2->setCurrentIndex(index);
      }
    }
  }

  // Load simulation time
  if (buildingConfig.contains("SimulationTime")) {
    ui->doubleSpinBox_4->setValue(buildingConfig["SimulationTime"].toDouble());
  }

  // Load AP and STA counts
  if (buildingConfig.contains("Ap_num")) {
    int apNum = buildingConfig["Ap_num"].toInt();
    ui->lcdNumber->display(apNum);
    Num_ap = apNum;
  }

  if (buildingConfig.contains("Sta_num")) {
    int staNum = buildingConfig["Sta_num"].toInt();
    ui->lcdNumber_2->display(staNum);
    Num_sta = staNum;
  }

  // Don't change the button state or tab here - let the caller control the flow
  // ui->pushButton_7->setEnabled(true);
  // ui->pushButton_7->setText("Check");
  // ui->tabWidget->setCurrentIndex(0);

  qDebug() << "[Simu_Config] Configuration loaded:";
  qDebug() << "  Building range:" << building_range[0] << "x"
           << building_range[1] << "x" << building_range[2];
  qDebug() << "  AP count:" << Num_ap;
  qDebug() << "  STA count:" << Num_sta;
}

void Simu_Config::switchToTab(int index) {
  if (ui && ui->tabWidget) {
    ui->tabWidget->setCurrentIndex(index);
  }
}

void Simu_Config::setButtonChecked() {
  if (ui && ui->pushButton_7) {
    ui->pushButton_7->setEnabled(false);
    ui->pushButton_7->setText("Checked");
  }
}

void Simu_Config::on_saveButton_clicked() {
  // Emit signal to let MainWindow handle the saving with its jsonHelper
  emit SaveProjectRequested();
}
