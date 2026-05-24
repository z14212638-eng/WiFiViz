#include "ap_config.h"
#include "ui_ap_config.h"
#include "config_ui_style.h"

Ap_config::Ap_config(QWidget *parent)
    : QWidget(parent), ui(new Ui::Ap_config)
{
    ui->setupUi(this);
    ConfigUiStyle::ApplyConfigTheme(this, ui->tabWidget);

    centerWindow(this);

    ui->tabWidget->setCurrentIndex(0);
    if (!ui->checkBox_4->isChecked())
    {
        pos_set = true;
    }

    if (!ui->checkBox_3->isChecked())
    {
        ui->comboBox->setEnabled(false);
        ui->comboBox_2->setEnabled(false);
        ui->doubleSpinBox_4->setEnabled(false);
        ui->doubleSpinBox_5->setEnabled(false);
        ui->doubleSpinBox_14->setEnabled(false);
        ui->doubleSpinBox_15->setEnabled(false);
        ui->doubleSpinBox_16->setEnabled(false);
        ui->doubleSpinBox_17->setEnabled(false);
    }

    if (phymac_set != true)
    {
        ui->tabWidget->setTabEnabled(3, false);
    }
    ui->tableWidget->setRowCount(0);

    // deduce the GI from the selected protocol
    ui->comboBox_5->setCurrentIndex(5);
    ui->comboBox_15->setEnabled(false);

    Restrict_channel();
}

// restrict the bandwidth,channel number and the frequency
void Ap_config::Restrict_channel()
{
    auto apply_slot_sifs = [this]()
    {
        auto standard = get_standard_from_string(
            ui->comboBox_5->currentText().toStdString());

        bool updated = false;
        double slot = ui->doubleSpinBox_7->value();
        double sifs = ui->doubleSpinBox_8->value();

        switch (standard)
        {
        case Standard_MAP::k80211b:
            slot = 20.0;
            sifs = 10.0;
            updated = true;
            break;

        case Standard_MAP::k80211a:
        case Standard_MAP::k80211g:
        case Standard_MAP::k80211n:
        case Standard_MAP::k80211ac:
        case Standard_MAP::k80211ax:
            slot = 9.0;
            sifs = 16.0;
            updated = true;
            break;

        default:
            break;
        }

        if (updated)
        {
            ui->doubleSpinBox_7->setValue(slot);
            ui->doubleSpinBox_8->setValue(sifs);
        }

        ui->doubleSpinBox_7->setEnabled(false);
        ui->doubleSpinBox_8->setEnabled(false);
    };

    auto apply_rate_control = [this]()
    {
        auto standard = get_standard_from_string(
            ui->comboBox_5->currentText().toStdString());

        auto is_allowed = [standard](const QString &name)
        {
            if (standard == Standard_MAP::k80211b)
            {
                return name == "Aarf" || name == "Amrr" || name == "Arf" ||
                       name == "Cara" || name == "Onoe" || name == "Rraa" ||
                       name == "Ideal" || name == "Constant" || name == "MinstrelHt";
            }
            if (standard == Standard_MAP::k80211a || standard == Standard_MAP::k80211g)
            {
                return name == "Aarf" || name == "Amrr" || name == "Arf" ||
                       name == "Cara" || name == "Onoe" || name == "Rraa" ||
                       name == "Minstrel" || name == "Ideal" || name == "Constant"||name == "MinstrelHt";
            }
            // 802.11n/ac/ax
            return name == "MinstrelHt" || name == "Ideal" || name == "Constant" ||
                   name == "ThomsonSampling";
        };

        int firstAllowed = -1;
        for (int i = 0; i < ui->comboBox_7->count(); ++i)
        {
            const QString item = ui->comboBox_7->itemText(i);
            const bool allowed = is_allowed(item);
            ui->comboBox_7->setItemData(i, allowed ? QVariant() : QVariant(0), Qt::UserRole - 1);
            if (allowed && firstAllowed == -1)
            {
                firstAllowed = i;
            }
        }

        if (firstAllowed != -1 &&
            !is_allowed(ui->comboBox_7->currentText()))
        {
            ui->comboBox_7->setCurrentIndex(firstAllowed);
        }
    };

    connect(ui->comboBox_5,
            &QComboBox::currentTextChanged,
            this,
            [this, apply_slot_sifs, apply_rate_control](const QString &text)
            {
                auto standard = get_standard_from_string(text.toStdString());

                for (int i = 0; i < ui->comboBox_15->count(); ++i)
                {
                    ui->comboBox_15->setItemData(
                        i, QVariant(), Qt::UserRole - 1);
                }

                switch (standard)
                {
                case Standard_MAP::k80211a:
                case Standard_MAP::k80211b:
                case Standard_MAP::k80211g:
                    ui->comboBox_15->setCurrentIndex(1);
                    ui->comboBox_15->setEnabled(false);
                    break;

                case Standard_MAP::k80211n:
                case Standard_MAP::k80211ac:
                    ui->comboBox_15->setEnabled(true);
                    ui->comboBox_15->setCurrentIndex(1);
                    ui->comboBox_15->setItemData(
                        2, QVariant(0), Qt::UserRole - 1);
                    ui->comboBox_15->setItemData(
                        3, QVariant(1), Qt::UserRole - 1);
                    break;

                case Standard_MAP::k80211ax:
                    ui->comboBox_15->setEnabled(true);
                    ui->comboBox_15->setCurrentIndex(1);
                    ui->comboBox_15->setItemData(
                        0, QVariant(0), Qt::UserRole - 1);
                    break;

                default:
                    break;
                }

                apply_slot_sifs();
                apply_rate_control();
            });

    connect(ui->comboBox_3,
            &QComboBox::currentTextChanged,
            this,
            [this, apply_slot_sifs](const QString &text)
            {
                ui->comboBox_16->clear();

                if (text == "2.4G")
                {
                    ui->comboBox_16->clear();
                    ui->comboBox_16->addItems({"20", "40"});
                    ui->comboBox_16->setCurrentText("20");

                    ui->spinBox->setRange(1, 13);
                    ui->spinBox->setSingleStep(1);
                    ui->spinBox->setValue(1);
                }
                else if (text == "5G")
                {
                    ui->comboBox_16->clear();
                    ui->comboBox_16->addItems({"20", "40", "80", "160"});
                    ui->comboBox_16->setCurrentText("40");

                    ui->spinBox->setRange(36, 165);
                    ui->spinBox->setSingleStep(4);
                    ui->spinBox->setValue(36);
                }

                apply_slot_sifs();
            });

    connect(ui->comboBox_16,
            &QComboBox::currentTextChanged,
            this,
            [this](const QString &bwText)
            {
                int bw = bwText.toInt();

                if (ui->comboBox_3->currentText() != "5G")
                    return;

                if (bw == 20 || bw == 40)
                {
                    ui->spinBox->setRange(36, 165);
                    ui->spinBox->setSingleStep(4);
                    ui->spinBox->setValue(36);
                }
                else if (bw == 80)
                {
                    ui->spinBox->setRange(42, 155);
                    ui->spinBox->setSingleStep(16);
                    ui->spinBox->setValue(42);
                }
                else if (bw == 160)
                {
                    ui->spinBox->setRange(50, 114);
                    ui->spinBox->setSingleStep(64);
                    ui->spinBox->setValue(50);
                }
            });

    apply_slot_sifs();
    apply_rate_control();
}

Ap_config::~Ap_config()
{
    delete ui;
}

void Ap_config::setAvailableTargets(const QVector<QPair<QString, QString>> &targets)
{
    m_availableTargets = targets;
}

// Page1_Finished
void Ap_config::setPosition(Ap &one_ap)
{
    if (!ui->checkBox_4->isChecked())
    {

        if (ui->doubleSpinBox->value() != 0.00 || ui->doubleSpinBox_2->value() != 0.00 || ui->doubleSpinBox_3->value() != 0.00)
        {

            ui->tabWidget->setCurrentIndex(1);
            one_ap.m_position = {ui->doubleSpinBox->value(),
                                 ui->doubleSpinBox_2->value(),
                                 ui->doubleSpinBox_3->value()};
            pos_set = true;
        }
        else
        {
            QMessageBox::critical(
                this,
                "Error",
                "The position can not be (0,0,0)",
                QMessageBox::Ok);
        }
    }
    else
    {
        pos_set = true;

        // Random position
        double x_max = this->Building_range[0];
        double y_max = this->Building_range[1];
        double z_max = this->Building_range[2];
        double x_rand = get_true_random_double(0, x_max);
        double y_rand = get_true_random_double(0, y_max);
        double z_rand = get_true_random_double(0, z_max);

        one_ap.m_position = {x_rand, y_rand, z_rand};
        std::cout << "The position of AP is: " << x_rand << " " << y_rand << " " << z_rand << std::endl;
        ui->tabWidget->setCurrentIndex(1);
    }
}

void Ap_config::on_pushButton_clicked()
{
    emit Page1();
}

// Page2_Finished

void Ap_config::setMobility(Ap &one_ap)
{
    // If mobility is not set
    if (!ui->checkBox_3->isChecked())
    {
        ui->tabWidget->setCurrentIndex(2);
        mobility_set = true;
        one_ap.Mobility = false;
        return;
    }

    // If mobility is set and "random" mode is selected
    else if (ui->checkBox_3->isChecked() && ui->comboBox->currentText() == "Random Mobility Model")
    {
        one_ap.Mobility = true;
        one_ap.mode = "Random Mobility Model";
        double x_max = this->Building_range[0];
        double y_max = this->Building_range[1];
        double x_rand_1 = get_true_random_double(0, x_max);
        double y_rand_1 = get_true_random_double(0, y_max);
        double x_rand_2 = get_true_random_double(0, x_max);
        double y_rand_2 = get_true_random_double(0, y_max);

        ui->doubleSpinBox_14->setValue((x_rand_1 > x_rand_2) ? x_rand_2 : x_rand_1);
        ui->doubleSpinBox_15->setValue((x_rand_1 > x_rand_2) ? x_rand_1 : x_rand_2);
        ui->doubleSpinBox_16->setValue((y_rand_1 > y_rand_2) ? y_rand_2 : y_rand_1);
        ui->doubleSpinBox_17->setValue((y_rand_1 > y_rand_2) ? y_rand_1 : y_rand_2);

        // set the boundaries of mobility
        one_ap.boundaries = {ui->doubleSpinBox_14->value(), ui->doubleSpinBox_15->value(), ui->doubleSpinBox_16->value(), ui->doubleSpinBox_17->value()};
        one_ap.time_change_interval = ui->doubleSpinBox_4->value();
        one_ap.mode = ui->comboBox_2->currentText();
        one_ap.distance_change_interval = ui->doubleSpinBox_5->value();
        one_ap.random_velocity = ui->doubleSpinBox_18->value();

        std::cout << "The boundaries of AP is: " << one_ap.boundaries[0] << " " << one_ap.boundaries[1] << " " << one_ap.boundaries[2] << " " << one_ap.boundaries[3] << std::endl;
        std::cout << "The time change interval of AP is: " << one_ap.time_change_interval << std::endl;
        std::cout << "The distance change interval of AP is: " << one_ap.distance_change_interval << std::endl;
        std::cout << "The random velocity of AP is: " << one_ap.random_velocity << std::endl;
        std::cout << "The mode of AP is: " << one_ap.mode.toStdString() << std::endl;

        ui->tabWidget->setCurrentIndex(2);
        one_ap.Mobility = true;
        return;
    }
    // If not choosing the "random" mode,check if the conditions are valid
    else if ((ui->doubleSpinBox_15->value() > ui->doubleSpinBox_14->value()) &&
             (ui->doubleSpinBox_17->value() > ui->doubleSpinBox_16->value()) &&
             (ui->doubleSpinBox_4->value() != 0.00 || ui->doubleSpinBox_5->value() != 0.00) &&
             (ui->comboBox->currentText() == "Draw the Route Manually"))
    {
        mobility_set = true;
    }
    else
    {
        mobility_set = false;
    }
    if (!pos_set)
    {
        ui->tabWidget->setCurrentIndex(0);
        QMessageBox::critical(
            this,
            "Error",
            "Please set the position",
            QMessageBox::Ok);
    }

    else
    {
        if (mobility_set)
        {
            ui->tabWidget->setCurrentIndex(2);
            one_ap.Mobility = true;
        }

        else
        {
            QMessageBox::critical(
                this,
                "Error",
                "Please set the mobility correctly",
                QMessageBox::Ok);
        }
    }
}

void Ap_config::on_pushButton_4_clicked()
{
    emit Page2();
}

// Page1_Random_Position
void Ap_config::on_checkBox_4_clicked(bool checked)
{
    if (checked)
    {
        ui->doubleSpinBox->setEnabled(false);
        ui->doubleSpinBox_2->setEnabled(false);
        ui->doubleSpinBox_3->setEnabled(false);
        pos_set = true;
    }
    else
    {
        ui->doubleSpinBox->setEnabled(true);
        ui->doubleSpinBox_2->setEnabled(true);
        ui->doubleSpinBox_3->setEnabled(true);
    }
}

// Page2_Mobility
void Ap_config::on_checkBox_3_clicked(bool checked)
{
    if (checked)
    {
        ui->comboBox->setEnabled(true);
        ui->comboBox_2->setEnabled(true);
        ui->doubleSpinBox_4->setEnabled(true);
        ui->doubleSpinBox_5->setEnabled(true);
        ui->doubleSpinBox_14->setEnabled(true);
        ui->doubleSpinBox_15->setEnabled(true);
        ui->doubleSpinBox_16->setEnabled(true);
        ui->doubleSpinBox_17->setEnabled(true);
        ui->doubleSpinBox_18->setEnabled(true);
        mobility_set = false;
    }
    else
    {
        ui->comboBox->setEnabled(false);
        ui->comboBox_2->setEnabled(false);
        ui->doubleSpinBox_4->setEnabled(false);
        ui->doubleSpinBox_5->setEnabled(false);
        ui->doubleSpinBox_14->setEnabled(false);
        ui->doubleSpinBox_15->setEnabled(false);
        ui->doubleSpinBox_16->setEnabled(false);
        ui->doubleSpinBox_17->setEnabled(false);
        ui->doubleSpinBox_18->setEnabled(false);
        mobility_set = true;
    }
}

// Page3_Beacon
void Ap_config::on_checkBox_clicked(bool checked)
{
    if (checked)
    {
        beacon_set = true;
        ui->doubleSpinBox_12->setEnabled(true);
        ui->doubleSpinBox_13->setEnabled(true);
        ui->checkBox_2->setEnabled(true);
    }
    else
    {
        beacon_set = false;
        ui->doubleSpinBox_12->setEnabled(false);
        ui->doubleSpinBox_13->setEnabled(false);
        ui->checkBox_2->setEnabled(false);
    }
}

// Page3_Rts_Cts
void Ap_config::on_checkBox_5_clicked(bool checked)
{
    if (checked)
    {
        ui->spinBox_3->setEnabled(true);
        Rts_Cts_set = true;
    }
    else
    {
        ui->spinBox_3->setEnabled(false);
        Rts_Cts_set = false;
    }
}

// Page3_Qos

void Ap_config::on_checkBox_6_clicked(bool checked)
{
    if (checked)
    {
        ui->comboBox_6->setEnabled(true);
        ui->pushButton_8->setEnabled(true);
        Qos_set = true;
    }
    else
    {
        ui->comboBox_6->setEnabled(false);
        ui->pushButton_8->setEnabled(false);
        Qos_set = false;
    }
}

// Edca_Config
void Ap_config::on_pushButton_8_clicked()
{
    emit Edca_to_config();
}

// Antenna_Config
void Ap_config::on_pushButton_9_clicked()
{
    emit Antenna_to_config();
}

void Ap_config::Load_One_Config(Ap &ap_now)
{
    // set the channel number
    ap_now.channel_number = ui->spinBox->value();
    qint16 channel_number = ui->spinBox->value();

    // set the frequency
    if (ui->comboBox_3->currentText() == "5G")
    {
        ap_now.Frequency = 5000 + channel_number * 5;
    }
    else if (ui->comboBox_3->currentText() == "2.4G")
    {
        ap_now.Frequency = 2400 + (channel_number - 1) * 5;
    }

    // set the GI
    if (ui->comboBox_15->currentText() == "400ns")
    {
        ap_now.GI = 400;
    }
    else if (ui->comboBox_15->currentText() == "800ns")
    {
        ap_now.GI = 800;
    }
    else if (ui->comboBox_15->currentText() == "1600ns")
    {
        ap_now.GI = 1600;
    }
    else if (ui->comboBox_15->currentText() == "3200ns")
    {
        ap_now.GI = 3200;
    }

    // set the bandwidth
    ap_now.bandwidth = ui->comboBox_16->currentText().toShort();
    // set the Tx power
    ap_now.TxPower = ui->doubleSpinBox_6->value();
    // set the Ssid
    ap_now.Ssid = ui->lineEdit->text();
    // set the phy_model
    ap_now.Phy_model = ui->comboBox_4->currentText();
    // set the Standard
    ap_now.Standard = ui->comboBox_5->currentText();
    // set the Slot
    ap_now.Slot = ui->doubleSpinBox_7->value();
    // set the Sifs
    ap_now.Sifs = ui->doubleSpinBox_8->value();
    // set the RxSensitivity
    ap_now.RxSensitivity = ui->doubleSpinBox_9->value();
    // set the CcaEdThreshold
    ap_now.CcaThreshold = ui->doubleSpinBox_10->value();
    // set the CcaSensitivity
    ap_now.CcaSensitivity = ui->doubleSpinBox_11->value();
    // set the Beacon
    ap_now.Beacon = ui->checkBox->isChecked();
    ap_now.Beacon_interval = ui->doubleSpinBox_12->value();
    ap_now.Beacon_Rate = ui->doubleSpinBox_13->value();
    ap_now.EnableBeaconJitter = ui->checkBox_5->isChecked();
    // set the Rts_Cts
    ap_now.RtsCts = ui->checkBox_5->isChecked();
    ap_now.RtsCts_Threshold = ui->spinBox_3->value();
    // set the Rate_ctr_algo
    ap_now.Rate_ctr_algo = ui->comboBox_7->currentText();
    // set the TxQueue
    ap_now.TxQueue = ui->comboBox_8->currentText();

    // set the Qos
    ap_now.Qos = ui->checkBox_6->isChecked();
    ap_now.Edca = ui->comboBox_6->currentText();
}

void Ap_config::Get_Edca_Config(Ap &ap_now, Edca_config &edca_config)
{
    ap_now.AC_VO_cwmin = edca_config.AC_VO_cwmin;
    ap_now.AC_VO_cwmax = edca_config.AC_VO_cwmax;
    ap_now.AC_VO_AIFSN = edca_config.AC_VO_AIFSN;
    ap_now.AC_VO_TXOP_LIMIT = edca_config.AC_VO_TXOP_LIMIT;

    ap_now.AC_VI_cwmin = edca_config.AC_VI_cwmin;
    ap_now.AC_VI_cwmax = edca_config.AC_VI_cwmax;
    ap_now.AC_VI_AIFSN = edca_config.AC_VI_AIFSN;
    ap_now.AC_VI_TXOP_LIMIT = edca_config.AC_VI_TXOP_LIMIT;

    ap_now.AC_BE_cwmin = edca_config.AC_BE_cwmin;
    ap_now.AC_BE_cwmax = edca_config.AC_BE_cwmax;
    ap_now.AC_BE_AIFSN = edca_config.AC_BE_AIFSN;
    ap_now.AC_BE_TXOP_LIMIT = edca_config.AC_BE_TXOP_LIMIT;

    ap_now.AC_BK_cwmin = edca_config.AC_BK_cwmin;
    ap_now.AC_BK_cwmax = edca_config.AC_BK_cwmax;
    ap_now.AC_BK_AIFSN = edca_config.AC_BK_AIFSN;
    ap_now.AC_BK_TXOP_LIMIT = edca_config.AC_BK_TXOP_LIMIT;

    ap_now.Msdu_aggregation = edca_config.Msdu_aggregation;
    ap_now.AMsdu_type = edca_config.AMsdu_type;
    ap_now.MaxAMsduSize = edca_config.MaxAMsduSize;

    ap_now.Mpdu_aggregation = edca_config.Mpdu_aggregation;
    ap_now.AMpdu_type = edca_config.AMpdu_type;
    ap_now.MaxAMpduSize = edca_config.MaxAMpduSize;
    ap_now.Density = edca_config.Density;
}

void Ap_config::Get_Antenna_Config(Ap &ap_now, Antenna &antenna_config)
{
    // set the Antenna
    std::cout << "the number of antenna is: " << antenna_config.antenna_list.size() << std::endl;

    if (antenna_config.AntennaCount() == 0)
    {
        QMessageBox::critical(
            this,
            "Error",
            "Please set the antenna",
            QMessageBox::Ok);
        return;
    }

    std::cout << antenna_config.antenna_list[0]->Antenna_type.toStdString() << std::endl;
    for (auto item : antenna_config.antenna_list)
    {
        ap_now.Antenna_list.push_back(std::move(item));
    }
}

// Page3_Finished
void Ap_config::on_pushButton_6_clicked()
{
    if (ui->lineEdit->text().isEmpty())
    {
        QMessageBox::critical(
            this,
            "Error",
            "Please set the Ssid",
            QMessageBox::Ok);
        return;
    }

    phymac_set = true;
    ui->tabWidget->setTabEnabled(3, false);
    emit LoadOneApConfig();
    ApIndex++;
    m_flowConfigs.clear();
    ui->tableWidget->setRowCount(0);
    emit Finish_setting_ap();
}

bool Ap_config::Integrity_Check()
{
    return true;
}

// Finish setting Page4
void Ap_config::on_pushButton_7_clicked()
{
    QMessageBox::information(
        this,
        tr("Traffic Disabled Here"),
        tr("Traffic configuration is now handled from the simulation map right sidebar. Right-click the node after creation to edit its flows."));
}

void Ap_config::Get_Traffic_Config(Ap &ap_now)
{
    // 遍历所有保存的流量配置
    for (const FlowConfig &config : m_flowConfigs)
    {
        TrafficConfig new_traffic;
        new_traffic.Flow_id = config.flowId;
        new_traffic.Protocol = config.protocol;
        new_traffic.Destination = config.destination;
        new_traffic.StartTime = config.startTime;
        new_traffic.EndTime = config.endTime;
        new_traffic.Tos = config.tos;
        
        // 根据流量类型设置不同的参数
        new_traffic.flowType = static_cast<TrafficConfig::FlowType>(config.flowType);
        
        switch (config.flowType) {
            case FlowConfig::OnOff:
                new_traffic.dataRate = config.dataRate;
                new_traffic.packetSize = config.packetSize;
                new_traffic.ontimeType = config.ontimeType;
                new_traffic.ontimeParams = config.ontimeParams;
                new_traffic.offtimeType = config.offtimeType;
                new_traffic.offtimeParams = config.offtimeParams;
                new_traffic.maxBytes = config.maxBytes;
                break;
                
            case FlowConfig::CBR:
                new_traffic.packetSize = config.packetSize;
                new_traffic.interval = config.interval;
                new_traffic.maxPackets = config.maxPackets;
                break;
                
            case FlowConfig::Bulk:
                new_traffic.maxBytes = config.maxBytes;
                break;
        }
        
        // Insert the data structure into the vector
        ap_now.Traffic_list.push_back(new_traffic);
    }
}
// add Traffic


void Ap_config::on_pushButton_3_clicked()
{
    if (!m_flowConfigs.isEmpty()) {
        m_flowConfigs.removeLast();
        
        int row = ui->tableWidget->rowCount() - 1;
        if (row >= 0) {
            ui->tableWidget->removeRow(row);
        }
    }
}


void Ap_config::on_pushButton_2_clicked()
{
    flow_dialog dialog(this, true);
    QVector<QPair<QString, QString>> candidates;
    const QString currentApValue = QString("AP_%1").arg(ApIndex);
    for (const auto &target : m_availableTargets)
    {
        if (target.second != currentApValue)
        {
            candidates.push_back(target);
        }
    }
    dialog.setAvailableTargets(candidates);
    
    if (dialog.exec() == QDialog::Accepted) {
        FlowConfig config = dialog.getFlowConfig();
        
        // 保存配置
        m_flowConfigs.push_back(config);
        
        // 在表格中显示
        if (ui->tableWidget->rowCount() == 1 &&
            ui->tableWidget->item(0, 0) &&
            ui->tableWidget->item(0, 0)->text().isEmpty())
        {
            ui->tableWidget->removeRow(0);
        }
        
        int rowCount = ui->tableWidget->rowCount();
        ui->tableWidget->insertRow(rowCount);
        ui->tableWidget->setItem(rowCount, 0, new QTableWidgetItem(config.flowId));
        ui->tableWidget->setItem(rowCount, 1, new QTableWidgetItem(QString::number(config.startTime)));
        ui->tableWidget->setItem(rowCount, 2, new QTableWidgetItem(QString::number(config.endTime)));
        ui->tableWidget->setItem(rowCount, 3, new QTableWidgetItem(config.protocol));
        ui->tableWidget->setItem(rowCount, 4, new QTableWidgetItem(config.destination));
        
        // 根据流量类型显示额外信息
        QString typeStr;
        switch(config.flowType) {
            case FlowConfig::OnOff:
                typeStr = "OnOff";
                break;
            case FlowConfig::CBR:
                typeStr = "CBR";
                break;
            case FlowConfig::Bulk:
                typeStr = "Bulk";
                break;
        }
        ui->tableWidget->setItem(rowCount, 5, new QTableWidgetItem(typeStr));
    }
}

void Ap_config::resetPage()
{
    qDebug() << "[Ap_config] resetPage()";

    // ===============================
    // 1️⃣ 内部状态变量
    // ===============================
    Building_range = {0, 0, 0};
    m_position     = {0, 0, 0};
    ApIndex        = 0;

    pos_set      = false;
    mobility_set = false;
    phymac_set   = false;
    beacon_set   = false;
    Rts_Cts_set  = false;
    Qos_set      = false;
    Ssid_set     = false;

    // ===============================
    // 2️⃣ Tab 状态
    // ===============================
    ui->tabWidget->setCurrentIndex(0);
    ui->tabWidget->setTabEnabled(3, false); // Page4 依赖 phymac_set

    // ===============================
    // 3️⃣ Page1：Position
    // ===============================
    ui->checkBox_4->setChecked(true); // random position off

    ui->doubleSpinBox->setEnabled(true);
    ui->doubleSpinBox_2->setEnabled(true);
    ui->doubleSpinBox_3->setEnabled(true);

    ui->doubleSpinBox->setValue(0.0);
    ui->doubleSpinBox_2->setValue(0.0);
    ui->doubleSpinBox_3->setValue(0.0);

    // ===============================
    // 4️⃣ Page2：Mobility
    // ===============================
    ui->checkBox_3->setChecked(false); // mobility off

    ui->comboBox->setEnabled(false);
    ui->comboBox_2->setEnabled(false);

    ui->doubleSpinBox_4->setEnabled(false);
    ui->doubleSpinBox_5->setEnabled(false);
    ui->doubleSpinBox_14->setEnabled(false);
    ui->doubleSpinBox_15->setEnabled(false);
    ui->doubleSpinBox_16->setEnabled(false);
    ui->doubleSpinBox_17->setEnabled(false);
    ui->doubleSpinBox_18->setEnabled(false);

    ui->doubleSpinBox_4->setValue(0);
    ui->doubleSpinBox_5->setValue(0);
    ui->doubleSpinBox_14->setValue(0);
    ui->doubleSpinBox_15->setValue(0);
    ui->doubleSpinBox_16->setValue(0);
    ui->doubleSpinBox_17->setValue(0);
    ui->doubleSpinBox_18->setValue(0);

    // ===============================
    // 5️⃣ Page3：PHY / MAC
    // ===============================
    // ui->lineEdit->clear();                  // SSID
    ui->comboBox_3->setCurrentText("5G");  // band
    ui->comboBox_4->setCurrentIndex(0);     // phy model
    ui->comboBox_5->setCurrentText("802.11ax");     // standard
    ui->comboBox_7->setCurrentIndex(0);
    ui->comboBox_8->setCurrentIndex(0);

    ui->spinBox->setValue(36);              // channel
    ui->spinBox_3->setValue(0);             // RTS threshold

    ui->doubleSpinBox_6->setValue(0.0);     // TxPower
    ui->doubleSpinBox_7->setValue(0.0);     // Slot
    ui->doubleSpinBox_8->setValue(0.0);     // Sifs
    ui->doubleSpinBox_7->setEnabled(false);
    ui->doubleSpinBox_8->setEnabled(false);
    ui->doubleSpinBox_9->setValue(0.0);
    ui->doubleSpinBox_10->setValue(0.0);
    ui->doubleSpinBox_11->setValue(0.0);

    ui->checkBox->setChecked(false);         // Beacon
    ui->doubleSpinBox_12->setEnabled(false);
    ui->doubleSpinBox_13->setEnabled(false);
    ui->checkBox_2->setEnabled(false);

    ui->checkBox_5->setChecked(false);       // RTS/CTS
    ui->spinBox_3->setEnabled(false);

    ui->checkBox_6->setChecked(false);       // QoS
    ui->comboBox_6->setEnabled(false);
    ui->pushButton_8->setEnabled(false);

    // ===============================
    // 6️⃣ Page4：Traffic
    // ===============================
    m_flowConfigs.clear();
    ui->tableWidget->setRowCount(0);

    // ===============================
    // 7️⃣ Channel / Band 约束恢复
    // ===============================
    Restrict_channel();

    qDebug() << "[Ap_config] resetPage() done";
}
