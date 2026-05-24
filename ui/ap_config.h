#ifndef AP_CONFIG_H
#define AP_CONFIG_H
#include "JsonHelper.h"
#include "edca_config.h"
#include "antennas.h"
#include "utils.h"
#include "flow_dialog.h"
#include <QMessageBox>
#include <QWidget>
#include <QVector>
#include <QPair>

namespace Ui
{
    class Ap_config;
}

class Ap_config : public QWidget,public ResettableBase
{
    Q_OBJECT

public:
    explicit Ap_config(QWidget *parent = nullptr);
    ~Ap_config();
    QVector<double> Building_range = {0, 0, 0};
    QVector<double> m_position = {0, 0, 0};
    qint8 ApIndex = 0;
    
    // 存储流量配置
    QVector<FlowConfig> m_flowConfigs;

    void setPosition(Ap &);
    void setMobility(Ap &);
    void Load_One_Config(Ap &);
    void Get_Edca_Config(Ap &, Edca_config &);
    void Get_Antenna_Config(Ap &, Antenna &);
    void Get_Traffic_Config(Ap &);
    void resetPage() override;
    void setAvailableTargets(const QVector<QPair<QString, QString>> &targets);

signals:
    void Finish_setting_ap();
    void Page1();
    void Page2();
    void Edca_to_config();
    void Antenna_to_config();
    void LoadOneApConfig();
    
private slots:

    // Ap_Position_set
    void on_pushButton_clicked();
    // Mobility_finished
    void on_pushButton_4_clicked();
    // Random_Position_set
    void on_checkBox_4_clicked(bool checked);
    // Ap_Mobility_set
    void on_checkBox_3_clicked(bool checked);
    // Beacons_set
    void on_checkBox_clicked(bool checked);
    // Rts_Cts_set
    void on_checkBox_5_clicked(bool checked);
    // Qos_set
    void on_checkBox_6_clicked(bool checked);
    // edca_config
    void on_pushButton_8_clicked();
    // antenna_config
    void on_pushButton_9_clicked();
    // Save_config(one ap configuration finished)
    void on_pushButton_6_clicked();
    // Remained to be done
    bool Integrity_Check();

    void on_pushButton_7_clicked();

    void on_pushButton_2_clicked();

    void on_pushButton_3_clicked();

    void Restrict_channel();

private:
    Ui::Ap_config *ui;
    QVector<QPair<QString, QString>> m_availableTargets;
    bool pos_set = false;
    bool mobility_set = false;
    bool phymac_set = false;

    bool beacon_set = false;
    bool Rts_Cts_set = false;
    bool Qos_set = false;

    bool Ssid_set = false;
};

#endif // AP_CONFIG_H
