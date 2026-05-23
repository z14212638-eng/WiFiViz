#pragma once

#include "JsonHelper.h"
#include "flow_dialog.h"

#include <QWidget>

class QLabel;
class QPushButton;
class QTableWidget;

class NodeTrafficPanel : public QWidget
{
    Q_OBJECT

public:
    explicit NodeTrafficPanel(QWidget *parent = nullptr);

    void setJsonHelper(JsonHelper *helper);
    void clearSelection();
    void showNode(const QString &nodeType, int nodeId);

private:
    JsonHelper *m_jsonHelper = nullptr;
    QString m_nodeType;
    int m_nodeId = -1;
    QVector<TrafficConfig> m_flows;

    QLabel *m_title = nullptr;
    QLabel *m_subtitle = nullptr;
    QLabel *m_hint = nullptr;
    QTableWidget *m_table = nullptr;
    QPushButton *m_addButton = nullptr;
    QPushButton *m_editButton = nullptr;
    QPushButton *m_deleteButton = nullptr;
    QPushButton *m_clearButton = nullptr;

    void rebuildTable();
    void setButtonsEnabled(bool enabled);
    QString flowTypeToString(TrafficConfig::FlowType type) const;
    QVector<QPair<QString, QString>> buildTargets() const;
    QString destinationLabel(const QString &value) const;
    FlowConfig toFlowConfig(const TrafficConfig &config) const;
    TrafficConfig toTrafficConfig(const FlowConfig &config) const;
    void syncFlowsToJson() const;

private slots:
    void onAddFlow();
    void onEditFlow();
    void onDeleteFlow();
    void onClearFlows();
};
