#include "node_traffic_panel.h"
#include <QFrame>
#include <QHeaderView>
#include <QHBoxLayout>
#include <QLabel>
#include <QMessageBox>
#include <QPushButton>
#include <QTableWidget>
#include <QVBoxLayout>

namespace
{
QPushButton *
CreateActionButton(const QString &text)
{
    auto *button = new QPushButton(text);
    button->setMinimumHeight(38);
    return button;
}

QString
FormatFlowWindow(double startTime, double endTime)
{
    return QString("%1 ~ %2").arg(startTime, 0, 'f', 3).arg(endTime, 0, 'f', 3);
}
} // namespace

NodeTrafficPanel::NodeTrafficPanel(QWidget *parent)
    : QWidget(parent)
{
    setObjectName(QStringLiteral("nodeTrafficPanel"));
    setMinimumWidth(320);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    setAttribute(Qt::WA_StyledBackground, true);
    setAutoFillBackground(true);
    setStyleSheet(
        "QWidget#nodeTrafficPanel {"
        "  background: qlineargradient(x1:0, y1:0, x2:1, y2:1,"
        "    stop:0 #FFFFFF, stop:1 #F4F8FC);"
        "  color: #152033;"
        "}"
        "QFrame#configSection {"
        "  background: #FFFFFF;"
        "  border: 1px solid #D6E2EE;"
        "  border-radius: 18px;"
        "}"
        "QLabel#sectionTitle {"
        "  color: #122033;"
        "  font-size: 18px;"
        "  font-weight: 800;"
        "}"
        "QLabel#fieldLabel {"
        "  color: #6C7B8D;"
        "  font-size: 12px;"
        "  font-weight: 700;"
        "}"
        "QTableWidget {"
        "  background: #FFFFFF;"
        "  alternate-background-color: #F4F8FC;"
        "  border: 1px solid #D6E2EE;"
        "  border-radius: 14px;"
        "  color: #152033;"
        "  gridline-color: transparent;"
        "}"
        "QTableWidget::item {"
        "  padding: 7px;"
        "  border-bottom: 1px solid #EDF3F9;"
        "}"
        "QTableWidget::item:selected {"
        "  background: #DDEBFF;"
        "  color: #152033;"
        "}"
        "QHeaderView::section {"
        "  background: #F4F8FC;"
        "  color: #243447;"
        "  border: none;"
        "  border-bottom: 1px solid #D6E2EE;"
        "  padding: 8px;"
        "  font-weight: 800;"
        "}"
        "QPushButton {"
        "  background: #DCE1E6;"
        "  border: 1px solid #B8C2CC;"
        "  border-radius: 10px;"
        "  padding: 7px 12px;"
        "  color: #243447;"
        "  font-weight: 800;"
        "}"
        "QPushButton:hover {"
        "  background: #E8ECEF;"
        "  border-color: #7FB9FF;"
        "}"
        "QPushButton:pressed {"
        "  background: #C9D0D6;"
        "}"
    );

    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(18, 18, 18, 18);
    root->setSpacing(14);

    auto *header = new QFrame(this);
    header->setObjectName("configSection");
    auto *headerLayout = new QVBoxLayout(header);
    headerLayout->setContentsMargins(18, 16, 18, 16);
    headerLayout->setSpacing(6);

    m_title = new QLabel(tr("Node Traffic"), header);
    m_title->setObjectName("sectionTitle");
    headerLayout->addWidget(m_title);

    m_subtitle = new QLabel(tr("Right-click a node in the map to edit its flows."), header);
    m_subtitle->setObjectName("fieldLabel");
    m_subtitle->setWordWrap(true);
    headerLayout->addWidget(m_subtitle);

    root->addWidget(header);

    m_hint = new QLabel(tr("No node selected"), this);
    m_hint->setObjectName("fieldLabel");
    m_hint->setWordWrap(true);
    root->addWidget(m_hint);

    m_table = new QTableWidget(this);
    m_table->setColumnCount(5);
    m_table->setHorizontalHeaderLabels(
        {tr("Flow ID"), tr("Type"), tr("Protocol"), tr("Destination"), tr("Time")});
    m_table->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    m_table->verticalHeader()->setVisible(false);
    m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_table->setSelectionMode(QAbstractItemView::SingleSelection);
    m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_table->setAlternatingRowColors(true);
    root->addWidget(m_table, 1);

    auto *row1 = new QHBoxLayout;
    row1->setSpacing(10);
    m_addButton = CreateActionButton(tr("Add Flow"));
    m_editButton = CreateActionButton(tr("Edit Flow"));
    m_deleteButton = CreateActionButton(tr("Delete Flow"));
    row1->addWidget(m_addButton);
    row1->addWidget(m_editButton);
    row1->addWidget(m_deleteButton);
    root->addLayout(row1);

    auto *row2 = new QHBoxLayout;
    row2->setSpacing(10);
    m_clearButton = CreateActionButton(tr("Clear All"));
    row2->addWidget(m_clearButton);
    row2->addStretch(1);
    root->addLayout(row2);

    root->addStretch(0);

    connect(m_addButton, &QPushButton::clicked, this, &NodeTrafficPanel::onAddFlow);
    connect(m_editButton, &QPushButton::clicked, this, &NodeTrafficPanel::onEditFlow);
    connect(m_deleteButton, &QPushButton::clicked, this, &NodeTrafficPanel::onDeleteFlow);
    connect(m_clearButton, &QPushButton::clicked, this, &NodeTrafficPanel::onClearFlows);

    clearSelection();
}

void NodeTrafficPanel::setJsonHelper(JsonHelper *helper)
{
    m_jsonHelper = helper;
}

void NodeTrafficPanel::clearSelection()
{
    m_nodeType.clear();
    m_nodeId = -1;
    m_flows.clear();
    m_title->setText(tr("Node Traffic"));
    m_subtitle->setText(tr("Right-click a node in the map to edit its flows."));
    m_hint->setText(tr("No node selected"));
    m_table->setRowCount(0);
    setButtonsEnabled(false);
}

void NodeTrafficPanel::showNode(const QString &nodeType, int nodeId)
{
    m_nodeType = nodeType;
    m_nodeId = nodeId;
    m_flows = m_jsonHelper ? m_jsonHelper->GetNodeTrafficConfigs(nodeType, nodeId)
                           : QVector<TrafficConfig>{};

    m_title->setText(tr("%1 %2 Traffic").arg(nodeType).arg(nodeId));
    m_subtitle->setText(tr("Configure all flows owned by this node. Changes are saved when you click Update in the simulation page."));
    m_hint->setText(tr("Selected node: %1 %2").arg(nodeType).arg(nodeId));
    setButtonsEnabled(true);
    rebuildTable();
}

void NodeTrafficPanel::rebuildTable()
{
    m_table->setRowCount(0);
    for (int i = 0; i < m_flows.size(); ++i)
    {
        const auto &flow = m_flows[i];
        const int row = m_table->rowCount();
        m_table->insertRow(row);
        m_table->setItem(row, 0, new QTableWidgetItem(flow.Flow_id));
        m_table->setItem(row, 1, new QTableWidgetItem(flowTypeToString(flow.flowType)));
        m_table->setItem(row, 2, new QTableWidgetItem(flow.Protocol));
        m_table->setItem(row, 3, new QTableWidgetItem(destinationLabel(flow.Destination)));
        m_table->setItem(row, 4, new QTableWidgetItem(FormatFlowWindow(flow.StartTime, flow.EndTime)));
    }

    setButtonsEnabled(!m_nodeType.isEmpty());
}

void NodeTrafficPanel::setButtonsEnabled(bool enabled)
{
    m_addButton->setEnabled(enabled);
    const bool hasFlow = enabled && !m_flows.isEmpty();
    m_editButton->setEnabled(hasFlow);
    m_deleteButton->setEnabled(hasFlow);
    m_clearButton->setEnabled(hasFlow);
}

QString NodeTrafficPanel::flowTypeToString(TrafficConfig::FlowType type) const
{
    switch (type)
    {
    case TrafficConfig::OnOff:
        return QStringLiteral("OnOff");
    case TrafficConfig::CBR:
        return QStringLiteral("CBR");
    case TrafficConfig::Bulk:
        return QStringLiteral("Bulk");
    }
    return QStringLiteral("Unknown");
}

QVector<QPair<QString, QString>> NodeTrafficPanel::buildTargets() const
{
    if (!m_jsonHelper)
    {
        return {};
    }

    QVector<QPair<QString, QString>> targets;
    const QString selfValue = QString("%1_%2").arg(m_nodeType).arg(m_nodeId);
    for (const auto &target : m_jsonHelper->GetAvailableNodeTargets())
    {
        if (target.second != selfValue)
        {
            targets.push_back(target);
        }
    }
    return targets;
}

QString NodeTrafficPanel::destinationLabel(const QString &value) const
{
    if (!m_jsonHelper)
    {
        return value;
    }

    for (const auto &target : m_jsonHelper->GetAvailableNodeTargets())
    {
        if (target.second == value)
        {
            return target.first;
        }
    }
    return value;
}

FlowConfig NodeTrafficPanel::toFlowConfig(const TrafficConfig &config) const
{
    FlowConfig flow;
    flow.flowId = config.Flow_id;
    flow.protocol = config.Protocol;
    flow.destination = config.Destination;
    flow.startTime = config.StartTime;
    flow.endTime = config.EndTime;
    flow.tos = config.Tos;
    flow.dataRate = config.dataRate;
    flow.packetSize = config.packetSize;
    flow.ontimeType = config.ontimeType;
    flow.ontimeParams = config.ontimeParams;
    flow.offtimeType = config.offtimeType;
    flow.offtimeParams = config.offtimeParams;
    flow.maxBytes = config.maxBytes;
    flow.interval = config.interval;
    flow.maxPackets = config.maxPackets;

    switch (config.flowType)
    {
    case TrafficConfig::OnOff:
        flow.flowType = FlowConfig::OnOff;
        break;
    case TrafficConfig::CBR:
        flow.flowType = FlowConfig::CBR;
        break;
    case TrafficConfig::Bulk:
        flow.flowType = FlowConfig::Bulk;
        break;
    }

    return flow;
}

TrafficConfig NodeTrafficPanel::toTrafficConfig(const FlowConfig &config) const
{
    TrafficConfig flow;
    flow.Flow_id = config.flowId;
    flow.Protocol = config.protocol;
    flow.Destination = config.destination;
    flow.StartTime = config.startTime;
    flow.EndTime = config.endTime;
    flow.Tos = config.tos;
    flow.dataRate = config.dataRate;
    flow.packetSize = config.packetSize;
    flow.ontimeType = config.ontimeType;
    flow.ontimeParams = config.ontimeParams;
    flow.offtimeType = config.offtimeType;
    flow.offtimeParams = config.offtimeParams;
    flow.maxBytes = config.maxBytes;
    flow.interval = config.interval;
    flow.maxPackets = config.maxPackets;

    switch (config.flowType)
    {
    case FlowConfig::OnOff:
        flow.flowType = TrafficConfig::OnOff;
        break;
    case FlowConfig::CBR:
        flow.flowType = TrafficConfig::CBR;
        break;
    case FlowConfig::Bulk:
        flow.flowType = TrafficConfig::Bulk;
        break;
    }

    return flow;
}

void NodeTrafficPanel::syncFlowsToJson() const
{
    if (!m_jsonHelper || m_nodeId < 0 || m_nodeType.isEmpty())
    {
        return;
    }
    m_jsonHelper->SetNodeTrafficConfigs(m_nodeType, m_nodeId, m_flows);
}

void NodeTrafficPanel::onAddFlow()
{
    flow_dialog dialog(this, m_nodeType == QLatin1String("AP"));
    dialog.setAvailableTargets(buildTargets());
    if (dialog.exec() != QDialog::Accepted)
    {
        return;
    }

    m_flows.push_back(toTrafficConfig(dialog.getFlowConfig()));
    syncFlowsToJson();
    rebuildTable();
    m_table->selectRow(m_table->rowCount() - 1);
}

void NodeTrafficPanel::onEditFlow()
{
    const int row = m_table->currentRow();
    if (row < 0 || row >= m_flows.size())
    {
        QMessageBox::information(this, tr("Edit Flow"), tr("Please select one flow first."));
        return;
    }

    flow_dialog dialog(this, m_nodeType == QLatin1String("AP"));
    dialog.setAvailableTargets(buildTargets());
    dialog.setFlowConfig(toFlowConfig(m_flows[row]));
    if (dialog.exec() != QDialog::Accepted)
    {
        return;
    }

    m_flows[row] = toTrafficConfig(dialog.getFlowConfig());
    syncFlowsToJson();
    rebuildTable();
    m_table->selectRow(row);
}

void NodeTrafficPanel::onDeleteFlow()
{
    const int row = m_table->currentRow();
    if (row < 0 || row >= m_flows.size())
    {
        QMessageBox::information(this, tr("Delete Flow"), tr("Please select one flow first."));
        return;
    }

    m_flows.removeAt(row);
    syncFlowsToJson();
    rebuildTable();
    if (row < m_table->rowCount())
    {
        m_table->selectRow(row);
    }
    else if (m_table->rowCount() > 0)
    {
        m_table->selectRow(m_table->rowCount() - 1);
    }
}

void NodeTrafficPanel::onClearFlows()
{
    if (m_flows.isEmpty())
    {
        return;
    }

    m_flows.clear();
    syncFlowsToJson();
    rebuildTable();
}
