#ifndef FLOW_DIALOG_H
#define FLOW_DIALOG_H

#include <QDialog>
#include <QMap>
#include <QPair>
#include <QString>
#include <QVector>

namespace Ui {
class flow_dialog;
}

// 流量配置结构
struct FlowConfig {
    QString flowId;
    QString protocol;
    QString destination;
    double startTime;
    double endTime;
    int tos;
    
    // OnOff 特有
    double dataRate;
    int packetSize;
    QString ontimeType;
    QMap<QString, double> ontimeParams;
    QString offtimeType;
    QMap<QString, double> offtimeParams;
    int maxBytes;
    
    // CBR 特有
    double interval;
    int maxPackets;
    
    // 流量类型标识
    enum FlowType { OnOff, CBR, Bulk } flowType;
};

class flow_dialog : public QDialog
{
    Q_OBJECT

public:
    explicit flow_dialog(QWidget *parent = nullptr, bool isAp = true);
    ~flow_dialog();
    void setAvailableTargets(const QVector<QPair<QString, QString>> &targets);
    
    // 获取配置
    FlowConfig getFlowConfig() const;
    
    // 设置配置（可选，用于编辑已有流量）
    void setFlowConfig(const FlowConfig &config);

private slots:
    void onOntimeTypeChanged(int index);
    void onOfftimeTypeChanged(int index);

private:
    Ui::flow_dialog *ui;
    
    // 存储随机变量参数
    QMap<QString, double> m_ontimeParams;
    QMap<QString, double> m_offtimeParams;
    QVector<QPair<QString, QString>> m_targets;
    bool m_isAp = true;
    
    void setupConnections();
    void showRandomVariableDialog(const QString &type, QMap<QString, double> &params, const QString &title);
    void populateTargetCombos();
    void setCurrentDestination(const QString &destination);
};

#endif // FLOW_DIALOG_H
