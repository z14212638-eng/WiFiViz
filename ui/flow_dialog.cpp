#include "flow_dialog.h"
#include "ui_flow_dialog.h"
#include "config_ui_style.h"
#include "random_variable_dialog.h"
#include <QComboBox>

flow_dialog::flow_dialog(QWidget *parent, bool isAp)
    : QDialog(parent)
    , ui(new Ui::flow_dialog)
    , m_isAp(isAp)
{
    ui->setupUi(this);
    ConfigUiStyle::ApplyConfigTheme(this, ui->tabWidget);
    setupConnections();
    
    // 初始化默认参数
    m_ontimeParams["Value"] = 1.0;
    m_offtimeParams["Value"] = 1.0;
    populateTargetCombos();
}

flow_dialog::~flow_dialog()
{
    delete ui;
}

void flow_dialog::setupConnections()
{
    // 使用 activated 信号：即使重新选中同一项也会触发，解决默认 Constant 无法弹出对话框的问题
    connect(ui->comboBox_ontime_type, QOverload<int>::of(&QComboBox::activated),
            this, &flow_dialog::onOntimeTypeChanged);
    
    connect(ui->comboBox_offtime_type, QOverload<int>::of(&QComboBox::activated),
            this, &flow_dialog::onOfftimeTypeChanged);

    // 允许用户直接在 lineEdit 中编辑 Constant 类型的数值
    connect(ui->lineEdit_ontime_value, &QLineEdit::editingFinished,
            this, [this]() {
                if (ui->comboBox_ontime_type->currentText() == "Constant") {
                    bool ok;
                    double val = ui->lineEdit_ontime_value->text().trimmed().toDouble(&ok);
                    if (ok) {
                        m_ontimeParams.clear();
                        m_ontimeParams["Value"] = val;
                    }
                }
            });

    connect(ui->lineEdit_offtime_value, &QLineEdit::editingFinished,
            this, [this]() {
                if (ui->comboBox_offtime_type->currentText() == "Constant") {
                    bool ok;
                    double val = ui->lineEdit_offtime_value->text().trimmed().toDouble(&ok);
                    if (ok) {
                        m_offtimeParams.clear();
                        m_offtimeParams["Value"] = val;
                    }
                }
            });
}

void flow_dialog::onOntimeTypeChanged(int index)
{
    QString type = ui->comboBox_ontime_type->currentText();
    showRandomVariableDialog(type, m_ontimeParams, tr("OnTime Random Variable"));
}

void flow_dialog::onOfftimeTypeChanged(int index)
{
    QString type = ui->comboBox_offtime_type->currentText();
    showRandomVariableDialog(type, m_offtimeParams, tr("OffTime Random Variable"));
}

void flow_dialog::showRandomVariableDialog(const QString &type, QMap<QString, double> &params, const QString &title)
{
    RandomVariableDialog dialog(type, this);
    dialog.setWindowTitle(title);
    
    // 如果已有参数，设置到对话框
    if (!params.isEmpty()) {
        dialog.setParameters(params);
    }
    
    // 显示对话框
    if (dialog.exec() == QDialog::Accepted) {
        params = dialog.getParameters();
        
        // 更新显示值
        QString summary;
        if (type == "Constant" && params.contains("Value")) {
            // Constant 类型只显示数值，便于用户直接编辑
            summary = QString::number(params["Value"]);
        } else {
            for (auto it = params.begin(); it != params.end(); ++it) {
                if (!summary.isEmpty()) summary += ", ";
                summary += QString("%1=%2").arg(it.key()).arg(it.value());
            }
        }
        
        // 根据是 OnTime 还是 OffTime 更新对应的 lineEdit
        if (title.contains("OnTime")) {
            ui->lineEdit_ontime_value->setText(summary);
        } else if (title.contains("OffTime")) {
            ui->lineEdit_offtime_value->setText(summary);
        }
    }
}

void flow_dialog::setAvailableTargets(const QVector<QPair<QString, QString>> &targets)
{
    m_targets = targets;
    populateTargetCombos();
}

void flow_dialog::populateTargetCombos()
{
    auto fillCombo = [](QComboBox *combo, const QVector<QPair<QString, QString>> &targets) {
        const QString currentValue = combo->currentData().toString();
        const QString currentText = combo->currentText();

        combo->clear();
        for (const auto &target : targets)
        {
            combo->addItem(target.first, target.second);
        }

        int index = combo->findData(currentValue);
        if (index < 0)
        {
            index = combo->findText(currentText);
        }
        if (index >= 0)
        {
            combo->setCurrentIndex(index);
        }
    };

    fillCombo(ui->comboBox_direction_onoff, m_targets);
    fillCombo(ui->comboBox_direction_cbr, m_targets);
    fillCombo(ui->comboBox_direction_bulk, m_targets);

    if (!m_targets.isEmpty())
    {
        if (ui->comboBox_direction_onoff->currentIndex() < 0)
        {
            ui->comboBox_direction_onoff->setCurrentIndex(0);
        }
        if (ui->comboBox_direction_cbr->currentIndex() < 0)
        {
            ui->comboBox_direction_cbr->setCurrentIndex(0);
        }
        if (ui->comboBox_direction_bulk->currentIndex() < 0)
        {
            ui->comboBox_direction_bulk->setCurrentIndex(0);
        }
    }
}

void flow_dialog::setCurrentDestination(const QString &destination)
{
    auto apply = [&destination](QComboBox *combo) {
        int index = combo->findData(destination);
        if (index < 0)
        {
            index = combo->findText(destination);
        }
        if (index >= 0)
        {
            combo->setCurrentIndex(index);
        }
    };

    apply(ui->comboBox_direction_onoff);
    apply(ui->comboBox_direction_cbr);
    apply(ui->comboBox_direction_bulk);
}

FlowConfig flow_dialog::getFlowConfig() const
{
    FlowConfig config;
    
    // 获取当前活动的标签页
    int currentIndex = ui->tabWidget->currentIndex();
    
    if (currentIndex == 0) { // OnOff 页面
        config.flowType = FlowConfig::OnOff;
        config.flowId = ui->lineEdit_flowid_onoff->text();
        config.dataRate = ui->doubleSpinBox_datarate->value();
        config.packetSize = ui->spinBox_packetsize->value();
        config.ontimeType = ui->comboBox_ontime_type->currentText();
        config.ontimeParams = m_ontimeParams;
        config.offtimeType = ui->comboBox_offtime_type->currentText();
        config.offtimeParams = m_offtimeParams;

        // 对 Constant 类型，从 lineEdit 读取最新数值（用户可能直接编辑了 lineEdit）
        if (config.ontimeType == "Constant") {
            bool ok;
            double val = ui->lineEdit_ontime_value->text().trimmed().toDouble(&ok);
            if (ok) {
                config.ontimeParams["Value"] = val;
            }
        }
        if (config.offtimeType == "Constant") {
            bool ok;
            double val = ui->lineEdit_offtime_value->text().trimmed().toDouble(&ok);
            if (ok) {
                config.offtimeParams["Value"] = val;
            }
        }
        config.maxBytes = ui->spinBox_maxbytes->value();
        config.protocol = ui->comboBox_protocol->currentText();
        config.tos = ui->spinBox_tos->value();
        config.startTime = ui->doubleSpinBox_starttime_onoff->value();
        config.endTime = ui->doubleSpinBox_endtime_onoff->value();
        config.destination = ui->comboBox_direction_onoff->currentData().toString();
    }
    else if (currentIndex == 1) { // CBR 页面
        config.flowType = FlowConfig::CBR;
        config.flowId = ui->lineEdit_flowid_cbr->text();
        config.packetSize = ui->spinBox_packetsize_cbr->value();
        config.interval = ui->doubleSpinBox_interval_cbr->value();
        config.maxPackets = ui->spinBox_maxpackets_cbr->value();
        config.protocol = ui->comboBox_protocol_cbr->currentText();
        config.tos = ui->spinBox_tos_cbr->value();
        config.startTime = ui->doubleSpinBox_starttime_cbr->value();
        config.endTime = ui->doubleSpinBox_endtime_cbr->value();
        config.destination = ui->comboBox_direction_cbr->currentData().toString();
    }
    else if (currentIndex == 2) { // Bulk 页面
        config.flowType = FlowConfig::Bulk;
        config.flowId = ui->lineEdit_flowid_bulk->text();
        config.maxBytes = ui->spinBox_maxbytes_bulk->value();
        config.protocol = ui->comboBox_protocol_bulk->currentText();
        config.tos = ui->spinBox_tos_bulk->value();
        config.startTime = ui->doubleSpinBox_starttime_bulk->value();
        config.endTime = ui->doubleSpinBox_endtime_bulk->value();
        config.destination = ui->comboBox_direction_bulk->currentData().toString();
    }
    
    return config;
}

void flow_dialog::setFlowConfig(const FlowConfig &config)
{
    // 根据流量类型切换到对应的标签页
    switch (config.flowType) {
        case FlowConfig::OnOff:
            ui->tabWidget->setCurrentIndex(0);
            ui->lineEdit_flowid_onoff->setText(config.flowId);
            ui->doubleSpinBox_datarate->setValue(config.dataRate);
            ui->spinBox_packetsize->setValue(config.packetSize);
            ui->comboBox_ontime_type->setCurrentText(config.ontimeType);
            m_ontimeParams = config.ontimeParams;
            ui->comboBox_offtime_type->setCurrentText(config.offtimeType);
            m_offtimeParams = config.offtimeParams;
            ui->spinBox_maxbytes->setValue(config.maxBytes);
            ui->comboBox_protocol->setCurrentText(config.protocol);
            ui->spinBox_tos->setValue(config.tos);
            ui->doubleSpinBox_starttime_onoff->setValue(config.startTime);
            ui->doubleSpinBox_endtime_onoff->setValue(config.endTime);
            setCurrentDestination(config.destination);
            break;
            
        case FlowConfig::CBR:
            ui->tabWidget->setCurrentIndex(1);
            ui->lineEdit_flowid_cbr->setText(config.flowId);
            ui->spinBox_packetsize_cbr->setValue(config.packetSize);
            ui->doubleSpinBox_interval_cbr->setValue(config.interval);
            ui->spinBox_maxpackets_cbr->setValue(config.maxPackets);
            ui->comboBox_protocol_cbr->setCurrentText(config.protocol);
            ui->spinBox_tos_cbr->setValue(config.tos);
            ui->doubleSpinBox_starttime_cbr->setValue(config.startTime);
            ui->doubleSpinBox_endtime_cbr->setValue(config.endTime);
            setCurrentDestination(config.destination);
            break;
            
        case FlowConfig::Bulk:
            ui->tabWidget->setCurrentIndex(2);
            ui->lineEdit_flowid_bulk->setText(config.flowId);
            ui->spinBox_maxbytes_bulk->setValue(config.maxBytes);
            ui->comboBox_protocol_bulk->setCurrentText(config.protocol);
            ui->spinBox_tos_bulk->setValue(config.tos);
            ui->doubleSpinBox_starttime_bulk->setValue(config.startTime);
            ui->doubleSpinBox_endtime_bulk->setValue(config.endTime);
            setCurrentDestination(config.destination);
            break;
    }
}
