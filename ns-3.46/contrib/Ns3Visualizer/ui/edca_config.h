#ifndef EDCA_CONFIG_H
#define EDCA_CONFIG_H

#include <QDialog>
#include "utils.h"

namespace Ui
{
    class Edca_config;
}

class Edca_config : public QDialog,public ResettableBase
{
    Q_OBJECT

public:
    explicit Edca_config(QWidget *parent = nullptr);
    ~Edca_config();
    bool Qos = false;
    QString Edca = "AC_BE";
    bool enable_ac_vo = false;
    QString trafficType_vo = "CBR";
    qint32 packetSize_vo = 160;
    qint32 interval_vo = 0;
    qint16 AC_VO_cwmin = 3;
    qint16 AC_VO_cwmax = 7;
    qint16 AC_VO_AIFSN = 2;
    qint16 AC_VO_TXOP_LIMIT = 47;

    bool enable_ac_vi = false;
    QString trafficType_vi = "CBR";
    qint32 packetSize_vi = 1200;
    qint32 meanDataRate_vi = 0;
    qint32 peakDataRate_vi = 0;
    qint16 AC_VI_cwmin = 7;
    qint16 AC_VI_cwmax = 15;
    qint16 AC_VI_AIFSN = 2;
    qint16 AC_VI_TXOP_LIMIT = 94;

    bool enable_ac_be = false;
    QString trafficType_be = "CBR";
    qint32 packetSize_be = 1200;
    qint32 DataRate_be = 5;
    qint16 AC_BE_cwmin = 15;
    qint16 AC_BE_cwmax = 1023;
    qint16 AC_BE_AIFSN = 3;
    qint16 AC_BE_TXOP_LIMIT = 0;

    bool enable_ac_bk = false;
    QString trafficType_bk = "CBR";
    qint32 packetSize_bk = 1200;
    qint32 DataRate_bk = 5;
    qint16 AC_BK_cwmin = 15;
    qint16 AC_BK_cwmax = 1023;
    qint16 AC_BK_AIFSN = 7;
    qint16 AC_BK_TXOP_LIMIT = 0;

    bool Msdu_aggregation = true;
    QString AMsdu_type = "AC_BE";
    quint16 MaxAMsduSize = 65535;

    bool Mpdu_aggregation = true;
    QString AMpdu_type = "AC_BE";
    qint16 MaxAMpduSize = 65;
    qint32 Density = 40;

    bool is_ap;
    bool is_sta;

    void resetPage() override;
signals:
    void BackToLastPage();
private slots:
    void on_buttonBox_accepted();

private:
    Ui::Edca_config *ui;
};

#endif // EDCA_CONFIG_H
