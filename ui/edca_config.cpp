#include "edca_config.h"
#include "ui_edca_config.h"
#include "config_ui_style.h"

Edca_config::Edca_config(QWidget *parent)
    : QDialog(parent)
    , ui(new Ui::Edca_config)
{
    ui->setupUi(this);
    ConfigUiStyle::RebuildEdcaConfig(this, ui->tabWidget);
    centerWindow(this);
}

Edca_config::~Edca_config()
{
    delete ui;
}

void Edca_config::on_buttonBox_accepted()
{
     AC_VO_cwmin = ui->spinBox->value();
     AC_VO_cwmax = ui->spinBox_2->value();
     AC_VO_AIFSN = ui->spinBox_3->value();
     AC_VO_TXOP_LIMIT = ui->spinBox_4->value();

     AC_VI_cwmin = ui->spinBox_5->value();
     AC_VI_cwmax = ui->spinBox_7->value();
     AC_VI_AIFSN = ui->spinBox_9->value();
     AC_VI_TXOP_LIMIT = ui->spinBox_8->value();

     AC_BE_cwmin = ui->spinBox_6->value();
     AC_BE_cwmax = ui->spinBox_10->value();
     AC_BE_AIFSN = ui->spinBox_11->value();
     AC_BE_TXOP_LIMIT = ui->spinBox_12->value();

     AC_BK_cwmin = ui->spinBox_13->value();
     AC_BK_cwmax = ui->spinBox_14->value();
     AC_BK_AIFSN = ui->spinBox_15->value();
     AC_BK_TXOP_LIMIT = ui->spinBox_16->value();

     Msdu_aggregation = ui->checkBox->isChecked();
     AMsdu_type = ui->comboBox->currentText();
     MaxAMsduSize = ui->spinBox_17->value();

     Mpdu_aggregation = ui->checkBox_2->isChecked();
     AMpdu_type = ui->comboBox_2->currentText();
     MaxAMpduSize = ui->spinBox_18->value();
     Density = ui->spinBox_19->value();

     emit BackToLastPage();
}

void Edca_config::resetPage()
{
    // ==== QoS / EDCA 基本状态 ====
    Qos = false;
    Edca = "AC_BE";

    // ==== AC_VO ====
    enable_ac_vo = false;
    trafficType_vo = "CBR";
    packetSize_vo = 160;
    interval_vo = 0;
    AC_VO_cwmin = 3;
    AC_VO_cwmax = 7;
    AC_VO_AIFSN = 2;
    AC_VO_TXOP_LIMIT = 47;

    ui->spinBox->setValue(AC_VO_cwmin);
    ui->spinBox_2->setValue(AC_VO_cwmax);
    ui->spinBox_3->setValue(AC_VO_AIFSN);
    ui->spinBox_4->setValue(AC_VO_TXOP_LIMIT);

    // ==== AC_VI ====
    enable_ac_vi = false;
    trafficType_vi = "CBR";
    packetSize_vi = 1200;
    meanDataRate_vi = 0;
    peakDataRate_vi = 0;
    AC_VI_cwmin = 7;
    AC_VI_cwmax = 15;
    AC_VI_AIFSN = 2;
    AC_VI_TXOP_LIMIT = 94;

    ui->spinBox_5->setValue(AC_VI_cwmin);
    ui->spinBox_7->setValue(AC_VI_cwmax);
    ui->spinBox_9->setValue(AC_VI_AIFSN);
    ui->spinBox_8->setValue(AC_VI_TXOP_LIMIT);

    // ==== AC_BE ====
    enable_ac_be = false;
    trafficType_be = "CBR";
    packetSize_be = 1200;
    DataRate_be = 5;
    AC_BE_cwmin = 15;
    AC_BE_cwmax = 1023;
    AC_BE_AIFSN = 3;
    AC_BE_TXOP_LIMIT = 0;

    ui->spinBox_6->setValue(AC_BE_cwmin);
    ui->spinBox_10->setValue(AC_BE_cwmax);
    ui->spinBox_11->setValue(AC_BE_AIFSN);
    ui->spinBox_12->setValue(AC_BE_TXOP_LIMIT);

    // ==== AC_BK ====
    enable_ac_bk = false;
    trafficType_bk = "CBR";
    packetSize_bk = 1200;
    DataRate_bk = 5;
    AC_BK_cwmin = 15;
    AC_BK_cwmax = 1023;
    AC_BK_AIFSN = 7;
    AC_BK_TXOP_LIMIT = 0;

    ui->spinBox_13->setValue(AC_BK_cwmin);
    ui->spinBox_14->setValue(AC_BK_cwmax);
    ui->spinBox_15->setValue(AC_BK_AIFSN);
    ui->spinBox_16->setValue(AC_BK_TXOP_LIMIT);

    // ==== MSDU 聚合 ====
    Msdu_aggregation = true;
    AMsdu_type = "AC_BE";
    MaxAMsduSize = 65535;

    ui->checkBox->setChecked(Msdu_aggregation);
    ui->comboBox->setCurrentText(AMsdu_type);
    ui->spinBox_17->setValue(MaxAMsduSize);

    // ==== MPDU 聚合 ====
    Mpdu_aggregation = true;
    AMpdu_type = "AC_BE";
    MaxAMpduSize = 65;
    Density = 40;

    ui->checkBox_2->setChecked(Mpdu_aggregation);
    ui->comboBox_2->setCurrentText(AMpdu_type);
    ui->spinBox_18->setValue(MaxAMpduSize);
    ui->spinBox_19->setValue(Density);

    qDebug() << "[Edca_config] resetPage() done";
}
