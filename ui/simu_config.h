#ifndef SIMU_CONFIG_H
#define SIMU_CONFIG_H

#include "JsonHelper.h"
#include "ap_config.h"
#include "node_config.h"
#include "ppdu_adapter.h"
#include "ppdu_timeline_view.h"
#include "ppdu_visual_item.h"
#include "qt_ppdu_reader.h"
#include "shm.h"
#include "utils.h"

#include <QGraphicsEllipseItem>
#include <QGraphicsSceneContextMenuEvent>
#include <QGraphicsRectItem>
#include <QGraphicsSceneHoverEvent>
#include <QGraphicsSceneMouseEvent>
#include <QGraphicsView>
#include <QPainter>
#include <QStyleOptionGraphicsItem>
#include <QThread>
#include <QWidget>
#include <QtGlobal>
#include <atomic>
#include <functional>
#include <qprocess.h>

namespace Ui {
class Simu_Config;
}
class QDialog;

class Simu_Config : public QWidget,public ResettableBase {
  Q_OBJECT

public:
  explicit Simu_Config(QWidget *parent = nullptr);
  ~Simu_Config();
  size_t Num_ap;
  size_t Num_sta;
  void Write_building_into_config(BuildingConfig &, Ap_config &, node_config &);
  void Draw_the_config_map();
  void update_json(QGraphicsScene *, QJsonObject &);
  void Update_json_map(JsonHelper &json_helper);
  void Load_General_Json(JsonHelper &json_helper);
  void Create_And_StartThread();
  void Generate_And_Build();
  void StartVisualizationReader();
  void RunGeneratedSimulation();
  void requestCleanup();
  void cleanupAndExit();
  void resetPage() override;
  void rebuildScene();
  void setNs3Path(const QString &path);
  void setSelectedScene(const QString &sceneName);
  void resetSimuScene();
  void addApToTable(int id, const QVector<double> &pos, bool mobility);
  void addStaToTable(int id, const QVector<double> &pos, bool mobility);
  void updateLcdNumbers();
  void drawCoordinateAxes(QGraphicsScene *targetScene, double width, double height, double scale);
  void setJsonHelper(JsonHelper *helper);
  void updateCurrentSceneJson(QJsonObject &buildingConfig);
  void loadConfigFromJson(const QJsonObject &buildingConfig);
  void switchToTab(int index);
  void setButtonChecked();
  void refreshFlowTargetSources();

  QVector<double> building_range = {0, 0, 0};

signals:
  void BackToLastPage();
  void Building_Set();
  void update();
  void AddSta();
  void AddAp();
  void LoadGeneralConfig();
  void ppduReady(const PpduVisualItem &item);
  void CreateAndStartThread();
  void ns3OutputReady(const QString &text);
  void simulationProcessCreated(QProcess *process);
  void generationReady();
  void simulation_end();
  void sniffFailed();
  void SaveProjectRequested();
  void NodeContextRequested(const QString &nodeType, int nodeId);
  void NodeTrafficSelectionCleared();
  

private slots:
  void on_pushButton_clicked();

  void on_pushButton_4_clicked();

  void on_pushButton_7_clicked();

  void on_pushButton_8_clicked();

  void on_pushButton_5_clicked();

  void on_pushButton_6_clicked();

  void on_pushButton_2_clicked();

  void on_pushButton_3_clicked();

  void on_pushButton_9_clicked();

  void on_saveButton_clicked();
  
  void showEnlargedMap();

private:
  Ui::Simu_Config *ui;
  QGraphicsScene *scene = nullptr;

  QThread *m_ppduThread = nullptr;
  QtPpduReader *m_ppduReader = nullptr;
  QProcess *ns3Process = nullptr;
  QString m_ns3Path;
  QString m_selectedScene;
  QString m_lastGeneratedSceneFileName;
  std::atomic_bool m_hasPpdu{false};
  QString m_copiedScratchPath;

  QPushButton *m_enlargeButton = nullptr;
  QDialog *m_buildWaitDialog = nullptr;
  JsonHelper *m_jsonHelper = nullptr;

  bool ensureScriptGeneratorBuilt(QString &outGeneratorPath);
  QString ns3ProgramPath() const;
  bool generateStandaloneScript(QString &outSceneFileName);
  void showBuildWaitDialog();
  void closeBuildWaitDialog();
  QString generatedScratchScriptPath(const QString &projectName) const;
  void cleanupOldGeneratedScratchScripts(const QString &keepFilePath) const;
  void stopPpduReader();
  bool prepareSceneScript(QString &outSceneFileName);
  bool removeNodeFromScene(const QString &type, int id);
  bool deleteSelectedNode(const QString &type);
  void clearNodes(const QString &type);

  void updateNodePositionInTable(const QString &type, int id, double x,
                                 double y, double z);
};

// NodeItem represents a movable node in the graphics scene
class NodeItem : public QGraphicsEllipseItem {
public:
  std::function<void(int id, double x, double y, double z)> onPositionCommitted;
  std::function<void(const QString &type, int id)> onContextRequested;
  NodeItem(const QRectF &buildingRect, double scale,
           QGraphicsItem *parent = nullptr)
      : QGraphicsEllipseItem(parent), m_rect(buildingRect), m_scale(scale) {
    setRect(-4, -4, 8, 8);
    setFlags(ItemIsMovable | ItemSendsGeometryChanges);
    setAcceptHoverEvents(true);
    m_infoItem = new QGraphicsSimpleTextItem(this);
    m_infoItem->setFlag(QGraphicsItem::ItemIgnoresTransformations);
    m_infoItem->setZValue(100);
    m_infoItem->setVisible(false);
  }

  void finishInit() { m_initialized = true; }

  QGraphicsSimpleTextItem *m_infoItem = nullptr;
  qint8 m_id = -1;
  double x_sim = 0;
  double y_sim = 0;
  double z_sim = 0;
  QString Type = "";

protected:
  bool m_hovered = false;
  bool m_initialized = false;

  void mousePressEvent(QGraphicsSceneMouseEvent *event) override {
    QGraphicsEllipseItem::mousePressEvent(event);
    
    // Show info when starting to drag
    if (m_initialized && !m_infoItem->isVisible()) {
      m_infoItem->setText(QString("Type:%1\nID: %2\nx=%3  y=%4  z=%5")
                              .arg(Type)
                              .arg(m_id)
                              .arg(x_sim, 0, 'f', 2)
                              .arg(y_sim, 0, 'f', 2)
                              .arg(z_sim, 0, 'f', 2));
      m_infoItem->setPos(8, 8);
      m_infoItem->setVisible(true);
    }
  }

  void mouseReleaseEvent(QGraphicsSceneMouseEvent *event) override {
    QGraphicsEllipseItem::mouseReleaseEvent(event);

    if (!m_initialized)
      return;

    const QPointF p = pos();
    x_sim = p.x() / m_scale;
    y_sim = p.y() / m_scale;

    if (onPositionCommitted)
      onPositionCommitted(m_id, x_sim, y_sim, z_sim);
  }

  void contextMenuEvent(QGraphicsSceneContextMenuEvent *event) override {
    if (onContextRequested) {
      onContextRequested(Type, m_id);
    }
    event->accept();
  }

  void hoverEnterEvent(QGraphicsSceneHoverEvent *event) override {
    m_hovered = true;
    update();

    m_infoItem->setText(QString("Type:%1\nID: %2\nx=%3  y=%4  z=%5")
                            .arg(Type)
                            .arg(m_id)
                            .arg(x_sim, 0, 'f', 2)
                            .arg(y_sim, 0, 'f', 2)
                            .arg(z_sim, 0, 'f', 2));

    m_infoItem->setPos(8, 8); // bias to the center of the node
    m_infoItem->setVisible(true);

    QGraphicsEllipseItem::hoverEnterEvent(event);
  }

  void hoverLeaveEvent(QGraphicsSceneHoverEvent *event) override {
    m_hovered = false;
    m_infoItem->setVisible(false);
    update();
    QGraphicsEllipseItem::hoverLeaveEvent(event);
  }

  void paint(QPainter *painter, const QStyleOptionGraphicsItem *option,
             QWidget *widget = nullptr) override {
    QGraphicsEllipseItem::paint(painter, option, widget);

    if (m_hovered) {
      painter->setRenderHint(QPainter::Antialiasing);
      QPen pen(QColor(0, 120, 215));
      pen.setWidthF(1.5);
      painter->setPen(pen);
      painter->setBrush(Qt::NoBrush);
      painter->drawEllipse(rect().adjusted(-3, -3, 3, 3));
    }
  }

  QVariant itemChange(GraphicsItemChange change,
                      const QVariant &value) override {
    if (change == ItemPositionChange) {
      QPointF p = value.toPointF();
      p.setX(qBound(m_rect.left(), p.x(), m_rect.right()));
      p.setY(qBound(m_rect.top(), p.y(), m_rect.bottom()));
      return p;
    }

    if (change == ItemPositionHasChanged) {
      QPointF p = pos();

      x_sim = p.x() / m_scale;
      y_sim = p.y() / m_scale;
      
      // Update info text in real-time during dragging
      if (m_infoItem && m_infoItem->isVisible()) {
        m_infoItem->setText(QString("Type:%1\nID: %2\nx=%3  y=%4  z=%5")
                                .arg(Type)
                                .arg(m_id)
                                .arg(x_sim, 0, 'f', 2)
                                .arg(y_sim, 0, 'f', 2)
                                .arg(z_sim, 0, 'f', 2));
      }
    }

    return QGraphicsEllipseItem::itemChange(change, value);
  }

  QRectF boundingRect() const override { return rect().adjusted(-5, -5, 5, 5); }

private:
  QRectF m_rect;
  double m_scale;
};

#endif // SIMU_CONFIG_H
